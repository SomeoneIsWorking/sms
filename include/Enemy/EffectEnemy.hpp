#ifndef ENEMY_EFFECT_ENEMY_HPP
#define ENEMY_EFFECT_ENEMY_HPP

#include <Enemy/WalkerEnemy.hpp>   // TWalkerEnemy
#include <Enemy/SmallEnemy.hpp>    // TSmallEnemyManager

class TLiveManager;

// The "moving fire effect" enemy (moveFireEffect.prm) — a walking fire hazard that burns
// Mario on contact. RE'd US GMSE01 (wide-RE sweep 2026-07-17). TEffectEnemy : TWalkerEnemy.
class TEffectEnemy : public TWalkerEnemy {
public:
	TEffectEnemy(const char* name = "エフェクト敵");
	virtual ~TEffectEnemy();

	virtual void perform(u32, JDrama::TGraphics*);
	virtual void init(TLiveManager*);
	virtual void reset();
	virtual void kill();
	virtual void behaveToWater(THitActor*);
	virtual void forceKill();
	virtual void setMActorAndKeeper();
	virtual void sendAttackMsgToMario();
	virtual void setDeadAnm();

public:
	/* 0x194 */ int mAttackMode;
};

// TEffectEnemyManager : TSmallEnemyManager — no new fields.
class TEffectEnemyManager : public TSmallEnemyManager {
public:
	TEffectEnemyManager(const char* name = "エネミーマネージャー");
	virtual ~TEffectEnemyManager();

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void initSetEnemies() {}
	virtual TSpineEnemy* createEnemyInstance();
};

#endif
