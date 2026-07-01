#include <MoveBG/MapObjMamma.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Strategic/MirrorActor.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>
#include <MarioUtil/MathUtil.hpp>
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
