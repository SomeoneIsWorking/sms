#include <MarioUtil/ShadowUtil.hpp>
#include <Map/Map.hpp>
#include <Strategic/HitActor.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <dolphin/gx.h>
#include <cmath>
#include <cstdlib>

#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
static bool sShadowDbg()
{
	static int v = -1;
	if (v < 0) {
		const char* e = getenv("SB_SHADOW_DBG");
		v = (e && *e && *e != '0') ? 1 : 0;
	}
	return v;
}
#define SHADOW_LOG(...) do { if (sShadowDbg()) std::fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define SHADOW_LOG(...) do {} while (0)
#endif

// Native port of ShadowUtil (MarioUtil/ShadowUtil.cpp @0x8022c000-0x80233800). Serves
// TMBindShadowManager (the shared per-frame ground-shadow accumulator) and TMBindShadowBody
// (the actor-side helper used by TMario and other actors that want a rich per-joint shadow).
//
// RE sources (all in scratch/decomp_shadow/, done via Ghidra headless on scratch/bin/sms_flat.bin):
//   calcVtx           @0x8022e0cc     (scratch/decomp_shadow/8022e0cc.c)
//   forceRequest      @0x8022ebbc     (8022ebbc.c)
//   request           @0x8022ecec     (8022ecec.c)
//   drawShadow        @0x8022f014     (8022f014.c)
//   drawShadowGD      @0x8022fa40     (8022fa40.c)
//   drawShadowVolume  @0x802305dc     (802305dc.c)
//   perform           @0x80231108     (80231108.c)
//   load              @0x80231288     (80231288.c)
// Field semantics for TCircleShadowRequest: cross-verified against TLiveActor::requestShadow()
// (reference/sms/src/Strategic/liveactor.cpp:307, fully decompiled, byte-matched).
//
// Design (see the extended comment blocks in MarioUtil/ShadowUtil.hpp for the full rationale):
//   * Requests are accumulated into TMBindShadowManager::mRequests during the frame's control
//     phase (actors call ::request()/::forceRequest()).
//   * calcVtx() ground-projects each request via gpMap->checkGround → TFootprint list.
//   * drawShadow() emits an alpha-blended dark decal (a small triangle-fan disc) per footprint
//     using GXBegin/GXPosition3f32/etc — the SAME real-GX path TMapObjWave::draw() rides, so
//     the native SDL_GPU capture picks it up with zero renderer-specific wiring.
//   * perform() runs calcVtx() then drawShadow() and clears the queues — self-contained;
//     see the "reset() timing" comment in the handoff/hpp.
//   * The GC original's TAlphaShadowBlendQuad linked-list clustering + Z-buffer-as-stencil
//     two-pass overlap-suppression is deliberately NOT reproduced (documented residual: overlaps
//     double-darken slightly). Faithful drawShadowVolume/drawShadowGD/forceRequest-with-clustering
//     are stubbed here (comment marks them as such) — no live caller for the volume path at
//     file-select, TModelWaterManager::drawRefracAndSpec's calls are gated by env pollution
//     (a separately-tracked open item, memory `delfino-lighting-wash`).

// gpBindShadowManager storage lives in native/boot_stubs/unresolved_stubs.cpp — do NOT
// redefine here (would be a duplicate-symbol link error). The ctor below overwrites it.

// -----------------------------------------------------------------------------
// TMBindShadowManager
// -----------------------------------------------------------------------------

TMBindShadowManager::TMBindShadowManager(const char* name)
    : JDrama::TViewObj(name)
{
	gpBindShadowManager = this;
	mRequestCount   = 0;
	mFootprintCount = 0;
}

// The vtable's load slot exists in the GC binary but the disassembly at 0x80231288 shows
// it as a pass-through to the base TViewObj::load (matching TGuide/TPauseMenu2). Preserving
// the base-call so name-lookup (JDrama::TNameRef::search) still resolves this object.
void TMBindShadowManager::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);
}

void TMBindShadowManager::reset()
{
	mRequestCount   = 0;
	mFootprintCount = 0;
}

void TMBindShadowManager::initEntry(TMBindShadowBody*)
{
	// GC-original tracked registered TMBindShadowBody instances into an internal list used
	// by the volume path. We do not use that list (drawShadow reads only the request queue).
}

