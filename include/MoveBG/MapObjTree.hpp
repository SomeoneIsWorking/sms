#ifndef MOVE_BG_MAP_OBJ_TREE_HPP
#define MOVE_BG_MAP_OBJ_TREE_HPP

#include <MoveBG/MapObjGeneral.hpp>
#include <dolphin/mtx.h>

class TMapCollisionMove;

// One per-tree leaf: a 0x3C-byte record carrying its own moving collision
// object plus a copy of the model-joint matrix it rides on. Layout + ctor
// reverse-engineered from US GMSE01 (element ctor @0x801f6ef4), see
// debug_journal/2026-07-15_mapobjtree_initmapobj_port_re.md.
class TMapObjLeaf {
public:
	TMapObjLeaf();

	/* 0x00 */ f32 unk0;
	/* 0x04 */ f32 unk4;
	/* 0x08 */ TMapCollisionMove* mCollision;
	/* 0x0C */ Mtx mMtx;
};

class TMapObjTree : public TMapObjGeneral {
public:
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual f32 getRadiusAtY(f32 param_1) const
	{
		return mMinCanopyRadius
		       + (mMaxCanopyRadius - mMinCanopyRadius)
		             * (mPosition.y + mDamageHeight - param_1) / mDamageHeight;
	}
	virtual void initMapObj();
	virtual void touchPlayer(THitActor*);

	int controlLeaf(int); // returns 1 when the leaf has settled, 0 while swinging (summed by perform)
	void initEach();
	TMapObjTree(const char* name = "木");

	static f32 mNearMiddle;
	static f32 mMiddleFar;
	static f32 mBananaTreeJumpPower;

	// Per-tree-species fields, absent from the upstream decomp header but
	// proven live by the DOL (initEach @0x801f6a64 writes them, initMapObj
	// @0x801f68b4 reads mLeafCount/mLeaves). Offsets match the US layout;
	// only the leaf count/array are load-bearing for the current port, the
	// f32 params drive controlLeaf/getRadiusAtY (not yet ported).
	/* 0x148 */ f32 unk148;         // leaf-spread radius param
	/* 0x14C */ f32 unk14C;         // leaf-spread height param
	/* 0x150 */ s32 mLeafCount;     // number of leaves (8 or 12 by species)
	/* 0x154 */ TMapObjLeaf* mLeaves;
	/* 0x158 */ u32 unk158;
	/* 0x15C */ f32 unk15C;         // leaf growth/sway params
	/* 0x160 */ f32 unk160;
	/* 0x164 */ f32 unk164;
	/* 0x168 */ f32 unk168;
};

class TMapEventSink;

class TMapObjTreeScale : public TMapObjTree {
public:
	enum {
		STATE_SMALL             = 0xB,
		STATE_SCALING_UP_Y_ONLY = 0xC,
		STATE_SCALING_UP        = 0xD,
	};

	virtual void loadAfter();
	virtual void control();
	virtual u32 touchWater(THitActor*);

	void startScaleUp();
	void beSmall();
	TMapObjTreeScale(const char* name = "スケールの木");

	static f32 mScaleSpeedY;
	static f32 mStatusChangeScaleY;
	static f32 mScaleSpeedXZ;
	static f32 mScaleMin;

public:
	/* 0x170 */ JGeometry::TVec3<f32> mParticlePositions[30];
	/* 0x2D8 */ s32 mNextFreeParticlePos;
	/* 0x2DC */ s32 mParticleEmitTimer;
	/* 0x2E0 */ TMapEventSink* unk2E0; // Bianco terrain-sinking event
};

#endif
