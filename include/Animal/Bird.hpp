#ifndef ANIMAL_BIRD_HPP
#define ANIMAL_BIRD_HPP

#include <Animal/AnimalBase.hpp>
#include <Animal/AnimalManager.hpp>
#include <Enemy/Enemy.hpp>       // TSpineEnemyParams
#include <Strategic/Nerve.hpp>   // DECLARE_NERVE

class TWireBinder;
class TLiveManager;

// The 9 Delfino-plaza bird nerves. All TSpineBase<TLiveActor> (execute sig
// __CFP24TSpineBase<10TLiveActor>). RE'd US GMSE01 (wide-RE workflow 2026-07-17);
// execute() bodies ported incrementally — DECLARE here, DEFINE_NERVE in Bird.cpp.
DECLARE_NERVE(TNerveAnimalBirdLanding, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdWaitOnGround, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdComeback, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor);

// TAnimalBirdParams : TSpineEnemyParams (first member @0xA8 = exactly after the
// TSpineEnemyParams base). 18 TParamRT tuning members. US ctor 0x8000c990 (0x378).
class TAnimalBirdParams : public TSpineEnemyParams {
public:
	TAnimalBirdParams(const char* prm);

	/* 0xA8  */ TParamRT<f32> mMarchSpeed;
	/* 0xBC  */ TParamRT<f32> mTurnSpeed;
	/* 0xD0  */ TParamRT<int> mReturnTimer;
	/* 0xE4  */ TParamRT<f32> mSearchLength;
	/* 0xF8  */ TParamRT<f32> mSearchHeight;
	/* 0x10C */ TParamRT<f32> mSearchAware;
	/* 0x120 */ TParamRT<f32> mSearchAngle;
	/* 0x134 */ TParamRT<int> mActionTimer;
	/* 0x148 */ TParamRT<int> mWaterproofTimerMax;
	/* 0x15C */ TParamRT<int> mFloatingTimerMax;
	/* 0x170 */ TParamRT<f32> mLandingGravityY;
	/* 0x184 */ TParamRT<f32> mLandingTorqueY;
	/* 0x198 */ TParamRT<f32> mWalkingTorqueY;
	/* 0x1AC */ TParamRT<f32> mWalkingSpeed;
	/* 0x1C0 */ TParamRT<int> mWalkTimer;
	/* 0x1D4 */ TParamRT<f32> mLandingFric;
	/* 0x1E8 */ TParamRT<int> mActionTimerAdd;
	/* 0x1FC */ TParamRT<f32> mWaterPowerY;
};

// TAnimalBirdManager : TAnimalManagerBase — no new fields; mirror of TMewManager
// with the bird model + param file. US ctor 0x8000c954.
class TAnimalBirdManager : public TAnimalManagerBase {
public:
	TAnimalBirdManager(const char* name = "?");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
};

// TAnimalBird : TAnimalBase — vtable size 0x114 == TAnimalBase's (no new virtuals;
// overrides load/receiveMessage/init/calcRootMatrix). New data fields at 0x154.
class TAnimalBird : public TAnimalBase {
public:
	TAnimalBird(const char* name = "?");

	virtual void load(JSUMemoryInputStream&);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();

	void loadAfter();
	void initParams();
	void bind();
	void moveObject();
	void doLanding(bool);
	void doFlyToCurPathNode();
	bool isFindMario() const;
	virtual const char** getBasNameTable() const;

public:
	/* 0x154 */ TWireBinder* mWireBinder;
	/* 0x158 */ JGeometry::TVec3<f32> mHomePos;
	/* 0x164 */ JGeometry::TVec3<f32> mHomeRot;
	/* 0x170 */ f32 mFlyHeight;
	/* 0x174 */ f32 mPhase;
	/* 0x178 */ TPathNode* mCurPathNode;
	/* 0x17C */ TPathNode* mNextPathNode;
};

#endif
