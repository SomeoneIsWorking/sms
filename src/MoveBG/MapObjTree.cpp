#include <MoveBG/MapObjTree.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <cstdio>
#include <cmath>
#include <dolphin/mtx.h>
#include <MoveBG/MapObjGeneral.hpp>
#include <Strategic/LiveActor.hpp>
#include <Player/MarioAccess.hpp>

// TMapObjTree — trees whose leaves each carry a moving collision object.
//
// The upstream decomp has an EMPTY MapObjTree.cpp; these three bodies
// (TMapObjLeaf ctor, initEach, initMapObj) are a cold reverse-engineering of
// the US GMSE01 DOL, not a transcription of existing decomp source. Ground
// truth + full derivation: debug_journal/2026-07-15_mapobjtree_initmapobj_port_re.md
// and scratch/re/mapobjtree_port_dossier.md. Address anchors are cited inline.

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
