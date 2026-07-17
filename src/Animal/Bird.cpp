#include <Animal/Bird.hpp>
#include <Animal/AnimalManager.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/TakeActor.hpp>
#include <Enemy/WireBinder.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <System/ParamInst.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <dolphin/os.h>

// ============================================================================
// Delfino-plaza BIRD chain — RE'd US GMSE01 (wide-RE workflow 2026-07-17,
// adversarially verified). TAnimalBird : TAnimalBase; TAnimalBirdManager :
// TAnimalManagerBase; TAnimalBirdParams : TSpineEnemyParams; + 9 nerves.
//
// STATUS: manager + params + bird ctor/loadAfter/calcRootMatrix/getBasNameTable
// are ported in FULL. init/initParams/load + per-frame behavior + the nerve
// executes are loud-once stubs pending their full specs — the factory is NOT
// yet registering birds (MarNameRefGen_Enemy.cpp still comments them out), so
// none of these stubs is reachable at runtime; they exist only so the class
// vtables link. The burn-down loop fills them next.
// ============================================================================

#define BIRD_TODO(fn)                                                          \
	do {                                                                       \
		static bool _once = false;                                             \
		if (!_once) {                                                          \
			_once = true;                                                       \
			OSReport("[STUB-CALLED] %s (unported bird behavior)\n", fn);        \
		}                                                                       \
	} while (0)

// ---------------------------------------------------------------------------
// TAnimalBirdManager  (mirror of TMewManager; no new fields)
// ---------------------------------------------------------------------------
TAnimalBirdManager::TAnimalBirdManager(const char* name)
    : TAnimalManagerBase(name)
{
}

void TAnimalBirdManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "bird_man.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TAnimalBirdManager::load(JSUMemoryInputStream& stream)
{
	// Bird manager keeps a TAnimalBirdParams as the enemy param block (unk38);
	// LP64: guest sizes the alloc 0x210 — `new` sizes the host object.
	unk38 = new TAnimalBirdParams("/Animal/bird.prm");
	TEnemyManager::load(stream);
}

void TAnimalBirdManager::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_FLYING1,
	                                              mObjNum);
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_CHUN,
	                                              mObjNum);
}

// ---------------------------------------------------------------------------
// TAnimalBirdParams  (18 TParamRT tuning members; defaults RE'd from the ctor)
// ---------------------------------------------------------------------------
TAnimalBirdParams::TAnimalBirdParams(const char* prm)
    : TSpineEnemyParams(prm)
    , PARAM_INIT(mMarchSpeed, 5.0f)
    , PARAM_INIT(mTurnSpeed, 0.1f)
    , PARAM_INIT(mReturnTimer, 1800)
    , PARAM_INIT(mSearchLength, 800.0f)
    , PARAM_INIT(mSearchHeight, 600.0f)
    , PARAM_INIT(mSearchAware, 400.0f)
    , PARAM_INIT(mSearchAngle, 90.0f)
    , PARAM_INIT(mActionTimer, 100)
    , PARAM_INIT(mWaterproofTimerMax, 45)
    , PARAM_INIT(mFloatingTimerMax, 30)
    , PARAM_INIT(mLandingGravityY, 1.0f)
    , PARAM_INIT(mLandingTorqueY, 2.0f)
    , PARAM_INIT(mWalkingTorqueY, 0.5f)
    , PARAM_INIT(mWalkingSpeed, 2.0f)
    , PARAM_INIT(mWalkTimer, 100)
    , PARAM_INIT(mLandingFric, 0.95f)
    , PARAM_INIT(mActionTimerAdd, 300)
    , PARAM_INIT(mWaterPowerY, 15.0f)
{
	TParams::load(mPrmPath);
}

// ---------------------------------------------------------------------------
// TAnimalBird  (: TAnimalBase)
// ---------------------------------------------------------------------------
TAnimalBird::TAnimalBird(const char* name)
    : TAnimalBase(0, name)
{
	mFrameTimer = nullptr;   // 0x150 (TAnimalBase field)
	mWireBinder = nullptr;   // 0x154
}

// bird animation .bas table (US .data 0x803ABD70, 9 entries)
static const char* bird_bastable[] = {
	/* 0 */ nullptr,
	/* 1 */ "/scene/bird/bas/bird_fly.bas",
	/* 2 */ "/scene/bird/bas/bird_open.bas",
	/* 3 */ nullptr,
	/* 4 */ nullptr,
	/* 5 */ "/scene/bird/bas/bird_start.bas",
	/* 6 */ "/scene/bird/bas/bird_stop.bas",
	/* 7 */ nullptr,
	/* 8 */ nullptr,
};
const char** TAnimalBird::getBasNameTable() const { return bird_bastable; }

// Overrides TAnimalBase::loadAfter — unconditionally registers two looping bird
// SE positions (no actor-type gate, unlike the seagull path).
void TAnimalBird::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_FLYING1, &mPosition);
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_CHUN, &mPosition);
}

