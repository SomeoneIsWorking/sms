// SelectMenu.cpp — file-select screen view-objects.
//
// This translation unit is UN-decompiled in the community decomp (the original was empty).
// Reconstructed faithfully from the GMSE01 DOL. So far: TSelectGrad (the animated background
// gradient). TSelectMenu (the file windows) + TSelectShineManager are reconstructed
// incrementally as the file-select port is fleshed out.
//
// TSelectGrad anchors (DOL): ctor 0x80175a4c, setStageColor 0x8017591c, perform 0x80175560.

#include <GC2D/SelectGrad.hpp>
#include <GC2D/SelectMenu.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/ReinitGX.hpp>
#include <System/Application.hpp>
#include <System/StageUtil.hpp>
#include <System/FlagManager.hpp>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <cstdio>
#include <cstdlib>

// ──────────────────────────────────────────────────────────────────────────────
// TSelectGrad — animated full-screen background gradient.
// ──────────────────────────────────────────────────────────────────────────────

TSelectGrad::TSelectGrad(const char* name)
    : JDrama::TViewObj(name)
{
	// ctor @0x80175a4c: default = stage-0 palette (top-left red, bottom-right yellow).
	mColorIdx[0] = 2;
	mColorIdx[1] = 0;
	mColorIdx[2] = 4;
	mColorA[0] = 0xff; mColorA[1] = 0x00; mColorA[2] = 0x00; mColorA[3] = 0xff; // red
	mColorB[0] = 0xff; mColorB[1] = 0xff; mColorB[2] = 0x00; mColorB[3] = 0xff; // yellow
}

// setStageColor @0x8017591c: pick the gradient palette by stage id.
void TSelectGrad::setStageColor(u8 stage)
{
	switch (stage) {
	case 4:
		mColorIdx[0] = 2; mColorIdx[1] = 0; mColorIdx[2] = 4;
		mColorA[0] = 0xff; mColorA[1] = 0x00; mColorA[2] = 0x00; mColorA[3] = 0xff;
		mColorB[0] = 0xff; mColorB[1] = 0xff; mColorB[2] = 0x00; mColorB[3] = 0xff;
		break;
	case 2:
		mColorIdx[0] = 3; mColorIdx[1] = 1; mColorIdx[2] = 5;
		mColorA[0] = 0xff; mColorA[1] = 0xff; mColorA[2] = 0x00; mColorA[3] = 0xff;
		mColorB[0] = 0x00; mColorB[1] = 0xff; mColorB[2] = 0x00; mColorB[3] = 0xff;
		break;
	case 3:
		mColorIdx[0] = 4; mColorIdx[1] = 2; mColorIdx[2] = 0;
		mColorA[0] = 0x00; mColorA[1] = 0xff; mColorA[2] = 0x00; mColorA[3] = 0xff;
		mColorB[0] = 0x00; mColorB[1] = 0xff; mColorB[2] = 0xff; mColorB[3] = 0xff;
		break;
	case 0xd:
		mColorIdx[0] = 0; mColorIdx[1] = 4; mColorIdx[2] = 2;
		mColorA[0] = 0x00; mColorA[1] = 0x00; mColorA[2] = 0xff; mColorA[3] = 0xff;
		mColorB[0] = 0xff; mColorB[1] = 0x00; mColorB[2] = 0xff; mColorB[3] = 0xff;
		break;
	default:
		// stages 0,1 (and any other) keep the ctor default palette.
		break;
	}
}

namespace {
// Faithful per-channel phase animator (perform & 2 block). `phase` indexes a 6-step cycle;
// at phase 0 the channel ramps up (+2, clamp 255), at phase 3 it ramps down (-2, clamp 0),
// otherwise it holds. Returns true if the channel reached a limit this frame.
bool grad_step(u8* c, int phase)
{
	if (phase == 3) {
		int v = (int)*c - 2;
		if (v < 0) { *c = 0; return true; }
		*c = (u8)v;
	} else if (phase == 0) {
		int v = (int)*c + 2;
		if (v > 0xff) { *c = 0xff; return true; }
		*c = (u8)v;
	}
	return false;
}
} // namespace

