#include <MoveBG/MapObjTown.hpp>

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
