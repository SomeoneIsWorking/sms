# TLightWithDBSet::makeDrawBuffer × 4 — RE findings, blockers, port shape

Date: 2026-07-02.  Context: continuation session picking up the "4 makeDrawBuffer
subclass ports" P0 the previous session left open.

## Status
Not landed. Blocked on two dependencies below. This note captures the RE so the
next session can execute without re-discovering.

## Function set

| Sym | Addr | Size (insns) |
|-----|------|--------------|
| `TIndirectLightWithDBSet::makeDrawBuffer`  | 0x802289ac | 114 |
| `TMapObjectLightWithDBSet::makeDrawBuffer` | 0x80228b74 | 101 |
| `TObjectLightWithDBSet::makeDrawBuffer`    | 0x80228d08 | 105 |
| `TPlayerLightWithDBSet::makeDrawBuffer`    | 0x80228eac | 137 |

Same shape; differ in the name-table offsets and (TPlayer) presence of a
third search loop.

## Algorithm shape (all 4)

```
int lightIdx = -1;
for (int i = 0; i < LightGroup->mLightCount; ++i)
    if (strcmp(LightGroup->mLights[i].name, "<light-name-A>") == 0) { lightIdx = i; break; }

int ambIdx = -1;
for (int i = 0; i < AmbGroup->mAmbColorCount; ++i)
    if (strcmp(AmbGroup->mAmbColors[i].name, "<amb-name-A>") == 0) { ambIdx = i; break; }

// (TPlayer only) a third search loop, stride 4 = pointer array — TBD which array

this->mDrawBuffers = new TLightDrawBuffer*[this->mBufferCount];
for (int i = 0; i < this->mBufferCount; ++i) {
    auto* buf = new TLightDrawBuffer(i, 0x100, "<drawbuf-name>");
    this->mDrawBuffers[i] = buf;
    auto* owner = new TLightCommon("<owner-name>");
    buf->setLight(owner);                    // via vtable slot at +0x24
    owner->mAmbBaseIdx  = ambIdx;            // TLightCommon +0x20
    owner->mLightBaseIdx = lightIdx;         // TLightCommon +0x24
}
```

Reads of guest globals:
- `r13-0x6114` = `gpTLightCommonLightAry` (already extern'd in LightUtil.cpp)
- `r13-0x6118` = `gpTLightCommonAmbAry` (already extern'd)
- Name-table base `r31 = 0x803ad868` (rodata) — per-subclass offsets below

## Per-subclass name offsets (bytes relative to 0x803ad868)

| Subclass  | LightAry needle | AmbAry needle | (TPlayer 3rd) | mgr-ctor name |
|-----------|----------------|---------------|---------------|---------------|
| TIndirect | +0xe0          | +0xf8         | —             | +0x1a4        |
| TMapObj   | +0x7c          | *TBD*         | —             | +0x170        |
| TObject   | +0x40          | *TBD*         | —             | +0x188        |
| TPlayer   | +0xc           | *TBD*         | stride-4 arr  | +0x15c        |

**BLOCKER #1:** the exact strings at these rodata offsets are not readable by
sunbright-recomp --disasm (rodata is not "in any text section"). Options:
(a) extend the recomp tool with a rodata dump mode; (b) read strings straight
from the extracted DOL. The offsets themselves are known from the disasm and
are stable across builds.

## mBufferCount is not 0 — TLightWithDBSetManager ctor sets it to 2 per subclass

**BLOCKER #2 / reconstruction bug:** the reconstructed manager ctor
(`LightUtil.cpp:454-474`) instantiates the 4 subclasses with `TLightWithDBSet(0,...)`,
so `mBufferCount = 0` and the port's alloc-loop runs zero iterations. This is
wrong — the RE'd ctor at `0x80228534` explicitly sets `mBufferCount = 2` for
all four subclasses.

Evidence per subclass block, format `(size stw : store site — stw r4, 0x1c(r28) with li r4, 2)`:
- TPlayer:  `li r4,2 @ 0x80228600` → `stw r4, 0x1c(r28) @ 0x80228614`
- TMapObj:  `li r4,2 @ 0x80228680` → `stw r4, 0x1c(r28) @ 0x80228690`
- TObject:  `li r4,2 @ 0x802286fc` → `stw r4, 0x1c(r28) @ 0x8022870c`
- TIndirect:`li r4,2 @ 0x80228778` → `stw r4, 0x1c(r28) @ 0x80228788`

**Fix (small, safe):** change the 4 subclass ctors in `LightUtil.cpp` to pass
`TLightWithDBSet(2, "<name>")`. Do NOT enable makeDrawBuffer until it's ported,
because the STOPGAP in changeLightDrawBuffer checks `!mDrawBuffers` not
`mBufferCount == 0`; setting the count without the alloc is behaviorally
identical to today.

## TLightDrawBuffer alloc + owner wiring — RE'd fields

Per-iteration inside makeDrawBuffer's alloc-loop (from TIndirect disasm at
0x80228a74 onward):

```
r3 = 0x84; bl operator_new                  // alloc TLightDrawBuffer (0x84 bytes)
mDrawBuffers[i] = buf                        // stwx into mDrawBuffers array
r3 = 0x74; bl operator_new                  // alloc TLightCommon owner (0x74 bytes)
r4 = owner; bl TLightCommon::ctor(owner, name)
buf->vptr->setLight(owner)                   // vtable slot at +0x24 (setLight)
owner->mAmbBaseIdx  = ambIdx     // TLightCommon +0x20
owner->mLightBaseIdx = lightIdx  // TLightCommon +0x24
```

## Recommended landing sequence (fresh session)

1. Read rodata strings via a small python helper that decodes the DOL sections.
2. Land the `mBufferCount = 2` ctor fix as its own commit — it's the visible
   RE'd manager-ctor defect.
3. Port TIndirect::makeDrawBuffer with a spec-derived unit test on the two
   search loops (pure `search_light_by_name(&LightAry, "...") → int` helpers).
4. Clone for TMapObj + TObject with the same pattern.
5. TPlayer last — has a 3rd search loop over a stride-4 array (unknown
   container), 137 insns vs the ~110 of the others.
6. Delete the STOPGAP in `changeLightDrawBuffer` once (5) is done.

## Verification path

Unit-test only until all 4 land. The subsystem is not observably reachable
from a single-function port (mDrawBuffers is only consumed by
`changeLightDrawBuffer`/`resetLightDrawBuffer`, which need the full 4 to be
useful). Do NOT stand up whole-scene parity here — see the CLAUDE.md
"unit-test-first, oracle-only-when-COMPLETE" rule (2026-07-01).
