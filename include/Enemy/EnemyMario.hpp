#ifndef ENEMY_ENEMY_MARIO_HPP
#define ENEMY_ENEMY_MARIO_HPP

#include <Player/Mario.hpp>
#include "Strategic/HitActor.hpp"

class TEMario;
class TMarioInputReplay;

class TEnemyMario : public TMario {
public:
	struct TReplayLink {
		u8 mNodeIndex;
		u8 mReplayIndex;
	};

	enum EMFlag {
		EM_FLAG_GOAL_REACHED    = 0x1,
		EM_FLAG_DISP_PENCIL     = 0x2,
		EM_FLAG_FORCE_WATER_HIT = 0x10,
		EM_FLAG_ENFORCE_TAKE    = 0x20,
		EM_FLAG_DISP_HP_METER   = 0x80,
	};

	enum EMDoing {
		EM_DOING_WAITING                     = 0x0,
		EM_DOING_RUN_AWAY                    = 0x1,
		EM_DOING_JUMPING                     = 0x2,
		EM_DOING_GET_CLOSER                  = 0x3,
		EM_DOING_TURNING                     = 0x4,
		EM_DOING_WALK_AROUND                 = 0x5,
		EM_DOING_WALK_GRAPH                  = 0x6,
		EM_DOING_HIDE                        = 0x7,
		EM_DOING_APPEAR                      = 0x8,
		EM_DOING_DISAPPEAR                   = 0x9,
		EM_DOING_DISAPPEAR_TO_GATE           = 0xA,
		EM_DOING_REPLAY                      = 0xB,
		EM_DOING_REPLAY_WAITING              = 0xC,
		EM_DOING_REPLAY_JUMP_TO_NEAREST_NODE = 0xD,
		EM_DOING_PRE_DOWN_ANIMATION          = 0xE,
		EM_DOING_DOWN_ANIMATION              = 0xF,
		EM_DOING_RUN_AWAY_TO_NEAREST_NODE    = 0x10,
		EM_DOING_REPLAY_RUN_AWAY             = 0x11,
		EM_DOING_UNK12                       = 0x12,
		EM_DOING_UNK13                       = 0x13,
		EM_DOING_WAITING_TO_INVITE_MARIO     = 0x14,
		EM_DOING_REPLAY_RUN_AWAY_TO_GATE     = 0x15,
		EM_DOING_WAITING_MARIO               = 0x16,
		EM_DOING_GATE_DRAWING                = 0x17,
		EM_DOING_UNK18                       = 0x18,
		EM_DOING_REPLAY_TO_GOAL              = 0x19,
		EM_DOING_UNK1A                       = 0x1A,
		EM_DOING_GET_PAD                     = 0x1B,
	};

	class TSettingParams : public TParams {
	public:
		TSettingParams(const char* path);

		TParamRT<f32> mSearchDist;
		TParamRT<f32> mSearchHeight;
		TParamRT<s16> mWaterCtMax;
		TParamRT<u8> mStopFlag;
		TParamRT<u8> mStampFlag;
		TParamRT<u8> mRandomFlag;
		TParamRT<u8> mCarryFlag;
		TParamRT<u8> mInvincibleFlag;
		TParamRT<f32> mRandomPow;
		TParamRT<s16> mDownTime;
		TParamRT<u8> mPolluteFlag;
		TParamRT<f32> mPolluteSize;
	};

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void initValues();
	virtual void checkReturn();
	virtual void checkController(JDrama::TGraphics* graphics);
	virtual void playerControl(JDrama::TGraphics* graphics);
	virtual void initModel();
	virtual void damageExec(THitActor* hittingActor, int damage,
	                        int damageAnimType, int waterEmit,
	                        f32 knockbackSpeed, int rumbleFrames,
	                        f32 pollutionAmount, s16 invincibilityFrames);

	void drawHPMeter(MtxPtr);
	void reachGoal();
	u8 thinkTrample();
	void hitWater(THitActor*);
	void consider();
	void considerAfter();
	void emGetPad();
	void emGateDrawing();
	void startGateDrawing();
	void emKeepStay();
	void emWaitingMario();
	void emReplayToGoal();
	void emReplayRunAwayToGate();
	void emWaitingToInviteMario();
	void emDrawStamp();
	void emEnforceTake();
	void decideDoingAfterCarry();
	void emReplayRunAway();
	void emRunAwayToNearestNode();
	void runAwayMoveEffect();
	void findRunAwayNearestNode();
	void startRunAway();
	void emDownAnimation();
	void emPreDownAnimation();
	void emReplayJumpToNearestNode();
	void emReplayWaiting();
	void emReplayWaitingToReplayJumpToNearestNode();
	void emReplay();
	void emDisappearToGate();
	void emDisappear();
	void startDisappear(u16);
	void emAppear();
	void emHide();
	void emTurning();
	void emWalkGraph();
	void emWalkAround();
	void emGetCloser();
	void emJumping();
	void emRunAway();
	virtual void emWaiting();
	bool tryTake();
	void changeEMWalkGraph();
	void changeEMJumping();
	void changeEMDoing(u16);
	void startMonteReplay(u32);
	void resetReplayStatus();
	void setStickToAngle(s16, f32);
	void setStickAgainstMario();
	f32 getStickPower();
	void kill();
	void initEnemyValues();
	bool isDispPencil() const;
	BOOL canJumpToNode() const;

	bool checkEMFlag(u32 flag) const { return mEMFlags & flag ? true : false; }
	void onEMFlag(u32 flag) { mEMFlags |= flag; }
	void offEMFlag(u32 flag) { mEMFlags &= ~flag; }

	bool canControl() const
	{
		if (mEMDoing == EM_DOING_REPLAY || mEMDoing == EM_DOING_REPLAY_WAITING
		    || mEMDoing == EM_DOING_REPLAY_RUN_AWAY) {
			return true;
		}
		return false;
	}

	bool isDispStamp() const
	{
		if (mEMDoing == EM_DOING_UNK13 && mSettingParams->mStampFlag.get() == 1)
			return true;

		return false;
	}
	bool isReachedToGate() const
	{
		return mEMDoing == EM_DOING_UNK18 ? true : false;
	}
	bool isDownWaitingToTalk() const
	{
		return mEMDoing == EM_DOING_DOWN_ANIMATION ? true : false;
	}

	TSettingParams* getSettingsParams() { return mSettingParams; }

public:
	u16 unk4290; // goal flag for
	u16 unk4292; // some kind of state
	char unk4294[10];
	TEMario* mEMario; // 0x42A0
	char unk42A4[12];
	f32 mCollisionCheckDist; /* 0x42B0 */ // distance for a collision check
	char unk42B4[94];
};

#endif
