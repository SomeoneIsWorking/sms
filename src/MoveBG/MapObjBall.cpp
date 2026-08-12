#include <MoveBG/MapObjBall.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <Strategic/LiveActor.hpp>
#include <Strategic/TakeActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <System/MarDirector.hpp>
#include <System/FlagManager.hpp>
#include <System/Particles.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <dolphin/mtx.h>
#include <MarioUtil/PacketUtil.hpp>
#include <dolphin/gx/GXEnum.h>
#include <Map/MapData.hpp>
#include <MarioUtil/MapUtil.hpp>
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
	// Rest threshold: SDA2 r2-0x23f8, now READ OUT OF THE IMAGE rather than guessed. The word
	// at guest 0x804147a8 is 0x36800000 = 3.8147e-06 (2^-18), which is nothing like the 0.01
	// this line used to hard-code as a STOPGAP -- four orders of magnitude tighter, i.e. very
	// nearly "exactly stopped". The same constant is loaded by control()'s states 2 and 3 at
	// 0x801e27f0 for the same velocity-squared test, which is what turned it up.
	constexpr float kRestThresholdSq = 3.8147e-06f;
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
	unk194 = 0; // see the header: retail leaves this uninitialised, the port must not
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

// TResetFruit::control -- US 0x801e23b4, 396 instructions. Per-frame behaviour of the plaza
// fruit, and one of the [STUB-CALLED] stubs a Delfino run prints, i.e. really executed.
//
// The instruction count is misleading: the body is a switch on mState (u16 at +0xfc) through a
// 14-entry jump table at 0x803D2818 with only SEVEN distinct targets. Seven of the fourteen
// states jump straight to the function epilogue (0x801e29c4 -- verified to be nothing but
// register restores), so they genuinely do nothing. Out-of-range states do nothing either:
// the prologue is `lhz r0,0xfc(r3); cmplwi r0,0xd; bgt <epilogue>`, so anything above 13 is
// silently ignored rather than indexing off the end of the table. Full map:
// docs/re_notes/tresetfruit_control_state_map.md.
//
// PORTED HERE: state 6 and the seven no-op states. The other six states are NOT ported and say
// so out loud -- a silent no-op there would be a behaviour change dressed as a stub (see the
// FAIL-FAST / no-silent-stubs rule); their addresses are in the report so the next porter has
// the target without re-deriving the table.
// The tail shared by control()'s states 6 and 11. Both table entries end in the identical 21
// instructions (0x801e26e4..0x801e2764 and 0x801e2660..0x801e26e0 -- same opcodes, same operands,
// same order), which is what an inline expanded at two call sites looks like. Reconstructed as
// that inline rather than copy-pasted twice, so a later correction cannot land in only one copy.
//
// "Let go of whoever is holding this, stop dead, and go to state 12." Both ends of the holding
// relationship are cleared, because TTakeActor::ensureTakeSituation would otherwise null one of
// them on the next frame anyway and the message would have been sent to a stale holder.
static void reset_fruit_release_and_drop(TResetFruit* self)
{
	self->TMapObjBall::control();

	if (self->checkMapObjFlag(TMapObjBase::MAP_OBJ_FLAG_UNK4000000))
		return;
	if (self->mStateTimer > 0)
		return;

	if (self->mHolder != nullptr) {
		self->mHolder->receiveMessage(self, HIT_MESSAGE_UNK8);
		self->mHolder->mHeldObject = nullptr;
		self->mHolder              = nullptr;
	}

	self->mVelocity.x = 0.0f;
	self->mVelocity.y = 0.0f;
	self->mVelocity.z = 0.0f;
	self->mState      = 12;
}

// The respawn delay, read from the global at guest 0x8040c924 (SDA1 r13-0x789c). Its value in
// the image is 360 and NO instruction anywhere in .text stores to it -- checked by scanning every
// word in the seven text sections for an r13-relative access with that displacement, which found
// seven sites, all `lwz`, all inside TResetFruit's own address range (0x801e1d80..0x801e36ec).
// A global only one class reads and nothing writes is that class's constant, so it is one here.
static const int sFruitWaitTimeToAppear = 360;

