#include <JSystem/J3D/J3DGraphBase/J3DTevs.hpp>
#include <JSystem/J3D/J3DGraphBase/Components/J3DLightObj.hpp>
#include <JSystem/J3D/J3DGraphBase/Components/J3DTexMtx.hpp>
#include <JSystem/J3D/J3DGraphBase/Components/J3DNBTScale.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/JRenderer.hpp>
#include <JSystem/ResTIMG.hpp>
#include <dolphin/gd.h>
#include <types.h>
#include <macros.h>

u8 GXTexMode0Ids[8]  = { 0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3 };
u8 GXTexMode1Ids[8]  = { 0x84, 0x85, 0x86, 0x87, 0xA4, 0xA5, 0xA6, 0xA7 };
u8 GXTexImage0Ids[8] = { 0x88, 0x89, 0x8A, 0x8B, 0xA8, 0xA9, 0xAA, 0xAB };
u8 GXTexImage1Ids[8] = { 0x8C, 0x8D, 0x8E, 0x8F, 0xAC, 0xAD, 0xAE, 0xAF };
u8 GXTexImage2Ids[8] = { 0x90, 0x91, 0x92, 0x93, 0xB0, 0xB1, 0xB2, 0xB3 };
u8 GXTexImage3Ids[8] = { 0x94, 0x95, 0x96, 0x97, 0xB4, 0xB5, 0xB6, 0xB7 };
u8 GXTexTlutIds[8]   = { 0x98, 0x99, 0x9A, 0x9B, 0xB8, 0xB9, 0xBA, 0xBB };

static u8 GX2HWFiltConv[6] = { 0x00, 0x04, 0x01, 0x05, 0x02, 0x06 };

const GXColor j3dDefaultColInfo = { 0xFF, 0xFF, 0xFF, 0xFF };
const GXColor j3dDefaultAmbInfo = { 0x32, 0x32, 0x32, 0x32 };
const u8 j3dDefaultColorChanNum = 0x01;
const J3DTevOrderInfo j3dDefaultTevOrderInfoNull
    = { GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL };
const J3DIndTexOrderInfo j3dDefaultIndTexOrderNull
    = { GX_TEXCOORD_NULL, GX_TEXMAP_NULL };
const GXColorS10 j3dDefaultTevColor = { 0xFF, 0xFF, 0xFF, 0xFF };
const J3DIndTexCoordScaleInfo j3dDefaultIndTexCoordScaleInfo = {};
const GXColor j3dDefaultTevKColor              = { 0xFF, 0xFF, 0xFF, 0xFF };
const J3DTevSwapModeInfo j3dDefaultTevSwapMode = {};
const J3DTevSwapModeTableInfo j3dDefaultTevSwapModeTable = { 0, 1, 2, 3 };
const J3DBlendInfo j3dDefaultBlendInfo
    = { GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP };
const J3DColorChanInfo j3dDefaultColorChanInfo
    = { 0, 0, 0, 2, 2, 0, 0xFF, 0xFF };
const u8 j3dDefaultTevSwapTableID = 0x1B;
const u16 j3dDefaultAlphaCmpID    = 0xE7;
const u16 j3dDefaultZModeID       = 0x17;

void J3DLoadArrayBasePtr(GXAttr attr, void* ptr)
{
#ifdef SMS_AURORA
	// Aurora rejects the raw CP_REG_ARRAYBASE_ID write (32-bit truncated host
	// pointer). Emit the Aurora-native GX_AURORA_LOAD_ARRAYBASE opcode via
	// its extended framing: [GX_AURORA(0x50)][u16 cmd=0x0010|a][u64 ptr]
	// [u32 size][u8 le]. Size is unknown here (this is a base-ptr-only
	// update; strides/counts set separately), so pass 0. Host assets are
	// pre-swapped (bmd_swap.cpp) so le=true.
	u32 a = attr == GX_VA_NBT ? 1 : attr - 9;
	GXCmd1u8(0x50);            // GX_AURORA
	GXCmd1u16((u16)(0x0010u | a));
	GXCmd1u64((u64)(uintptr_t)ptr);
	GXCmd1u32(0);              // size (0 = trust)
	GXCmd1u8(1);               // little-endian
#else
	u32 a = attr == GX_VA_NBT ? 1 : attr - 9;
	GXCmd1u8(8);
	GXCmd1u8(a + 0xA0);
	GXCmd1u32((u32)((uintptr_t)ptr & 0x7fffffff));
#endif
}

