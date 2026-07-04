#include <System/MarDirector.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <System/Application.hpp>
#include <System/MSoundMainSide.hpp>
#include <System/MarioGamePad.hpp>
#include <System/PerformList.hpp>
#include <System/StageUtil.hpp>
#include <System/FlagManager.hpp>
#include <System/CardManager.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <Strategic/ObjHitCheck.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <THPPlayer/THPPlayer.h>
#include <GC2D/GCConsole2.hpp>
#include <GC2D/ConsoleStr.hpp>
#include <GC2D/ScrnFader.hpp>
#include <GC2D/PauseMenu2.hpp>
#include <GC2D/CardSave.hpp>
#include <GC2D/Guide.hpp>
#include <GC2D/SunGlass.hpp>
#include <GC2D/Talk2D2.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/MarioPositionObj.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/MapObjDolpic.hpp>
#include <NPC/NpcBase.hpp>
#include <dolphin/gx.h>

#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
extern "C" bool sb_boot_drive_scene();   // native/src/scene_drive.cpp
extern "C" int  sb_own_gxlist();                 // scene_drive.cpp — SB_OWN_GXLIST
extern "C" int  sb_boot_capture_begin_scene();   // sms_boot_j3d_capture.cpp — once-per-present lock
extern "C" void sb_boot_capture_end_scene();
extern "C" void sb_boot_capture_set_phase(int);  // tag captured batches with their source perform list
static bool sb_dir_dbg() {
	static int v = -1;
	if (v < 0) { const char* e = getenv("SB_J3D_DBG"); v = (e && e[0] && e[0] != '0') ? 1 : 0; }
	return v != 0;
}
static int sb_pl_count(TPerformList* l) {
	if (!l) return -1;
	int n = 0;
	for (auto it = l->getChildren().begin(); it != l->getChildren().end(); ++it) ++n;
	return n;
}
#endif

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

extern OSThread gSetupThread;

