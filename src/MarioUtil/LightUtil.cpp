#include <MarioUtil/LightUtil.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>

TLightWithDBSetManager* gpLightManager;

TLightCommon::TLightCommon(const char* name)
    : JDrama::TViewObj(name)
{
}

void TLightCommon::loadAfter() { }

void TLightCommon::getLightColor(int) const { }

void TLightCommon::getAmbColor(int) const { }

void TLightCommon::getLightPosition(int) { }

void TLightCommon::setLight(const JDrama::TGraphics*, int) { }

void TLightCommon::perform(u32, JDrama::TGraphics*) { }

void TLightShadow::perform(u32, JDrama::TGraphics*) { }

void TLightMario::perform(u32, JDrama::TGraphics*) { }

void TLightMario::setLight(const JDrama::TGraphics*, int) { }

void TLightMario::getLightColor(int) const { }

void TLightMario::getAmbColor(int) const { }

TLightDrawBuffer::TLightDrawBuffer(int, u32, const char* name)
    : JDrama::TViewObj(name)
{
}

void TLightDrawBuffer::perform(u32, JDrama::TGraphics*) { }

void TLightWithDBSet::perform(u32, JDrama::TGraphics*) { }

void TLightWithDBSet::changeLightDrawBuffer(int param_1)
{
	unk14 = nullptr;
	unk18 = nullptr;
	if (param_1 > unk1C)
		param_1 = 0;

#ifdef SMS_NATIVE_PLATFORM
	// STOPGAP: the per-light draw-buffer set (unk10) is NOT built — makeDrawBuffer() is a
	// no-op stub across the whole TLightWithDBSet hierarchy (LightUtil.cpp:69-75), and the
	// TLightDrawBuffer::perform that would actually render those buffers is not wired into any
	// perform list. So redirecting an actor's draw into unk10[param_1] (the original behavior
	// below) would (a) deref a null unk10 and (b), even if built, send the actor into an
	// UNRENDERED buffer -> the actor would vanish. When the set is absent, leaving unk14/unk18
	// null makes entry() draw into the CURRENT (normal Chr) buffer and resetLightDrawBuffer() a
	// no-op (its own !unk14/!unk18 guards) — the correct degenerate result when no per-light
	// shadow volumes exist, which is exactly this stub's state. This is what lets NPCs (whose
	// MActor::setLightData assigns a real unk3C on shadow ground) render at all.
	// PROPER FIX: port the light-with-DB-set subsystem — TLightWithDBSet::makeDrawBuffer building
	// unk10 (TLightDrawBuffer opa/xlu pairs), wire their perform into PerformList GX, and
	// implement TLightCommon::setLight — then restore the redirect below.
	if (!unk10)
		return;
#endif

	unk14 = j3dSys.getDrawBuffer(0);
	unk18 = j3dSys.getDrawBuffer(1);

	j3dSys.setDrawBuffer(unk10[param_1]->unk14->mDrawBuffer, 0);
	j3dSys.setDrawBuffer(unk10[param_1]->unk18->mDrawBuffer, 1);
}

void TLightWithDBSet::resetLightDrawBuffer()
{
	if (!unk14)
		return;
	if (!unk18)
		return;

	j3dSys.setDrawBuffer(unk14, 0);
	j3dSys.setDrawBuffer(unk18, 1);
	unk14 = nullptr;
	unk18 = nullptr;
}

void TPlayerLightWithDBSet::makeDrawBuffer() { }

void TObjectLightWithDBSet::makeDrawBuffer() { }

void TMapObjectLightWithDBSet::makeDrawBuffer() { }

void TIndirectLightWithDBSet::makeDrawBuffer() { }

// The decomp left the TLightWithDBSet hierarchy ctors and the manager's unk14
// initialization unimplemented (declared-only). The base/subclass ctors are restored
// here (minimal: the draw-buffer setup that the unimplemented makeDrawBuffer would
// build is left null — the whole light-with-DB-set path is stubbed, perform/
// makeDrawBuffer are no-ops). unk20 is the per-light "enabled" flag the actor managers
// write (TLiveManager / MActor: getUnk14(i)->unk20 = 1); it must be addressable.
TLightWithDBSet::TLightWithDBSet(int idx, const char* name)
    : JDrama::TViewObj(name)
    , unk10(nullptr)
    , unk14(nullptr)
    , unk18(nullptr)
    , unk1C(idx)
    , unk20(0)
{
}

TPlayerLightWithDBSet::TPlayerLightWithDBSet()
    : TLightWithDBSet(0, "<TPlayerLightWithDBSet>")
{
}
TMapObjectLightWithDBSet::TMapObjectLightWithDBSet()
    : TLightWithDBSet(0, "<TMapObjectLightWithDBSet>")
{
}
TObjectLightWithDBSet::TObjectLightWithDBSet()
    : TLightWithDBSet(0, "<TObjectLightWithDBSet>")
{
}
TIndirectLightWithDBSet::TIndirectLightWithDBSet()
    : TLightWithDBSet(0, "<TIndirectLightWithDBSet>")
{
}

TLightWithDBSetManager::TLightWithDBSetManager(const char* name)
    : JDrama::TViewObj(name)
{
#ifdef SMS_NATIVE_PLATFORM
	// Allocate the 4-entry light-set array the managers index by light kind
	// (player/mapobj/object/indirect). The decomp's empty ctor left unk14 wild, so
	// the first getUnk14(i)->unk20 = 1 (TLiveManager ctor) dereferenced garbage.
	unk10 = new TLightMario;
	unk14 = new TLightWithDBSet*[4];
	unk14[0] = new TPlayerLightWithDBSet;
	unk14[1] = new TMapObjectLightWithDBSet;
	unk14[2] = new TObjectLightWithDBSet;
	unk14[3] = new TIndirectLightWithDBSet;
	unk18.r = unk18.g = unk18.b = unk18.a = 0;
	unk1C = unk20 = unk24 = 0;
	unk28 = unk2C = unk30 = unk34 = unk38 = unk3C = unk40 = unk44 = 0.0f;
	unk48.x = unk48.y = unk48.z = 0.0f;
	unk54 = unk55 = 0;
#endif
}

void TLightWithDBSetManager::loadAfter() { }

void TLightWithDBSetManager::perform(u32, JDrama::TGraphics*) { }

void TLightWithDBSetManager::addChildGroupObj(
    JDrama::TViewObjPtrListT<JDrama::TViewObj, JDrama::TViewObj>*)
{
}

void TLightWithDBSetManager::makeDrawBuffer()
{
	for (int i = 0; i < 4; ++i)
		if (unk14[i]->unk20)
			unk14[i]->makeDrawBuffer();
}

void TLightWithDBSetManager::getLightPos() const { }
