#include <System/MarDirector.hpp>
#include <System/PerformList.hpp>
#include <System/EventWatcher.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <System/Application.hpp>
#include <dolphin/os.h>
#include <cstdlib>
#endif

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static void dummy(Vec* v) { *v = (Vec) { 0.0f, 0.0f, 0.0f }; }

void* gpSceneCmnDat;
int gpSceneCmnDatSize;

TMarDirector::TMarDirector()
    : unk18(nullptr)
    , mPerformListGX(nullptr)
    , mPerformListSilhouette(nullptr)
    , mPerformListGXPost(nullptr)
    , mPerformListMovement(nullptr)
    , mPerformListCalcAnim(nullptr)
    , unk30(new TPerformList)
    , unk34(new TPerformList)
    , unk38(new TPerformList)
    , unk3C(new TPerformList)
    , unk40(new TPerformList)
    , mShinePfLstMov(nullptr)
    , mShinePfLstAnm(nullptr)
    , unk4C(0)
    , unk4E(0)
    , unk50(0)
    , unk54(0)
    , unk68(0)
    , unk6C(120.0f)
    , unk80(nullptr)
    , mTalkingNPC(nullptr)
    , unkB8(nullptr)
    , unkBC(nullptr)
    , unkC8(0)
    , unkD4(0)
    , unkD8(0)
    , unkDC(nullptr)
    , unk128(0)
    , unk24C(0)
    , unk24D(0)
    , mTransitionActor(0)
    , unk25C(nullptr)
    , unk260(0)
{
	gpMarDirector = this;
	unk58         = 0;
	unk5C         = 0;
	mState        = STATE_UNK0;
	unk88.reserve(100);
	initLoadParticle();
	mNextGameState = 0;
	unk125 = 0;
	mGameState = 0;
	OSInitStopwatch(&unkE8, "イベント用ストップウォッチ");
}

void* TMarDirector::setupThreadFunc(void* param_1)
{
	// NOTE (native fix): same omitted-return UB as TMovieDirector::setupThreadFunc.
	// On PPC this was a tail call to loadResource() whose r3 result flows through as
	// the thread's return value; GCC -O2 emits no `ret`, so the thread runs off the
	// end into the adjacent function with a garbage `this` -> SEGV. Return the
	// loadResource() result so the setup thread terminates cleanly.
	return (void*)(intptr_t)((TMarDirector*)param_1)->loadResource();
}

extern OSThread gSetupThread;
extern u8* gpSetupThreadStack;

u32 TMarDirector::setup(JDrama::TDisplay* param_1, TMarioGamePad** param_2,
                        u8 param_3, u8 param_4)
{
	unkC0 = param_1;
	unk18 = param_2;
	mMap  = param_3;
	mScenario = param_4;
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_JKR_DBG") || getenv("SB_MOVIE_DBG"))
		OSReport("[mardir] TMarDirector::setup spawning setup thread "
		         "(map=%d unk30=%p)\n",
		         mMap, (void*)gpApplication.unk30);
#endif
#ifdef SMS_NATIVE_PLATFORM
	setupThreadFunc(this);
	// The native port runs setupThreadFunc synchronously (no OSCreateThread),
	// so gSetupThread has no backing NativeThread and OSIsThreadTerminated()
	// would answer FALSE forever. Mark the OSThread's state as MORIBUND so
	// TMarDirector::direct()'s `if (unk260 == 0) { if (!OSIsThreadTerminated
	// (&gSetupThread)) return 0; }` guard actually falls through to
	// setupObjects() and the game loop starts making per-frame progress —
	// without this the game thread returns 0 every frame and TApplication::
	// proc's outer while loop keeps recreating the TMarDirector, hitting
	// loadResource repeatedly, leaking the JKR heap and eventually OOMing on
	// mountFixed while the Aurora presenter watchdog fires.
	gSetupThread.state = OS_THREAD_STATE_MORIBUND;
#else
	OSCreateThread(&gSetupThread, &setupThreadFunc, this,
	               (void*)(gpSetupThreadStack + 0x10000), 0x10000, 0x11, 0);
	OSResumeThread(&gSetupThread);
#endif
	return 0;
}

void TMarDirector::registerEventWatcher(TEventWatcher* param_1)
{
	unk80->insert(param_1);
}
