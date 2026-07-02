#include <MoveBG/MapObjMamma.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapMirror.hpp>
#include <Map/MapStaticObject.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <cmath>
#include <Strategic/MirrorActor.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRef.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>
#include <MarioUtil/MathUtil.hpp>
#include "sms_boot_mamma_mirror.h"
#include "sms_boot_shining_stone.h"

// Native port of TShiningStone::perform (@0x801d07b4). RE: scratch/decomp_next/801d07b4.c.
// 太陽石 "sun stone" — the shining rocks in Sirena Beach (stage 6) that sparkle when Mario
// activates them. Each stone owns 4 "spoke" MActors + a main MActor body, and emits up to
// 3 sparkle-particle IDs (0x143 / 0x144 / 0x145) per spoke per frame based on activation
// state (mActivatedCount ∈ [0..3]).
//
// SDA scan (tools/dol_sda.py 0x801d07b4): no SDA2 constants; ONE r13 global
//   r13 - 0x6070  →  gpMarioParticleManager
// Cleanly identified by KNOWN_SDA1 in dol_sda.py — a first-class demonstration that the
// tool unblocks ports at zero manual disasm cost.
//
// TShiningStone::load is ported below (was previously a documented gap).

void TShiningStone::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	// Faithful to the RE: 4 iterations, per iteration delegate the spoke's perform then
	// conditionally emit up to 3 sparkle particles at THIS stone's position based on
	// mActivatedCount. The count-comparison-per-emit is unit-tested pure logic in the
	// sms_boot_shining_stone header.
	if (mSpokes != nullptr) {
		for (int i = 0; i < 4; ++i) {
			MActor* spoke = mSpokes[i];
			if (spoke) {
				spoke->perform(param_1, param_2);
			}
			// Emit sparkle particles — one per activation-state level up to 3.
			if (gpMarioParticleManager) {
				const int n = sb::shining_stone_particle_emit_count(mActivatedCount);
				for (int p = 0; p < n; ++p) {
					const s32 particleId = 0x143 + p;
					gpMarioParticleManager->emit(particleId, &mPosition, 1, this);
				}
			}
		}
	}
	// After the loop, delegate the main body's perform (visible mesh).
	if (mMainActor) {
		mMainActor->perform(param_1, param_2);
	}
}

// Native port of TShiningStone::load (@0x801d0564). RE: scratch/decomp_shining_load/801d0564.c.
// Loads the four spoke .bmd models + the main stone body + registers the 4 sparkle particle
// resources (0x143 sparkle-A / 0x144 sparkle-B / 0x145 sparkle-C / 0x56 flash). The base-TR
// matrix is built once from the object's world pos + rotation and copied into each MActor's
// J3DModel via getBaseTRMtx() — spokes and body share the same origin transform (individual
// spoke motion is driven by their BMD anims at draw time).
//
// SDA scan (tools/dol_sda.py 0x801d0564):
//   SDA2[-0x280c] = 0x43360b61 (f32 182.04444...) — degrees→s16 conversion; extracted to
//                                                   sb::shining_stone_deg_to_rot_int().
//   SDA1[-0x62b8] = gpMapObjManager  (getMActorAnmData() supplies the shared anm data)
//   SDA1[-0x5fe0] = gpResourceManager (JPAResourceManager::load registers .jpa by id)
// Particle-registration is idempotent (guarded by 4 file-scope bool latches so the .jpa is
// pulled off disc once regardless of how many stones exist in a scene).
void TShiningStone::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);

	Mtx mtx;
	MsMtxSetXYZRPH(mtx, mPosition.x, mPosition.y, mPosition.z,
	               static_cast<s16>(sb::shining_stone_deg_to_rot_int(mRotation.x)),
	               static_cast<s16>(sb::shining_stone_deg_to_rot_int(mRotation.y)),
	               static_cast<s16>(sb::shining_stone_deg_to_rot_int(mRotation.z)));

	const char* const* spoke_paths = sb::shining_stone_spoke_bmd_paths();
	mSpokes = new MActor*[4];
	for (int i = 0; i < 4; ++i) {
		mSpokes[i] = SMS_MakeMActorWithAnmData(
		    spoke_paths[i], gpMapObjManager->getMActorAnmData(), 3, 0x10020000);
		PSMTXCopy(mtx, mSpokes[i]->getModel()->getBaseTRMtx());

		TMirrorActor* mir = new TMirrorActor("太陽石in鏡");
		mir->init(mSpokes[i]->getModel(), 0x1a);
	}

	mMainActor = SMS_MakeMActorWithAnmData("/scene/mapObj/ShiningStone.bmd",
	                                       gpMapObjManager->getMActorAnmData(), 3,
	                                       0x10020000);
	mMainActor->setBpk("shiningstone");
	mMainActor->setBtk("shiningstone");
	PSMTXCopy(mtx, mMainActor->getModel()->getBaseTRMtx());

	static bool s_registered_143 = false;
	static bool s_registered_144 = false;
	static bool s_registered_145 = false;
	static bool s_registered_056 = false;
	if (!s_registered_143) {
		gpResourceManager->load("/scene/mapObj/ShiningStone1.jpa", 0x143);
		s_registered_143 = true;
	}
	if (!s_registered_144) {
		gpResourceManager->load("/scene/mapObj/ShiningStone2.jpa", 0x144);
		s_registered_144 = true;
	}
	if (!s_registered_145) {
		gpResourceManager->load("/scene/mapObj/ShiningStone3.jpa", 0x145);
		s_registered_145 = true;
	}
	if (!s_registered_056) {
		gpResourceManager->load("/scene/mapObj/ShiningStoneF.jpa", 0x56);
		s_registered_056 = true;
	}
}

