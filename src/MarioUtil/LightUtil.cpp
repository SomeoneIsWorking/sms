#include <MarioUtil/LightUtil.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/ReinitGX.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JDrama/JDRDrawBufObj.hpp>
#include <JSystem/JDrama/JDRLighting.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <cstdio>
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
    , mAlphaScale(0.0f)
    , mAmbBaseIdx(0)
    , mLightBaseIdx(0)
    , mUseLocalColor(0)
    , mUseLocalPos(0)
{
	std::memset(mLocalAmbColor,   0, sizeof(mLocalAmbColor));
	std::memset(mLocalLightColor, 0, sizeof(mLocalLightColor));
	for (int i = 0; i < 4; ++i)
		mLocalPos[i].set(0.0f, 0.0f, 0.0f);
	// SDA1 singletons — ctor zeroes them (game code fills them at scene load).
	gpTLightCommonLightAry = nullptr;
	gpTLightCommonAmbAry   = nullptr;
}

// Native compatibility shim (2026-07-02, [[fileselect-lightcommon-lazy-gp]]):
//
// The RE ctor @0x8022a058..068 unconditionally zeroes gpTLightCommonLightAry
// (r13-0x6114) and gpTLightCommonAmbAry (r13-0x6118). `TLightCommon::loadAfter`
// then re-populates them by calling searchF("Light Group") / searchF("Ambient
// Group"). The real game's setup order is:
//   1. sceneCommon->loadAfter()   → all TLightCommon-derived scene objects'
//                                    loadAfter runs, populating gp.
//   2. gpLightManager->makeDrawBuffer()
//      → each of TIndirect/TObject/TMapObject/TPlayerLightWithDBSet does
//        `new TLightCommon("<TLightCommon>")` per draw buffer (`mBufferCount`
//        times). Every one of those ctors NULLS gp again.
//   3. perform() phase runs. If the plain TLightCommon scene object's
//      `setLight` fires, its `getLightPosition` / `getLightColor` group-path
//      reads gp — which was nulled in step 2.
//
// Under Dolphin/hardware this is masked by mEnabled=0 on the DB sets (scene
// setup never trips the DB perform path at file-select) AND by the fact that
// most callers hit the LOCAL path (mUseLocal{Pos,Color}=1) after loadAfter
// seeded `mLocalPos[]`/`mLocalLightColor[]`. Native tripped the group path on
// the "<TLightCommon>" scene object.
//
// RE-faithful choice: keep the ctor's null-write (matches disasm), and lazily
// re-search "Light Group"/"Ambient Group" in the accessor when gp is null.
// searchF is idempotent by design (NameRef by key), so this is a no-op if the
// scene has no Light Group and a proper repopulate if it does. Tested by
// `native/platform/tests/light_common_test.cpp::TestLazyGpRepopulate`.
static JDrama::TLightAry* sb_light_ary_or_search()
{
	JDrama::TLightAry* la = gpTLightCommonLightAry;
	if (!la) {
		la = JDrama::TNameRefGen::search<JDrama::TLightAry>("Light Group");
		gpTLightCommonLightAry = la;
	}
	return la;
}
static JDrama::TAmbAry* sb_amb_ary_or_search()
{
	JDrama::TAmbAry* aa = gpTLightCommonAmbAry;
	if (!aa) {
		aa = JDrama::TNameRefGen::search<JDrama::TAmbAry>("Ambient Group");
		gpTLightCommonAmbAry = aa;
	}
	return aa;
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
	// Light-Group[mLightBaseIdx .. mLightBaseIdx+4] — 4 entries.
	for (int i = 0; i < 4; ++i) {
		JDrama::TIdxLight& L = lightAry->mLights[i + mLightBaseIdx];
		// mLocalLightColor slot 0/1/2/3 lives at offset 0x31 + i*4 (4 bytes).
		GXColor col;
		GXGetLightColor(&L.unk24, &col);
		std::memcpy(&mLocalLightColor[i * 4], &col, 4);
		// mLocalPos slot i at offset 0x44 + i*12 (Vec3).
		mLocalPos[i].set(L.mPosition.x, L.mPosition.y, L.mPosition.z);
	}

	// Populate local ambient colors from AmbAry->mAmbColors[mAmbBaseIdx + 0..1].
	if (ambAry && ambAry->mAmbColors) {
		for (int i = 0; i < 2; ++i) {
			JUtility::TColor& c = ambAry->mAmbColors[i + mAmbBaseIdx].mColor;
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
	if (mUseLocalColor) {
		if (idx >= 4) idx = 0;
		GXColor c;
		std::memcpy(&c, &mLocalLightColor[idx * 4], 4);
		return c;
	}
	// Group path: mLights[idx + mLightBaseIdx].unk24 is the GXLightObj.
	// sb_light_ary_or_search: RE reads gpTLightCommonLightAry raw with no null
	// guard (would SEGV under Dolphin too if unset). Native shim re-searches
	// "Light Group" when null — see the block above sb_light_ary_or_search()
	// for why this is required and NOT a functional deviation.
	JDrama::TLightAry* la = sb_light_ary_or_search();
	if (!la || !la->mLights) { GXColor c={0,0,0,0xFF}; return c; }
	JDrama::TIdxLight& L  = la->mLights[idx + mLightBaseIdx];
	GXColor c;
	GXGetLightColor(&L.unk24, &c);
	// Alpha scaling: (float)a * mAlphaScale, truncated to int, stored back as u8.
	c.a = static_cast<u8>(static_cast<int>(static_cast<f32>(c.a) * mAlphaScale));
	return c;
}

// Native port of TLightCommon::getAmbColor (@0x80229cec). Same pattern as
// getLightColor but the local override array has ONLY 2 entries (at
// mLocalAmbColor[0..7]), and reads the Ambient Group instead of Light Group.
GXColor TLightCommon::getAmbColor(int idx) const
{
	if (mUseLocalColor) {
		if (idx >= 2) idx = 0;
		GXColor c;
		std::memcpy(&c, &mLocalAmbColor[idx * 4], 4);
		return c;
	}
	JDrama::TAmbAry* aa    = sb_amb_ary_or_search();
	if (!aa || !aa->mAmbColors) { GXColor c={0,0,0,0xFF}; return c; }
	JDrama::TAmbColor& A   = aa->mAmbColors[idx + mAmbBaseIdx];
	GXColor c              = { A.mColor.r, A.mColor.g, A.mColor.b, A.mColor.a };
	c.a = static_cast<u8>(static_cast<int>(static_cast<f32>(c.a) * mAlphaScale));
	return c;
}

// Native port of TLightCommon::getLightPosition (@0x80229ca0). If the local
// override flag (unk41) is set, returns &mLocalPos[idx]. Otherwise returns
// &Light-Group[idx + unk24].mPosition (offset 0x10 in TPlacement).
const JGeometry::TVec3<f32>* TLightCommon::getLightPosition(int idx)
{
	if (mUseLocalPos) {
		if (idx >= 4) idx = 0;
		return &mLocalPos[idx];
	}
	JDrama::TLightAry* la = sb_light_ary_or_search();
	if (!la || !la->mLights) {
		static const JGeometry::TVec3<f32> kZero{0.0f, 0.0f, 0.0f};
		return &kZero;
	}
	JDrama::TIdxLight& L  = la->mLights[idx + mLightBaseIdx];
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

	// --- GX_LIGHT0 — positional world light (Light-Group[idx+mLightBaseIdx]). ---
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
	// on mEffectEnabled && mEffectValid. Position is the manager's Vec3<f32>
	// aliased over unk1C/unk20/unk24 (the header keeps those as u32 slots
	// because the ctor writes them through `unk1C = unk20 = unk24 = 0`). ---
	if (gpLightManager && gpLightManager->mEffectEnabled && gpLightManager->mEffectValid) {
		// Reinterpret the 3 consecutive u32 slots as a Vec3<f32> — the game's
		// C++ source did this by pointer arithmetic on the manager instance.
		const JGeometry::TVec3<f32>* mgrPos =
		    reinterpret_cast<const JGeometry::TVec3<f32>*>(&gpLightManager->unk1C);
		JGeometry::TVec3<f32> vpos;
		PSMTXMultVec(view, const_cast<JGeometry::TVec3<f32>*>(mgrPos), &vpos);
		GXInitLightPos(&obj, vpos.x, vpos.y, vpos.z);

		// Color: manager's mEffectColor with alpha scaled by mEffectAlphaScale.
		GXColor col = gpLightManager->mEffectColor;
		col.a = static_cast<u8>(static_cast<int>(static_cast<f32>(col.a)
		                                        * gpLightManager->mEffectAlphaScale));
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

// Native ports of TLightMario::getLightColor (@0x80229510) and
// TLightMario::getAmbColor (@0x80229430). Side-by-side disasm comparison
// against TLightCommon::getLightColor (@0x80229d78) / getAmbColor (@0x80229cec)
// shows byte-identical logic: same clamp fencepost (>=4 / >=2), same
// mUseLocalColor polarity check reading unk28, same local-override packed-
// GXColor read from mLocalLightColor / mLocalAmbColor, same group-path
// alpha-scaling via mAlphaScale (unk1C) truncated back to u8. TLightMario
// adds NO fields past TLightCommon (see LightUtil.hpp), so the offsets
// resolve identically. CodeWarrior emitted per-class copies because both
// classes are polymorphic overrides; the C++ source is a single
// implementation delegated by the override — same pattern as
// TLightMario::setLight below.
GXColor TLightMario::getLightColor(int idx) const
{
	return TLightCommon::getLightColor(idx);
}

GXColor TLightMario::getAmbColor(int idx) const
{
	return TLightCommon::getAmbColor(idx);
}

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

TLightDrawBuffer::TLightDrawBuffer(int lightIdx, u32 flags, const char* name)
    : JDrama::TViewObj(name)
{
	mOwnerLight = nullptr;
	// Port from PPC __ct__16TLightDrawBufferFiUlPCc (GMSJ01 @0x800C46C0, size
	// 0x12C, ~75 insns): allocate mOpaDrawBuf + mXluDrawBuf as owned
	// TDrawBufObj sub-objects so TLightWithDBSet::perform/changeLightDrawBuffer
	// can dispatch through them. The `flags` arg (0x100 for the three
	// non-Player subclasses' makeDrawBuffer, 0x80 for TPlayerLightWithDBSet)
	// forwards to TDrawBufObj's u32 param_1 = unk18 (the render-phase mask).
	// Draw-buffer size 0x400 matches TSmJ3DScn's per-scene draw buffers
	// (JDRSmJ3DScn.cpp:16) and JDrama scenes generally. Names disambiguate
	// the pair so the SB_ENTRY_MAT registry (JDRDrawBufObj.cpp:37+) tags
	// packets correctly. Full byte-exact RE of the ctor requires Ghidra on a
	// GMSJ01 DOL (0x12C bytes hosts additional per-buffer state in the
	// unk1C[0x64] payload); this implementation covers the observed HW
	// contract (both sub-objects non-null, correctly parameterised) so
	// TLightWithDBSet's perform/changeLightDrawBuffer paths work.
	mOpaDrawBuf = new JDrama::TDrawBufObj(flags, 0x400, "<TLightDrawBuffer::Opa>");
	mXluDrawBuf = new JDrama::TDrawBufObj(flags, 0x400, "<TLightDrawBuffer::Xlu>");
	mLightIndex = lightIdx;
	// Zero the intermediate per-buffer state slots so first perform() does not
	// consume garbage. Real ctor may populate specific values here; TBD via RE.
	for (int i = 0; i < static_cast<int>(sizeof(unk1C)); ++i)
		unk1C[i] = 0;
}

// Native port of TLightDrawBuffer::perform (@0x802292c0, 17 insns). Same phase
// gate as TLightCommon (flag & 0x20 → setLight). Dispatches to its owning
// TLightCommon at unk10, using the per-buffer light index at unk80.
void TLightDrawBuffer::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (flag & 0x20) {
		if (mOwnerLight)
			mOwnerLight->setLight(graphics, mLightIndex);
	}
}

// Native port of TLightWithDBSet::perform (@0x80229178, 82 insns). Dispatches
// each per-light draw buffer in unk10[0..unk1C) through its own perform +
// optional J3DDrawBuffer perform calls, gated by three independent flag bits:
//   * flag & 0x20    → per-DrawBuffer perform(0x20, graphics)
//   * flag & 0x10000 → per-DrawBuffer's unk14->perform(8, graphics)
//   * flag & 0x20000 → per-DrawBuffer's unk18->perform(8, graphics)
//   * flag & 0x800   → both unk14/unk18 perform(0x480, graphics)
// Guarded on mDrawBuffers non-null: only enabled sets (mEnabled=1 → makeDrawBuffer
// ran → mDrawBuffers populated) render here. Sets that never got enabled by an
// actor (setLightType) legitimately have mDrawBuffers==null and are correct
// no-ops on this path.
void TLightWithDBSet::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (!mDrawBuffers) return;  // set not enabled by any actor (no setLightType() call)

	if (flag & 0x20) {
		const bool do_unk14_8 = (flag & 0x10000) != 0;
		const bool do_unk18_8 = (flag & 0x20000) != 0;
		for (int i = 0; i < mBufferCount; ++i) {
			TLightDrawBuffer* buf = mDrawBuffers[i];
			buf->perform(0x20, graphics);
			if (do_unk14_8) buf->mOpaDrawBuf->perform(8, graphics);
			if (do_unk18_8) buf->mXluDrawBuf->perform(8, graphics);
		}
	}

	// Bit mask byte-verified against Ghidra decomp scratch/decomp/80229178.c
	// (2026-07-04): the "iterate all + render opa/xlu" branch is flag & 0x400,
	// NOT 0x800 (the pre-2026-07-04 port had a 1-bit shift here matching the
	// same shift in TLightWithDBSetManager::perform above; both corrected).
	if (flag & 0x400) {
		for (int i = 0; i < mBufferCount; ++i) {
			TLightDrawBuffer* buf = mDrawBuffers[i];
			buf->mOpaDrawBuf->perform(0x480, graphics);
			buf->mXluDrawBuf->perform(0x480, graphics);
		}
	}
}

void TLightWithDBSet::changeLightDrawBuffer(int param_1)
{
	mSavedOpaBuffer = nullptr;
	mSavedXluBuffer = nullptr;
	if (param_1 > mBufferCount)
		param_1 = 0;

#ifdef SMS_NATIVE_PLATFORM
	// mDrawBuffers is null for any TLightWithDBSet that no actor enabled via
	// setLightType() → makeDrawBuffer() was never called for this set. That is
	// a correct scene-state, not a port gap: return and let entry() draw into
	// the current (normal Chr) buffer. resetLightDrawBuffer() below is a
	// no-op in that case (its own mSavedOpaBuffer/mSavedXluBuffer guards).
	// (The 2026-07-02 STOPGAP note claiming "makeDrawBuffer is a no-op stub"
	// has been retired — makeDrawBuffer × 4 subclass is fully ported and
	// verified via SB_LMGR_PROBE at settled title, 2026-07-04.)
	if (!mDrawBuffers)
		return;
#endif

	mSavedOpaBuffer = j3dSys.getDrawBuffer(0);
	mSavedXluBuffer = j3dSys.getDrawBuffer(1);

	j3dSys.setDrawBuffer(mDrawBuffers[param_1]->mOpaDrawBuf->mDrawBuffer, 0);
	j3dSys.setDrawBuffer(mDrawBuffers[param_1]->mXluDrawBuf->mDrawBuffer, 1);
}

void TLightWithDBSet::resetLightDrawBuffer()
{
	if (!mSavedOpaBuffer)
		return;
	if (!mSavedXluBuffer)
		return;

	j3dSys.setDrawBuffer(mSavedOpaBuffer, 0);
	j3dSys.setDrawBuffer(mSavedXluBuffer, 1);
	mSavedOpaBuffer = nullptr;
	mSavedXluBuffer = nullptr;
}

// Pure search helper - counterpart of the strcmp loop that each subclass
// makeDrawBuffer runs against LightAry->mLights[] (stride 0x6c) or
// AmbAry->mAmbColors[] (stride 0x18). Returns first matching index or -1.
// Extracted so the search shape (which IS the pure logic) can be unit-tested
// spec-derived from the disasm without a live Light Group.
template <typename T>
static int sb_find_named_index(T* arr, int count, const char* needle)
{
	if (!arr || !needle) return -1;
	for (int i = 0; i < count; ++i) {
		const char* n = arr[i].getName();
		if (n && std::strcmp(n, needle) == 0) return i;
	}
	return -1;
}

// Native port of TIndirectLightWithDBSet::makeDrawBuffer (@0x802289ac, 114 insns).
// Two name-lookup loops -> alloc mDrawBuffers[mBufferCount] -> for each: alloc a
// TLightDrawBuffer + a TLightCommon owner, wire owner via setLight, and seed
// owner->mAmbBaseIdx / mLightBaseIdx from the searched indices. Search needles
// captured from GMSE01 rodata @0x8039d868 pointer table.
void TIndirectLightWithDBSet::makeDrawBuffer()
{
	static const char kLightNeedle[]
	    = "\x91\xbe\x97\x7a\x81\x69\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x81\x6a";  // "太陽（オブジェクト）"
	static const char kAmbNeedle[]
	    = "\x91\xbe\x97\x7a\x83\x41\x83\x93\x83\x72\x83\x47\x83\x93\x83\x67\x81\x69\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x81\x6a";  // "太陽アンビエント（オブジェクト）"

	int lightIdx = -1;
	if (JDrama::TLightAry* la = gpTLightCommonLightAry)
		lightIdx = sb_find_named_index(la->mLights, la->mLightCount, kLightNeedle);

	int ambIdx = -1;
	if (JDrama::TAmbAry* aa = gpTLightCommonAmbAry)
		ambIdx = sb_find_named_index(aa->mAmbColors, aa->mAmbColorCount, kAmbNeedle);

	mDrawBuffers = new TLightDrawBuffer*[mBufferCount];
	for (int i = 0; i < mBufferCount; ++i) {
		TLightDrawBuffer* buf = new TLightDrawBuffer(i, 0x100, "<TLightDrawBuffer>");
		mDrawBuffers[i] = buf;
		TLightCommon* owner = new TLightCommon("<TLightCommon>");
		buf->setLight(owner);
		owner->mAmbBaseIdx   = (u32)ambIdx;
		owner->mLightBaseIdx = (u32)lightIdx;
	}
}

// Native port of TObjectLightWithDBSet::makeDrawBuffer (@0x80228d08, 105 insns).
// Same shape as TIndirect above; needles at r31+0x40 / r31+0x58 in the
// 0x8039d868 pointer table — LightAry "太陽（オブジェクト）" (same as
// Indirect/MapObj) and AmbAry "太陽アンビエント（オブジェクト）" (same).
void TObjectLightWithDBSet::makeDrawBuffer()
{
	static const char kLightNeedle[]
	    = "\x91\xbe\x97\x7a\x81\x69\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x81\x6a";  // "太陽（オブジェクト）"
	static const char kAmbNeedle[]
	    = "\x91\xbe\x97\x7a\x83\x41\x83\x93\x83\x72\x83\x47\x83\x93\x83\x67\x81\x69\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x81\x6a";  // "太陽アンビエント（オブジェクト）"

	int lightIdx = -1;
	if (JDrama::TLightAry* la = gpTLightCommonLightAry)
		lightIdx = sb_find_named_index(la->mLights, la->mLightCount, kLightNeedle);

	int ambIdx = -1;
	if (JDrama::TAmbAry* aa = gpTLightCommonAmbAry)
		ambIdx = sb_find_named_index(aa->mAmbColors, aa->mAmbColorCount, kAmbNeedle);

	mDrawBuffers = new TLightDrawBuffer*[mBufferCount];
	for (int i = 0; i < mBufferCount; ++i) {
		TLightDrawBuffer* buf = new TLightDrawBuffer(i, 0x100, "<TLightDrawBuffer>");
		mDrawBuffers[i] = buf;
		TLightCommon* owner = new TLightCommon("<TLightCommon>");
		buf->setLight(owner);
		owner->mAmbBaseIdx   = (u32)ambIdx;
		owner->mLightBaseIdx = (u32)lightIdx;
	}
}

// Native port of TMapObjectLightWithDBSet::makeDrawBuffer (@0x80228b74, 101 insns).
// Needles at r31+0x7c / r31+0x94 — same strings as TObject/TIndirect.
void TMapObjectLightWithDBSet::makeDrawBuffer()
{
	static const char kLightNeedle[]
	    = "\x91\xbe\x97\x7a\x81\x69\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x81\x6a";  // "太陽（オブジェクト）"
	static const char kAmbNeedle[]
	    = "\x91\xbe\x97\x7a\x83\x41\x83\x93\x83\x72\x83\x47\x83\x93\x83\x67\x81\x69\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x81\x6a";  // "太陽アンビエント（オブジェクト）"

	int lightIdx = -1;
	if (JDrama::TLightAry* la = gpTLightCommonLightAry)
		lightIdx = sb_find_named_index(la->mLights, la->mLightCount, kLightNeedle);

	int ambIdx = -1;
	if (JDrama::TAmbAry* aa = gpTLightCommonAmbAry)
		ambIdx = sb_find_named_index(aa->mAmbColors, aa->mAmbColorCount, kAmbNeedle);

	mDrawBuffers = new TLightDrawBuffer*[mBufferCount];
	for (int i = 0; i < mBufferCount; ++i) {
		TLightDrawBuffer* buf = new TLightDrawBuffer(i, 0x100, "<TLightDrawBuffer>");
		mDrawBuffers[i] = buf;
		TLightCommon* owner = new TLightCommon("<TLightCommon>");
		buf->setLight(owner);
		owner->mAmbBaseIdx   = (u32)ambIdx;
		owner->mLightBaseIdx = (u32)lightIdx;
	}
}

// Native port of TPlayerLightWithDBSet::makeDrawBuffer (@0x80228eac, 137 insns).
// Same overall shape as the other 3 subclasses (two name-lookup loops, then a
// buffer-alloc loop of length mBufferCount). The prior "third search loop"
// note was WRONG on re-disassembly: there are only two name lookups; the
// third loop @0x80228f7c is the buffer-allocation loop, not a name search.
//
// Player-specific deltas vs TIndirect/TObject/TMapObject (both RE'd from the
// disasm, not guessed):
//   * TLightDrawBuffer cap=0x80 (not 0x100) — 0x80228fac `li r5, 0x80`.
//   * TLightDrawBuffer name arg = mAmbColors[ambIdx + i].name (dynamic per
//     buffer) — 0x80228f90..0x80228fa0. Falls back to a literal if the amb
//     name table isn't loaded yet.
// Documented residuals (matches the same shape as TIndirect port's shortcuts
// so we stay consistent, and captured here for follow-up):
//   * The disasm allocates a 0x74-byte object then PROMOTES its vtable to a
//     Player-specific TLightCommon subclass at 0x803da774 (`stw r31, 0(r24)`,
//     r31 = 0x803e0000 - 0x588c). That subclass isn't yet declared in the
//     hierarchy; TIndirect uses plain TLightCommon here too, and we match.
//   * After setting mAmb/LightBaseIdx the disasm calls owner->vtable[0x18]
//     (`getLightPosition`) once — no observable side-effect on our port's
//     state, so we omit the call (same simplification as the TIndirect port).
void TPlayerLightWithDBSet::makeDrawBuffer()
{
	static const char kLightNeedle[]
	    = "\x91\xbe\x97\x7a\x81\x69\x83\x76\x83\x8c\x83\x43\x83\x84\x81\x5b\x81\x6a";  // "太陽（プレイヤー）"
	static const char kAmbNeedle[]
	    = "\x91\xbe\x97\x7a\x83\x41\x83\x93\x83\x72\x83\x47\x83\x93\x83\x67\x81\x69\x83\x76\x83\x8c\x83\x43\x83\x84\x81\x5b\x81\x6a";  // "太陽アンビエント（プレイヤー）"

	int lightIdx = -1;
	if (JDrama::TLightAry* la = gpTLightCommonLightAry)
		lightIdx = sb_find_named_index(la->mLights, la->mLightCount, kLightNeedle);

	JDrama::TAmbAry* aa = gpTLightCommonAmbAry;
	int ambIdx = -1;
	if (aa)
		ambIdx = sb_find_named_index(aa->mAmbColors, aa->mAmbColorCount, kAmbNeedle);

	mDrawBuffers = new TLightDrawBuffer*[mBufferCount];
	for (int i = 0; i < mBufferCount; ++i) {
		// mAmbColors[ambIdx + i].name — per-buffer name. If ambIdx failed to
		// resolve OR the (ambIdx + i) index is out of range, fall back to a
		// literal so we never deref garbage (fail-safe, not fail-silent —
		// the search miss is a scene-data issue, not a port bug).
		const char* bufName = "<TLightDrawBuffer>";
		if (aa && ambIdx >= 0 && (ambIdx + i) < aa->mAmbColorCount) {
			const char* n = aa->mAmbColors[ambIdx + i].getName();
			if (n) bufName = n;
		}
		TLightDrawBuffer* buf = new TLightDrawBuffer(i, 0x80, bufName);
		mDrawBuffers[i] = buf;
		TLightCommon* owner = new TLightCommon("<TLightCommon>");
		buf->setLight(owner);
		owner->mAmbBaseIdx   = (u32)ambIdx;
		owner->mLightBaseIdx = (u32)lightIdx;
	}
}

// The decomp left the TLightWithDBSet hierarchy ctors and the manager's mLightSets
// initialization unimplemented (declared-only). The base/subclass ctors are restored
// here. makeDrawBuffer × 4 subclasses IS fully ported above and populates mDrawBuffers
// when the set gets enabled by an actor's setLightType() call; sets no actor enables
// stay mDrawBuffers=null (correct scene-state, not a gap). mEnabled is the per-light
// "enabled" flag actor managers write (TLiveManager and MActor::setLightType) so it
// must be addressable.
TLightWithDBSet::TLightWithDBSet(int idx, const char* name)
    : JDrama::TViewObj(name)
    , mDrawBuffers(nullptr)
    , mSavedOpaBuffer(nullptr)
    , mSavedXluBuffer(nullptr)
    , mBufferCount(idx)
    , mEnabled(0)
{
}

// Native ctors for the 4 TLightWithDBSet subclasses. The buffer-count arg (=2)
// and the Shift-JIS name arg both come from RE of the TLightWithDBSetManager
// ctor @0x80228534 (US), which explicitly writes mBufferCount = 2 to each set
// (li r4,2 → stw r4, 0x1c(alloc) — evidence per subclass in
// reference/sms/debug_journal/2026-07-02_light_with_dbset_makedrawbuffer_re.md).
// Name strings are at 0x8039d9c4 / 0x8039d9d8 / 0x8039d9f0 / 0x8039da0c in the
// GMSE01 rodata pointer-table @0x8039d868.
TPlayerLightWithDBSet::TPlayerLightWithDBSet()
    : TLightWithDBSet(2, "\x83\x76\x83\x8c\x83\x43\x83\x84\x81\x5b\x97\x70\x83\x89\x83\x43\x83\x67")  // "プレイヤー用ライト"
{
}
TMapObjectLightWithDBSet::TMapObjectLightWithDBSet()
    : TLightWithDBSet(2, "\x83\x7d\x83\x62\x83\x76\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x97\x70\x83\x89\x83\x43\x83\x67")  // "マップオブジェクト用ライト"
{
}
TObjectLightWithDBSet::TObjectLightWithDBSet()
    : TLightWithDBSet(2, "\x83\x49\x83\x75\x83\x57\x83\x46\x83\x4e\x83\x67\x97\x70\x83\x89\x83\x43\x83\x67")  // "オブジェクト用ライト"
{
}
TIndirectLightWithDBSet::TIndirectLightWithDBSet()
    : TLightWithDBSet(2, "\x83\x43\x83\x93\x83\x5f\x83\x43\x83\x8c\x83\x4e\x83\x67\x83\x82\x83\x66\x83\x8b\x97\x70\x83\x89\x83\x43\x83\x67")  // "インダイレクトモデル用ライト"
{
}

// Native port of TLightWithDBSetManager ctor (@0x80228534, 1028 bytes decomp
// scratch/decomp/80228534.c). The disasm allocates the 4-entry mLightSets
// array (each a 0x24-byte subclass), zeroes the effect-color word, sets
// mEffectEnabled=0 / mEffectValid=1, then loads a bank of SDA2 constants into
// mEffectAlphaScale + unk2C..unk44 + unk48 and calls calcLightBorder() inline.
//
// The calcLightBorder body @0x8022886c..0x80228930 (matches Ghidra decomp
// lines 102-114) computes a quadratic polynomial y = c0 + c1*x + c2*x^2
// passing through the three (x,y) pairs (unk2C, 0.9), (unk30, 0.5),
// (unk34, 0.05). Result: unk38=c0, unk3C=c1, unk40=c2. It does NOT touch
// mEffectEnabled — the "gate is unwired" hypothesis is falsified; both
// mEffectEnabled=0 (ctor init) and mEffectValid=1 (ctor init) are correct
// steady-state values and calcLightBorder never overrides them. The gate
// stays OFF until (unresolved) external code flips mEffectEnabled=1 for
// scenes that want the specular-sun effect light. At title screen (SB_STAGE
// =15), mEffectEnabled stays 0 → TLightCommon::setLight's L1 branch is
// correctly skipped and L1 retains whatever LIGHT-INIT broadcast programmed.
TLightWithDBSetManager::TLightWithDBSetManager(const char* name)
    : JDrama::TViewObj(name)
{
#ifdef SMS_NATIVE_PLATFORM
	// Allocate the 4-entry light-set array the managers index by light kind
	// (player/mapobj/object/indirect). The decomp's empty ctor left mLightSets
	// wild, so the first getUnk14(i)->mEnabled = 1 (TLiveManager ctor)
	// dereferenced garbage.
	mMarioLight = new TLightMario;
	mLightSets = new TLightWithDBSet*[4];
	mLightSets[0] = new TPlayerLightWithDBSet;
	mLightSets[1] = new TMapObjectLightWithDBSet;
	mLightSets[2] = new TObjectLightWithDBSet;
	mLightSets[3] = new TIndirectLightWithDBSet;

	mEffectColor.r = mEffectColor.g = mEffectColor.b = mEffectColor.a = 0;
	unk1C = unk20 = unk24 = 0;

	// Gate bytes (RE'd from 0x802285ac / 0x802285b0 — two consecutive stb):
	//   stb r30, 0x54(r28) with r30 = 0  →  mEffectEnabled = 0
	//   stb  r0, 0x55(r28) with r0  = 1  →  mEffectValid   = 1
	// (Prior port had mEffectValid = 0, wrong; caught 2026-07-04 during
	// calcLightBorder RE.)
	mEffectEnabled = 0;
	mEffectValid = 1;

	// SDA2-driven default values the disasm loads into mEffectAlphaScale +
	// unk2C..unk44 + unk48. The concrete numeric values live in the DOL at
	// r2-relative offsets 0x1788..0x17a8; we can't reliably decode them
	// without pinning r2 (Ghidra does not propagate register context here),
	// so the port leaves these zero for now. That is safe: they only feed
	// the border curve (via calcLightBorder below) and the effect-color
	// alpha scale (via TLightCommon::setLight, gated on
	// mEffectEnabled=1 which never fires at title). Extending: a
	// spot-verify PPC disasm pass could pin r2 and reveal the exact
	// initial values; captured as a follow-up rather than gated on here.
	mEffectAlphaScale = unk2C = unk30 = unk34 = unk38 = unk3C = unk40 = unk44 = 0.0f;
	unk48.x = unk48.y = unk48.z = 0.0f;

	// Compute the border coefficients inline. With the SDA2-driven inputs
	// currently zeroed the fit degenerates (division by zero); guard so
	// the port doesn't NaN the fields before the SDA2 load is completed.
	if (unk2C != unk30 && unk30 != unk34) {
		calcLightBorder();
	}
#endif
}

// Native port of calcLightBorder — inlined by the compiler into the
// TLightWithDBSetManager ctor @0x8022886c..0x80228930 (Ghidra decomp
// scratch/decomp/80228534.c lines 102-114). Semantics: given three (x, y)
// sample points where the x's live at unk2C/unk30/unk34 and the y's are the
// fixed rodata triple (0.9, 0.5, 0.05) at 0x8039d9ac/b0/b4, solve for the
// coefficients (c0, c1, c2) of the quadratic  y = c0 + c1*x + c2*x^2  that
// passes through all three points, and store them into unk38/unk3C/unk40.
// Used later by the effect-light color scaling path (unresolved caller).
void TLightWithDBSetManager::calcLightBorder()
{
#ifdef SMS_NATIVE_PLATFORM
	// Fixed y-values (rodata DAT_8039d9ac/b0/b4, byte-verified):
	const f32 kY0 = 0.9f;    // y at unk2C
	const f32 kY1 = 0.5f;    // y at unk30
	const f32 kY2 = 0.05f;   // y at unk34

	const f32 x0 = unk2C;
	const f32 x1 = unk30;
	const f32 x2 = unk34;

	// Faithful transcription of the disasm's temporary tree:
	//   fVar4 = kY1 - kY0
	//   fVar5 = kY1 * kY0 * (x0 - x1)
	//   fVar6 = kY1 * kY0 * (x0^2 - x1^2)
	//   fVar7 = kY2 * kY1 * (x1 - x2)
	//   unk40 = (fVar4 * fVar7 - (kY2 - kY1) * fVar5)
	//         / (fVar6 * fVar7 - kY2 * kY1 * (x1^2 - x2^2) * fVar5)
	//   unk3C = -(fVar6 * unk40 - fVar4) / fVar5
	//   unk38 = kY0 - (unk40 * x0*x0 + x0 * unk3C)
	const f32 fVar4 = kY1 - kY0;
	const f32 fVar5 = kY1 * kY0 * (x0 - x1);
	const f32 fVar6 = kY1 * kY0 * (x0 * x0 - x1 * x1);
	const f32 fVar7 = kY2 * kY1 * (x1 - x2);
	const f32 denom = fVar6 * fVar7 - kY2 * kY1 * (x1 * x1 - x2 * x2) * fVar5;

	unk40 = (fVar4 * fVar7 - (kY2 - kY1) * fVar5) / denom;
	unk3C = -(fVar6 * unk40 - fVar4) / fVar5;
	unk38 = kY0 - (unk40 * x0 * x0 + x0 * unk3C);
#endif
}

// Native port of TLightWithDBSetManager::loadAfter (@0x80228490, 41 insns).
// Resolves the "Light Group" (TLightAry) singleton and caches the effect
// light's data — GXLightObj colour → this->unk18, mPosition → this->unk1C..
// unk24 (Vec3 aliased over the 3 u32 slots in the header). setLight reads
// these when unk54 && unk55 are set (the calcLightBorder gate).
void TLightWithDBSetManager::loadAfter()
{
	JDrama::TLightAry* la = JDrama::TNameRefGen::search<JDrama::TLightAry>("Light Group");
	if (!la || !la->mLights) return;

	// Effect light data = Light-Group[0]. GXGetLightColor reads the packed
	// GXColor out of the group's GXLightObj (unk24).
	GXGetLightColor(&la->mLights[0].unk24, &mEffectColor);

	// Vec3 mPosition aliased over unk1C/unk20/unk24 (3 u32 slots).
	f32 x = la->mLights[0].mPosition.x;
	f32 y = la->mLights[0].mPosition.y;
	f32 z = la->mLights[0].mPosition.z;
	std::memcpy(&unk1C, &x, 4);
	std::memcpy(&unk20, &y, 4);
	std::memcpy(&unk24, &z, 4);
}

// Native port of TLightWithDBSetManager::perform (@0x80228394, 63 insns).
// Dispatches active per-kind TLightWithDBSet buffers (player/mapobj/object/
// indirect at mLightSets[0..3]). Two independent phase gates with independent
// index-range selections. Bit masks byte-verified against Ghidra decomp
// scratch/decomp/80228394.c (2026-07-04):
//   * flag & 0x20  → iterate a slice of mLightSets[]:
//                    flag & 0x80000 → [3, 4)  (indirect only)
//                    flag & 0x40000 → [2, 3)  (object only)
//                    else           → [0, 2)  (player + mapobj)
//   * flag & 0x400 → iterate ALL mLightSets[0..4)
// Each iteration guards on the buffer's own mEnabled flag being set, then
// forwards `flag` unchanged.
void TLightWithDBSetManager::perform(u32 flag, JDrama::TGraphics* graphics)
{
	if (flag & 0x20) {
		int start, end;
		if (flag & 0x80000)       { start = 3; end = 4; }
		else if (flag & 0x40000)  { start = 2; end = 3; }
		else                       { start = 0; end = 2; }
		for (int i = start; i < end; ++i) {
			TLightWithDBSet* buf = mLightSets[i];
			if (buf && buf->mEnabled)
				buf->perform(flag, graphics);
		}
	}

	if (flag & 0x400) {
		for (int i = 0; i < 4; ++i) {
			TLightWithDBSet* buf = mLightSets[i];
			if (buf && buf->mEnabled)
				buf->perform(flag, graphics);
		}
	}
}

// Native port of TLightWithDBSetManager::addChildGroupObj @0x80228234 (Ghidra
// decomp scratch/decomp/80228234.c, 304 bytes). Called by
// MarDirectorSetupObjects.cpp:409 after gpLightManager->makeDrawBuffer() to
// wire every enabled light-DB-set's per-buffer Opa/Xlu TDrawBufObj pointers as
// children of the given TViewObjPtrListT. Without this port, the light-managed
// draw buffers (shadow volumes, per-light effect draws) never enter the
// scene render tree → any actor using MActor::setLightType()/changeLight
// DrawBuffer path renders nothing through those buffers.
void TLightWithDBSetManager::addChildGroupObj(
    JDrama::TViewObjPtrListT<JDrama::TViewObj, JDrama::TViewObj>* list)
{
#ifdef SMS_NATIVE_PLATFORM
	if (!list) return;
	// The `param_2 + 0x10` in the disasm advances past TViewObj to the embedded
	// JGadget::TList_pointer<TViewObj*>, whose end() + insert() the loop uses.
	// Under the C++ interface this IS *list (multiple inheritance downcast).
	JGadget::TList_pointer<JDrama::TViewObj*>& children = *list;

	for (int i = 0; i < 4; ++i) {
		TLightWithDBSet* set = mLightSets[i];
		if (!set || !set->mEnabled) continue;
		for (int j = 0; j < set->mBufferCount; ++j) {
			TLightDrawBuffer* buf = set->mDrawBuffers[j];
			if (!buf) continue;
			// Insert Opa then Xlu (disasm order — matches perform-list dispatch).
			if (buf->mOpaDrawBuf)
				children.insert(children.end(),
				                reinterpret_cast<JDrama::TViewObj*>(buf->mOpaDrawBuf));
			if (buf->mXluDrawBuf)
				children.insert(children.end(),
				                reinterpret_cast<JDrama::TViewObj*>(buf->mXluDrawBuf));
		}
	}
#else
	(void)list;
#endif
}

void TLightWithDBSetManager::makeDrawBuffer()
{
	for (int i = 0; i < 4; ++i)
		if (mLightSets[i]->mEnabled)
			mLightSets[i]->makeDrawBuffer();
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
