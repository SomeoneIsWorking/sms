#include <MarioUtil/ShadowUtil.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/ReinitGX.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/LightUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/HitActor.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <dolphin/os.h>
#include <cmath>
#include <cstdlib>

// Pure-logic unit shared with the unit test (spec-derived expected values live there).
// See sms_boot_shadow_gate.h for the semantics comments. The port BELOW calls into these
// helpers rather than duplicating the logic — that's what makes the test validate the
// SHIPPING function, not a fork. Header is pure C++ (<cmath>, <cstdint> only), portable.
#include "sms_boot_shadow_gate.h"

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
	// Retail ctor @0x802313e4: allocates the request/footprint/group/box/vtx pools,
	// seeds the shadow color (stage-variant: retail keys off a byte @0x803e970e —
	// an unidentified static; ==7 -> (0x2d,0x28,0x3c,0x5a), ==6 -> (9,9,0x1c,0x74).
	// Neither stage is reachable yet; port those variants when they land).
	gpBindShadowManager = this;
	mRequestCount = 0;
	mGroupCount   = 0;
	mVtxCount     = 0;
	mType2Count   = 0;
	mQuads        = new TAlphaShadowQuad[kMaxRequests];
	mBoxes        = new TAlphaShadowBlendQuad[kMaxRequests];
	mShadowDir.set(0.0f, 1.0f, 0.0f);
	mShadowColor  = { 0x1e, 0x32, 0x73, 0xb4 };
}

// Retail load @0x80231288 (scratch/decomp_shadow/80231288.c): base load, then the four
// shadow VOLUME models from the globally-mounted /common archive via
// JKRFileLoader::getGlbResource + J3DModelLoaderDataBase::load(data, 0x10210000).
// Guest wraps each in a 0x1c SDLModelData holder; host stores the J3DModelData directly.
void TMBindShadowManager::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);

	static const char* const kModelPaths[4] = {
		"/common/shadowCircle.bmd",    // [0] circle volume, near LOD
		"/common/shadowCircleLow.bmd", // [1] circle volume, far LOD
		"/common/shadowCube.bmd",      // [2] body-prism fallback
		"/common/ShipShadow.bmd",      // [3] type-3 (ship) volume
	};
	for (int i = 0; i < 4; ++i) {
		void* res = JKRFileLoader::getGlbResource(kModelPaths[i]);
		if (res == nullptr) {
			// FAIL FAST: a missing model means every draw through it silently
			// vanishes — exactly the class of defect the no-silent-stubs rule bans.
			OSPanic(__FILE__, __LINE__,
			        "TMBindShadowManager::load: missing shadow model '%s'", kModelPaths[i]);
		}
		mModels[i] = J3DModelLoaderDataBase::load(res, 0x10210000);
#ifdef SMS_NATIVE_PLATFORM
		// On this port the loader does NOT bake the per-shape VCD/VAT display
		// lists (mGDCommands stays zeroed = GX NOPs; J3D packet draws go through
		// the native capture hook instead and never need them). The shadow path
		// draws these shapes RAW (SMS_SettingDrawShape's GXCallDisplayList +
		// J3DShapeDraw geometry DLs), so the descriptor DL must exist or the CP
		// decodes the geometry with whatever descriptor the caller left bound
		// (found as a 33 MB staging overflow: indexed verts sized as direct F32).
		for (u16 s = 0; s < mModels[i]->getShapeNum(); ++s)
			mModels[i]->getShapeNodePointer(s)->makeVcdVatCmd();
		if (sShadowDbg()) {
			J3DShape* sh = mModels[i]->getShapeNodePointer(0);
			std::fprintf(stderr,
			    "[shadow] model %d shapes=%u bounds min=(%.1f,%.1f,%.1f) max=(%.1f,%.1f,%.1f)\n",
			    i, mModels[i]->getShapeNum(), sh->unk10.x, sh->unk10.y, sh->unk10.z,
			    sh->unk1C.x, sh->unk1C.y, sh->unk1C.z);
		}
#endif
	}

	mDrawDone     = 1;
	mRequestCount = 0;
	mGroupCount   = 0;
	mFlag65       = 0;
	mVtxCount     = 0;
	mType2Count   = 0;
}

void TMBindShadowManager::reset()
{
	mRequestCount = 0;
	mGroupCount   = 0;
	mVtxCount     = 0;
	mType2Count   = 0;
}

void TMBindShadowManager::initEntry(TMBindShadowBody*)
{
	// GC-original tracked registered TMBindShadowBody instances into an internal list used
	// by the volume path. We do not use that list (drawShadow reads only the request queue).
}

