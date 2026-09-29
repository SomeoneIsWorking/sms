#include <MoveBG/MapObjTree.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapData.hpp> // TBGCheckData - Mario's ground plane (touchPlayer)
#include <Map/MapEventSink.hpp>
#include <Map/PollutionManager.hpp>
#include <Camera/CameraShake.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <macros.h> // ARRAY_COUNT
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MSound/MSound.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <cstdio>
#include <cmath>
#include <dolphin/mtx.h>
#include <MoveBG/MapObjGeneral.hpp>
#include <Strategic/LiveActor.hpp>
#include <Player/MarioAccess.hpp>

// Frames between scale-tree dust puffs (upstream src/MoveBG/MapObjTree.cpp:30).
static int sWaitTime = 1;

// Scale-tree species constants. Restored 2026-09-29 from upstream
// (src/MoveBG/MapObjTree.cpp:31-34), lost with the TMapObjTreeScale bodies
// below in the 2026-09-28 merge d13cd0284: `git show
// d13cd0284^2:src/MoveBG/MapObjTree.cpp` carries all four, `git show
// d13cd0284:...` carries none. Without them the class's control() has nothing
// to scale by.
//
// mBananaTreeJumpPower is the same casualty and is LIVE here:
// src/Map/MapData.cpp:13 reads it and nothing in this fork defines it.
// mNearMiddle / mMiddleFar are declared in the header, read nowhere, and are
// not defined upstream either - left alone.
f32 TMapObjTreeScale::mScaleMin           = 0.1f;
f32 TMapObjTreeScale::mScaleSpeedXZ       = 0.007f;
f32 TMapObjTreeScale::mStatusChangeScaleY = 0.3f;
f32 TMapObjTreeScale::mScaleSpeedY        = 0.005f;
f32 TMapObjTree::mBananaTreeJumpPower     = 1000.0f;

// TMapObjTree — trees whose leaves each carry a moving collision object.
//
// The TMapObjLeaf ctor, initEach, initMapObj, controlLeaf, perform and the two bodies
// below are a cold reverse-engineering of the US GMSE01 DOL, not a transcription of
// existing decomp source. Ground truth + full derivation:
// debug_journal/2026-07-15_mapobjtree_initmapobj_port_re.md and
// scratch/re/mapobjtree_port_dossier.md. Address anchors are cited inline.
//
// (Corrected 2026-09-28: the comment here used to claim "the upstream decomp has an
// EMPTY MapObjTree.cpp". That premise, from eb2fd714, is stale — upstream/main ships
// 344 lines, including the TMapObjTree ctor and touchPlayer that eb2fd714 dropped
// along with the rest. Those two are recovered below from upstream, mapped onto this
// fork's field names.)

// TMapObjTree ctor. Restored 2026-09-28 from upstream (src/MoveBG/MapObjTree.cpp:196),
// which zero-initialises every species/leaf field and starts the leaf sway frozen.
// Field mapping (upstream -> this fork), all confirmed against initEach below, which
// assigns the same five constants to the same five fields:
//   mMinCanopyRadius    -> unk148     mLeafTouchImpulse   -> unk15C
//   mMaxCanopyRadius    -> unk14C     mLeafHipDropImpulse -> unk160
//   mLeafNum            -> mLeafCount mLeafStiffness      -> unk164
//   mLeaves             -> mLeaves    mLeafDamping        -> unk168
//   mFreezeLeaves       -> unk158
// Upstream also zeroes a 0x16C field (unk16C) that this fork's header does not declare
// and that nothing here reads; it is not reconstructed.
TMapObjTree::TMapObjTree(const char* name)
    : TMapObjGeneral(name)
    , unk148(0.0f)
    , unk14C(0.0f)
    , mLeafCount(0)
    , mLeaves(nullptr)
    , unk158(1) // leaves start frozen; the first touchPlayer unfreezes them
    , unk15C(0.0f)
    , unk160(0.0f)
    , unk164(0.0f)
    , unk168(0.0f)
{
}

// Element ctor @0x801f6ef4: zero the two leading f32s, identity the joint
// matrix, and allocate a default TMapCollisionMove into +8. (initMapObj later
// overwrites +8 with a freshly configured collision — that is a faithful
// reproduction of the original's benign one-time leak, not a bug we added.)
TMapObjLeaf::TMapObjLeaf()
{
	unk0 = 0.0f;
	unk4 = 0.0f;
	PSMTXIdentity(mMtx);
	mCollision = new TMapCollisionMove();
}

