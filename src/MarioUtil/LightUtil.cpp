#include <MarioUtil/LightUtil.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JDrama/JDRLighting.hpp>
#include <cstring>

TLightWithDBSetManager* gpLightManager;

// r13-0x6114 = the "Light Group" TLightAry* singleton, cached at TLightCommon
// ctor-time (initialized to 0 there, populated by JDrama scene load). r13-0x6118
// is the AmbAry singleton, same pattern. Names below are fabricated.
extern JDrama::TLightAry* gpTLightCommonLightAry;   // r13-0x6114
extern JDrama::TAmbAry*   gpTLightCommonAmbAry;     // r13-0x6118
JDrama::TLightAry* gpTLightCommonLightAry;
JDrama::TAmbAry*   gpTLightCommonAmbAry;

TLightCommon::TLightCommon(const char* name)
    : JDrama::TViewObj(name)
    , unk10(1.0f)  // final ctor write: SDA2 -0x1770 = 1.0f
    , unk14(0.0f)
    , unk18(0.0f)
    , unk1C(0.0f)
    , unk20(0)
    , unk24(0)
    , unk28(0)
    , unk41(0)
{
	std::memset(mLocalAmbColor,   0, sizeof(mLocalAmbColor));
	std::memset(mLocalLightColor, 0, sizeof(mLocalLightColor));
	for (int i = 0; i < 4; ++i)
		mLocalPos[i].set(0.0f, 0.0f, 0.0f);
	// SDA1 singletons — ctor zeroes them (game code fills them at scene load).
	gpTLightCommonLightAry = nullptr;
	gpTLightCommonAmbAry   = nullptr;
}

void TLightCommon::loadAfter() { }

// Native port of TLightCommon::getLightColor (@0x80229d78). When the LOCAL
// override flag (unk28) is set, returns one of 4 packed GXColors stored at
// mLocalLightColor[]. Otherwise reads the corresponding Light-Group entry,
// scales its ALPHA by unk1C (as a f32 multiplier converted back to u8),
// and returns the modified color.
GXColor TLightCommon::getLightColor(int idx) const
{
	if (unk28) {
		if (idx >= 4) idx = 0;
		GXColor c;
		std::memcpy(&c, &mLocalLightColor[idx * 4], 4);
		return c;
	}
	// Group path: mLights[idx + unk24].unk24 is the GXLightObj.
	JDrama::TLightAry* la = gpTLightCommonLightAry;
	JDrama::TIdxLight& L  = la->mLights[idx + unk24];
	GXColor c;
	GXGetLightColor(&L.unk24, &c);
	// Alpha scaling: (float)a * unk1C, truncated to int, stored back as u8.
	c.a = static_cast<u8>(static_cast<int>(static_cast<f32>(c.a) * unk1C));
	return c;
}

// Native port of TLightCommon::getAmbColor (@0x80229cec). Same pattern as
// getLightColor but the local override array has ONLY 2 entries (at
// mLocalAmbColor[0..7]), and reads the Ambient Group instead of Light Group.
GXColor TLightCommon::getAmbColor(int idx) const
{
	if (unk28) {
		if (idx >= 2) idx = 0;
		GXColor c;
		std::memcpy(&c, &mLocalAmbColor[idx * 4], 4);
		return c;
	}
	JDrama::TAmbAry* aa    = gpTLightCommonAmbAry;
	JDrama::TAmbColor& A   = aa->mAmbColors[idx + unk20];
	GXColor c              = { A.mColor.r, A.mColor.g, A.mColor.b, A.mColor.a };
	c.a = static_cast<u8>(static_cast<int>(static_cast<f32>(c.a) * unk1C));
	return c;
}

// Native port of TLightCommon::getLightPosition (@0x80229ca0). If the local
// override flag (unk41) is set, returns &mLocalPos[idx]. Otherwise returns
// &Light-Group[idx + unk24].mPosition (offset 0x10 in TPlacement).
const JGeometry::TVec3<f32>* TLightCommon::getLightPosition(int idx)
{
	if (unk41) {
		if (idx >= 4) idx = 0;
		return &mLocalPos[idx];
	}
	JDrama::TLightAry* la = gpTLightCommonLightAry;
	JDrama::TIdxLight& L  = la->mLights[idx + unk24];
	return &L.mPosition;
}

void TLightCommon::setLight(const JDrama::TGraphics*, int) { }

// Forward decls for engine primitives called by perform. Symbols already
// resolve through the native GX shim / SMS runtime.
extern "C" void ReInitializeGX();
void SMS_DrawInit();

// Native port of TLightCommon::perform (@0x802298fc). Two independent phase
// gates:
//   * flag & 0x80 (bit 24) — the LIGHT-INIT phase: pull position via
//     getLightPosition(0) + color via getLightColor(0), build a single
//     GXLightObj with all-zero attenuation (= GX directional-uniform
//     configuration), and broadcast it to GX_LIGHT0/1/2. Also flushes
//     ReInitializeGX + SMS_DrawInit before the load.
//   * flag & 0x20 (bit 26) — the setLight phase: dispatch to the virtual
//     setLight(graphics, 0). Subclasses override this.
// Faithful to the disasm's slot layout and the RE-verified all-zero
// GXInitLightAttn call.
void TLightCommon::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (flag & 0x80) {
		ReInitializeGX();
		SMS_DrawInit();

		const JGeometry::TVec3<f32>* pos = getLightPosition(0);
		GXColor                       col = getLightColor(0);

		GXLightObj obj{};
		GXInitLightPos(&obj, pos->x, pos->y, pos->z);
		GXInitLightColor(&obj, col);
		GXInitLightAttn(&obj, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		GXLoadLightObjImm(&obj, GX_LIGHT0);
		GXLoadLightObjImm(&obj, GX_LIGHT1);
		GXLoadLightObjImm(&obj, GX_LIGHT2);
	}

	if (flag & 0x20) {
		setLight(graphics, 0);
	}
}

void TLightShadow::perform(u32 flag, JDrama::TGraphics* graphics)
{
	// Native port of TLightShadow::perform (@0x802298c0). Same phase gate as
	// TLightCommon's flag & 0x20 branch, dispatching to setLight with the
	// hardcoded index 1 (the shadow-light's slot in the light group).
	if (flag & 0x20)
		setLight(graphics, 1);
}

// Native port of TLightMario::perform (@0x80229880). Same phase gate as
// TLightShadow's, but the setLight index comes from gpMSMainProc's current
// stage state (a s16 at the manager singleton, SDA1[-0x6098]). Fallback to 0
// when we can't resolve the singleton on native — matches the "no-effect"
// default the file-select scene actually uses.
void TLightMario::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (flag & 0x20) {
		// TODO: wire gpMSMainProc's s16 stage-state field into this index.
		// The RE reads `lha r5, 0(r6)` where r6 = *(r13-0x6098) = gpMSMainProc.
		// Default 0 is the safe file-select value.
		setLight(graphics, 0);
	}
}

void TLightMario::setLight(const JDrama::TGraphics*, int) { }

// TLightMario overrides — stubbed for now. TLightCommon's base impl is what
// the file-select light path uses (setLight port in scene_drive.cpp reads from
// Light-Group directly, not through these getters). Faithful ports pending.
GXColor TLightMario::getLightColor(int) const { return GXColor{0, 0, 0, 0xFF}; }

GXColor TLightMario::getAmbColor(int) const   { return GXColor{0, 0, 0, 0xFF}; }

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
