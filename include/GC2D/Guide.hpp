#ifndef GC2D_GUIDE_HPP
#define GC2D_GUIDE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

class JKRMemArchive;
class J2DPicture;
class J2DPane;
class TBoundPane;
class TExPane;
class TMarioGamePad;

class TGuide : public JDrama::TViewObj {
public:
	TGuide(const char* name = "<Guide>");
	void load(JSUMemoryInputStream& stream);
	void resetObjects();
	void resetScore();
	void setup(JKRMemArchive*);
	void setup2(JKRMemArchive*);
	void startMoveCursor();
	void startMoveCursor2();
	void linkSelect();
	void changePattern(J2DPicture*, short, u32);
	void mirrorPattern(J2DPicture*, short, u32);
	void rotatePattern(J2DPicture*, short, u32, short);
	void shinePattern(TBoundPane*, short, u32);
	void mmarkPattern(TExPane*, short, u32);
	void searchNearPoint(short*, short*, short, short);
	int checkPoint(int, int); // returns the hit index, or -1 — US 0x8017a6bc leaves it in r3
	void changeBotStatus(int);
	void placeMario();
	void appearGuidePane(int);
	void disappearGuidePane(int);
	void perform(u32, JDrama::TGraphics*);

public:
	// unk10 and unk164 were inside the padding arrays; broken out for the native port because
	// startMoveCursor writes both (US 0x8017b450: `stw r0,0x10(r3)` with 9, `stb r0,0x164(r3)`
	// with 0). Adding named fields is safe here — every access in this tree is BY NAME, so the
	// host offset does not have to match the guest's; only raw-offset access would care.
	/* 0x10 */ u32 unk10;
	/* 0x14 */ char unk14[0xC0 - 0x14];
	/* 0xC0 */ TMarioGamePad* unkC0;
	/* 0xC4 */ u8 unkC4;
	/* 0xC5 */ u8 unkC5;
	/* 0xC6 */ char unkC6[0x164 - 0xC6];
	/* 0x164 */ u8 unk164;
	/* 0x165 */ char unk165[0x168 - 0x165];
	// The two pane tables checkPoint hit-tests. Sizes are the loop bounds at US 0x8017a734
	// (`cmpwi r26, 0xe`) and 0x8017a7a0 (`cmpwi r26, 0xa`); the element type is J2DPane*
	// because the code reads a JUTRect at +0x14 and a bool at +0xC of each entry, which is
	// J2DPane's mBounds and mVisible exactly.
	/* 0x168 */ J2DPane* unk168[14];
	/* 0x1A0 */ char unk1A0[0x44c - 0x1A0];
	/* 0x44C */ J2DPane* unk44c[10];
	/* 0x474 */ char unk474[0x6f8 - 0x474];
};

#endif
