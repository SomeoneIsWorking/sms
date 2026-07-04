#include <GC2D/SunGlass.hpp>
#include <System/MarDirector.hpp>
#include <JSystem/JDrama/JDRRect.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <dolphin/gx.h>
#include <cmath>
#include <cstdlib>

// Pure-logic unit shared with the unit test (spec-derived expected values live there).
// See sms_boot_sunglass.h for the semantics comments. This shipping port calls into the
// helpers rather than duplicating the fade math — same discipline as ShadowUtil.cpp.
// Path-B-only: the Aurora Path A build takes the original GC path below unmodified.
#ifdef SMS_NATIVE_PLATFORM
#include "sms_boot_sunglass.h"
#endif

#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
static bool sSunGlassDbg()
{
	static int v = -1;
	if (v < 0) {
		const char* e = getenv("SB_SUNGLASS_DBG");
		v = (e && *e && *e != '0') ? 1 : 0;
	}
	return v;
}
#define SG_LOG(...) do { if (sSunGlassDbg()) std::fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define SG_LOG(...) do {} while (0)
#endif

// Native port of GC2D/SunGlass.cpp (@0x8017cfc4..0x8017d574). Two classes:
//  - TSunGlass: a fullscreen coloured overlay (JUtility::TColor unk14) with an alpha fade
//    animation; used at file-select as the "SunGlass fader" (constructed via MarNameRefGen
//    as TSunGlass(TColor(0,0,0,80)) — a semi-transparent black tint).
//  - TSunShine: subclass, used to flash the screen white when Mario collects a shine
//    (ctor colour TColor(135,135,135,0) — invisible at rest).
//
// RE sources (all in scratch/decomp_sunglass/, Ghidra headless on scratch/bin/sms_flat.bin):
//   TSunShine::~TSunShine   @0x8017cfc4  (8017cfc4.c)  — trivial dtor, not ported here
//   TSunShine::loadAfter    @0x8017d048  (8017d048.c)  — not analysed; not required at file-select
//   TSunShine::perform      @0x8017d0a4  (8017d0a4.c)  — reads getShineAlpha + emits particle on collect
//   TSunGlass::load         @0x8017d180  (8017d180.c)  — attaches gpMarioGamePad from MarDirector
//   TSunGlass::loadAfter    @0x8017d1bc  (8017d1bc.c)  — initial alpha from getShineAlpha
//   TSunGlass::perform      @0x8017d264  (8017d264.c)  — fade step + draw
//   TSunGlass::draw         @0x8017d354  (8017d354.c)  — GX_QUADS fullscreen coloured quad
//   TSunGlass::startFade    @0x8017d574  (8017d574.c)  — sets start/end/reset step counter
//
// DOCUMENTED GAPS (kept honest per CLAUDE.md's no-bandaid rule):
//  1. getShineAlpha() reads two SDA2 float constants I have NOT resolved from the DOL. It
//     also depends on gpMarioGamePad->unk7C ("rumbleMode" or similar per Application layout).
//     Ported as a documented stub returning 0 — matches file-select's expected behavior
//     (no active shine gauge → invisible overlay); needs DOL-constant resolution + shine-gauge
//     integration to correctly reflect a real gameplay shine collection.
//  2. TSunShine::perform's particle-emit branch (FUN_80325300 = JPAEmitterManager::createSimple
//     or similar) is skipped — file-select doesn't collect shines. Documented.
//  3. TSunGlass::load's gamepad attach reads gpMarDirector->unk18[1] — kept faithful to the
//     decompile even though the +1 index is unexplained (would be pad #1, not #0; possibly a
//     -1-adjusted pad-array base). Null-guarded so a missing director doesn't crash.

// -----------------------------------------------------------------------------
// TSunGlass
// -----------------------------------------------------------------------------

// Faithful skeleton: JDrama::TViewObj::load reads the NameRef header (matches TGuide/
// TPauseMenu2 pattern) so name-based search still finds this object, then attaches the
// primary game pad pointer from MarDirector (per the RE @0x8017d180).
void TSunGlass::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);
	if (gpMarDirector && gpMarDirector->unk18) {
		// Decompile literally reads unk18[1] via (int*)((char*)unk18+4). Kept faithful; a real
		// board could route this to gpApplication.mGamePads[0] if the +1 turns out to be a
		// -1-adjusted base. Null-checked so a not-yet-populated director doesn't crash.
		unk10 = *(gpMarDirector->unk18 + 1);
	} else {
		unk10 = nullptr;
	}
}

// TSunGlass::getShineAlpha (inlined into perform/loadAfter/startFade at the GC side). See the
// DOCUMENTED GAP #1 above — returns 0 as an honest stub. The header exposes it publicly per
// the RE (SunGlass.hpp:30), so leaving it callable while flagging the gap.
namespace {
inline u8 sunglass_get_shine_alpha(TSunGlass* /*self*/) {
	// STOPGAP: honest zero until DOL SDA2 constants at [-0x4658]/[-0x465c]/[-0x4650] +
	// TMarioGamePad's shine-gauge accessor are resolved. Returning 0 = invisible overlay,
	// which is the expected file-select rest state (no shine collection there).
	return 0;
}
}  // anonymous ns

void TSunGlass::getShineAlpha() { /* pure-inline in GC; kept as a no-op vtable-adjacent method */ }

void TSunGlass::loadAfter()
{
	unk14.a = sunglass_get_shine_alpha(this);
}

void TSunGlass::changeAlpha(u8* /*out*/) { /* inlined at GC side; kept for header symmetry */ }

