#ifndef MARIO_UTIL_SHADOW_UTIL_HPP
#define MARIO_UTIL_SHADOW_UTIL_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <dolphin/mtx.h>
#include <dolphin/gx.h>

class THitActor;
class J3DModel;
class J3DModelData;

class TCircleShadowRequest {
public:
	TCircleShadowRequest()
	    : unkC(0.0f)
	    , unk10(0.0f)
	    , unk14(0.0f)
	    , unk18(0.0f)
	    , unk1C(0)
	    , unk1D(1)
	    , unk20(0)
	{
		unk0.set(0.0f, 0.0f, 0.0f);
	}

public:
	/* 0x0 */ JGeometry::TVec3<f32> unk0;
	/* 0xC */ f32 unkC;
	/* 0x10 */ f32 unk10;
	/* 0x14 */ f32 unk14;
	/* 0x18 */ f32 unk18;
	/* 0x1C */ u8 unk1C;
	/* 0x1D */ u8 unk1D;
	/* 0x20 */ u32 unk20;
};

class TMBindShadowBody {
public:
	TMBindShadowBody(THitActor*, J3DModel*, float);
	bool isUseThisJoint(int);
	bool isCircleJoint(int);
	bool isBodyJoint(int);
	void entryDrawShadow();
	void calc();

public:
	// NATIVE PORT NOTE: TMBindShadowBody's own methods (ctor/isUseThisJoint/isCircleJoint/
	// isBodyJoint/calc/entryDrawShadow) have NO surviving symbols anywhere in the GMSE01
	// binary (verified: not in ShadowUtil.cpp's own address range 0x8022c000-0x80233800, not
	// anywhere in reference/sms_gmse01_funcs.txt) — CodeWarrior fully inlined them into
	// TMario::perform (MarioMain.cpp:166, the only call site: `unk390->entryDrawShadow()`).
	// Reconstructing the exact per-joint (isUseThisJoint/isCircleJoint/isBodyJoint) enumeration
	// would need decompiling TMario::perform's whole compiled body and manually separating the
	// inlined shadow logic — not done here. entryDrawShadow() below submits ONE approximate
	// circle request (actor position + a constant radius scaled by mScale) via the SAME real
	// TMBindShadowManager::request() every other TLiveActor uses (TLiveActor::requestShadow(),
	// Strategic/liveactor.cpp:307, is byte-for-byte decompiled and confirms the field semantics
	// this reuses). Residual: Mario's shadow is a single circle, not the true multi-joint bound
	// silhouette — a real gap, not a hack; the mechanism (request queue -> ground-projected
	// footprint -> alpha decal) is faithful.
	THitActor* mActor;
	J3DModel* mModel;
	f32 mScale;
};

struct TAlphaShadowQuad;

class TMBindShadowManager;

extern TMBindShadowManager* gpBindShadowManager;

class TMBindShadowManager : public JDrama::TViewObj {
public:
	TMBindShadowManager(const char* name = "<TMBindShadowManager>");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void reset();
	void initEntry(TMBindShadowBody*);
	void drawShadowVolume(bool, TAlphaShadowQuad*);
	void drawShadowGD(u32, JDrama::TGraphics*);
	void drawShadow(u32, JDrama::TGraphics*);
	void request(const TCircleShadowRequest&, u32);
	void forceRequest(const TCircleShadowRequest&, u32);
	void calcVtx();

public:
	// NATIVE PORT (Ghidra RE, GMSE01 — full map: debug_journal/2026-07-16_drawshadow_re_map.md;
	// decompiles scratch/decomp_shadow/): calcVtx @0x8022e0cc, request @0x8022ecec,
	// forceRequest @0x8022ebbc, drawShadow @0x8022f014 (Z-tested volume + EFB dst-alpha
	// stencil), drawShadowVolume @0x802305dc, perform @0x80231108, load @0x80231288
	// (loads the /common/shadow*.bmd volume models), ctor @0x802313e4,
	// conectCubeSame/Differ @0x80230e68/0x80230fac. The earlier decal simplification is
	// REPLACED by the faithful implementation (Aurora GX supports GXSetDstAlpha and the
	// DSTALPHA/INVDSTALPHA blend factors natively). drawShadowGD (@0x8022fa40, GD
	// display-list variant behind a zero-init debug toggle retail never sets) is routed to
	// drawShadow — same passes, immediate emission.
	static constexpr int kMaxRequests = 512; // guest 0x200
	static constexpr int kMaxGroups   = 256; // guest 0x100
	static constexpr int kMaxVtx      = 30;  // guest 0x1e

