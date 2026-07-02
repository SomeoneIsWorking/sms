#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <System/MarDirector.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorAnm.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

#ifdef SMS_NATIVE_PLATFORM
#include "sms_boot_startanim.h"
#endif

void TMapObjBase::changeObjMtx(MtxPtr mtx)
{
	mPosition.x = mtx[3][0];
	mPosition.y = mtx[3][1] + mYOffset;
	mPosition.z = mtx[3][2];
	if (mMActor) {
		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK100)) {
			setModelMtx(mtx);
		} else {
			calcRootMatrix();
			getModel()->calc();
		}
	}
	removeMapCollision();
	setUpCurrentMapCollision();
}

void TMapObjBase::changeObjSRT(const JGeometry::TVec3<f32>& param_1,
                               const JGeometry::TVec3<f32>& param_2,
                               const JGeometry::TVec3<f32>& param_3)
{
	Mtx mtx;
	MsMtxSetTRS(mtx, param_1.x, param_1.y, param_1.z, param_2.x, param_2.y,
	            param_2.z, param_3.x, param_3.y, param_3.z);
	changeObjMtx(mtx);
}

u32 TMapObjBase::getSDLModelFlag() const { return 3; }

void TMapObjBase::awake()
{
	offLiveFlag(LIVE_FLAG_UNK4000);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	setUpCurrentMapCollision();
}

void TMapObjBase::sleep()
{
	onLiveFlag(LIVE_FLAG_UNK4000);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	removeMapCollision();
}

void TMapObjBase::setObjHitData(u16 param_1)
{
	if (!mMapObjData->mHit)
		return;

	if (mMapObjData->mHit->unk0 <= param_1)
		return;

	const TMapObjHitDataTable* table = &mMapObjData->mHit->unkC[param_1];

	if (table->unk0 >= 0.0f) {
		f32 fVar2 = mScaling.x > mScaling.z ? mScaling.x : mScaling.z;
		f32 fVar3 = mScaling.y;
		setHitParams(table->unk0 * fVar2, table->unk4 * fVar3,
		             table->unk8 * fVar2, table->unkC * fVar3);
	}
}

void TMapObjBase::removeMapCollision()
{
	if (!mMapCollisionManager)
		return;

	// TODO: fakematch fix properly!!!1111
	if (mMapCollisionManager->unk8
	    && mMapCollisionManager->unk8->mKind != TMapCollisionBase::KIND_STATIC)
		if (mMapCollisionManager->unk8)
			((volatile TMapCollisionManager*)mMapCollisionManager)
			    ->unk8->remove();
}

void TMapObjBase::setUpCurrentMapCollision()
{
	TMapCollisionManager* colman = mMapCollisionManager;
	if (!colman)
		return;

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK8)) {
		mMapCollisionManager->getUnk8()->setUpMtx(getModel()->getAnmMtx(0));
	} else {
		JGeometry::TVec3<f32> pos(mPosition.x, mPosition.y - mYOffset,
		                          mPosition.z);
		colman->setUpUnk8TRS(pos, mRotation, mScaling);
	}
}

void TMapObjBase::setUpMapCollision(u16 param_1)
{
	if (!mMapObjData->mCollision
	    || !mMapObjData->mCollision->unk4[param_1].unk0)
		return;

	JGeometry::TVec3<f32> pos(mPosition.x, mPosition.y - mYOffset, mPosition.z);

	mMapCollisionManager->changeCollision(param_1);

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK8)) {
		mMapCollisionManager->getUnk8()->setUpMtx(getModel()->getAnmMtx(0));
	} else {
		mMapCollisionManager->setUpUnk8TRS(pos, mRotation, mScaling);
	}
}

void TMapObjBase::soundBas(u32 param_1, f32 param_2, f32 param_3)
{
	f32 currFrame = mMActor->getFrameCtrl(0)->getFrame();
	if (currFrame <= param_2 && param_2 < currFrame + param_3) {
		SMSGetMSound()->startSoundActor(param_1, &mPosition, 0, nullptr, 0, 4);
	}
}

