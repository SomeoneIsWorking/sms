#include <MarioUtil/LightUtil.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/ReinitGX.hpp>
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

void TLightCommon::loadAfter()
{
	mAmbAry   = JDrama::TNameRefGen::search<JDrama::TAmbAry>("Ambient Group");
	mLightAry = JDrama::TNameRefGen::search<JDrama::TLightAry>("Light Group");
	mLightPos = &mLightAry->getLight(0)->mPosition;
	unk10     = 50.0f;
	for (int i = 0; i < 4; ++i) {
		unk31[i] = mLightAry->getLight(unk24 + i)->getColor();
		unk44[i] = mLightAry->getLight(unk24 + i)->mPosition;
	}
	unk29[0] = mAmbAry->getAmb(unk20)->getColor();
	unk29[1] = mAmbAry->getAmb(unk20 + 1)->getColor();
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

void TLightCommon::setLight(const JDrama::TGraphics* gfx, int index)
{
	ReInitializeGX();
	SMS_DrawInit();
	int lightIndex = index * 2;

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

	GXSetChanAmbColor(GX_COLOR0A0, getAmbColor(index));
}

// TLightMario overrides — stubbed for now. TLightCommon's base impl is what
// the file-select light path uses (setLight port in scene_drive.cpp reads from
// Light-Group directly, not through these getters). Faithful ports pending.
GXColor TLightMario::getLightColor(int) const { return GXColor{0, 0, 0, 0xFF}; }

GXColor TLightMario::getAmbColor(int) const   { return GXColor{0, 0, 0, 0xFF}; }

void TLightMario::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_LIGHT)
		setLight(graphics, *gpMarioLightID);
}

void TLightMario::setLight(const JDrama::TGraphics* gfx, int index)
{
	ReInitializeGX();
	SMS_DrawInit();
	int lightIndex = index * 2;

	GXLightObj light;
	Vec pos;
	MTXMultVec(gfx->getViewMtx(), getLightPosition(lightIndex), &pos);
	GXInitLightPos(&light, pos.x, pos.y, pos.z);
	GXInitLightColor(&light, getLightColor(lightIndex));
	GXInitLightAttn(&light, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	GXLoadLightObjImm(&light, GX_LIGHT0);

	gpLightManager->setEffectLight(gfx, &light);

	Vec spos;
	MTXMultVec(gfx->getViewMtx(), getLightPosition(lightIndex), &spos);
	VECNormalize(&spos, &spos);
	GXInitSpecularDir(&light, -spos.x, -spos.y, -spos.z);
	GXInitLightColor(&light, getLightColor(lightIndex));
	GXInitLightAttn(&light, 0.0f, 0.0f, 1.0f, unk10 / 2.0f, 0.0f,
	                1.0f - unk10 / 2.0f);
	GXLoadLightObjImm(&light, GX_LIGHT2);

	GXSetChanAmbColor(GX_COLOR0A0, getAmbColor(index));
}

GXColor TLightMario::getLightColor(int index) const
{
	GXColor color = TLightCommon::getLightColor(index + unk24);
	color.a *= unk14;
	return color;
}

GXColor TLightMario::getAmbColor(int index) const
{
	index += unk24;
	GXColor color = TLightCommon::getAmbColor(index);
	color.a *= unk14;
	return color;
}

#pragma dont_inline on
TLightDrawBuffer::TLightDrawBuffer(int param_1, u32 param_2, const char* name)
    : JDrama::TViewObj(name)
    , unk10(nullptr)
    , mOpaDrawBufferObject(nullptr)
    , mXluDrawBufferObject(nullptr)
    , unk80(param_1)
{
	snprintf(unk1C, 0x32, "%s%s", name, "opa");
	mOpaDrawBufferObject = new JDrama::TDrawBufObj(3, param_2, unk1C);

	snprintf(unk4E, 0x32, "%s%s", name, "xlu");
	mXluDrawBufferObject = new JDrama::TDrawBufObj(4, param_2, unk4E);
}
#pragma dont_inline reset

void TLightDrawBuffer::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_LIGHT)
		unk10->setLight(graphics, unk80);
}

TLightWithDBSet::TLightWithDBSet(int param_1, const char* name)
    : JDrama::TViewObj(name)
{
	unk10 = nullptr;
	unk14 = nullptr;
	unk18 = nullptr;
	unk1C = param_1;
	unk20 = 0;
}