// perform @0x80175560.
void TSelectGrad::perform(u32 flags, JDrama::TGraphics* /*gfx*/)
{
	{ static int v=-1; if(v<0){const char*e=getenv("SB_SEL_DBG");v=(e&&e[0]&&e[0]!='0')?1:0;}
	  if(v){static int n=0; if((n++%120)==0) std::fprintf(stderr,"[sel] TSelectGrad::perform flags=0x%x A=%d,%d,%d B=%d,%d,%d\n",
	        flags,mColorA[0],mColorA[1],mColorA[2],mColorB[0],mColorB[1],mColorB[2]); } }
	// ── calc (flag bit 0x2): animate the gradient colours. ──
	if (flags & 0x2) {
		bool advance = false;
		for (int ch = 0; ch < 3; ch++) {
			// B-corner channel steps with the raw phase; A-corner with phase-1 (mod 6).
			advance |= grad_step(&mColorB[ch], mColorIdx[ch]);
			int aphase = mColorIdx[ch] - 1;
			if (aphase < 0) aphase = 5;
			grad_step(&mColorA[ch], aphase);
		}
		if (advance) {
			for (int ch = 0; ch < 3; ch++)
				if (++mColorIdx[ch] > 5) mColorIdx[ch] = 0;
		}
	}

	// ── draw (flag bit 0x8): full-screen gradient quad. ──
	if (flags & 0x8) {
		GXSetDither(GX_TRUE);

		Mtx pos;
		PSMTXIdentity(pos);
		GXLoadPosMtxImm(pos, GX_PNMTX0);

		GXSetCullMode(GX_CULL_BACK);
		GXSetNumTexGens(0);
		GXSetNumTevStages(1);
		GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		GXSetNumChans(1);
		GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, 0,
		              GX_DF_NONE, GX_AF_NONE);
		GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0,
		              GX_DF_NONE, GX_AF_NONE);
		GXColor white = { 0xff, 0xff, 0xff, 0xff };
		GXSetChanAmbColor(GX_COLOR0A0, white);

		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
		GXClearVtxDesc();
		GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);

		// Averaged (mid-edge) colour = (A + B) / 2 per channel.
		u8 avg[3];
		for (int i = 0; i < 3; i++)
			avg[i] = (u8)(((int)mColorA[i] + (int)mColorB[i]) >> 1);

		// Quad in the ortho space (0,16,600,464), z=-100. Corners:
		//   (0,16)  = A  | (600,16)  = avg
		//   (0,464) = avg| (600,464) = B
		GXBegin(GX_QUADS, GX_VTXFMT0, 4);
		GXPosition3f32(0.0f, 16.0f, -100.0f);
		GXColor3u8(mColorA[0], mColorA[1], mColorA[2]);
		GXPosition3f32(600.0f, 16.0f, -100.0f);
		GXColor3u8(avg[0], avg[1], avg[2]);
		GXPosition3f32(600.0f, 464.0f, -100.0f);
		GXColor3u8(mColorB[0], mColorB[1], mColorB[2]);
		GXPosition3f32(0.0f, 464.0f, -100.0f);
		GXColor3u8(avg[0], avg[1], avg[2]);
		GXEnd();
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// TSelectMenu — the file-slot select windows (scenario_select_1.blo J2DScreen).
// ──────────────────────────────────────────────────────────────────────────────

// ctor @0x801753d0 — zero-init; the window-colour indices are (re)set in setup.
TSelectMenu::TSelectMenu(const char* name)
    : JDrama::TViewObj(name)
    , mState(0)
    , mScreen(nullptr)
    , mGamePad(nullptr)
    , mShineMgr(nullptr)
    , mDir(nullptr)
    , mStage(0)
    , mCursor(0)
    , mNumSlots(0)
    , mDisabled(0)
    , mFrameScale(1.0f)
{
	mColorIdx[0] = 0;
	mColorIdx[1] = 0;
	mColorIdx[2] = 0;
}