void J3DSetVtxAttrFmtv(GXVtxFmt vtxfmt, GXVtxAttrFmtList* list, bool param_3)
{
	u32 posCnt  = GX_POS_XYZ;
	u32 posType = GX_F32;
	u32 posFrac = 0;

	u32 nrmCnt  = GX_NRM_XYZ;
	u32 nrmType = GX_F32;
	u32 nrmIdx3 = 0;

	u32 c0Cnt  = GX_CLR_RGBA;
	u32 c0Type = GX_RGBA8;

	u32 c1Cnt  = GX_CLR_RGBA;
	u32 c1Type = GX_RGBA8;

	u32 tx0Cnt  = GX_TEX_ST;
	u32 tx0Type = GX_F32;
	u32 tx0Frac = 0;

	u32 tx1Cnt  = GX_TEX_ST;
	u32 tx1Type = GX_F32;
	u32 tx1Frac = 0;

	u32 tx2Cnt  = GX_TEX_ST;
	u32 tx2Type = GX_F32;
	u32 tx2Frac = 0;

	u32 tx3Cnt  = GX_TEX_ST;
	u32 tx3Type = GX_F32;
	u32 tx3Frac = 0;

	u32 tx4Cnt  = GX_TEX_ST;
	u32 tx4Type = GX_F32;
	u32 tx4Frac = 0;

	u32 tx5Cnt  = GX_TEX_ST;
	u32 tx5Type = GX_F32;
	u32 tx5Frac = 0;

	u32 tx6Cnt  = GX_TEX_ST;
	u32 tx6Type = GX_F32;
	u32 tx6Frac = 0;

	u32 tx7Cnt  = GX_TEX_ST;
	u32 tx7Type = GX_F32;
	u32 tx7Frac = 0;

	for (; list->attr != 0xff; ++list) {
		switch (list->attr) {
		case GX_VA_POS:
			posCnt  = list->cnt;
			posType = list->type;
			posFrac = list->frac;
			break;
		case GX_VA_NRM:
		case GX_VA_NBT:
			nrmType = list->type;
			if (list->cnt == 2) {
				nrmCnt  = GX_NRM_NBT;
				nrmIdx3 = 1;
			} else {
				if (param_3)
					nrmCnt = 1;
				else
					nrmCnt = list->cnt;
				nrmIdx3 = 0;
			}
			break;
		case GX_VA_CLR0:
			c0Cnt  = list->cnt;
			c0Type = list->type;
			break;
		case GX_VA_CLR1:
			c1Cnt  = list->cnt;
			c1Type = list->type;
			break;
		case GX_VA_TEX0:
			tx0Cnt  = list->cnt;
			tx0Type = list->type;
			tx0Frac = list->frac;
			break;
		case GX_VA_TEX1:
			tx1Cnt  = list->cnt;
			tx1Type = list->type;
			tx1Frac = list->frac;
			break;
		case GX_VA_TEX2:
			tx2Cnt  = list->cnt;
			tx2Type = list->type;
			tx2Frac = list->frac;
			break;
		case GX_VA_TEX3:
			tx3Cnt  = list->cnt;
			tx3Type = list->type;
			tx3Frac = list->frac;
			break;
		case GX_VA_TEX4:
			tx4Cnt  = list->cnt;
			tx4Type = list->type;
			tx4Frac = list->frac;
			break;
		case GX_VA_TEX5:
			tx5Cnt  = list->cnt;
			tx5Type = list->type;
			tx5Frac = list->frac;
			break;
		case GX_VA_TEX6:
			tx6Cnt  = list->cnt;
			tx6Type = list->type;
			tx6Frac = list->frac;
			break;
		case GX_VA_TEX7:
			tx7Cnt  = list->cnt;
			tx7Type = list->type;
			tx7Frac = list->frac;
		}
	}

	GDOverflowCheck(6);
	J3DGDWriteCPCmd(vtxfmt + CP_REG_VAT_GRP0_ID,
	                CP_REG_VAT_GRP0(posCnt, posType, posFrac, nrmCnt, nrmType,
	                                c0Cnt, c0Type, c1Cnt, c1Type, tx0Cnt,
	                                tx0Type, tx0Frac, 1, nrmIdx3));
	GDOverflowCheck(6);
	J3DGDWriteCPCmd(vtxfmt + CP_REG_VAT_GRP1_ID,
	                CP_REG_VAT_GRP1(tx1Cnt, tx1Type, tx1Frac, tx2Cnt, tx2Type,
	                                tx2Frac, tx3Cnt, tx3Type, tx3Frac, tx4Cnt,
	                                tx4Type, 1));
	GDOverflowCheck(6);
	J3DGDWriteCPCmd(vtxfmt + CP_REG_VAT_GRP2_ID,
	                CP_REG_VAT_GRP2(tx4Frac, tx5Cnt, tx5Type, tx5Frac, tx6Cnt,
	                                tx6Type, tx6Frac, tx7Cnt, tx7Type,
	                                tx7Frac));
}

