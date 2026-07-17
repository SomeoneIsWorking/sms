#include <MoveBG/MapObjBall.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <Strategic/LiveActor.hpp>
#include <Strategic/TakeActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <System/MarDirector.hpp>
#include <System/FlagManager.hpp>
#include <dolphin/mtx.h>
#include <MarioUtil/PacketUtil.hpp>
#include <dolphin/gx/GXEnum.h>
#include <Map/MapData.hpp>
#include <cmath>
#include "sms_boot_reset_fruit.h"
#include "sms_boot_coverfruit.h"

#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
static bool sFruitDbg()
{
	static int v = -1;
	if (v < 0) { const char* e = getenv("SB_FRUIT_DBG"); v = (e && *e && *e != '0') ? 1 : 0; }
	return v;
}
#define FR_LOG(...) do { if (sFruitDbg()) std::fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define FR_LOG(...) do {} while (0)
#endif

// Native port of TResetFruit::perform (@0x801e21d0). RE at
// scratch/decomp_resetfruit/801e21d0.c. TResetFruit is the "無限フルーツ" (infinite fruit) — the
// respawnable coconut / fruit found on beaches (including the file-select stage's beach).
// Its ::perform is a THIN dispatch: it handles a Pinna Park (stage 7) Yoshi-touch special case,
// else falls through to the parent's perform.
//
// File-select is stage 15, so the Pinna Park branch is never entered here — the file-select
// coconut's render path is entirely the parent's TMapObjGeneral::perform (already implemented
// natively in MapObjGeneral.cpp:587). Making this vtable slot NON-empty means the coconut model
// gets its perform-passes driven (calc, entry, draw) instead of silently no-op'ing.
//
// DOCUMENTED GAP (kept honest per no-bandaid rule): the stage-7 branch — Yoshi-touch state
// machine + vtable slot calls (+0x158, +0x104, +0xC0, +0x10) + flag/state manipulation at unk3C
// and mState — is NOT reproduced yet. Never fires at file-select; needed only when running
// Pinna Park (stage 7). Named + address of every unresolved vtable slot preserved above so a
// future session can complete this quickly. The stage-7 "should this branch enter" predicate
// IS pure and unit-tested in sms_boot_reset_fruit.h → sb::reset_fruit_should_enter_pinna_park_branch.
void TResetFruit::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	// Stage-7 predicate (extracted into pure sb helper so the test validates the real branch
	// condition, not a fork). We currently DO NOT execute the branch body even when the predicate
	// fires — that's the documented gap. At file-select this is never taken anyway.
	const int stage    = gpMarDirector ? gpMarDirector->mMap : 0;
	const int state    = (int)mState;
	const float vx     = mVelocity.x, vy = mVelocity.y, vz = mVelocity.z;
	const float vel_sq = vx * vx + vy * vy + vz * vz;
	// Rest threshold: SDA2 [-0x23F8] in the RE. Not resolved from the DOL yet; use a small
	// positive value that matches the "at rest" intent. STOPGAP: hard-coded 0.01 (1cm/frame^2 in
	// game units) — refined once the DOL constant is looked up. Doesn't matter at file-select
	// (stage != 7 short-circuits the predicate immediately).
	constexpr float kRestThresholdSq = 0.01f;
	if (sb::reset_fruit_should_enter_pinna_park_branch(stage, state, vel_sq, kRestThresholdSq)) {
		FR_LOG("[fruit] Pinna Park branch predicate fires - body unimplemented, delegating anyway\n");
		// See DOCUMENTED GAP above. Delegate rather than silently continue as if the branch ran.
	}

	// General path: delegate to the already-implemented parent. This is what makes the coconut
	// model actually render (parent runs the per-frame perform-list dispatch that drives
	// calc/entry/draw through the J3D model).
	TMapObjGeneral::perform(param_1, param_2);
}

// Native port of TCoverFruit::loadAfter (@0x801e1748). RE via scratch/disasm.py + PAL/US
// symbol delta (getBool at PAL 0x8028C83C + 0x81E8 US delta = 0x80294A24 confirmed against
// setBlueCoinFlag/getShineFlag pairs in that range).
// フタのフルーツ ("lid fruit"): after the base loadAfter, check the "was this fruit already
// collected in this save" boolean; if set, kill the object at load time so the fruit never
// appears. TCoverFruit does NOT override makeObjDead, so the vtable slot 0x104 dispatched by
// the RE resolves to TMapObjBase::makeObjDead (zeros velocity, ORs 0x10 into mLiveFlag).
//
// SDA scan (tools/dol_sda.py 0x801e1748):
//   SDA1[-0x6060] = gpFlagManager  (== TFlagManager::getInstance())
//
// Vtable slot 0x104 resolution (added to memory for future ports): TMapObjBase's vtable is at
// US VA 0x803c2ab8 (ctor stw pattern @0x801af6c8). Offset 0x104 → 0x801b0738, whose body
// zeros mVelocity + sets mLiveFlag bit 0x10 = makeObjDead (the virtual declared between
// makeObjAppeared and changeObjSRT in MapObjBase.hpp — matches vtable ordering).
void TCoverFruit::loadAfter()
{
	TMapObjBase::loadAfter();
	if (TFlagManager::getInstance()->getBool(sb::kCoverFruitCollectedFlag)) {
		makeObjDead();
	}
}

