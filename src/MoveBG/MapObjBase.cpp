#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjGeneral.hpp>
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
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <System/MarDirector.hpp>

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
		if (checkMapObjFlag(0x100)) {
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
	unk64 &= ~1;
	setUpCurrentMapCollision();
}

void TMapObjBase::sleep()
{
	onLiveFlag(LIVE_FLAG_UNK4000);
	unk64 |= 1;
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
		f32 fVar2     = mScaling.x > mScaling.z ? mScaling.x : mScaling.z;
		f32 fVar3     = mScaling.y;
		mAttackRadius = table->unk0 * fVar2;
		mAttackHeight = table->unk4 * fVar3;
		mDamageRadius = table->unk8 * fVar2;
		mDamageHeight = table->unkC * fVar3;
		calcEntryRadius();
	}
}

void TMapObjBase::removeMapCollision()
{
	if (!mMapCollisionManager)
		return;

	if (!mMapCollisionManager->unk8)
		return;

	if (mMapCollisionManager->unk8 && mMapCollisionManager)
		mMapCollisionManager->unk8->remove();
}

void TMapObjBase::setUpCurrentMapCollision()
{
	TMapCollisionManager* colman = mMapCollisionManager;
	if (!colman)
		return;

	if (checkMapObjFlag(8)) {
		J3DModel* model        = getModel();
		TMapCollisionBase* col = mMapCollisionManager->getUnk8();
		MTXCopy(model->getAnmMtx(0), col->unk20);
		col->setUp();
	} else {
		Mtx mtx;
		JGeometry::TVec3<f32> pos(mPosition.x, mPosition.y - mYOffset,
		                          mPosition.z);
		MsMtxSetTRS(mtx, pos.x, pos.y, pos.z, mRotation.x, mRotation.y,
		            mRotation.z, mScaling.x, mScaling.y, mScaling.z);
		TMapCollisionBase* col = colman->getUnk8();
		MTXCopy(mtx, col->unk20);
		col->setUp();
	}
}

void TMapObjBase::setUpMapCollision(u16 param_1)
{
	if (!mMapObjData->mCollision
	    || !mMapObjData->mCollision->unk4[param_1].unk0)
		return;

	JGeometry::TVec3<f32> pos(mPosition.x, mPosition.y - mYOffset, mPosition.z);

	mMapCollisionManager->changeCollision(param_1);

	if (checkMapObjFlag(8)) {
		J3DModel* model        = getModel();
		TMapCollisionBase* col = mMapCollisionManager->getUnk8();
		MTXCopy(model->getAnmMtx(0), col->unk20);
		col->setUp();
	} else {
		Mtx mtx;
		TMapCollisionManager* colman = mMapCollisionManager;
		MsMtxSetTRS(mtx, pos.x, pos.y, pos.z, mRotation.x, mRotation.y,
		            mRotation.z, mScaling.x, mScaling.y, mScaling.z);
		TMapCollisionBase* col = colman->getUnk8();
		MTXCopy(mtx, col->unk20);
		col->setUp();
	}
}

void TMapObjBase::soundBas(u32 param_1, f32 param_2, f32 param_3)
{
	f32 currFrame = mMActor->getFrameCtrl(0)->getFrame();
	if (currFrame <= param_2 && param_2 < currFrame + param_3) {
		if (gpMSound->gateCheck(param_1))
			MSoundSESystem::MSoundSE::startSoundActor(param_1, getPosition(), 0,
			                                          nullptr, 0, 4);
	}
}