const J3DLightInfo j3dDefaultLightInfo = {
	0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0xff, 0xff,
	0xff, 0xff, 1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
};

const J3DTexCoordInfo j3dDefaultTexCoordInfo[8] = {
	{ GX_MTX2x4, GX_TG_TEX0, GX_IDENTITY },
	{ GX_MTX2x4, GX_TG_TEX1, GX_IDENTITY },
	{ GX_MTX2x4, GX_TG_TEX2, GX_IDENTITY },
	{ GX_MTX2x4, GX_TG_TEX3, GX_IDENTITY },
	{ GX_MTX2x4, GX_TG_TEX4, GX_IDENTITY },
	{ GX_MTX2x4, GX_TG_TEX5, GX_IDENTITY },
	{ GX_MTX2x4, GX_TG_TEX6, GX_IDENTITY },
	{ GX_MTX2x4, GX_TG_TEX7, GX_IDENTITY },
};

const J3DTexMtxInfo j3dDefaultTexMtxInfo = {
	0x01, 0x00, 0xFF, 0xFF, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0,
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
};

const J3DIndTexMtxInfo j3dDefaultIndTexMtxInfo = {
	0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1,
};

const J3DTevStageInfo j3dDefaultTevStageInfo = {
	0x04, 0x0A, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x05, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
};

const J3DIndTevStageInfo j3dDefaultIndTevStageInfo = {};

const J3DFogInfo j3dDefaultFogInfo = {
	0, 0, 0x140, 0.0f, 0.0f, 0.0f, 0.0f, 0x00, 0x00, 0x00, 0x00,
};

const J3DNBTScaleInfo j3dDefaultNBTScaleInfo = { 0, 1.0f, 1.0f, 1.0f };

void J3DGDSetTexLookupMode(GXTexMapID id, GXTexWrapMode wrapS,
                           GXTexWrapMode wrapT, GXTexFilter minFilter,
                           GXTexFilter magFilter, f32 minLOD, f32 maxLOD,
                           f32 lodBias, u8 biasClamp, u8 edgeLOD,
                           GXAnisotropy maxAniso)
{
	// clang-format off
	u32 reg1 =
		(wrapS) << 0 |
		(wrapT) << 2 |
		(magFilter == GX_LINEAR) << 4 |
		(GX2HWFiltConv[minFilter]) << 5 |
		(!edgeLOD) << 8 |
		((u8)(lodBias * 32.0f)) << 9 |
		(maxAniso) << 19 |
		(biasClamp) << 21 |
		(GXTexMode0Ids[id] << 24);
	// clang-format on
	GDOverflowCheck(5);
	J3DGDWriteBPCmd(reg1);

	// clang-format off
	u32 reg2 =
		((u8)(minLOD * 16.0f)) << 0 |
		((u8)(maxLOD * 16.0f)) << 8 |
		(GXTexMode1Ids[id] << 24);
	// clang-format on
	GDOverflowCheck(5);
	J3DGDWriteBPCmd(reg2);
}