// Squared distance from the request to Mario — retail computes it in both request paths
// (guest reads gpMarioOriginal(+0x124); host uses the same position via SMS_GetMarioPos)
// and stores it in unk18, which drawShadow later compares against 2e7 for LOD selection.
static f32 sbShadowDistSqToMario(const TCircleShadowRequest& req)
{
	JGeometry::TVec3<f32> mp = SMS_GetMarioPos();
	const f32 dx = req.unk0.x - mp.x;
	const f32 dy = req.unk0.y - mp.y;
	const f32 dz = req.unk0.z - mp.z;
	return dz * dz + dx * dx + dy * dy;
}

void TMBindShadowManager::forceRequest(const TCircleShadowRequest& req, u32 flags)
{
	// Retail @0x8022ebbc: no gates (a caller that already decided the shadow appears),
	// but it DOES fill unk18 (dist^2 to Mario) and unk20 (flags) like request().
	if (mRequestCount < kMaxRequests) {
		TCircleShadowRequest& dst = mRequests[mRequestCount];
		dst       = req;
		dst.unk20 = flags;
		dst.unk18 = sbShadowDistSqToMario(req);
		++mRequestCount;
	}
}

void TMBindShadowManager::request(const TCircleShadowRequest& req, u32 flags)
{
	// Gates ported from 8022ecec.c live in the pure sb::shadow_gate_accept helper (unit-tested
	// against hand-derived expected values in native/platform/tests/shadow_gate_test.cpp — the
	// port SHIPS through this call so the test validates the real function, not a fork).
	sb::ShadowReq pr{ req.unk0.x, req.unk0.y, req.unk0.z, req.unkC, req.unk10, req.unk1D };
	const bool in_area = !gpMap || gpMap->isInArea(req.unk0.x, req.unk0.z);
	if (!sb::shadow_gate_accept(pr, in_area)) return;
	if (mRequestCount < kMaxRequests) {
		const f32 distSq = sbShadowDistSqToMario(req);
		if (req.unk1C == 2) {
			// Retail: type-2 requests go to the side channel (+0x70, cap 1) and do
			// NOT enter the footprint pipeline. mFar keys off dist^2 > 2e8.
			if (mType2Count == 0) {
				mType2[0].mPos = req.unk0;
				mType2[0].mFar = (distSq > 2.0e8f) ? 1 : 0;
				mType2[0].mOn  = 1;
				++mType2Count;
			}
			return;
		}
		TCircleShadowRequest& dst = mRequests[mRequestCount];
		dst       = req;
		dst.unk20 = flags;
		dst.unk18 = distSq;
		++mRequestCount;
	}
}

// conectCubeDiffer @0x80230fac: merge `add` into cumulative box `dst` when both carry the
// SAME nonzero key (bit 0x40000000 clear on both), their Y levels are within 50 and their
// XZ AABBs overlap. On merge, `dst` expands to cover `add`.
static bool sbConectCubeDiffer(TMBindShadowManager::TAlphaShadowBlendQuad* dst,
                               TMBindShadowManager::TAlphaShadowBlendQuad* add)
{
	if (dst == nullptr || add == nullptr) return false;
	const u32 k1 = dst->mKey, k2 = add->mKey;
	if (k1 != k2 || k1 == 0 || k2 == 0) return false;
	if ((k1 & 0x40000000) != 0 || (k2 & 0x40000000) != 0) return false;
	if (50.0f < std::fabs(dst->mY - add->mY)) return false;
	if (dst->mMinX <= add->mMaxX && add->mMinX <= dst->mMaxX &&
	    dst->mMinZ <= add->mMaxZ && add->mMinZ <= dst->mMaxZ) {
		if (dst->mMaxX <= add->mMaxX) dst->mMaxX = add->mMaxX;
		if (add->mMinX <= dst->mMinX) dst->mMinX = add->mMinX;
		if (dst->mMaxZ <= add->mMaxZ) dst->mMaxZ = add->mMaxZ;
		if (add->mMinZ <= dst->mMinZ) dst->mMinZ = add->mMinZ;
		if (add->mY <= dst->mY)       dst->mY    = add->mY;
		if (dst->mDy <= add->mDy)     dst->mDy   = add->mDy;
		return true;
	}
	return false;
}