void TMapObjBase::startSound(u16 param_1)
{
	if (unk100 != param_1)
		unk100 = param_1;

	if (!mMapObjData->mSound) {
		u32 uVar3 = TMapObjGeneral::mDefaultSound.unk0[unk100];
		if (uVar3 != 0xffffffff)
			SMSGetMSound()->startSoundActor(uVar3, &mPosition, 0, nullptr, 0,
			                                4);
	} else {
		u32 uVar3 = mMapObjData->mSound->unk4->unk0[unk100];
		if (uVar3 != 0xffffffff)
			SMSGetMSound()->startSoundActor(uVar3, &mPosition, 0, nullptr, 0,
			                                4);
	}
}

bool TMapObjBase::hasModelOrAnimData(u16 param_1) const
{
	if (!mMapObjData->mAnim || mMapObjData->mAnim->unk0 <= param_1
	    || (!mMapObjData->mAnim->unk4[param_1].unk4
	        && !mMapObjData->mAnim->unk4[param_1].unk0)) {
		return false;
	}

	return true;
}

bool TMapObjBase::hasAnim(u16 param_1) const
{
	if (!mMapObjData->mAnim || mMapObjData->mAnim->unk0 <= param_1
	    || !mMapObjData->mAnim->unk4[param_1].unk4) {
		return false;
	}

	return true;
}

bool TMapObjBase::animIsFinished() const
{
	if (!mMapObjData->mAnim || mMapObjData->mAnim->unk0 == 0
	    || !mMapObjData->mAnim->unk4[unkFE].unk4)
		return true;

	if (mMActor->curAnmEndsNext(0, nullptr))
		return true;
	else
		return false;
}

void TMapObjBase::stopAnim() { }

void TMapObjBase::startControlAnim(u16 param_1)
{
	startAnim(param_1);
	if (mMapObjData->mAnim && param_1 < mMapObjData->mAnim->unk0)
		mMActor->getFrameCtrl(mMapObjData->mAnim->unk4[param_1].unk8)
		    ->setRate(0);
}

void TMapObjBase::startBck(const char* param_1)
{
	offMapObjFlag(MAP_OBJ_FLAG_UNK100);
	mMActor->setBck(param_1);
}

