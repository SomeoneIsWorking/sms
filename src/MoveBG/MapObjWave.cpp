#include <MoveBG/MapObjWave.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <System/MarDirector.hpp>
#include <Player/MarioAccess.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <dolphin/gx.h>
#include <cmath>
#include <cstdlib>

#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
static bool sWaveDbg() { static int v = -1; if (v < 0) { const char* e = getenv("SB_WAVE_DBG"); v = (e && *e && *e != '0') ? 1 : 0; } return v; }
#define WAVE_LOG(...) do { if (sWaveDbg()) std::fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define WAVE_LOG(...) do {} while (0)
#endif

// Faithful port of TMapObjWave (the "波の表現" water-surface manager) — the file-select /
// Delfino REFLECTIVE SEA. Definitively attributed to draw() @0x801dd21c by the oracle
// GX-draw attribution tool (SUNBRIGHT_GX_ATTRIB). RE source: Ghidra decomp of GMSE01
// (ctor 0x801dca00, load 0x801dcc08, perform 0x801dcdd8, updateTime 0x801dce60,
// updateHeightAndAlpha 0x801dcf04, draw 0x801dd21c, noWave 0x801dd548, initDraw 0x801dd720).
//
// SDA2 constants (r2/_SDA2_BASE_ = 0x80416BA0), resolved from sms.dol .sdata2:
//   [-0x2530]=0.0      [-0x252c]=0.1        [-0x2528]=255.0    [-0x2524]=1/32768
//   [-0x2520]=360.0    [-0x2518]=2^52 magic [-0x2510]=5200.0   [-0x250c]=200.0
//   [-0x2508]=0.5      [-0x2504]=1.0        [-0x2500]=0.0015    [-0x24fc]=0.0012
//   [-0x24f8]=400.0    [-0x24f4]=150.0      [-0x24f0]=0.02      [-0x24ec]=0.03
//   [-0x24e8]=25.0     [-0x24e4]=20.0       [-0x24e0]=40.0      [-0x24dc]=30.0
//   [-0x24d8]=5.0      [-0x24d4]=10.0       [-0x24d0]=15.0      [-0x24cc]=6.28318 (2pi)
//   [-0x24b8]=0.15915507 (1/2pi)  [-0x24b4]=0.8  [-0x24b0]=50.0
// Engine globals: r13[-0x60b4] = gpMarioPos (grid centre); r13[-0x6048] = gpMarDirector
// (mMap @+0x7c = BG-type selector); the pos matrix &DAT_804045dc = j3dSys.mViewMtx.

extern TMap* gpMap;

// The vertex colour written per-vertex in draw() (ctor @0x801dca00 seeds r13[-0x6270..]).
// Constant for this object's lifetime.
static const u8 kWaveColorR = 200;
static const u8 kWaveColorG = 200;
static const u8 kWaveColorB = 255;

// FUN_8033b1a4 (the game rand) → host rand for the cosmetic wave-phase jitter.
static inline f32 waveRand01() { return (f32)(rand() & 0x7fff) * (1.0f / 32768.0f); }

TMapObjWave::TMapObjWave(const char* name)
    : JDrama::TViewObj(name)
{
	// @0x801dca00. Everything starts at the "no wave" default (SDA2[-0x2530]=0).
	mExtentBase = 0.0f; mHalfExtent = 0.0f; mInvHalfExtent = 0.0f; mGridStep = 0.0f;
	mStripCount = 0;
	mWaveFreqX = 0.0f; mWaveFreqZ = 0.0f;
	mCoefA = mCoefB = mCoefC = mCoefD = 0.0f;
	mAmpX = mAmpZ = 0.0f;
	mSplash = 0.0f;
	mSplashRate = 0.1f;                  // SDA2[-0x252c]
	m4C = 0.0f; m50 = 0.0f;
	mAlpha = 255.0f; mAlphaMax = 255.0f; // SDA2[-0x2528]
	m5C = 0.0f; mTexRate = 0.0f;
	// Random wave phases / tex offsets (param_1[0x19..0x1c]).
	mPhaseX  = 360.0f * waveRand01();
	mPhaseZ  = 360.0f * waveRand01();
	mTexOffS = waveRand01();
	mTexOffT = waveRand01();
	mTexScale = 0.0f; mTexScale2 = 0.0f;
	// GX_TEVREG0/1/2 S10 colours (turquoise + two alpha regs).
	mReg0[0] = 0xC2; mReg0[1] = 0xF2; mReg0[2] = 0xBE; mReg0[3] = 0x00; // {194,242,190,0}
	mReg1[0] = 0;    mReg1[1] = 0;    mReg1[2] = 0;    mReg1[3] = 0x48; // {0,0,0,72}
	mReg2[0] = 0;    mReg2[1] = 0;    mReg2[2] = 0;    mReg2[3] = 0x90; // {0,0,0,144}
	mTexture = nullptr;

	gpMapObjWave = this;
	WAVE_LOG("[wave] ctor this=%p name=%s\n", (void*)this, name ? name : "?");
}