	// Guest 0x3c record: the 5-corner slanted prism footprint a close-to-ground
	// type-1 (body) shadow extrudes into a volume.
	struct TAlphaShadowVtx {
		JGeometry::TVec3<f32> p[5];
	};
	// Guest 0x20 record ("TAlphaShadowBlendQuad" per conectCube* symbols): the XZ
	// cluster box. mKey is compared bit-wise by conectCubeDiffer (retail stores the
	// request flag word; calcVtx writes 0).
	struct TAlphaShadowBlendQuad {
		f32 mMinX, mY, mMinZ, mMaxX, mDy, mMaxZ;
		u32 mKey;
		TAlphaShadowBlendQuad* mNext;
	};
	// Guest 0x70 record (the drawShadowVolume param type): one projected footprint.
	struct TShadowGroup {
		u32 mMask;
		TAlphaShadowQuad* mFpHead;
		TAlphaShadowQuad* mFpTail;
		TAlphaShadowBlendQuad* mBoxHead;
		TAlphaShadowBlendQuad* mBoxTail;
	};
	// Guest 0x14 record filled by request() for type-2 requests (side channel, cap 1).
	struct TType2Rec {
		JGeometry::TVec3<f32> mPos;
		u8 mFar;
		u8 mOn;
	};

	TCircleShadowRequest mRequests[kMaxRequests]; // +0x10
	int mRequestCount = 0;                        // +0x14
	TAlphaShadowQuad* mQuads = nullptr;           // +0x18 [kMaxRequests]
	TShadowGroup mGroups[kMaxGroups];             // +0x1c
	int mGroupCount = 0;                          // +0x20
	TAlphaShadowBlendQuad* mBoxes = nullptr;      // +0x24 [kMaxRequests]
	TAlphaShadowVtx mVtxPool[kMaxVtx];            // +0x28
	int mVtxCount = 0;                            // +0x2c
	JGeometry::TVec3<f32> mShadowDir;             // +0x30 normalize(light pos), set each perform flag-4
	J3DModelData* mModels[4] = {};                // +0x3c (guest holds SDLModelData* holders; host stores the modeldata directly)
	u16 mType2Count = 0;                          // +0x40
	u8 mDrawDone = 0;                             // +0x49
	GXColor mShadowColor;                         // +0x5c (stage-dependent, see ctor)
	f32 mProbeOffset = 30.0f;                     // +0x60
	u8 mDebugCubeFlag = 0;                        // +0x64
	u8 mFlag65 = 0;                               // +0x65
	f32 mAspectLo = 0.5f;                         // +0x68
	f32 mAspectHi = 1.55f;                        // +0x6c
	TType2Rec mType2[1];                          // +0x70
};

// Guest 0x70 footprint record (defined after the manager for the nested types).
struct TAlphaShadowQuad {
	f32 mSize;                                          // +0x00 volume height half-extent
	Mtx mMtx;                                           // +0x04 modelview (view * TRS)
	JGeometry::TVec3<f32> mCorner[4];                   // +0x34 ground quad corners
	TMBindShadowManager::TAlphaShadowVtx* mVtx;         // +0x64 prism footprint (or null)
	TCircleShadowRequest* mReq;                         // +0x68
	TAlphaShadowQuad* mNext;                            // +0x6c cluster link
};

#endif
