#ifndef MARIO_UTIL_LIGHT_UTIL_HPP
#define MARIO_UTIL_LIGHT_UTIL_HPP

#include <JSystem/JDrama/JDRDrawBufObj.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <dolphin/types.h>

class J3DDrawBuffer;

class TLightWithDBSet;
class TLightMario;

namespace JDrama {
class TAmbAry;
class TLightAry;
} // namespace JDrama

class TLightWithDBSetManager : public JDrama::TViewObj {
public:
	TLightWithDBSetManager(const char*);

	virtual ~TLightWithDBSetManager() { }
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void calcLightBorder();
	GXColor getEffectLightColor() const;
	void setEffectLight(const JDrama::TGraphics*, GXLightObj*);
	Vec* getLightPos() const;
	void makeDrawBuffer();
	void addChildGroupObj(JDrama::TViewObjPtrListT<JDrama::TViewObj>*);

	// fabricated
	TLightWithDBSet* getUnk14(int i) { return unk14[i]; }
	GXColor getUnk18() const { return unk18; }
	f32 getUnk28() const { return unk28; }

public:
	/* 0x10 */ TLightMario* unk10;
	/* 0x14 */ TLightWithDBSet** unk14;
	/* 0x18 */ GXColor unk18;
	/* 0x1C */ JGeometry::TVec3<f32> unk1C;
	/* 0x28 */ float unk28;
	/* 0x2C */ float unk2C;
	/* 0x30 */ float unk30;
	/* 0x34 */ float unk34;
	/* 0x38 */ float unk38;
	/* 0x3C */ float unk3C;
	/* 0x40 */ float unk40;
	/* 0x44 */ float unk44;
	/* 0x48 */ Vec unk48;
	/* 0x54 */ u8 unk54;
	/* 0x55 */ u8 unk55;
};

extern TLightWithDBSetManager* gpLightManager;

class TLightCommon : public JDrama::TViewObj {
public:
	TLightCommon(const char* = "<TLightCommon>");

	virtual void loadAfter();
	virtual void perform(u32, JDrama::TGraphics*);
	virtual GXColor getLightColor(int) const;
	virtual GXColor getAmbColor(int) const;
	virtual const JGeometry::TVec3<f32>* getLightPosition(int);
	virtual void setLight(const JDrama::TGraphics*, int);

	// Field layout reverse-engineered from GMSE01 @0x80229fbc (ctor),
	// @0x80229ca0 (getLightPosition), @0x80229cec (getAmbColor),
	// @0x80229d78 (getLightColor). All decomp-invisible until now — the
	// header declared TLightCommon as size sizeof(TViewObj), which was WRONG.
public:
	/* 0x10 */ f32 unk10;      // ctor: SDA2 -0x17a4 first, then overwritten with -0x1770 default
	/* 0x14 */ f32 unk14;      // = 0.0f
	/* 0x18 */ f32 unk18;      // = 0.0f
	/* 0x1C */ f32 unk1C;      // = 0.0f — used as an alpha scale in getLightColor's group path
	/* 0x20 */ u32 unk20;      // idx-offset added into Light-Group index for getLightColor's group path
	/* 0x24 */ u32 unk24;      // idx-offset added into Light-Group index for getLightPosition's group path
	/* 0x28 */ u8  unk28;      // "use LOCAL ambient/light-color override" flag (getAmbColor + getLightColor)
	/* 0x29 */ u8  mLocalAmbColor[2 * 4];     // packed GXColor[2] at 0x29..0x30 (getAmbColor local override)
	/* 0x31 */ u8  mLocalLightColor[4 * 4];   // packed GXColor[4] at 0x31..0x40 (getLightColor local override)
	/* 0x41 */ u8  unk41;      // "use LOCAL light-position override" flag (getLightPosition)
	/* 0x42 */ u8  pad_42_43[2];
	/* 0x44 */ JGeometry::TVec3<f32> mLocalPos[4];  // 4×12 = 48 bytes (0x44..0x74)
};
// static_assert(sizeof(TLightCommon) == 0x74, "TLightCommon layout drift");

class TLightDrawBuffer : public JDrama::TViewObj {
public:
	TLightDrawBuffer(int, u32, const char*);

	virtual ~TLightDrawBuffer() { }
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void setLight(TLightCommon* light)
	{
		unk10 = light;
		unk10->loadAfter();
	}

	JDrama::TDrawBufObj* getOpaDbo() { return mOpaDrawBufferObject; }
	JDrama::TDrawBufObj* getXluDbo() { return mXluDrawBufferObject; }

public:
	/* 0x10 */ TLightCommon* unk10;
	/* 0x14 */ JDrama::TDrawBufObj* mOpaDrawBufferObject;
	/* 0x18 */ JDrama::TDrawBufObj* mXluDrawBufferObject;
	/* 0x1C */ char unk1C[0x32];
	/* 0x4E */ char unk4E[0x32];
	/* 0x80 */ int unk80;
};

class TLightWithDBSet : public JDrama::TViewObj {
public:
	TLightWithDBSet(int, const char*);

	virtual ~TLightWithDBSet() { }
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void makeDrawBuffer() = 0;

	int getAmbIndex(const char*);
	int getLightIndex(const char*);
	void getLightDrawBuffer(int);
	void getXluDrawBuffer(int);
	void getOpaDrawBuffer(int);
	void resetLightDrawBuffer();
	void changeLightDrawBuffer(int);
	void addChildGroupObj(JDrama::TViewObjPtrListT<JDrama::TViewObj>*);

	bool isEnabled() const { return unk20; }
	void enable() { unk20 = true; }

protected:
	/* 0x10 */ TLightDrawBuffer** unk10;
	/* 0x14 */ J3DDrawBuffer* unk14;
	/* 0x18 */ J3DDrawBuffer* unk18;
	/* 0x1C */ int unk1C;
	/* 0x20 */ bool unk20;
};

class TIndirectLightWithDBSet : public TLightWithDBSet {
public:
	TIndirectLightWithDBSet();

	virtual ~TIndirectLightWithDBSet() { }
	virtual void makeDrawBuffer();
};

class TMapObjectLightWithDBSet : public TLightWithDBSet {
public:
	TMapObjectLightWithDBSet();

	virtual ~TMapObjectLightWithDBSet() { }
	virtual void makeDrawBuffer();
};

class TObjectLightWithDBSet : public TLightWithDBSet {
public:
	TObjectLightWithDBSet();

	virtual ~TObjectLightWithDBSet() { }
	virtual void makeDrawBuffer();
};

class TPlayerLightWithDBSet : public TLightWithDBSet {
public:
	TPlayerLightWithDBSet();

	virtual ~TPlayerLightWithDBSet() { }
	virtual void makeDrawBuffer();
};

class TLightMario : public TLightCommon {
public:
	TLightMario(const char* name = "<TLightMario>")
	    : TLightCommon(name)
	{
	}

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual GXColor getLightColor(int) const;
	virtual GXColor getAmbColor(int) const;
	virtual void setLight(const JDrama::TGraphics*, int);

	// Overrides of TLightCommon virtuals (return GXColor now that the header
	// reflects the actual RE-derived signatures).
	GXColor getAmbColor(int) const;
	GXColor getLightColor(int) const;
};

class TLightShadow : public TLightCommon {
public:
	TLightShadow(const char* name = "<TLightShadow>")
	    : TLightCommon(name)
	{
	}

	virtual ~TLightShadow() { }
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
};

#endif
