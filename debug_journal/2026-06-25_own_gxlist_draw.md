# GX perform-list vs hand-driven scene_drive — measurements — 2026-06-25

## ⛔ DIRECTION CORRECTION (user, 2026-06-25): DO NOT FLIP / DO NOT retire hand-driving
The user explicitly rejected the "make the GX-list capture the default / retire the hand-driving"
plan ("we are making a PC port of this game, don't flip anything"). The chosen direction is
**EXTEND scene_drive (the PC-native walk)** to draw more of the scene — NOT capture/replay the GC
perform-list dispatch. `SB_OWN_GXLIST` (committed f64dbeb / reference f67feb2) stays only as an
OPT-IN DIAGNOSTIC that MEASURES what the real perform-list flow draws (it never changes the
default). The measurements below are still valid; the "own the GX list / flip" framing is dead.

## Why naive scene_drive extension (drive the priority/reflection map buffers) adds NOTHING (measured)
Tried adding drive_group("マップ", 0x4000200 / 0x2000200) for the Map 半透明優先 / 優先2 buffers and
drive_group1("落書きグループ", Graffito): groups FOUND, but 0 extra batches (still 57). Root cause:
those priority/reflection buffers are populated by the real list's **マップグループ→DrawBuf MapOpa**
entry path (preEntry: マップグループ@0x204 → MapOpa, THEN マップ@0x4000200 → 優先). The hand-driven
scene_drive draws the map via a DIFFERENT path — **scene->perform(0x8)** (ストラテジ→マップグループ into
the SCENE's own buffers) — so マップ's priority entry has no base to build on and yields nothing.
The two map-draw mechanisms are mutually exclusive: an earlier session ADDED drive_map
(マップグループ→MapOpa) and it Z-FOUGHT the scene->perform(0x8) map (duplicate opaque surfaces), so
it was removed. To faithfully get the priority/reflection layers, scene_drive would have to SWITCH
the map base from scene->perform(0x8) to the マップグループ→MapOpa path (+ mirror/indirect/priority),
i.e. a larger refactor — not a naive add. (Reverted the dead drive_map_layers; 0 measured benefit.)

## The 57→130 gap is mostly redundant passes + minor UI, NOT missing visible scene
Per-material-key diff (frame 236): GX-list (130 scene batches) vs hand-driven (57): 16/16 shared
keys, GX-list has ~2× verts on them (= same map materials drawn AGAIN in MapOpa + reflection
buffers — overdraw, not new pixels). Only 3 keys are genuinely GX-list-only (69ebca44 / da39a890 /
85b2f9ca): small on-screen textured elements upper-centre (clipX~[-90,40] clipY~[130,350]) + an
off-screen [-8600] duplicate each — minor file-select UI (sun icons / labels class), not the map.

## DOMINANT visible problem = the scene renders OVERBRIGHT-WHITE (xfb bright 138-193)
All frame dumps are white; eyeballing is invalid (parity rules already say so). This — not missing
geometry — is what blocks "runs fine". The plaza materials are NOT GX-lit (chanCtrl lighting bit
off, cc0=0x0700) so they draw matColor(white)×texture unshaded. Deferred per the "more porting
before parity debugging" directive, but it is the real gate to visual verification.


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

## ⭐ OVERBRIGHT-WHITE ROOT-CAUSE LOCALIZATION (the real "runs fine" blocker)
The scene renders overbright-WHITE (scene-band mean 254.5/255). Localized it step by step with new
default-off diagnostics in native/render/sms_boot_present.cpp (SB_SKIP_IMM, SB_OPAQUE_ONLY,
SB_TEXTURED_ONLY) + the existing SB_TEV_SOLID + SUNBRIGHT_NGX_TEVDBG=tex:
- NOT a 2D overlay/fader: `SB_SKIP_IMM=1` (drop all 2D imm batches) → 3D scene STILL white.
- NOT blend/additive accumulation: `SB_OPAQUE_ONLY=1` (opaque scene batches only) → STILL white.
- NOT the combiner structure: the dominant map material (224004d9, ×28) has fogType=0, nstg=1,
  matC0 white, combiner color_env=0x08f8af = **RASC×TEXC** (raster × texture) — correct GX_MODULATE.
- Textures DECODE fine: the 71 dumped scene-batch textures span brightness 16–245 (real content),
  and nvk uploads them (makeTexture + sampler, nvk.cpp ~L1007).
- DECISIVE: `SB_TEV_SOLID` (raster-only) == normal == `SUNBRIGHT_NGX_TEVDBG=tex` (raw
  `texture(tex[0],vUV[0])`) == `SB_TEXTURED_ONLY=1`+TEVDBG=tex == 254.5 white. If the texture were
  reaching the shader the textured render would be DARKER than raster-only. It isn't.
→ ROOT CAUSE (localized, not yet fixed): **scene-batch texture sampling returns WHITE.** Even the
  hardcoded simple sampler `o = texture(tex[0], vUV[0])` (TEVDBG=tex, same shader the WORKING 2D imm
  textured batches use) returns white for textured 3D scene batches. So the geometry+raster+combiner
  are fine; the texture is not reaching the fragment for scene batches. Suspects to chase NEXT:
  (a) the scene vertex's vUV[0] in the shader ≠ the v.uv[0] the capture wrote (make_v DOES write
      tv.uv via sb_texgen_uv — capture L645-648 — so check the vertex-attribute binding / tev.vert
      reads uv for scene verts; imm verts work, so compare the two vertex layouts/bindings), OR
  (b) the per-batch sampler/descriptor binding in renderTevFrame isn't actually wiring scene
      b.tex[0] to sampler unit 0 for these batches (vs imm), OR
  (c) the texgen UVs are real-valued (b6 uv[-3.71,12.77;0,1]) but sample a uniform/white region.
  Repro: `SUNBRIGHT_NGX_TEVDBG=tex SB_SKIP_IMM=1 SB_TEXTURED_ONLY=1 SB_FRAME_DUMP=1
  SB_FRAME_DUMP_START=240 SB_FRAME_DUMP_MAX=1 ... ./build-native/sms-boot` → boot_0240 is white;
  oracle (pure Dolphin) shows a normal-bright plaza. This is THE thing to fix to match the oracle.

