#include <MoveBG/MapObjOption.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <GC2D/CardLoad.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

#ifdef SMS_NATIVE_PLATFORM
// stdio/stdlib LAST: an SMS header above shadows the stdio names (EOF-macro shim).
#include <stdio.h>
#include <stdlib.h>
#endif

static void dummy(Vec* v)
{
	*v = (Vec) { 0.0f, 0.0f, 0.0f };
	*v = (Vec) { 1.0f, 1.0f, 1.0f };
}

void TFileLoadBlock::makeBlockNoCard() { }

void TFileLoadBlock::makeBlockNormal()
{
	startAnim(0);
	mState = STATE_NORMAL;
}

void TFileLoadBlock::makeBlockRock()
{
	startAnim(1);
	mState = STATE_ROCKING;
}

static int sRumbleTime = 8;

void TFileLoadBlock::pushed()
{
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_SEL_DBG"))
		fprintf(stderr, "[fileblock] pushed() block=%d -> setSelected\n", mBlockIndex);
#endif
	startBck("fileloadblock");
	gpCardLoad->setSelected(mBlockIndex);
	SMSRumbleMgr->start(0x15, sRumbleTime, (float*)nullptr);
	gpMarioParticleManager->emit(0x6E, &mBlockPosition, 0, nullptr);
	gpMarioParticleManager->emit(PARTICLE_MS_M_AMIATTACK, &mBlockPosition, 0, nullptr);
	mStateTimer                    = 120;
	mSiblingBlock0->mStateTimer    = 120;
	mSiblingBlock1->mStateTimer    = 120;
}

void TFileLoadBlock::touchPlayer(THitActor* param_1)
{
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_SEL_DBG")) {
		static int s_once[3] = {0,0,0};
		int b = mBlockIndex & 3;
		if (b < 3 && !s_once[b]) { s_once[b] = 1;
			fprintf(stderr, "[fileblock] touchPlayer block=%d state=normal? %d headAtk=%d timerEngaged=%d\n",
			        mBlockIndex, (int)isState(STATE_NORMAL), (int)marioHeadAttack(), (int)isStateTimerEngaged()); }
	}
#endif
	if (isState(STATE_NORMAL) && marioHeadAttack() && !isStateTimerEngaged())
		pushed();
}

BOOL TFileLoadBlock::receiveMessage(THitActor* sender, u32 message)
{
	if (isState(STATE_NORMAL) && message == HIT_MESSAGE_PUSH_UP
	    && !isStateTimerEngaged()) {
		pushed();
		return true;
	}

	return false;
}

void TFileLoadBlock::loadAfter()
{
	TMapObjBase::loadAfter();

	if (mBlockIndex == 0) {
		mSiblingBlock0
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＢ");
		mSiblingBlock1
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＣ");
	} else if (mBlockIndex == 1) {
		mSiblingBlock0
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＡ");
		mSiblingBlock1
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＣ");
	} else {
		mSiblingBlock0
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＡ");
		mSiblingBlock1
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＢ");
	}
}

void TFileLoadBlock::initMapObj()
{
	TMapObjBase::initMapObj();
	if (strcmp("FileLoadBlockA", getUnkF4()) == 0)
		mBlockIndex = 0;
	else if (strcmp("FileLoadBlockB", getUnkF4()) == 0)
		mBlockIndex = 1;
	else if (strcmp("FileLoadBlockC", getUnkF4()) == 0)
		mBlockIndex = 2;

	SMS_LoadParticle("/scene/map/map/ms_m_fileblock.jpa", 0x6E);

	mBlockPosition.set(mPosition.x, mPosition.y, mPosition.z);
}

TFileLoadBlock::TFileLoadBlock(const char* name)
    : TMapObjBase(name)
    , mBlockIndex(0)
    , mSiblingBlock0(nullptr)
    , mSiblingBlock1(nullptr)
{
	mBlockPosition.x = mBlockPosition.y = mBlockPosition.z = 0.0f;
}

void TMapObjOptionWall::onCollision() { mWarpCollision->setUp(); }

void TMapObjOptionWall::offCollision() { mWarpCollision->remove(); }

void TMapObjOptionWall::init()
{
	mWarpCollision = new TMapCollisionWarp;
	mWarpCollision->init("/scene/map/map/option_wall.col", 0, nullptr);
}

TMapObjOptionWall::TMapObjOptionWall(const char* name)
    : THitActor(name)
    , mWarpCollision(nullptr)
{
}
