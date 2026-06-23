#ifndef GC2D_SELECT_GRAD_HPP
#define GC2D_SELECT_GRAD_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

// Background gradient view-obj for the file-select screen.
// Reconstructed from the DOL (GMSE01): ctor @0x80175a4c, setStageColor @0x8017591c,
// perform @0x80175560. The class is un-decompiled in reference/sms (SelectMenu.cpp is
// empty); this is a faithful re-implementation. Layout/size verified against the DOL
// (operator new(0x24) in TSelectDir::rsetup).
class TSelectGrad : public JDrama::TViewObj {
public:
	TSelectGrad(const char* name = "<TSelectGrad>");

	void setStageColor(u8 stage);
	virtual void perform(u32, JDrama::TGraphics*);

public:
	/* 0x10 */ int mColorIdx[3];               // phase indices (0..5) for the 3 colour channels
	/* 0x1C */ u8  mColorA[4];                  // top-left corner RGBA (animated)
	/* 0x20 */ u8  mColorB[4];                  // bottom-right corner RGBA (animated)
};

#endif
