#include <System/EventWatcher.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <Strategic/Spine.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/MSoundBGM.hpp>
#include <GC2D/Talk2D2.hpp>
#include <GC2D/GCConsole2.hpp>
#include <GC2D/ConsoleStr.hpp>
#include <NPC/NpcBase.hpp>
#include <NPC/NpcEvent.hpp>
#include <Map/MapEventSink.hpp>
#include <Map/PollutionManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/ModelGate.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/MapObjItem2.hpp>
#include <MoveBG/MapObjBall.hpp>
#include <Enemy/Conductor.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <Camera/CubeManagerBase.hpp>

// rogue includes needed for matching sinit & bss
#include <M3DUtil/InfectiousStrings.hpp>

// TODO: from M3UJoint or J3DJoint?
static void dummy()
{
	(Vec) { 0.0f, 0.0f, 0.0f };
	(Vec) { 1.0f, 1.0f, 1.0f };
}

template <class T> inline T* get_name_ref(TSpcSlice slice)
{
	T* result = nullptr;

	switch (slice.typeof()) {
	case TSpcSlice::TYPE_STRING:
		result = JDrama::TNameRefGen::search<T>(slice.getDataString());
		break;

	case TSpcSlice::TYPE_INT:
		// LP64: the slot carries a host pointer (pushed via pushPtr); read it at
		// full pointer width, never truncated through the 32-bit int path.
		result = (T*)slice.getDataPtr();
		break;
	}

	return result;
}

static void evGetSystemFlag(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TSpcSlice idSlice = interp->pop();
	int id            = idSlice.getDataInt();

	interp->push((int)TFlagManager::getInstance()->getFlag(id));
}

static void evSetSystemFlag(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	TSpcSlice valueSlice = interp->pop();
	TSpcSlice flagSlice  = interp->pop();
	int flag             = flagSlice.getDataInt();
	int value            = valueSlice.getDataInt();

	TFlagManager::getInstance()->setFlag(flag, value);

	interp->push();
}

static void evGetNameRefHandle(TSpcTypedInterp<TEventWatcher>* interp,
                               u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);

	JDrama::TNameRef* ref = JDrama::TNameRefGen::search<JDrama::TNameRef>(
	    interp->pop().getDataString());

	interp->pushPtr(ref);
}

static void evGetNameRefName(TSpcTypedInterp<TEventWatcher>* interp,
                             u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);

	int ref = interp->pop().getDataInt();

	const char* name = ref ? ((JDrama::TNameRef*)ref)->getName() : "";

	TSpcSlice slice;
	slice.setDataString(name);

	interp->push(slice);
}

static void getNameRefPtr(TSpcSlice) { }

static void evGetNPCType(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num) {
}

static void evSetFlagNPCDontTalk(TSpcTypedInterp<TEventWatcher>* interp,
                                 u32 arg_num)
{
}

static void evSetFlagNPCDontThrow(TSpcTypedInterp<TEventWatcher>* interp,
                                  u32 arg_num)
{
}

static void evSetFlagNPCDead(TSpcTypedInterp<TEventWatcher>* interp,
                             u32 arg_num)
{
}

static void evIsNearSameActors(TSpcTypedInterp<TEventWatcher>* interp,
                               u32 arg_num)
{
}

static void evIsNearActors(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
}

static void evGetTalkNPC(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);

	TBaseNPC* npc = gpMarDirector->getTalkingNPC();

	if (!npc)
		interp->push(0);
	else
		interp->pushPtr(npc);
}

static void evGetTalkNPCName(TSpcTypedInterp<TEventWatcher>* interp,
                             u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);

	TBaseNPC* npc = gpMarDirector->getTalkingNPC();

	if (!npc) {
		TSpcSlice slice;
		slice.setDataString("");
		interp->push(slice);
	} else {
		TSpcSlice slice;
		slice.setDataString(npc->getName());
		interp->push(slice);
	}
}

static void evSetTalkMsgID(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int p1 = interp->pop().getDataInt();
	int p2 = interp->pop().getDataInt();
	gpTalk2D->setMessageID(p2, p1);
	interp->push();
}

