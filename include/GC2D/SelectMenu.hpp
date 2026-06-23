#ifndef GC2D_SELECT_MENU_HPP
#define GC2D_SELECT_MENU_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

class J2DSetScreen;
class JKRArchive;
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

public:
	int                  mState;        // perform state machine (DOL 0x10; 0 = idle)
	int                  mColorIdx[3];  // window-colour indices (DOL 0x14, set in setup)
	J2DSetScreen*        mScreen;       // DOL 0x20 — scenario_select_1.blo
	TMarioGamePad*       mGamePad;      // DOL 0x100 — set by rsetup (menu->unk100)
	TSelectShineManager* mShineMgr;     // DOL 0x130
	TSelectDir*          mDir;          // DOL 0x134
	u8                   mStage;        // DOL 0x13A
	u8                   mCursor;       // DOL 0x13B — selected slot (0xff = none)
	u8                   mNumSlots;     // DOL 0x13C
	u8                   mDisabled;     // DOL 0x14A — 1 = menu suppressed (title-ish)
	f32                  mFrameScale;   // DOL 0x14C — 1.0 / SMSGetAnmFrameRate()
};

#endif
