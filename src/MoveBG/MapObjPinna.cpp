#include <MoveBG/MapObjPinna.hpp>
#include <System/Particles.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>
#include "sms_boot_amiking.h"

// Native port of TAmiKing::loadAfter (@0x801d48e0). RE: scratch/decomp_next5/801d48e0.c.
// あみキング (Ami King / "net king") — Pinna Park's giant net-body boss creature. loadAfter
// runs once per scene load; it forwards to TMapObjBase::loadAfter then idempotently
// registers the boss's spawn/hit particle resource (id 0x184, /scene/mapObj/amiking.jpa).
// The RE uses a file-scope byte latch at 0x803fd1ec to ensure the .jpa is pulled off disc
// exactly once even if multiple instances share the scene.
//
// SDA scan (tools/dol_sda.py 0x801d48e0):
//   SDA1[-0x5fe0] = gpResourceManager   (JPAResourceManager::load(name, id))
void TAmiKing::loadAfter()
{
	TMapObjBase::loadAfter();

	static bool s_registered_184 = false;
	if (!s_registered_184) {
		gpResourceManager->load(sb::kAmiKingParticlePath, sb::kAmiKingParticleId);
		s_registered_184 = true;
	}
}
