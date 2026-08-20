#ifndef GC2D_GUIDE_HPP
#define GC2D_GUIDE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JUtility/JUTRect.hpp>

class JKRMemArchive;
class J2DPicture;
class J2DPane;
class J2DSetScreen;
class JUTTexture;
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
	// The names preserve the US GMSE01 layout until each field has a proven semantic name.  Native
	// code accesses these by member, not raw host offset; the comments are the retail offsets.
	struct GuideRow {
		u8 unk0;
		u8 unk1;
		u8 unk2;
		u8 pad3;
		u16 unk4;
		u8 unk6;
		u8 unk7;
	};

	/* 0x10 */ u32 unk10;
	/* 0x14 */ GuideRow unk14[13];
	/* 0x7C */ char unk7C[0xBC - 0x7C];
	/* 0xBC */ J2DSetScreen* unkBC;
	/* 0xC0 */ TMarioGamePad* unkC0;
	/* 0xC4 */ u8 unkC4;
	/* 0xC5 */ u8 unkC5;
	/* 0xC6 */ char unkC6[2];
	/* 0xC8 */ JUTTexture* unkC8[10];
	/* 0xF0 */ char unkF0[4];
	/* 0xF4 */ J2DPane* unkF4;
	/* 0xF8 */ J2DPane* unkF8[2];
	/* 0x100 */ J2DPane* unk100;
	/* 0x104 */ J2DPane* unk104[2];
	/* 0x10C */ J2DPane* unk10C[3];
	/* 0x118 */ J2DPane* unk118;
	/* 0x11C */ J2DPane* unk11C[2];
	/* 0x124 */ J2DPane* unk124;
	/* 0x128 */ TExPane* unk128[2];
	/* 0x130 */ J2DPane* unk130;
	/* 0x134 */ J2DPane* unk134[10];
	/* 0x15C */ char unk15C[8];
	/* 0x164 */ u8 unk164;
	/* 0x165 */ char unk165[3];
	// The two pane tables checkPoint hit-tests. Sizes are the loop bounds at US 0x8017a734
	// (`cmpwi r26, 0xe`) and 0x8017a7a0 (`cmpwi r26, 0xa`); the element type is J2DPane*
	// because the code reads a JUTRect at +0x14 and a bool at +0xC of each entry, which is
	// J2DPane's mBounds and mVisible exactly.
	/* 0x168 */ J2DPane* unk168[14]; // [13] is the distinct 0x19C pane
	/* 0x1A0 */ TExPane* unk1A0;
	/* 0x1A4 */ char unk1A4[0x1C0 - 0x1A4];
	/* 0x1C0 */ TExPane* unk1C0[13];
	/* 0x1F4 */ TExPane* unk1F4;
	/* 0x1F8 */ char unk1F8[0x218 - 0x1F8];
	/* 0x218 */ JUTRect unk218[13];
	/* 0x2E8 */ JUTRect unk2E8;
	/* 0x2F8 */ char unk2F8[0x378 - 0x2F8];
	/* 0x378 */ TExPane* unk378[13];
	/* 0x3AC */ TExPane* unk3AC;
	/* 0x3B0 */ char unk3B0[0x3D0 - 0x3B0];
	/* 0x3D0 */ J2DPane* unk3D0[10];
	/* 0x3F8 */ char unk3F8[0x424 - 0x3F8];
	/* 0x424 */ TExPane* unk424;
	/* 0x428 */ TExPane* unk428;
	/* 0x42C */ s16 unk42C;
	/* 0x42E */ char unk42E[2];
	/* 0x430 */ J2DPane* unk430;
	/* 0x434 */ JUTRect unk434;
	/* 0x444 */ TBoundPane* unk444;
	/* 0x448 */ J2DPane* unk448;
	/* 0x44C */ J2DPane* unk44c[10];
	/* 0x474 */ void* unk474;
	/* 0x478 */ TExPane* unk478;
	/* 0x47C */ u8 unk47C;
};

#endif