// conectCubeSame @0x80230e68: keyless variant used for the pairwise group merge; margin
// comes from the f32 global @r13-0x60fc (BSS -> 0.0 — retail never writes it on the boot
// path; kept as a named constant so a future writer is visible).
static const f32 kConectSameMargin = 0.0f;
static bool sbConectCubeSame(TMBindShadowManager::TAlphaShadowBlendQuad* dst,
                             TMBindShadowManager::TAlphaShadowBlendQuad* add)
{
	if (dst == nullptr || add == nullptr) return false;
	const f32 m = kConectSameMargin;
	if (50.0f < std::fabs(dst->mY - add->mY)) return false;
	if (dst->mMinX <= add->mMaxX - m && add->mMinX + m <= dst->mMaxX &&
	    dst->mMinZ <= add->mMaxZ - m && add->mMinZ + m <= dst->mMaxZ) {
		if (dst->mMaxX <= add->mMaxX - m) dst->mMaxX = add->mMaxX;
		if (add->mMinX + m <= dst->mMinX) dst->mMinX = add->mMinX;
		if (dst->mMaxZ <= add->mMaxZ - m) dst->mMaxZ = add->mMaxZ;
		if (add->mMinZ + m <= dst->mMinZ) dst->mMinZ = add->mMinZ;
		if (add->mY <= dst->mY)           dst->mY    = add->mY;
		if (dst->mDy <= add->mDy)         dst->mDy   = add->mDy;
		return true;
	}
	return false;
}

// Retail calcVtx @0x8022e0cc (scratch/decomp_shadow/8022e0cc.c + calcvtx dossier disasm).
// Phase 1 projects every request to a ground footprint (matrix + corner quad + optional
// 5-corner prism); phase 2 clusters footprints into draw groups by box overlap.
// Corner direction tables = DAT_8039db90/dba0.
static const f32 kCornerDirX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
static const f32 kCornerDirZ[4] = { -1.0f, -1.0f, 1.0f, 1.0f };