static void evGetTalkMode(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	interp->push((int)gpTalk2D->getTalkMode());
}

static void evGetTalkSelectedValue(TSpcTypedInterp<TEventWatcher>* interp,
                                   u32 arg_num)
{
}

static void evSetValue2TalkVariable(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
}

static void evIsTalkModeNow(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int value = gpMarDirector->isTalkModeNow() ? 1 : 0;
	interp->push(value);
}

static void evSetFlagNPCCanTaken(TSpcTypedInterp<TEventWatcher>* interp,
                                 u32 arg_num)
{
}

// TODO: removeme
extern const TNerveBase<TLiveActor>* NerveGetByIndex(int param_1);

static void evPushNerve4LiveActor(TSpcTypedInterp<TEventWatcher>* interp,
                                  u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	TSpcSlice nerveSlice                = interp->pop();
	int nerveId                         = nerveSlice.getDataInt();
	const TNerveBase<TLiveActor>* nerve = NerveGetByIndex(nerveId);
	const char* actorName               = interp->pop().getDataString();

	TLiveActor* liveActor = JDrama::TNameRefGen::search<TLiveActor>(actorName);
	if (liveActor && nerve)
		liveActor->mSpine->pushNerve(nerve);

	interp->push();
}

static void evIsOnLiveActorFlag(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int flag = interp->pop().getDataInt();

	TLiveActor* liveActor = get_name_ref<TLiveActor>(interp->pop());

	int result = 0;
	if (liveActor)
		result = liveActor->mLiveFlag & flag;
	interp->push(result);
}

static void evSetHide4LiveActor(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int value             = interp->pop().getDataInt();
	const char* actorName = interp->pop().getDataString();

	TLiveActor* liveActor = JDrama::TNameRefGen::search<TLiveActor>(actorName);
	if (liveActor) {
		if (value) {
			liveActor->onLiveFlag(LIVE_FLAG_HIDDEN);
			liveActor->onHitFlag(HIT_FLAG_NO_COLLISION);
		} else {
			liveActor->offLiveFlag(LIVE_FLAG_HIDDEN);
			liveActor->offHitFlag(HIT_FLAG_NO_COLLISION);
		}
	}

	interp->push();
}

static void evSetDead4LiveActor(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int value             = interp->pop().getDataInt();
	const char* actorName = interp->pop().getDataString();

	TLiveActor* liveActor = JDrama::TNameRefGen::search<TLiveActor>(actorName);
	if (liveActor) {
		if (value) {
			liveActor->onLiveFlag(LIVE_FLAG_DEAD);
			liveActor->onHitFlag(HIT_FLAG_NO_COLLISION);
		} else {
			liveActor->offLiveFlag(LIVE_FLAG_DEAD);
			liveActor->offHitFlag(HIT_FLAG_NO_COLLISION);
		}
	}

	interp->push();
}

static void evSetTimeLimit(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int time = interp->pop().getDataInt();
	OSResetStopwatch(&gpMarDirector->unkE8);
	gpMarDirector->unk120 = time;
	interp->push();
}

static void evSetAttentionTime(TSpcTypedInterp<TEventWatcher>* interp,
                               u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int tmp = interp->pop().getDataInt();

	// not implemented?

	interp->push();
}

static void evSetPollutionIncreaseCount(TSpcTypedInterp<TEventWatcher>* interp,
                                        u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int tmp = interp->pop().getDataInt();

	// not implemented?

	interp->push();
}

static void evGetRestTime(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	interp->push(gpMarDirector->getRestTime());
}

static void evGetPollutionLevel(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	interp->push((int)gpPollution->getPollutionDegree());
}

static void evSetEventStart(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
}

static void evSetEventEnd(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
}

static void evSetNextStage(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int scenario = interp->pop().getDataInt();
	int stage    = interp->pop().getDataInt();

	gpMarDirector->setNextStage((scenario & 0xff) + ((stage + 1) << 8),
	                            nullptr);

	interp->push();
}

