// SelectDir.cpp — file-select director (TSelectDir).
//
// Reconstructed from the GMSE01 DOL; the community decomp left this TU empty (file-select runs
// only under Dolphin's JIT in the hybrid build). Anchors: setup 0x80177400, setupThreadFunc
// 0x801773e0, rsetup 0x801761b0, direct 0x80175ec4, changeOrder 0x80176188, FUN_802f7d28 =
// JDrama::TDirector::direct().
//
// Structure mirrors the sister directors TMenuDirector / TMovieDirector (MenuDir.cpp,
// MovieDirector.cpp) — same JDrama display-build idiom (mount arc, build groups + screens +
// ortho projections, wire via assignCamera/assignViewObj; per-frame TDirector::direct()).
//
// PORT STATUS (incremental): this milestone builds the background-gradient sub-scene
// (Group Grad + TSelectGrad + Screen Grad). The full rsetup also creates TSelectMenu (the file
// windows), TSelectShineManager (shine counts), the 2D/3D screens and the JPA particle emitters
// — those are reconstructed in follow-up steps and are marked TODO below. Nothing here is a
// shortcut around the original: the omitted objects are simply not-yet-ported.

#include <System/SelectDir.hpp>
#include <System/MarioGamePad.hpp>
#include <System/Application.hpp>
#include <System/Resolution.hpp>
#include <GC2D/SelectGrad.hpp>
#include <GC2D/SelectMenu.hpp>
#include <GC2D/ScrnFader.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <JSystem/JDrama/JDRDStageGroup.hpp>
#include <JSystem/JDrama/JDRDStage.hpp>
#include <JSystem/JDrama/JDREfbCtrl.hpp>
#include <JSystem/JDrama/JDRScreen.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JDrama/JDRRect.hpp>
#include <dolphin/os.h>
#include <cstdio>
#include <cstdlib>

static bool sel_dbg()
{
	static int v = -1;
	if (v < 0) { const char* e = getenv("SB_SEL_DBG"); v = (e && e[0] && e[0] != '0') ? 1 : 0; }
	return v != 0;
}

// Shared boot setup-thread (also used by TMenuDirector / TMovieDirector).
extern OSThread gSetupThread;
extern u8* gpSetupThreadStack;

TSelectDir::TSelectDir()
    : JDrama::TDirector("<TSelectDir>")
    , mGamePad(nullptr)
    , mDisplay(nullptr)
    , mSelectMenu(nullptr)
    , mSelectGrad(nullptr)
    , mSelectShineMgr(nullptr)
    , mArchive(nullptr)
    , mEmitterMgr0(nullptr)
    , mEmitterMgr1(nullptr)
    , mSetupDone(0)
    , mStage(0)
    , mScreen2D(nullptr)
    , mScreenGrad(nullptr)
    , mFlag4C(0)
{
}

// setup @0x80177400 — store display/pad/stage, spawn the setup thread (which runs rsetup).
void TSelectDir::setup(JDrama::TDisplay* display, TMarioGamePad* pad, unsigned char stage)
{
	mDisplay = display;
	mGamePad = pad;
	mStage   = stage;
	// (DOL also resets a RumbleMgr here — irrelevant to rendering, deferred.)
#ifdef SMS_NATIVE_PLATFORM
	setupThreadFunc(this);
#else
	OSCreateThread(&gSetupThread, &setupThreadFunc, this,
	               gpSetupThreadStack + 0x10000, 0x10000, 0x11, 0);
	OSResumeThread(&gSetupThread);
#endif
}

// setupThreadFunc @0x801773e0 — just runs rsetup on the worker thread.
void* TSelectDir::setupThreadFunc(void* param)
{
	((TSelectDir*)param)->rsetup();
	return nullptr;
}