void loadCullMode(u8) { }

void J3DLightObj::load(u32 id) const
{
	GXLightID light = (GXLightID)(1 << id);
	GDSetLightPos(light, mLightPosition.x, mLightPosition.y, mLightPosition.z);
	GDSetLightAttn(light, mCosAtten.x, mCosAtten.y, mCosAtten.z, mDistAtten.x,
	               mDistAtten.y, mDistAtten.z);
	GDSetLightColor(light, mColor);
	GDSetLightDir(light, mLightDirection.x, mLightDirection.y,
	              mLightDirection.z);
}

void J3DTexMtx::calc()
{
	Mtx44 mtx2;
	Mtx44 mtx1;

	// clang-format off
	Mtx fixupMtx1 = {
    0.5,  0.0, 0.5, 0.0,
    0.0, -0.5, 0.5, 0.0,
    0.0,  0.0, 1.0, 0.0,
  };
	Mtx fixupMtx2 = {
    0.5,  0.0, 0.0, 0.5,
    0.0, -0.5, 0.0, 0.5,
    0.0,  0.0, 1.0, 0.0,
  };
	// clang-format on

	u32 useMayaFormat = (mInfo >> 7) & 1;
	u32 format        = mInfo & 0x7F;
	if (format == J3DTexMtxMode_Projmap || format == J3DTexMtxMode_ViewProjmap
	    || format == J3DTexMtxMode_EnvmapEffectMtx) {
		if (useMayaFormat == 0) {
			J3DGetTextureMtx(mSRT, mCenter, mtx1);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMaya(mSRT, mtx1);
		}
		MTXConcat(mtx1, fixupMtx1, mtx1);
		J3DMtxProjConcat(mtx1, mEffectMtx, mtx2);
		MTXConcat(mtx2, mViewMtx, mTotalMtx);
	} else if (format == J3DTexMtxMode_Envmap) {
		if (useMayaFormat == 0) {
			J3DGetTextureMtx(mSRT, mCenter, mtx2);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMaya(mSRT, mtx2);
		}

		MTXConcat(mtx2, fixupMtx1, mtx2);
		MTXConcat(mtx2, mViewMtx, mTotalMtx);
	} else if (format == J3DTexMtxMode_EnvmapOldEffectMtx) {
		if (useMayaFormat == 0) {
			J3DGetTextureMtxOld(mSRT, mCenter, mtx1);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMayaOld(mSRT, mtx1);
		}
		MTXConcat(mtx1, fixupMtx2, mtx1);
		J3DMtxProjConcat(mtx1, mEffectMtx, mtx2);
		MTXConcat(mtx2, mViewMtx, mTotalMtx);
	} else if (format == J3DTexMtxMode_EnvmapOld) {
		if (useMayaFormat == 0) {
			J3DGetTextureMtxOld(mSRT, mCenter, mtx2);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMayaOld(mSRT, mtx2);
		}
		MTXConcat(mtx2, fixupMtx2, mtx2);
		MTXConcat(mtx2, mViewMtx, mTotalMtx);
	} else if (format == J3DTexMtxMode_EnvmapBasic) {
		if (useMayaFormat == 0) {
			J3DGetTextureMtxOld(mSRT, mCenter, mtx2);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMayaOld(mSRT, mtx2);
		}
		MTXConcat(mtx2, mViewMtx, mTotalMtx);
	} else if (format == J3DTexMtxMode_ProjmapBasic
	           || format == J3DTexMtxMode_ViewProjmapBasic
	           || format == J3DTexMtxMode_Unknown5) {
		if (useMayaFormat == 0) {
			J3DGetTextureMtxOld(mSRT, mCenter, mtx1);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMayaOld(mSRT, mtx1);
		}
		J3DMtxProjConcat(mtx1, mEffectMtx, mtx2);
		MTXConcat(mtx2, mViewMtx, mTotalMtx);
	} else if (format == J3DTexMtxMode_Unknown4) {
		if (useMayaFormat == 0) {
			J3DGetTextureMtxOld(mSRT, mCenter, mtx1);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMayaOld(mSRT, mtx1);
		}
		J3DMtxProjConcat(mtx1, mEffectMtx, mTotalMtx);
	} else {
		if (useMayaFormat == 0) {
			J3DGetTextureMtxOld(mSRT, mCenter, mTotalMtx);
		} else if (useMayaFormat == 1) {
			J3DGetTextureMtxMayaOld(mSRT, mTotalMtx);
		}
	}
}

