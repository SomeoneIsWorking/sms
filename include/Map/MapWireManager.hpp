#ifndef MAP_MAP_WIRE_MANAGER_HPP
#define MAP_MAP_WIRE_MANAGER_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <Strategic/TakeActor.hpp>

class TMapWireActorManager;
class TMapWire;

class TMapWireActor : public TTakeActor {
public:
	MtxPtr getTakingMtx() { return nullptr; }
	void checkTakingActor();
	f32 getPosInWire() const;
	void getTipPoints(JGeometry::TVec3<f32>*, JGeometry::TVec3<f32>*) const;
	BOOL receiveMessage(THitActor* sender, u32 message);
	void init(TMapWireActorManager*);
	TMapWireActor(const char*);

	static f32 mCommonAttackRadius;
	static f32 mCommonAttackHeight;

public:
	/* 0x70 */ u8 unk70;
	/* 0x74 */ void* unk74;
};

class TMapWireActorManager {
public:
	void doActorToWire();
	void doWireToActor();
	TMapWireActorManager(TTakeActor*);

public:
	/* 0x0 */ TTakeActor* unk0;
	/* 0x4 */ TMapWireActor unk4;
	/* 0x7C */ u32 unk7C;
};

class TMapWireManager;

// LP64/native: was a tentative DEFINITION in this header -> one per includer = multiple
// definition at the full-closure boot link. `extern` here; defined once in MapWireManager.cpp.
extern TMapWireManager* gpMapWireManager;

class TMapWireManager : public JDrama::TViewObj {
public:
	TMapWire* getWire(int index) const;
	u32 getWireNo(const JGeometry::TVec3<f32>&) const;
	void getPointPosInNthWire(int, const JGeometry::TVec3<f32>&,
	                          JGeometry::TVec3<f32>*) const;
	void getPointPosInWire(const JGeometry::TVec3<f32>&,
	                       JGeometry::TVec3<f32>*) const;
	void perform(u32, JDrama::TGraphics*);
	void entry(TTakeActor*);
	void loadAfter();
	void load(JSUMemoryInputStream&);
	TMapWireManager(const char* name = "ワイヤー管理");

	static JUtility::TColor mUpperSurface;
	static JUtility::TColor mLowerSurface;

	// fabricated
	static TMapWire* getGlobalWire(int index)
	{
		return gpMapWireManager->mWires[index];
	}

public:
	/* 0x10 */ int mWireNum;                       // active wires (= gpCubeWire's count)
	/* 0x14 */ u32 mMaxWireNum;                     // mWires[] capacity (from scene data)
	/* 0x18 */ TMapWire** mWires;
	/* 0x1C */ u32 mActorMgrNum;                    // live mActorMgrs[] entries (insert index)
	/* 0x20 */ u32 mMaxActorMgrNum;                 // mActorMgrs[] capacity (from scene data)
	/* 0x24 */ TMapWireActorManager** mActorMgrs;
	/* 0x28 */ u16 unk28;
};

#endif
