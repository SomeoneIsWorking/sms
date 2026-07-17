#include "MoveBG/MapObjCorona.hpp"
#include "MoveBG/MapObjBase.hpp"
#include "JSystem/JParticle/JPAResourceManager.hpp"

extern JPAResourceManager* gpResourceManager;

// Native port of TBathtub::loadAfter (@0x801fb894). バスタブ — Corona Mountain's
// bathtub (Bowser's final-stage arena). Idempotently registers four particle
// resources with gpResourceManager (r13-0x5fe0): steam (yuge), fountain (funsui),
// and two break-piece variants. Each registration is guarded by its own file-scope
// byte latch so if multiple TBathtub instances load in the scene the .jpa file is
// only pulled off disc once. IDs (0x1be, 0x1bf, 0xf6, 0xf7) match the RE. This
// override does NOT chain to TMapObjBase::loadAfter — the disasm starts directly
// with the first flag check, no base call.
void TBathtub::loadAfter()
{
	static bool s_registered_1be = false;
	if (!s_registered_1be) {
		gpResourceManager->load("/scene/map/map/ms_lkp_yuge1.jpa", 0x1be);
		s_registered_1be = true;
	}

	static bool s_registered_1bf = false;
	if (!s_registered_1bf) {
		gpResourceManager->load("/scene/map/map/ms_kp_funsui.jpa", 0x1bf);
		s_registered_1bf = true;
	}

	static bool s_registered_f6 = false;
	if (!s_registered_f6) {
		gpResourceManager->load("/scene/map/map/ms_kp_break_a.jpa", 0xf6);
		s_registered_f6 = true;
	}

	static bool s_registered_f7 = false;
	if (!s_registered_f7) {
		gpResourceManager->load("/scene/map/map/ms_kp_break_b.jpa", 0xf7);
		s_registered_f7 = true;
	}
}

void TBathtub::hipdrop(const JGeometry::TVec3<f32>&) { }

void TBathtub::quake(const JGeometry::TVec3<f32>&) { }

u8 TBathtub::getNumGripsDead() const { return 0; }

void TBathtub::tumble(f32, f32) { }

MtxPtr TBathtub::getTakingMtx() { return nullptr; }

MtxPtr TBathtub::getSubmarineMtxInDemo() { return nullptr; }

MtxPtr TBathtub::getPeachMtxInDemo() { return nullptr; }

MtxPtr TBathtub::getKoopaJrMtxInDemo() { return nullptr; }

BOOL TBathtub::receiveMessage(THitActor* sender, u32 message) { return false; }

Mtx* TBathtub::getRootJointMtx() const { return nullptr; }

void TBathtub::perform(u32, JDrama::TGraphics*) { }

void TBathtub::control() { }

void TBathtub::calcBathtubData() { }

void TBathtub::setupCollisions_() { }

void TBathtub::removeCollisions_() { } // Unused

void TBathtub::startDemo() { }

bool TBathtub::allowsTumble() const { return false; }

void TBathtub::calcRootMatrix() { }

bool TBathtub::getNearGrip(const JGeometry::TVec3<f32>&, f32, f32*) const
{
	return false;
}

u8 TBathtub::getNextJuncture(const JGeometry::TVec3<f32>&,
                             const JGeometry::TVec3<f32>&) const
{
	return 0;
}

u8 TBathtub::getNextGrip(const JGeometry::TVec3<f32>&,
                         const JGeometry::TVec3<f32>&, f32, f32*) const
{
	return 0;
}

void TBathtub::updatePosture_() { }

TBathtub::TBathtub(const char* name)
    : TMapObjBase(name)
{
}

void TBathtub::load(JSUMemoryInputStream&) { }

u8 TBathtub::getNumKillerLaunchable() const { return 0; }

bool TBathtub::isKillerAttackable() const { return false; }

u8 TBathtub::getNumKillerBurstable() const { return 0; }

// Unused
bool TBathtub::isBreaking() const { return false; }

// Unused
bool TBathtub::isKillerLaunchable() const { return false; }

// Unused
void TBathtub::showMessage(u32) { }

// Unused
u8 TBathtub::getNearJuncture(const JGeometry::TVec3<f32>&) const { return 0; }

// Unused
MtxPtr TBathtub::getKoopaMtxInDemo() { return nullptr; }

// Unused
MtxPtr TBathtub::getWaterMtx(s32) { return nullptr; }

// Unused
MtxPtr TBathtub::getShineEffectMtx() { return nullptr; }

// Unused
MtxPtr TBathtub::getShineMtx() { return nullptr; }

// Unused
void TBathtub::liftMario(const JGeometry::TVec3<f32>&) { }

// Unused
void TBathtub::trample(const JGeometry::TVec3<f32>&) { }
