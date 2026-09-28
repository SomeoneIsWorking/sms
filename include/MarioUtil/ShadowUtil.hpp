#ifndef MARIO_UTIL_SHADOW_UTIL_HPP
#define MARIO_UTIL_SHADOW_UTIL_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JGadget/std-list.hpp>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>

class THitActor;
class J3DModel;
class J3DModelData;
class SDLModelData;
class TCircleShadowRequest;

enum {
	SHADOW_TYPE_CIRCLE = 0,
	SHADOW_TYPE_SQUARE = 1,
	SHADOW_TYPE_TREE   = 2,
	SHADOW_TYPE_SHIP   = 3,
};

class TCircleShadowRequest {
public:
	TCircleShadowRequest()
	    : mRadiusX(0.0f)
	    , mRadiusZ(0.0f)
	    , mRotationY(0.0f)
	    , mCameraDistSq(0.0f)
	    , mShadowType(SHADOW_TYPE_CIRCLE)
	    , mNeedsGroundCheck(1)
	    , mActorType(0)
	{
		mPosition.set(0.0f, 0.0f, 0.0f);
	}

public:
	/* 0x0 */ JGeometry::TVec3<f32> mPosition;
	/* 0xC */ f32 mRadiusX;
	/* 0x10 */ f32 mRadiusZ;
	/* 0x14 */ f32 mRotationY;
	/* 0x18 */ f32 mCameraDistSq;
	/* 0x1C */ u8 mShadowType;
	/* 0x1D */ u8 mNeedsGroundCheck;
	/* 0x20 */ u32 mActorType;
};


// Guest 0x20 record ("TAlphaShadowBlendQuad" per the conectCube* symbols): the XZ
// cluster box. mKey is compared bit-wise by conectCubeDiffer (retail stores the
// request flag word; calcVtx writes 0). Upstream reads the same six floats as a
// 3D min/max pair and the same 0x18 word as unk18; the RE map's reading is the
// one the port's conectCubeSame/connectCubeDiffer implement.
class TAlphaShadowBlendQuad {
public:
	TAlphaShadowBlendQuad()
	    : mMinX(-1.0f)
	    , mY(-1.0f)
	    , mMinZ(-1.0f)
	    , mMaxX(1.0f)
	    , mDy(1.0f)
	    , mMaxZ(1.0f)
	    , mKey(0)
	    , mNext(nullptr)
	{
	}

public:
	/* 0x0 */ f32 mMinX;
	/* 0x4 */ f32 mY;
	/* 0x8 */ f32 mMinZ;
	/* 0xC */ f32 mMaxX;
	/* 0x10 */ f32 mDy;
	/* 0x14 */ f32 mMaxZ;
	/* 0x18 */ u32 mKey;
	/* 0x1C */ TAlphaShadowBlendQuad* mNext;
};

struct TAlphaShadowQuad;

class TAlphaShadowQuadAry {
public:
	TAlphaShadowQuadAry()
	    : mQuadHead(nullptr)
	    , mQuadTail(nullptr)
	    , mBlendHead(nullptr)
	    , mBlendTail(nullptr)
	{
	}

public:
	/* 0x0 */ u32 unk0;
	/* 0x4 */ TAlphaShadowQuad* mQuadHead;
	/* 0x8 */ TAlphaShadowQuad* mQuadTail;
	/* 0xC */ TAlphaShadowBlendQuad* mBlendHead;
	/* 0x10 */ TAlphaShadowBlendQuad* mBlendTail;
};

class TSquareShadowInfo {
public:
	TSquareShadowInfo();

public:
	/* 0x0 */ Vec mPoints[5];
};

class TModelShadowInfo {
public:
	TModelShadowInfo();

public:
	/* 0x0 */ JGeometry::TVec3<f32> mPosition;
	/* 0xC */ bool mIsFar;
	/* 0xD */ bool unkD;
	/* 0x10 */ f32 unk10;
};