void TMBindShadowManager::calcVtx()
{
	SHADOW_LOG("[shadow] calcVtx enter: requests=%d\n", mRequestCount);
	// PORT SEAM: retail scene setup guarantees the map collision data exists
	// before the first perform; this port's load interleaving does not (the
	// first flag-4 perform can precede the collision load — timing-dependent,
	// found as a checkGround SIGSEGV on the first title frame). Guest code has
	// no such guard; skipping a frame here only delays the first footprints.
	if (gpMap == nullptr || gpMap->mCollisionData == nullptr
	    || gpMap->mCollisionData->unk14 == nullptr
	    || gpMap->mCollisionData->unk18 == nullptr)
		return;
	mVtxCount = 0;

	for (int i = 0; i < mRequestCount; ++i) {
		TCircleShadowRequest& req = mRequests[i];
		TAlphaShadowQuad&     fp  = mQuads[i];

		const f32 origX = req.unk0.x, origY = req.unk0.y, origZ = req.unk0.z;

		if (req.unk1C == 1) {
			// Body (type-1): slide the centre along the projected light direction by
			// half the 200-unit body height (r13-0x7708 = 200.0). Transcribed literally.
			const f32 topY = origY + 200.0f;
			const f32 sx   = mShadowDir.x;
			const f32 sz   = mShadowDir.z;
			req.unk0.x = 0.5f * (-(sx * (topY - origY) - origX) + -(sx * (origY - origY) - origX));
			req.unk0.y = 0.5f * (origY + origY);
			req.unk0.z = 0.5f * (-(sz * (topY - origY) - origZ) + -(sz * (origY - origY) - origZ));
		}

		const f32 projX = req.unk0.x;
		const f32 projY = req.unk0.y;
		const f32 projZ = req.unk0.z;

		f32 groundY = projY;
		if (req.unk1D != 0) {
			const TBGCheckData* hit = nullptr;
			groundY = gpMap->checkGround(projX, projY + mProbeOffset, projZ, &hit);
			if (hit != nullptr) {
				const s16 t = (s16)hit->mBGType;
				const bool water = (t == 0x100) || (t == 0x101) ||
				                   ((u16)(t - 0x102) < 4) || (t == 0x4104);
				if (water)
					groundY = gpMap->checkGroundIgnoreWaterSurface(projX, projY + mProbeOffset,
					                                               projZ, &hit);
			}
		}
		if (req.unk1C != 1)
			req.unk0.y = groundY;

		// Effective TRS scales (constants: 0.08 grow factor, r13-0x7704 = 0.02 for
		// type-3, 0.2 fixed type-3 sy, rot 90deg for the pitched cylinder).
		f32 sxScale = (0.08f * req.unkC) * 1.0f;
		f32 syScale = (0.08f * req.unk10) * 1.0f;
		f32 rotXDeg = 90.0f;
		if (req.unk1C == 3) {
			syScale   = 0.2f;
			sxScale   = (0.08f * req.unk10) * 0.02f;
			req.unk10 = 1.0f;
			req.unkC  = 1.0f;
			rotXDeg   = 0.0f;
		}
		// 9th TRS arg (sz): syScale, additionally x1.5 (SDA2[-0x1640]) for requests
		// flagged exactly 0x80000001 (from the calcvtx dossier disasm @0x8022e79c).
		f32 szScale = syScale;
		if (req.unk20 == 0x80000001)
			szScale *= 1.5f;

		req.unkC  *= 0.8f;
		req.unk10 *= 0.8f;

		// Volume half-height: max radius, clamped to 200, x1.1.
		{
			f32 s = req.unkC;
			if (s < req.unk10) s = req.unk10;
			if (200.0f < s) s = 200.0f;
			fp.mSize = s * 1.1f;
		}
		fp.mVtx  = nullptr;
		fp.mReq  = &req;
		fp.mNext = nullptr;

		f32 mtxX = projX, mtxY = projY, mtxZ = projZ;
		f32 mtxSx = sxScale, mtxSy = syScale, mtxSz = szScale;
		f32 mtxRotX = rotXDeg;

		if (req.unk1C == 1 && mVtxCount < (kMaxVtx - 1) &&
		    std::fabs(groundY - req.unk0.y) < 1.0f) {
			// Close-to-ground body shadow: emit the 5-corner prism footprint in
			// coords local to the ORIGINAL position, and build the matrix there
			// with unit scale (the prism is already world-sized).
			mtxX = origX; mtxY = origY; mtxZ = origZ;
			mtxRotX = 0.0f;
			TAlphaShadowVtx& q = mVtxPool[mVtxCount];
			const f32 dx = projX - origX;
			const f32 dz = projZ - origZ;
			if (projX <= origX && projZ <= origZ) {
				q.p[0].set(req.unkC,      0.0f, -req.unk10);
				q.p[1].set(dx + req.unkC, 0.0f, dz - req.unk10);
				q.p[2].set(dx - req.unkC, 0.0f, dz - req.unk10);
				q.p[3].set(dx - req.unkC, 0.0f, dz + req.unk10);
				q.p[4].set(-req.unkC,     0.0f, req.unk10);
			} else {
				q.p[0].set(1.0f,      0.0f, 1.0f);
				q.p[1].set(1.0f + dx, 0.0f, dz - 1.0f);
				q.p[2].set(dx - 1.0f, 0.0f, dz - 1.0f);
				q.p[3].set(dx - 1.0f, 0.0f, 1.0f + dz);
				q.p[4].set(-1.0f,     0.0f, 1.0f);
			}
			// Retail stores the POOL BASE, not &pool[count] (disasm @0x8022e780:
			// `lwz r0,0x28(r31); stw r0,0x64(r26)`). Only Mario emits type-1 in
			// practice so count<=1 and base==slot; transcribed faithfully.
			fp.mVtx = &mVtxPool[0];
			++mVtxCount;
			mtxSx = 1.0f;
			mtxSy = 1.0f;
			mtxSz = 1.0f;
		}

		// Retail bakes view*TRS here (PSMTXConcat(@0x804045dc == j3dSys.mViewMtx,
		// m, m)) — valid on guest because the frame's LAST 3D pass leaves the main
		// view in j3dSys, so at next frame's calc phase it still holds it. This
		// port's pipeline doesn't guarantee that (found as garbage view-space
		// translations -> volumes hovering at the camera). HOST ADAPTATION with
		// identical output: store the TRS only; drawShadow concats the graphics
		// view (same source as its own retail view load) at draw time.
		MsMtxSetTRS(fp.mMtx, mtxX, mtxY, mtxZ, mtxRotX, req.unk14, 0.0f,
		            mtxSx, mtxSy, mtxSz);

		for (int k = 0; k < 4; ++k) {
			fp.mCorner[k].x = req.unkC * kCornerDirX[k] + req.unk0.x;
			fp.mCorner[k].y = groundY;
			fp.mCorner[k].z = req.unk10 * kCornerDirZ[k] + req.unk0.z;
		}

		if (mRequestCount >= kMaxRequests)
			return; // retail mid-loop guard (0x1ff check)
	}

	// ---- Phase 2: cluster footprints into draw groups (skipped in retail's debug
	// fullscreen mode, which the port does not carry). ----
	if (mRequestCount == 0)
		return;

	for (int g = 0; g < mGroupCount; ++g) {
		TShadowGroup& grp = mGroups[g];
		grp.mFpHead = nullptr;
		grp.mFpTail = nullptr;
		grp.mBoxHead = nullptr;
		grp.mBoxTail = nullptr;
		grp.mMask = 0x20000000;
	}
	mGroupCount = 0;

	for (int i = 0; i < mRequestCount; ++i) {
		TAlphaShadowQuad&      fp  = mQuads[i];
		TAlphaShadowBlendQuad& box = mBoxes[i];
		fp.mNext  = nullptr;
		box.mNext = nullptr;
		box.mMinX = fp.mCorner[0].x;
		box.mMinZ = fp.mCorner[0].z;
		box.mMaxX = fp.mCorner[2].x;
		box.mMaxZ = fp.mCorner[2].z;
		box.mY    = fp.mCorner[0].y;
		box.mDy   = 20.0f + fp.mSize; // r13-0x7700 = 20.0
		box.mKey  = 0;

		bool joined = false;
		for (int g = 0; g < mGroupCount; ++g) {
			TShadowGroup& grp = mGroups[g];
			if (sbConectCubeDiffer(grp.mBoxHead, &box)) {
				joined = true;
				grp.mFpTail->mNext  = &fp;
				grp.mFpTail         = &fp;
				grp.mBoxTail->mNext = &box;
				grp.mBoxTail        = &box;
				break;
			}
		}
		if (!joined) {
			if (mGroupCount < kMaxGroups) {
				TShadowGroup& grp = mGroups[mGroupCount];
				grp.mFpHead = grp.mFpTail = &fp;
				grp.mBoxHead = grp.mBoxTail = &box;
				grp.mMask = 0x20000000;
				if ((fp.mReq->unk20 & 0x40000000) != 0)
					grp.mMask = 0x40000000;
				++mGroupCount;
			} else {
				mGroupCount = kMaxGroups;
			}
		}
	}

	// Pairwise group merge (conectCubeSame on the cumulative head boxes).
	for (int a = 0; a < mGroupCount; ++a) {
		TShadowGroup& ga = mGroups[a];
		for (int b = 0; b < mGroupCount; ++b) {
			if (a == b || ga.mFpHead == nullptr) continue;
			TShadowGroup& gb = mGroups[b];
			if (gb.mFpHead == nullptr) continue;
			if (!sbConectCubeSame(ga.mBoxHead, gb.mBoxHead)) continue;
			ga.mFpTail->mNext  = gb.mFpHead;
			ga.mFpTail         = gb.mFpTail;
			ga.mBoxTail->mNext = gb.mBoxHead;
			ga.mBoxTail        = gb.mBoxTail;
			if ((ga.mFpHead->mReq->unk20 & 0x40000000) != 0 ||
			    (gb.mFpHead->mReq->unk20 & 0x40000000) != 0)
				ga.mMask = 0x40000000;
			gb.mFpHead = nullptr;
			gb.mBoxHead = nullptr;
		}
	}

	SHADOW_LOG("[shadow] calcVtx: %d requests -> %d groups (vtx %d)\n",
	           mRequestCount, mGroupCount, mVtxCount);
}