int TMarDirector::direct()
{
	int vsyncRate = 600 / (int)SMSGetVSyncTimesPerSec();

	if (unk260 == 0) {
		if (!OSIsThreadTerminated(&gSetupThread))
			return 0;

		void* local_40;
		OSJoinThread(&gSetupThread, &local_40);
		if (local_40)
			return 4;

		setupObjects();
		unk260 = 1;
	}

	u32 desiredAppState = TApplication::APP_STATE_DEFAULT;

	JDrama::TGraphics local_140;

#ifdef SMS_NATIVE_PLATFORM
	// The first loop iteration of every direct() call takes the else-branch
	// (catch-up render) because unk4C&0x4000 persists from the prior call's
	// `break` (which skips the line-180 clear). That branch consumes
	// local_140.mRenderMode before the unk34 path's TFrmGXSet (the only place
	// the render mode is copied from the display) runs. On GC local_140 reuses
	// the same stack slot and still holds the previous frame's (display-derived)
	// render mode, so it works; on LP64 that slot is eventually clobbered and
	// the uninitialized GXRenderModeObj (efbHeight==0) divide-by-zeros in
	// GXGetYScaleFactor during IssueGXCopyDisp. Seed the TFrmGXSet-managed
	// fields from the display (unkC0) up front — the same source of truth
	// TFrmGXSet::perform copies from — so the catch-up render is deterministic.
	local_140.mFrameBuffer = unkC0->getCurrentFrameBuffer();
	local_140.mRenderMode  = unkC0->getRenderMode();
	local_140.getUnkFC().setBit(0x1, unkC0->unk64.check(0x1));
	local_140.getUnkFC().setBit(0x2, unkC0->unk64.check(0x2));
	local_140.getUnkFC().setBit(0x4, unkC0->unk64.check(0x4));
	local_140.getUnkFC().setBit(0x8, unkC0->unk64.check(0x8));
	local_140.getUnkFC().setBit(0x10, unkC0->unk64.check(0x10));
	local_140.getUnkFC().setBit(0x20, unkC0->unk64.check(0x20));
	local_140.getUnkFC().setBit(0x40, unkC0->unk64.check(0x40));
	local_140.mFBClamp    = unkC0->getFBClamp();
	local_140.mClearColor = unkC0->getClearColor();
	local_140.mClearZ     = unkC0->getClearZ();
#endif

	u8 bVar2 = gpMSound->unkA8;
	unk54 += vsyncRate;

#ifdef SMS_NATIVE_PLATFORM
	if (sb_dir_dbg()) {
		static bool once = false;
		if (!once) {
			once = true;
			fprintf(stderr,
			        "[dir] LIST SIZES: unk30=%d unk34(preEntry)=%d unk38=%d unk3C=%d unk40=%d "
			        "GX=%d GXPost=%d Silhouette=%d CalcAnim=%d Movement=%d ShineMov=%d ShineAnm=%d\n",
			        sb_pl_count(unk30), sb_pl_count(unk34), sb_pl_count(unk38), sb_pl_count(unk3C),
			        sb_pl_count(unk40), sb_pl_count(mPerformListGX), sb_pl_count(mPerformListGXPost),
			        sb_pl_count(mPerformListSilhouette), sb_pl_count(mPerformListCalcAnim),
			        sb_pl_count(mPerformListMovement), sb_pl_count(mShinePfLstMov),
			        sb_pl_count(mShinePfLstAnm));
		}
		static long dc = 0;
		if ((++dc % 200) == 0 || (unk4C & 0x000f))
			fprintf(stderr, "[dir] direct() call %ld entry-state unk4C=0x%x mState=%d unk124=%d\n",
			        dc, unk4C, mState, (int)unk124);
	}
#endif

	int i = 0;
	for (;;) {
		if (!(unk4C & 0x4000)) {
			++i;
			if (i == 1)
				unk4C |= 0x2000;
			unk54 -= 5;
			if (unk54 < 5)
				unk4C |= 0x4000;

			// inline?
			u32 uVar8 = 0;
			u8 bVar7  = bVar2;
			if (unk4C & 0x4000) {
				if (unk258)
					unk258->stageLoop();
			} else {
				uVar8 |= 2;
				bVar7 &= ~0x1;
			}
			gpMSound->unkA8 = bVar7;

			switch (mState) {
			case STATE_UNK5:
			case STATE_UNK11:
			case STATE_UNK12:
				uVar8 |= 2;
				uVar8 |= 1;
				break;

			case STATE_UNK10:
				uVar8 |= 2;
				uVar8 |= 1;
				break;
			}

			if (!(uVar8 & 1))
				++unk58;
			++unk5C;
			if (unk4C & 0x2000) {
				if (mState == STATE_UNK4 || mState == STATE_UNK7) {
					SMSRumbleMgr->update();
				}
			} else {
				for (int i = 0; i < 4; ++i) {
					// TODO: inline?
					TMarioGamePad* pad    = unk18[i];
					pad->mButton.mTrigger = 0;
					pad->mButton.mRelease = 0;

					unk18[i]->updateMeaning();
					unk18[i]->offFlag(0x10);
				}
			}

			u32 tmp = 0;
			if (unk4C & 0x2000)
				tmp |= 1;
			if (unk4C & 0x4000)
				tmp |= 2;
			local_140.unk2 = tmp;

			// inline
			bool bVar1 = true;
			if ((unk58 & 1) || (unk58 & 2))
				bVar1 = false;

			if (bVar1)
				gpObjHitCheck->checkActorsHit();
			else
				gpObjHitCheck->clearHitNum();

			u32 uVar11 = ~uVar8;
			u32 uVar4  = uVar11;
			if (unk58 & 1)
				uVar4 &= ~0x100;
			if (unk58 & 2)
				uVar4 &= 0x200;
			// Verified against the original DOL (0x80299b2c-0x80299b6c): the unk4E&1
			// branch selects the SHINE-stage movement list; the else branch (normal
			// stages, incl. Delfino Plaza) drives mPerformListMovement. The prior decomp
			// called mShinePfLstMov in BOTH branches (a misdecompilation that left the
			// normal-stage movement/calc pass dead → NPC calcRootMatrix never ran).
			if (unk4E & 1)
				mShinePfLstMov->perform(uVar4, &local_140);
			else
				mPerformListMovement->perform(uVar4, &local_140);

			u32 uVar44 = 0;
			if (!(unk4C & 0x4000))
				uVar44 |= 2;
			unk30->perform(~uVar44, &local_140);
			movement();
#ifdef SMS_NATIVE_PLATFORM
			if (getenv("SB_CALCPASS_DBG")) {
				static int n = 0;
				if (n < 6) { ++n;
					fprintf(stderr, "[calcpass] uVar8=0x%x uVar11=0x%x unk4E=0x%x skip(uVar8&2)=%d "
					        "list=%s CalcAnim&uVar11&0x2000000=0x%x\n",
					        uVar8, uVar11, unk4E, (uVar8 & 2) != 0,
					        (unk4E & 1) ? "CalcAnim" : "ShinePfLstAnm",
					        uVar11 & 0x2000000);
				}
			}
#endif
			if (!(uVar8 & 2)) {
				// Verified against the original DOL (0x80299bac-0x80299bf4): unk4E&1 →
				// SHINE-stage anim list; else (normal stages) → mPerformListCalcAnim
				// (the list holding コンダクター/NPC managers, マップグループ, etc.). The
				// decomp had these two SWAPPED.
				if (unk4E & 1)
					mShinePfLstAnm->perform(uVar11, &local_140);
				else
					mPerformListCalcAnim->perform(uVar11, &local_140);
			}

			if (unk4C & 0x4000) {
				local_140.unk2 = 0;
#ifdef SMS_NATIVE_PLATFORM
				if (sb_dir_dbg()) {
					static long n = 0;
					if ((++n % 200) == 0 || n <= 2)
						fprintf(stderr, "[dir] unk34->perform (ENTRY pass) n=%ld\n", n);
				}
#endif
				unk34->perform(0xffffffff, &local_140);
				break;
			}
		} else {
#ifdef SMS_NATIVE_PLATFORM
			if (sb_dir_dbg()) {
				static long n = 0;
				if ((++n % 200) == 0 || n <= 2)
					fprintf(stderr, "[dir] RENDER-else branch n=%ld\n", n);
			}
#endif
			local_140.unk2 = 0;
#ifdef SMS_NATIVE_PLATFORM
			// SB_OWN_GXLIST: this real master GX perform-list render IS the owned native draw
			// path. Bracket it with the capture's once-per-present lock so the J3DShape::draw taps
			// land in the present buffer exactly once per VI present (begin_scene returns 1 only on
			// the first render after the present consumed the prior frame). The hand-driving in
			// scene_drive is then setup-only. See debug_journal/2026-06-25_own_gxlist_draw.md.
			const bool sb_own = sb_own_gxlist() != 0;
			const bool sb_capture_now = sb_own && sb_boot_capture_begin_scene() != 0;
			if (sb_capture_now) sb_boot_capture_set_phase(1);
#endif
			unk40->perform(0xffffffff, &local_140);
#ifdef SMS_NATIVE_PLATFORM
			if (sb_capture_now) sb_boot_capture_set_phase(2);
#endif
			unk38->perform(0xffffffff, &local_140);
#ifdef SMS_NATIVE_PLATFORM
			if (sb_capture_now) sb_boot_capture_set_phase(3);
#endif
			unk3C->perform(0xffffffff, &local_140);
#ifdef SMS_NATIVE_PLATFORM
			if (sb_capture_now) sb_boot_capture_set_phase(4);
#endif
			mPerformListGX->perform(0xffffffff, &local_140);
			if ((gpSilhouetteManager->unk48 > 0.0f ? true : false)
			    || gpCamera->unk2C8 != -1) {
#ifdef SMS_NATIVE_PLATFORM
				if (sb_capture_now) sb_boot_capture_set_phase(5);
#endif
				mPerformListSilhouette->perform(0xffffffff, &local_140);
			}
#ifdef SMS_NATIVE_PLATFORM
			if (sb_capture_now) sb_boot_capture_set_phase(6);
#endif
			mPerformListGXPost->perform(0xffffffff, &local_140);
			GXInvalidateTexAll();
#ifdef SMS_NATIVE_PLATFORM
			if (sb_capture_now) sb_boot_capture_end_scene();
#endif
		}
		desiredAppState = changeState();
		unk4C &= ~0x6000;
	}

	gpMSound->unkA8 = bVar2;

#ifdef SMS_NATIVE_PLATFORM
	// OWN the scene-draw flow: the GC perform dispatch never delivers bit 0x8 to the scene,
	// so drive TSmJ3DScn::perform(8) directly (once/frame) — entry()+draw of only the active
	// scene models, with the live camera view + a valid render mode. See native/src/scene_drive.cpp.
	sb_boot_drive_scene();
#endif
	return desiredAppState;
}