static void evRegisterMovie(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int movieId = interp->pop().getDataInt();
	gpMarDirector->fireStreamingMovie(movieId);
	interp->push();
}

static void evGameOver(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarDirector->onUnk4CFlag(0x1);
	interp->push();
}

static void evIsGraffitoCoverage0(TSpcTypedInterp<TEventWatcher>* interp,
                                  u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	interp->push(gpPollution->cleanedAll() ? 1 : 0);
}

static void evSetGraffitoMultiplied(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
}

static void evIsBossDefeated(TSpcTypedInterp<TEventWatcher>* interp,
                             u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	interp->push(gpConductor->isBossDefeated() ? 1 : 0);
}

static void evLaunchEventClearDemo(TSpcTypedInterp<TEventWatcher>* interp,
                                   u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	TGCConsole2* console = gpMarDirector->getConsole();
	console->unk94->startAppearShineGet();
	console->unk34[0x13] = 1;
	interp->push();
}

static void evIsEMarioReachedToGoal(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
}

static void evIsEMarioDownWaitingToTalk(TSpcTypedInterp<TEventWatcher>* interp,
                                        u32 arg_num)
{
}

static void evStartEMarioRunAway(TSpcTypedInterp<TEventWatcher>* interp,
                                 u32 arg_num)
{
}

static void evStartEMarioGateDrawing(TSpcTypedInterp<TEventWatcher>* interp,
                                     u32 arg_num)
{
}

static void evStartEMarioDisappear(TSpcTypedInterp<TEventWatcher>* interp,
                                   u32 arg_num)
{
}

static void evStartOpenModelGate(TSpcTypedInterp<TEventWatcher>* interp,
                                 u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TSpcSlice gateSlice = interp->pop();
	TModelGate* gate    = get_name_ref<TModelGate>(gateSlice);
	gate->startOpen();
	interp->push();
}

static void evIsMapEventFinishedAll(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TSpcSlice eventSlice = interp->pop();
	TMapEvent* event     = get_name_ref<TMapEvent>(eventSlice);
	interp->push(event->isFinishedAll());
}

static void evRaiseBuilding(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);

	int id = interp->pop().getDataInt();

	TMapEventSinkShadowMario* event
	    = JDrama::TNameRefGen::search<TMapEventSinkShadowMario>(
	        "イベント（カゲマリオゲート）");

	if (event)
		event->raiseBuilding(id);

	interp->push();
}

static void evForceCloseTalk(TSpcTypedInterp<TEventWatcher>* interp,
                             u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);

	gpTalk2D->forceCloseTalk();

	interp->push();
}

static void evInsertTimer(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);

	int p1 = interp->pop().getDataInt();
	int p2 = interp->pop().getDataInt();

	if (p2 == 0)
		gpMarDirector->getConsole()->startAppearTimer(0, p1);
	else if (p2 == 2)
		gpMarDirector->getConsole()->startAppearTimer(1, p1);
	else
		gpMarDirector->getConsole()->startDisappearTimer();

	interp->push();
}

static void evStartTimer(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);

	int time = interp->pop().getDataInt();

	gpMarDirector->startTimer();
	gpMarDirector->getConsole()->startMoveTimer(time);

	interp->push();
}

static void evStartMonteman(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
}

static void evStopTimer(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarDirector->getConsole()->stopMoveTimer();
	interp->push();
}

static void evMonteManReachFlag(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
}

static void evGetTime(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	interp->push(gpMarDirector->getConsole()->getFinishedTime());
}

static void evKillShine(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TSpcSlice shineSlice = interp->pop();
	TShine* shine        = get_name_ref<TShine>(shineSlice);
	shine->kill();
	interp->push();
}

static void evKillMushroom1up(TSpcTypedInterp<TEventWatcher>* interp,
                              u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	get_name_ref<TMushroom1up>(interp->pop())->kill();
	interp->push();
}

static void evAppearMushroom1up(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TMushroom1up* mushroom = get_name_ref<TMushroom1up>(interp->pop());
	mushroom->appear();
	SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_1UP_APPEAR, 0, nullptr, 0);
	interp->push();
}

