#ifndef ENEMY_DEBUTELESA_HPP
#define ENEMY_DEBUTELESA_HPP

#include <Enemy/SmallEnemy.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>

// The fat Boo ("デブテルサ" / DebuTelesa). RE'd US GMSE01 (wide-RE sweep 2026-07-17).
// TDebuTelesa : TSmallEnemy — drools particle effects from its right hand + mouth joints.
class TDebuTelesaSaveLoadParams : public TSmallEnemyParams {
public:
	TDebuTelesaSaveLoadParams(const char* path);
};

class TDebuTelesaManager : public TSmallEnemyManager {
public:
	TDebuTelesaManager(const char* name = "デブテルサマネージャー");
	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual void clipEnemies(JDrama::TGraphics*);
};

class TDebuTelesa : public TSmallEnemy {
public:
	TDebuTelesa(const char* name = "デブテルサ");

	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void kill();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual bool changeByJuice();
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual bool isCollidMove(THitActor*);
	virtual bool doKeepDistance();

public:
	/* 0x198 */ s32 mYodareJointIdx;
	/* 0x19C */ s32 mRHandJointIdx;
	/* 0x1A0 */ JGeometry::TVec3<f32> mRHandPos;
};

DECLARE_NERVE(TNerveDebuTelesaWait, TLiveActor);

#endif