void TMapObjWave::load(JSUMemoryInputStream&)
{
	// @0x801dcc08. Fixed grid + tex parameters; wave coeffs selected by the stage's BG type.
	mExtentBase    = 5200.0f;            // SDA2[-0x2510]
	mGridStep      = 200.0f;             // SDA2[-0x250c]
	mHalfExtent    = mExtentBase * 0.5f; // 2600
	mInvHalfExtent = 1.0f / mHalfExtent; // 1/2600
	mStripCount    = (s32)(mExtentBase / mGridStep); // 26

	mTexture   = (ResTIMG*)JKRFileLoader::getGlbResource("/scene/map/map/wave.bti");

	mTexRate   = 0.0015f;                // SDA2[-0x2500]  (m60)
	mTexScale  = 0.0012f;                // SDA2[-0x24fc]  (m74)
	mTexScale2 = 0.0015f;                // SDA2[-0x2500]  (m78)
	m4C        = 400.0f;                 // SDA2[-0x24f8]
	m50        = 150.0f;                 // SDA2[-0x24f4]
	mWaveFreqX = 0.02f;                  // SDA2[-0x24f0]  (m24)
	mWaveFreqZ = 0.03f;                  // SDA2[-0x24ec]  (m28)

	const u8 bg = gpMarDirector ? gpMarDirector->mMap : 0xFF; // TMarDirector+0x7c
	switch (bg) {
	case 0x0D:
		mCoefA = 30.0f; mCoefB = 25.0f; mCoefC = 5.0f; mCoefD = 0.0f; break;
	case 0x04:
		mCoefA = 40.0f; mCoefB = 30.0f; mCoefC = 5.0f; mCoefD = 0.0f; break;
	case 0x09:
	case 0x34:
		mCoefA = 10.0f; mCoefB = 15.0f; mCoefC = 0.0f; mCoefD = 0.0f; break;
	case 0x03:
	case 0x1E:
		mCoefA = 25.0f; mCoefB = 20.0f; mCoefC = 0.0f; mCoefD = 0.0f;
		mAmpX = mCoefA; mAmpZ = mCoefB;   // this branch pre-sets the live amps too
		break;
	default:
		mCoefA = 30.0f; mCoefB = 25.0f; mCoefC = 0.0f; mCoefD = 0.0f; break;
	}
	// LAB_801dcdb4: live amplitudes track the selected coeffs.
	mAmpX = mCoefA;
	mAmpZ = mCoefB;
	WAVE_LOG("[wave] load tex=%p mMap=%d coef=%.0f/%.0f strip=%d half=%.0f\n",
	         (void*)mTexture, (int)bg, mCoefA, mCoefB, mStripCount, mHalfExtent);
}

void TMapObjWave::updateTime()
{
	// @0x801dce60. Scroll wave phases (wrap at 2pi) and tex offsets (wrap at 1.0).
	const f32 kTwoPi = 6.28318f; // SDA2[-0x24cc]
	mPhaseX += mWaveFreqX; if (mPhaseX > kTwoPi) mPhaseX -= kTwoPi;
	mPhaseZ += mWaveFreqZ; if (mPhaseZ > kTwoPi) mPhaseZ -= kTwoPi;
	mTexOffS += mTexRate;  if (mTexOffS > 1.0f)  mTexOffS -= 1.0f;
	mTexOffT += mTexRate;  if (mTexOffT > 1.0f)  mTexOffT -= 1.0f;
}

