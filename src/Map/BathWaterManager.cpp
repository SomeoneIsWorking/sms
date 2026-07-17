#include <Map/BathWaterManager.hpp>

#include <dolphin/mtx.h>
#include <dolphin/gx.h>

static void draw_mist(u16 x, u16 y, u16 wd, u16 ht, void* buffer)
{
	Mtx e_m;
	Mtx44 m;
	GXTexObj tex_obj;

	GXColor tev_color = { 0x03, 0x03, 0x03, 0x00 };
	u8 vFilter[7]     = { 0x15, 0x00, 0x00, 0x16, 0x00, 0x00, 0x15 };

	f32 f_left   = x;
	f32 f_wd     = wd;
	f32 f_top    = y;
	f32 f_ht     = ht;
	f32 f_right  = f_left + f_wd;
	f32 f_bottom = f_top + f_ht;
	f32 offset_x = (4.0f / f_wd);
	f32 offset_y = (2.0f / f_ht);

	C_MTXOrtho(m, f_top, f_bottom, f_left, f_right, 0.0f, 1.0f);
	PSMTXIdentity(e_m);
	GXSetTexCopySrc(x, y, wd, ht);
	GXSetCopyFilter(GX_FALSE, 0, GX_TRUE, vFilter);
	GXSetTexCopyDst(wd >> 1, ht >> 1, GX_TF_RGB565, GX_TRUE);
	GXCopyTex(buffer, GX_FALSE);
	GXPixModeSync();
	GXInitTexObj(&tex_obj, buffer, wd >> 1, ht >> 1, GX_TF_RGB565, GX_CLAMP,
	             GX_CLAMP, 0);
	GXInitTexObjLOD(&tex_obj, GX_LINEAR, GX_LINEAR, 0.0, 0.0, 0.0, GX_FALSE,
	                GX_FALSE, GX_ANISO_1);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX1, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX2, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX3, GX_DIRECT);

	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX3, GX_TEX_ST, GX_F32, 0);

	GXSetNumChans(0);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);

	GXSetNumTexGens(4);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, GX_FALSE,
	                  0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, 0x3c, GX_FALSE,
	                  0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX2, 0x3c, GX_FALSE,
	                  0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX3, 0x3c, GX_FALSE,
	                  0x7d);

	GXLoadTexObj(&tex_obj, GX_TEXMAP0);

	GXSetNumTevStages(4);

	// Stage 0
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF, GX_CC_C0);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_FALSE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE0);

	// Stage 1
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF,
	                GX_CC_CPREV);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_FALSE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE1);

	// Stage 2
	GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD2, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF,
	                GX_CC_CPREV);
	GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_FALSE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE2);

	// Stage 3
	GXSetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD3, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE3, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF,
	                GX_CC_CPREV);
	GXSetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_DIVIDE_2,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE3);

	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);

	GXSetTevColor(GX_TEVREG0, tev_color);
	GXSetProjection(m, GX_ORTHOGRAPHIC);
	GXSetViewport(f_left, f_top, f_wd, f_ht, 0.0, 1.0);
	GXSetScissor(x, y, wd, ht);

	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GXSetAlphaUpdate(GX_FALSE);
	GXSetColorUpdate(GX_TRUE);
	GXLoadPosMtxImm(e_m, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetCullMode(GX_CULL_NONE);
	GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_ZERO, GX_BL_ZERO, GX_LO_NOOP);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2u16(x, y);
	GXTexCoord2f32(-offset_x, 0.0f);
	GXTexCoord2f32(offset_x, 0.0f);
	GXTexCoord2f32(0.0f, -offset_y);
	GXTexCoord2f32(0.0f, offset_y);
	GXPosition2u16(x + wd, y);
	GXTexCoord2f32(1.0f - offset_x, 0.0f);
	GXTexCoord2f32(1.0f + offset_x, 0.0f);
	GXTexCoord2f32(1.0f, -offset_y);
	GXTexCoord2f32(1.0f, offset_y);
	GXPosition2u16(x + wd, y + ht);
	GXTexCoord2f32(1.0f - offset_x, 1.0f);
	GXTexCoord2f32(1.0f + offset_x, 1.0f);
	GXTexCoord2f32(1.0f, 1.0f - offset_y);
	GXTexCoord2f32(1.0f, 1.0f + offset_y);
	GXPosition2u16(x, y + ht);
	GXTexCoord2f32(-offset_x, 1.0f);
	GXTexCoord2f32(+offset_x, 1.0f);
	GXTexCoord2f32(0.0f, 1.0f - offset_y);
	GXTexCoord2f32(0.0f, 1.0f + offset_y);
	GXEnd();

	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2u16(x, y);
	GXTexCoord2f32(-offset_x, 0.0f);
	GXTexCoord2f32(offset_x, 0.0f);
	GXTexCoord2f32(0.0f, -offset_y);
	GXTexCoord2f32(0.0f, offset_y);
	GXPosition2u16(x + wd, y);
	GXTexCoord2f32(1.0f - offset_x, 0.0f);
	GXTexCoord2f32(1.0f + offset_x, 0.0f);
	GXTexCoord2f32(1.0f, -offset_y);
	GXTexCoord2f32(1.0f, offset_y);
	GXPosition2u16(x + wd, y + ht);
	GXTexCoord2f32(1.0f - offset_x, 1.0f);
	GXTexCoord2f32(1.0f + offset_x, 1.0f);
	GXTexCoord2f32(1.0f, 1.0f - offset_y);
	GXTexCoord2f32(1.0f, 1.0f + offset_y);
	GXPosition2u16(x, y + ht);
	GXTexCoord2f32(-offset_x, 1.0f);
	GXTexCoord2f32(+offset_x, 1.0f);
	GXTexCoord2f32(0.0f, 1.0f - offset_y);
	GXTexCoord2f32(0.0f, 1.0f + offset_y);
	GXEnd();
}