// rsetup @0x801761b0 — build the JDrama display. (Gradient sub-scene; see PORT STATUS above.)
int TSelectDir::rsetup()
{
	if (sel_dbg()) std::fprintf(stderr, "[sel] rsetup ENTER stage=%d\n", mStage);
	void* arcBlob = SMSLoadArchive("/data/select.arc", nullptr, 0, nullptr);
	if (sel_dbg()) std::fprintf(stderr, "[sel] arcBlob=%p\n", arcBlob);
	mArchive      = new JKRMemArchive;
	if (!mArchive->mountFixed(arcBlob, MBF_0)) {
		if (sel_dbg()) std::fprintf(stderr, "[sel] mountFixed FAILED\n");
		return 1;
	}

	JDrama::TViewObjPtrListT<JDrama::TViewObj>* rootViewObjs
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("root View Objs");
	unk10 = rootViewObjs;
	unk14 = new JDrama::TDStageGroup(mDisplay);

	// Group Grad → holds the animated background gradient.
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* groupGrad
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("Group Grad");
	rootViewObjs->getChildren().push_back(groupGrad);

	mSelectGrad = new TSelectGrad("<TSelectGrad>");
	mSelectGrad->setStageColor(mStage);
	groupGrad->getChildren().push_back(mSelectGrad);

	// Group 2D → holds the file-slot select windows (TSelectMenu). The DOL rsetup also
	// builds Group 3D / Group 2D Particle + TSelectShineManager + two JPAEmitterManager
	// particle sets; those are not-yet-ported (the menu does not need them to render).
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group2D
	    = new JDrama::TViewObjPtrListT<JDrama::TViewObj>("Group 2D");
	rootViewObjs->getChildren().push_back(group2D);

	mSelectMenu = new TSelectMenu("<TSelectMenu>");
	mSelectMenu->mGamePad = mGamePad; // DOL: menu->unk100 = TSelectDir gamepad
	group2D->getChildren().push_back(mSelectMenu);

	// Display + EFB-copy rect.
	JDrama::TDStageDisp* stageDisp = new JDrama::TDStageDisp;
	unk14->getChildren().push_back(stageDisp);
	JDrama::TRect rect(0, 0, SMSGetTitleRenderWidth(), SMSGetTitleRenderHeight());
	stageDisp->getEfbCtrlDisp()->TEfbCtrl::setSrcRect(rect);

	// Screen Grad: ortho projection (0,16,600,464) over the gradient group. rsetup overrides
	// the default ±1 near/far to -100/100 (the TSelectGrad quad sits at z=-100, on the near
	// plane); with the ±1 default the quad is z-clipped and the screen stays black.
	JDrama::TOrthoProj* gradCam = new JDrama::TOrthoProj(0.0f, 16.0f, 600.0f, 464.0f);
	gradCam->mNear = -100.0f;
	gradCam->mFar  = 100.0f;
	groupGrad->getChildren().push_back(gradCam);

	JDrama::TScreen* gradScreen = new JDrama::TScreen(rect, "Screen Grad");
	stageDisp->getUnk14()->getChildren().push_back(gradScreen);
	gradScreen->assignCamera(gradCam);
	gradScreen->assignViewObj(groupGrad);
	mScreenGrad = gradScreen;

	// Screen 2D: full-frame ortho over Group 2D. TSelectMenu::perform overrides the GX
	// projection with its own J2DOrthoGraph (built from the graphics viewport this screen
	// establishes), so this camera only fixes the screen's viewport rect; ±100 near/far
	// keeps any pane z in range. Added AFTER Screen Grad so the gradient draws behind it.
	JDrama::TOrthoProj* menuCam = new JDrama::TOrthoProj(
	    0.0f, 0.0f, (float)SMSGetTitleRenderWidth(), (float)SMSGetTitleRenderHeight());
	menuCam->mNear = -100.0f;
	menuCam->mFar  = 100.0f;
	group2D->getChildren().push_back(menuCam);

	JDrama::TScreen* menuScreen = new JDrama::TScreen(rect, "Screen 2D");
	stageDisp->getUnk14()->getChildren().push_back(menuScreen);
	menuScreen->assignCamera(menuCam);
	menuScreen->assignViewObj(group2D);
	mScreen2D = menuScreen;

	if (sel_dbg()) std::fprintf(stderr,
	    "[sel] rsetup DONE: grad=%p gradScr=%p menu=%p menuScr=%p disp=%p\n",
	    (void*)mSelectGrad, (void*)gradScreen, (void*)mSelectMenu, (void*)menuScreen,
	    (void*)stageDisp);

	// TODO(file-select port): the original sets per-screen unkC draw-order masks here
	// (changeOrder toggles the 2D vs grad screen). With both screens drawn at the default
	// mask (0 = draw) the menu composites over the gradient, which is the resting layout.
	return 0;
}

// changeOrder @0x80176188 — swap which screen draws (2D menu ⇄ gradient). Faithful, but the
// 2D screen is not built yet; guarded so it is safe to call once TSelectMenu lands.
void TSelectDir::changeOrder()
{
	if (mScreen2D)
		mScreen2D->unkC.on(0xb);
	if (mScreenGrad)
		mScreenGrad->unkC.off(0xb);
}

// direct @0x80175ec4 — wait for the setup thread, then per-frame draw via TDirector::direct().
int TSelectDir::direct()
{
	if (!mSetupDone) {
		if (!OSIsThreadTerminated(&gSetupThread)) {
			if (sel_dbg()) { static int n=0; if((n++%120)==0) std::fprintf(stderr, "[sel] direct: waiting setup thread\n"); }
			return TApplication::APP_STATE_DEFAULT - 1; // 0: still setting up
		}
		void* res;
		OSJoinThread(&gSetupThread, &res);
		mSetupDone = 1;
		if (sel_dbg()) std::fprintf(stderr, "[sel] direct: setup DONE, entering draw loop\n");
		// Reveal the screen. (The DOL runs TSMSFader::startWipe here; a fade-in is the
		// faithful reveal — exact wipe transition is TODO.)
		gpApplication.mFader->startFadeinT(0.25f);
		// First-frame: build the menu's J2DScreen from the mounted select.arc (DOL direct
		// @0x80175ec4 calls TSelectMenu::setup then startOpenWindow). The window-open
		// animation (startOpenWindow + perform's calc) is not-yet-ported, so the menu
		// renders at its .blo default layout.
		if (mSelectMenu)
			mSelectMenu->setup(mStage, mArchive, mSelectShineMgr, this);
		// TODO(file-select port): startOpenWindow + the open animation / input navigation.
		return TApplication::APP_STATE_DEFAULT - 1; // 0
	}

	if (sel_dbg()) { static int n=0; if((n++%120)==0) std::fprintf(stderr, "[sel] direct: drawing (TDirector::direct)\n"); }
	JDrama::TDirector::direct(); // testPerform(3) on unk10 (calc), testPerform(8) on unk14 (draw)

	// TODO(file-select port): menu navigation / file-pick transition (sets next area + fade-out).
	return TApplication::APP_STATE_DEFAULT; // keep running file-select
}
