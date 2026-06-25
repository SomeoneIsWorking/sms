# Perform-list filter byteswap — THE root cause of "NPC calcRootMatrix never runs"

Date: 2026-06-25 (late-5). Status: ROOT CAUSE FOUND + fixed in the working tree, but the fix
exposes a downstream architectural change (the GX draw perform-list activates → guest drawsync
crash + double-draw with the hand-driving). NOT yet committable. See "Remaining work" below.

## The finding (verified, not deduced)

`TPerformList::load` (src/System/PerformList.cpp) reads each entry's 4-byte **filter** from the
data file `PerformLists.bin` via `stream.read(&value, 4)` — which is a RAW `readData` (memcpy),
**no byteswap** (JSUInputStream::read; the decomp already byteswaps the BE string-length prefix
with `JSU_BE16`, proving the raw-read needs manual swapping on LE). The data file is big-endian.

On the LE native host the filter came out byteswapped:
- conductor's CalcAnim filter read as **0x2000000**; real BE value is **0x2** (the CALC op bit).
- conductor's Movement filter read as **0x1000000**; real value **0x1** (MOVEMENT).
- camera-1 CalcAnim **0x6000000** → real **0x6** (calc|viewCalc).

`TPerformList::perform` ANDs the call flag with each entry's filter and calls
`testPerform(call & filter)`. With the byteswapped 0x2000000 filter, the conductor received
0x2000000 — and `TConductor::perform` / `TLiveActor::perform` only test LOW bits (0x1/0x2/0x4/
0x200; **verified against the original DOL** — TLiveActor 0x80217ec0, TConductor 0x80033ed8,
testPerform 0x802fcc94, TPerformList::perform 0x802a4e28 — none shift or test the high bits).
So the entire movement/calc perform pass was **inert** → `TLiveActor::calcRootMatrix` (sets the
NPC base TR matrix from mPosition) NEVER ran → NPCs drew with an unset/identity root matrix →
projected to extreme NDC (present bbox x[±239008]). This is the handoff's "NPC positioning"
frontier, fully root-caused.

## Fix (in working tree, src/System/PerformList.cpp)

```c
stream.read(&value, 4);
value = JSU_BE32(value);   // BE data dword on LE host; no-op on BE
```

Verified: SB_PL_DBG now shows コンダクター CalcAnim filter=0x2, Movement→0x3001 (0x1|0x3000 load
transform); SB_COND_DBG shows the conductor receiving **0x2 (calc)** and **0x3001 (movement)**;
SB_ROOTMTX_DBG shows `calcRootMatrix` FIRING via the faithful director path (no SB_NPC_CALC hack).

## Second fix (verified vs DOL): the director's calc/movement branch was misdecompiled

src/System/MarDirectorDirect.cpp calc branch selected the wrong perform list. Original DOL
(0x80299b2c movement, 0x80299bac anim) does:
- `if (unk4E & 1)` → SHINE-stage lists (mShinePfLstMov / mShinePfLstAnm)
- `else` (normal stages incl. Delfino Plaza) → **mPerformListMovement / mPerformListCalcAnim**

The decomp had movement calling mShinePfLstMov in BOTH branches, and the anim branch SWAPPED.
`unk4E & 1` is the shine-stage flag; bit 0x1 of unk4E is never set for normal stages (only 0x2
gets set in setupObjects). Fixed both branches to match the DOL. Needed alongside the byteswap so
the corrected (now-meaningful) CalcAnim/Movement lists actually get driven.

## The downstream consequence (why it's not committable yet)

The byteswap corrects ALL data-loaded lists, including **PerformList GX / GX Post / Silhouette**
(the DRAW dispatch). Those were previously inert *because of the same byteswap* — which is the
real reason sms-boot hand-drives the scene in native/src/scene_drive.cpp (scene->perform(0x8) +
drive_sky/drive_chr). With correct filters the GX perform list now issues real GX draws:
- DEFAULT plaza shape count went 57 → 75 (GX list draws ON TOP of the hand-driving → double-draw).
- The guest `TDrawSyncManager::threadFunc` (src/System/DrawSyncManager.cpp) CRASHES: its
  "fabricated" FIFO + the 32-bit token classification (`>=0x80000000`/`>=0x10000`) are 64-bit-
  incompatible under real GX breakpoint traffic. gdb: SIGSEGV at threadFunc+0xea, `mov (%rdx),%rdi`
  with rdx=0x70 (near-null deref from the corrupted FIFO). CLAUDE.md already records that this
  exact manager was REPLACED natively in sunbright (sms_drawsync_lossproof.cpp) for the same
  64-bit-fragility reason; sms-boot has no such replacement.

So the byteswap is the correct root-cause fix, but landing it = the full **"own the perform-list
draw dispatch"** task (handoff directive #1), which is large:

## Remaining work (the real "own the perform-list dispatch" task — START HERE next session)

1. **Neuter/replace the guest TDrawSyncManager for sms-boot.** Native GX has NO GP FIFO
   (GXEnableBreakPt/GXDisableBreakPt are already no-ops in native/platform/gx_fb_impl.cpp), so the
   CPU↔GPU breakpoint pacing is vestigial. Make pushBreakPoint / drawSyncCallbackSub not enqueue
   (or the threadFunc FIFO 64-bit-safe) under SMS_NATIVE_PLATFORM — faithful: there is no async
   FIFO race to pace when the renderer captures shapes synchronously. This unblocks running with
   the GX list active.
2. **Reconcile the GX perform-list draw with the hand-driving.** With the GX list now drawing, the
   hand-driven scene_drive.cpp (drive_sky/drive_chr/scene->perform(0x8)) is redundant/duplicative
   (57→75 shapes). Decide: retire the hand-driving and let the GX perform list be the single draw
   path (the faithful end state), OR keep hand-driving and suppress the GX-list draw. Retiring the
   hand-driving is the directive-aligned destination ("own the perform-list dispatch", no blackbox)
   — but verify the GX list draws the full scene correctly first (it may need the right view/present
   wiring; handoff LATE-2's "degenerate gradient" diagnosis was made WITH the byteswapped filters,
   so re-evaluate it now that filters are correct).
3. Once non-crashing + single-draw: verify NPC positions (calcRootMatrix now runs → bbox should be
   sane, not ±239008) and clipping (clipActors with the real camera/view). Then commit.

## Tooling added (env-gated, keep)
- `SB_COND_DBG=1` (conductor.cpp): one-shot per distinct param_1 the conductor receives.
- `SB_CALCPASS_DBG=1` (MarDirectorDirect.cpp): the director calc-branch list selection + uVar8/uVar11.
- `SB_PL_DBG=1` (existing): per (list, entry, filter) at load — now shows the CORRECT (swapped) filters.

## Disasm verification artifacts (scratch/frames/, regenerate with sunbright-recomp --disasm + capstone)
TLiveActor::perform 0x80217ec0, TConductor::perform 0x80033ed8, testPerform 0x802fcc94,
TPerformList::perform 0x802a4e28, MarDirector::direct calc branch 0x80299b2c/0x80299bac.