static void evAppearShineFromNPC(TSpcTypedInterp<TEventWatcher>* interp,
                                 u32 arg_num)
{
}

static void evAppearShine(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);

	const char* p1 = interp->pop().getDataString();
	const char* p2 = interp->pop().getDataString();

	if (strcmp(p1, "") != 0) {
		gpItemManager->makeShineAppearWithDemoOffset(p2, p1, 0.0f, 0.0f, 0.0f);
	} else {
		TShine* shine = JDrama::TNameRefGen::search<TShine>(p2);
		shine->appearWithTime(1200, -1, -1, -1);
	}
	interp->push();
}

static void evAppearShineFromNPCWithoutDemo(TSpcTypedInterp<TEventWatcher>*,
                                            u32)
{
}

static void evAppearShineFromKageMario(TSpcTypedInterp<TEventWatcher>* interp,
                                       u32 arg_num)
{
}

static void evAppearShineForWoodBox(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);

	int index = interp->pop().getDataInt();

	if (index) // huh?
		index = 1;

	static const char* sShineViewObjName[] = {
		"木箱ゲーム用シャイン１",
		"木箱ゲーム用シャイン２",
	};

	gpItemManager->makeShineAppearWithDemo(sShineViewObjName[index],
	                                       "木箱ゲーム用シャインカメラ",
	                                       -4010.0f, 9850.0f, -4040.0f);

	interp->push();
}

static void evChangeNozzle(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TWaterGun::TNozzleType id
	    = (TWaterGun::TNozzleType)interp->pop().getDataInt();
	if (id == TWaterGun::DivingHelmet)
		gpMarioOriginal->setDivHelm();
	else
		gpMarioOriginal->mWaterGun->changeNozzle(id, true);
	interp->push();
}

static void evStartMarioTalking(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarioOriginal->startTalking();
	interp->push();
}

static void evCheckWoodBox(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int p1 = interp->pop().getDataInt();
	int p2 = interp->pop().getDataInt();

	int count = p2 - p1 + 1;

	char buffer[] = "ゲーム木箱00";
	for (int i = p2; i <= p1; ++i) {
		if (i < 10) {
			buffer[10] = '0' + i;
			buffer[11] = 0;
		} else {
			buffer[10] = '0' + i / 10;
			buffer[11] = '0' + i % 10;
		}
		TMapObjBase* obj = JDrama::TNameRefGen::search<TMapObjBase>(buffer);
		if (obj && obj->checkLiveFlag(LIVE_FLAG_DEAD))
			--count;
	}

	interp->push(count);
}

static void evRefreshWoodBox(TSpcTypedInterp<TEventWatcher>* interp,
                             u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int p1 = interp->pop().getDataInt();
	int p2 = interp->pop().getDataInt();

	char buffer[] = "ゲーム木箱00";
	for (int i = p2; i <= p1; ++i) {
		if (i < 10) {
			buffer[10] = '0' + i;
			buffer[11] = 0;
		} else {
			buffer[10] = '0' + i / 10;
			buffer[11] = '0' + i % 10;
		}
		TMapObjBase* obj = JDrama::TNameRefGen::search<TMapObjBase>(buffer);
		if (obj)
			obj->appear();
	}

	interp->push();
}

static void evKillWoodBox(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int p1 = interp->pop().getDataInt();
	int p2 = interp->pop().getDataInt();

	char buffer[] = "ゲーム木箱00";
	for (int i = p2; i <= p1; ++i) {
		if (i < 10) {
			buffer[10] = '0' + i;
			buffer[11] = 0;
		} else {
			buffer[10] = '0' + i / 10;
			buffer[11] = '0' + i % 10;
		}
		TMapObjBase* obj = JDrama::TNameRefGen::search<TMapObjBase>(buffer);
		if (obj)
			obj->makeObjDead();
	}

	interp->push();
}

