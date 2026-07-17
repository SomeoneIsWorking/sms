#ifndef ANIMAL_ANIMAL_BASE_HPP
#define ANIMAL_ANIMAL_BASE_HPP

#include <Enemy/Enemy.hpp>

class TAnimalBase : public TSpineEnemy {
public:
	TAnimalBase(u32, const char* name = "?");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();

	void execWalk(bool);
	void getRotationFlyToDir(JGeometry::TVec3<f32>*,
	                         const JGeometry::TVec3<f32>&, f32, f32);
	void resetRandomCurPathNode();
	void loadAfter();

	// Private flock-spawn helper called by load() once per sibling clone. Absent
	// from the original header (weak US symbol); added for the native port (RE'd
	// US GMSE01 @0x80008cd4). Scatters a bare-constructed clone around this template.
	void initNoLoad_(TAnimalBase* other);

public:
	/* 0x150 */ int* mFrameTimer;
};

#endif
