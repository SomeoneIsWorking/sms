#ifndef ENEMY_FRUITSBOAT_HPP
#define ENEMY_FRUITSBOAT_HPP

#include <Enemy/Enemy.hpp>        // TSpineEnemy, TSpineEnemyParams
#include <Enemy/EnemyManager.hpp> // TEnemyManager
#include <Strategic/Nerve.hpp>    // DECLARE_NERVE
#include <System/ParamInst.hpp>   // TParamRT / PARAM_INIT

class TLiveManager;
class J3DAnmTransform;
class J3DFrameCtrl;

// The Delfino/Ricco harbor "fruit boat" (ShipDolpic*). Rides the sea surface, pitches
// with the wave slope, rolls (spring), tips toward Mario, follows a graph path or a baked
// BCK track. RE'd US GMSE01 (wide-RE sweep 2026-07-17). 4 kinds by manager ctor int.
//
// STATUS: manager + params ported; the boat's wave physics (moveObject/setGroundCollision/
// requestShadow/setBckTrack/calcRootMatrix/init/load + the 2 nerves) are loud stubs pending
// their own focused pass — several APIs (global name registry, gpCameraPos, TCircleShadow
// request, anim table) need verification. Factory NOT registered yet (boat non-functional).

class TFruitsBoatParams : public TSpineEnemyParams {
public:
	TFruitsBoatParams(const char* prm);
	/* 0xA8 */ TParamRT<f32> mSLMoveSpeed;
	/* 0xBC */ TParamRT<f32> mSLRotSpeed;
	/* 0xD0 */ TParamRT<f32> mSLBckMoveSpeed;
};

class TFruitsBoatManager : public TEnemyManager {
public:
	TFruitsBoatManager(int kind, const char* name);
	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();

public:
	/* 0x54 */ int mBoatKind;
};

class TFruitsBoat : public TSpineEnemy {
public:
	TFruitsBoat(const char* name = "?");
	virtual void load(JSUMemoryInputStream&);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void requestShadow();
	virtual void setGroundCollision();
	virtual Mtx* getRootJointMtx() const;

	int setBckTrack(const char* name);

public:
	/* 0x150 */ s16 mBckReverse;
	/* 0x154 */ f32 mShadowScaleX;
	/* 0x158 */ f32 mShadowScaleZ;
	/* 0x15C */ J3DAnmTransform* mBckAnm;
	/* 0x160 */ J3DFrameCtrl* mBckFrameCtrl;
	/* 0x164 */ JGeometry::TVec3<f32> mTiltAxis;
	/* 0x170 */ f32 mRollAngle;
	/* 0x174 */ f32 mRollVel;
};

DECLARE_NERVE(TNerveFruitsBoatBckTrace, TLiveActor);
DECLARE_NERVE(TNerveFruitsBoatGraphWander, TLiveActor);

#endif