static void evIsInsideCube(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int cubeId = interp->pop().getDataInt();

	// TODO: getPos10cmAbove or something like that?
	JGeometry::TVec3<f32> pos = gpMarioOriginal->mPosition;
	pos.y += 10.0f;

	interp->push(gpCubeArea->isInCube(pos, cubeId) ? 1 : 0);
}

static void evSetMarioWaiting(TSpcTypedInterp<TEventWatcher>* interp,
                              u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarioOriginal->changePlayerStatus(MARIO_STATUS_WAIT, 0, true);
	interp->push();
}

static void evStartMareBottleDemo(TSpcTypedInterp<TEventWatcher>* interp,
                                  u32 arg_num)
{
}

static void evIsFinishMareBottleDemo(TSpcTypedInterp<TEventWatcher>* interp,
                                     u32 arg_num)
{
}

static void evIsInsideFastCube(TSpcTypedInterp<TEventWatcher>* interp,
                               u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int p1 = interp->pop().getDataInt();
	int p2 = interp->pop().getDataInt();

	// TODO: getPos10cmAbove or something like that?
	JGeometry::TVec3<f32> pos = gpMarioOriginal->mPosition;
	pos.y += 10.0f;

	int value;
	switch (p2) {
	case 0:
		value = gpCubeFastA->isInCube(pos, p1) ? 1 : 0;
		break;
	case 1:
		value = gpCubeFastB->isInCube(pos, p1) ? 1 : 0;
		break;
	case 2:
		value = gpCubeFastC->isInCube(pos, p1) ? 1 : 0;
		break;
	default:
		value = 0;
		break;
	}

	interp->push(value);
}

static void evSetTransScale(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(7, &arg_num);
	f32 tz = interp->pop().getDataFloat();
	f32 ty = interp->pop().getDataFloat();
	f32 tx = interp->pop().getDataFloat();
	f32 sz = interp->pop().getDataFloat();
	f32 sy = interp->pop().getDataFloat();
	f32 sx = interp->pop().getDataFloat();

	TSpcSlice objName = interp->pop();
	TMapObjBase* obj  = get_name_ref<TMapObjBase>(objName);

	obj->makeObjAppeared();
	obj->changeObjSRT(JGeometry::TVec3<f32>(sx, sy, sz),
	                  JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
	                  JGeometry::TVec3<f32>(tx, ty, tz));

	interp->push();
}

static void evSetEventID(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int p1          = interp->pop().getDataInt();
	TSpcSlice slice = interp->pop();
	// TODO: type unconfirmed
	TMapObjBase* event = get_name_ref<TMapObjBase>(slice);
	event->unk134      = p1;
	interp->push();
}

static void evManiCoinDown(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarDirector->getConsole()->startAppearStar();
	interp->push();
}

static void evStartBGM(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	MSBgm::startBGM(interp->pop().getDataInt());
	interp->push(TSpcSlice());
}

static void evEggYoshiStartFruit(TSpcTypedInterp<TEventWatcher>* interp,
                                 u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TSpcSlice eggSlice = interp->pop();
	TEggYoshi* egg     = get_name_ref<TEggYoshi>(eggSlice);
	if (!egg->checkLiveFlag(LIVE_FLAG_DEAD))
		egg->startFruit();
	interp->push();
}

static void evPutNozzle(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num) { }

static void evStopBGM(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	MSBgm::stopBGM(interp->pop().getDataInt(), 10);
	interp->push(TSpcSlice());
}

static void evStartSE(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int se = interp->pop().getDataInt();
	SMSGetMSound()->startSoundSystemSE(se, 0, nullptr, 0);
	interp->push(TSpcSlice());
}

static void evStartEventSE(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int se = interp->pop().getDataInt();
	SMSGetMSound()->startSoundSystemSE(se, 0, nullptr, 0);
	interp->push(TSpcSlice());
}

static void evStartMiss(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarioOriginal->loserExec();
	interp->push();
}

static void evChangeSunglass(TSpcTypedInterp<TEventWatcher>* interp,
                             u32 arg_num)
{
}

