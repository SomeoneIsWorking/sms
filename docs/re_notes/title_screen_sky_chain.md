# Title-screen sky-rendering chain — Ghidra RE audit (2026-07-03)

Top-down documentation of the perform-list dispatch chain that renders the title-screen
(stage-15) sky, VERIFIED against the retail `main.dol` (GMSE01) via Ghidra headless
decompilation with section-aware memory loading. Every function whose body is stated
below has been byte-verified: the Ghidra decomp of the binary matches the source in
this repository.

This chain is what the SoH-style native port ([sunbright](https://github.com/SomeoneIsWorking/sunbright))
implements — the RE'd bodies here are its ground truth.

## Ghidra loading

Install the [Cuyler36 GameCube Loader](https://github.com/Cuyler36/Ghidra-GameCube-Loader)
extension (release matching your Ghidra version) so `.dol` files import through a native
loader that parses the 7 text + 11 data sections at their real addresses and uses the
Gekko/Broadway sleigh (paired singles). Then:

```
analyzeHeadless <projdir> <projname> -import <path>/sms.dol -loader-autoloadMaps false
```

`analyzeHeadless` resolves via `$PATH`. `-loader-autoloadMaps false` is required headless
(otherwise the loader pops a "Load Symbols?" `OptionDialog` when no `.map` file sits next
to the DOL and hangs). Post-scripts (`-postScript foo.py`) need `pyghidraRun -H` under
Ghidra ≥12 since Jython is gone; `-preScript`/`-import` runs still use `analyzeHeadless`.

A legacy `BinaryLoader` + `DolLoad.py` pre-script workaround exists in the sunbright
decomp-port skill for Ghidra 11.x installs without the extension — not needed here.

## Chain (per-frame render at stage 15)

The perform-list at title stage-15 gameplay is built by:

* `TMarDirector::preEntry(TPerformList*)` @ **0x8029c1a4**
  — [`src/System/MarDirectorPreEntry.cpp:18`](../../src/System/MarDirectorPreEntry.cpp)

It appends 30+ entries; the sky-relevant ones (in order):

  ```cpp
  list->push_back("camera 1",             0x10);          // camera perform
  list->push_back("J3D System Set View Mtx", 0x4);        // view mtx
  list->push_back("DrawBuf Sky Opa",      0x480);         // frameInit + setActive(Opa)
  list->push_back("DrawBuf Sky Xlu",      0x480);         // frameInit + setActive(Xlu)
  list->push_back("空グループ",           0x204);         // ENTRY sky-group actors → active Sky bufs
  list->push_back("DrawBuf MapOpa",       0x480);
  list->push_back("DrawBuf MapXlu",       0x480);
  list->push_back("マップグループ",       0x204);         // ENTRY map-group actors → active Map bufs
  ...
  ```

The `push_back(const char*, u32)` overload wraps each in a `TPerformLink` (name, flag)
resolved lazily via `TNameRefGen::search`.

### Walker

* `TPerformList::perform(u32 flag, TGraphics*)` @ **0x802a4e28**
  — [`src/System/PerformList.cpp:20`](../../src/System/PerformList.cpp)

  ```cpp
  void TPerformList::perform(u32 param_1, TGraphics* param_2) {
      forEachPerform(getChildren().begin(), getChildren().end(), param_2, param_1);
  }
  // forEachPerform: for each link, link->unk4->testPerform(param_4 & link->unk8, param_3);
  ```

Ghidra body (section-aware, 216 bytes) — matches.

### Sky-group actor

* `TSky::perform(u32 flag, TGraphics*)` @ **0x8019ffe4**
  — [`src/Map/Sky.cpp:17`](../../src/Map/Sky.cpp)

  ```cpp
  void TSky::perform(u32 param_1, TGraphics* param_2) {
      if (param_1 & 2) {
          Mtx local_4c;
          MTXInverse(gpCamera->unk1EC, local_4c);
          Mtx afStack_7c;
          MTXIdentity(afStack_7c);
          afStack_7c[0][3] = -(local_4c[0][2] * gpCamera->unk1EC[2][3]
                             - (-local_4c[0][0] * gpCamera->unk1EC[0][3]
                                - local_4c[0][1] * gpCamera->unk1EC[1][3]));
          // ... [1][3], [2][3] similar
          if (gpMarDirector->mMap == 15) {
              // rotation matrix from unk48 (degrees) — animated 0.035°/frame per unk4C default
              Mtx local_ac;
              f32 s = sinf(unk48 * 0.017453294f);
              f32 c = cosf(unk48 * 0.017453294f);
              // local_ac = rotY(unk48°)
              MTXConcat(local_ac, afStack_7c, afStack_7c);
              unk48 += unk4C;
              unk48 = MsWrap<f32>(unk48, 0.0f, 360.0f);
          }
          unk44->getModel()->setBaseTRMtx(afStack_7c);
      }
      unk44->perform(param_1, param_2);        // MActor — dispatches sky.bmd shapes
      if ((param_1 & 8) != 0) {
          // GC-only backdrop: solid opaque sphere with material colour (0x00, 0x12, 0xEE, 0x80).
          // Not called under the stage-15 preEntry flag 0x204 (bit 3 clear); TSmJ3DScn::perform
          // walks its drawbufs later with 0x8, which is when GXDrawSphere fires.
          GXClearVtxDesc();
          GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
          GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
          GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
          GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
          GXSetCullMode(GX_CULL_FRONT);
          GXSetNumChans(1);
          GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 1, GX_DF_CLAMP, GX_AF_NONE);
          GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,  GX_AF_NONE);
          GXSetChanMatColor(GX_COLOR0A0, (GXColor){0x00, 0x12, 0xEE, 0x80});
          GXSetNumTexGens(0);
          GXSetCurrentMtx(GX_PNMTX0);
          Mtx s; MTXScale(s, 100000.f, 100000.f, 100000.f);
          GXLoadPosMtxImm(s, GX_PNMTX0);
          GXLoadNrmMtxImm(s, GX_PNMTX0);
          GXSetNumTevStages(1);
          GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
          GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
          GXSetZCompLoc(1);
          GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
          GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
          GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
          GXDrawSphere(8, 0x10);
      }
  }
  ```

Ghidra body (section-aware, 856 bytes) — matches.

### Model shape entry

* `MActor::perform(u32 flag, TGraphics*)` @ **0x802391bc**
  — [`src/M3DUtil/MActor.cpp:401`](../../src/M3DUtil/MActor.cpp)

  ```cpp
  void MActor::perform(u32 param_1, TGraphics* param_2) {
      if (param_1 & 0x2)   calcAnm();
      if (param_1 & 0x4)   viewCalc();
      if (param_1 & 0x200) entry();       // pushes each sky.bmd shape into the currently-active drawbuf
  }
  ```

Ghidra body (section-aware) — matches.

### Draw-buffer flush

* `JDrama::TDrawBufObj::perform(u32 flag, TGraphics*)` @ **0x802f830c**
  — [`src/JSystem/JDrama/JDRDrawBufObj.cpp:68`](../../src/JSystem/JDrama/JDRDrawBufObj.cpp)

  ```cpp
  void TDrawBufObj::perform(u32 flag, TGraphics*) {
      if (flag & 0x80)    mDrawBuffer->frameInit();       // preEntry(0x480)
      if (flag & 0x400) { // preEntry(0x480) — setActive
          if (unk18 & 3)  j3dSys.setDrawBuffer(mDrawBuffer, 0);   // 0 = OPA slot
          if (unk18 & 4)  j3dSys.setDrawBuffer(mDrawBuffer, 1);   // 1 = XLU slot
      }
      if (flag & 0x8) {                                   // TSmJ3DScn::perform (draw pass)
          j3dSys.setNBTFlag(unk18);
          mDrawBuffer->draw();                            // walks entries, calls J3DShape::draw per
      }
  }
  ```

Ghidra body (section-aware, 164 bytes) — matches.

### EFB-target actor (the sky ph1 render-target-substrate)

* `JDrama::TEfbCtrlTex::perform(u32 flag, TGraphics*)` @ **0x802f8bac**
  — [`src/JSystem/JDrama/JDREfbCtrl.cpp:80`](../../src/JSystem/JDrama/JDREfbCtrl.cpp)

  This is the actor that on the ph1 pass CLAIMS an EFB region as a render target and,
  on `flag & 0x8`, ISSUES `GXCopyTex(mImagePtr, doClear)` to snapshot the drawn
  contents into a texture whose consumer is a later composite pass. **This is why the
  sky-XLU-buffered cloud-strip draw at ph1 renders off-screen on GC** — the copy
  destination becomes a texture, not visible pixels; a subsequent composite pass
  samples it. Native PC ports (sunbright) that don't emulate EFB→texture copies MUST
  filter the ph1 draw of the cloud strip (shaderKey `0x7bc0841d`), or the SRCALPHA/ONE
  additive blend double-draws over the visible main scene. See
  [sunbright commit `f44e45f`](https://github.com/SomeoneIsWorking/sunbright/commit/f44e45f)
  for the resulting port.

  Ghidra body (section-aware, 504 bytes) — matches the source above.

## Perform-flag summary (title stage-15)

| perform-list flag | when it fires | who runs                             |
|-------------------|---------------|--------------------------------------|
| 0x480             | frameInit + setActive on a DrawBuf   | `TDrawBufObj::perform`                |
| 0x204             | frameInit + entry on a group actor   | children's `::perform`; MActor→entry  |
| 0x0008            | draw pass (TSmJ3DScn walking bufs)   | `TDrawBufObj::perform` → `.draw()`    |

## Cloud-strip material RE (shaderKey 0x7bc0841d43634f53)

Extracted from the runtime-generated fragment shader (byte-exact TEV port):

```glsl
// stage0:  prev.rgb = TEX(uv0) × RASC             (RASC = white; matSrc = REG(255,255,255))
//          prev.a   = TEX(uv0).a × RASC.a
// stage1:  prev.rgb = clamp((TEX(uv1) × prev) << 1, 0, 255)  = min(2 × t0 × t1, 1)   [TEV_SCALE_2]
//          prev.a   = clamp((TEX(uv1).a × prev.a) << 1, 0, 255) = min(2 × t0 × t1, 1)
// blend:   SRC_ALPHA / ONE  (additive)
```

The 8×8 texture is I4 (r=g=b=a=intensity per texel). The dispatched shape is a 40-vertex
strip in sky.bmd; its two texgens sample the same I4 asset with different UV matrices,
producing the animated cloud puff pattern via a `t0 × t1` intersection.

## Verification procedure

1. Import the DOL via the GameCube-loader extension (see "Ghidra loading" above).
2. `pyghidraRun -H <projdir> <projname> -process sms.dol -noanalysis
    -postScript DecompDump.py` with `DECOMP_TARGETS` file containing the addresses.
3. Diff Ghidra decomp against the C body in this repo.

All decomps in this chain match the source. Any drift found during future audits
should be committed here alongside a corrected source body.