void TMBindShadowManager::forceRequest(const TCircleShadowRequest& req, u32 /*flags*/)
{
	// Faithful minimum: bypasses request()'s frustum/isInArea/NaN gates (that's what "force"
	// means — a caller that has ALREADY decided the shadow must appear this frame).
	if (mRequestCount < kMaxRequests) {
		mRequests[mRequestCount++] = req;
	}
}

void TMBindShadowManager::request(const TCircleShadowRequest& req, u32 /*flags*/)
{
	// Faithful gates from 8022ecec.c (simplified where equivalent):
	//   - reject NaN/Inf x,z (the exponent-bit-twiddle in the decompile is a plain isfinite).
	//   - reject if outside the current map's XZ bounds.
	// Skipped-with-note: the distance-to-camera LOD cutoff (needs a camera-position global not
	// exposed by native/src/camera_latch.cpp; file-select has few actors so it's a non-issue —
	// documented, not silent).
	if (!std::isfinite(req.unk0.x) || !std::isfinite(req.unk0.z)) return;
	if (gpMap && !gpMap->isInArea(req.unk0.x, req.unk0.z)) return;

	if (mRequestCount < kMaxRequests) {
		mRequests[mRequestCount++] = req;
	}
}

void TMBindShadowManager::calcVtx()
{
	mFootprintCount = 0;
	if (!gpMap) return;

	for (int i = 0; i < mRequestCount; ++i) {
		const TCircleShadowRequest& r = mRequests[i];

		f32 groundY = r.unk0.y;
		if (r.unk1D != 0) {
			// Not grounded: raycast down. +50 upward start offset (SDA2[-0x24b0] in the RE, a
			// common "start slightly above the reported position" convention — see MapObjWave's
			// getHeight for the same 50.0f value). The extra `campos[+0x60]` offset from the
			// decompile is not chased (camera-frame-of-reference tweak; kept as a documented
			// approximation, not a hidden fudge).
			const TBGCheckData* result = nullptr;
			f32 y = gpMap->checkGround(r.unk0.x, r.unk0.y + 50.0f, r.unk0.z, &result);
			if (y > -30000.0f) {
				groundY = y;
			} else {
				// No ground under this actor — hide the shadow rather than pinning it at the
				// -32767 sentinel Y (that would draw the disc at infinite depth = 1 wasted decal).
				continue;
			}
		}

		if (mFootprintCount >= kMaxRequests) break;

		TFootprint& fp  = mFootprints[mFootprintCount++];
		fp.mPos.x       = r.unk0.x;
		fp.mPos.y       = groundY + 0.1f; // tiny lift to avoid z-fighting with the ground poly
		fp.mPos.z       = r.unk0.z;
		fp.mRadiusX     = r.unkC;
		fp.mRadiusZ     = r.unk10;
		// Alpha: faithful intent = per-request 8-bit alpha stored at offset +0x1D (0..255). The
		// GC original modulates by a distance-based fade + a per-frame global; we use a fixed
		// intensity (documented — refine against oracle if needed).
		fp.mAlpha       = 180;
	}

	SHADOW_LOG("[shadow] calcVtx: %d requests -> %d footprints\n",
	           mRequestCount, mFootprintCount);
}