class TModelShadow {
public:
	TModelShadow(SDLModelData*, void*, int);

	void update();
	void calc(int, JDrama::TGraphics*);
	void draw(int, JDrama::TGraphics*);
};

class TMBindShadowBody;

class TMBindShadowParts {
public:
	TMBindShadowParts(J3DModel*, u8, TMBindShadowBody*, f32);

	void calc(f32);

public:
	/* 0x0 */ f32 mMinRadius;
	/* 0x4 */ TMBindShadowBody* mBody;
	/* 0x8 */ const char* mJointName;
	/* 0xC */ MtxPtr mJointMtx;
	/* 0x10 */ MtxPtr mChildMtx;
	/* 0x14 */ bool unk14;
	/* 0x15 */ bool mIsCircle;
	/* 0x16 */ bool mIsBody;
};

class TMBindShadowBody {
public:
	TMBindShadowBody(THitActor*, J3DModel*, f32);

	bool isUseThisJoint(int);
	bool isCircleJoint(int);
	bool isBodyJoint(int);
	void entryDrawShadow();
	void calc();

public:
	// NATIVE PORT NOTE: TMBindShadowBody's own methods (ctor/isUseThisJoint/
	// isCircleJoint/isBodyJoint/calc/entryDrawShadow) have NO surviving symbols
	// anywhere in the GMSE01 binary (verified: not in ShadowUtil.cpp's own
	// address range 0x8022c000-0x80233800, not anywhere in
	// reference/sms_gmse01_funcs.txt) — CodeWarrior fully inlined them into
	// TMario::perform (MarioMain.cpp:166, the only call site:
	// `unk390->entryDrawShadow()`). Reconstructing the exact per-joint
	// (isUseThisJoint/isCircleJoint/isBodyJoint) enumeration would need
	// decompiling TMario::perform's whole compiled body and manually separating
	// the inlined shadow logic — not done here. entryDrawShadow() below submits
	// ONE approximate circle request (actor position + a constant radius scaled
	// by mScale) via the SAME real TMBindShadowManager::request() every other
	// TLiveActor uses (TLiveActor::requestShadow(), Strategic/liveactor.cpp:307,
	// is byte-for-byte decompiled and confirms the field semantics this reuses).
	// Residual: Mario's shadow is a single circle, not the true multi-joint
	// bound silhouette — a real gap, not a hack; the mechanism (request queue
	// -> ground-projected footprint -> alpha decal) is faithful.
	THitActor* mActor;
	J3DModel* mModel;
	f32 mScale;
};
class TMBindShadowManager;

extern TMBindShadowManager* gpBindShadowManager;

class TMBindShadowManager : public JDrama::TViewObj {
public:
	TMBindShadowManager(const char* name = "<TMBindShadowManager>");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32, JDrama::TGraphics*);

	void reset();
	void initEntry(TMBindShadowBody*);
	void drawShadowVolume(bool, TAlphaShadowQuad*);
	void drawShadowGD(u32, JDrama::TGraphics*);
	void drawShadow(u32, JDrama::TGraphics*);
	void request(const TCircleShadowRequest&, u32);
	void forceRequest(const TCircleShadowRequest&, u32);
	void calcVtx();