// TMapObjTree::touchPlayer. Restored 2026-09-28 from upstream (src/MoveBG/MapObjTree.cpp:46),
// dropped by our eb2fd714. Any player touch unfreezes the leaf sway (unk158 = 0), and
// when the player is actually standing on THIS tree's collision the leaf under Mario gets
// an angular-velocity kick: bigger for a hip attack than for standing on it.
//
// The same field mapping as the ctor above, with upstream's mLeafTouchImpulse -> unk15C
// and mLeafHipDropImpulse -> unk160 (the values initEach writes match upstream's
// mLeafTouchImpulse / mLeafHipDropImpulse arm-for-arm). The ground-plane data index
// reaches the leaf array directly, which is why TBGCheckData::getData()/getActor() are
// the two accessors used here.
void TMapObjTree::touchPlayer(THitActor* /* player */)
{
	unk158 = 0;

	const TBGCheckData* groundPlane = SMS_GetMarioGroundPlane();
	s16 data = groundPlane->getData();
	if (groundPlane->getActor() != this || data < 0 || data >= mLeafCount)
		return;

	if (marioHipAttack())
		mLeaves[data].unk4 += unk160;
	else if (marioIsOn())
		mLeaves[data].unk4 += unk15C;
}

// initEach @0x801f6a64: flat switch on THitActor::mActorType (0x4C) selecting
// this species' leaf count + spread/growth constants. The SDA2 float literals
// were resolved with tools/dol_sda.py --sda2. Actor types outside
// 0x40000034..0x40000039 fall through untouched (the DOL blr/bgelr arms).
void TMapObjTree::initEach()
{
	switch (mActorType) {
	case 0x40000034: // shares the 0x38 (palm) arm @0x801f6aac
	case 0x40000038:
		unk148     = 20.0f;
		unk14C     = 95.0f;
		mLeafCount = 12;
		unk15C     = 0.001f;
		unk160     = 0.006f;
		unk164     = 0.01f;
		unk168     = 0.97f;
		break;
	case 0x40000035: // arm @0x801f6ae8
		unk148     = 20.0f;
		unk14C     = 100.0f;
		mLeafCount = 8;
		unk15C     = 0.001f;
		unk160     = 0.006f;
		unk164     = 0.01f;
		unk168     = 0.97f;
		break;
	case 0x40000036: // arm @0x801f6b24
		unk148     = 50.0f;
		unk14C     = 100.0f;
		mLeafCount = 12;
		unk15C     = 0.001f;
		unk160     = 0.006f;
		unk164     = 0.01f;
		unk168     = 0.97f;
		break;
	case 0x40000037: // arm @0x801f6b60
		unk148     = 95.0f;
		unk14C     = 60.0f;
		mLeafCount = 8;
		unk15C     = 0.001f;
		unk160     = 0.006f;
		unk164     = 0.01f;
		unk168     = 0.97f;
		break;
	case 0x40000039: // arm @0x801f6b9c
		unk148     = 70.0f;
		unk14C     = 100.0f;
		mLeafCount = 8;
		unk15C     = 0.004f;
		unk160     = 0.008f;
		unk164     = 0.03f;
		unk168     = 0.9f;
		break;
	default:
		break;
	}
}

