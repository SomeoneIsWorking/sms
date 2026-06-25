# SB_NPC_ON map-gone: double draw-buffer entry corrupts the shared chain (FIXED)

## Symptom
With `SB_NPC_ON=1`, the Delfino plaza map vanished — the captured gameplay scene collapsed to
**16 shapes / 7242 verts (Mario + sky only)** vs NPC-off's **56 shapes / 133461 verts**. The hang
and OOM flood were already fixed (see handoff LATE-3); this was the remaining frontier.

## Root cause (proven, not deduced)
`native/src/scene_drive.cpp` hand-drives the whole render by calling `scene->perform(0x8)` on
`通常シーン` (TSmJ3DScn). That blanket walk recurses into EVERY scene child with flag 0x20C. Two of
those children both draw the NPCs into the SAME scene draw buffer in one frame:

- **ストラテジ (TStrategy)** child #2 → `TStrategy::perform(8)` walks group `unk10[9]` = **ＮＰＣグループ**
  (31 Monte townspeople) and enters their J3DMatPackets into the scene buffer.
- **コンダクター (TConductor)** child #4 → its manager walk (`TObjManager::perform` over the i0
  `!hasMapCollision` NPC managers) enters the SAME NPC packets again.

A `J3DMatPacket` has a SINGLE `next` pointer (`unk4`). `J3DDrawBuffer::entryMatSort` calls
`packet->drawClear()` (J3DPacket.hpp:54 → `unk4 = nullptr`) at the TOP of every entry, then the
`isSame` dedup path returns WITHOUT re-linking. So the conductor's re-entry of an
already-chained NPC packet nulls its `next`, truncating the shared chain at that packet. Because
ストラテジ enters the NPC group LAST (prepended → head of the chain), the truncation orphans
everything entered before it — the entire map.

### Evidence chain (tooling added, all env-gated)
- `SB_SCENE_BUF=1` (scene_drive): scene buffers held 50+10 packets NPC-off, **1+1** NPC-on.
- Per-child bisect: ストラテジ entered +178/+48; コンダクター then netted **−177/−47** (chain broke).
- `SB_FI_TRACE=1` (J3DDrawBuffer::frameInit watch): the only frameInits on the scene buffers were
  the legitimate once-per-frame reset from `TSmJ3DScn::perform` — proving it was NOT a reset but a
  `next`-pointer truncation.
- `SB_COND_TRACE` (temporary, reverted): the drop happened precisely in the i0 NPC-manager walk.
- `SB_STRAT_DUMP=1` (Strategy.cpp): confirmed group[9] = ＮＰＣグループ with the 31 Monte actors.

### Why real hardware never hits this
The real draw is the master GX perform list `MarDirectorPreEntry::preEntry`, which **never pushes
ストラテジ**. It draws each group once into a DEDICATED buffer: `マップグループ → MapOpa/MapXlu`, and
the NPCs exclusively via `コンダクター (0x204)` into `ChrOpa/ChrXlu` (set active just before it).
`TStrategy::perform(8)` drawing ＮＰＣグループ is dead in practice on HW (nothing performs ストラテジ
with 0x8). sms-boot's `scene->perform(0x8)` blanket walk resurrects that dead path → the double entry.

## Fix
`src/Strategic/Strategy.cpp` `TStrategy::perform`, under `SMS_NATIVE_PLATFORM`: do NOT draw
`unk10[9]` (ＮＰＣグループ) in the 0x8 block. The NPCs are drawn once by the conductor (the
real-HW path), restoring single-entry. Map is still drawn via ストラテジ's `マップグループ` (unk10[0]).

Result: NPC-on now captures **222 shapes / 217737 verts** (map + NPCs), the plaza renders
correctly with Mario, NPC-off fully unregressed (56/133461), OOM flood still 1 (benign).

## Note for the future faithful path
This is consistent with the standing directive to OWN THE PERFORM-LIST DISPATCH. The blanket
`scene->perform(0x8)` is a hand-driven bridge that doesn't match HW's per-group/per-buffer
sequencing. The proper end-state drives `MarDirectorPreEntry`'s list (map→MapOpa, NPCs→ChrOpa,
etc.) so each model enters exactly one dedicated buffer — at which point this group[9] suppression
becomes unnecessary. Until then, the suppression is the minimal faithful fix.
