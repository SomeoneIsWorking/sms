// SelectMenu.cpp — file-select screen view-objects.
//
// This translation unit is UN-decompiled in the community decomp (the original was empty).
// Reconstructed faithfully from the GMSE01 DOL. So far: TSelectGrad (the animated background
// gradient). TSelectMenu (the file windows) + TSelectShineManager are reconstructed
// incrementally as the file-select port is fleshed out.
//
// TSelectGrad anchors (DOL): ctor 0x80175a4c, setStageColor 0x8017591c, perform 0x80175560.

#include <GC2D/SelectGrad.hpp>
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