// initMapObj @0x801f68b4: base init + initEach, then build the per-leaf
// collision array. Each leaf gets a TMapCollisionMove named
// "/mapObj/palmLeaf%02d" (palm, mActorType 0x40000038) or "/mapObj/%sLeaf%02d"
// (all others, %s = the object's own name at unkF4), initialized against the
// model's joint matrices [mLeafCount .. 1] (joint 0, the root, is skipped).
void TMapObjTree::initMapObj()
{
	TMapObjGeneral::initMapObj();
	initEach();

	mLeaves = new TMapObjLeaf[mLeafCount];

	for (int i = 0; i < mLeafCount; ++i) {
		TMapObjLeaf* leaf = &mLeaves[i];

		// Faithful re-alloc: the array ctor already put a collision at +8;
		// the original replaces it here with the configured one.
		leaf->mCollision = new TMapCollisionMove();

		char name[0x100];
		if (mActorType == 0x40000038)
			std::snprintf(name, sizeof(name), "/mapObj/palmLeaf%02d", i + 1);
		else
			std::snprintf(name, sizeof(name), "/mapObj/%sLeaf%02d", unkF4, i + 1);

		leaf->mCollision->init(name, 0, this);   // vtable +8 -> Move::init
		leaf->mCollision->setAllData((s16)i);
		leaf->mCollision->remove();               // vtable +0x20 -> mark NEEDS_SETUP

		// Ride the joint matrix (mNodeMatrices[mLeafCount - i]) into both the
		// leaf record and the collision object, then activate it.
		MtxPtr joint = getModel()->getAnmMtx(mLeafCount - i);
		PSMTXCopy(joint, leaf->mMtx);
		PSMTXCopy(leaf->mMtx, leaf->mCollision->unk20);
		leaf->mCollision->setUp();                // vtable +0x18 -> clear NEEDS_SETUP
	}

	if (mMapCollisionManager)
		mMapCollisionManager->unk10 = nullptr;
}

// controlLeaf @0x801f6c74 (US GMSE01, size 0x1BC) - cold RE from the DOL.
// Per-frame spring-damper sway for ONE leaf. Returns true when the leaf has
// SETTLED (|angle| < unk15C), false while still swinging; caller
// TMapObjTree::perform @0x801f6bd8 sums these and latches unk158 once all
// leaves report settled. A resting leaf (vel==0) always reports settled.
// Add includes: <Player/MarioAccess.hpp> (gpMarioSpeedY) and <cmath> (fabsf).
// Header decl must change void -> bool (see notes).
int TMapObjTree::controlLeaf(int index)
{
	TMapObjLeaf* leaf = &mLeaves[index];

	// Resting leaf: no integration, just keep the collision matrix synced.
	if (leaf->unk4 == 0.0f) {
		// Refresh collision only when Mario is not rising (fcmpo+cror(lt|eq) => <=0).
		if (*gpMarioSpeedY <= 0.0f) {
			Mtx tmp;
			PSMTXCopy(leaf->mMtx, tmp);      // DOL @0x8009544c paired-single Mtx copy
			leaf->mCollision->moveMtx(tmp);  // vtable +0x14 = moveMtx(MtxPtr)
		}
		return true;
	}

	// Spring-damper integrator (fadds / fnmsubs / fmuls).
	leaf->unk0 += leaf->unk4;                        // angle += vel
	leaf->unk4  = leaf->unk4 - leaf->unk0 * unk164;  // vel -= angle * stiffness
	leaf->unk4  = leaf->unk4 * unk168;               // vel *= damping

	// Rotation about local +X by the current angle.
	Mtx rot;
	Vec axis = { 1.0f, 0.0f, 0.0f };
	PSMTXRotAxisRad(rot, &axis, leaf->unk0);

	// swayed = leafMtx * rot (in-place dst==srcA, as in the DOL).
	Mtx swayed;
	PSMTXCopy(leaf->mMtx, swayed);
	PSMTXConcat(swayed, rot, swayed);

	// Write into the model joint matrix (mNodeMatrices[mLeafCount - index]).
	PSMTXCopy(swayed, getModel()->getAnmMtx(mLeafCount - index));

	// Refresh collision only when Mario is not rising.
	if (*gpMarioSpeedY <= 0.0f)
		leaf->mCollision->moveMtx(swayed);

	// Settled once the swing amplitude drops below the threshold.
	return fabsf(leaf->unk0) < unk15C;
}

// TMapObjTree::perform @US 0x801f6bd8 (JP 0x801CE3BC), size 0x9C.
// Per-frame: on the logic pass, drive every leaf's sway via controlLeaf and,
// once all leaves have come to rest AND nothing is colliding with the tree,
// latch the tree as "settled" so leaf control stops running. Then delegate to
// the parent for the normal MapObjGeneral perform-list dispatch (calc/entry/
// draw). Cold-RE'd from the US GMSE01 DOL; see structure below.
//
// Guest field map (accessed by NAME on host — guest offsets are for provenance):
//   this+0x158 unk158     : "settled" latch. Retail reads/writes only the top
//                           byte (lbz/stb) as a 0/1 flag; declared u32 in the
//                           header, used purely as a boolean here.
//   param_1 & 1           : logic/calc pass flag (same bit MapObjGeneral tests).
//   this+0x150 mLeafCount : leaf count (s32).
//   this+0x48  mColCount  : THitActor::mColCount (u16) — live collision count.
//   controlLeaf(i)        : returns 1 if leaf i is at rest, else 0 (summed).
void TMapObjTree::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	// Run leaf control only while unsettled, and only on the logic pass.
	if (unk158 == 0 && (param_1 & 1)) {
		int settledCount = 0;
		for (int i = 0; i < mLeafCount; ++i)
			settledCount += controlLeaf(i);

		// Latch "settled" once every leaf reports at-rest and no collisions
		// are registered against the tree (mColCount == 0). controlLeaf keeps
		// getting called each frame until this latches.
		if (mColCount == 0 && settledCount == mLeafCount)
			unk158 = 1;
	}

	TMapObjGeneral::perform(param_1, param_2);
}

