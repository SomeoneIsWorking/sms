#ifndef GC2D_GUIDE_HPP
#define GC2D_GUIDE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

class JKRMemArchive;
class J2DPicture;
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
	void checkPoint(int, int);
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
	/* 0x165 */ char unk165[0x6f8 - 0x165];
};

#endif