// Native port of TCoverFruit::calcRootMatrix (@0x801e1840, US GMSE01, size 0x144). RE'd from
// disasm (workflow 2026-07-17, verified vs the binary); a simplified twin of the ported
// TMapObjGeneral::calcRootMatrix (MapObjGeneral.cpp). フタのフルーツ ("lid fruit"):
//   - held (mHolder != null): copy the holder's taking matrix into the model's base TR matrix
//     and snap mPosition to that matrix's translation column (src[0..2][3]).
//   - else: build the base TR matrix from SRT (position with mYOffset removed in Y, rotation via
//     the f32 MsMtxSetXYZRPH overload = deg->s16 by 65536/360, matching the disasm SDA2 const).
//   - always: push mScaling into the model base scale.
// getModel() is re-fetched per use to mirror the three `bl getModel` sites (not hoisted).
void TCoverFruit::calcRootMatrix()
{
	if (mHolder) {
		MtxPtr src = mHolder->getTakingMtx();
		MTXCopy(src, getModel()->getBaseTRMtx());
		mPosition.set(src[0][3], src[1][3], src[2][3]);
	} else {
		MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x,
		               mPosition.y - mYOffset, mPosition.z, mRotation.x,
		               mRotation.y, mRotation.z);
	}
	getModel()->setBaseScale(mScaling);
}

// Field-order guard: the ball-physics unkNNN fields must stay in ascending order (their
// /* 0xNNN */ header comments are GUEST offsets; on the LP64 host they sit past the larger
// base — accessed by NAME, so host offset is irrelevant, but keep them contiguous/ordered).
#include <cstddef>
static_assert(offsetof(TMapObjBall, unk190) - offsetof(TMapObjBall, unk148) == 19 * 4 - 4,
              "TMapObjBall physics fields not contiguous");

// Native port of TMapObjBall::initMapObj (@0x801e3ac8, US GMSE01, size 0x4F4). RE'd from the
// DOL disasm (wide-RE workflow 2026-07-17; every instruction + SDA2 f32 constant resolved). The
// keystone for all TMapObjBall subclasses (fruit/coconut/watermelon/durian): chains to the
// already-ported TMapObjGeneral::initMapObj (which builds the J3D model), seeds mInitialScaling
// from mScaling, then a switch on mActorType loads that fruit type's physics coefficients
// (bounce/friction/scale-rates at this+0x148..0x190) and body radius. A tail of independent
// per-id `if`s (faithful to the disasm's separate tests, not else-if) overrides body radius /
// unk190 for a few ids.
void TMapObjBall::initMapObj()
{
	TMapObjGeneral::initMapObj();

	mInitialScaling.x = mScaling.x;
	mInitialScaling.y = mScaling.y;
	mInitialScaling.z = mScaling.z;

	switch (mActorType) {
	case 0x400000d0: // does not set unk148
		unk14C = 4.0f;
		unk150 = 0.0f;
		unk154 = 0.0f;
		unk158 = 0.15f;
		unk15C = 0.0f;
		unk160 = 0.9f;
		unk164 = 0.06f;
		unk168 = 1.5f;
		unk16C = 0.5f;
		unk170 = 0.5f;
		unk174 = 0.2f;
		unk178 = 2.5f;
		unk17C = 0.001f;
		unk180 = 0.3f;
		unk184 = 1.5f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C      = mBodyRadius / 3.0f;
		break;

	case 0x40000064:
		unk148 = 0.6f;
		unk14C = 2.0f;
		unk150 = 0.02f;
		unk154 = 0.0f;
		unk158 = 0.055f;
		unk15C = 0.02f;
		unk160 = 0.83f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 0.5f;
		unk168 = 0.02f;
		unk16C = 0.5f;
		unk17C = 1.2f;
		unk180 = 0.8f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C      = mBodyRadius / 3.0f;
		break;

	case 0x40000393:
		unk148 = 0.6f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 15.0f;
		unk158 = 0.5f;
		unk15C = 1.3f;
		unk160 = 1.0f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C      = 50.0f;
		break;

	case 0x40000390:
	case 0x40000391:
	case 0x40000392:
		unk148 = 0.4f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 0.0f;
		unk158 = 1.2f;
		unk15C = 0.8f;
		unk160 = 0.5f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C      = 50.0f;
		break;

	case 0x40000394:
		unk148 = 0.2f;
		unk14C = 0.0f;
		unk150 = 0.0f;
		unk154 = 0.0f;
		unk158 = 0.0f;
		unk15C = 0.0f;
		unk160 = 0.0f;
		unk170 = 0.0f;
		unk174 = 0.0f;
		unk178 = 0.0f;
		unk164 = 0.0f;
		unk168 = 0.0f;
		unk16C = 0.0f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C      = 50.0f;
		break;

	case 0x40000395:
		unk148 = 0.4f;
		unk14C = 0.2f;
		unk150 = 1.3f;
		unk154 = 0.0f;
		unk158 = 1.2f;
		unk15C = 0.8f;
		unk160 = 0.5f;
		unk170 = 0.9f;
		unk174 = 0.13f;
		unk178 = 20.0f;
		unk164 = 2.0f;
		unk168 = 0.02f;
		unk16C = 0.3f;
		unk17C = 0.05f;
		unk180 = 0.5f;
		unk184 = 1.0f;
		unk188 = 1.5f;
		mBodyRadius = 50.0f * mScaling.y;
		unk18C      = 50.0f;
		break;

	default:
		break;
	}

	// Tail: independent per-id overrides (separate `if`s in the disasm, not else-if).
	if (mActorType == 0x40000393) {
		mBodyRadius = 45.0f * mScaling.y;
		unk190      = mBodyRadius;
	}
	if (mActorType == 0x40000390) {
		mBodyRadius = 40.0f * mScaling.y;
		unk190      = 20.0f;
	}
	if (mActorType == 0x40000391) {
		mBodyRadius = 40.0f * mScaling.y;
		unk190      = 20.0f;
	}
	if (mActorType == 0x40000392) {
		unk190 = 10.0f;
	}
}