// Native port of TBathWaterPreprocessor::perform (@0x801aa5a8). Thin dispatch: on the DRAW
// flag bit, forward to the manager's renderer->render(graphics, bathtubData, waters, params, 2).
// RE: scratch/decomp_bathwater/801aa5a8.c.
//
// vtable slot resolution: TBathWaterRenderer's virtuals in declaration order are prerender[0]
// / render[1] / getHeight[2]. CodeWarrior emits the dtor as vtable[0] (byte 0), so the
// virtuals sit at byte offsets 4/8/12 respectively. The RE's `*(*(int**)(mgr+0x30) + 8)`
// dereferences vtable[8/4] = the 3rd slot with a dtor prefix = render. Args
// (graphics, bathtubData, TBathWater** waters, TBathWaterParams** params, count=2) match
// TBathWaterRenderer::render's signature exactly, confirming the pick.
//
// The dispatch gate is the pure sb::bath_water_preprocessor_should_dispatch predicate
// (native/render/sms_boot_bath_water.h), unit-tested in bath_water_preprocessor_test.cpp.
// The shipping port routes through it so the test validates the real gate.
#include "sms_boot_bath_water.h"

void TBathWaterPreprocessor::perform(u32 flags, JDrama::TGraphics* graphics)
{
	// mgr->unk24 is declared u32 in the header but the RE treats it as a POINTER to a
	// TBathtubData-adjacent structure (the RE's `mgr->unk24 + 0x170` yields the
	// TBathtubData& passed to render). We match that here — a subtle typing mismatch that
	// only matters at this seam; unk24 is written by other parts of the manager as a raw
	// pointer address. If the header is ever retyped, remove this cast.
	TBathWaterManager* mgr = unk10;
	const bool bathtub_present  = mgr && mgr->unk24 != 0;
	const bool renderer_present = mgr && mgr->unk30 != nullptr;
	if (!sb::bath_water_preprocessor_should_dispatch(flags, mgr != nullptr,
	                                                 bathtub_present, renderer_present)) {
		return;
	}
	const TBathtubData* bathtub = reinterpret_cast<const TBathtubData*>((uintptr_t)mgr->unk24 + 0x170);
	mgr->unk30->render(graphics, *bathtub, mgr->unk20, mgr->unk14, 2);
}