static void evSetCollision(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);
	int value       = interp->pop().getDataInt();
	TSpcSlice actor = interp->pop();

	THitActor* hitActor = get_name_ref<THitActor>(actor);

	if (!value)
		hitActor->onHitFlag(HIT_FLAG_NO_COLLISION);
	else
		hitActor->offHitFlag(HIT_FLAG_NO_COLLISION);

	interp->push();
}

static void evWarpMario(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num) { }

static void evStartAppearJetBalloon(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
	interp->verifyArgNum(2, &arg_num);

	int p1 = interp->pop().getDataInt();
	int p2 = interp->pop().getDataInt();

	switch (p2) {
	case 0:
		if (p1 == 1)
			gpMarDirector->getConsole()->startAppearJetBalloon(0, 8);
		break;

	case 1:
		if (p1 == 1)
			gpMarDirector->getConsole()->startAppearJetBalloon(1, 10);
		break;

	case 2:
		if (p1 == 1)
			gpMarDirector->getConsole()->startAppearRedCoin();
		break;
	}

	interp->push();
}

static void evSetEventForWaterMelon(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	TBigWatermelon* melon = get_name_ref<TBigWatermelon>(interp->pop());
	melon->startEvent();
	interp->push();
}

static void evAppearReadyGo(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarDirector->getConsole()->unk94->startAppearReady();
	interp->push();
}

static void evAppear8RedCoinsAndTimer(TSpcTypedInterp<TEventWatcher>* interp,
                                      u32 arg_num)
{
}

static void evWarpFrontToMario(TSpcTypedInterp<TEventWatcher>* interp,
                               u32 arg_num)
{
}

static void evOnNeutralMarioKey(TSpcTypedInterp<TEventWatcher>* interp,
                                u32 arg_num)
{
	interp->verifyArgNum(0, &arg_num);
	gpMarDirector->unk18[0]->onNeutralMarioKey();
	interp->push();
}

static void evInvalidatePad(TSpcTypedInterp<TEventWatcher>* interp, u32 arg_num)
{
	interp->verifyArgNum(1, &arg_num);
	int frames = interp->pop().getDataInt();

	gpMarDirector->unk18[0]->mDisabledFrames = frames;

	interp->push();
}

static void evIsWaterMelonIsReached(TSpcTypedInterp<TEventWatcher>* interp,
                                    u32 arg_num)
{
}

