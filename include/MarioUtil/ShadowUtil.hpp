#ifndef MARIO_UTIL_SHADOW_UTIL_HPP
#define MARIO_UTIL_SHADOW_UTIL_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>

class THitActor;
class J3DModel;

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

class TAlphaShadowQuad;

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
	// NATIVE PORT (Ghidra RE, GMSE01): calcVtx @0x8022e0cc, request @0x8022ecec,
	// forceRequest @0x8022ebbc, drawShadow @0x8022f014, drawShadowGD @0x8022fa40,
	// drawShadowVolume @0x802305dc, perform @0x80231108, load @0x80231288 (scratch/decomp_shadow/).
	// The GC original merges/batches overlapping footprints via a hand-rolled linked-list +
	// TAlphaShadowBlendQuad clustering (conectCubeSame/conectCubeDiffer @0x80230e68/0x80230fac)
	// to draw each covered pixel's darkening ONCE even under N overlapping circles, implemented
	// as a Z-buffer-as-pseudo-stencil two-pass trick (GC's GX has no real stencil buffer). We
	// do NOT replicate that CPU-side batching: it is a rendering-hardware workaround, not game
	// behavior, and our SDL_GPU renderer draws each footprint as an independent alpha-blended
	// decal (native/render — see TMBindShadowManager::drawShadow). RESIDUAL: overlapping
	// footprints double-darken slightly at the overlap (a real, minor, documented gap — not
	// hidden). The proper fix (matching the original's visual result exactly) is a real GPU
	// stencil/coverage-mask pass; not built here — SDL_GPU's current depth target
	// (native/render/gx_sdlgpu.cpp DEPTH_FMT=D32_FLOAT) has no stencil aspect, so this would
	// need a depth+stencil format switch and a 2-pass pipeline, which is out of scope for the
	// initial shadow-visibility fix.
	static constexpr int kMaxRequests = 512;
	TCircleShadowRequest mRequests[kMaxRequests];
	int mRequestCount = 0;

	struct TFootprint {
		JGeometry::TVec3<f32> mPos;   // ground-projected centre
		f32 mRadiusX, mRadiusZ;
		u8 mAlpha;
	};
	TFootprint mFootprints[kMaxRequests];
	int mFootprintCount = 0;
};

#endif
