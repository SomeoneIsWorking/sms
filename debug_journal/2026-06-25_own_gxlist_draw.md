# Own the GX perform-list draw path (retire hand-driving) — 2026-06-25

## Correction to the LATE-5 handoff premise
The handoff said the GX-list-only path (`SB_NO_DRIVE_SCENE=1`) "draws 130 batches but bbox is
extreme (x[±239008]) → a VIEW/MATRIX WIRING gap." **The bbox-extreme signal is a CONFOUND**, not
evidence of a wiring gap:
- The DEFAULT hand-driven path (which renders the plaza fine) shows the SAME extreme bbox
  `x[±239008]` — a large surface (ground/sea plane) legitimately extends far past the screen edges,
  so per-batch max-|ndc| is huge in BOTH modes. A naive "|ndc|<2" per-batch filter classified
  ALL batches as "extreme" in BOTH modes (sane=0/extreme=57 hand-driven, sane=0/extreme=130 GX-list).
  Useless as a discriminator.

## The right metric: parity dump per-batch `onscr` + displayed batch count
Generated parity dumps (`SB_PARITY_DUMP`) at a content frame (~236-241, before the DELFINO PLAZA
stage-name flash whites the screen ~frame 300+):
- **oracle (pure Dolphin)**: nbatch=155, nverts=225060, onscr=224960 (~99%)
- **hand-driven native** (frame 241): nbatch=60, 34/60 batches on-screen, onscr verts=20202, xfb bright=138
- **GX-list native** (SB_NO_DRIVE_SCENE, frame 236): nbatch=140, 55/140 batches on-screen,
  onscr verts=33907, xfb bright=193

→ The GX perform-list path draws FAR more of the scene (140 ≈ oracle 155, vs hand-driven 60) and
more on-screen geometry (55 batches / 33907 verts vs 34 / 20202). It is the richer, more faithful
draw path. Confirms the directive: own the GX list, retire the hand-driving.

Caveat: the plaza renders OVERBRIGHT WHITE in BOTH modes (xfb bright 138-193) — a known pre-existing
overbright issue. So visual eyeballing is invalid; ALL verification here is value-level (parity dump).
GX-list is brighter (193 vs 138) likely because scene_drive's manual GX light load (the stubbed
TLightCommon path) does NOT run in SB_NO_DRIVE_SCENE mode → materials draw full-bright unlit. Owning
the path must keep that light load running.

## Capture/lock mechanism (native/render/sms_boot_j3d_capture.cpp)
The capture taps `J3DShape::draw`. `g_locked` gates it; `begin_scene`/`end_scene` bracket exactly one
capture per VI present (begin_scene returns 1 only on the first call after a present `take` re-armed
`g_want_capture`). Currently scene_drive brackets the HAND-DRIVEN walk; the real GX perform-list draws
(MarDirectorDirect.cpp else-branch: mPerformListGX/GXPost) are skipped (g_locked stays true between
presents). SB_NO_DRIVE_SCENE bypasses scene_drive entirely so g_locked never locks → the real path is
captured (accumulating, but ~stable at 140).

## Plan (this session)
`SB_OWN_GXLIST=1` (opt-in first, verify, then flip default):
- scene_drive runs SETUP only (camera/projection latch/lights/option-camera/settle), skips the
  hand-driven draw block.
- MarDirectorDirect.cpp else-branch brackets the real perform-list render with begin_scene/end_scene
  so it's captured once per present.
- Verify via parity: ~140 batches WITH lighting applied (bright toward 138, not 193).