// setup @0x8017449c — load the file-slot J2DScreen. (See PORT STATUS in the header: this
// milestone loads the screen and lets it draw at its .blo defaults; the ~3.5 KB of
// per-file save-data population and the window panes used by the open animation are TODO.)
void TSelectMenu::setup(u8 stage, JKRArchive* archive, TSelectShineManager* shineMgr,
                        TSelectDir* dir)
{
	mStage      = stage;
	mShineMgr   = shineMgr;
	mDir        = dir;
	mFrameScale = 1.0f / SMSGetAnmFrameRate();

	// Title-ish stages (the DOL's `stage == 10 || stage < 2`) get no select windows:
	// the menu is suppressed and selection defaults to "none".
	if (stage == 10 || stage < 2) {
		mDisabled = 1;
		mCursor   = 0xff;
		return;
	}

	// Window colour indices (DOL: *(this+0x14/0x18/0x1c) = 2,0,4) — used by the
	// not-yet-ported per-window save-data lookups.
	mColorIdx[0] = 2;
	mColorIdx[1] = 0;
	mColorIdx[2] = 4;

	mScreen = new J2DSetScreen("scenario_select_1.blo", archive);

	{ static int v=-1; if(v<0){const char*e=getenv("SB_SEL_DBG");v=(e&&e[0]&&e[0]!='0')?1:0;}
	  if(v){
		std::fprintf(stderr,"[sel] === pane tree dump (stage=%d) ===\n", stage);
		struct W { static void rec(J2DPane* p, int d) {
			if (!p) return;
			char fc[5]={0}; char kc[5]={0};
			u32 ui = (u32)p->mUserInfoTag;  // FourCC name that search() matches
			u32 kk = p->getKind();
			fc[0]=(char)(ui>>24); fc[1]=(char)(ui>>16); fc[2]=(char)(ui>>8); fc[3]=(char)ui;
			kc[0]=(char)(kk>>24); kc[1]=(char)(kk>>16); kc[2]=(char)(kk>>8); kc[3]=(char)kk;
			for(int i=0;i<4;i++){ if(fc[i]<32||fc[i]>126) fc[i]='.'; if(kc[i]<32||kc[i]>126) kc[i]='.'; }
			const JUTRect& b = p->getBounds();
			std::fprintf(stderr,"[sel]  %*s'%s' kind=%s vis=%d alpha=%d bounds=(%d,%d,%d,%d)\n",
				d*2,"",fc,kc,(int)p->isVisible(),(int)p->getAlpha(),b.x1,b.y1,b.x2,b.y2);
			for (JSUTreeIterator<J2DPane> it = p->getFirstChild(); it != p->getEndChild(); ++it)
				rec(it.getObject(), d+1);
		}};
		W::rec(mScreen,0);
		std::fprintf(stderr,"[sel] === end pane tree ===\n");
	  } }

	// ── Per-file save-data population (DOL setup @0x8017449c). ──────────────────────────
	// This is the foundation milestone of that block: it resolves the menu's panes from the
	// scenario_select_1.blo screen and sets their resting (pre-animation) visibility from
	// the save flags. The window-open animation, the slot-indicator (i_o*/i_e*) row, the
	// score/coin digit textures and the BMG stage-name strings are reconstructed in the
	// follow-up steps and are marked TODO below. The leading-byte search tags below are the
	// DOL constants verbatim ("s_0" = 0x00735f30, etc.); J2DScreen::search resolves them
	// against the loaded panes (probed live), so this is a faithful port, not a guess.

	// Hide the spare "0_0" file window (DOL line 1: *(0_0 + 0xC) = 0). With both .s_0 and
	// .0_0 visible at the .blo default, the screen showed "EPISODE 1" twice; the original
	// hides 0_0 immediately, leaving the single centred s_0 window.
	mScreen->search(0x00305f30)->hide(); // "0_0"

	// Stage banner: the .blo defaults the Bianco banner (bi_0) visible; setup hides it and
	// shows the banner for THIS stage. The per-stage 3-char prefix table (DOL @0x80388308,
	// indexed by stage); banner pane tag = (prefix << 8) | 0x30 (e.g. stage 2 → "bi_0",
	// stage 4 → "mm_0").  Stage→prefix: 0,1 absent; 2 bi_,3 rc_,4 mm_,5 pi_,6 sr_,8 mo_,9 mr_.
	static const u32 kStagePrefix[11] = {
	    0, 0, 0x0062695f, 0x0072635f, 0x006d6d5f, 0x0070695f, 0x0073725f, 0,
	    0x006d6f5f, 0x006d725f, 0,
	};
	mScreen->search(0x62695f30)->hide(); // hide default "bi_0" banner group
	if (stage < 11 && kStagePrefix[stage] != 0) {
		u32 bannerTag = (kStagePrefix[stage] << 8) | 0x30;
		if (J2DPane* banner = mScreen->search(bannerTag)) {
			banner->show();
			// The banner-half pictures (e.g. mm_a/mm_b) default hidden for non-Bianco
			// stages; show them so the stage's banner texture draws (the DOL builds the
			// banner picture at runtime — showing the .blo halves is the equivalent).
			for (JSUTreeIterator<J2DPane> it = banner->getFirstChild();
			     it != banner->getEndChild(); ++it)
				it.getObject()->show();
		}
	}

	// The "100-coin" mark over the score row starts hidden (DOL: *(sc_s + 0xC) = 0); it is
	// only revealed when that stage's special-shine flag is set (TODO with the score-mark
	// table). Keeping it hidden is the faithful resting default.
	mScreen->search(0x73635f73)->hide(); // "sc_s"

	// Per-slot episode-mark state + how many file slots exist + the resting cursor. Default
	// every slot to OPEN (2); each collected shine for this stage marks its slot CLEARED (3)
	// and extends the slot count; slots past the count are LOCKED (0). (DOL lines computing
	// *(this+0x150..0x157), *(this+0x13C) numSlots, *(this+0x13B) cursor.)
	u8 shineStage = SMS_getShineStage(stage);
	for (int i = 0; i < 8; i++)
		mEpisodeState[i] = 2;
	mNumSlots = 0;
	for (int i = 0; i < 8; i++) {
		if (SMS_isGetShine(shineStage, (u32)i, false)) {
			mEpisodeState[i] = 3;
			mNumSlots = (u8)(i + 2);
		}
	}
	if (mNumSlots > 8) mNumSlots = 8;
	if (mNumSlots == 0) mNumSlots = 1;
	mCursor = (u8)(mNumSlots >= 1 ? mNumSlots - 1 : 0);
	for (int i = mNumSlots; i < 8; i++)
		mEpisodeState[i] = 0;

	{ static int v=-1; if(v<0){const char*e=getenv("SB_SEL_DBG");v=(e&&e[0]&&e[0]!='0')?1:0;}
	  if(v) std::fprintf(stderr,"[sel] setup: shineStage=%d numSlots=%d cursor=%d states=%d%d%d%d%d%d%d%d\n",
	    shineStage,mNumSlots,mCursor,mEpisodeState[0],mEpisodeState[1],mEpisodeState[2],
	    mEpisodeState[3],mEpisodeState[4],mEpisodeState[5],mEpisodeState[6],mEpisodeState[7]); }

	// TODO(file-select port, next step): score/coin digit textures (coin_number_*.bti →
	// sc_1/sc_2/sc_3 via changeTexture), the slot-indicator i_o*/i_e* row (revealed by the
	// open animation), the BMG stage/scenario-name strings, the sc_s score-mark, and the
	// window-open animation / input navigation in perform's calc path.
}

