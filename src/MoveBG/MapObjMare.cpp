#include <MoveBG/MapObjMare.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>
#include <Map/MapWireManager.hpp>
#include "sms_boot_mare_event_point.h"

// Native port of TMareEventPoint::load (@0x801d77b4). RE: scratch/decomp_next3/801d77b4.c.
// A "TMareEventPoint" is one of the invisible trigger volumes scattered around Noki Bay /
// Corona Mountain — designer-authored named locations game logic queries by name to fire
// cutscenes, mission-state transitions, or physics events. The load only calls the base
// TActor::load + a single initHitActor with a hard-coded actor-type + no attack shape + a
// 300r × 600h damage-cylinder trigger volume.
//
// SDA scan (tools/dol_sda.py 0x801d77b4):
//   SDA2[-0x267c] = 0.0    (attackRadius AND attackHeight — no attack cylinder)
//   SDA2[-0x2678] = 300.0  (damageRadius — trigger cylinder XZ radius)
//   SDA2[-0x2674] = 600.0  (damageHeight — trigger cylinder Y extent)
// Actor type constant `sb::kMareEventPointActorType = 0x40000236` — the specific type flag
// game logic uses to enumerate event points. Extracted to the pure header + tested so any
// accidental digit swap (e.g. 0x40000326) trips the unit test.
void TMareEventPoint::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	initHitActor(sb::kMareEventPointActorType, 0, 0,
	             sb::kMareEventPointAttackRadius,
	             sb::kMareEventPointAttackHeight,
	             sb::kMareEventPointDamageRadius,
	             sb::kMareEventPointDamageHeight);
}

// Native port of TMapObjGrowTree::loadAfter (@0x801d942c). RE: scratch/decomp_next5/801d942c.c.
// もやしの木 ("bean-sprout tree") — the grow-from-water trees dotted around Noki Bay. loadAfter
// extends the base loadAfter then removes the tree's static map-collision registration so
// the tree can grow/shrink dynamically at runtime without a stale collision volume left over
// from its initial (fully-grown) placement.
//
// SDA scan (tools/dol_sda.py 0x801d942c): no SDA references — pure engine-call sequence.
void TMapObjGrowTree::loadAfter()
{
	TMapObjBase::loadAfter();
	removeMapCollision();
}

// Native port of TWireBell::loadAfter (@0x801d8f1c). RE: scratch/decomp_next5/801d8f1c.c.
// ワイヤー鈴（紫）— the purple wire-bells strung across Noki Bay's wire ropes. loadAfter
// extends the base then resolves the bell's wire-pin index by asking gpMapWireManager
// which wire in the scene passes closest to the bell's placed position. That index is
// cached in mWireIndex so per-tick control() can query the wire's live pose without a
// spatial lookup each frame.
//
// SDA scan (tools/dol_sda.py 0x801d8f1c):
//   SDA1[-0x6308] = gpMapWireManager  (getWireNo(const Vec3&) → wire index)
void TWireBell::loadAfter()
{
	TMapObjBase::loadAfter();
	mWireIndex = gpMapWireManager->getWireNo(mPosition);
}
