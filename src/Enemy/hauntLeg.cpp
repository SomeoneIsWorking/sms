#include <Enemy/HauntLeg.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <dolphin/os.h>

// THauntLeg — haunting-leg enemy. RE'd US GMSE01 (wide-RE sweep 2026-07-17). Spawn+walk
// core ported; the ballistic haunt-jump + custom leg-orientation render are simplified/
// deferred (several matrix/velocity APIs need verification) — the leg spawns, walks its
// graph path via the TWalkerEnemy base, and renders upright.

#define HL_TODO(fn)                                                            \
	do {                                                                       \
		static bool _once = false;                                             \
		if (!_once) { _once = true; OSReport("[STUB-CALLED] %s (hauntleg jump/render deferred)\n", fn); } \
	} while (0)

// ---- manager ----
THauntLegManager::THauntLegManager(const char* name)
    : TSmallEnemyManager(name)
{
}

TSpineEnemy* THauntLegManager::createEnemyInstance() { return new THauntLeg; }

void THauntLegManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TWalkerEnemyParams("/enemy/hauntLeg.prm");
}

void THauntLegManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "hauntleg.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_MaterialUseIndirect
		      | J3DMLF_UseUniqueMaterials | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

// ---- actor ----
THauntLeg::THauntLeg(const char* name)
    : TWalkerEnemy(name)
{
	mHauntObj = nullptr;
	mGrabbed  = 0;
	mAirReset = 1;
	mTarget   = nullptr;
	mJumpVelocity.set(0.0f, 0.0f, 0.0f);
	mSpinAngle = 0.0f;
}

THauntLeg::~THauntLeg() {}

void THauntLeg::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000025;
	mHitFlags |= 0x60000000;
}

void THauntLeg::reset()
{
	mTarget   = nullptr;
	mGrabbed  = 0;
	mAirReset = 1;
	TWalkerEnemy::reset();
}

void THauntLeg::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("hauntleg.bmd", 3);
}

static const char* hauntleg_bastable[] = {
	"/scene/hauntleg/bas/hauntleg_generate.bas",
	"/scene/hauntleg/bas/hauntleg_walk.bas",
	"/scene/hauntleg/bas/hauntleg_wait.bas",
};
const char** THauntLeg::getBasNameTable() const { return hauntleg_bastable; }

void THauntLeg::setGenerateAnm() { setBckAnm(0); }
void THauntLeg::setRunAnm() { setBckAnm(1); }
void THauntLeg::setWalkAnm() { setBckAnm(1); }
void THauntLeg::setWaitAnm() { setBckAnm(2); }

void THauntLeg::setDeadAnm()
{
	if (mTarget)
		mTarget->receiveMessage(this, 6); // HIT_MESSAGE_PUT — release grabbed
	mHolder     = nullptr;
	mHeldObject = nullptr;
}

bool THauntLeg::isCollidMove(THitActor*) { return false; }

void THauntLeg::attackToMario() { sendAttackMsgToMario(); }

// --- simplified/deferred (ballistic jump + custom leg orientation) ---
MtxPtr THauntLeg::getTakingMtx() { HL_TODO("THauntLeg::getTakingMtx"); return getModel()->getBaseTRMtx(); }
void THauntLeg::calcRootMatrix() { TSpineEnemy::calcRootMatrix(); } // custom ground-normal + mHauntObj mirror deferred

DEFINE_NERVE(TNerveHauntLegHaunt, TLiveActor) { HL_TODO("TNerveHauntLegHaunt"); return 0; }