template <> void TSpcTypedBinary<TEventWatcher>::initUserBuiltin()
{
	// clang-format off
  bindSystemDataToSymbol("getSystemFlag", (void*)&evGetSystemFlag);
  bindSystemDataToSymbol("setSystemFlag", (void*)&evSetSystemFlag);
  bindSystemDataToSymbol("getNameRefHandle", (void*)&evGetNameRefHandle);
  bindSystemDataToSymbol("getNameRefName", (void*)&evGetNameRefName);
  bindSystemDataToSymbol("getNPCType", (void*)&evGetNPCType);
  bindSystemDataToSymbol("setFlagNPCDontTalk", (void*)&evSetFlagNPCDontTalk);
  bindSystemDataToSymbol("setFlagNPCDontThrow", (void*)&evSetFlagNPCDontThrow);
  bindSystemDataToSymbol("setFlagNPCDead", (void*)&evSetFlagNPCDead);
  bindSystemDataToSymbol("isNearSameActors", (void*)&evIsNearSameActors);
  bindSystemDataToSymbol("isNearActors", (void*)&evIsNearActors);
  bindSystemDataToSymbol("getTalkNPC", (void*)&evGetTalkNPC);
  bindSystemDataToSymbol("getTalkNPCName", (void*)&evGetTalkNPCName);
  bindSystemDataToSymbol("setTalkMsgID", (void*)&evSetTalkMsgID);
  bindSystemDataToSymbol("getTalkMode", (void*)&evGetTalkMode);
  bindSystemDataToSymbol("getTalkSelectedValue", (void*)&evGetTalkSelectedValue);
  bindSystemDataToSymbol("setValue2TalkVariable", (void*)&evSetValue2TalkVariable);
  bindSystemDataToSymbol("isTalkModeNow", (void*)&evIsTalkModeNow);
  bindSystemDataToSymbol("setFlagNPCCanTaken", (void*)&evSetFlagNPCCanTaken);
  bindSystemDataToSymbol("pushNerve4LiveActor", (void*)&evPushNerve4LiveActor);
  bindSystemDataToSymbol("isOnLiveActorFlag", (void*)&evIsOnLiveActorFlag);
  bindSystemDataToSymbol("setHide4LiveActor", (void*)&evSetHide4LiveActor);
  bindSystemDataToSymbol("setDead4LiveActor", (void*)&evSetDead4LiveActor);
  bindSystemDataToSymbol("setTimeLimit", (void*)&evSetTimeLimit);
  bindSystemDataToSymbol("setAttentionTime", (void*)&evSetAttentionTime);
  bindSystemDataToSymbol("setPollutionIncreaseCount", (void*)&evSetPollutionIncreaseCount);
  bindSystemDataToSymbol("getRestTime", (void*)&evGetRestTime);
  bindSystemDataToSymbol("getPollutionLevel", (void*)&evGetPollutionLevel);
  bindSystemDataToSymbol("setNextStage", (void*)&evSetNextStage);
  bindSystemDataToSymbol("registerMovie", (void*)&evRegisterMovie);
  bindSystemDataToSymbol("gameOver", (void*)&evGameOver);
  bindSystemDataToSymbol("isGraffitoCoverage0", (void*)&evIsGraffitoCoverage0);
  bindSystemDataToSymbol("setGraffitoMultiplied", (void*)&evSetGraffitoMultiplied);
  bindSystemDataToSymbol("isBossDefeated", (void*)&evIsBossDefeated);
  bindSystemDataToSymbol("launchEventClearDemo", (void*)&evLaunchEventClearDemo);
  bindSystemDataToSymbol("isEMarioReachedToGoal", (void*)&evIsEMarioReachedToGoal);
  bindSystemDataToSymbol("isEMarioDownWaitingToTalk", (void*)&evIsEMarioDownWaitingToTalk);
  bindSystemDataToSymbol("startEMarioRunAway", (void*)&evStartEMarioRunAway);
  bindSystemDataToSymbol("startEMarioGateDrawing", (void*)&evStartEMarioGateDrawing);
  bindSystemDataToSymbol("startEMarioDisappear", (void*)&evStartEMarioDisappear);
  bindSystemDataToSymbol("startOpenModelGate", (void*)&evStartOpenModelGate);
  bindSystemDataToSymbol("isMapEventFinishedAll", (void*)&evIsMapEventFinishedAll);
  bindSystemDataToSymbol("raiseBuilding", (void*)&evRaiseBuilding);
  bindSystemDataToSymbol("forceCloseTalk", (void*)&evForceCloseTalk);
  bindSystemDataToSymbol("insertTimer", (void*)&evInsertTimer);
  bindSystemDataToSymbol("startTimer", (void*)&evStartTimer);
  bindSystemDataToSymbol("startMonteman", (void*)&evStartMonteman);
  bindSystemDataToSymbol("stopTimer", (void*)&evStopTimer);
  bindSystemDataToSymbol("monteManReachFlag", (void*)&evMonteManReachFlag);
  bindSystemDataToSymbol("getTime", (void*)&evGetTime);
  bindSystemDataToSymbol("killShine", (void*)&evKillShine);
  bindSystemDataToSymbol("killMushroom1up", (void*)&evKillMushroom1up);
  bindSystemDataToSymbol("appearMushroom1up", (void*)&evAppearMushroom1up);
  bindSystemDataToSymbol("appearShineFromNPC", (void*)&evAppearShineFromNPC);
  bindSystemDataToSymbol("appearShineFromNPCWithoutDemo", (void*)&evAppearShineFromNPCWithoutDemo);
  bindSystemDataToSymbol("appearShineFromKageMario", (void*)&evAppearShineFromKageMario);
  bindSystemDataToSymbol("appearShine", (void*)&evAppearShine);
  bindSystemDataToSymbol("appearShineForWoodBox", (void*)&evAppearShineForWoodBox);
  bindSystemDataToSymbol("changeNozzle", (void*)&evChangeNozzle);
  bindSystemDataToSymbol("startMarioTalking", (void*)&evStartMarioTalking);
  bindSystemDataToSymbol("isInsideCube", (void*)&evIsInsideCube);
  bindSystemDataToSymbol("setMarioWaiting", (void*)&evSetMarioWaiting);
  bindSystemDataToSymbol("setTransScale", (void*)&evSetTransScale);
  bindSystemDataToSymbol("setEventID", (void*)&evSetEventID);
  bindSystemDataToSymbol("startBGM", (void*)&evStartBGM);
  bindSystemDataToSymbol("stopBGM", (void*)&evStopBGM);
  bindSystemDataToSymbol("startMiss", (void*)&evStartMiss);
  bindSystemDataToSymbol("startSE", (void*)&evStartSE);
  bindSystemDataToSymbol("startEventSE", (void*)&evStartEventSE);
  bindSystemDataToSymbol("changeSunglass", (void*)&evChangeSunglass);
  bindSystemDataToSymbol("setCollision", (void*)&evSetCollision);
  bindSystemDataToSymbol("warpMario", (void*)&evWarpMario);
  bindSystemDataToSymbol("startAppearJetBalloon", (void*)&evStartAppearJetBalloon);
  bindSystemDataToSymbol("appear8RedCoinsAndTimer", (void*)&evAppear8RedCoinsAndTimer);
  bindSystemDataToSymbol("warpFrontToMario", (void*)&evWarpFrontToMario);
  bindSystemDataToSymbol("appearReadyGo", (void*)&evAppearReadyGo);
  bindSystemDataToSymbol("onNeutralMarioKey", (void*)&evOnNeutralMarioKey);
  bindSystemDataToSymbol("invalidatePad", (void*)&evInvalidatePad);
  bindSystemDataToSymbol("checkWoodBox", (void*)&evCheckWoodBox);
  bindSystemDataToSymbol("refreshWoodBox", (void*)&evRefreshWoodBox);
  bindSystemDataToSymbol("killWoodBox", (void*)&evKillWoodBox);
  bindSystemDataToSymbol("maniCoinFallDown", (void*)&evManiCoinDown);
  bindSystemDataToSymbol("eggYoshiStartFruit", (void*)&evEggYoshiStartFruit);
  bindSystemDataToSymbol("putNozzle", (void*)&evPutNozzle);
  bindSystemDataToSymbol("startMareBottleDemo", (void*)&evStartMareBottleDemo);
  bindSystemDataToSymbol("isFinishMareBottleDemo", (void*)&evIsFinishMareBottleDemo);
  bindSystemDataToSymbol("isInsideFastCube", (void*)&evIsInsideFastCube);
  bindSystemDataToSymbol("setEventForWaterMelon", (void*)&evSetEventForWaterMelon);
  bindSystemDataToSymbol("isWaterMelonIsReached", (void*)&evIsWaterMelonIsReached);
	// clang-format on
	TNpcEvent::initNpcBuiltin(this);
}

TEventWatcher::TEventWatcher(const char* name)
    : JDrama::TViewObj(name)
    , mBinary(nullptr)
    , mInterp(nullptr)
{
}

TEventWatcher::TEventWatcher(const char* name, const char* script)
    : JDrama::TViewObj(name)
    , mBinary(nullptr)
    , mInterp(nullptr)
{
	launchScript(script);
}

void TEventWatcher::launchScript(const char* script)
{
	if (void* res = JKRGetResource(script)) {
		mBinary = new TSpcTypedBinary<TEventWatcher>(res);
		mBinary->init();
		mInterp = new TSpcTypedInterp<TEventWatcher>(mBinary, this, 0x20, 0x20,
		                                             0x20, 0x20);
	}
}

void TEventWatcher::perform(u32 param_1, JDrama::TGraphics*)
{
	if ((param_1 & 1) && mInterp)
		mInterp->update();
}
