#include <Enemy/Seal.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjHitCheck.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/MapCollisionManager.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <System/Particles.hpp>
#include <Player/ModelWaterManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>

// ============================================================================
// TSeal — the Delfino-plaza beach seal (OrangeSeal). RE'd US GMSE01 (wide-RE
// workflow 2026-07-17, adversarially verified). TSeal : TSpineEnemy;
// TSealManager : TEnemyManager; + 3 nerves (Die/Wait/Sleep). Seal sleeps until
// Mario nears (Wait), can be sprayed to death (Die).
// ============================================================================

// ---------------------------------------------------------------------------
// TSealManager : TEnemyManager  (no new fields)
// ---------------------------------------------------------------------------
TSealManager::TSealManager(const char* name)
    : TEnemyManager(name)
{
}

void TSealManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "gene_orange_model1.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_MaterialUseIndirect
		      | J3DMLF_UseUniqueMaterials | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TSealManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);   // base does the real work; seal adds no config
}

// ---------------------------------------------------------------------------
// TSeal : TSpineEnemy
// ---------------------------------------------------------------------------
TSeal::TSeal(const char* name)
    : TSpineEnemy(name)
{
	mSprayCount = 0;
	onLiveFlag(LIVE_FLAG_UNK10);
}

TSeal::~TSeal() {}

void TSeal::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(manager, 2);
	mMActor       = mMActorKeeper->createMActor("gene_orange_model1.bmd", 0);
	mMActor->offMakeDL();

	f32 r = 100.0f * mScaling.x;
	initHitActor(0x10000024, 1, 0x81000000, r, r, r, r);
	offHitFlag(1);

	// register into the 敵グループ enemy-group hit-check list (same idiom as the animals)
	JDrama::TNameRefGen::search<TIdxGroupObj>("敵グループ")->add(this);

	// face-pitch offset +270deg, normalized to [0,360)
	mRotation.x += 270.0f;
	while (mRotation.x >= 360.0f)
		mRotation.x -= 360.0f;
	while (mRotation.x < 0.0f)
		mRotation.x += 360.0f;

	mMapCollisionManager = new TMapCollisionManager(1, "/scene/seal", this);
	mMapCollisionManager->init("gene_orange_col1.col", 2, nullptr);
	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);

	mHitPoints = getSaveParam() ? getSaveParam()->mSLHitPointMax.get() : 1;

	mSpine->initWith(&TNerveSealSleep::theNerve());   // starts asleep
}

void TSeal::calcRootMatrix()
{
	J3DModel* model = getModel();
	MsMtxSetXYZRPH(model->getBaseTRMtx(),
	               mPosition.x, mPosition.y, mPosition.z,
	               mRotation.x, mRotation.y, mRotation.z);
	model->setBaseScale(mScaling);
}

void TSeal::perform(u32 param_1, JDrama::TGraphics* graphics)
{
	// bit0: attack any Mario we're touching
	if (!checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN) && (param_1 & 1)) {
		for (int i = 0; i < mColCount; ++i) {
			THitActor* col = mCollisions[i];
			if (col->isActorType(0x80000001))   // Mario
				col->receiveMessage(this, 0xE); // HIT_MESSAGE_ATTACK
		}
	}

	TSpineEnemy::perform(param_1, graphics);

	if (param_1 & 1) {
		updateSquareToMario();
		mSprayCount = 0;
	}

	// bit1: positional idle bark when Mario is near
	if ((param_1 & 2) && !checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)
	    && mDistToMarioSquared < 2250000.0f) {
		if (gpMSound->gateCheck(MSD_SE_EN_ORANGESEAL_WAIT))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_EN_ORANGESEAL_WAIT, &mPosition, 0, nullptr, 0, 4);
	}
}

BOOL TSeal::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->mActorType != 0x01000001 || message != 0xF /* SPRAYED_BY_WATER */)
		return FALSE;

	gpMarioParticleManager->emit(PARTICLE_MS_ENM_WATHIT, &sender->mPosition, 0, nullptr);
	gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &sender->mPosition, 0, 0.0f, 0, 0, 4);

	if (gpModelWaterManager->unk5D5F == 0)
		return TRUE;   // acknowledged, no kill

	gpMSound->startSoundSet(MSD_SE_ERASE_SCRAWL, &sender->mPosition, 0, 0.0f, 0, 0, 4);

	if (mSpine->getLatestNerve() != &TNerveSealDie::theNerve()) {
		mMapCollisionManager->getUnk8()->remove();
		mSpine->pushNerve(&TNerveSealDie::theNerve());
	}
	mSprayCount += 1;
	return TRUE;
}

// ---------------------------------------------------------------------------
// nerves
// ---------------------------------------------------------------------------
DEFINE_NERVE(TNerveSealSleep, TLiveActor)
{
	TSeal* seal    = static_cast<TSeal*>(spine->getBody());
	MActor* mActor = seal->getMActor();

	if (spine->getTime() == 0) {
		if (!mActor->checkCurBckFromIndex(1))
			mActor->setBckFromIndex(-1);
	}

	if (seal->getDistToMarioSquared() < 2250000.0f) {   // Mario near -> wake
		spine->pushAfterCurrent(&TNerveSealWait::theNerve());
		return TRUE;
	} else {
		if (mActor->curAnmEndsNext(0, nullptr) && mActor->checkCurBckFromIndex(1))
			mActor->setBckFromIndex(-1);
	}
	return FALSE;
}

DEFINE_NERVE(TNerveSealWait, TLiveActor)
{
	TSeal* seal    = static_cast<TSeal*>(spine->getBody());
	MActor* mActor = seal->getMActor();

	if (spine->getTime() == 0)
		mActor->setBckFromIndex(3);

	if (mActor->curAnmEndsNext(0, nullptr)) {
		if (mActor->checkCurBckFromIndex(3))
			mActor->setBckFromIndex(2);
	}

	if (seal->getDistToMarioSquared() > 2250000.0f) {   // Mario far -> sleep
		if (mActor->curAnmEndsNext(0, nullptr)) {
			mActor->setBckFromIndex(1);
			spine->pushAfterCurrent(&TNerveSealSleep::theNerve());
			return TRUE;
		}
	}
	return FALSE;
}

DEFINE_NERVE(TNerveSealDie, TLiveActor)
{
	TSeal* seal = static_cast<TSeal*>(spine->getBody());

	if (spine->getTime() == 0) {
		seal->getMActor()->setBckFromIndex(0);   // death anim
		MtxPtr mtx = seal->getMActor()->getModel()->mBaseMtx;
		if (JPABaseEmitter* e = gpMarioParticleManager->emitAndBindToMtxPtr(0xD1, mtx, 0, seal)) {
			e->unk154.set(seal->mScaling);
			e->unk174.set(seal->mScaling);
		}
		if (JPABaseEmitter* e = gpMarioParticleManager->emitAndBindToMtxPtr(0xD2, mtx, 0, seal)) {
			e->unk154.set(seal->mScaling);
			e->unk174.set(seal->mScaling);
		}
	}

	if (gpMSound->gateCheck(0x6010))
		MSoundSESystem::MSoundSE::startSoundActor(0x6010, &seal->mPosition, 0, nullptr, 0, 4);

	if (seal->getMActor()->curAnmEndsNext(0, nullptr)) {
		seal->mHitFlags |= 1;
		seal->kill();
		spine->pushAfterCurrent(&TNerveSealSleep::theNerve());
		return TRUE;
	}
	return FALSE;
}