// perform @0x80172c90 (TViewObj vtable slot 8). calc on flag bit 0x1, draw on bit 0x8.
void TSelectMenu::perform(u32 flags, JDrama::TGraphics* gfx)
{
	{ static int v=-1; if(v<0){const char*e=getenv("SB_SEL_DBG");v=(e&&e[0]&&e[0]!='0')?1:0;}
	  if(v){static int n=0; if((n++%120)==0) std::fprintf(stderr,
	        "[sel] TSelectMenu::perform flags=0x%x state=%d screen=%p disabled=%d\n",
	        flags, mState, (void*)mScreen, mDisabled); } }

	if (flags & 0x1) {
		// TODO(file-select port): the DOL calc path (perform cases 1-9) runs the
		// window-open animation and the input/navigation state machine. It dereferences
		// the per-file panes that setup is not yet populating, so it is deferred; with
		// the screen drawn at its .blo defaults the menu is static for this milestone.
	}

	// Draw path (DOL: bit 0x8, state in [0,10)). ReInitializeGX + SMS_DrawInit, then draw
	// the screen through a J2DOrthoGraph built from the graphics viewport.
	if ((flags & 0x8) && mScreen != nullptr && mState >= 0 && mState < 10) {
		ReInitializeGX();
		SMS_DrawInit();
		J2DOrthoGraph graph(gfx->getViewport());
		graph.setup2D();
		graph.setup2D();
		mScreen->draw(0, 0, &graph);
	}
}
