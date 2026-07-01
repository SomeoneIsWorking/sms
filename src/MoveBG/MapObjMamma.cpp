#include <MoveBG/MapObjMamma.hpp>
#include <M3DUtil/MActor.hpp>
#include <System/EmitterViewObj.hpp>
#include <JSystem/JGeometry.hpp>
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
// DOCUMENTED GAP (kept honest per no-bandaid rule): TShiningStone::load is still an empty
// stub in ring3_stubs.cpp:197. Without it, mSpokes/mMainActor are never populated, so this
// perform will short-circuit on the null-guards below. That's SAFE (no crash, no draw) —
// but the full sparkle effect won't visibly land until load is also ported. The perform
// port unblocks the vtable slot AND lets load be ported later without a lockstep change.

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