void TMapObjBase::startAnim(u16 param_1)
{
#ifdef SMS_NATIVE_PLATFORM
	// Native port of TMapObjBase::startAnim (@0x801b09d4, 125 insns). Kicks off
	// animation slot `param_1` on this map-object. The BMD/BCK data are indexed
	// by `mMapObjData->mAnim->unk4[idx]` (a TMapObjAnimData: unk0=model name,
	// unk4=anim name, unk8=slot, unk10=anmsound path).
	//
	// Layout in one pass:
	//   1. clear any active MAnmSound              (setAnmSound(nullptr))
	//   2. lazy-load mMActor from anim[0].unk0     (if we don't have one yet)
	//   3. bail if no anim table / idx out of range (fencepost `count > idx`)
	//   4. if unk8==0 for this entry AND we had a previous slot → freeze &
	//      reset that previous slot's frame-ctrl + anm sentinel
	//   5. stopAnmSound() to kill anything the old slot started
	//   6. if the slot actually changed → maybe reload mMActor from entry.unk0
	//   7. if entry.unk4 (anim name) → clear the "playing" flag bit, dispatch
	//      MActor::setAnimation(name, slot); guarded second clear if bit 0x200
	//      is set (force-restart gate). Else: reset MActor's mtx-calc back to
	//      the model's root joint (raw offset chain 0x58 in J3DMtxCalc; the
	//      concrete subclass is decided by the model).
	//   8. if entry.unk10 (anmsound path) → setAnmSound(path).
	//
	// Pure predicates (fencepost, sentinel, flag-bit constants) live in
	// native/render/sms_boot_startanim.h and are unit-tested by startanim_test.
	using namespace sb::startanim;

	if (mAnmSound)
		setAnmSound(nullptr);

	const TMapObjAnimDataInfo* anim = mMapObjData->mAnim;

	if (mMActor == nullptr) {
		if (anim && anim->unk0 > 0)
			mMActor = mMActorKeeper->getMActor(anim->unk4[0].unk0);
	}

	anim = mMapObjData->mAnim;
	if (anim == nullptr)
		return;

	const u16 count = anim->unk0;
	const u16 idx   = param_1;
	if (!has_slot(count, idx))
		return;

	const TMapObjAnimData* entry = &anim->unk4[idx];

	if (entry->unk8 == 0) {
		// Freeze/reset the OUTGOING slot's frame-ctrl if we had one.
		if (!is_sentinel(unkFE) && anim && count != 0) {
			const TMapObjAnimData* outEntry = &anim->unk4[unkFE];
			if (outEntry->unk4 != nullptr) {
				u8 outSlot = outEntry->unk8;
				mMActor->getFrameCtrl(outSlot)->setRate(0.0f);
				mMActor->getFrameCtrl(outSlot)->setFrame(0.0f);
				mMActor->getUnk28(outSlot)->unk0 = kMActorAnmReset;
				unkFE = kSentinelSlot;
			}
		}
		stopAnmSound();

		if (!skip_reload(unkFE, idx)) {
			unkFE = idx;
			if (entry->unk0 != nullptr)
				mMActor = mMActorKeeper->getMActor(entry->unk0);
		}
	}

	if (entry->unk4 != nullptr) {
		unkF8 &= ~kMapObjFlagBit_ClearOnStart;
		mMActor->setAnimation(entry->unk4, entry->unk8);
		if (unkF8 & kMapObjFlagBit_ForceClearGate)
			unkF8 &= ~kMapObjFlagBit_ClearOnStart;

		if (entry->unk10 != nullptr)
			setAnmSound(entry->unk10);
	} else {
		// No anim name → reset MActor's mtx-calc back to the model's root
		// joint. Field chain from RE (801b0b8c..0ba4):
		//   J3DMtxCalc[0x58] = mMActor->unk4/*J3DModel*/->mModelData->mJointNodePointer[0]
		// The exact 0x58 field is a concrete-subclass slot (J3DMtxCalc itself
		// is abstract); the base pointer + offset is faithful to the disasm.
		//
		// Native compat (2026-07-02, [[fileselect-startanim-mtxcalc-null]]):
		// The RE unconditionally writes into mMActor->unk8 (J3DMtxCalc*); if
		// unk8 is null it SEGVs identically to native. On hardware it isn't
		// null because MActor::setModel (MActor.cpp:114) runs early enough
		// that unk8 is populated before any startAnim("no name") reaches
		// here. Native's `TFileLoadBlock::makeBlockNormal → startAnim(0)`
		// fires from TCardLoad::changeScene BEFORE the file-block's MActor
		// has been through setModel — mMActorKeeper->getMActor returns an
		// MActor whose model has not yet been bound to its J3DModelData.
		// Skip the reset when unk8 is null: this call path is "clear the
		// currently-playing anim" (entry->unk4 == null); with no
		// currently-bound MtxCalc there is nothing to reset. The real fix
		// (initialize mMActor->unk8 upstream, i.e. wire setModel earlier in
		// the file-block init) is a separate port item tracked by
		// [[fileselect-startanim-mtxcalc-null]].
		J3DModel*     model     = mMActor->getModel();
		J3DModelData* modelData = model ? model->getModelData() : nullptr;
		J3DJoint*     rootJoint =
		    modelData ? modelData->mJointNodePointer[0] : nullptr;
		if (mMActor->unk8 && rootJoint) {
			*reinterpret_cast<J3DJoint**>(
			    reinterpret_cast<u8*>(mMActor->unk8) + 0x58)
			    = rootJoint;
		}
	}
#endif
}

void TMapObjBase::makeObjDefault()
{
	mPosition.set(mInitialPosition.x, mInitialPosition.y + mYOffset,
	              mInitialPosition.z);

	mRotation = mInitialRotation;
	mScaling  = mInitialScaling;

	mVelocity.zero();
	onLiveFlag(LIVE_FLAG_UNK10);
	if (mMActor) {
		calcRootMatrix();
		getModel()->calc();
	}
	mGroundHeight = gpMap->checkGround(mPosition, &mGroundPlane);
}