// Native port of TMapObjBall::makeObjDefault (@0x801e42bc, US GMSE01, size 0x58). RE'd +
// workflow-verified (2026-07-17). Runs the base default-state setup (which recomputes mPosition
// from mInitialPosition + mYOffset and calls getModel()->calc()), then re-stamps the model's
// root node matrix translation from mPosition, lifting the ball center by mBodyRadius so it
// rests on the ground.
void TMapObjBall::makeObjDefault()
{
	TMapObjBase::makeObjDefault();

	MtxPtr anmMtx = getModel()->getAnmMtx(0);
	anmMtx[0][3] = mPosition.x;
	anmMtx[1][3] = mPosition.y + mBodyRadius;
	anmMtx[2][3] = mPosition.z;
}


// Native port of TMapObjBall::calcCurrentMtx (@0x801e43f0, US GMSE01, size 0x430). RE'd +
// workflow-verified (2026-07-17). Builds the ball's per-frame world matrix so it rolls in its
// travel direction: snap-to-rest on flat ground, else roll (axis = horizontal vec perpendicular
// to velocity via getVerticalVecToTargetXZ; angle = 2*speed/bodyRadius; Rodrigues rotation
// concatenated onto the model base orientation), position centered one body-radius above ground,
// with two per-actor-type pivot tweaks (same 50/10 constants as makeObjAppeared). All callees
// ported (getVerticalVecToTargetXZ, makeMtxRotByAxis, PSMTX*, JGeometry::TUtil sqrt/inv_sqrt).
void TMapObjBall::calcCurrentMtx()
{
	Mtx currentMtx;
	PSMTXIdentity(currentMtx);

	f32 restThreshold = mMapObjData->mPhysical->unk4->unkC;

	// Snap tiny horizontal drift to a dead stop when resting on perfectly flat ground.
	if (fabsf(mVelocity.x) < restThreshold && fabsf(mVelocity.z) < restThreshold
	    && mGroundPlane->getNormal().y == 1.0f) {
		mVelocity.x = 0.0f;
		mVelocity.z = 0.0f;
	}

	// Roll the ball while it is still moving horizontally.
	if (fabsf(mVelocity.x) > restThreshold || fabsf(mVelocity.z) > restThreshold) {
		JGeometry::TVec3<f32> vertical;
		getVerticalVecToTargetXZ(mPosition.x + mVelocity.x, mPosition.z + mVelocity.z,
		                         &vertical);

		f32 speed = JGeometry::TUtil<f32>::sqrt(
		    mVelocity.x * mVelocity.x + mVelocity.z * mVelocity.z);

		JGeometry::TVec3<f32> axis;
		f32 magSq = vertical.x * vertical.x + vertical.y * vertical.y
		          + vertical.z * vertical.z;
		if (magSq <= JGeometry::TUtil<f32>::epsilon()) {
			axis.set(0.0f, 0.0f, 0.0f);
		} else {
			f32 inv = JGeometry::TUtil<f32>::inv_sqrt(magSq);
			axis.set(vertical.x * inv, vertical.y * inv, vertical.z * inv);
		}

		f32 angle = 2.0f * (speed / mBodyRadius);
		makeMtxRotByAxis(axis, angle, currentMtx);
	}

	// Concatenate the roll onto the model's base orientation (translation stripped).
	Mtx modelMtx;
	PSMTXCopy(getModel()->getAnmMtx(0), modelMtx);
	modelMtx[0][3] = 0.0f;
	modelMtx[1][3] = 0.0f;
	modelMtx[2][3] = 0.0f;
	PSMTXConcat(currentMtx, modelMtx, currentMtx);

	// Centre the ball one body-radius above its ground position.
	currentMtx[0][3] = mPosition.x;
	currentMtx[1][3] = mPosition.y + mBodyRadius;
	currentMtx[2][3] = mPosition.z;

	if (isActorType(0x40000394) && currentMtx[1][1] > 0.0f)
		currentMtx[1][3] -= 50.0f * currentMtx[1][1];
	if (isActorType(0x40000392))
		currentMtx[1][3] -= 10.0f * (1.0f - currentMtx[1][1]);

	PSMTXCopy(currentMtx, getModel()->getAnmMtx(0));
}

