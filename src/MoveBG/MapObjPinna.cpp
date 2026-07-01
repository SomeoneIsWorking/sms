#include <MoveBG/MapObjPinna.hpp>
#include <MoveBG/MapObjManager.hpp>
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

// Native port of TPinnaEntrance::loadAfter (@0x801d49cc). RE: scratch/decomp_next5/801d49cc.c.
// ピンナ入り口 — Pinna Park entrance. After the base loadAfter, spawns a companion
// "GateManta" (the giant manta silhouette that swims through the water gate). Rotation is
// (90°, 0°, 0°) — pitch it onto its side — and scale is identity.
//
// SDA scan (tools/dol_sda.py 0x801d49cc):
//   SDA2[-0x2728] = 90.0f  (pitch rotation)
//   SDA2[-0x2724] = 1.0f   (identity scale)
//   SDA2[-0x2750] = 0.0f   (yaw + roll)
void TPinnaEntrance::loadAfter()
{
	TMapObjBase::loadAfter();
	JGeometry::TVec3<f32> rotation(90.0f, 0.0f, 0.0f);
	JGeometry::TVec3<f32> scale(1.0f, 1.0f, 1.0f);
	TMapObjBaseManager::newAndRegisterObj("GateManta", mPosition, rotation, scale);
}