// Retail drawShadow @0x8022f014 — the EFB destination-alpha stencil. Per cluster group:
//   1. alpha stamp: color-update OFF, DstAlpha(1,0), Z ALWAYS — SMS_DrawCube over the
//      group's cumulative box zeroes the dst alpha in the affected screen rect.
//   2. volume mark: Z LEQUAL (no update), cull BACK, blend (ONE, ZERO) with alpha-update
//      on — the volume's front faces that pass the Z test write the shadow color's alpha
//      into dst alpha (the "stencil" mark lands only where the volume meets geometry).
//   3. darken: blend (DSTALPHA, INVDSTALPHA), Z GEQUAL, cull FRONT, color-update ON —
//      the volume's back faces darken exactly the marked pixels.
//   4. type-3 (ship) footprints additionally draw their model color-off / Z ALWAYS.
//   5. the stamp cube again (restores dst alpha), plus an optional debug blend pass.
void TMBindShadowManager::drawShadow(u32 flags, JDrama::TGraphics* g)
{
	// Retail's r13-0x60f8 debug byte selects a fullscreen-quad visualization variant.
	// BSS zero-init, no writer on the boot path; the port keeps only the real branch.

#ifdef SMS_NATIVE_PLATFORM
	// TEMP DIAGNOSTIC (remove after the staging-overflow bisect): SB_SHADOW_BISECT=1
	// returns before the GX setup, =2 returns after setup / before the group loop.
	static int sBisect = -1;
	if (sBisect < 0) { const char* e = getenv("SB_SHADOW_BISECT"); sBisect = e ? atoi(e) : 0; }
	if (sBisect == 1) return;
	SHADOW_LOG("[shadow] drawShadow flags=%08x groups=%d requests=%d\n",
	           (unsigned)flags, mGroupCount, mRequestCount);
#endif

	ReInitializeGX();
	GXSetZCompLoc(GX_TRUE);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_ALPHA0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE, GX_AF_NONE);
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetAlphaUpdate(GX_TRUE);
	{
		GXColor c = mShadowColor;
#ifdef SMS_NATIVE_PLATFORM
		// SB_SHADOW_VIZ=1: paint the shadow passes bright red to separate
		// "my passes write these pixels" from indirect state leakage.
		static int sViz = -1;
		if (sViz < 0) { const char* e = getenv("SB_SHADOW_VIZ"); sViz = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (sViz) c = { 0xff, 0x00, 0x00, 0xff };
#endif
		GXSetChanMatColor(GX_COLOR0A0, c);
	}
	GXSetCurrentMtx(GX_PNMTX0);
	MtxPtr view = g->getUnkB4();
	GXLoadNrmMtxImm(view, GX_PNMTX0);

#ifdef SMS_NATIVE_PLATFORM
	if (sBisect == 2) return; // TEMP DIAGNOSTIC
#endif
	for (int gi = 0; gi < mGroupCount; ++gi) {
		TShadowGroup& grp = mGroups[gi];
		if (grp.mFpHead == nullptr || grp.mBoxHead == nullptr) continue;
		if ((flags & grp.mMask) == 0) continue;

		// Pass 1 — alpha stamp over the cumulative box.
		GXSetCullMode(GX_CULL_NONE);
		GXLoadPosMtxImm(view, GX_PNMTX0);
		GXSetColorUpdate(GX_FALSE);
		GXSetDstAlpha(GX_TRUE, 0);
		GXSetZMode(GX_TRUE, GX_ALWAYS, GX_FALSE);
		GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
		TAlphaShadowBlendQuad* box = grp.mBoxHead;
		JGeometry::TVec3<f32> cubeMin(box->mMinX, box->mY - box->mDy, box->mMinZ);
		JGeometry::TVec3<f32> cubeMax(box->mMaxX, box->mY + box->mDy, box->mMaxZ);
#ifdef SMS_NATIVE_PLATFORM
		{
			static int sN = 0;
			if (sN < 12 && sShadowDbg()) { ++sN;
				const TCircleShadowRequest* rq = grp.mFpHead->mReq;
				SHADOW_LOG("[shadow] grp%d cube=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f) req pos=(%.0f,%.0f,%.0f) r=(%.1f,%.1f) t=%d sz=%.1f mtxT=(%.0f,%.0f,%.0f)\n",
				           gi, cubeMin.x, cubeMin.y, cubeMin.z, cubeMax.x, cubeMax.y, cubeMax.z,
				           rq->unk0.x, rq->unk0.y, rq->unk0.z, rq->unkC, rq->unk10, rq->unk1C,
				           grp.mFpHead->mSize,
				           grp.mFpHead->mMtx[0][3], grp.mFpHead->mMtx[1][3], grp.mFpHead->mMtx[2][3]);
			}
		}
#endif
		SMS_DrawCube(cubeMin, cubeMax);

		// LOD by the head footprint's distance-to-Mario (2e7 = SDA2[-0x1668]).
		const bool far_ = (2.0e7f <= grp.mFpHead->mReq->unk18);
		SMS_SettingDrawShape(far_ ? mModels[1] : mModels[0], 0);
		const bool useNear = !far_;

		// Pass 2 — volume mark into dst alpha.
		GXSetDstAlpha(GX_FALSE, 0);
		GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
		GXSetCullMode(GX_CULL_BACK);
		GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
		Mtx fpMv;
		for (TAlphaShadowQuad* fp = grp.mFpHead; fp != nullptr; fp = fp->mNext) {
			PSMTXConcat(view, fp->mMtx, fpMv);
			GXLoadPosMtxImm(fpMv, GX_PNMTX0);
			drawShadowVolume(useNear, fp);
		}

		// Pass 3 — darken through the dst-alpha mask.
		GXSetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_INVDSTALPHA, GX_LO_NOOP);
		GXSetZMode(GX_TRUE, GX_GEQUAL, GX_FALSE);
		GXSetCullMode(GX_CULL_FRONT);
		GXSetDstAlpha(GX_TRUE, 0);
		GXSetColorUpdate(GX_TRUE);
		for (TAlphaShadowQuad* fp = grp.mFpHead; fp != nullptr; fp = fp->mNext) {
			PSMTXConcat(view, fp->mMtx, fpMv);
			GXLoadPosMtxImm(fpMv, GX_PNMTX0);
			drawShadowVolume(useNear, fp);
		}

		// Pass 4 — type-3 (ship) extras.
		GXSetCullMode(GX_CULL_BACK);
		GXSetColorUpdate(GX_FALSE);
		GXSetDstAlpha(GX_TRUE, 0);
		GXSetZMode(GX_TRUE, GX_ALWAYS, GX_FALSE);
		for (TAlphaShadowQuad* fp = grp.mFpHead; fp != nullptr; fp = fp->mNext) {
			if (fp->mReq->unk1C == 3) {
				PSMTXConcat(view, fp->mMtx, fpMv);
				GXLoadPosMtxImm(fpMv, GX_PNMTX0);
				SMS_SettingDrawShape(mModels[3], 0);
				SMS_DrawShape(mModels[3], 0);
			}
		}

		// Pass 5 — re-stamp the cube (restore dst alpha in the rect).
		GXClearVtxDesc();
		GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GXLoadPosMtxImm(view, GX_PNMTX0);
		SMS_DrawCube(cubeMin, cubeMax);

		if (mDebugCubeFlag != 0) {
			GXSetColorUpdate(GX_TRUE);
			GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
			GXSetZMode(GX_TRUE, GX_ALWAYS, GX_FALSE);
			SMS_DrawCube(cubeMin, cubeMax);
		}
	}

	GXSetZCompLoc(GX_FALSE);
	GXSetColorUpdate(GX_FALSE);
	GXSetAlphaUpdate(GX_TRUE);
	GXSetDstAlpha(GX_TRUE, 0);
