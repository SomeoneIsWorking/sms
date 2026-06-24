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
	mState = 1;
}

void TFileLoadBlock::makeBlockRock()
{
	startAnim(1);
	mState = 2;
}

static int sRumbleTime = 8;

void TFileLoadBlock::pushed()
{
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_SEL_DBG"))
		fprintf(stderr, "[fileblock] pushed() block=%d -> setSelected\n", unk138);
#endif
	startBck("fileloadblock");
	gpCardLoad->setSelected(unk138);
	SMSRumbleMgr->start(0x15, sRumbleTime, (float*)nullptr);
	gpMarioParticleManager->emit(0x6E, &unk144, 0, nullptr);
	gpMarioParticleManager->emit(0x39, &unk144, 0, nullptr);
	mTimeTilAppear         = 0x78;
	unk13C->mTimeTilAppear = 0x78;
	unk140->mTimeTilAppear = 0x78;
}

void TFileLoadBlock::touchPlayer(THitActor* param_1)
{
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_SEL_DBG")) {
		static int s_once[3] = {0,0,0};
		int b = unk138 & 3;
		if (b < 3 && !s_once[b]) { s_once[b] = 1;
			fprintf(stderr, "[fileblock] touchPlayer block=%d state1=%d headAtk=%d waiting=%d\n",
			        unk138, (int)isState(1), (int)marioHeadAttack(), (int)isWaitingToAppear()); }
	}
#endif
	if (isState(1) && marioHeadAttack() && !isWaitingToAppear()) {
		pushed();
	}
}

BOOL TFileLoadBlock::receiveMessage(THitActor* sender, u32 message)
{

	if (isState(1) && message == HIT_MESSAGE_UNK2 && !isWaitingToAppear()) {
		pushed();
		return true;
	}

	return false;
}

void TFileLoadBlock::loadAfter()
{
	TMapObjBase::loadAfter();

	if (unk138 == 0) {
		unk13C
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＢ");
		unk140
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＣ");
	} else if (unk138 == 1) {
		unk13C
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＡ");
		unk140
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＣ");
	} else {
		unk13C
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＡ");
		unk140
		    = JDrama::TNameRefGen::search<TFileLoadBlock>("ロードブロックＢ");
	}
}

void TFileLoadBlock::initMapObj()
{
	TMapObjBase::initMapObj();
	if (strcmp("FileLoadBlockA", getUnkF4()) == 0)
		unk138 = 0;
	else if (strcmp("FileLoadBlockB", getUnkF4()) == 0)
		unk138 = 1;
	else if (strcmp("FileLoadBlockC", getUnkF4()) == 0)
		unk138 = 2;

	SMS_LoadParticle("/scene/map/map/ms_m_fileblock.jpa", 0x6E);

	unk144.set(mPosition.x, mPosition.y, mPosition.z);
}

TFileLoadBlock::TFileLoadBlock(const char* name)
    : TMapObjBase(name)
    , unk138(0)
    , unk13C(nullptr)
    , unk140(nullptr)
{
	unk144.x = unk144.y = unk144.z = 0.0f;
}

void TMapObjOptionWall::onCollision() { unk68->setUp(); }

void TMapObjOptionWall::offCollision() { unk68->remove(); }

void TMapObjOptionWall::init()
{
	unk68 = new TMapCollisionWarp;
	unk68->init("/scene/map/map/option_wall.col", 0, nullptr);
}

TMapObjOptionWall::TMapObjOptionWall(const char* name)
    : THitActor(name)
    , unk68(nullptr)
{
}