void J3DTexMtx::load(u32 id) const
{
	J3DGDLoadTexMtxImm((MtxPtr)&mTotalMtx, id * 3 + 0x1e,
	                   (GXTexMtxType)mProjection);
}

#ifdef SMS_AURORA
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" const char* sb_gx_last_marker();
extern "C" const char* sb_cpu_drawbuf_name();
// ResTIMG::imageDataOffset is a 32-bit field holding the offset from the
// ResTIMG header to its image data. For most TIMGs it is a small POSITIVE
// offset. But shared/mirror textures rebase it as a POINTER DELTA
// ((uintptr_t)&src + src.imageDataOffset - (uintptr_t)&dst, MapMirror.cpp:321)
// which is NEGATIVE when src precedes dst. Stored in a u32 and added to a
// 64-bit host pointer without sign extension, a negative delta becomes ~+4GB
// and reads out of bounds (crashes aurora convert_texture). Sign-extend the
// 32-bit delta so the add is the intended displacement. Same LP64 class as
// [[fileselect-cap-texture-setrestimg-lp64]]. (No-op for normal positive
// offsets: bit 31 clear.)
static inline intptr_t sb_timg_image_offset(const ResTIMG* img)
{
	return (intptr_t)(s32)img->imageDataOffset;
}
// GD-context emitter of the Aurora extended texture-object command. The GC
// BP writes below (image attr/ptr/lookup mode) carry wrap/filter state that
// Aurora parses, but the TEXTURE DATA binding needs the host pointer + dims
// + format via GX_AURORA_LOAD_TEXOBJ (34-byte payload, big-endian, matching
// aurora's handle_aurora). texObjId=0 selects Aurora's uncached texture path.
static void sb_gd_load_texobj_aurora(u32 map, const ResTIMG* img)
{
	if (img->isIndexTexture) {
		// CI textures also need GX_AURORA_LOAD_TLUT plumbing (J3DGDLoadTlut's
		// raw BP 0x64/0x65 stream is not parsed by Aurora) — separate arc.
		static int warned = 0;
		if (!warned) {
			warned = 1;
			fprintf(stderr, "[sb] loadTexNo: CI texture bound without Aurora TLUT metadata (fmt=%d)\n",
			        img->format);
		}
	}
	// imageDataOffset is a 32-bit field. For most TIMGs it is a small positive
	// offset from the ResTIMG header to its image data. But MapMirror rebases
	// it as a POINTER DELTA ((uintptr_t)&src + off - (uintptr_t)&dst) truncated
	// to u32 (MapMirror.cpp:321) — negative on LP64 when src precedes dst, so
	// the stored u32 is ~4GB and `img + (u32)off` lands 4GB out of bounds
	// (crashes convert_texture). Sign-extend the 32-bit delta so the add is the
	// intended negative displacement. Same LP64 class as
	// [[fileselect-cap-texture-setrestimg-lp64]].
	const void* data = (const u8*)img + sb_timg_image_offset(img);
	if (getenv("SB_TEXOBJ_DBG")) {
		static long n = 0;
		bool hi = (img->imageDataOffset & 0x80000000u) != 0;
		if (hi || ++n <= 60)
			fprintf(stderr, "[texobj] map=%u img=%p off=%08x data=%p %ux%u fmt=%u mips=%u ci=%u%s\n", map,
			        (void*)img, img->imageDataOffset, data, img->width, img->height, img->format & 0xf,
			        img->mipmapCount, img->isIndexTexture, hi ? " <BIT31>" : "");
	}
	GDOverflowCheck(37);
	J3DGDWrite_u8(0x50);                     // GX_AURORA opcode
	J3DGDWrite_u16(0x0030);                  // GX_AURORA_LOAD_TEXOBJ
	J3DGDWrite_u8((u8)map);
	J3DGDWrite_u32((u32)((u64)(uintptr_t)data >> 32));  // ptr hi (BE u64)
	J3DGDWrite_u32((u32)(uintptr_t)data);               // ptr lo
	J3DGDWrite_u32(img->width);
	J3DGDWrite_u32(img->height);
	J3DGDWrite_u32(img->format & 0xf);
	J3DGDWrite_u32(img->isIndexTexture ? map : 0);      // GXTlut slot
	// Mip level count, straight from the TIMG header (mipmapCount==1 means
	// "base level only", matching aurora's GXTexObj_::mip_count() convention
	// exactly -- see JUTTexture.cpp/MapMirror.cpp, which set this field to 1
	// for their synthetic no-mip textures). Previously hardcoded to 0 ("no
	// mips") for every GD-context texture bind regardless of the source
	// asset's real mip chain: aurora derives mip_count() from
	// has_mips()/max_lod() (texobj mode0/mode1), and this GD-context emit
	// wrote neither, so every material texture bound this way -- including
	// small, heavily-tiled ones like the sky's 8x8 I4 cloud texture -- was
	// uploaded with exactly 1 level. Minifying a repeating pattern without
	// mip filtering aliases into a fine regular moire/crosshatch; the tiny
	// cloud texture (tiled many times across the sky dome) is the
	// worst-case instance of this general gap. The BMD's mip data (when
	// mipmapCount>1) is stored contiguously after the base level in the
	// same tiled format, which is exactly what
	// GX_AURORA_LOAD_TEXOBJ's raw pointer + width/height/format triggers
	// aurora's DecodeTiled<>() to walk (see command_processor.cpp).
	J3DGDWrite_u8(img->mipmapCount);                    // mip level count
	// texObjId: a STABLE content hash of the texture identity (data ptr + dims
	// + format + mips). Was hardcoded 0 ("uncached"), which made aurora's
	// static-texture cache (keyed by texObjId; skipped when 0) MISS on every
	// bind and re-convert+re-upload the SAME texture once per draw — measured
	// as ~90% of frame time (resolve_sampled_textures ~150us/draw). A stable id
	// lets re-binds of the same texture hit the cache, so conversions drop to
	// ~zero after warmup. In-place content changes at a fixed address are rare
	// for J3D static textures (animation swaps the bound img pointer, changing
	// the hash); EFB-copy/dynamic textures bypass this cache (copyTextures
	// path), so this is safe for them. FNV-1a over the identity fields.
	{
		u64 h = 1469598103934665603ull;
		auto mix = [&h](u64 v) {
			for (int i = 0; i < 8; ++i) { h ^= (v >> (i * 8)) & 0xff; h *= 1099511628211ull; }
		};
		mix((u64)(uintptr_t)data);
		mix(((u64)img->width << 32) | ((u64)img->height << 16) | (u64)(img->format & 0xf));
		mix((u64)img->mipmapCount);
		u32 id = (u32)(h ^ (h >> 32));
		if (id == 0) id = 1;                            // 0 is the "no id" sentinel
		J3DGDWrite_u32(id);                             // texObjId (content hash)
	}
	J3DGDWrite_u32(1);                                  // texDataVersion
}
#endif

