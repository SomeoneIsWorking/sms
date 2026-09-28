#ifndef GC2D_SELECT_MENU_HPP
#define GC2D_SELECT_MENU_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JUtility/JUTRect.hpp>

#include <JSystem/JUtility/JUTTexture.hpp>

class J2DPane;
class J2DSetScreen;
class J2DTextBox;
class J2DPicture;
class JKRArchive;
class TExPane;
class TBoundPane;
class TSelectShineManager;
class TSelectDir;
class TMarioGamePad;

// File/scenario-select menu view-obj (the file-slot windows). Reconstructed from the
// GMSE01 DOL; the GC2D/SelectMenu.cpp TU is un-decompiled in the community decomp (the
// select screen only ever ran under Dolphin's JIT in the hybrid build). Anchors:
//   ctor          @0x801753d0  (operator new(0x170) in TSelectDir::rsetup)
//   setup         @0x8017449c  (loads scenario_select_1.blo + per-file save data)
//   perform       @0x80172c90  (slot 8 of the TViewObj vtable: calc + J2DScreen draw)
//   startOpenWindow@0x80172990, getPrevIndex @0x80172bdc, getNextIndex @0x80172c34
//
// PORT STATUS (incremental, milestone 2): this builds the menu's J2DScreen and draws it
// (the file-slot windows render at their .blo default layout). The full setup also
// populates per-file save data (shine/coin digit pictures, scenario marks, window
// visibility from TFlagManager) and the window-open animation / input navigation in
// perform's calc path — those are reconstructed in follow-up steps and are marked TODO.
// Nothing here is a shortcut around the original: the omitted behaviour is not-yet-ported.
//
// NOTE on layout: like the sister TSelectGrad, this is a PC-native object (host operator
// new, host sizeof) — it is never read by guest/recomp code in the sms-boot build — so the
// members are declared by meaning and laid out by the host compiler. The DOL's 0x170 size
// and its field offsets (in the anchors above) are the RE source, not a layout contract.
class TSelectMenu : public JDrama::TViewObj {
public:
	TSelectMenu(const char* name = "<TSelectMenu>");

	void setup(u8 stage, JKRArchive* archive, TSelectShineManager* shineMgr,
	           TSelectDir* dir);

	virtual void perform(u32, JDrama::TGraphics*);

	// The perform state machine's phases, DOL 0x10 — the same ten states the
	// guest's SelectMenuState enum names and that the draw path's
	// `mState >= 0 && mState < 10` gate admits.
	enum SelectMenuState {
		// Close the menu.
		CLOSE_MENU = 0,

		// Animate letterbox bars from top and bottom of screen.
		LETTERBOX_ANIMATION = 1,

		// Slide the stage banner in from the right of the screen.
		STAGE_BANNER_SLIDE = 2,

		// Squash the stage banner when it hits the left side.
		STAGE_BANNER_SQUASH = 3,

		// Stretch the banner after squash.
		STAGE_BANNER_STRETCH = 4,

		// Fade the menu items in.
		APPEAR_MENU = 5,

		// Await and process player input.
		MENU_INPUT_LOOP = 6,

		// Animate the menu elements.
		MENU_ANIM_LOOP = 7,

		// Fade the menu elements out.
		DISAPPEAR_MENU = 8,

		// Wait for a moment before closing the menu.
		WAIT_BEFORE_CLOSE = 9,
	};

public:
	int                  mState;        // perform state machine (DOL 0x10; 0 = idle)
	int                  mColorIdx[3];  // window-colour indices (DOL 0x14, set in setup)
	J2DSetScreen*        mScreen;       // DOL 0x20 — scenario_select_1.blo
	TExPane*             mLetterBoxTop;    // DOL 0x24 — letterbox bar, top
	TExPane*             mLetterBoxBottom; // DOL 0x28 — letterbox bar, bottom
	J2DTextBox*          mStageName;       // DOL 0x2C — stage-name text box
	TExPane*             mStageBannerPane; // DOL 0x30 — stage banner image
	TBoundPane*          mStageBannerText;   // DOL 0x34 — banner text
	TBoundPane*          mStageBannerShadow; // DOL 0x38 — banner shadow
	TExPane*             mScenarioPane1;   // DOL 0x40 — scenario (shine) pane, first
	J2DTextBox*          mScenarioText1;   // DOL 0x44
	J2DPicture*          mScenarioImg1;    // DOL 0x48 — scenario banner image
	J2DPicture*          mScenarioShadow1; // DOL 0x4C — its shadow
	bool                 mSelectNext;      // DOL 0x54 — next rather than previous slot
	TExPane*             mScenarioPane2;   // DOL 0x68 — the second pane, animating over
	J2DTextBox*          mScenarioText2;   // DOL 0x6C
	J2DPicture*          mScenarioImg2;    // DOL 0x70
	J2DPicture*          mScenarioShadow2; // DOL 0x74
	s16                  mScenarioPaneDist;// DOL 0x7C — slide distance, half height
	JUTTexture*          mScenarioTex[8];  // DOL 0x80 — per-scenario banner textures
	J2DPane*             mShineList;       // DOL 0xA0 — the shine selection pane
	J2DPane*             mScorePane;       // DOL 0xA4 — collected-coin display
	J2DPicture*          mShineMarks[8];   // DOL 0xD8 — per-slot shine icon
	bool                 mMarkPulseDir;    // DOL 0xD8 — shine icon pulse direction
	TMarioGamePad*       mGamePad;      // DOL 0x100 — set by rsetup (menu->unk100)
	J2DPane*             mArrowL;         // DOL 0x104 — left arrow
	J2DPane*             mArrowR;         // DOL 0x108 — right arrow
	bool                 mArrowAnimDir;   // DOL 0x10C — arrow slide direction
	u8                   mArrowAnimPos;   // DOL 0x10D
	JUTRect              mArrowLBounds;   // DOL 0x110
	JUTRect              mArrowRBounds;   // DOL 0x120
	TSelectShineManager* mShineMgr;     // DOL 0x130
	TSelectDir*          mDir;          // DOL 0x134
	u8                   mLetterboxAnimFrame;  // DOL 0x138
	u8                   mSelectShineAnimFrame; // DOL 0x139
	u8                   mStage;        // DOL 0x13A
	u8                   mCursor;       // DOL 0x13B — selected slot (0xff = none)
	u8                   mNumSlots;     // DOL 0x13C
	JUtility::TColor     mSelectedMarkCol; // DOL 0x140 — selected shine icon colour
	JUtility::TColor     mMarkCol;         // DOL 0x144 — unselected shine icon colour
	u8                   mSelectedMarkAlpha; // DOL 0x148
	u8                   mMarkAlpha;         // DOL 0x149
	u8                   mDisabled;     // DOL 0x14A — 1 = menu suppressed (title-ish)
	f32                  mFrameScale;   // DOL 0x14C — 1.0 / SMSGetAnmFrameRate()
	u8                   mEpisodeState[8]; // DOL 0x150 — per-slot mark state (0=locked,2=open,3=cleared)
	void*                mScenarioBmg2; // DOL 0x15C — second scenario's BMG
	s16                  mWaitBeforeCloseTimer; // DOL 0x16C

	// The slot navigation the state machine's input phase calls. DOL anchors:
	// startOpenWindow @0x80172990, getPrevIndex @0x80172bdc, getNextIndex @0x80172c34.
	s8 getNextIndex();
	s8 getPrevIndex();
	void startOpenWindow();
	void startCloseWindow();
};

#endif