#ifdef SMS_NATIVE_PLATFORM
	// HOST SEAM: retail exits with color-update OFF and relies on the next draw
	// buffer's ReInitializeGX (gd-reinit-gx.cpp:211) to turn it back on before
	// anything else draws. This port's captured-J3D buffers don't interleave
	// that call, so the stuck-off bit made every subsequent buffer invisible
	// (the A/B/C blocks vanished). Restoring it here produces the same
	// observable output as retail's re-init.
	GXSetColorUpdate(GX_TRUE);
#endif
}

void TMBindShadowManager::drawShadowGD(u32 flags, JDrama::TGraphics* g)
{
	// Retail @0x8022fa40 records the SAME pass sequence into GD display lists behind
	// a zero-init debug toggle (r13-0x60f7) retail never sets on the boot path; the
	// port routes it to the immediate variant.
	drawShadow(flags, g);
}

// Retail drawShadowVolume @0x802305dc. eps = SDA2[-0x1624] = 50.0.
// Prism index tables @0x8039db48: top tris (2,1,0)(3,2,0)(4,3,0), bottom (0,1,2)(0,2,3)(0,3,4).
void TMBindShadowManager::drawShadowVolume(bool useNear, TAlphaShadowQuad* fp)
{
	const f32 kEps = 50.0f;
	const u8 type = fp->mReq->unk1C;
	if (type == 1) {
		if (fp->mVtx == nullptr) {
			SMS_SettingDrawShape(mModels[2], 0);
			SMS_DrawShape(mModels[2], 0);
		} else {
			const JGeometry::TVec3<f32>* p = fp->mVtx->p;
			GXClearVtxDesc();
			GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
			GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
			// Top + bottom caps: 6 triangles.
			static const int kTop[9] = { 2, 1, 0, 3, 2, 0, 4, 3, 0 };
			static const int kBot[9] = { 0, 1, 2, 0, 2, 3, 0, 3, 4 };
			GXBegin(GX_TRIANGLES, GX_VTXFMT0, 18);
			for (int i = 0; i < 9; ++i)
				GXPosition3f32(p[kTop[i]].x, p[kTop[i]].y + kEps, p[kTop[i]].z);
			for (int i = 0; i < 9; ++i)
				GXPosition3f32(p[kBot[i]].x, p[kBot[i]].y - kEps, p[kBot[i]].z);
			GXEnd();
			// Side walls: 5 edges x 4 triangles (both windings), 60 verts.
			GXBegin(GX_TRIANGLES, GX_VTXFMT0, 60);
			static const int kEdge[5][2] = { {0,1}, {1,2}, {2,3}, {3,4}, {4,0} };
			for (int e = 0; e < 5; ++e) {
				const JGeometry::TVec3<f32>& a = p[kEdge[e][0]];
				const JGeometry::TVec3<f32>& b = p[kEdge[e][1]];
				GXPosition3f32(a.x, a.y + kEps, a.z);
				GXPosition3f32(b.x, b.y + kEps, b.z);
				GXPosition3f32(b.x, b.y - kEps, b.z);
				GXPosition3f32(b.x, b.y - kEps, b.z);
				GXPosition3f32(a.x, a.y - kEps, a.z);
				GXPosition3f32(a.x, a.y + kEps, a.z);
				GXPosition3f32(b.x, b.y + kEps, b.z);
				GXPosition3f32(a.x, a.y + kEps, a.z);
				GXPosition3f32(b.x, b.y - kEps, b.z);
				GXPosition3f32(b.x, b.y - kEps, b.z);
				GXPosition3f32(a.x, a.y + kEps, a.z);
				GXPosition3f32(a.x, a.y - kEps, a.z);
			}
			GXEnd();
		}
	} else if (type == 3) {
		SMS_SettingDrawShape(mModels[3], 0);
		SMS_DrawShape(mModels[3], 0);
	} else {
		// Circle: draw the LOD model primed by drawShadow (or by the tail below after
		// a prism/ship footprint) and return — retail re-primes only after the types
		// that bound OTHER geometry.
		if (useNear) {
			SMS_DrawShape(mModels[0], 0);
		} else {
			SMS_DrawShape(mModels[1], 0);
		}
		return;
	}
	if (useNear) {
		SMS_SettingDrawShape(mModels[0], 0);
	} else {
		SMS_SettingDrawShape(mModels[1], 0);
	}
}