void TMBindShadowManager::drawShadow(u32 /*flags*/, JDrama::TGraphics* /*g*/)
{
	if (mFootprintCount == 0) return;

	// Vertex format: POS(f32 xyz) + CLR0(rgba8). Same GX-only-through-the-shipping-emit-path
	// approach TMapObjWave::draw() uses (proven to reach the native capture pipeline).
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);

	// Stage 0: color = raster (per-vertex black), alpha = raster (per-vertex alpha).
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, (GXTevColorArg)0xF, (GXTevColorArg)0xF,
	                (GXTevColorArg)0xF, (GXTevColorArg)0xA); // = RASC
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, (GXTevAlphaArg)7, (GXTevAlphaArg)7,
	                (GXTevAlphaArg)7, (GXTevAlphaArg)0); // = RASA
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GXSetAlphaCompare(GX_GEQUAL, 0, GX_AOP_AND, GX_LEQUAL, 255);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE); // test on, write off (transparent decal)
	GXSetCullMode(GX_CULL_NONE);

	// Soft disc: 12-triangle fan (center + 12 rim vertices, drawn as GX_TRIANGLES).
	static const int kSteps = 12;
	static f32 sCos[kSteps + 1] = {0.0f};
	static f32 sSin[kSteps + 1] = {0.0f};
	static bool sTblReady = false;
	if (!sTblReady) {
		for (int i = 0; i <= kSteps; ++i) {
			f32 a = (f32)i * (6.28318530718f / (f32)kSteps);
			sCos[i] = std::cos(a);
			sSin[i] = std::sin(a);
		}
		sTblReady = true;
	}

	for (int i = 0; i < mFootprintCount; ++i) {
		const TFootprint& fp = mFootprints[i];
		const f32 cx = fp.mPos.x, cy = fp.mPos.y, cz = fp.mPos.z;
		const f32 rx = fp.mRadiusX, rz = fp.mRadiusZ;
		const u8  a  = fp.mAlpha;

		GXBegin(GX_TRIANGLES, GX_VTXFMT0, (u16)(kSteps * 3));
		for (int s = 0; s < kSteps; ++s) {
			// Center vertex: opaque at requested alpha, dark colour.
			GXPosition3f32(cx, cy, cz);
			GXColor4u8(0, 0, 0, a);
			// Rim vertex s: alpha 0 (soft radial falloff).
			GXPosition3f32(cx + rx * sCos[s], cy, cz + rz * sSin[s]);
			GXColor4u8(0, 0, 0, 0);
			// Rim vertex s+1.
			GXPosition3f32(cx + rx * sCos[s + 1], cy, cz + rz * sSin[s + 1]);
			GXColor4u8(0, 0, 0, 0);
		}
		GXEnd();
	}
}

void TMBindShadowManager::drawShadowGD(u32 flags, JDrama::TGraphics* g)
{
	// Faithful to the GC original's role: an alias for drawShadow on the GD (GDA-mode?) path.
	// No known live caller distinguishes them at file-select.
	drawShadow(flags, g);
}

void TMBindShadowManager::drawShadowVolume(bool /*isEnv*/, TAlphaShadowQuad* /*quads*/)
{
	// GC-only extruded-shadow-volume path (used by TModelWaterManager::drawRefracAndSpec's
	// pollution darkening, gated at file-select — not currently reached). Not ported here;
	// separately tracked (memory `delfino-lighting-wash`). This is a documented gap, not a stub
	// masquerading as a fix.
}

void TMBindShadowManager::perform(u32 /*flags*/, JDrama::TGraphics* g)
{
	// Self-contained: draw accumulated footprints, then clear both queues for the next frame.
	// The flag-bit semantics of the GC original were never resolved (see the extended comment
	// in ShadowUtil.hpp point 6) so we do NOT gate on a guessed bit — perform() drawing on every
	// invocation is harmless once request/reset accounting is self-contained (the cheap early
	// out in drawShadow handles a zero-footprint pass with a single branch).
	calcVtx();
	drawShadow(0, g);
	reset();
}

// -----------------------------------------------------------------------------
// TMBindShadowBody
// -----------------------------------------------------------------------------

TMBindShadowBody::TMBindShadowBody(THitActor* actor, J3DModel* model, float scale)
    : mActor(actor)
    , mModel(model)
    , mScale(scale)
{
}

// The remaining TMBindShadowBody methods (isUseThisJoint, isCircleJoint, isBodyJoint, calc)
// have no surviving symbols anywhere in GMSE01 — CodeWarrior inlined them into
// TMario::perform. See ShadowUtil.hpp for the full analysis. We don't define bodies here
// either (the linker never needs them; only add them if a link error says otherwise).

void TMBindShadowBody::entryDrawShadow()
{
	if (!gpBindShadowManager || !mActor) return;

	TCircleShadowRequest req;
	req.unk0     = mActor->getPosition();
	// Approximate radius: mScale * a constant. The 40.0f is a tunable eyeball (starts around
	// Mario's body radius at scale=1.0). Refine against the oracle screenshot rather than baking
	// a magic number in silently — this is the same "approximate but named" gap the .hpp comment
	// flags for TMBindShadowBody as a whole.
	const f32 kRadius = 40.0f * mScale;
	req.unkC     = kRadius;
	req.unk10    = kRadius;
	req.unk1D    = 1; // treat as not-grounded → let calcVtx raycast down to the true ground.

	gpBindShadowManager->request(req, 0);
}
