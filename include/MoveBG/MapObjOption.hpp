#ifndef MOVE_BG_MAP_OBJ_OPTION_HPP
#define MOVE_BG_MAP_OBJ_OPTION_HPP

#include <MoveBG/MapObjBase.hpp>

class TFileLoadBlock : public TMapObjBase {
public:
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void initMapObj();
	virtual void touchPlayer(THitActor*);

	void makeBlockNoCard();
	void makeBlockNormal();
	void makeBlockRock();
	void pushed();
	TFileLoadBlock(const char* name = "ファイル読み込みブロック");

public:
	/* 0x138 */ u8 mBlockIndex;           // 0/1/2 = FileLoadBlockA/B/C
	/* 0x13C */ TFileLoadBlock* mSiblingBlock0; // the other two file blocks
	/* 0x140 */ TFileLoadBlock* mSiblingBlock1;
	/* 0x144 */ JGeometry::TVec3<f32> mBlockPosition; // = mPosition (set in initMapObj)
};

class TMapObjOptionWall : public THitActor {
public:
	void onCollision();
	void offCollision();
	void init();
	TMapObjOptionWall(const char*);

public:
	/* 0x68 */ TMapCollisionWarp* mWarpCollision;
};

#endif
