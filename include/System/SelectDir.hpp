#ifndef SYSTEM_SELECT_DIR_HPP
#define SYSTEM_SELECT_DIR_HPP

#include <JSystem/JDrama/JDRDirector.hpp>

namespace JDrama {
class TDisplay;
class TScreen;
template <class T, class U> class TViewObjPtrListT;
class TViewObj;
};

class TMarioGamePad;
class JKRMemArchive;
class JPAEmitterManager;
class TSelectMenu;
class TSelectGrad;
class TSelectShineManager;

// File-select director. Reconstructed from the GMSE01 DOL (the GC2D/SelectDir.cpp TU is
// un-decompiled in the community decomp). Anchors: setup 0x80177400, setupThreadFunc
// 0x801773e0, rsetup 0x801761b0, direct 0x80175ec4, changeOrder 0x80176188.
//
// Layout (verified vs rsetup's member stores): inherits TDirector (unk10=root view objs,
// unk14=TDStageGroup) and adds members from 0x18.
class TSelectDir : public JDrama::TDirector {
public:
	TSelectDir();

	virtual int direct();

	void setup(JDrama::TDisplay*, TMarioGamePad*, unsigned char);
	static void* setupThreadFunc(void*);
	int  rsetup();
	void changeOrder();

public:
	/* 0x18 */ TMarioGamePad* mGamePad;
	/* 0x1C */ JDrama::TDisplay* mDisplay;
	/* 0x20 */ TSelectMenu* mSelectMenu;
	/* 0x24 */ TSelectGrad* mSelectGrad;
	/* 0x28 */ TSelectShineManager* mSelectShineMgr;
	/* 0x2C */ JKRMemArchive* mArchive;
	/* 0x30 */ JPAEmitterManager* mEmitterMgr0;
	/* 0x34 */ JPAEmitterManager* mEmitterMgr1;
	/* 0x38 */ u8 mSetupDone;
	/* 0x39 */ u8 pad39[7];
	/* 0x40 */ u8 mStage;
	/* 0x41 */ u8 pad41[3];
	/* 0x44 */ JDrama::TScreen* mScreen2D;
	/* 0x48 */ JDrama::TScreen* mScreenGrad;
	/* 0x4C */ u8 mFlag4C;
	/* 0x4D */ u8 pad4D[3];
};

#endif