// ---------------------------------------------------------------------------
// TMapObjTreeScale - the Bianco/Delfino trees that grow when the water is
// clean.
// ---------------------------------------------------------------------------
//
// The whole class body is restored 2026-09-29 from upstream
// (src/MoveBG/MapObjTree.cpp: ctor 336, startScaleUp 211, touchWater 219,
// control 230, beSmall 303, loadAfter 321), which this fork lost in the
// 2026-09-28 merge d13cd0284 - present at d13cd0284^2, absent from d13cd0284 -
// while keeping the declarations. It is the one hole in this pass with a LIVE
// call site: src/System/MarNameRefGen_MapObj.cpp:232 does `return new
// TMapObjTreeScale;`, so the constructor and, through it, the three virtual
// overrides (control/touchWater/loadAfter) that the vtable names had no
// definition in this tree.
//
// FIELD MAPPING: none needed. This fork's include/MoveBG/MapObjTree.hpp:56-79
// and upstream's include/MoveBG/MapObjTree.hpp spell TMapObjTreeScale
// identically - the same three state constants (STATE_SMALL 0xB,
// STATE_SCALING_UP_Y_ONLY 0xC, STATE_SCALING_UP 0xD) at the same offsets
// (mParticlePositions 0x170[30], mNextFreeParticlePos 0x2D8, mParticleEmitTimer
// 0x2DC, unk2E0 0x2E0) and the same four statics. The three base-class members
// the bodies touch (mState, from TMapObjBase +0xFC; mPosition and mScaling,
// from JDrama::TActor/TPlacement) keep their names in this fork too.
//
// The lifecycle is upstream's: a polluted map (or Delfino, map 4) loads the
// tree already small, and beSmall() hides its shape packets; clean water starts
// the Y-only growth, and the growth is finished in control() by turning the
// tree into a normal, collidable map object.

// beSmall: the "just spawned under water" state. Shrink to the species minimum,
// go to sleep (no control pass), take no collisions, become a non-attackable
// solid, and hide every shape packet so nothing of the tree is drawn while it
// is under water.
void TMapObjTreeScale::beSmall()
{
	mScaling.set(mScaleMin, mScaleMin, mScaleMin);
	sleep();
	offHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	setObjHitData(0);
	mDamageRadius = mAttackRadius;
	calcEntryRadius();
	mDamageHeight = 30.0f;
	calcEntryRadius();
	removeMapCollision();
	offMapObjFlag(MAP_OBJ_FLAG_UNK100);
	mActorType = 0x4000003B;
	mState     = STATE_SMALL;
	SMS_HideAllShapePacket(getModel());
}

// startScaleUp: the transition out of STATE_SMALL. Wake the object back up,
// give it the grown-tree actor type, drop its map collision (it has none until
// the growth completes) and enter the Y-only growth step.
void TMapObjTreeScale::startScaleUp()
{
	awake();
	mActorType = 0x40000039;
	removeMapCollision();
	mState = STATE_SCALING_UP_Y_ONLY;
}

// touchWater: a fully grown tree keeps the general map object's water reaction;
// a scaled-down one starts growing instead of being pushed around.
u32 TMapObjTreeScale::touchWater(THitActor* water)
{
	if (mScaling.x == 1.0f)
		return TMapObjGeneral::touchWater(water);

	if (isState(STATE_SMALL))
		startScaleUp();

	return 1;
}