static void decideNextStage()
{
	// TODO: inline hell. TFlagT is my mortal enemy
	TGameSequence local_3C;

	int stage = SMS_getShineStage(gpApplication.mCurrArea.getStage());
	switch (stage) {
	case 0:
		local_3C.set(1, 0xff, JDrama::TFlagT<u16>());
		break;
	case 1:
	case 2:
	default:
		local_3C.set(1, 0xff, JDrama::TFlagT<u16>());
		break;
	}
	gpApplication.setNextArea(local_3C);
}

static void decideNextStageOfMiss() { }

static void checkDefeatShadowMarioAll() { }

// fabricated
static inline bool decideSomething()
{
	static u8 stages[] = { 0x6, 0x10, 0x1A, 0x24, 0x2E, 0x38, 0x42 };

	int i = 0;
	do {
		if (!TFlagManager::smInstance->getShineFlag(stages[i]))
			return false;
		++i;
	} while (i < 7);

	return true;
}

static int decideNextScenario(u8 param_1)
{
	if ((int)param_1 != 1)
		return 0;

	if (TFlagManager::smInstance->getBool(0x103AE))
		return 2;

	if (decideSomething())
		return 9;

	if (TFlagManager::smInstance->getBool(0x10389))
		return 8;

	if (TFlagManager::smInstance->getBool(0x10386)
	    && TFlagManager::smInstance->getBool(0x10387)) {
		if (TFlagManager::smInstance->getFlag(0x40000) >= 10)
			return 7;
		else
			return 6;
	}

	if (TFlagManager::smInstance->getBool(0x10385))
		return 5;

	if (TFlagManager::smInstance->getBool(0x10384))
		return 1;

	return 0;
}

