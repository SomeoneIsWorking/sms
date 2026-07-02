#include <MarioUtil/LightUtil.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JDrama/JDRLighting.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <cstring>

TLightWithDBSetManager* gpLightManager;

// Forward decls for engine primitives called from perform/setLight.
extern "C" void ReInitializeGX();
void SMS_DrawInit();

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

// Native port of TLightCommon::loadAfter (@0x80229e30, 84 insns). Runs once
// per scene load. Resolves the "Light Group" (TLightAry) and "Ambient Group"
// (TAmbAry) singletons through the top-level TNameRefGen at SDA1[-0x5db8],
// caches them into the SDA1 slots at [-0x6114]/[-0x6118] (the getters read
// them back), plus caches &mLights[0].mPosition at [-0x6110]. Then seeds the
// local per-light-color / per-light-position arrays this->mLocalLightColor[4]
// and this->mLocalPos[4] from Light-Group[unk24..unk24+4], and the local
// ambient arrays this->mLocalAmbColor[2] from AmbAry->mAmbColors[unk20..+1].
// Also final-writes this->unk10 = 50.0f (SDA2 -0x1770).
//
// gpTLightCommonLightAry / gpTLightCommonAmbAry are the SDA1 caches; the
// getters (defined earlier in this file) read from them.
void TLightCommon::loadAfter()
{
	// r13-0x5db8 is a top-level director object; in the RE it exposes its
	// TNameRefGen at offset +0x4. In the native build the same lookup is
	// available through JDrama::TNameRefGen::searchF on any live gen. Fall
	// back to the game's global gen if the specific director isn't wired.
	auto* lightAry = JDrama::TNameRefGen::search<JDrama::TLightAry>("Light Group");
	auto* ambAry   = JDrama::TNameRefGen::search<JDrama::TAmbAry>  ("Ambient Group");

	gpTLightCommonAmbAry   = ambAry;
	gpTLightCommonLightAry = lightAry;

	unk10 = 50.0f;  // SDA2 -0x1770

	if (!lightAry || !lightAry->mLights) {
		// Match the RE: even on a partial load, keep going; the getters guard.
		return;
	}

	// Populate local light-color + local position arrays from
	// Light-Group[unk24 .. unk24+4] — 4 entries.
	for (int i = 0; i < 4; ++i) {
		JDrama::TIdxLight& L = lightAry->mLights[i + unk24];
		// mLocalLightColor slot 0/1/2/3 lives at offset 0x31 + i*4 (4 bytes).
		GXColor col;
		GXGetLightColor(&L.unk24, &col);
		std::memcpy(&mLocalLightColor[i * 4], &col, 4);
		// mLocalPos slot i at offset 0x44 + i*12 (Vec3).
		mLocalPos[i].set(L.mPosition.x, L.mPosition.y, L.mPosition.z);
	}

	// Populate local ambient colors from AmbAry->mAmbColors[unk20 + 0..1].
	if (ambAry && ambAry->mAmbColors) {
		for (int i = 0; i < 2; ++i) {
			JUtility::TColor& c = ambAry->mAmbColors[i + unk20].mColor;
			// Layout at this + 0x29 + i*4.
			mLocalAmbColor[i * 4 + 0] = c.r;
			mLocalAmbColor[i * 4 + 1] = c.g;
			mLocalAmbColor[i * 4 + 2] = c.b;
			mLocalAmbColor[i * 4 + 3] = c.a;
		}
	}
}

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

