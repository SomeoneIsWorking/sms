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
};

class TOneShotGenerator : public THitActor {
public:
	TOneShotGenerator(const char* name = "<TOneShotGenerator>");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	// Ivars deduced from the load RE (@0x8008f710). CodeWarrior emitted 3 slots between
	// THitActor's end (0x68) and the second string; the middle 4 bytes (unk6C) aren't
	// touched by load — kept as opaque so the class size lines up with the game's.
	//
	//   mSpawnKey1  = first stream.readString() result (the trigger name)
	//   mSpawnKey2  = second stream.readString() result (the payload name)
	// Naming is provisional (both look like TNameRef targets consumed by loadAfter, not
	// yet ported); rename when loadAfter is decompiled and their roles are named.
	/* 0x68 */ const char* mSpawnKey2;   // written second by load, stored at lower offset
	/* 0x6C */ u32 unk6C;
	/* 0x70 */ const char* mSpawnKey1;   // written first by load, stored at higher offset
};

#endif
