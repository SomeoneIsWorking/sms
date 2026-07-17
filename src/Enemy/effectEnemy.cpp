#include <Enemy/EffectEnemy.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <System/Particles.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Player/MarioAccess.hpp>
#include <dolphin/mtx.h>

// TEffectEnemy — the "moving fire effect" enemy (moveFireEffect.prm). RE'd US GMSE01
// (wide-RE sweep 2026-07-17). TEffectEnemy : TWalkerEnemy; TEffectEnemyManager :
// TSmallEnemyManager.

TEffectEnemy::TEffectEnemy(const char* name)
    : TWalkerEnemy(name)
{
	mAttackMode = 0;
}

TEffectEnemy::~TEffectEnemy() {}

void TEffectEnemy::setDeadAnm()
{
	gpMarioParticleManager->emitAndBindToPosPtr(0x8b, &mPosition, 0, nullptr);
	if (gpMSound->gateCheck(0x28c5))
		MSoundSESystem::MSoundSE::startSoundActor(0x28c5, &mPosition, 0, nullptr, 0, 4);
	onLiveFlag(LIVE_FLAG_UNK20000);
}

void TEffectEnemy::sendAttackMsgToMario()
{
	switch (mAttackMode) {
	case 0:
		SMS_SendMessageToMario(this, 0xA); // burn
		kill();
		break;
	case 1:
		SMS_SendMessageToMario(this, 0x9);
		break;
	default:
		SMS_SendMessageToMario(this, 0xE); // attack
		break;
	}
}

void TEffectEnemy::behaveToWater(THitActor* actor)
{
	if (mHitPoints <= 1)
		kill();
	else
		TSmallEnemy::behaveToWater(actor);
}

void TEffectEnemy::reset()
{
	TWalkerEnemy::reset();
}

void TEffectEnemy::perform(u32 param, JDrama::TGraphics* gfx)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	if (param & 1)
		TWalkerEnemy::moveObject();

	if ((param & 2) && !checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		u8 denom = getSaveParam() ? getSaveParam()->mSLHitPointMax.get() : 1;
		f32 ratio = (f32)((s32)mHitPoints / (s32)denom);
		JGeometry::TVec3<f32> tmp;
		PSVECScale(&mScaling, &tmp, ratio); // computed-but-unused in retail (faithful)
		gpMarioParticleManager->emitAndBindToPosPtr(0x1ed, &mPosition, 3, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x135, &mPosition, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x136, &mPosition, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x137, &mPosition, 1, this);
	}

	THitActor::perform(param, gfx);
}

void TEffectEnemy::forceKill()
{
	const TBGCheckData* g = mGroundPlane;
	bool doAreaCheck;

	if (g->isIllegalData()) {
		doAreaCheck = true;
	} else if (g->mBGType == 0x800 || g->mBGType == 0x100 || g->mBGType == 0x101
	           || g->mBGType == 0x102 || g->mBGType == 0x103 || g->mBGType == 0x104
	           || g->mBGType == 0x105 || g->mBGType == 0x4104) {
		if (checkLiveFlag(LIVE_FLAG_AIRBORNE))
			doAreaCheck = true;
		else if (checkLiveFlag(LIVE_FLAG_UNK10))
			doAreaCheck = true;
		else {
			kill();
			return;
		}
	} else {
		doAreaCheck = true;
	}

	if (doAreaCheck) {
		if (!gpMap->isInArea(mPosition.x, mPosition.z))
			kill();
	}
}

void TEffectEnemy::kill()
{
	setDeadAnm();
	onLiveFlag(LIVE_FLAG_DEAD);
	mHitFlags |= 1;
}

void TEffectEnemy::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("default.bmd", 3);
}

void TEffectEnemy::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000005;
}

// ---------------------------------------------------------------------------
// TEffectEnemyManager
// ---------------------------------------------------------------------------
TEffectEnemyManager::TEffectEnemyManager(const char* name)
    : TSmallEnemyManager(name)
{
}

TEffectEnemyManager::~TEffectEnemyManager() {}

TSpineEnemy* TEffectEnemyManager::createEnemyInstance()
{
	return new TEffectEnemy("エフェクト敵");
}

void TEffectEnemyManager::loadAfter()
{
	TEnemyManager::loadAfter(); // deliberately skips TSmallEnemyManager::loadAfter
}

void TEffectEnemyManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TWalkerEnemyParams("/enemy/moveFireEffect.prm");
}