void loadTexNo(u32 param_1, const u16& param_2)
{
	ResTIMG* img = &j3dSys.getTexture()->mResources[param_2];
	J3DSys::sTexCoordScaleTable[param_1].field_0x00 = img->width;
	J3DSys::sTexCoordScaleTable[param_1].field_0x02 = img->height;
#ifdef SMS_AURORA
	// SB_CLOUD_DBG: root-cause tool for the sky/cloud crosshatch. Ground truth
	// (scratch/oracle/cloud_ground_truth.txt) says GC binds the 8x8 I4 cloud
	// puff texture with wrapS=wrapT=Clamp(0). Isolate by texture dims so this
	// doesn't spam every material bind.
	if (getenv("SB_CLOUD_DBG") && img->width == 8 && img->height == 8) {
		static long n = 0;
		if (++n <= 60)
			fprintf(stderr,
			    "[cloud-dbg] map=%u dims=%ux%u fmt=%u wrapS=%u wrapT=%u mipCount=%u minFilt=%u magFilt=%u mark='%s'\n",
			    param_1, img->width, img->height, img->format & 0xf, img->wrapS, img->wrapT,
			    img->mipmapCount, img->minFilter, img->magFilter, sb_gx_last_marker());
	}
	// TEMP crosshatch-hunt: dump the exact bytes + owning J3DTexture identity for
	// the two phantom texobjs (16x16 fmt0, 64x64 fmt1) so we can tell whether the
	// header content is corrupt or texNo is indexing into the wrong table.
	{
		extern int g_sbXhSkyInitDL;
		if (g_sbXhSkyInitDL) {
			static long n = 0;
			if (++n <= 40) {
				const u8* raw = (const u8*)img;
				fprintf(stderr,
				    "[xh-skyinit] texNo=%u map=%u tex=%p num=%u img=%p hex=",
				    param_2, param_1, (void*)j3dSys.getTexture(),
				    j3dSys.getTexture() ? j3dSys.getTexture()->getNum() : 0, (void*)img);
				for (int i = 0; i < 0x20; i++) fprintf(stderr, "%02x", raw[i]);
				fprintf(stderr, "\n");
			}
		}
	}
	if (getenv("SB_XH_MIRROR_DBG") && img->wrapS == 2 && img->wrapT == 2) {
		static long n = 0;
		if (++n <= 60) {
			const u8* raw = (const u8*)img;
			fprintf(stderr,
			    "[xh-mirror] texNo=%u map=%u tex=%p num=%u img=%p caller=%p hex=",
			    param_2, param_1, (void*)j3dSys.getTexture(),
			    j3dSys.getTexture() ? j3dSys.getTexture()->getNum() : 0, (void*)img,
			    __builtin_return_address(1));
			for (int i = 0; i < 0x20; i++) fprintf(stderr, "%02x", raw[i]);
			fprintf(stderr, "\n");
		}
	}
	if (getenv("SB_XH_DBG")) {
		const char* mk = sb_cpu_drawbuf_name();
		if (getenv("SB_XH_ALL") && mk && strstr(mk, "Sky") != nullptr) {
			static long an = 0;
			if (++an <= 400)
				fprintf(stderr, "[xh-all] map=%u dims=%ux%u fmt=%u mk='%s'\n", param_1,
				    img->width, img->height, img->format & 0xf, mk ? mk : "(null)");
		}
		bool hit = mk && strstr(mk, "Sky Xlu") != nullptr;
		if (hit) {
			static long n = 0;
			if (++n <= 200) {
				const u8* raw = (const u8*)img;
				fprintf(stderr,
				    "[xh-dbg] texNo=%u map=%u tex=%p num=%u img=%p mark='%s' hex=",
				    param_2, param_1, (void*)j3dSys.getTexture(),
				    j3dSys.getTexture() ? j3dSys.getTexture()->getNum() : 0,
				    (void*)img, sb_gx_last_marker());
				for (int i = 0; i < 0x20; i++) fprintf(stderr, "%02x", raw[i]);
				fprintf(stderr, "\n");
			}
		}
	}
#endif
	J3DGDSetTexImgAttr((GXTexMapID)param_1, img->width, img->height,
	                   (GXTexFmt)(img->format & 0xf));
#ifdef SMS_AURORA
	// Same signed-delta reconstruction as the LOAD_TEXOBJ emit (this BP write
	// is inert under Aurora, but keep the pointer arithmetic consistent).
	J3DGDSetTexImgPtr((GXTexMapID)param_1, (u8*)img + sb_timg_image_offset(img));
#else
	J3DGDSetTexImgPtr((GXTexMapID)param_1, (u8*)img + img->imageDataOffset);
#endif

	J3DGDSetTexLookupMode(
	    (GXTexMapID)param_1, (GXTexWrapMode)img->wrapS,
	    (GXTexWrapMode)img->wrapT, (GXTexFilter)img->minFilter,
	    (GXTexFilter)img->magFilter, img->minLod * 0.125f, img->maxLod * 0.125f,
	    img->lodBias * 0.01f, img->biasClamp, img->doEdgeLod,
	    (GXAnisotropy)img->maxAnisotropy);

	if (img->isIndexTexture == true) {
		GXTlutSize tlutSize = img->numColors > 16 ? GX_TLUT_256 : GX_TLUT_16;

		J3DGDLoadTlut((u8*)img + img->paletteOffset, (param_1 << 15) + 0xC0000,
		              tlutSize);
		J3DGDSetTexTlut((GXTexMapID)param_1, (param_1 << 15) + 0xC0000,
		                (GXTlutFmt)img->colorFormat);
	}
#ifdef SMS_AURORA
	sb_gd_load_texobj_aurora(param_1, img);
#endif
}

