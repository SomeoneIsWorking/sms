#include <Enemy/FruitsBoat.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <dolphin/os.h>

// TFruitsBoat — Delfino/Ricco harbor fruit boat. RE'd US GMSE01 (wide-RE sweep 2026-07-17).
// Manager + params ported; boat wave-physics methods are loud stubs pending a focused pass.

#define BOAT_TODO(fn)                                                          \
	do {                                                                       \
		static bool _once = false;                                             \
		if (!_once) { _once = true; OSReport("[STUB-CALLED] %s (fruitboat physics unported)\n", fn); } \
	} while (0)

// ---- TFruitsBoatParams ----
TFruitsBoatParams::TFruitsBoatParams(const char* prm)
    : TSpineEnemyParams(prm)
    , PARAM_INIT(mSLMoveSpeed, 4.0f)
    , PARAM_INIT(mSLRotSpeed, 0.1f)
    , PARAM_INIT(mSLBckMoveSpeed, 0.2f)
{
	TParams::load(mPrmPath);
}

// ---- TFruitsBoatManager ----
TFruitsBoatManager::TFruitsBoatManager(int kind, const char* name)
    : TEnemyManager(name)
{
	mBoatKind = kind;
}

TSpineEnemy* TFruitsBoatManager::createEnemyInstance() { return nullptr; }

void TFruitsBoatManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TFruitsBoatParams("/enemy/fruitsBoat.prm");
	TEnemyManager::load(stream);
}

void TFruitsBoatManager::createModelData()
{
	static const u32 F = J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
	                     | (1 << J3DMLF_TevStageNumShift);
	static TModelDataLoadEntry e0[] = { { "ShipDolpic.bmd", F, 0 }, { nullptr, 0, 0 } };
	static TModelDataLoadEntry e1[] = { { "ShipDolpic2.bmd", F, 0 }, { nullptr, 0, 0 } };
	static TModelDataLoadEntry e2[] = { { "ShipDolpic3.bmd", F, 0 }, { nullptr, 0, 0 } };
	static TModelDataLoadEntry e3[] = { { "ShipDolpic4.bmd", F, 0 }, { nullptr, 0, 0 } };
	TModelDataLoadEntry* tbl[4] = { e0, e1, e2, e3 };
	createModelDataArray(tbl[mBoatKind & 3]);
}

// ---- TFruitsBoat ----
TFruitsBoat::TFruitsBoat(const char* name)
    : TSpineEnemy(name)
{
	mBckReverse   = 0;
	mShadowScaleX = 800.0f;
	mShadowScaleZ = 800.0f;
	mBckAnm       = nullptr;
	mBckFrameCtrl = nullptr;
	mTiltAxis.set(0.0f, 1.0f, 0.0f);
	mRollAngle = 0.0f;
	mRollVel   = 0.0f;
}

BOOL TFruitsBoat::receiveMessage(THitActor*, u32) { return FALSE; }

// --- physics/setup pending a focused pass (unreachable — not factory-registered) ---
void TFruitsBoat::init(TLiveManager* manager) { BOAT_TODO("TFruitsBoat::init"); TSpineEnemy::init(manager); }
void TFruitsBoat::load(JSUMemoryInputStream& stream) { BOAT_TODO("TFruitsBoat::load"); TSpineEnemy::load(stream); }
void TFruitsBoat::calcRootMatrix() { BOAT_TODO("TFruitsBoat::calcRootMatrix"); TSpineEnemy::calcRootMatrix(); }
void TFruitsBoat::moveObject() { BOAT_TODO("TFruitsBoat::moveObject"); }
void TFruitsBoat::requestShadow() { BOAT_TODO("TFruitsBoat::requestShadow"); }
void TFruitsBoat::setGroundCollision() { BOAT_TODO("TFruitsBoat::setGroundCollision"); }
Mtx* TFruitsBoat::getRootJointMtx() const { return (Mtx*)getModel()->getBaseTRMtx(); }
int TFruitsBoat::setBckTrack(const char*) { BOAT_TODO("TFruitsBoat::setBckTrack"); return -1; }

DEFINE_NERVE(TNerveFruitsBoatBckTrace, TLiveActor) { BOAT_TODO("TNerveFruitsBoatBckTrace"); return 0; }
DEFINE_NERVE(TNerveFruitsBoatGraphWander, TLiveActor) { BOAT_TODO("TNerveFruitsBoatGraphWander"); return 0; }