void TResetFruit::control()
{
	switch (mState) {
	// ── the seven no-op states ──────────────────────────────────────────────────────────────
	// Table entries for 0, 4, 5, 7, 8, 9 and 10 all point at the epilogue. Reproduced as an
	// explicit empty case rather than folded into `default:` so that "this state does nothing"
	// stays distinguishable from "this state is not ported".
	case 0:
	case 4:
	case 5:
	case 7:
	case 8:
	case 9:
	case 10:
		break;

	// ── state 6: the fruit is being carried, and this is the drop check ─────────────────────
	// 0x801e26e4, 33 instructions, ported complete:
	//
	//   801e26e8  bl   control__11TMapObjBallFv     ; the ball physics still run
	//   801e26f0  lwz  r0,0xf8(r30) ; rlwinm. 0,5,5 ; MAP_OBJ_FLAG_UNK4000000 -> bail
	//   801e26f8  lwz  r0,0x104(r30); cmpwi/ble     ; state timer still running -> bail
	//   801e2718  lwz  r3,0x68(r30) ; cmplwi/beq    ; mHolder
	//   801e2730  lwz  r12,0xa0(r12); blrl          ; virtual on the HOLDER, args (this, 8)
	//   801e273c  stw  r0,0x6c(r3)  ; stw r0,0x68(r30)   ; clear BOTH ends of the hold
	//   801e274c  lfs  f0,-0x2428(r2) = 0.0f ; stfs 0xac/0xb0/0xb4  ; velocity killed
	//   801e2760  sth  r0,0xfc(r30) with r0 = 0xc   ; advance to state 12
	//
	// The virtual at vtable byte 0xa0 is receiveMessage. That identification was WRONG in an
	// earlier pass of this RE and is worth recording, because the error is systematic: MWCC
	// vtables carry two leading zero words and the vptr points at the OBJECT START, so every
	// dispatch offset already includes that +8. Measured rather than assumed -- across the whole
	// US .text the smallest `lwz r12,X(r12)` is X=8 (174 sites) and X=0/4 never occur, which
	// only holds if slot 0 lives at +8. With the bias applied, __vt__10TTakeActor (size 0xB4,
	// so its last slot is at 0xB0) puts getRadiusAtY at 0xb0, moveRequest 0xac,
	// ensureTakeSituation 0xa8, getTakingMtx 0xa4 and receiveMessage at 0xa0. Reading the offset
	// as unbiased shifted every one of those by two slots and made this call look like
	// ensureTakeSituation -- a no-argument method, which cannot be what a three-argument call
	// site invokes. Independently corroborated: TBGGesso does the identical thing in the same
	// held-object context (src/Enemy/bossgesso.cpp:269).
	case 6:
		reset_fruit_release_and_drop(this);
		break;

	// ── not ported ─────────────────────────────────────────────────────────────────────────
	// Each is a separate body in retail; sizes are instruction counts, not bytes.
	// ── state 1 (STATE_NORMAL): the fruit is lying in the world, taking collisions ──────────
	// 0x801e23f8, 90 instructions, ported complete. This is the state the plaza fruit is
	// ACTUALLY in -- it is the one the loud report above turned up on a real SB_STAGE=1 run,
	// which is why it was ported next rather than the largest body.
	//
	//   801e23f8  lwz 0x64 ; rlwinm 0,0,0x1e ; stw   ; mHitFlags &= ~1 (bit 0 only)
	//   801e252c  lhz r0,0x48(r30) ; cmpw r31,r0     ; loop over mColCount, SIGNED compare
	//   801e241c  lwzx r4, r3, r29                   ; mCollisions[i], byte-stepped by 4
	//   801e2410  lhz r5,0xfc(r30)  RE-READ each iteration — touchActor can change mState,
	//                                and the four skip-tests below then fire on later passes
	//   801e2490  bl touchActor__11TMapObjBallFP9THitActor   ; DIRECT call, not a vtable slot
	//   801e2500  lwz r12,0x164(r12)                 ; = getLivingTime (see the note in case 6
	//                                                  for why that offset resolves this way)
	//   801e2550  lwz r12,0x1ec(r12)                 ; = calcCurrentMtx
	//
	// The `offLiveFlag(LIVE_FLAG_UNK10)` near the end is reproduced although it cannot change
	// anything: the loop already skipped this iteration if that bit was set, and the only call
	// between the test and the clear is getLivingTime, which is const. It is retail's
	// instruction and costs nothing, so it stays rather than being "cleaned up" -- deleting it
	// would be a silent divergence for a reader who later diffs against the disassembly.
	//
	// mGroundPlane is dereferenced WITHOUT a null check, exactly as retail does. TLiveActor
	// always has a ground plane by the time control() runs; if that ever stops being true the
	// crash is the correct outcome per the FAIL-FAST rule, and a null guard here would hide it.
	case 1: {
		mHitFlags &= ~1;

		for (int i = 0; i < (int)mColCount; ++i) {
			THitActor* other = mCollisions[i];

			const u16 st = mState;
			if (st == 2 || st == 3 || st == 12 || st == 10)
				continue;

			TMapObjBall::touchActor(other);

			if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
				continue;
			if (mState != STATE_NORMAL)
				continue;
			if (checkLiveFlag(LIVE_FLAG_UNK10))
				continue;

			if (mStateTimer <= 0) {
				unkF8 |= MAP_OBJ_FLAG_DISAPPEARING;
				mStateTimer = getLivingTime();
			}
			offLiveFlag(LIVE_FLAG_UNK10);
			mState = 0xb;
		}

		// Resting on a moving platform: the plane carries the actor it belongs to, and the
		// fruit's matrix has to be rebuilt from it every frame.
		if (mGroundPlane->mActor != nullptr)
			calcCurrentMtx();
		break;
	}
	// ── states 2 and 3: carried / in flight ────────────────────────────────────────────────
	// 0x801e2768, 60 instructions, ported complete. ONE body serves both states in retail (two
	// jump-table entries, one target), so they share a case here too.
	//
	//   801e276c  bl control__14TMapObjGeneralFv   ; the GENERAL control, not the ball's
	//   801e2770  unk194 countdown, floored at 0 rather than allowed to go negative
	//   801e27ac  vtable 0xa4 on mHolder = getTakingMtx  (bias-corrected; 0xa0 is receiveMessage)
	//   801e27c0  r1+0xac is stack Mtx row 1 col 3 -> the held matrix is lifted by unk190
	//   801e27d8  lwz r4, 0x58(r3) after getModel  ; J3DModel::mNodeMatrices, copied INTO
	//   801e27f0  SDA2 r2-0x23f8 = 3.8147e-06f, against |mVelocity|^2
	//   801e2828  cror cr0eq, cr0lt, cr0eq         ; makes the test <=, not <
	//
	// The velocity copy through the stack at r1+0xc0 is a by-value TVec3 argument the compiler
	// spilled; it is not reproduced because it cannot be observed.
	case 2:
	case 3: {
		TMapObjGeneral::control();

		if (unk194 != 0)
			unk194 -= 1;

		if (mState == 6) {
			// Carried: track the holder's hand, raised by the fruit's own offset.
			Mtx held;
			PSMTXCopy(mHolder->getTakingMtx(), held);
			held[1][3] += unk190;
			PSMTXCopy(held, getModel()->mNodeMatrices[0]);
			break;
		}

		// Not carried: rebuild the matrix unless the fruit is both at rest AND not resting on
		// an actor-backed plane, in which case nothing it depends on can have moved.
		const f32 velSq = mVelocity.z * mVelocity.z
		    + (mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y);
		if (velSq <= 3.8147e-06f && mGroundPlane->mActor == nullptr)
			break;

		calcCurrentMtx();
		break;
	}
	// ── state 11: settled on the ground, with the Gelato sand special case ─────────────────
	// 0x801e2560, 97 instructions, ported complete. State 1 advances here.
	//
	// Its second half, from `bl control__11TMapObjBallFv` at 0x801e2664 to the end, is
	// INSTRUCTION-FOR-INSTRUCTION the same as state 6's — see reset_fruit_release_and_drop.
	//
	//   801e256c  gpMarDirector->mMap == 4                 ; Gelato Beach
	//   801e25bc  SDA2 r2-0x23f4 = 200.0f, + mGroundHeight (0xc8), vs mPosition.y
	//   801e25dc  ground->mActorType == 0x400000cd         ; the sand
	//   801e2624  bl SMS_GetSandRiseUpRatio(ground)        ; r3 is the GROUND ACTOR, not this
	//   801e2630  SDA2 r2-0x23f0 = 0.05f ; r2-0x2408 = 20.0f
	//
	// The ratio is stored in unk198 and compared against its own PREVIOUS value, so the fruit
	// is pushed up (mVelocity.y += 20) only while the sand is still RISING, not merely while it
	// is high. That is why unk198 is reset to 0 the moment the fruit is not standing on an
	// actor-backed plane -- otherwise a stale high-water mark would suppress the next rise.
	//
	// The 0x400000cd test appears TWICE in the disassembly with identical operands, the second
	// as the negated form. Reproduced once: it is an MWCC artefact of the branch structure and
	// the two tests cannot disagree.
	case 11: {
		mHitFlags &= ~1;

		if (gpMarDirector->mMap == 4 && checkLiveFlag(LIVE_FLAG_UNK10))
			offLiveFlag(LIVE_FLAG_UNK10);

		if (mGroundPlane->mActor != nullptr) {
			if (checkLiveFlag(LIVE_FLAG_UNK10))
				offLiveFlag(LIVE_FLAG_UNK10);

			if (mPosition.y < 200.0f + mGroundHeight) {
				const TLiveActor* ground = mGroundPlane->mActor;
				if (ground->mActorType == 0x400000cd) {
					const f32 prevRatio = unk198;
					unk198              = SMS_GetSandRiseUpRatio(ground);
					if (unk198 > 0.05f && unk198 > prevRatio)
						mVelocity.y += 20.0f;
				}
			}
		} else {
			unk198 = 0.0f;
		}

		reset_fruit_release_and_drop(this);
		break;
	}
	// ── state 12: the fruit that was just dropped vanishes ─────────────────────────────────
	// 0x801e2858, 35 instructions, ported complete. State 6 sets mState = 12, so this is the
	// step immediately after the drop and the two belong together.
	//
	//   801e2870  fmadds f0, f2, f1, f0   ; mPosition.y += mBodyRadius * 0.5f
	//                                       (0xbc = mBodyRadius; SDA2 r2-0x2404 = 0x3f000000)
	//   801e287c  0x124/0x128/0x12c -> 0x24/0x28/0x2c  ; mScaling = mInitialScaling
	//   801e2894  bl emitAndScale(0xe5, 0, &mPosition) ; PARTICLE_MS_ENM_DISAP_A_W
	//   801e28a0  bl gateCheck(0x387d) then the 6-arg start   ; the inlined body of
	//                                       MSound::startSoundActor, gpMSound at r13-0x6044
	//   801e28cc  mStateTimer = 0xf0 ; bl sleep ; mState = 13
	//
	// The scale restore is deliberate and not redundant: the fruit shrinks while it is carried,
	// and it has to be back at its authored size before the disappear particle is scaled to it.
	case 12:
		mPosition.y += mBodyRadius * 0.5f;

		mScaling.x = mInitialScaling.x;
		mScaling.y = mInitialScaling.y;
		mScaling.z = mInitialScaling.z;

		emitAndScale(PARTICLE_MS_ENM_DISAP_A_W, 0, &mPosition);
		SMSGetMSound()->startSoundActor(
		    MSD_SE_SMOKE_EFFECT, &mPosition, 0, nullptr, 0, 4);

		mStateTimer = 0xf0;
		sleep();
		mState = 13;
		break;
	// ── state 13: the wait is over, put the fruit back ─────────────────────────────────────
	// 0x801e28e4, 56 instructions, ported complete. State 12 advances here after 0xf0 frames.
	//
	//   801e2904  li 0xff ; sth 0x19c/0x19e/0x1a0   ; unk19c is a GXColorS10 -- r,g,b restored,
	//                                                 ALPHA (0x1a2) deliberately untouched
	//   801e2918  bl awake__11TMapObjBaseFv
	//   801e2920  mState = 0xb   <-- set, then overwritten with 0xa at 0x801e2990. Retail does
	//                                both; the four calls in between can read it.
	//   801e292c  vtable 0x158 = makeObjDefault ; 0x104 = makeObjDead ; 0xc0 = calcRootMatrix
	//   801e296c  getModel()->vtable 0x10 = J3DModel::calc  (update/entry/calc/viewCalc are
	//                                declared BEFORE ~J3DModel, so calc is index 2 = byte 0x10)
	//   801e2988  rlwinm r3,r3,0,0xe,0xc  clears bit 13 = 0x40000 = MAP_OBJ_FLAG_DISAPPEARING
	//   801e2994  gpMarDirector->mMap == 3 (Ricco Harbour) && unk1a4 -> makeObjDead again
	case 13:
		if (mStateTimer > 0)
			break;

		unk19c.r = 255;
		unk19c.g = 255;
		unk19c.b = 255;

		awake();
		mState = 0xb;

		makeObjDefault();
		makeObjDead();
		calcRootMatrix();
		getModel()->calc();

		mStateTimer = sFruitWaitTimeToAppear;
		unkF8 &= ~MAP_OBJ_FLAG_DISAPPEARING;
		mState = 10;

		if (gpMarDirector->mMap == 3 && unk1a4 != 0)
			makeObjDead();
		break;

	// Retail's `cmplwi r0,0xd; bgt` -- states above 13 are ignored, not an error.
	default:
		break;
	}
}