// Retail perform @0x80231108: flag 4 = calc phase (refresh the shadow direction from the
// light manager, then calcVtx); flag 8 = draw phase; flag 0x20000000 = end-of-frame reset.
void TMBindShadowManager::perform(u32 flags, JDrama::TGraphics* g)
{
	if ((flags & 4) != 0) {
		mDrawDone = 0;
		if (gpLightManager != nullptr) {
			// Retail normalizes the light-pos global @r13-0x6110 unconditionally
			// (guest scene setup guarantees it's a real sun position). This port
			// maps it to the manager's mEffectPos, which is ZERO until the light
			// manager's own perform runs — VECNormalize(0) is NaN and a NaN
			// shadow direction walked checkGround's grid out of bounds (SIGSEGV
			// on the first title frame). Keep the ctor default (straight down)
			// until a real position exists.
			const JGeometry::TVec3<f32>* lp = gpLightManager->getLightPos();
			if (lp != nullptr
			    && (lp->x != 0.0f || lp->y != 0.0f || lp->z != 0.0f))
				VECNormalize((const Vec*)lp, (Vec*)&mShadowDir);
		}
		calcVtx();
	}
	if ((flags & 8) != 0) {
		drawShadow(flags, g);
		if ((flags & 0x20000000) != 0) {
			mDrawDone     = 1;
			mRequestCount = 0;
			mGroupCount   = 0;
			mFlag65       = 0;
			mVtxCount     = 0;
			mType2Count   = 0;
		}
	}
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

#ifdef SMS_NATIVE_PLATFORM
	// One-shot log per session: proves whether the display-Mario's TMBindShadowBody is one
	// of the 1 footprint/frame observed at settled file-select, or whether Mario's shadow
	// path is NOT walked here (see the handoff's independent follow-up).
	if (sShadowDbg()) {
		static int sLogged = 0;
		if (sLogged < 5) { ++sLogged;
			auto p = mActor->getPosition();
			std::fprintf(stderr, "[shadow] entryDrawShadow actor=%p pos=(%.0f,%.0f,%.0f) scale=%.2f\n",
			             (void*)mActor, p.x, p.y, p.z, mScale);
		}
	}
#endif

	const auto& p = mActor->getPosition();
	sb::ShadowReq built = sb::shadow_body_make_request(p.x, p.y, p.z, mScale);
	TCircleShadowRequest req;
	req.unk0.x   = built.x;
	req.unk0.y   = built.y;
	req.unk0.z   = built.z;
	req.unkC     = built.radX;
	req.unk10    = built.radZ;
	req.unk1D    = built.unk1D;
	gpBindShadowManager->request(req, 0);
}
