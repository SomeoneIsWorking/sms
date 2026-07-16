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
	// Retail @0x802281b8 returns the Vec* global @r13-0x6110, which
	// TLightCommon::loadAfter (@0x80229e30) publishes as
	// &LightGroup.mLights[0].mPosition — the scene's FIRST light (the sun).
	// (An earlier guess mapped it to mEffectPos, which stays zero at file-select
	// — that made the shadow direction straight-down instead of sun-slanted.)
	const JGeometry::TVec3<f32>* getLightPos() const;
	void makeDrawBuffer();
	void addChildGroupObj(JDrama::TViewObjPtrListT<JDrama::TViewObj>*);

	// fabricated (renamed for clarity — see mLightSets below)
	TLightWithDBSet* getUnk14(int i) { return mLightSets[i]; }

public:
	/* 0x10 */ TLightMario* mMarioLight;                    // was unk10
	/* 0x14 */ TLightWithDBSet** mLightSets;                // was unk14 — 4 per-kind sets (player/mapobj/object/indirect)
	/* 0x18 */ GXColor mEffectColor;                        // was unk18 — GX_LIGHT1 effect color
	/* 0x1C */ u32 unk1C;                                   // aliased over Vec3<f32> mEffectPos (setLight reads as 3 f32s)
	/* 0x20 */ u32 unk20;                                   // ...
	/* 0x24 */ u32 unk24;                                   // ...
	/* 0x28 */ float mEffectAlphaScale;                     // was unk28 — setLight scales mEffectColor.a
	/* 0x2C */ float unk2C;
	/* 0x30 */ float unk30;
	/* 0x34 */ float unk34;
	/* 0x38 */ float unk38;
	/* 0x3C */ float unk3C;
	/* 0x40 */ float unk40;
	/* 0x44 */ float unk44;
	/* 0x48 */ Vec unk48;
	/* 0x54 */ u8 mEffectEnabled;                           // was unk54 — setLight gate #1 for GX_LIGHT1
	/* 0x55 */ u8 mEffectValid;                             // was unk55 — setLight gate #2 for GX_LIGHT1
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
	/* 0x10 */ f32 mShininess;         // was unk10 — GX_LIGHT2 specular shininess. setLight (@0x80229c30)
	                                   // feeds it as GXInitLightAttn(0,0,1, s/2,0,1-s/2) (GXInitLightShininess
	                                   // inlined). Init path sets 50.0f → distAtt=(25,0,-24), matching the
	                                   // file-select oracle dff. Ctor default 1.0f.
	/* 0x14 */ f32 unk14;              // = 0.0f
	/* 0x18 */ f32 unk18;              // = 0.0f
	/* 0x1C */ f32 mAlphaScale;        // was unk1C — multiplies GXColor.a in getLightColor/getAmbColor group paths
	/* 0x20 */ u32 mAmbBaseIdx;        // was unk20 — added into AmbGroup index in getAmbColor group path
	/* 0x24 */ u32 mLightBaseIdx;      // was unk24 — added into LightGroup index in getLightPosition/getLightColor group paths
	/* 0x28 */ u8  mUseLocalColor;     // was unk28 — gate for LOCAL ambient/light-color override
	/* 0x29 */ u8  mLocalAmbColor[2 * 4];     // packed GXColor[2] at 0x29..0x30 (getAmbColor local override)
	/* 0x31 */ u8  mLocalLightColor[4 * 4];   // packed GXColor[4] at 0x31..0x40 (getLightColor local override)
	/* 0x41 */ u8  mUseLocalPos;       // was unk41 — gate for LOCAL light-position override (getLightPosition)
	/* 0x42 */ u8  pad_42_43[2];
	/* 0x44 */ JGeometry::TVec3<f32> mLocalPos[4];  // 4×12 = 48 bytes (0x44..0x74)
};
// static_assert(sizeof(TLightCommon) == 0x74, "TLightCommon layout drift");

class TLightDrawBuffer : public JDrama::TViewObj {
public:
	TLightDrawBuffer(int, u32, const char*);

	virtual ~TLightDrawBuffer() { }
	virtual void perform(u32, JDrama::TGraphics*);
	// Store the owning light: perform's flag-0x20 phase dispatches
	// owner->setLight(graphics, mLightIndex) — the per-buffer relight that
	// re-establishes lights+ambient before the buffer's draws. This was an
	// empty `{ }` stub, silently discarding the owner every makeDrawBuffer
	// passed in — the per-buffer relight never ran, so the buffers' draws
	// inherited whatever ambient the previous node left (found 2026-07-16 as
	// the black-ambient block tint after the shadow port's faithful mid-frame
	// ReInitializeGX; the banned silent-success-stub class again).
	virtual void setLight(TLightCommon* light) { mOwnerLight = light; }

public:
	/* 0x10 */ TLightCommon* mOwnerLight;                   // was unk10 — perform dispatches setLight through it
	/* 0x14 */ JDrama::TDrawBufObj* mOpaDrawBuf;            // was unk14 — perform forwards to it in OPA phase
	/* 0x18 */ JDrama::TDrawBufObj* mXluDrawBuf;            // was unk18 — perform forwards to it in XLU phase
	/* 0x1C */ char unk1C[0x64];
	/* 0x80 */ int mLightIndex;                             // was unk80 — passed to owner->setLight as idx arg
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

public:
	/* 0x10 */ TLightDrawBuffer** mDrawBuffers;             // was unk10 — allocated by makeDrawBuffer, iterated in perform
	/* 0x14 */ J3DDrawBuffer* mSavedOpaBuffer;              // was unk14 — J3D draw-buffer saved for reset
	/* 0x18 */ J3DDrawBuffer* mSavedXluBuffer;              // was unk18 — J3D draw-buffer saved for reset
	/* 0x1C */ int mBufferCount;                            // was unk1C — length of mDrawBuffers
	/* 0x20 */ u8 mEnabled;                                 // was unk20 — set by external callers, gates manager perform
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