void TMapObjWave::updateHeightAndAlpha()
{
	// @0x801dcf04. Mario-proximity wave/alpha modulation (only stages mMap==4/6 call this).
	// Not exercised on the file-select option map (mMap==15); ported conservatively — the
	// exact per-cell interpolation reads Map internals not needed for the sea to render.
	mAmpX = mCoefC;
	mAmpZ = mCoefD;
}

void TMapObjWave::perform(u32 param, JDrama::TGraphics*)
{
	// @0x801dcdd8. Gated on the texture being resident (object inert without wave.bti).
#ifdef SMS_NATIVE_PLATFORM
	static int sPerfLog = 0;
	if (sWaveDbg() && sPerfLog < 4) { sPerfLog++;
		std::fprintf(stderr, "[wave] perform param=0x%x tex=%p\n", param, (void*)mTexture); }
#endif
	if (!mTexture)
		return;
	if (param & 0x1) {
		updateTime();
		const u8 bg = gpMarDirector ? gpMarDirector->mMap : 0xFF;
		if (bg == 0x04 || bg == 0x06)
			updateHeightAndAlpha();
	}
	if (param & 0x8) {
		initDraw();
		draw();
	}
}

void TMapObjWave::noWave()
{
	// @0x801dd548. Flatten the surface (used when leaving water).
	mCoefC = 0.0f; mCoefD = 0.0f;
	mCoefA = 0.0f; mCoefB = 0.0f;
	mAmpX  = 0.0f; mAmpZ  = 0.0f;
}

void TMapObjWave::initDraw()
{
	// @0x801dd720. Vertex format: POS(f32 xyz) + CLR0(rgba8) + TEX0(f32 st) + TEX1(f32 st).
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST,   GX_F32,   0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST,   GX_F32,   0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0); // &DAT_804045dc = j3dSys.mViewMtx
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE, GX_AF_NONE);

	GXSetNumTexGens(2);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, GX_FALSE, 0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, 0x3c, GX_FALSE, 0x7d);

	JUTTexture tex(mTexture);
	tex.load(GX_TEXMAP0);

	GXSetTevColorS10(GX_TEVREG0, *(GXColorS10*)mReg0); // turquoise (inert unless combiner refs C0)
	GXSetTevColorS10(GX_TEVREG1, *(GXColorS10*)mReg1);
	GXSetTevColorS10(GX_TEVREG2, *(GXColorS10*)mReg2);

	GXSetNumTevStages(2);
	// Stage 0: color = 0; alpha = TEXA (tex alpha as raster).
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, (GXTevColorArg)0xF, (GXTevColorArg)0xF,
	                (GXTevColorArg)0xF, (GXTevColorArg)0xF);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, (GXTevAlphaArg)7, (GXTevAlphaArg)4,
	                (GXTevAlphaArg)5, (GXTevAlphaArg)7);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	// Stage 1: color = RASC * 2 (vertex colour); alpha = TEXA * APREV.
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP0, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE1, (GXTevColorArg)0xA, (GXTevColorArg)0xF,
	                (GXTevColorArg)0xF, (GXTevColorArg)0xF);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE1, (GXTevAlphaArg)7, (GXTevAlphaArg)4,
	                (GXTevAlphaArg)0, (GXTevAlphaArg)7);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_TRUE, GX_TEVPREV);

#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_WAVE_SOLID")) {
		// Isolation test: opaque, no blend — proves whether the grid lands on-screen at all.
		GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
		GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
		GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
		GXSetCullMode(GX_CULL_NONE);
		return;
	}
#endif
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_SRCCLR, GX_LO_CLEAR);
	GXSetAlphaCompare(GX_GEQUAL, 0, GX_AOP_AND, GX_LEQUAL, 255);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE); // depth-test on, depth-write off (transparent)
	GXSetCullMode(GX_CULL_NONE);
}