int TMarDirector::changeState()
{
	int desiredAppState = TApplication::APP_STATE_DEFAULT;
	u8 nextState        = mState;
	switch (mState) {
	case STATE_UNK0:
		switch (gpApplication.mCurrArea.unk0) {
		case 0xf:
			nextState = STATE_UNK4;
			unk50 |= 1;
			break;

		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x8:
		case 0x9:
		case 0x34:
			nextState = STATE_UNK1;
			unk50 |= 0x6;
			break;

		case 1:
			if (unk4E & 2) {
				nextState = STATE_UNK1;
				unk50 |= 6;
			} else {
				nextState = STATE_UNK2;
			}
			break;

		default:
			if (JKRGetResource("/scene/map/camera/startcamera.bck")) {
				nextState = STATE_UNK1;
				unk50 |= 10;
			} else {
				nextState = STATE_UNK4;
			}
			break;
		}
		break;

	case STATE_UNK1:
		if (unk4E & 4) {
			if (mConsole->unk94->unk2BC == 4) {
				nextState = STATE_UNK3;
				unk4E &= ~0x4;
			}
		} else {
			TGameSequence& curArea = gpApplication.mCurrArea;
			TConsoleStr* str       = mConsole->unk94;
			f32 iVar2              = gpCamera->getRestDemoFrames() / 120.0f;
			if (iVar2 <= str->getWipeCloseTime()
			    || ((curArea.getStage() != 1 || curArea.getScenario() != 1)
			        && (curArea.getStage() != 1 || curArea.getScenario() != 9)
			        && (unk18[0]->mEnabledFrameMeaning & 0x61))) {
				unk4E |= 4;
				mConsole->unk94->startCloseWipe((unk50 & 8) != 0);
				unk50 &= ~0x8;
			}
		}
		break;

	case STATE_UNK3:
		if (gpApplication.mCurrArea.unk0 == 1) {
			if (mConsole->unk94->unk2BC == 6)
				nextState = STATE_UNK2;
		} else if (mConsole->unk94->unk2BC == 6
		           && !gpMarioOriginal->checkStatusType(
		               MARIO_STATUS_FLAG_UNK1000)) {
			nextState = STATE_UNK4;
		}
		break;

	case STATE_UNK2:
		if (!unkE0->unk26
		    && !gpMarioOriginal->checkStatusType(MARIO_STATUS_FLAG_UNK1000))
			nextState = STATE_UNK4;
		break;

	case STATE_UNK4:
		nextState = updateGameMode();
		break;

	case STATE_UNK5:
		switch (unkAC->getNextState()) {
		case 0:
			nextState = STATE_UNK4;
			break;
		case 1:
			unkE4     = 4;
			nextState = STATE_UNK12;
			unkB4     = TApplication::APP_STATE_DONE;
			break;
		case 5:
			decideNextStage();
			unk4C &= ~0x100;
			moveStage();
			unkE4     = 2;
			nextState = STATE_UNK9;
			break;
		}
		break;

	case STATE_UNK10:
		if (unk78->unkC4 && gpApplication.mFader->isFullyFadedIn())
			nextState = STATE_UNK4;
		break;

	case STATE_UNK11: {
		switch (unkAC->mCardSave->getNextState()) {
		case 0:
			if (unk261 == 7) {
				TFlagManager::smInstance->restore();
				TFlagManager::smInstance->setBool(true, 0x30001);
				if (!TFlagManager::smInstance->getFlag(0x40000)) {
					gpApplication.mNextArea.set(0, 0, 0);
				} else {
					gpApplication.mNextArea.set(1, 0xff, 0);
				}
				unk4C &= ~0x100;
				moveStage();
				gpApplication.mFader->setFadeStatus(
				    TSMSFader::FADE_STATUS_FULLY_FADED_OUT);
				desiredAppState = TApplication::APP_STATE_GAMEPLAY;
			} else {
				nextState = STATE_UNK4;
			}
			break;
		case 1:
			if (unk261 == 7) {
				gpApplication.mFader->setFadeStatus(
				    TSMSFader::FADE_STATUS_FULLY_FADED_OUT);
				desiredAppState = TApplication::APP_STATE_DONE;
			} else {
				unkE4     = 4;
				nextState = STATE_UNK12;
				unkB4     = TApplication::APP_STATE_DONE;
			}
			break;
		}
		break;
	}

	case STATE_UNK7:
		if (gpApplication.mFader->isFullyFadedOut()
		    && (MSBgm::getHandle(2) == 0 || unk5C - unk60 >= 1200)) {
			if (TFlagManager::smInstance->getFlag(0x20001) >= 0) {
				TFlagManager::smInstance->setBool(true, 0x30002);
				const TGameSequence& curArea = gpApplication.mCurrArea;
				if (SMS_isExMap() || curArea.getStage() == 0
				    || curArea.getStage() == 60) {
					gpApplication.mNextArea.set(curArea.getStage(), 0, 0);
				} else {
					decideNextStage();
				}
				unk4C &= ~0x100;
				moveStage();
				unkE4 = 0xf;
				gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
				nextState = STATE_UNK12;
			} else {
				gpApplication.mFader->startWipe(0xE, 0.3f, 0.0f);
				gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
				unk261    = 7;
				nextState = STATE_UNK11;
				mConsole->startDisappearStar();
				mConsole->startDisappearCoin();
			}
		}
		break;

	case STATE_UNK9:
	case STATE_UNK12:
		if (gpApplication.mFader->isFullyFadedOut()
		    && gpMSound->checkWaveOnAram(MS_WAVE_DEFAULT))
			desiredAppState = unkB4;
		break;
	}

	if (unk18[0]->isSomethingPushed()
	    && gpCardManager->getLastStatus() != CARD_RESULT_BUSY
	    && (unk4C & 0x4000) && !(unk50 & 0x10)) {
		nextState = STATE_UNK12;
		unk50 |= 0x10;
		unkE4 = 4;
		unkB4 = TApplication::APP_STATE_DONE;
	}

	if (nextState != mState) {
#ifdef SMS_NATIVE_PLATFORM
		if (sb_dir_dbg())
			fprintf(stderr,
			        "[dir-state] %d -> %d  area=(%d,%d) unk4E=0x%x unk50=0x%x "
			        "unkD0=%d unkD1=%d unkE4=%u\n",
			        mState, nextState, gpApplication.mCurrArea.unk0,
			        gpApplication.mCurrArea.unk1, unk4E, unk50, unkD0, unkD1,
			        unkE4);
#endif
		currentStateFinalize(nextState);
		nextStateInitialize(nextState);
		mState = nextState;
	}

	return desiredAppState;
}