## ⚠️ CORRECTION on the overbright cause — it is NOT a lighting bug, NOT a texture-sample bug
Dug deeper and FALSIFIED two earlier leads:
- **"texture samples white" was a misread.** The textured-only render DOES show the buildings (faint
  roof/window edges) — the wall/floor/sky textures are just genuinely BRIGHT. Dumped map textures are
  real cream stucco (e.g. (254,253,228)) + some dark (16). Textures decode + sample fine.
- **"lighting mis-decoded (me->lit wrong)" is FALSE.** J3DColorChan bit layout (verified in
  include/.../Components/J3DColorChan.hpp): `getMatSrc()=mChanCtrl>>0&1`, **`getEnable()=mChanCtrl>>1&1`**.
  The dominant map material chanCtrl=0x701 → bit1(enable)=0 → lighting genuinely DISABLED;
  bit0(matSrc)=1 → raster sources from the VERTEX colour. So `me->lit=false` / `do_light=0` are
  CORRECT — these materials are not GX-lit. Do NOT "fix lighting" for them.
So the map raster = vertex CLR0 (0x701 materials; the capture DOES read it with real variance —
batchdbg b6 rgb 0.81 cvar 0.22, b7 0.44/0.76/0.79 cvar 0.24) or white matColor (0x700) — then ×
the (bright) texture. The remaining overbright question is therefore one (or more) of:
  1. The SKY/upper area renders WHITE not blue — the sky batch raster is blue (bbox b0 0,0.07,0.93)
     but the sky uses SCREEN/additive blend (CLAUDE.md: sky ti=11 src=ONE dst=INVSRCCLR); additive
     layers with no fog stack toward white. FOG IS UNIMPLEMENTED (sms_boot_material.cpp L123-127) —
     the game fogs distant/additive layers down; sms-boot leaves them full-bright. Strong suspect.
  2. The plaza floor/walls render brighter than the oracle — compare sms-boot pixels to the
     oracle xfb grid (oracle.jsonl has it) region-by-region to QUANTIFY where the excess is, instead
     of "it's white". Build that compare (oracle xfb grid vs sms-boot frame downsampled to 4x4).
  3. Vertex CLR0 values: confirm the 0x701 baked vertex colours match the oracle (a baked-AO/shading
     channel read too bright → unshaded buildings). batchdbg shows variance, so they're read, but
     verify magnitude vs oracle.
NEXT CONCRETE STEP: implement GX FOG (the one clearly-missing pipeline stage, L123-127) — it is the
most likely single cause of additive/distant layers blowing white — then re-measure brightness vs
the oracle xfb grid. If fog doesn't close it, do the region-by-region oracle-xfb vs sms-boot compare
to localize the excess before touching anything else.

## Plan (this session)
`SB_OWN_GXLIST=1` (opt-in first, verify, then flip default):
- scene_drive runs SETUP only (camera/projection latch/lights/option-camera/settle), skips the
  hand-driven draw block.
- MarDirectorDirect.cpp else-branch brackets the real perform-list render with begin_scene/end_scene
  so it's captured once per present.
- Verify via parity: ~140 batches WITH lighting applied (bright toward 138, not 193).