void TMapObjBase::makeObjDead()
{
	mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
	onLiveFlag(LIVE_FLAG_UNK10);

	if (unkFE != 0xffff && mMapObjData->mAnim && mMapObjData->mAnim->unk0 > 0
	    && mMapObjData->mAnim->unk4[unkFE].unk4) {
		u32 uVar6 = mMapObjData->mAnim->unk4[unkFE].unk8;
		mMActor->getFrameCtrl(uVar6)->setRate(0.0f);
		mMActor->getFrameCtrl(uVar6)->setFrame(0.0f);
		mMActor->getUnk28(uVar6)->unk0 = 0xffffffff;
		unkFE                          = 0xffff;
	}

	unk100 = 0xffff;
	onHitFlag(HIT_FLAG_NO_COLLISION);
	removeMapCollision();
	mStateTimer = 0;
	if (mHeldObject) {
		mHeldObject->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHeldObject = nullptr;
	}

	if (mHolder) {
		mHolder->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHolder = nullptr;
	}

	onLiveFlag(0xD9);
	mState = STATE_DEAD;
	if (mMActor)
		SMS_HideAllShapePacket(getModel());
}

void TMapObjBase::makeObjAppeared()
{
	offLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_UNK8);
	mVelocity.x = 0.0f;
	mVelocity.y = 0.0f;
	mVelocity.z = 0.0f;
	onLiveFlag(LIVE_FLAG_UNK10);
	mStateTimer = 0;
	offHitFlag(HIT_FLAG_NO_COLLISION);
	setObjHitData(0);
	if (unk100 != 0)
		unk100 = 0;

	if (!mMapObjData->mSound) {
		u32 sound = TMapObjGeneral::mDefaultSound.unk0[unk100];
		if (sound != 0xffffffff)
			SMSGetMSound()->startSoundActor(sound, &mPosition, 0, nullptr, 0,
			                                4);
	} else {
		u32 sound = mMapObjData->mSound->unk4->unk0[unk100];
		if (sound != 0xffffffff)
			SMSGetMSound()->startSoundActor(sound, &mPosition, 0, nullptr, 0,
			                                4);
	}

	mGroundHeight = gpMap->checkGround(mPosition, &mGroundPlane);
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (mMapObjData->mMove) {
		J3DFrameCtrl* ctrl = mMapObjData->mMove->unk8;
		ctrl->setFrame((f32)ctrl->getStart());
		ctrl->setRate(1.0f);

		mMapObjData->mMove->unk8->setRate(SMSGetAnmFrameRate());
	}

	startAnim(0);
	if (mMActor)
		SMS_ShowAllShapePacket(getModel());

	mPosition.y -= mYOffset;
	if (mMapObjData->mCollision && mMapObjData->mCollision->unk4[0].unk0 != 0) {
		f32 x = mPosition.x;
		f32 y = mPosition.y - mYOffset;
		f32 z = mPosition.z;
		mMapCollisionManager->changeCollision(0);
		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK8)) {
			MtxPtr mtx = getModel()->getAnmMtx(0);

			TMapCollisionBase* col = mMapCollisionManager->getUnk8();
			col->setMtx(mtx);
			col->setUp();
		} else {
			Mtx mtx;
			TMapCollisionManager* manager = mMapCollisionManager;
			MsMtxSetTRS(mtx, x, y, z, mRotation.x, mRotation.y, mRotation.z,
			            mScaling.x, mScaling.y, mScaling.z);

			TMapCollisionBase* col = manager->getUnk8();
			col->setMtx(mtx);
			col->setUp();
		}
	}
	mPosition.y += mYOffset;
	mState = STATE_NORMAL;
}

void TMapObjBase::moveByBck() { }