void TLightWithDBSet::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_LIGHT) {
		for (int i = 0; i < unk1C; ++i) {
			unk10[i]->perform(CUE_LIGHT, graphics);
			if (cue & CUE_UNK10000)
				unk10[i]->getOpaDbo()->perform(CUE_DRAW, graphics);
			if (cue & CUE_UNK20000)
				unk10[i]->getXluDbo()->perform(CUE_DRAW, graphics);
		}
	}
	if (cue & CUE_SET_DRAW_BUFFER) {
		for (int i = 0; i < unk1C; ++i) {
			unk10[i]->getOpaDbo()->perform(CUE_SET_DRAW_BUFFER | CUE_DRAW_INIT,
			                               graphics);
			unk10[i]->getXluDbo()->perform(CUE_SET_DRAW_BUFFER | CUE_DRAW_INIT,
			                               graphics);
		}
	}
}

void TLightWithDBSet::addChildGroupObj(
    JDrama::TViewObjPtrListT<JDrama::TViewObj>* list)
{
	if (unk20) {
		for (int j = 0; j < unk1C; ++j) {
			list->insert(unk10[j]->getOpaDbo());
			list->insert(unk10[j]->getXluDbo());
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

	j3dSys.setDrawBuffer(unk10[param_1]->getOpaDbo()->getDrawBuffer(), 0);
	j3dSys.setDrawBuffer(unk10[param_1]->getXluDbo()->getDrawBuffer(), 1);
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

void TLightWithDBSet::getOpaDrawBuffer(int) { }

void TLightWithDBSet::getXluDrawBuffer(int) { }

void TLightWithDBSet::getLightDrawBuffer(int) { }

int TLightWithDBSet::getLightIndex(const char* name)
{
	for (int i = 0; i < TLightCommon::mLightAry->getLightNum(); ++i)
		if (strcmp(name, TLightCommon::mLightAry->getLight(i)->getName()) == 0)
			return i;
	return -1;
}

int TLightWithDBSet::getAmbIndex(const char* name)
{
	for (int i = 0; i < TLightCommon::mAmbAry->getAmbNum(); ++i)
		if (strcmp(name, TLightCommon::mAmbAry->getAmb(i)->getName()) == 0)
			return i;
	return -1;
}

void TPlayerLightWithDBSet::makeDrawBuffer()
{
	static const char lightName[] = "太陽（プレイヤー）";
	static const char ambName[]   = "太陽アンビエント（プレイヤー）";

	int lightIndex = getLightIndex(lightName);
	int ambIndex   = getAmbIndex(ambName);
	unk10          = new TLightDrawBuffer*[unk1C];
	for (int i = 0; i < unk1C; ++i) {
		unk10[i] = new TLightDrawBuffer(
		    i, 0x80, TLightCommon::mAmbAry->getAmb(ambIndex + i)->getName());
		TLightMario* light = new TLightMario();
		unk10[i]->setLight(light);
		unk10[i]->unk10->unk20 = ambIndex;
		unk10[i]->unk10->unk24 = lightIndex;
		unk10[i]->unk10->loadAfter();
	}
}

void TObjectLightWithDBSet::makeDrawBuffer()
{
	static const char lightName[] = "太陽（オブジェクト）";
	static const char ambName[]   = "太陽アンビエント（オブジェクト）";

	int lightIndex = getLightIndex(lightName);
	int ambIndex   = getAmbIndex(ambName);
	unk10          = new TLightDrawBuffer*[unk1C];
	for (int i = 0; i < unk1C; ++i) {
		unk10[i] = new TLightDrawBuffer(
		    i, 0x100, TLightCommon::mAmbAry->getAmb(ambIndex + i)->getName());
		TLightCommon* light = new TLightCommon();
		unk10[i]->setLight(light);
		unk10[i]->unk10->unk20 = ambIndex;
		unk10[i]->unk10->unk24 = lightIndex;
		unk10[i]->unk10->loadAfter();
	}
}

void TMapObjectLightWithDBSet::makeDrawBuffer()
{
	static const char lightName[] = "太陽（オブジェクト）";
	static const char ambName[]   = "太陽アンビエント（オブジェクト）";

	int lightIndex = getLightIndex(lightName);
	int ambIndex   = getAmbIndex(ambName);
	static const char* className[]
	    = { "マップオブジェ太陽", "マップオブジェ影" };
	unk10 = new TLightDrawBuffer*[unk1C];
	for (int i = 0; i < unk1C; ++i) {
		unk10[i]            = new TLightDrawBuffer(i, 0x100, className[i]);
		TLightCommon* light = new TLightCommon();
		unk10[i]->setLight(light);
		unk10[i]->unk10->unk20 = ambIndex;
		unk10[i]->unk10->unk24 = lightIndex;
		unk10[i]->unk10->loadAfter();
	}
}

void TIndirectLightWithDBSet::makeDrawBuffer()
{
	static const char lightName[] = "太陽（オブジェクト）";
	static const char ambName[]   = "太陽アンビエント（オブジェクト）";

	int lightIndex = getLightIndex(lightName);
	int ambIndex   = getAmbIndex(ambName);
	static const char* className[]
	    = { "インダイレクト太陽", "インダイレクト影" };
	unk10 = new TLightDrawBuffer*[unk1C];
	for (int i = 0; i < unk1C; ++i) {
		unk10[i]            = new TLightDrawBuffer(i, 0x100, className[i]);
		TLightCommon* light = new TLightCommon();
		unk10[i]->setLight(light);
		unk10[i]->unk10->unk20 = ambIndex;
		unk10[i]->unk10->unk24 = lightIndex;
		unk10[i]->unk10->loadAfter();
	}
}

TPlayerLightWithDBSet::TPlayerLightWithDBSet()
    : TLightWithDBSet(2, "プレイヤー用ライト")
{
}

TObjectLightWithDBSet::TObjectLightWithDBSet()
    : TLightWithDBSet(2, "オブジェクト用ライト")
{
}

TMapObjectLightWithDBSet::TMapObjectLightWithDBSet()
    : TLightWithDBSet(2, "マップオブジェクト用ライト")
{
}

TIndirectLightWithDBSet::TIndirectLightWithDBSet()
    : TLightWithDBSet(2, "インダイレクトモデル用ライト")
{
}

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

void TLightWithDBSetManager::loadAfter()
{
	JDrama::TLightAry* group
	    = JDrama::TNameRefGen::search<JDrama::TLightAry>("Light Group");
	unk18 = group->getLight(0)->getColor();
	unk1C = group->getLight(0)->mPosition;
}

void TLightWithDBSetManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_LIGHT) {
		int start;
		int end;
		if (cue & CUE_UNK80000) {
			start = 3;
			end   = 4;
		} else if (cue & CUE_UNK40000) {
			start = 2;
			end   = 3;
		} else {
			start = 0;
			end   = 2;
		}
		for (int i = start; i < end; ++i)
			if (unk14[i]->isEnabled())
				unk14[i]->perform(cue, graphics);
	}
	if (cue & CUE_SET_DRAW_BUFFER) {
		for (int i = 0; i < 4; ++i)
			if (unk14[i]->isEnabled())
				unk14[i]->perform(cue, graphics);
	}
}

void TLightWithDBSetManager::addChildGroupObj(
    JDrama::TViewObjPtrListT<JDrama::TViewObj>* list)
{
	for (int i = 0; i < 4; ++i)
		unk14[i]->addChildGroupObj(list);
}

void TLightWithDBSetManager::makeDrawBuffer()
{
	for (int i = 0; i < 4; ++i)
		if (unk14[i]->isEnabled())
			unk14[i]->makeDrawBuffer();
}

Vec* TLightWithDBSetManager::getLightPos() const
{
	return TLightCommon::mLightPos;
}

void TLightWithDBSetManager::setEffectLight(const JDrama::TGraphics* gfx,
                                            GXLightObj* light)
{
	if (unk54 && unk55) {
		Vec epos;
		MTXMultVec(gfx->getViewMtx(), unk1C, &epos);
		GXInitLightPos(light, epos.x, epos.y, epos.z);
		GXInitLightColor(light, getEffectLightColor());
		GXInitLightAttnA(light, 1.0f, 0.0f, 0.0f);
		GXInitLightDistAttn(light, 1000.0f, 0.5f, GX_DA_STEEP);
		GXLoadLightObjImm(light, GX_LIGHT1);
	}
}

GXColor TLightWithDBSetManager::getEffectLightColor() const
{
	GXColor result = unk18;
	result.a *= unk28;
	return result;
}

void TLightWithDBSetManager::calcLightBorder()
{
	f32 a[3] = { 0.9f, 0.5f, 0.05f };
	f32 b[3];
	b[0] = unk2C;
	b[1] = unk30;
	b[2] = unk34;

	f32 P[2];
	f32 Q[2];
	f32 R[2];
	for (int i = 0; i < 2; ++i) {
		P[i] = a[i + 1] * (a[i] * (b[i] * b[i] - b[i + 1] * b[i + 1]));
		Q[i] = a[i + 1] * (a[i] * (b[i] - b[i + 1]));
		R[i] = a[i + 1] - a[i];
	}

	unk40 = (R[0] * Q[1] - R[1] * Q[0]) / (P[0] * Q[1] - P[1] * Q[0]);
	unk3C = (R[0] - P[0] * unk40) / Q[0];
	unk38 = a[0] - (unk40 * (b[0] * b[0]) + b[0] * unk3C);
}
