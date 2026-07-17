#include <MoveBG/MapObjBianco.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>

// Bianco Hills big horizontal watermill. RE'd US GMSE01 (wide-RE sweep 2026-07-17).
// TBiancoWatermill : TMapObjBase — spins constantly about Z and plays a loop sound.
// (Sibling bell/vertical-mill/mini-windmill in this TU deferred — uncertain string/matrix
// /water-physics parts flagged in the spec.)

TBiancoWatermill::TBiancoWatermill(const char* name)
    : TMapObjBase(name)
{
	mSpinSpeed = 0.3f;
	mSound     = nullptr;
}

void TBiancoWatermill::initMapObj()
{
	TMapObjBase::initMapObj();
	// (retail also tweaks a model-appear field by exact BMD-name match; deferred — the
	//  variant strings need .data extraction and only affect a cosmetic appear timing.)
}

void TBiancoWatermill::control()
{
	mRotation.z -= mSpinSpeed; // constant spin about Z
	if (gpMSound->gateCheck(0x3043))
		MSoundSESystem::MSoundSE::startSoundNpcActor(0x3043, &mPosition, 0, &mSound, 0, 4);
}

u32 TBiancoWatermill::touchWater(THitActor*) { return 0; } // water has no effect on the big wheel

void TBiancoWatermill::turnByEnemy(THitActor*, const TBGCheckData*) {} // no-op override
