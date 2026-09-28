#ifndef ENEMY_GENERATOR_HPP
#define ENEMY_GENERATOR_HPP

#include <Strategic/HitActor.hpp>

class TEnemyManager;
class TGraphWeb;

class TGenerator : public JDrama::TViewObj {
public:
	TGenerator(const char* name = "<TGenerator>");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

public:
	/* 0x10 */ JGeometry::TVec3<f32> mPos;
	/* 0x1C */ JGeometry::TVec3<f32> mRot;
	/* 0x28 */ const char* mManagerName;
	/* 0x2C */ TEnemyManager* mManager;
	/* 0x30 */ const char* mGraphName;
	/* 0x34 */ TGraphWeb* mGraph;
	/* 0x38 */ s32 mInterval;
	/* 0x3C */ s32 mTimer;
	// Present in retail sizeof (0x44) but never initialized nor read in this
	// TU, purpose unobserved.
	/* 0x40 */ s32 unk40;
};

class TOneShotGenerator : public THitActor {
public:
	TOneShotGenerator(const char* name = "<TOneShotGenerator>");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	// Ivars deduced from the load RE (@0x8008f710). CodeWarrior emitted 3 slots between
	// THitActor's end (0x68) and the second string. The provisional names this class
	// carried while only load was decompiled are resolved now that loadAfter is: the
	// first string (stored at the HIGHER offset 0x70) is the graph name and the second
	// (lower offset 0x68) the enemy-manager name, which is what loadAfter looks both up
	// through gpConductor. The 0x6C slot between them is the manager pointer that
	// loadAfter fills.
	/* 0x68 */ const char* mManagerName; // written second by load, stored at lower offset
	/* 0x6C */ TEnemyManager* mManager;
	/* 0x70 */ const char* mGraphName;   // written first by load, stored at higher offset
	/* 0x74 */ TGraphWeb* mGraph;
	/* 0x78 */ s32 mCount;
};

#endif