// Native port of TMammaMirrorMapOperator::loadAfter (@0x801cf1b0). RE:
// scratch/decomp_mamma_mirror/801cf1b0.c. Runs once after the Sirena Beach scene has
// finished loading. Populates the 8 per-node bounding-box + threshold arrays and caches the
// 3 mirror-actor positions (mirrorS/M/L). The 4th name-lookup ("鏡内地形" = mirror-interior
// terrain) is where the 8 joints come from — its J3DModelData is walked from
// mJointNodePointer[2] via mYounger (sibling) for 8 nodes.
//
// SDA scan (tools/dol_sda.py 0x801cf1b0):
//   SDA2[-0x2864/-0x285c/-0x2854] = "mirrorS"/"mirrorM"/"mirrorL"  (name strings)
//   SDA2[-0x284c] = 0.5      (bbox half-scale)
//   SDA2[-0x2848] = 2000.0   (radius padding)
//   SDA2[-0x2844] = 3000.0   (radius cap)
//   SDA1[-0x5db8] = <unknown scene/setup container>  → we substitute JDrama::TNameRef::search
//                   which walks the same global name registry the game's own NameRefGen
//                   feeds into (functionally identical intent — resolve by name).
void TMammaMirrorMapOperator::loadAfter()
{
	TMapStaticObj* mirrorS = (TMapStaticObj*)JDrama::TNameRef::search("mirrorS");
	if (mirrorS) mMirrorSPos = mirrorS->getPosition();

	TMapStaticObj* mirrorM = (TMapStaticObj*)JDrama::TNameRef::search("mirrorM");
	if (mirrorM) mMirrorMPos = mirrorM->getPosition();

	TMapStaticObj* mirrorL = (TMapStaticObj*)JDrama::TNameRef::search("mirrorL");
	if (mirrorL) mMirrorLPos = mirrorL->getPosition();

	TMapStaticObj* mirrorTerrain
	    = (TMapStaticObj*)JDrama::TNameRef::search("鏡内地形");
	if (!mirrorTerrain) return;

	J3DModelData* md = mirrorTerrain->getModelData();
	if (!md) return;

	// Start at joints[2] (index-2), walk mYounger siblings for 8 nodes.
	J3DJoint* joint = md->getJointNodePointer(2);
	for (int i = 0; i < 8 && joint != nullptr; ++i) {
		const Vec& mn = joint->getMin();
		const Vec& mx = joint->getMax();

		mNodes[i] = joint;
		mNodeCenter[i].x = sb::mamma_node_center(mn.x, mx.x);
		mNodeCenter[i].y = sb::mamma_node_center(mn.y, mx.y);
		mNodeCenter[i].z = sb::mamma_node_center(mn.z, mx.z);
		mNodeRadius[i]   = sb::mamma_node_threshold(mn.x, mx.x, mn.z, mx.z);

		joint = static_cast<J3DJoint*>(joint->getYounger());
	}
}