void loadDither(u8) { }

void loadNBTScale(J3DNBTScale& param_0)
{
	if (param_0.mbHasScale == true) {
		j3dSys.setNBTScale(&param_0.mScale);
	} else {
		j3dSys.setNBTScale(nullptr);
	}
}

u8 j3dTexCoordTable[7624];
u8 j3dTevSwapTableTable[1024];
u8 j3dAlphaCmpTable[768];
u8 j3dZModeTable[96];

void makeTexCoordTable()
{
	u8 texMtx[] = {
		GX_TEXMTX0, GX_TEXMTX1, GX_TEXMTX2, GX_TEXMTX3, GX_TEXMTX4,  GX_TEXMTX5,
		GX_TEXMTX6, GX_TEXMTX7, GX_TEXMTX8, GX_TEXMTX9, GX_IDENTITY,
	};

	u8* table = j3dTexCoordTable;
	for (u32 i = 0; i < 11; ++i) {
		for (u32 j = 0; j < 21; ++j) {
			for (int k = 0; k < ARRAY_COUNT(texMtx); ++k) {
				u32 idx = j * 11 + i * 0xe7 + k;

				table[idx * 3 + 0] = i;
				table[idx * 3 + 1] = j;
				table[idx * 3 + 2] = texMtx[k];
			}
		}
	}
}

