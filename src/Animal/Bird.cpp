#include <Animal/AnimalSave.hpp>
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
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

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

	// mAnimalSave is a HARD requirement of TAnimalManagerBase, not an optional
	// extra: TAnimalBase::init (AnimalBase.cpp:157) and ::perform (:247) both
	// dereference it unconditionally, and TAnimalManagerBase's ctor leaves it
	// null. Omitting it here is what crashed the plaza —
	//   SIGSEGV fault=0x30 in TAnimalBase::perform
	//   -> mgr->mAnimalSave->mSLSharedAnmNum.get()
	// once birds were actually registered and started performing. Mirrors
	// TMewManager::load, which sets it plus the view-clip fields derived from it.
	// Same .prm as the tuning block above: TParams keys are read by name, so the
	// save block picks up mSLSharedAnmNum / mSLViewClip* from /Animal/bird.prm.
	mAnimalSave     = new TAnimalSaveIndividual("/Animal/bird.prm");
	mViewClipNear   = mAnimalSave->mSLViewClipNear.value;
	mViewClipFarPtr = &mAnimalSave->mSLViewClipFar.value;
	unk3C           = mAnimalSave->mSLViewClipRadius.value;
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

// TAnimalBird::load — US GMSE01 0x8000dea8, 0x154 bytes. Fully RE'd 2026-08-12; this replaces
// a stopgap that consumed the stream param and did nothing else.
//
// It is NOT a flock spawn (calling TAnimalBase::load here would wrongly clone a flock). It reads
// one 4-byte event id, spawns the ITEM this bird carries, derives the bird's species from that
// item's actor type, and tints the body material accordingly.
//
// The species tint table is US .data 0x80373988: four GXColorS10 (four s16 each, which is exactly
// the 8-byte stride the code indexes it by, `slwi r3, r4, 3`).
static const GXColorS10 bird_body_color[4] = {
	/* 0 */ { 0, 100, 255, 0 },
	/* 1 */ { 0, 200, 0, 0 },
	/* 2 */ { 255, 200, 0, 0 },
	/* 3 */ { 255, 0, 0, 0 },
};

void TAnimalBird::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);

	s32 eventId;
	stream.read(&eventId, sizeof(eventId));

	// The carried item. A NEGATIVE id means "no specific event" and falls back to event 100 with
	// an EMPTY name; both name strings were read out of the image rather than guessed
	// (SDA2 r2-0x7e40 = "鳥用" / bird-use, r2-0x7e38 = "").
	TMapObjBase* item;
	if (eventId >= 0) {
		item = TMapObjBaseManager::newAndRegisterObjByEventID(eventId, "鳥用");
	} else {
		item = TMapObjBaseManager::newAndRegisterObjByEventID(100, "");
	}

	// Retail dereferences this unconditionally. Ours must not, because our
	// newAndRegisterObjByEventID has a `default: return nullptr` for every event id it does not
	// implement yet — so a null here means AN UNPORTED ITEM TYPE, not a bird problem. Loud and
	// once, naming the id, so it reads as the porting gap it is.
	if (item == nullptr) {
		BIRD_TODO("TAnimalBird::load: newAndRegisterObjByEventID returned NULL "
		          "(unported item event id) - species defaults, no tint");
		mBirdKind = 1; // the `default:` species retail picks for an unrecognised actor type
		return;
	}

	// STORED AT 0x150, WHICH IS TAnimalBase::mFrameTimer'S SLOT. That reuse is retail's, not a
	// mistake here: `stw r3, 0x150(r31)`. It is safe for birds specifically — TAnimalBird's ctor
	// nulls mFrameTimer and its init() never allocates the int[2] that TAnimalBase::init does, so
	// nothing ever reads this slot as a timer for a bird. Reproduced through the same storage
	// rather than given a new field, because a separate field would silently diverge if a bird
	// ever DID run the shared AnimalNerve timer path.
	mFrameTimer = reinterpret_cast<int*>(item);

	// Species from the item's actor type. Compared against mActorType directly (`cmpw`), not via
	// isActorType(), matching the disassembly. Raw hex constants match the decomp's own style for
	// these ids (see MoveBG/MapObjHide.cpp).
	switch (item->mActorType) {
	case 0x20000010:
		mBirdKind = 0;
		// A bird whose blue coin has ALREADY been collected is born dead: mLiveFlag |= 1 is
		// LIVE_FLAG_DEAD. The coin is keyed by (current map, event id truncated to a byte).
		// SDA1[-0x6060] is gpFlagManager, which is TFlagManager::getInstance() (same identity
		// established in MoveBG/MapObjBall.cpp).
		if (TFlagManager::getInstance()->getBlueCoinFlag(gpMarDirector->mMap, (u8)eventId)) {
			mLiveFlag |= 1;
		}
		break;
	case 0x20000013: mBirdKind = 2; break;
	case 0x2000000F: mBirdKind = 3; break;
	default:         mBirdKind = 1; break;
	}

	// Tint the body material, found BY NAME through the model data's material name table —
	// "_mat_body1", read from the image at SDA2 r2-0x7eb0.
	//
	// DOCUMENTED SEAM, with a real root cause behind it. getModel() is mMActor->unk4, and our
	// TAnimalBird::init() is what creates mMActor ("bird_man.bmd") — but in this port init() has
	// not run by the time load() does, so mMActor is null here and retail's unconditional
	// getModel() segfaults. Verified from a core dump: the fault is in this function, frame #0,
	// under TMarDirector::setupObjects.
	//
	// That ORDERING is the defect, not this line. Retail reaches load() with the actor already
	// initialised because the manager creates and inits its objects first; our birds are not
	// manager-created yet. The proper fix is that creation order, which is the TAnimalBirdManager
	// arc — NOT a null check here. Until then this reports and skips, so the species selection
	// and blue-coin logic above still run and the missing piece is the tint alone.
	if (mMActor == nullptr) {
		BIRD_TODO("TAnimalBird::load: no MActor at load() time - init() has not run, so the "
		          "body tint is skipped (manager-driven init ordering unported)");
		return;
	}
	const u16 matIdx = (u16)getModel()->getModelData()->mMaterialName->getIndex("_mat_body1");
	SMS_InitPacket_OneTevColor(getModel(), matIdx, GX_TEVREG1, &bird_body_color[mBirdKind]);
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
	mPhase = 1.0f - 0.1f * ((f32)rand() * (1.0f / (f32)(RAND_MAX + 1.0f)) - 0.5f);

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