// Native port of TMammaMirrorMapOperator::perform (@0x801cf46c). RE:
// scratch/decomp_mamma_mirror/801cf46c.c. Called on every gameplay tick (mask bit 2). Each
// tick: if no mirror is currently active (gpMirrorModelManager->mCurrentMirrorIndex == -1) hide all 8
// interior-terrain joints; otherwise show each joint that lies OUTSIDE the LOD radius OR
// is farther from the camera than the active mirror is. Cache the show/hide state to only
// call SMS_ShowJoint on transitions.
//
// SDA scan (tools/dol_sda.py 0x801cf46c):
//   SDA2[-0x2868] = 0.0   (guard: only Newton-refine sqrt when dsq > 0)
//   SDA2[-0x284c] = 0.5   (Newton refinement)
//   SDA2[-0x2840] = 3.0   (Newton refinement)
//   SDA1[-0x6318] = gpMirrorModelManager (mCurrentMirrorIndex = mirror index, mMirrorCamera = TMirrorCamera)
// The refined-sqrt sequence in the RE is `frsqrte + one Newton step` and converges to
// sqrt(dsq); we use `sqrtf` directly (more accurate, sub-ULP difference is irrelevant for a
// coarse-radius visibility gate).
void TMammaMirrorMapOperator::perform(u32 param_1, JDrama::TGraphics* /*graphics*/)
{
	if ((param_1 & 2) == 0) return;

	TMirrorModelManager* mgr = gpMirrorModelManager;
	if (!mgr) return;   // scenes without a mirror manager: nothing to gate

	// No mirror is currently active → hide all interior-terrain joints.
	if (mgr->mCurrentMirrorIndex == -1) {
		for (int i = 0; i < 8; ++i) {
			if (mNodeVisible[i] == 0 && mNodes[i] != nullptr) {
				SMS_ShowJoint(mNodes[i]->getMesh(), false);
				mNodeVisible[i] = 1;
			}
		}
		return;
	}

	TMirrorCamera* cam = mgr->mMirrorCamera;
	const int idx = mgr->mCurrentMirrorIndex;
	if (cam == nullptr) return;

	// Select the active mirror's cached position. The RE indexes `param_1 + idx*0xc + 0xb8`
	// which is `(&mMirrorSPos)[idx]` byte-wise; we spell the three cases out for clarity
	// and to avoid undefined pointer arithmetic across separately-declared fields.
	const JGeometry::TVec3<f32>* mirror_pos;
	switch (idx) {
		case 0:  mirror_pos = &mMirrorSPos; break;
		case 1:  mirror_pos = &mMirrorMPos; break;
		case 2:  mirror_pos = &mMirrorLPos; break;
		default: return;  // defensive: mCurrentMirrorIndex outside [-1,2] would be a game-state bug
	}

	const float mdx = cam->mReflectedPos.x - mirror_pos->x;
	const float mdy = cam->mReflectedPos.y - mirror_pos->y;
	const float mdz = cam->mReflectedPos.z - mirror_pos->z;
	const float dist_mirror_sq = mdx*mdx + mdy*mdy + mdz*mdz;
	const float dist_mirror = (dist_mirror_sq > 0.0f) ? std::sqrt(dist_mirror_sq) : 0.0f;

	for (int i = 0; i < 8; ++i) {
		if (mNodes[i] == nullptr) continue;

		const float ndx = cam->mReflectedPos.x - mNodeCenter[i].x;
		const float ndy = cam->mReflectedPos.y - mNodeCenter[i].y;
		const float ndz = cam->mReflectedPos.z - mNodeCenter[i].z;
		const float dist_node_sq = ndx*ndx + ndy*ndy + ndz*ndz;
		const float dist_node = (dist_node_sq > 0.0f) ? std::sqrt(dist_node_sq) : 0.0f;

		if (sb::mamma_node_should_be_hidden(dist_node, mNodeRadius[i], dist_mirror)) {
			if (mNodeVisible[i] == 0) {
				SMS_ShowJoint(mNodes[i]->getMesh(), false);
				mNodeVisible[i] = 1;
			}
		} else {
			if (mNodeVisible[i] != 0) {
				SMS_ShowJoint(mNodes[i]->getMesh(), true);
				mNodeVisible[i] = 0;
			}
		}
	}
}

// Native port of TSandLeaf::control (@0x801d358c). RE: scratch/decomp_next5/801d358c.c.
// すなやまの芽 — Sirena Beach's sandcastle leaves. Per-tick: the leaf's world Y is
// re-snapped to the ground beneath its current XZ, so it stays visually attached even as
// the sand terrain shifts (a manor of sand shifting during Mecha-Bowser fight). The check
// starts 200 units above the current y to avoid a spurious "already below ground" reading.
//
// SDA scan (tools/dol_sda.py 0x801d358c):
//   SDA2[-0x2758] = 200.0f  (upward start offset for the ground raycast)
//   SDA1[-0x6328] = gpMap   (checkGround target)
void TSandLeaf::control()
{
	TMapObjBase::control();
	mGroundHeight = gpMap->checkGround(mPosition.x, mPosition.y + 200.0f,
	                                    mPosition.z, &mGroundPlane);
	mPosition.y = mGroundHeight;
}
