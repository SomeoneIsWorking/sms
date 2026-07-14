#include <MoveBG/MapObjTree.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <cstdio>

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