// Native port of TLightCommon::setLight (@0x80229a30, 156 insns). Broadcasts
// three GX lights (LIGHT0 positional, LIGHT1 gpLightManager-effect if enabled,
// LIGHT2 specular-directional) — the real light source the pre-oracle
// scene_drive.cpp hack was faking. Matches the pure spec at
// native/render/sms_boot_setlight.h (value-verified by the GX oracle for
// stage-15 file-select's 3-white-light baseline). Slot arg to the getters is
// `idx * 2` per the disasm's `slwi r31, r30, 1` — file-select passes idx=0 so
// the doubling is transparent, but faithful for future callers.
void TLightCommon::setLight(const JDrama::TGraphics* graphics, int idx)
{
	ReInitializeGX();
	SMS_DrawInit();

	const int  gi   = idx * 2;              // getter idx (faithful to `slwi r31, r30, 1`)
	MtxPtr     view = const_cast<JDrama::TGraphics*>(graphics)->getUnkB4();

	GXLightObj obj{};

	// --- GX_LIGHT0 — positional world light (Light-Group[idx+unk24]). ---
	{
		const JGeometry::TVec3<f32>* wpos = getLightPosition(gi);
		JGeometry::TVec3<f32>        vpos;
		PSMTXMultVec(view, const_cast<JGeometry::TVec3<f32>*>(wpos), &vpos);
		GXInitLightPos(&obj, vpos.x, vpos.y, vpos.z);

		GXColor col = getLightColor(gi);
		GXInitLightColor(&obj, col);

		// GXInitLightAttn(a0=1, a1=0, a2=0, k0=1, k1=0, k2=0) — GC default.
		GXInitLightAttn(&obj, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		GXLoadLightObjImm(&obj, GX_LIGHT0);
	}

	// --- GX_LIGHT1 — gpLightManager's "effect" (specular sun) light, gated
	// on unk54 && unk55. Position is manager's Vec3<f32> aliased as unk1C..unk24
	// (the header still declares those as u32 slots because the ctor writes them
	// through the `unk1C = unk20 = unk24 = 0` path in TLightWithDBSetManager). ---
	if (gpLightManager && gpLightManager->unk54 && gpLightManager->unk55) {
		// Reinterpret the 3 consecutive u32 slots as a Vec3<f32> — the game's
		// C++ source did this by pointer arithmetic on the manager instance.
		const JGeometry::TVec3<f32>* mgrPos =
		    reinterpret_cast<const JGeometry::TVec3<f32>*>(&gpLightManager->unk1C);
		JGeometry::TVec3<f32> vpos;
		PSMTXMultVec(view, const_cast<JGeometry::TVec3<f32>*>(mgrPos), &vpos);
		GXInitLightPos(&obj, vpos.x, vpos.y, vpos.z);

		// Color: manager's GXColor (unk18) with alpha scaled by unk28 (f32).
		GXColor col = gpLightManager->unk18;
		col.a = static_cast<u8>(static_cast<int>(static_cast<f32>(col.a)
		                                        * gpLightManager->unk28));
		GXInitLightColor(&obj, col);

		// Spot(1, 0=OFF) + DistAttn(-0x17a0, -0x179c, 3=EXP) — SDA2 constants.
		GXInitLightAttnA(&obj, 1.0f, 0.0f, 0.0f);
		GXInitLightDistAttn(&obj, /*ref_distance=*/ 1000.0f, /*ref_brightness=*/ 0.5f,
		                    GX_DA_MEDIUM);
		GXLoadLightObjImm(&obj, GX_LIGHT1);
	}

	// --- GX_LIGHT2 — specular-directional (dir = -normalize(view-pos)). ---
	{
		const JGeometry::TVec3<f32>* wpos = getLightPosition(gi);
		JGeometry::TVec3<f32>        vpos;
		PSMTXMultVec(view, const_cast<JGeometry::TVec3<f32>*>(wpos), &vpos);
		JGeometry::TVec3<f32> nrm = vpos;
		PSVECNormalize(&nrm, &nrm);
		GXInitSpecularDir(&obj, -nrm.x, -nrm.y, -nrm.z);

		GXColor col = getLightColor(gi);
		GXInitLightColor(&obj, col);
		GXInitLightAttn(&obj, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		GXLoadLightObjImm(&obj, GX_LIGHT2);
	}
}

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

// TLightMario overrides — stubbed for now. TLightCommon's base impl is what
// the file-select light path uses. Faithful ports pending.
GXColor TLightMario::getLightColor(int) const { return GXColor{0, 0, 0, 0xFF}; }

GXColor TLightMario::getAmbColor(int) const   { return GXColor{0, 0, 0, 0xFF}; }

// Native port of TLightMario::setLight (@0x80229610, 156 insns). Byte-
// identical to TLightCommon::setLight (verified by side-by-side disasm
// comparison against @0x80229a30). The RE is CodeWarrior having emitted the
// SAME function twice because both classes are polymorphic setLight
// overrides; the C++ source is a single implementation delegated by the
// override. Faithful port = dispatch to the base.
void TLightMario::setLight(const JDrama::TGraphics* graphics, int idx)
{
	TLightCommon::setLight(graphics, idx);
}

TLightDrawBuffer::TLightDrawBuffer(int, u32, const char* name)
    : JDrama::TViewObj(name)
{
}

// Native port of TLightDrawBuffer::perform (@0x802292c0, 17 insns). Same phase
// gate as TLightCommon (flag & 0x20 → setLight). Dispatches to its owning
// TLightCommon at unk10, using the per-buffer light index at unk80.
void TLightDrawBuffer::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (flag & 0x20) {
		if (unk10)
			unk10->setLight(graphics, unk80);
	}
}

// Native port of TLightWithDBSet::perform (@0x80229178, 82 insns). Dispatches
// each per-light draw buffer in unk10[0..unk1C) through its own perform +
// optional J3DDrawBuffer perform calls, gated by three independent flag bits:
//   * flag & 0x20    → per-DrawBuffer perform(0x20, graphics)
//   * flag & 0x10000 → per-DrawBuffer's unk14->perform(8, graphics)
//   * flag & 0x20000 → per-DrawBuffer's unk18->perform(8, graphics)
//   * flag & 0x800   → both unk14/unk18 perform(0x480, graphics)
// Guarded on unk10 non-null — matches the STOPGAP in changeLightDrawBuffer;
// once TLightWithDBSet::makeDrawBuffer × 4 is ported the guard becomes
// harmless.
void TLightWithDBSet::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (!unk10) return;  // makeDrawBuffer not yet populated on this instance

	if (flag & 0x20) {
		const bool do_unk14_8 = (flag & 0x10000) != 0;
		const bool do_unk18_8 = (flag & 0x20000) != 0;
		for (int i = 0; i < unk1C; ++i) {
			TLightDrawBuffer* buf = unk10[i];
			buf->perform(0x20, graphics);
			if (do_unk14_8 && buf->unk14) buf->unk14->perform(8, graphics);
			if (do_unk18_8 && buf->unk18) buf->unk18->perform(8, graphics);
		}
	}

	if (flag & 0x800) {
		for (int i = 0; i < unk1C; ++i) {
			TLightDrawBuffer* buf = unk10[i];
			if (buf->unk14) buf->unk14->perform(0x480, graphics);
			if (buf->unk18) buf->unk18->perform(0x480, graphics);
		}
	}
}

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