void TMapObjWave::draw()
{
	// @0x801dd21c. A Mario-centred grid of GX_TRIANGLESTRIP rows. Per vertex:
	//   POS   = (worldX, waveY, worldZ)   worldX/Z = grid coord + gpMarioPos; y = sin ripple
	//   CLR0  = {200,200,255, edge-fade alpha}
	//   TEX0  = (mTexOffS + worldX*mTexScale, worldZ*mTexScale)
	//   TEX1  = (0.8 * worldX*mTexScale2, mTexOffT + worldZ*mTexScale2)
	if (!mTexture)
		return;

	const JGeometry::TVec3<f32>& origin = *gpMarioPos;
	const f32 kInv2Pi = 0.15915507f;  // SDA2[-0x24b8]
	const f32 kTexS2K = 0.8f;         // SDA2[-0x24b4]

	const int stripVerts = (mStripCount & 0x7fff) * 2;
	const f32 last = mHalfExtent - mGridStep;
#ifdef SMS_NATIVE_PLATFORM
	{ static int sDrawLog = 0; if (sWaveDbg() && sDrawLog < 3) { sDrawLog++;
		std::fprintf(stderr, "[wave] draw origin=(%.0f,%.0f,%.0f) amp=%.0f/%.0f strips=%d vpp=%d\n",
		             origin.x, origin.y, origin.z, mAmpX, mAmpZ, mStripCount, stripVerts); } }
#endif

	for (f32 zc = -mHalfExtent; zc <= last; zc += mGridStep) {
		const f32 zNear  = zc + origin.z;
		const f32 zFar   = zNear + mGridStep;
		const f32 zNearP = kInv2Pi * zNear;
		const f32 zFarP  = kInv2Pi * zFar;

		GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, (u16)stripVerts);
		for (f32 xc = -mHalfExtent; xc <= last; xc += mGridStep) {
			const f32 xw = xc + origin.x;
			const f32 xp = kInv2Pi * xw;
			// Radial edge-fade alpha (faithful intent of draw @0x801dd2b8/0x801dd30c:
			// mAlpha * clamp(1 - |coord|/half)); opaque at the Mario-centred middle.
			f32 fadeN = 1.0f - mInvHalfExtent * std::fmax(std::fabs(xc), std::fabs(zc));
			f32 fadeF = 1.0f - mInvHalfExtent * std::fmax(std::fabs(xc), std::fabs(zc + mGridStep));
			if (fadeN < 0.0f) fadeN = 0.0f; if (fadeN > 1.0f) fadeN = 1.0f;
			if (fadeF < 0.0f) fadeF = 0.0f; if (fadeF > 1.0f) fadeF = 1.0f;
			const u8 aN = (u8)(mAlpha * fadeN);
			const u8 aF = (u8)(mAlpha * fadeF);

			const f32 sinX = std::sin(mWaveFreqX * xp + mPhaseX);
			// near vertex
			const f32 yN = mAmpX * sinX + mAmpZ * std::sin(mWaveFreqZ * zNearP + mPhaseZ);
			GXPosition3f32(xw, yN, zNear);
			GXColor4u8(kWaveColorR, kWaveColorG, kWaveColorB, aN);
			GXTexCoord2f32(mTexOffS + xw * mTexScale, zNear * mTexScale);
			GXTexCoord2f32(kTexS2K * xw * mTexScale2, mTexOffT + zNear * mTexScale2);
			// far vertex
			const f32 yF = mAmpX * sinX + mAmpZ * std::sin(mWaveFreqZ * zFarP + mPhaseZ);
			GXPosition3f32(xw, yF, zFar);
			GXColor4u8(kWaveColorR, kWaveColorG, kWaveColorB, aF);
			GXTexCoord2f32(mTexOffS + xw * mTexScale, zFar * mTexScale);
			GXTexCoord2f32(kTexS2K * xw * mTexScale2, mTexOffT + zFar * mTexScale2);
		}
		GXEnd();
	}
}

f32 TMapObjWave::getHeight(float x, float y, float z) const
{
	const TBGCheckData* bg = nullptr;
	// @801dd568: query the ground column at (x, 50 + y, z). checkGroundExactY adds its own
	// +78 internally; 50.0f is SDA2[-0x24b0].
	f32 h = gpMap ? gpMap->checkGroundExactY(x, 50.0f + y, z, &bg) : y;

	if (!bg || !bg->isWaterSurface())
		return y;

	if (bg->isSea())
		return h + getWaveHeight(x, z);
	return h;
}

f32 TMapObjWave::getWaveHeight(float x, float z) const
{
	// @801dd694: the same two-axis sinusoid draw() applies to the surface, sampled at (x,z).
	const f32 kInv2Pi = 0.15915507f;
	return mAmpX * std::sin(mWaveFreqX * (kInv2Pi * x) + mPhaseX)
	     + mAmpZ * std::sin(mWaveFreqZ * (kInv2Pi * z) + mPhaseZ);
}