void TMarDirector::currentStateFinalize(u8 next_state)
{
	switch (mState) {
	case STATE_UNK0:
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Group 2D")
		    ->unkC.off(0xB);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Guide")->unkC.on(0xB);

		gpApplication.mFader->startWipe(unkE4, 0.4f, 0.0f);
		SMSRumbleMgr->reset();
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		break;

	case STATE_UNK1:
		unk18[0]->mFlags &= ~0x1;
		gpCamera->endDemoCamera();
		mConsole->unk94->startOpenWipe();
		MSMainProc::endStageEntranceDemo(gpApplication.mCurrArea.unk0,
		                                 gpApplication.mCurrArea.unk1);
		break;

	case STATE_UNK4:
		if (unk124 == 0)
			OSStopStopwatch(&unkE8);
		unk18[0]->mFlags &= ~0x2;
		break;

	case STATE_UNK5:
		unk18[0]->mFlags &= ~0x1;
		SMSRumbleMgr->finishPause();
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		break;

	case STATE_UNK10:
		unk18[0]->mFlags &= ~0x1;
		SMSRumbleMgr->finishPause();

		JDrama::TNameRefGen::search<JDrama::TViewObj>("Group 2D")
		    ->unkC.off(0xB);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Guide")->unkC.on(0xB);

		SMSSwitch2DArchive("guide", gArBkConsole);
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		break;

	case STATE_UNK11:
		unk18[0]->mFlags &= ~0x1;
		SMSRumbleMgr->finishPause();
		if (gpApplication.mCurrArea.unk0 == 1)
			THPPlayerPlay();
		switch (unk261) {
		case 3:
			mConsole->startAppearBalloon(0xE0048, true);
			break;

		case 4:
			mConsole->startAppearBalloon(0xE0049, true);
			break;
		}
		break;
	}
}

void TMarDirector::setMario()
{
	bool cVar4 = false;
	if (TFlagManager::getInstance()->getBool(0x30006)) {
		TFlagManager::getInstance()->setBool(false, 0x30006);
		cVar4 = true;
	}

	u8 uVar10 = unkD0;

	TMarioPositionObj* marioSetPosition
	    = JDrama::TNameRefGen::search<TMarioPositionObj>("マリオセット位置");
#ifdef SMS_NATIVE_PLATFORM
	if (sb_dir_dbg())
		fprintf(stderr,
		        "[dir-setMario] unkD0=%d unkD1=%d marioSetPos=%p posCount=%d "
		        "marioPos=(%.1f,%.1f,%.1f)\n",
		        unkD0, unkD1, (void*)marioSetPosition,
		        marioSetPosition ? marioSetPosition->unkD0 : -1,
		        gpMarioOriginal->mPosition.x, gpMarioOriginal->mPosition.y,
		        gpMarioOriginal->mPosition.z);
#endif
	if (!marioSetPosition || marioSetPosition->unkD0 == 0)
		uVar10 = 0;

	switch (unkD1) {
	case 1: {
		f32 fVar1 = 0.0f;

		const JGeometry::TVec3<f32>* pos = nullptr;

		if (uVar10) {
			fVar1 = marioSetPosition->getUnk70(uVar10 - 1).y;
			pos   = &marioSetPosition->getUnk10(uVar10 - 1);
		}
		gpMarioOriginal->rollingStart(pos, fVar1);
	} break;

	case 2: {
		int iVar9 = 0;
		switch (SMS_getShineStage(gpApplication.mPrevArea.getStage())) {
		case 5:
		case 6:
		case 7:
			iVar9 = 1;
			break;
		case 8:
			iVar9 = 2;
			break;
		}
		f32 fVar1 = 0.0f;

		const JGeometry::TVec3<f32>* pos = nullptr;

		if (uVar10) {
			fVar1 = marioSetPosition->getUnk70(uVar10 - 1).y;
			pos   = &marioSetPosition->getUnk10(uVar10 - 1);
		}
		gpMarioOriginal->returnStart(pos, fVar1, cVar4, iVar9);
	} break;

	case 4:
		gpMarioOriginal->toroccoStart();
		break;

	case 3:
		const JGeometry::TVec3<f32>* pos = nullptr;
		if (uVar10)
			pos = &marioSetPosition->getUnk10(uVar10 - 1);
		gpMarioOriginal->waitingStart(pos, 0.0f);
		break;
	}

	switch (gpApplication.mCurrArea.getStage()) {
	case 0x3C:
		gpMarioOriginal->mWaterGun->changeNozzle(TWaterGun::Rocket, true);
		break;

		// TODO: crazy cases
	case 0:
	case 7:
		gpMarioOriginal->mWaterGun->changeNozzle(
		    (TWaterGun::TNozzleType)TFlagManager::getInstance()->getFlag(
		        0x40004),
		    true);
		gpMarioOriginal->mWaterGun->changeNozzle(TWaterGun::Spray, true);
		break;
	}

	u32 uVar6 = SMS_getShineIDofExStage(gpApplication.mCurrArea.getStage());
	if (uVar6 != 0xff && TFlagManager::getInstance()->getShineFlag(uVar6) == 0)
		gpMarioOriginal->offFlag(MARIO_FLAG_HAS_FLUDD);
}