// control: the growth driver. STATE_SMALL waits for clean water (never on
// Delfino, where map 4 is permanently polluted); STATE_SCALING_UP_Y_ONLY raises
// only Y until it passes the species' status-change scale; STATE_SCALING_UP
// then brings X and Z up and, on completion, hands the tree back to the map as
// a normal object. While either growth step is running the tree keeps its hit
// data empty, and outside the demo-mode carve-out it rumbles the camera and
// puffs dust from a ring buffer of positions.
void TMapObjTreeScale::control()
{
	switch (mState) {
	case STATE_SMALL:
		if (SMSGetMarDirector()->getCurrentMap() != 4
		    && !gpPollution->isPolluted(mPosition.x, mPosition.y, mPosition.z))
			startScaleUp();
		break;

	case STATE_SCALING_UP_Y_ONLY:
		SMSGetMSound()->startSoundActor(MSD_SE_OBJ_TREE_APPEAR, &mPosition, 0,
		                                nullptr, 0, 4);
		mScaling.y += mScaleSpeedY;
		if (mScaling.y > mStatusChangeScaleY)
			mState = STATE_SCALING_UP;
		break;

	case STATE_SCALING_UP:
		SMSGetMSound()->startSoundActor(MSD_SE_OBJ_TREE_APPEAR, &mPosition, 0,
		                                nullptr, 0, 4);
		if (mScaling.y < 1.0f)
			mScaling.y += mScaleSpeedY;
		else
			mScaling.y = 1.0f;

		if (mScaling.x < 1.0f) {
			mScaling.x += mScaleSpeedXZ;
			mScaling.z += mScaleSpeedXZ;
		} else {
			mScaling.x = 1.0f;
			mScaling.z = 1.0f;
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);
			getModel()->calc();
			offHitFlag(HIT_FLAG_CANNOT_ATTACK);
			setUpCurrentMapCollision();
			mState = STATE_NORMAL;
		}
		break;

	default:
		TMapObjGeneral::control();
		break;
	}

	if (isState(STATE_SCALING_UP_Y_ONLY) || isState(STATE_SCALING_UP)) {
		setObjHitData(0);
		if (SMSGetMarDirector()->getCurrentMap() != 2
		    || (!SMSGetMarDirector()->isDemoModeNow()
		        && (unk2E0 == nullptr || unk2E0->isBuried(1)))) {
			SMSRumbleMgr->start(0x13, &mPosition);
			gpCameraShake->keepShake(CAM_SHAKE_MODE_UNK5, 1.0f);
		}

		if (mParticleEmitTimer > sWaitTime) {
			// circular buffer of particle positions
			mParticlePositions[mNextFreeParticlePos].set(
			    mPosition.x + 400.0f * MsRandF() - 200.0f, mPosition.y,
			    mPosition.z + 400.0f * MsRandF() - 200.0f);

			gpMarioParticleManager->emit(
			    PARTICLE_MS_RAKU_KIE, &mParticlePositions[mNextFreeParticlePos],
			    2, this);
			++mNextFreeParticlePos;
			if (mNextFreeParticlePos >= ARRAY_COUNT(mParticlePositions))
				mNextFreeParticlePos = 0;
			mParticleEmitTimer = 0;
		}
		++mParticleEmitTimer;
	}
}

// loadAfter: base load, then the map's own state decides whether the tree is
// born small, and the named event object for the sinking terrain is looked up.
// The name is upstream's, spelled in the game archive: "Event (Bianco terrain
// sinking)".
void TMapObjTreeScale::loadAfter()
{
	TMapObjGeneral::loadAfter();

	if (SMSGetMarDirector()->getCurrentMap() == 4
	    || gpPollution->isPolluted(mPosition.x, mPosition.y, mPosition.z)) {
		beSmall();
	}

	unk2E0 = (TMapEventSink*)JDrama::TNameRefGen::getInstance()
	             ->getRootNameRef()
	             ->search("イベント（地形沈むビアンコ）");
}

// The scale tree is a plain TMapObjTree plus its own particle ring and its
// event reference; the leaf array and per-species constants are the base
// class's.
TMapObjTreeScale::TMapObjTreeScale(const char* name)
    : TMapObjTree(name)
    , mNextFreeParticlePos(0)
    , mParticleEmitTimer(0)
    , unk2E0(nullptr)
{
	for (int i = 0; i < ARRAY_COUNT(mParticlePositions); ++i)
		mParticlePositions[i].zero();
}
