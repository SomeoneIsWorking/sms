#ifndef MOVE_BG_MAP_OBJ_MAMMA_HPP
#define MOVE_BG_MAP_OBJ_MAMMA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjEx.hpp>

// TODO: mark virtual methods as such

class TSandLeaf : public TMapObjBase {
public:
	u32 touchWater(THitActor*);
	void control();
	TSandLeaf(const char* name = "すなやまの芽")
	    : TMapObjBase(name)
	    , unk138(0)
	{
	}

public:
	/* 0x138 */ u32 unk138;
};

class TSandBase : public TMapObjBase {
public:
	void isDown() const;
	void withering();
	TSandBase(const char*);
};

class TSandLeafBase : public TSandBase {
public:
	void grow();
	void control();
	void initMapObj();
	TSandLeafBase(const char* name = "すなやまの芽の土台");
};

class TSandBomb : public TSandLeaf {
public:
	void makeObjAppeared();
	u32 touchWater(THitActor*);
	u32 getSDLModelFlag() const;
	void initMapObj();

	TSandBomb()
	    : TSandLeaf("すなやま爆弾")
	    , unk13C(0)
	    , unk140(0)
	{
	}

public:
	/* 0x13C */ u32 unk13C;
	/* 0x140 */ u8 unk140;
};

class TSandBombBase : public TSandBase {
public:
	void withered();
	void expanded();
	void exploding();
	void explode();
	void waitBeforeExplode();
	void grow();
	void control();
	void findTriggerActor();
	void loadAfter();
	void initMapObj();
	TSandBombBase(const char* name = "すなやま爆弾の土台");
};

class TSandCastle : public TSandBombBase {
public:
	void withering();
	void expanded();
	void explode();
	void waitBeforeExplode();
	void calcRootMatrix();
	void findTriggerActor();
	void loadAfter();
	void initMapObj();
	TSandCastle(const char* name = "砂の城");
};

class TLeanMirror : public TMapObjBase {
public:
	void enemyIsOn() const;
	void draw() const;
	void updateSpeedVec(const JGeometry::TVec3<f32>&, f32);
	BOOL receiveMessage(THitActor* sender, u32 message);
	void touchPlayer(THitActor*);
	void touchEnemy(THitActor*);
	void calcCurrentMtx(MtxPtr);
	void release();
	void controlGoTarget();
	void controlShake();
	void control();
	void loadAfter();
	u32 getSDLModelFlag() const;
	void initMapObj();
	void load(JSUMemoryInputStream&);
	TLeanMirror(const char* name = "ぐらぐら鏡");
};

class TShiningStone : public THitActor {
public:
	void endDemo();
	void putOnLight(TLiveActor*);
	void perform(u32, JDrama::TGraphics*);
	void load(JSUMemoryInputStream&);
	TShiningStone(const char* name = "太陽石");

	// Ivars identified from the perform RE @0x801d07b4 (scratch/decomp_next/801d07b4.c).
	// The class body was empty in the header — CodeWarrior emitted the fields but the
	// GC decomp lost them. Added here so the native port can reference them by name
	// rather than raw byte-offset casts. Sizes fit the used byte offsets exactly.
	/* 0x68 */ MActor** mSpokes;      // pointer to array of 4 MActor* (the "rays")
	/* 0x6C */ MActor*  mMainActor;   // main stone body MActor
	/* 0x70 */ u32 unk70;             // not referenced by perform — kept as opaque
	/* 0x74 */ int mActivatedCount;   // 0..3 — # of activation-state particles to emit per frame per spoke
};

class TMammaBlockRotate : public TMapObjBase {
public:
	u32 touchWater(THitActor*);
	void control();
	void initMapObj();
	void load(JSUMemoryInputStream&);
	TMammaBlockRotate(const char* name = "太陽の塔ブロック");
};

class TMammaYacht : public TMapObjBase {
public:
	void control();
	void initMapObj();
	TMammaYacht(const char* name = "砂の城");
};

class TSandBird : public TJointCoin {
public:
	virtual void control();
	virtual void initMapObj();
	virtual TMapObjBase* makeObjFromJointName(const char*, unsigned short);
	virtual bool nameIsObj(const char*);

	TSandBird(const char* name = "おおすな鳥");
};

class TGoalWatermelon : public TMapObjBase {
public:
	void touchActor(THitActor*);
	void control();
	void loadAfter();
	void load(JSUMemoryInputStream&);
	TGoalWatermelon(const char* name = "スイカゴール");
};

class J3DJoint;

class TMammaMirrorMapOperator : public JDrama::TViewObj {
public:
	void show(int);
	void hide(int);
	void perform(u32, JDrama::TGraphics*);
	void loadAfter();
	TMammaMirrorMapOperator(const char* name = "鏡内地形操作");

	// Ivars from the loadAfter/perform RE (@0x801cf1b0 / @0x801cf46c). The class body was
	// empty in the header — the GC decomp lost the fields. Byte offsets fit the used
	// accesses exactly (loadAfter writes 0x10..0xD8; perform reads the same range).
	//
	//   mNodes[i]        = joints[2] + i sibling walks (mYounger chain in J3DModelData)
	//   mNodeCenter[i]   = 0.5 * (joint.mMin + joint.mMax)   (XYZ midpoint)
	//   mNodeRadius[i]   = min(max(0.5*dx, 0.5*dz) + 2000, 3000)   (horizontal LOD radius)
	//   mNodeVisible[i]  = per-node show/hide cache (transitions call SMS_ShowJoint)
	//   mMirror{S,M,L}Pos = positions of the "mirrorS/M/L" TMapStaticObj scene entries
	/* 0x10 */ J3DJoint* mNodes[8];
	/* 0x30 */ JGeometry::TVec3<f32> mNodeCenter[8];
	/* 0x90 */ f32 mNodeRadius[8];
	/* 0xB0 */ u8 mNodeVisible[8];
	/* 0xB8 */ JGeometry::TVec3<f32> mMirrorSPos;
	/* 0xC4 */ JGeometry::TVec3<f32> mMirrorMPos;
	/* 0xD0 */ JGeometry::TVec3<f32> mMirrorLPos;
};

class TSandEgg : public TMapObjBase {
public:
	u32 getSDLModelFlag() const;
	TSandEgg(const char* name = "すなのたまご");
};

#endif