void TMapObjBase::touchBoss(THitActor* boss)
{
	boss->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

void TMapObjBase::touchEnemy(THitActor* enemy)
{
	enemy->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

void TMapObjBase::touchPlayer(THitActor* player)
{
	player->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

void TMapObjBase::touchActor(THitActor* actor)
{
	if (actor->checkActorType(ACTOR_TYPE_PLAYER))
		touchPlayer(actor);
	else if (actor->checkActorType(ACTOR_TYPE_ENEMY))
		touchEnemy(actor);
	else if (actor->checkActorType(ACTOR_TYPE_BOSS))
		touchBoss(actor);
}

void TMapObjBase::ensureTakeSituation()
{
	if (mHeldObject && mHeldObject->mHolder != this)
		mHeldObject = nullptr;

	if (mHolder && mHolder->mHeldObject != this) {
		if (mPosition.y != mGroundHeight)
			offLiveFlag(LIVE_FLAG_UNK10);

		mHolder = nullptr;
	}
}

void TMapObjBase::control()
{
	for (int i = 0; i < mColCount; ++i)
		touchActor(getCollision(i));

	if (mMapObjData->mMove) {
		TMapObjMoveData* move = mMapObjData->mMove;

		move->unk4->setFrame(move->unk8->getFrame());
		move->unk8->update();
		J3DTransformInfo info;
		move->unk4->getTransform(1, &info);
		mLinearVelocity.x
		    = info.mTranslate.x + mInitialPosition.x - mPosition.x;
		mLinearVelocity.y
		    = info.mTranslate.y + mInitialPosition.y - mPosition.y;
		mLinearVelocity.z
		    = info.mTranslate.z + mInitialPosition.z - mPosition.z;
		mRotation.x
		    = info.mRotation.x * (360.0f / 65536.0f) + mInitialRotation.x;
		mRotation.y
		    = info.mRotation.y * (360.0f / 65536.0f) + mInitialRotation.y;
		mRotation.z
		    = info.mRotation.z * (360.0f / 65536.0f) + mInitialRotation.z;
	}
}

void TMapObjBase::setGroundCollision()
{
	if (!mMapCollisionManager)
		return;
	if (mMapCollisionManager->unk8->mKind != TMapCollisionBase::KIND_MOVE)
		return;

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK2)) {
		if (mColCount == 0 && unk102 == 0)
			return;
		--unk102;
		if (mColCount != 0)
			unk102 = 4;
	}

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK8)) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		if (mMapCollisionManager->unk8)
			mMapCollisionManager->unk8->moveMtx(mtx);
	} else {
		JGeometry::TVec3<f32> pos(mPosition.x, mPosition.y - mYOffset,
		                          mPosition.z);
		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4)) {
			mMapCollisionManager->unk8->offFlag(
			    TMapCollisionBase::FLAG_UNK8000);
			mMapCollisionManager->unk8->offFlag(
			    TMapCollisionBase::FLAG_UNK4000);
			if (mMapCollisionManager->unk8)
				mMapCollisionManager->unk8->moveSRT(pos, mRotation, mScaling);
		} else {
			mMapCollisionManager->unk8->offFlag(
			    TMapCollisionBase::FLAG_UNK4000);
			if (mMapCollisionManager->unk8)
				mMapCollisionManager->unk8->moveTrans(pos);
		}
	}
}