void TMarDirector::nextStateInitialize(u8 next_state)
{
	TGameSequence& currSeq = gpApplication.mCurrArea;

	switch (next_state) {
	case 1: {
		const char* pcVar8 = "startcamera";
		unk18[0]->onFlag(0x1);
		unk68 = 0;
		if (gpApplication.mCurrArea.unk0 == 1 && (unk4E & 2)) {
			if (gpApplication.mCurrArea.unk1 == 8) {
				switch (TFlagManager::smInstance->getFlag(0x60003)) {
				case 0:
					if (TFlagManager::smInstance->getFlag(0x40000) >= 0x14)
						pcVar8 = "mareopen_startcamera";
					break;
				case 1:
					pcVar8 = "yoshi_startcamera";
					break;
				case 2:
					pcVar8 = "turbo_startcamera";
					break;
				case 3:
					pcVar8 = "rocket_startcamera";
					break;
				}
			} else {
				if (TFlagManager::smInstance->getBool(0x50001)) {
					pcVar8 = "sinkricco";
				} else {
					if (TFlagManager::smInstance->getBool(0x50002))
						pcVar8 = "sinkmamma";
				}
			}
		}
#ifdef SMS_NATIVE_PLATFORM
		if (sb_dir_dbg())
			fprintf(stderr,
			        "[dir-demo] startDemoCamera('%s')  startcamera.bck=%p\n",
			        pcVar8,
			        JKRGetResource("/scene/map/camera/startcamera.bck"));
#endif
		gpCamera->startDemoCamera(pcVar8, nullptr, -1, 0.0f, true);
		if (unk50 & 4) {
			mConsole->unk94->startAppearScenario();
			unk50 &= ~0x4;
		}
		MSMainProc::startStageEntranceDemo(currSeq.unk0, currSeq.unk1);
		break;
	}

	case 3:
		unk68 = 0;
		if (!(unk50 & 1)) {
			MSMainProc::startStageBGM(currSeq.unk0, currSeq.unk1);
			setMario();
			unk50 |= 1;
		}
		break;

	case 2:
		if (!(unk50 & 1)) {
			MSMainProc::startStageBGM(currSeq.unk0, currSeq.unk1);
			setMario();
			unk50 |= 1;
		}
		if (mMap != 0xf)
			mConsole->unkC.off(0xB);
		break;

	case 4:
		if (mState <= STATE_UNK3 && mMap != 0xf)
			mConsole->unkC.off(0xB);
		if (unk50 & 2) {
			mConsole->unk94->startAppearGo();
			unk50 &= ~0x2;
		}
		if (!(unk50 & 1)) {
			MSMainProc::startStageBGM(currSeq.unk0, currSeq.unk1);
			setMario();
			unk50 |= 1;
		}
		if (!unk124)
			OSStartStopwatch(&unkE8);
		unk18[0]->onFlag(0x2);
		break;

	case 12:
		if (currSeq.unk0 == 1)
			THPPlayerStop();
	// !!!fallthrough!!!
	case 9: {
		gpApplication.mFader->startWipe(unkE4, 0.4f, 0.0f);
		if (unkE4 == 8)
			SMSGetMSound()->startSoundSystemSE(MSD_SE_MA_INTO_DOKAN, 0, nullptr,
			                                   0);
		MSound* sound = gpMSound;
		sound->fadeOutAllSound(SMSGetVSyncTimesPerSec() * 0.4f);
		SMSRumbleMgr->reset();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		break;
	}

	case 5:
		if (currSeq.unk0 == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		unkAC->setDrawStart();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		unk18[0]->onFlag(0x1);
		break;

	case 10:
		if (currSeq.unk0 == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		unk18[0]->onFlag(0x1);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Group 2D")->unkC.on(0xB);
		JDrama::TNameRefGen::search<JDrama::TViewObj>("Guide")->unkC.off(0xB);
		if (gpMSound->gateCheck(MSD_SE_SY_WIPE_IN))
			SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_WIPE_IN, 0, nullptr,
			                                   0);
		gpApplication.mFader->startWipe(6, 1.0f, 0.0f);
		unk78->setup(nullptr);
		unk78->startMoveCursor();
		break;

	case 11:
		if (currSeq.unk0 == 1)
			THPPlayerPause();
		SMSRumbleMgr->startPause();
		unkAC->mCardSave->init(unk261);
		for (int i = 0; i < 4; ++i)
			JUTGamePad::CRumble::stopMotor(unk18[i]->mPortNum);
		unk18[0]->onFlag(0x1);
		break;

	case 7:
		gpMarDirector->mConsole->unk94->startAppearMiss();
		TFlagManager::smInstance->decFlag(0x20001, 1);
		unk60 = unk5C;
		gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
		if (TFlagManager::smInstance->getFlag(0x20001) >= 0) {
			MSBgm::startBGM(MSD_BGM_BOSS);
			if (unk4E & 8)
				gpApplication.mFader->startWipe(2, 0.0f, 2.0f);
			else
				gpApplication.mFader->startWipe(10, 0.0f, 2.2f);
		} else {
			MSBgm::startBGM(MSD_BGM_BOSSHANA_2ND3RD);
			gpApplication.mFader->startWipe(0xD, 0.0f, 2.0f);
		}
		break;
	}
}

u8 TMarDirector::updateGameMode()
{
	u8 r29 = mState;

	switch (unk124) {
	case 0:
#ifdef SMS_NATIVE_PLATFORM
		if (sb_dir_dbg()) {
			static int s_go = -1;
			int go = SMS_CheckMarioFlag(MARIO_FLAG_GAME_OVER) ? 1 : 0;
			if (go != s_go) {
				fprintf(stderr, "[gamemode] unk124=0 unk4C=0x%x GAME_OVER=%d mState=%d\n",
				        unk4C, go, (int)mState);
				s_go = go;
			}
		}
#endif
		if (!(unk4C & 0x1FFF)) {
			if (SMS_CheckMarioFlag(MARIO_FLAG_GAME_OVER)) {
				unk4C |= 0x20;
				break;
			}

			if (mMap != 15) {
				if (unk18[0]->mButton.mTrigger & 0x10) {
					r29 = STATE_UNK10;
					break;
				}

				if (unk18[0]->mEnabledFrameMeaning & 0x1) {
					if (gpMarioOriginal->checkActionThing3()) {
						r29 = STATE_UNK5;
						break;
					}

					SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_NOT_COLLECT, 0,
					                                   nullptr, 0);
				}
			}
		} else {
			if (unk4C & 0x20) {
				unk4C &= ~0x20;
				r29 = STATE_UNK7;
				TFlagManager::getInstance()->setFlag(0x40002, 0);
				break;
			}

			if (unk4C & 0x1) {
				unk4C &= ~0x1;
				unk126 = 3;

				TGCConsole2* console = gpMarDirector->mConsole;
				console->unk94->startAppearShineGet();
				console->unk34[19] = 1;
				MSBgm::startBGM(MSD_BGM_CHUBOSS);
				TFlagManager::getInstance()->setBool(true, 0x30006);
				TFlagManager::getInstance()->setShineFlag(unk25C->getEventId());
				f32 fVar3 = unkDC->mRate;
				unkDC->registFadeout(fVar3 * 1.0f, fVar3 * 5.3333333f);
				unk4C |= 0x8202;
				unk261 = 6;
				decideNextStage();
				break;
			}

			if (unk4C & 0x40) {
				unk126 = 3;
				break;
			}

			if (unk4C & 0x200) {
				unk4C &= ~0x200;
				r29 = STATE_UNK11;
				break;
			}

			if (unk4C & 0x8) {
				unk4C &= ~0x8;
				unk4C |= 0x2;
				unk126 = 3;
				if (gpApplication.mNextArea.getStage() == 5) {
					fireStartDemoCamera("hodai_dpt_pinna1", nullptr, -1, 0.0f,
					                    false, nullptr, 0, nullptr, 0);
					if (unk254 != nullptr)
						unk254->startDemo();
					break;
				}

				if (gpApplication.mNextArea.getStage() == 6) {
					fireStartDemoCamera("camera_sirena_gate_in", nullptr, -1,
					                    0.0f, false, nullptr, 0, nullptr, 0);
					break;
				}

				if (gpApplication.mNextArea.getStage() == 8) {
					fireStartDemoCamera("camera_monte_gate_in", nullptr, -1,
					                    0.0f, false, nullptr, 0, nullptr, 0);
					break;
				}

				break;
			}

			if (unk4C & 0x4) {
				unk4C &= ~0x4;
				unk4C |= 0x2;
				unk126 = 3;
				fireStartDemoCamera(nullptr, nullptr, -1, 0.0f, false, nullptr,
				                    0, mTransitionActor, 0);
				break;
			}

			if (unk4C & 0x2) {
				moveStage();
				r29 = STATE_UNK9;
				break;
			}
		}
		break;

	case 2:
		if (unk4C & 0x40) {
			unk126 = 4;
		} else {
			if (unkB0->unk248 == 0)
				unk126 = 0;
		}
		break;

	case 4: {
		bool bVar5  = false;
		bool uVar15 = 0;
		if (unk4C & 0x80) {
			uVar15 = 1;
			bVar5  = true;
			unk4C &= ~0x80;
		} else {
			if (!gpCamera->getRestDemoFrames()) {
				if (!MSBgm::getHandle(2) || unk5C - unk60 >= 1200) {
					bVar5  = true;
					uVar15 = unk12C[unk24D].unk10;
				}
			}
		}

		if (bVar5) {
			u32 prev = unk24D++;
			unk24D &= 0x7;
			TDemoInfo* info = &unk12C[prev];
			if (unk24D != unk24C) {
				gpCamera->endDemoCamera();
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 1);

				gpCamera->startDemoCamera(info->unk0, info->unk4, info->unk8,
				                          info->unkC, info->unk10);
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 0);
			} else {
				unk4C &= ~0x40;
				unk126 = unk124 == 4 ? 2 : 0;
				if (uVar15 != 0)
					gpCamera->endDemoCamera();
				if (unk12C[prev].unk14 != nullptr)
					(*unk12C[prev].unk14)(unk12C[prev].unk18, 1);
			}
		}
	} break;
	}

	if (unk24D == unk24C)
		unk4C &= ~0x80;

	unk125 = unk124;

	if (unk124 != unk126) {
		switch (unk124) {
		case 2:
			if (unk126 == 0) {
				unkA0 = 0;
				unkA4 = 0;
				unk18[0]->mFlags &= ~0x2;
				OSStartStopwatch(&unkE8);
			}
			break;

		case 3:
		case 4:
			if (unk124 == 4)
				MSMainProc::fromTalkingCameraDemo(unk124 == 4);
			else
				MSMainProc::fromInnerCameraDemo();
			unk18[0]->mFlags &= ~0x80;
			OSStartStopwatch(&unkE8);
			break;
		}

		switch (unk126) {
		case 0:
			break;

		case 1:
			unkA0->onLiveFlag(LIVE_FLAG_UNK40000);
			unkA0->unkC.off(0x3);
			unk18[0]->mFlags |= 0x8;
			OSStopStopwatch(&unkE8);
			break;

		case 2:
			if (unk124 == 1)
				unkB0->openTalkWindow(unkA0);
			break;

		case 3:
		case 4:
			if (unk124 == 4)
				MSMainProc::toTalkingCameraDemo();
			else
				MSMainProc::toInnerCameraDemo();
			unk18[0]->mFlags |= 0x10;
			if (unk12C[unk24D].unk20.mValue == 1) {
				gpCamera->startGateDemoCamera(unk12C[unk24D].unk1C);
			} else {
				TDemoInfo* info = &unk12C[unk24D];
				gpCamera->startDemoCamera(info->unk0, info->unk4, info->unk8,
				                          info->unkC, info->unk10);
				if (info->unk14 != nullptr)
					(*info->unk14)(info->unk18, 0);
			}
			OSStopStopwatch(&unkE8);
			unk60 = unk5C;
			break;
		}

		unk124 = unk126;
	}

	if (unk128 & 0x1) {
		unk128 &= ~0x1;
		unk128 |= 0x2;
	} else {
		unk128 &= ~0x2;
	}

	return r29;
}

