#ifndef MOVE_BG_MAP_OBJ_TOWN_HPP
#define MOVE_BG_MAP_OBJ_TOWN_HPP

#include "Strategic/HitActor.hpp"
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <MoveBG/MapObjHide.hpp>

// TODO: mark virtual methods as such

class TDoor : public TMapObjBase {
public:
	void touchPlayer(THitActor*);
	void load(JSUMemoryInputStream&);
	TDoor(const char* name = "ドア");

	// Ivar deduced from the load RE (@0x801c24cc). Header was empty; CW emitted a single
	// byte at TMapObjBase's end (0x138). Nonzero → the door is "locked" (specific gameplay
	// gate — needs a key / stage-flag). Named provisionally.
	/* 0x138 */ u8 mLocked;
};

class TManhole : public TMapObjGeneral {
public:
	void touchPlayer(THitActor*);
	void animationFinished();
	void appeared();
	void calc();
	void setGroundCollision();
	void makeManholeUnuseful(const TMapObjBase*);
	void loadAfter();
	void initMapObj();
	TManhole(const char* name = "マンホール");
};

class TMapObjBillboard : public THideObjBase {
public:
	void swing(THitActor*);
	void touchActor(THitActor*);
	u32 touchWater(THitActor*);
	TMapObjBillboard(const char* name = "看板")
	    : THideObjBase(name)
	{
	}
};

class TMapObjChangeStage : public TMapObjBase {
public:
	void touchPlayer(THitActor*);
	void load(JSUMemoryInputStream&);
	TMapObjChangeStage(const char* name = "ステージ切り替え")
	    : TMapObjBase(name)
	    , unk138(0)
	{
	}

public:
	/* 0x138 */ u16 unk138;
};

class TMapObjChangeStageHipDrop : public TMapObjChangeStage {
public:
	void touchPlayer(THitActor*);
	void initMapObj();
	TMapObjChangeStageHipDrop(const char* name
	                          = "ステージ切り替え（ヒップドロップ）")
	    : TMapObjChangeStage(name)
	{
	}
};

class TMapObjStartDemo : public TMapObjBase {
public:
	void touchPlayer(THitActor*);
	void load(JSUMemoryInputStream&);
	TMapObjStartDemo(const char* name = "デモ開始オブジェ")
	    : TMapObjBase(name)
	{
	}
};

class TDamageObj : public THitActor {
public:
	void perform(u32, JDrama::TGraphics*);
	void init(u32);
	void load(JSUMemoryInputStream&);
	TDamageObj(const char* name = "ダメージオブジェ")
	    : THitActor(name)
	{
	}
};

class TShadowObj {
public:
	void load(JSUMemoryInputStream&);
};

class TMapObjWaterSpray : public TMapObjBase {
public:
	void calc();
	void load(JSUMemoryInputStream&);
	TMapObjWaterSpray(const char* name = "汎用水しぶき");
};

class THideObjInfo : public JDrama::TViewObj {
public:
	void perform(u32, JDrama::TGraphics*);
	void action(long);
	void load(JSUMemoryInputStream&);
	THideObjInfo(const char* name = "オブジェ出現情報");
};

class TMapObjSwitch : public TMapObjBase {
public:
	void control();
	BOOL receiveMessage(THitActor*, u32);
	void registerObjInfo(THideObjInfo*);
	void load(JSUMemoryInputStream&);
	TMapObjSwitch(const char* name = "オブジェスイッチ");
};

class TRedCoinSwitch : public TMapObjBase {
public:
	BOOL receiveMessage(THitActor*, u32);
	void control();
	void loadAfter();
	void load(JSUMemoryInputStream&);
	TRedCoinSwitch(const char* name = "赤コインスイッチ");

public:
	// Countdown budget (frame-ish units) for the red-coin mission the switch triggers.
	// Populated by ::load from the scene stream: non-positive seed → 1200 default (~20 s
	// @ 60 fps), else seed × 10. Field offset observed as this+0x138 in the RE disasm of
	// 0x801c088c (`stw r0, 0x138(r30)`) — TMapObjBase runs through 0x134, so this is the
	// first TRedCoinSwitch-owned field.
	/* 0x138 */ s32 mTimerDuration;
};

class TBasketReverse : public TMapObjBase {
public:
	void kill();
	void initMapObj();
	TBasketReverse(const char* name = "さかさバスケット");
};

#endif
