#ifndef ENEMY_EGGGEN_HPP
#define ENEMY_EGGGEN_HPP

#include <Enemy/Enemy.hpp>        // TSpineEnemy
#include <Enemy/EnemyManager.hpp> // TEnemyManager

class TLiveManager;

// The Yoshi-egg generator ("EggGenerator" / "WickedEggGenerator"). RE'd US GMSE01
// (wide-RE sweep 2026-07-17). TEggGenerator : TSpineEnemy — vtable 0x114 (overrides
// init + control + dtor only; no new fields, no nerves — behaviour is in control()).
class TEggGenerator : public TSpineEnemy {
public:
	TEggGenerator(const char* name = "?");

	virtual ~TEggGenerator();
	virtual void init(TLiveManager*);
	virtual void control();
};

// TEggGenManager : TEnemyManager — no new fields (params in inherited unk38).
class TEggGenManager : public TEnemyManager {
public:
	TEggGenManager(const char* name = "?");

	virtual ~TEggGenManager();
	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

#endif