void TSunGlass::startFade(int mode, bool active)
{
	// RE @0x8017d574. Two modes:
	//  - mode == 2: fade INTO the shine-alpha over unk22 frames — unk1C=100 (start), unk1D=getShineAlpha (end).
	//  - else:      fade OUT to unk22 frames                    — unk1C=getShineAlpha (start), unk1D=100 (end).
	const u8 sh = sunglass_get_shine_alpha(this);
	if (mode == 2) {
		unk1D = sh;
		unk1C = 100;
	} else {
		unk1D = 100;
		unk1C = sh;
	}
	unk26 = active ? 1 : 0;
	unk24 = 0;
	SG_LOG("[sunglass] startFade mode=%d active=%d start=%u end=%u total=%u\n",
	       mode, (int)active, unk1C, unk1D, unk22);
}

void TSunGlass::perform(u32 flags, JDrama::TGraphics* graphics)
{
	// Fade progression on the ANIM bit (0x1) — the pure sb::sunglass_fade_* helpers are the
	// unit-tested spec truth. The port SHIPS through them so the test validates the real code.
	if ((flags & 1u) != 0 && unk26 != 0) {
#ifdef SMS_NATIVE_PLATFORM
		unk14.a = sb::sunglass_fade_step_alpha(unk1C, unk1D, unk24, unk22);
		sb::FadeAdvance adv = sb::sunglass_fade_advance(unk24, unk22);
		unk24  = adv.new_cur;
		unk26  = adv.new_active ? 1 : 0;
#endif
	}
	// Draw on the DRAW bit (0x8) — virtual dispatch so TSunShine's own draw (if it overrode)
	// runs; the decompile calls through vtable slot +0x24, which is draw(rect, color).
	if ((flags & 8u) != 0) {
		JUtility::TColor tmp = unk14;
		if (graphics) {
			// graphics + 0x54 = TGraphics::mViewport as a TRect (per the RE offset). Access
			// via the public struct instead of raw byte offset.
			draw(graphics->getViewport(), tmp);
		}
	}
}

// TSunGlass::draw @0x8017d354. Emits a fullscreen coloured quad (GX_QUADS with 4 verts).
// Matches TMBindShadowManager::drawShadow / TMapObjWave::draw's raw-GX pattern — no
// renderer-specific wiring; the native SDL_GPU capture picks it up automatically.
void TSunGlass::draw(const JDrama::TRect& rect, JUtility::TColor colour)
{
	if (colour.a == 0) return; // early-out: fully transparent → no visible effect.

	// Vertex format: POS(f32 xyz) + CLR0(rgba8), no textures, no lighting.
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	// Identity view matrix — a fullscreen overlay is drawn in screen space (or through an
	// ortho projection the drawing pipeline expects to already be active for HUD-class draws).
	static const float ident[3][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0}};
	GXLoadPosMtxImm((float(*)[4])ident, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);

	// Stage 0: colour = raster (from vertex CLR0), alpha = raster.
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, (GXTevColorArg)0xF, (GXTevColorArg)0xF,
	                (GXTevColorArg)0xF, (GXTevColorArg)0xA);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, (GXTevAlphaArg)7, (GXTevAlphaArg)7,
	                (GXTevAlphaArg)7, (GXTevAlphaArg)0);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	// The RE @0x8017d354 chooses the blend mode by an unk1A flag: bit 2 clear → standard alpha
	// blend, bit 2 set → additive (SRCALPHA * ONE) — the "SunShine" bright flash uses the latter.
	if ((unk1A & 2u) == 0) {
		GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	} else {
		GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	}
	GXSetAlphaCompare(GX_GEQUAL, 0, GX_AOP_AND, GX_LEQUAL, 255);
	GXSetZMode(GX_TRUE, GX_ALWAYS, GX_FALSE); // depth-test disabled (screen overlay)
	GXSetCullMode(GX_CULL_NONE);

	// 4 corners of the rect. Z=0 (in the identity/ortho HUD projection this reads as
	// mid-clip-space, in front of any 3D scene — the tint composites over everything).
	// NOTE: locals are lx0/ly0/lx1/ly1 (NOT y1 — that shadows the libm y1() Bessel function).
	const f32 lx0 = (f32)rect.x1, ly0 = (f32)rect.y1;
	const f32 lx1 = (f32)rect.x2, ly1 = (f32)rect.y2;
	const u8 r = colour.r, g = colour.g, b = colour.b, a = colour.a;

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(lx0, ly0, 0.0f); GXColor4u8(r, g, b, a);
	GXPosition3f32(lx1, ly0, 0.0f); GXColor4u8(r, g, b, a);
	GXPosition3f32(lx1, ly1, 0.0f); GXColor4u8(r, g, b, a);
	GXPosition3f32(lx0, ly1, 0.0f); GXColor4u8(r, g, b, a);
	GXEnd();
}

// -----------------------------------------------------------------------------
// TSunShine (subclass — different draw colour, different flash trigger)
// -----------------------------------------------------------------------------

void TSunShine::loadAfter()
{
	// TSunShine's loadAfter (@0x8017d048) is not analysed here; deferring to the base class'
	// initial alpha compute is faithful enough for file-select (where TSunShine isn't triggered).
	TSunGlass::loadAfter();
}

void TSunShine::perform(u32 flags, JDrama::TGraphics* graphics)
{
	// The RE @0x8017d0a4 is simpler than TSunGlass's: no fade-step arithmetic — the alpha is
	// recomputed from getShineAlpha every anim tick. Shine-collect detection (FUN_80273928)
	// + JPA particle emit (FUN_80325300) is a DOCUMENTED GAP (see above); kept out until the
	// shine-gauge accessor is wired.
	if ((flags & 8u) != 0 && graphics) {
		JUtility::TColor tmp = unk14;
		draw(graphics->getViewport(), tmp);
	}
	if ((flags & 1u) != 0) {
		unk14.a = sunglass_get_shine_alpha(this);
	}
}