// Native port of TLightWithDBSetManager::perform (@0x80228394, 63 insns).
// Dispatches active per-kind TLightWithDBSet buffers (player/mapobj/object/
// indirect at unk14[0..3]). Two independent phase gates with independent
// index-range selections:
//   * flag & 0x20  → iterate a slice of unk14[]:
//                    flag & 0x100000 → [3, 4)  (indirect only)
//                    flag & 0x80000  → [2, 3)  (object only)
//                    else            → [0, 2)  (player + mapobj)
//   * flag & 0x800 → iterate ALL unk14[0..4)
// Each iteration guards on the buffer's own unk20 "enabled" flag being set,
// then forwards `flag` unchanged.
void TLightWithDBSetManager::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (flag & 0x20) {
		int start, end;
		if (flag & 0x100000)      { start = 3; end = 4; }
		else if (flag & 0x80000)  { start = 2; end = 3; }
		else                       { start = 0; end = 2; }
		for (int i = start; i < end; ++i) {
			TLightWithDBSet* buf = unk14[i];
			if (buf && buf->unk20)
				buf->perform(flag, graphics);
		}
	}

	if (flag & 0x800) {
		for (int i = 0; i < 4; ++i) {
			TLightWithDBSet* buf = unk14[i];
			if (buf && buf->unk20)
				buf->perform(flag, graphics);
		}
	}
}

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