void TMapObjBase::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (gpMarDirector->isTalkModeNow() && !gpMarDirector->isDemoModeNow()) {
		if (checkLiveFlag(LIVE_FLAG_DEAD))
			return;

		if (isActorType(0x4000003B))
			return;

		if (cue & CUE_MOVE) {
			setGroundCollision();
			cue &= ~CUE_MOVE;
		}

		if ((cue & CUE_CALC_ANIM) && mMActor) {
			if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT | LIVE_FLAG_UNK200)) {
				if (getModel()->mShapePackets->unk30 != 0)
					SMS_HideAllShapePacket(getModel());
			} else {
				if (getModel()->mShapePackets->unk30 == 0)
					SMS_ShowAllShapePacket(getModel());
			}
		}

		if (checkLiveFlag(LIVE_FLAG_UNK200))
			return;

		if (hasMapCollision())
			cue &= ~CUE_CALC_ANIM;
	}

	if (cue & CUE_MOVE) {
		if (isStateTimerEngaged())
			--mStateTimer;

		if (unk100 == 0) {
			if (!mMapObjData->mSound) {
				u32 sound = TMapObjGeneral::mDefaultSound.unk0[unk100];
				if (sound != 0xffffffff)
					SMSGetMSound()->startSoundActor(sound, &mPosition, 0,
					                                nullptr, 0, 4);
			} else {
				u32 sound = mMapObjData->mSound->unk4->unk0[unk100];
				if (sound != 0xffffffff)
					SMSGetMSound()->startSoundActor(sound, &mPosition, 0,
					                                nullptr, 0, 4);
			}
		}
		if (checkLiveFlag(LIVE_FLAG_DEAD))
			dead();
	}

	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	if (cue & CUE_DRAW)
		draw();

	if (checkLiveFlag(LIVE_FLAG_UNK4000)) {
		if (cue & CUE_CALC_ANIM)
			cue &= ~CUE_CALC_ANIM;
		if (cue & CUE_ENTRY)
			cue &= ~CUE_ENTRY;
		if (cue & CUE_CALC_VIEW)
			cue &= ~CUE_CALC_VIEW;
	}

	if (cue & CUE_CALC_ANIM) {
		calc();
		if (mMActor) {
			if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT | LIVE_FLAG_UNK200)) {
				if (getModel()->mShapePackets->unk30 != 0)
					SMS_HideAllShapePacket(getModel());
			} else {
				if (getModel()->mShapePackets->unk30 == 0)
					SMS_ShowAllShapePacket(getModel());
			}
		}
		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK100)) {
			cue &= ~CUE_CALC_ANIM;
		} else if (checkMapObjFlag(MAP_OBJ_FLAG_UNK200)) {
			if (mMActor && mMActor->curAnmEndsNext(0, nullptr))
				onMapObjFlag(MAP_OBJ_FLAG_UNK100);
		}
	}

	if (cue & CUE_ENTRY) {
		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK1000))
			cue &= ~CUE_ENTRY;
		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK800))
			cue &= ~CUE_ENTRY;
	}

	if ((cue & CUE_CALC_VIEW) && mMActor
	    && checkMapObjFlag(MAP_OBJ_FLAG_UNK400)) {
		getModel()->viewCalc();
		cue &= ~CUE_CALC_VIEW;
		requestShadow();
	}

	TLiveActor::perform(cue, graphics);
}

u32 TMapObjBase::getShadowType()
{
	if (isActorType(0x40000034) || isActorType(0x40000035)
	    || isActorType(0x40000036) || isActorType(0x40000037)
	    || isActorType(0x40000039)) {
		return 2;
	} else if (checkMapObjFlag(MAP_OBJ_FLAG_UNK2000)) {
		return 1;
	} else {
		return 0;
	}
}

Mtx* TMapObjBase::getRootJointMtx() const
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK8) || mYOffset != 0.0f)
		return (Mtx*)mMActor->getModel()->getAnmMtx(0);
	return nullptr;
}

void TMapObjBase::calcRootMatrix()
{
	J3DModel* model = getModel();
	MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x, mPosition.y - mYOffset,
	               mPosition.z, mRotation.x, mRotation.y, mRotation.z);
	model->setBaseScale(mScaling);
}

BOOL TMapObjBase::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_UNK5 && checkMapObjFlag(MAP_OBJ_FLAG_UNK40)) {
		mHeldObject = (TTakeActor*)sender;
		return true;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER)
		return touchWater(sender);

	return false;
}

void TMapObjBase::initAndRegister(const char* param_1)
{
	unkF4 = param_1;
	initMapObj();
	if (mMapObjData->unkC) {
		JDrama::TNameRefGen::search<TIdxGroupObj>(mMapObjData->unkC)
		    ->push_back(this);
	}
}

void TMapObjBase::loadAfter()
{
	TLiveActor::loadAfter();
	mGroundHeight = gpMap->checkGround(mPosition, &mGroundPlane);
}

void TMapObjBase::load(JSUMemoryInputStream& stream)
{
	TActor::load(stream);

	unkF4 = stream.readString();
	loadBeforeInit(stream);
	initMapObj();
	offMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
	makeObjAppeared();
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK80)) {
		f32 value;
		stream >> value;
		setDamageHeight(value);
		setAttackHeight(value);
		offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	}
}

TMapObjBase::TMapObjBase(const char* name)
    : TLiveActor(name)
    , unkF4(nullptr)
    , unkF8(0)
    , mState(STATE_NORMAL)
    , unkFE(0xffff)
    , unk100(0xffff)
    , unk102(0)
    , mStateTimer(0)
    , mYOffset(0.0f)
    , mMapObjData(nullptr)
    , mEventId(0)
{
	mInitialPosition.zero();
	mInitialRotation.zero();
	mInitialScaling.set(1.0f, 1.0f, 1.0f);
}
