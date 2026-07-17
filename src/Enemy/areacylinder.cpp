#include <Enemy/AreaCylinder.hpp>
#include <Enemy/Conductor.hpp>
#include <JSystem/JGeometry.hpp>

#ifdef SMS_NATIVE_PLATFORM
static inline f32 sb_bswapf(f32 v)
{
	u32 b;
	__builtin_memcpy(&b, &v, 4);
	b = __builtin_bswap32(b);
	__builtin_memcpy(&v, &b, 4);
	return v;
}
#endif

// TAreaCylinder — a cylindrical area-trigger volume (scene name "AreaCylinder"/"AreaSphere").
// RE'd US GMSE01 (wide-RE sweep 2026-07-17). A lightweight TViewObj holding geometry; the
// manager keeps a list and answers point-in-cylinder queries. Already factory-registered
// (MarNameRefGen.cpp). Not an enemy — the seal/bird manager idiom does NOT apply.

TAreaCylinder::TAreaCylinder(const char* name)
    : JDrama::TViewObj(name)
{
	unk10 = 0.0f;
	unk14 = 0.0f;
	unk18 = 0.0f;
	unk1C = 0.0f;
	// unk20/unk24 set by load()
}

void TAreaCylinder::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);

	stream.read(&unk10, sizeof(f32)); // center X
	stream.read(&unk14, sizeof(f32)); // base Y
	stream.read(&unk18, sizeof(f32)); // center Z
	f32 rot[3];
	stream.read(&rot[0], sizeof(f32));
	stream.read(&rot[1], sizeof(f32));
	stream.read(&rot[2], sizeof(f32)); // rotation, unused
	stream.read(&unk1C, sizeof(f32)); // scale X -> radius
	stream.read(&unk20, sizeof(f32)); // scale Y -> height
	f32 sclZ;
	stream.read(&sclZ, sizeof(f32));  // scale Z, unused
#ifdef SMS_NATIVE_PLATFORM
	// JSUInputStream::read raw-copies WITHOUT byteswap; scene data is big-endian. Swap the
	// f32s we keep (each explicitly — do NOT rely on host field contiguity).
	unk10 = sb_bswapf(unk10);
	unk14 = sb_bswapf(unk14);
	unk18 = sb_bswapf(unk18);
	unk1C = sb_bswapf(unk1C);
	unk20 = sb_bswapf(unk20);
#endif
	unk1C *= 50.0f; // radius
	unk20 *= 50.0f; // height

	stream.readString(); // model/attr name, discarded
	s32 n;
	stream.read(&n, sizeof(s32));
#ifdef SMS_NATIVE_PLATFORM
	n = (s32)__builtin_bswap32((u32)n);
#endif
	for (s32 i = 0; i < n; ++i) { // linked-object refs, discarded
		s32 tmp;
		stream.read(&tmp, sizeof(s32));
		stream.readString();
	}
	const char* mgrName = stream.readString();

	TAreaCylinderManager* mgr = (TAreaCylinderManager*)gpConductor->search(mgrName);
	if (mgr == nullptr) {
		mgr = new TAreaCylinderManager(mgrName);
		gpConductor->registerAreaCylinderManager(mgr);
	}
	mgr->registerCylinder(this);

	s32 v;
	stream.read(&v, sizeof(s32));
#ifdef SMS_NATIVE_PLATFORM
	v = (s32)__builtin_bswap32((u32)v);
#endif
	unk24 = (f32)v / 100.0f;
}

void TAreaCylinder::perform(u32, JDrama::TGraphics*) {}

// ---------------------------------------------------------------------------
// TAreaCylinderManager
// ---------------------------------------------------------------------------
TAreaCylinderManager::TAreaCylinderManager(const char* name)
    : JDrama::TViewObj(name)
{
}

void TAreaCylinderManager::perform(u32, JDrama::TGraphics*) {}

void TAreaCylinderManager::registerCylinder(TAreaCylinder* cyl)
{
	mList.push_back(cyl);
}

bool TAreaCylinderManager::contain(const JGeometry::TVec3<f32>& pos)
{
	return getCylinderContains(pos) != nullptr;
}

TAreaCylinder* TAreaCylinderManager::getCylinderContains(const JGeometry::TVec3<f32>& pos)
{
	for (JGadget::TList<TAreaCylinder*>::iterator it = mList.begin(); it != mList.end(); ++it) {
		TAreaCylinder* c = *it;
		if (pos.y < c->unk14)
			continue;
		if (pos.y > c->unk14 + c->unk20)
			continue;
		f32 dx = pos.x - c->unk10;
		f32 dz = pos.z - c->unk18;
		if (dx * dx + dz * dz <= c->unk1C * c->unk1C)
			return c;
	}
	return nullptr;
}
