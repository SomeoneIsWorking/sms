#include <MoveBG/MapObjBall.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <Strategic/LiveActor.hpp>
#include <Strategic/TakeActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <System/MarDirector.hpp>
#include <System/FlagManager.hpp>
#include <dolphin/mtx.h>
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
