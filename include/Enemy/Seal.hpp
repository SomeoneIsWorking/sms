#ifndef ENEMY_SEAL_HPP
#define ENEMY_SEAL_HPP

#include <Enemy/Enemy.hpp>        // TSpineEnemy
#include <Enemy/EnemyManager.hpp> // TEnemyManager
#include <Strategic/Nerve.hpp>    // DECLARE_NERVE

class TLiveManager;

// The Delfino-plaza beach seal (OrangeSeal). RE'd US GMSE01 (wide-RE workflow 2026-07-17).
// TSeal : TSpineEnemy — vtable size 0x114 == TSpineEnemy's (no new virtuals, only overrides).
class TSeal : public TSpineEnemy {
public:
	TSeal(const char* name = "?");
	virtual ~TSeal();

	virtual void perform(u32, JDrama::TGraphics*);      // 0x800feb00
	virtual BOOL receiveMessage(THitActor*, u32);       // 0x800fecc8
	virtual void init(TLiveManager*);                   // 0x800feec8
	virtual void calcRootMatrix();                      // 0x800fec24

public:
	/* 0x150 */ s32 mSprayCount;   // spray/damage hit counter (only field past TSpineEnemy)
};

// TSealManager : TEnemyManager — no new fields (thinner than TAnimalManagerBase).
class TSealManager : public TEnemyManager {
public:
	TSealManager(const char* name = "?");               // 0x800feac4

	virtual void load(JSUMemoryInputStream&);           // 0x800fea70
	virtual void createModelData();                     // 0x800fea90
};

DECLARE_NERVE(TNerveSealDie, TLiveActor);    // execute 0x800fe564
DECLARE_NERVE(TNerveSealWait, TLiveActor);   // execute 0x800fe7ac
DECLARE_NERVE(TNerveSealSleep, TLiveActor);  // execute 0x800fe944

#endif
