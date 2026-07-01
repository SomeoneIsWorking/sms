#ifndef MOVE_BG_MAP_OBJ_WAVE_HPP
#define MOVE_BG_MAP_OBJ_WAVE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

struct ResTIMG;

class TMapObjWave;

extern TMapObjWave* gpMapObjWave;

// TMapObjWave ("波の表現") — the file-select / Delfino REFLECTIVE SEA surface.
//
// A camera(Mario)-centred animated water grid. perform(8) issues a raw immediate-mode GX
// GX_TRIANGLESTRIP mesh (see draw()) sampling /scene/map/map/wave.bti through two planar
// texgens, blended SRCALPHA/SRCCLR — this is the turquoise sea reported by the oracle
// GX-draw attribution (draw @0x801dd21c). Ported faithfully from the GMSE01 decomp; member
// offsets, SDA2 constants and vertex format are documented in the .cpp.
class TMapObjWave : public JDrama::TViewObj {
public:
	TMapObjWave(const char* name = "波の表現");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void movement();
	void updateTime();
	void updateHeightAndAlpha();
	void draw();
	void getAlpha(float, float) const;
	void noWave();
	f32 getHeight(float, float, float) const;
	f32 getWaveHeight(float, float) const;
	void getStaticTexPos0(float) const;
	void getStaticTexPos1(float) const;
	void getMoveTexPos0(float) const;
	void getMoveTexPos1(float) const;
	void initDraw();

public:
	// Data members — byte offsets match the GMSE01 layout (see MapObjWave.cpp). The base
	// JDrama::TViewObj occupies 0x00..0x0F; these begin at 0x10.
	/* 0x10 */ f32 mExtentBase;   // grid full extent seed (5200)
	/* 0x14 */ f32 mHalfExtent;   // mExtentBase * 0.5 (2600) — grid runs [-half, +half)
	/* 0x18 */ f32 mInvHalfExtent;// 1.0 / mHalfExtent — alpha edge-fade slope
	/* 0x1C */ f32 mGridStep;     // spacing between grid lines (200)
	/* 0x20 */ s32 mStripCount;   // mExtentBase / mGridStep (26) — verts per strip = *2
	/* 0x24 */ f32 mWaveFreqX;    // wave angular freq, X (0.02)
	/* 0x28 */ f32 mWaveFreqZ;    // wave angular freq, Z (0.03)
	/* 0x2C */ f32 mCoefA;        // BG-type-selected wave coeffs
	/* 0x30 */ f32 mCoefB;
	/* 0x34 */ f32 mCoefC;
	/* 0x38 */ f32 mCoefD;
	/* 0x3C */ f32 mAmpX;         // live X wave amplitude (init = mCoefA)
	/* 0x40 */ f32 mAmpZ;         // live Z wave amplitude (init = mCoefB)
	/* 0x44 */ f32 mSplash;       // Mario-proximity splash height (type 4/6 only)
	/* 0x48 */ f32 mSplashRate;   // 0.1
	/* 0x4C */ f32 m4C;           // 400
	/* 0x50 */ f32 m50;           // 150
	/* 0x54 */ f32 mAlpha;        // base vertex alpha (255)
	/* 0x58 */ f32 mAlphaMax;     // 255
	/* 0x5C */ f32 m5C;
	/* 0x60 */ f32 mTexRate;      // per-frame tex-offset scroll rate (0.0015)
	/* 0x64 */ f32 mPhaseX;       // wave phase, X (random init, scrolled by updateTime)
	/* 0x68 */ f32 mPhaseZ;       // wave phase, Z
	/* 0x6C */ f32 mTexOffS;      // TEX0.s scroll offset
	/* 0x70 */ f32 mTexOffT;      // TEX1.t scroll offset
	/* 0x74 */ f32 mTexScale;     // TEX0 planar scale (0.0012)
	/* 0x78 */ f32 mTexScale2;    // TEX1 planar scale (0.0015)
	/* 0x7C */ s16 mReg0[4];      // GX_TEVREG0 S10 colour {194,242,190,0}
	/* 0x84 */ s16 mReg1[4];      // GX_TEVREG1 S10 colour {0,0,0,72}
	/* 0x8C */ s16 mReg2[4];      // GX_TEVREG2 S10 colour {0,0,0,144}
	/* 0x94 */ ResTIMG* mTexture; // /scene/map/map/wave.bti (0 => object inert)
};

#endif
