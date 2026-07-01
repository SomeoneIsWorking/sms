#include <MoveBG/MapObjTown.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include "sms_boot_door.h"

// Native port of TDamageObj::perform (@0x801c1570). RE: scratch/decomp_next3/801c1570.c.
// A ダメージオブジェ ("damage object") is any passive world hazard that hurts everything it
// touches — spikes, sharp rocks, an active geyser column. Per-tick behavior is minimal:
// dispatch the base THitActor::perform (matrix / collision-list housekeeping), then poke
// the first entry of the collision list (if any) with HIT_MESSAGE_ATTACK. THitActor's
// receiveMessage dispatch handles the actual damage — this class only fires the message.
//
// SDA scan (tools/dol_sda.py 0x801c1570): no SDA references at all.
//
// Note on the "small-test" pattern: the port's pure logic is a single `mColCount != 0`
// guard. Instead of a vacuous predicate test, mare_event_point_test-style constant assertion
// would target the HIT_MESSAGE_ATTACK enum value — but that value already lives in
// Strategic/HitActor.hpp as a named enum, so changing it there would break dozens of call
// sites. A separate spec test here would just re-assert what the enum already documents;
// skipping it per proportionality (the RE's dispatch shape is the spec, cross-checked at
// review time via the enum name).
void TDamageObj::perform(u32 param_1, JDrama::TGraphics* graphics)
{
	THitActor::perform(param_1, graphics);
	if (mColCount != 0) {
		mCollisions[0]->receiveMessage(this, HIT_MESSAGE_ATTACK);
	}
}

// Native port of TDoor::load (@0x801c24cc). RE: scratch/decomp_door/801c24cc.c.
// Extends TMapObjBase::load; reads a single serialized u32 from the scene stream and
// interprets it as a boolean: nonzero → door is "locked" (specific gameplay gate depends on
// the individual door — key, stage-flag, or story trigger). The predicate lives in
// sms_boot_door.h so a wrong operator regression (`> 0` mishandling 0xFFFFFFFF, `== 1`
// mishandling any other nonzero value) fails a specific unit test.
//
// SDA scan (tools/dol_sda.py 0x801c24cc): no SDA references.
void TDoor::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	u32 serialized = 0;
	stream.read(&serialized, sizeof(serialized));
	if (sb::door_locked_from_serialized(serialized)) {
		mLocked = 1;
	}
}

// Native port of TManhole::loadAfter (@0x801c1bc0). RE: scratch/decomp_next5/801c1bc0.c.
// マンホール (Delfino Plaza's manholes). loadAfter freezes the manhole's default BCK
// animation: after the base loadAfter has set up the MActor + BCK bindings, the manhole's
// frame-ctrl 0 (the primary BCK) gets its rate zeroed so the cover doesn't spin on its own.
// The specific animation is triggered later by touchPlayer / animationFinished; while idle,
// mRate = 0 keeps it visually static.
//
// SDA scan (tools/dol_sda.py 0x801c1bc0):
//   SDA2[-0x2b20] = 0.0f
void TManhole::loadAfter()
{
	TMapObjBase::loadAfter();
	mMActor->getFrameCtrl(0)->setRate(0.0f);
}

// Native port of TMapObjSwitch::control (@0x801c0e18). RE: scratch/decomp_next3/801c0e18.c.
// オブジェスイッチ — the switches around Delfino that arm a timer for a group of hidden
// objects (block appearance countdown). Per-tick: after the base control, if a countdown
// is active (mTimeTilAppear > 0), play the ticking timer SE. mTimeTilAppear is decremented
// elsewhere; we only sound while it's still counting.
//
// SDA scan (tools/dol_sda.py 0x801c0e18):
//   SDA1[-0x6044] = gpMSound  (playTimer(u32) ticks the countdown SE)
// FUN_80013e90 identified as MSound::playTimer(u32) via PAL/US symbol delta (PAL playTimer
// @0x80013EEC minus 0x5c region-offset = US 0x80013e90 exactly).
void TMapObjSwitch::control()
{
	TMapObjBase::control();
	if (mTimeTilAppear > 0) {
		gpMSound->playTimer(mTimeTilAppear);
	}
}
