#include <Enemy/Generator.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/Graph.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Strategic/Strategy.hpp>
#include <dolphin/mtx.h>

// Native port of TOneShotGenerator::load (@0x8008f710). RE: scratch/decomp_next3/8008f710.c.
// Reads two null-terminated names from the scene-load stream. Storage is at +0x70 (first
// read) and +0x68 (second read); the RE writes to the higher offset first, so preserve the
// exact call order to match any state-observing code that might depend on partial-load
// snapshots.
//
// SDA scan (tools/dol_sda.py 0x8008f710): no SDA references at all — pure stream I/O over
// engine primitives, so nothing to look up.
void TOneShotGenerator::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	mGraphName   = stream.readString();
	mManagerName = stream.readString();
}

void TOneShotGenerator::loadAfter()
{
	if (mCollisions == nullptr) {
		mManager = (TEnemyManager*)gpConductor->getManagerByName(mManagerName);
		if (mGraph == nullptr)
			mGraph = gpConductor->getGraphByName(mGraphName);

		initHitActor(0x2000001, 1, 0x80000000, 80.0f, 120.0f, 80.0f, 120.0f);
		offHitFlag(HIT_FLAG_NO_COLLISION);

		static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
		    ->getChildren()
		    .push_back(this);
		gpConductor->registerOtherObj(this);
	}
}

BOOL TOneShotGenerator::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->isActorType(0x1000001)) {
		if (mCount != 0) {
			TSpineEnemy* enemy = mManager->getFarOutEnemy();
			if (enemy != nullptr) {
				enemy->getTracer()->setGraph(mGraph);

				JGeometry::TVec3<f32> rot(0.0f, 0.0f, 0.0f);
				JGeometry::TVec3<f32> vel(0.0f, 4.0f, 0.0f);

				Mtx m;
				MsMtxSetRotRPH(m, mRotation.x, mRotation.y, mRotation.z);
				MTXMultVec(m, &vel, &vel);

				enemy->resetSRTV(mPosition, rot, enemy->mScaling, vel);
			}
			mCount = 0;
		}
		return TRUE;
	}
	return FALSE;
}
