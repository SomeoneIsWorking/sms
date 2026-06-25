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

## What blocks flipping the default to GX-list (precise, measured)
sms-boot runs the REAL boot (no fastboot) → the scene it shows is the FILE-SELECT screen (the
Delfino plaza as backdrop + the 3 file-block cubes + a display Mario), NOT gameplay. So the
hand-driven vs GX-list comparison above IS on the file-select scene (the scene many prior sessions
tuned). Diffing per-material-key clip-AABB between the two paths at the same frame (236):
- ALL 16 hand-driven material keys are present in the GX-list dump (0 lost) + 3 new. GX-list draws
  a faithful SUPERSET; most shared keys have ~2× the verts (multi-pass: opa+xlu+reflection) with
  IDENTICAL clip extents.
- EXCEPT two keys where the hand-driving does work the real perform list does NOT:
  1. **Mario** (key c539bdd263592117): hand-driven clip x=[-57,93] (centered, in view) →
     GX-list x=[-11764,-8498] (off-screen left). drive_chr runs gpMarioOriginal->calcAnim(2)+
     calcView+entryModels (poses + positions the display Mario); the real perform list draws Mario
     via プレーヤーグループ but without that pose → wrong place.
  2. **Sky sphere backdrop** (key 89d26a8259998b13): hand-driven x=[-159357,159357] → GX-list
     COLLAPSED x=[-8,8]. drive_sky draws it with flag 0x20E (=0x204 | 0x2 setBaseTRMtx | 0x8
     GXDrawSphere); the real perform list pushes 空グループ with only 0x204 → no GXDrawSphere
     backdrop. (RE open: how real HW draws the sky backdrop — drive_sky's 0x8 may be a hand-driven
     enhancement, not the faithful mechanism.)
  3. (likely also) the file-block CUBE calc — drive_chr runs calcRootMatrix + setBaseScale(1) per
     cube because TMapObjBase::perform is an empty stub, so the real calc pass leaves them
     degenerate. (Cube key not yet isolated; verify at the settled choice state.)

→ SAFE FLIP PATH: under SB_OWN_GXLIST, let the real perform list draw the MAP, but still drive the
sky-sphere backdrop + Mario pose + cube calc INSIDE the same once-per-present capture window (these
are decomp-stub gaps the real list can't cover). Requires moving those specific drives into the
render-branch capture bracket (scene_drive currently runs after the loop, outside the lock). Then
the GX-list path is a strict superset of the hand-driven scene and the flip is safe.

## Plan (this session)
`SB_OWN_GXLIST=1` (opt-in first, verify, then flip default):
- scene_drive runs SETUP only (camera/projection latch/lights/option-camera/settle), skips the
  hand-driven draw block.
- MarDirectorDirect.cpp else-branch brackets the real perform-list render with begin_scene/end_scene
  so it's captured once per present.
- Verify via parity: ~140 batches WITH lighting applied (bright toward 138, not 193).