public:
	// NATIVE PORT (Ghidra RE, GMSE01 — full map:
	// debug_journal/2026-07-16_drawshadow_re_map.md; decompiles scratch/decomp_shadow/):
	// calcVtx @0x8022e0cc, request @0x8022ecec, forceRequest @0x8022ebbc,
	// drawShadow @0x8022f014 (Z-tested volume + EFB dst-alpha stencil),
	// drawShadowVolume @0x802305dc, perform @0x80231108, load @0x80231288
	// (loads the /common/shadow*.bmd volume models), ctor @0x802313e4,
	// conectCubeSame/Differ @0x80230e68/0x80230fac. The earlier decal
	// simplification is REPLACED by the faithful implementation (Aurora GX
	// supports GXSetDstAlpha and the DSTALPHA/INVDSTALPHA blend factors
	// natively). drawShadowGD (@0x8022fa40, GD display-list variant behind a
	// zero-init debug toggle retail never sets) is routed to drawShadow — same
	// passes, immediate emission.
	//
	// The record shapes and the member offsets below are the RE map's; the
	// 2026-09-28 merge replaced this class with the upstream declaration, which
	// models a different data flow (quad arrays and square shadows) than the
	// port's live implementation uses.
	enum {
		kMaxRequests = 512, // guest 0x200
		kMaxGroups   = 256, // guest 0x100
		kMaxVtx      = 30,  // guest 0x1e
	};

	// Guest 0x3c record: the 5-corner slanted prism footprint a close-to-ground
	// type-1 (body) shadow extrudes into a volume.
	struct TAlphaShadowVtx {
		JGeometry::TVec3<f32> p[5];
	};
	// Guest 0x70 record (the drawShadowVolume param type): one projected
	// footprint.
	struct TShadowGroup {
		u32 mMask;
		TAlphaShadowQuad* mFpHead;
		TAlphaShadowQuad* mFpTail;
		TAlphaShadowBlendQuad* mBoxHead;
		TAlphaShadowBlendQuad* mBoxTail;
	};
	// Guest 0x14 record filled by request() for type-2 requests (side channel,
	// cap 1).
	struct TType2Rec {
		JGeometry::TVec3<f32> mPos;
		u8 mFar;
		u8 mOn;
	};

	TCircleShadowRequest mRequests[kMaxRequests]; // +0x10
	int mRequestCount;                            // +0x14
	TAlphaShadowQuad* mQuads;                     // +0x18 [kMaxRequests]
	TShadowGroup mGroups[kMaxGroups];             // +0x1c
	int mGroupCount;                              // +0x20
	TAlphaShadowBlendQuad* mBoxes;                // +0x24 [kMaxRequests]
	TAlphaShadowVtx mVtxPool[kMaxVtx];            // +0x28
	int mVtxCount;                                // +0x2c
	JGeometry::TVec3<f32> mShadowDir;             // +0x30 normalize(light pos), set each perform flag-4
	J3DModelData* mModels[4];                     // +0x3c (guest holds SDLModelData* holders; host stores the modeldata directly)
	u16 mType2Count;                              // +0x40
	u8 mDrawDone;                                 // +0x49
	GXColor mShadowColor;                         // +0x5c (stage-dependent, see ctor)
	f32 mProbeOffset;                             // +0x60
	u8 mDebugCubeFlag;                            // +0x64
	u8 mFlag65;                                   // +0x65
	f32 mAspectLo;                                // +0x68
	f32 mAspectHi;                                // +0x6c
	TType2Rec mType2[1];                          // +0x70
};

// Guest 0x70 footprint record, declared after the manager so the nested vertex
// type is complete. Upstream spells the same bytes mRadius/mCorners[4]/
// mSquareOutline/mRequest; the RE map's reading is the one the port's calcVtx
// and drawShadow implement.
struct TAlphaShadowQuad {
	TAlphaShadowQuad()
	    : mSize(0.01f)
	    , mVtx(nullptr)
	    , mReq(nullptr)
	    , mNext(nullptr)
	{
	}

	void reset();

	/* 0x00 */ f32 mSize; // volume height half-extent
	/* 0x04 */ Mtx mMtx;  // modelview (view * TRS)
	/* 0x34 */ JGeometry::TVec3<f32> mCorner[4]; // ground quad corners
	/* 0x64 */ TMBindShadowManager::TAlphaShadowVtx* mVtx; // prism footprint (or null)
	/* 0x68 */ TCircleShadowRequest* mReq;
	/* 0x6C */ TAlphaShadowQuad* mNext; // cluster link
};

#endif