// Native port of TResetFruit ctor (@0x801e1bf4): base TMapObjBall ctor, then init the fruit's
// TEV tint (unk19c = opaque white) + unk198 (0.0, SDA2[-0x2428]) + unk1a4 (0). Ported here;
// stub removed from movebg_stubs.cpp.
TResetFruit::TResetFruit(const char* name) : TMapObjBall(name)
{
	unk198 = 0.0f;
	unk1a4 = 0;
	unk19c.r = 0xff;
	unk19c.g = 0xff;
	unk19c.b = 0xff;
	unk19c.a = 0xff;
}

// Native port of TResetFruit::initMapObj (@0x801e1c5c). Chains to the (now-ported)
// TMapObjBall::initMapObj (builds model + physics), then binds the ctor-set white TEV color
// into TEV register 0 of the model's material packet. Raw arg (GXTevRegID)1 == GX_TEVREG0.
void TResetFruit::initMapObj()
{
	TMapObjBall::initMapObj();
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &unk19c);
}

// Native port of TResetFruit::makeObjAppeared (@0x801e2084, US GMSE01, size 0x130).
// Faithful RE from the DOL disasm (tools/re/disasm_range.py). Called when a picked/eaten
// 無限フルーツ (infinite fruit) respawns: reset the object to its default appeared state, then
// stamp the fruit model's root node matrix (getAnmMtx(0)) translation directly from mPosition
// (Y lifted by the body radius so the ball rests on the ground), with two per-fruit-type Y
// nudges keyed on mActorType.
//
// Field map (all verified against the class headers):
//   unkF8  MAP_OBJ_FLAG_UNK4000000 (0x4000000)  -> checkMapObjFlag
//   mBodyRadius (TLiveActor 0xBC), mPosition (TPlacement 0x10), mActorType (THitActor 0x4C),
//   unkE8 (TLiveActor 0xE8, s8), mState (TMapObjBase 0xFC, u16 <- 0xB).
// SDA2 float literals resolved via tools/dol_sda.py: [-0x2428]=0.0, [-0x23E8]=50.0,
//   [-0x23EC]=1.0, [-0x23CC]=10.0.
//
// The two mActorType keys are specific fruit variants from the MapObjManager event-id table
// (0x40000394 = case 1000, 0x40000392 = case 1003); the retail code compares the raw keys
// (isActorType), which is what we reproduce.
//
// UNPORTED CALLEES (both still boot_stubs — reported in unportedDeps): the conditional
// makeObjDefault() [vtable slot 0x158 = TMapObjBall::makeObjDefault @0x801e42bc] and the
// unconditional calcCurrentMtx() [vtable slot 0x1EC = TMapObjBall::calcCurrentMtx @0x801e43f0].
// TMapObjBase::makeObjAppeared (direct bl @0x801b0430, US-unlabeled gap fn) IS ported
// (MapObjBase.cpp:344). The AnmMtx translation stamp below runs regardless, so the fruit is
// positioned even while calcCurrentMtx remains stubbed.
void TResetFruit::makeObjAppeared()
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		makeObjDefault();

	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3] = mPosition.x;
	mtx[1][3] = mPosition.y + mBodyRadius;
	mtx[2][3] = mPosition.z;

	if (isActorType(0x40000394)) {
		if (mtx[1][1] > 0.0f)
			mtx[1][3] -= 50.0f * mtx[1][1];
	}
	if (isActorType(0x40000392))
		mtx[1][3] -= 10.0f * (1.0f - mtx[1][1]);

	unkE8 = 0;
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		mState = 0xb;
}
