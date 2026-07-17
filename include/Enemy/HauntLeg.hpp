#ifndef ENEMY_HAUNTLEG_HPP
#define ENEMY_HAUNTLEG_HPP

#include <Enemy/WalkerEnemy.hpp>   // TWalkerEnemy
#include <Enemy/SmallEnemy.hpp>    // TSmallEnemyManager
#include <Strategic/Nerve.hpp>     // DECLARE_NERVE

class TLiveManager;

// THauntLeg — a "haunting leg" enemy (ハントレッグ). RE'd US GMSE01 (wide-RE sweep
// 2026-07-17). THauntLeg : TWalkerEnemy — walks a graph path, jumps to grab a target.
class THauntLeg : public TWalkerEnemy {
public:
	THauntLeg(const char* name = "ハントレッグ");
	virtual ~THauntLeg();

	virtual MtxPtr getTakingMtx();
	virtual const char** getBasNameTable() const;
	virtual void calcRootMatrix();
	virtual void init(TLiveManager*);
	virtual void reset();
	virtual void attackToMario();
	virtual bool isCollidMove(THitActor*);
	virtual void setMActorAndKeeper();
	virtual void setDeadAnm();
	virtual void setRunAnm();
	virtual void setWalkAnm();
	virtual void setWaitAnm();
	virtual void setGenerateAnm();

public:
	/* 0x194 */ THitActor* mHauntObj;
	/* 0x198 */ u8 mGrabbed;
	/* 0x199 */ u8 mAirReset;
	/* 0x19C */ THitActor* mTarget;
	/* 0x1A0 */ JGeometry::TVec3<f32> mJumpVelocity;
	/* 0x1AC */ f32 mSpinAngle;
};

class THauntLegManager : public TSmallEnemyManager {
public:
	THauntLegManager(const char* name = "?");
	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

DECLARE_NERVE(TNerveHauntLegHaunt, TLiveActor);

#endif