void TMapObjBase::startSound(u16 param_1)
{
	if (unk100 != param_1)
		unk100 = param_1;

	if (!mMapObjData->mSound) {
		u32 uVar3 = TMapObjGeneral::mDefaultSound.unk0[unk100];
		if (uVar3 != 0xffffffff && gpMSound->gateCheck(uVar3))
			MSoundSESystem::MSoundSE::startSoundActor(uVar3, mPosition, 0,
			                                          nullptr, 0, 4);
	} else {
		u32 uVar3 = mMapObjData->mSound->unk4->unk0[unk100];
		if (uVar3 != 0xffffffff && gpMSound->gateCheck(uVar3))
			MSoundSESystem::MSoundSE::startSoundActor(uVar3, mPosition, 0,
			                                          nullptr, 0, 4);
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
	unkF8 &= ~0x100;
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
	mPosition.x = mInitialPosition.x;
	mPosition.y = mInitialPosition.y + mYOffset;
	mPosition.z = mInitialPosition.z;

	mRotation = mInitialRotation;
	mScaling  = mInitialScaling;

	mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
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
	mLiveFlag |= 0x10;

	if (unkFE != 0xffff && mMapObjData->mAnim && mMapObjData->mAnim->unk0 > 0
	    && mMapObjData->mAnim->unk4[unkFE].unk4) {
		u32 uVar6 = mMapObjData->mAnim->unk4[unkFE].unk8;
		mMActor->getFrameCtrl(uVar6)->setRate(0.0f);
		mMActor->getFrameCtrl(uVar6)->setFrame(0.0f);
		mMActor->getUnk28(uVar6)->unk0 = 0xffffffff;
		unkFE                          = 0xffff;
	}

	unk100 = 0xffff;
	unk64 |= 1;
	removeMapCollision();
	mTimeTilAppear = 0;
	if (mHeldObject) {
		mHeldObject->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHeldObject = nullptr;
	}

	if (mHolder) {
		mHolder->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHolder = nullptr;
	}

	onLiveFlag(0xD9);
	mState = 0;
	if (mMActor)
		SMS_HideAllShapePacket(getModel());
}

void TMapObjBase::makeObjAppeared() { }

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
			mLiveFlag &= ~0x10;

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

void TMapObjBase::setGroundCollision() { }

// Native port of TMapObjBase::perform (@0x801afb48, 1068B). This is the
// per-frame dispatcher inherited by ALL map-object actors — palm trees,
// coin blocks, cover fruit, warp pipes, etc. Was empty { } before.
//
// Vtable slots RE'd from __vt__11TMapObjBase (found at 0x803C2AB8 via
// __ct__11TMapObjBase); byte offsets into the vtable pointer:
//   0xC4 → control()                  (TMapObjBase override)
//   0xD4 → requestShadow()            (inherited from TLiveActor)
//   0xEC → hasMapCollision() const    (inherited from TLiveActor)
//   0x138 → calc()                    (TMapObjBase override — empty stub)
//   0x13C → draw() const              (TMapObjBase override — empty stub)
//   0x140 → dead()                    (TMapObjBase override — empty stub)
//
// The J3DModel* vtable-slot-0x1c call at 0x801aff20 is a subclass-specific
// virtual (base J3DModel has only 5 virtuals; slot 0x1c is beyond that).
// Preserved as a raw vtable-offset call — a runtime subclass MActor holds
// implements it and picks the semantic. Not guessing which named virtual
// it maps to.
//
// _DAT_803c2c58 is a file-static u32 in TMapObjBase.cpp's TU that gates a
// per-frame sound cue. It's initialized to -1 (no sound) in .data and
// specific actors set it to a sound id to trigger a one-shot dispatch.
namespace {
static u32 s_pendingSoundId = 0xFFFFFFFFu; // .data @0x803c2c58
} // namespace

void TMapObjBase::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	u8 marState = gpMarDirector->unk124;

	// TALK-MODE MAIN BLOCK: only runs when unk124 IS 1 or 2 (talk mode).
	// The double-negative structure in the decomp collapses to:
	//   "enter iff talk-mode; the inner demo-check never fires because
	//   1/2 and 3/4 are disjoint".
	if (marState == 1 || marState == 2) {
		if (mLiveFlag & LIVE_FLAG_DEAD)
			return;
		if (mActorType == 0x4000003b) // specific "no-perform" actor type
			return;

		if (param_1 & 1) {
			control();
			param_1 &= ~1u;
		}

		if ((param_1 & 2) && mMActor != nullptr) {
			J3DModel* model = getModel();
			// The show/hide decision reads a byte at
			// (model->mShapePackets[0] + 0x30). ShapePacket's +0x30
			// stores the show/hide flag flipped by SMS_ShowAll /
			// SMS_HideAll. 0 = shown, non-0 = hidden.
			u8 packetHiddenFlag = *((u8*)model->getShapePacket(0) + 0x30);
			if ((mLiveFlag & 0x204) == 0) {
				if (packetHiddenFlag == 0)
					SMS_ShowAllShapePacket(model);
			} else {
				if (packetHiddenFlag != 0)
					SMS_HideAllShapePacket(model);
			}
		}

		if (mLiveFlag & 0x200)
			return;

		if (hasMapCollision())
			param_1 &= ~2u;
	}

	// SECOND BLOCK: runs when not in talk mode OR after talk-mode block
	// completes. First: movement phase — decrement mTimeTilAppear and
	// dispatch the file-static sound cue (if any).
	if (param_1 & 1) {
		if (mTimeTilAppear > 0)
			--mTimeTilAppear;

		if (unk100 == 0) {
			u32 soundId;
			const TMapObjSoundInfo* soundOverride = mMapObjData->mSound;
			if (soundOverride == nullptr) {
				soundId = s_pendingSoundId;
			} else {
				// mMapObjData->mSound->unk4->unk0[0] — per-actor sound id
				soundId = soundOverride->unk4->unk0[0];
			}
			if (soundId != 0xFFFFFFFFu && gpMSound->gateCheck(soundId)) {
				MSoundSESystem::MSoundSE::startSoundActor(
				    soundId, &mPosition, 0, nullptr, 0, 4);
			}
		}

		if (mLiveFlag & 1)
			dead();
	}

	if ((mLiveFlag & 1) == 0) {
		if (param_1 & 8)
			draw();

		if (mLiveFlag & 0x4000) {
			if (param_1 & 2)     param_1 &= ~2u;
			if (param_1 & 0x200) param_1 &= ~0x200u;
			if (param_1 & 4)     param_1 &= ~4u;
		}

		if (param_1 & 2) {
			calc();

			if (mMActor != nullptr) {
				J3DModel* model = getModel();
				u8 packetHiddenFlag
				    = *((u8*)model->getShapePacket(0) + 0x30);
				if ((mLiveFlag & 0x204) == 0) {
					if (packetHiddenFlag == 0)
						SMS_ShowAllShapePacket(model);
				} else {
					if (packetHiddenFlag != 0)
						SMS_HideAllShapePacket(model);
				}
			}

			if ((unkF8 & 0x100) == 0) {
				if ((unkF8 & 0x200) && mMActor != nullptr
				    && mMActor->curAnmEndsNext(0, nullptr)) {
					unkF8 |= 0x100;
				}
			} else {
				param_1 &= ~2u;
			}
		}

		if (param_1 & 0x200) {
			if (unkF8 & 0x1000) param_1 &= ~0x200u;
			if (unkF8 & 0x800)  param_1 &= ~0x200u;
		}

		if ((param_1 & 4) && mMActor != nullptr && (unkF8 & 0x400)) {
			J3DModel* model = getModel();
			// Raw vtable dispatch at byte offset 0x1c — a subclass
			// virtual beyond base J3DModel's 5 documented virtuals.
			typedef void (*Vt1cFn)(J3DModel*);
			Vt1cFn fn = *(Vt1cFn*)((*(u8**)model) + 0x1c);
			fn(model);
			param_1 &= ~4u;
			requestShadow();
		}

		TLiveActor::perform(param_1, param_2);
	}
}