// If held by a TTakeActor, copy the holder's taking matrix into the model base
// matrix; else fall back to the spine-enemy root build. Then raise the model
// root Y by 35.
void TAnimalBird::calcRootMatrix()
{
	if (mHolder != nullptr) {
		PSMTXCopy(mHolder->getTakingMtx(), getModel()->mBaseMtx);
	} else {
		TSpineEnemy::calcRootMatrix();
	}
	getModel()->mBaseMtx[1][3] += 35.0f;
}

// --- pending full specs (unreachable until the factory registers birds) ---
// STOPGAP: TAnimalBird::load (US 0x8000dea8, 0x154) is NOT a flock spawn — it calls
// TSpineEnemy::load, reads a 4-byte param, selects mBirdKind (species 0..3), sets flags,
// and loads species-specific model/anim (callees 0x801b5610/0x80294580/0x80218a50/0x80235df4
// + the 0x80373988 species table still to resolve). Full RE pending (the wide-RE agent for
// this one method hit the schema-retry cap). Until then, do the stream-safe minimum so
// scene-stream alignment is preserved if ever reached; do NOT call TAnimalBase::load (that
// would wrongly spawn a clone flock). Birds are not factory-registered yet, so this is dead.
void TAnimalBird::load(JSUMemoryInputStream& stream)
{
	BIRD_TODO("TAnimalBird::load (stopgap: species/model load unported)");
	TSpineEnemy::load(stream);
	s32 param;
	stream.read(&param, sizeof(param));   // consume the 4-byte param load() reads
}
BOOL TAnimalBird::receiveMessage(THitActor*, u32) { BIRD_TODO("TAnimalBird::receiveMessage"); return 0; }

// Bird override of TAnimalBase::init: register with the manager, build the MActor from the
// NAMED bird model (not from-all-BMD), seed the "wait on ground" nerve, apply bird params +
// hit tuning, init the anim-sound. US GMSE01 @0x8000e14c.
void TAnimalBird::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(manager, 1);
	mMActor       = mMActorKeeper->createMActor("bird_man.bmd", 0);

	mSpine->initWith(&TNerveAnimalBirdWaitOnGround::theNerve());
	initParams();

	// Collision attribute 0x10000032 (NOT getActorType()); radii attack 50/50, damage 70/80.
	initHitActor(0x10000032, 0, 0, 50.0f, 50.0f, 70.0f, 80.0f);
	onHitFlag(2);
	offHitFlag(1);
	mScaledBodyRadius = 35.0f;
	initAnmSound();
}

// US GMSE01 @0x8000dffc. Snapshot home transform, seed hit points from the save param,
// clear path nodes, set fly height, clear airborne, pick a random anim phase, and bind to
// a map wire if the spawn point is on one.
void TAnimalBird::initParams()
{
	mHomePos.set(mPosition.x, mPosition.y, mPosition.z);
	mHomePos.y += 90.0f;
	mHomeRot.set(mRotation.x, mRotation.y, mRotation.z);

	TSpineEnemyParams* p = getSaveParam();
	mHitPoints = p ? p->mSLHitPointMax.get() : 1;

	mCurPathNode  = nullptr;
	mNextPathNode = nullptr;
	mFlyHeight    = 1.0f;
	offLiveFlag(LIVE_FLAG_AIRBORNE);

	// Random anim phase near 1.0 (decomp idiom rand()*(1/(RAND_MAX+1))).
	mPhase = 1.0f - 0.1f * ((f32)rand() * (1.0f / (f32)(RAND_MAX + 1)) - 0.5f);

	if (TWireBinder::isOnWire(mPosition)) {
		mWireBinder = new TWireBinder;
		mWireBinder->init(mPosition);
	}
}
void TAnimalBird::bind() { BIRD_TODO("TAnimalBird::bind"); }
void TAnimalBird::moveObject() { BIRD_TODO("TAnimalBird::moveObject"); }
void TAnimalBird::doLanding(bool) { BIRD_TODO("TAnimalBird::doLanding"); }
void TAnimalBird::doFlyToCurPathNode() { BIRD_TODO("TAnimalBird::doFlyToCurPathNode"); }
bool TAnimalBird::isFindMario() const { return false; }

// --- 9 bird nerves (execute bodies pending; return 0 = stay-in-nerve) ---
DEFINE_NERVE(TNerveAnimalBirdLanding, TLiveActor) { BIRD_TODO("TNerveAnimalBirdLanding"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdWaitOnGround, TLiveActor) { BIRD_TODO("TNerveAnimalBirdWaitOnGround"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor) { BIRD_TODO("TNerveAnimalBirdGraphWander"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor) { BIRD_TODO("TNerveAnimalBirdPreLanding"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdComeback, TLiveActor) { BIRD_TODO("TNerveAnimalBirdComeback"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor) { BIRD_TODO("TNerveAnimalBirdChangeToCoin"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor) { BIRD_TODO("TNerveAnimalBirdTakeoff"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor) { BIRD_TODO("TNerveAnimalBirdWalkOnGround"); return 0; }
DEFINE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor) { BIRD_TODO("TNerveAnimalBirdActionOnGround"); return 0; }