void makeAlphaCmpTable()
{
	u8* table = j3dAlphaCmpTable;
	for (u32 i = 0; i != 8; ++i) {
		for (int j = 0; j != 4; ++j) {
			for (u32 k = 0; k != 8; ++k) {
				u32 idx = i * 32 + j * 8 + k;

				table[idx * 3 + 0] = i;
				table[idx * 3 + 1] = j;
				table[idx * 3 + 2] = k;
			}
		}
	}
}

void makeZModeTable()
{
	u8* table = j3dZModeTable;
	for (int i = 0; i < 2; ++i) {
		for (u32 j = 0; j < 8; ++j) {
			for (int k = 0; k < 2; ++k) {
				u32 idx = j * 2 + i * 16 + k;

				table[idx * 3 + 0] = i;
				table[idx * 3 + 1] = j;
				table[idx * 3 + 2] = k;
			}
		}
	}
}

void makeTevSwapTable()
{
	u8* table = j3dTevSwapTableTable;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			for (int k = 0; k < 4; ++k) {
				for (int l = 0; l < 4; ++l) {
					u32 idx = i * 64 + j * 16 + k * 4 + l;

					table[idx * 4 + 0] = i;
					table[idx * 4 + 1] = j;
					table[idx * 4 + 2] = k;
					table[idx * 4 + 3] = l;
				}
			}
		}
	}
}