void TMarDirector::moveStage()
{
#ifdef SMS_NATIVE_PLATFORM
	if (sb_dir_dbg())
		fprintf(stderr, "[movestage] FIRED next=stage%d/scn%d curr=stage%d\n",
		        (int)gpApplication.mNextArea.getStage(),
		        (int)gpApplication.mNextArea.getScenario(),
		        (int)gpApplication.mCurrArea.getStage());
#endif
	unkB4 = TApplication::APP_STATE_GAMEPLAY;
	unkE4 = 15;
	gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));

	u8 sVar4 = SMS_getShineStage(gpApplication.mNextArea.getStage());
	u8 sVar5 = SMS_getShineStage(gpApplication.mCurrArea.getStage());
	if (sVar4 != sVar5)
		TFlagManager::smInstance->setFlag(0x40002, 0);

	TGameSequence& nextArea = gpApplication.mNextArea;

	if (nextArea.unk1 == 0xff)
		switch (nextArea.unk0) {
		case 1:
			unkE4         = 2;
			nextArea.unk1 = decideNextScenario(nextArea.getStage());
			TFlagManager::smInstance->setFlag(0x40003, 0);
			break;

		case 13: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 0:
				thing = 0;
				break;
			case 2:
				thing = 1;
				break;
			case 4:
				thing = 2;
				break;
			case 5:
				thing = 3;
				break;
			case 6:
				thing = 4;
				break;
			case 7:
				thing = 5;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 0x3A: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 0:
				thing = 1;
				break;
			case 7:
				thing = 0;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 7: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 1:
				thing = 0;
				break;
			case 2:
				thing = 1;
				break;
			case 3:
			case 4:
				thing = 2;
				break;
			case 6:
				thing = 3;
				break;
			case 7:
				thing = 4;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 14: {
			unkE4     = 2;
			u32 thing = 0;
			switch (TFlagManager::smInstance->getFlag(0x40003)) {
			case 3:
				thing = 0;
				break;
			case 4:
				thing = 1;
				break;
			}
			nextArea.unk1 = thing;
			break;
		}

		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 8:
			unkE4 = 2;
			unkB4 = TApplication::APP_STATE_BOOT;
			break;

		case 9:
			gpApplication.mFader->setColor(
			    JUtility::TColor(0xD2, 0xD2, 0xD2, 0xFF));
			unkB4 = TApplication::APP_STATE_TITLE;
			break;

		case 0x34:
			unkE4         = 8;
			nextArea.unk1 = 0;
			TFlagManager::smInstance->setFlag(0x40003, 0);
			break;

		case 0:
			nextArea.unk1 = 0;
			TFlagManager::smInstance->setFlag(0x40003, 0);
			break;

		default:
			unkE4         = 2;
			nextArea.unk1 = 0;
			break;
		}

	if (nextArea.unk1 != 0xff) {
		if (unk4C & 0x100) {
			unkE4 = 15;
			gpApplication.mFader->setColor(JUtility::TColor(0, 0, 0, 0xff));
			unkB4 = TApplication::APP_STATE_MOVIE;
		} else {
			unkB4 = TApplication::APP_STATE_GAMEPLAY;
		}
	}

	if (gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD)) {
		u32 r5 = 0;
		if ((int)gpMarioOriginal->mWaterGun->mSecondNozzle == 3)
			r5 = 4;
		TFlagManager::smInstance->setFlag(0x40004, r5);
	}
}

JStage::TObject* TMarDirector::JSGFindObject(const char* param_1,
                                             JStage::TEObject param_2) const
{
	if (strcmp("cam_int1", param_1) == 0) {
		JDrama::TCamera* cam
		    = (JDrama::TCamera*)const_cast<TMarDirector*>(this)->search(
		        "camera 1");
		return cam;
	}

	if (strcmp("mario", param_1) == 0) {
		JDrama::TActor* mario
		    = (JDrama::TActor*)const_cast<TMarDirector*>(this)->search(
		        "マリオ");
		return mario;
	}

	return JDrama::TDirector::JSGFindObject(param_1, param_2);
}