// Empty TMapObjBase virtuals — byte-exact `blr` in the binary. Ports for
// vtable slot fill; concrete behavior lives in derived-class overrides.
void TMapObjBase::calc() { }
void TMapObjBase::draw() const { }
void TMapObjBase::dead() { }

u32 TMapObjBase::getShadowType()
{
	if (isActorType(0x40000034) || isActorType(0x40000035)
	    || isActorType(0x40000036) || isActorType(0x40000037)
	    || isActorType(0x40000039)) {
		return 2;
	} else if (checkMapObjFlag(0x2000)) {
		return 1;
	} else {
		return 0;
	}
}

Mtx* TMapObjBase::getRootJointMtx() const
{
	if (checkMapObjFlag(0x8) || mYOffset != 0.0f)
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
	if (message == HIT_MESSAGE_UNK5 && checkMapObjFlag(0x40)) {
		mHeldObject = (TTakeActor*)sender;
		return 1;
	} else if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		return touchWater(sender);
	} else {
		return false;
	}
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
	unkF8 &= ~0x40000;
	makeObjAppeared();
	if (checkMapObjFlag(0x80)) {
		f32 value;
		stream.read(&value, 4);
		mDamageHeight = value;
		calcEntryRadius();
		mAttackHeight = value;
		calcEntryRadius();
		unk64 &= ~4;
		unk64 &= ~2;
	}
}

TMapObjBase::TMapObjBase(const char* name)
    : TLiveActor(name)
    , unkF4(nullptr)
    , unkF8(0)
    , mState(1)
    , unkFE(0xffff)
    , unk100(0xffff)
    , unk102(0)
    , mTimeTilAppear(0)
    , mYOffset(0.0f)
    , mMapObjData(nullptr)
    , unk134(0)
{
	mInitialPosition.zero();
	mInitialRotation.zero();
	mInitialScaling.set(1.0f, 1.0f, 1.0f);
}
