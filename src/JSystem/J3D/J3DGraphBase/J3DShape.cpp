#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTevs.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/JRenderer.hpp>
#include <dolphin/os.h>
#include <dolphin/gx.h>
#include <dolphin/gd.h>

J3DShapeMtx::LoadPipeline J3DShapeMtx::mtxLoadPipeline[4] = {
	&J3DShapeMtx::loadMtxIndx_PNGP,
	&J3DShapeMtx::loadMtxIndx_PCPU,
	&J3DShapeMtx::loadMtxIndx_NCPU,
	&J3DShapeMtx::loadMtxIndx_PNCPU,
};
u32 J3DShapeMtx::currentPipeline;

void J3DShapeMtx::loadMtxIndx_PNGP(int id, u16 mtx_index) const
{
	j3dSys.loadPosMtxIndx(id, mtx_index);
	j3dSys.loadNrmMtxIndx(id, mtx_index);
}

void J3DShapeMtx::loadMtxIndx_PCPU(int id, u16 mtx_index) const
{
	GXLoadPosMtxImm(j3dSys.mViewMtx, id * 3);
	j3dSys.loadNrmMtxIndx(id, mtx_index);
}

void J3DShapeMtx::loadMtxIndx_NCPU(int id, u16 mtx_index) const
{
	j3dSys.loadPosMtxIndx(id, mtx_index);
	GXLoadNrmMtxImm(j3dSys.mViewMtx, id * 3);
}

void J3DShapeMtx::loadMtxIndx_PNCPU(int id, u16) const
{
	GXLoadPosMtxImm(j3dSys.mViewMtx, id * 3);
	GXLoadNrmMtxImm(j3dSys.mViewMtx, id * 3);
}

void J3DShapeMtx::load() const
{
	LoadPipeline ld = mtxLoadPipeline[currentPipeline];
	(this->*ld)(0, unk4);
}

void J3DShapeMtx::calcNBTScale(const Vec& param_1, float (*param_2)[3][3],
                               float (*param_3)[3][3])
{
	J3DPSMtx33Copy(param_2[unk4], param_3[unk4]);
	J3DScaleNrmMtx33(param_3[unk4], param_1);
}

// ~J3DShapeMtx is included in the binary and must go *here*
static void dummy(J3DShapeMtx* mtx) { mtx->~J3DShapeMtx(); }

void J3DShapeMtxDL::load() const { GXCallDisplayList(mDisplayList, 0x20); }

void J3DShapeMtxMulti::load() const
{
	LoadPipeline ld = mtxLoadPipeline[currentPipeline];
	for (int i = 0; i < unk8; ++i) {
		if (unkC[i] == 0xffff)
			continue;
		(this->*ld)(i, unkC[i]);
	}
}

void J3DShapeMtxMulti::calcNBTScale(const Vec& param_1, float (*param_2)[3][3],
                                    float (*param_3)[3][3])
{
	for (int i = 0; i < unk8; ++i) {
		if (unkC[i] == 0xffff)
			continue;
		J3DPSMtx33Copy(param_2[unkC[i]], param_3[unkC[i]]);
		J3DScaleNrmMtx33(param_3[unkC[i]], param_1);
	}
}

J3DShapeDraw::J3DShapeDraw(const u8* list, u32 size)
{
	mDisplayList     = list;
	mDisplayListSize = size;
}

void J3DShapeDraw::draw() const
{
#ifdef SMS_NATIVE_PLATFORM
	// SB_SHAPEDRAW_DBG=1: per-call trace of the geometry display lists
	// entering the fifo (ptr/size + first opcode bytes) — pairs with aurora's
	// [draw-stats] to tell "DL never reaches fifo" from "fifo drops it".
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_SHAPEDRAW_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dbg) {
			static long n = 0;
			++n;
			if ((n % 500) == 0 || n <= 30) {
				const u8* d = (const u8*)mDisplayList;
				fprintf(stderr, "[shapedraw] n=%ld dl=%p size=%u first=[%02x %02x %02x %02x %02x %02x %02x %02x]\n",
				        n, (void*)mDisplayList, mDisplayListSize,
				        d ? d[0] : 0, d ? d[1] : 0, d ? d[2] : 0, d ? d[3] : 0,
				        d ? d[4] : 0, d ? d[5] : 0, d ? d[6] : 0, d ? d[7] : 0);
			}
		}
	}
#endif
	GXCallDisplayList((void*)mDisplayList, mDisplayListSize);
}

void J3DShape::initialize()
{
	unk0           = 0;
	mIndex         = 0xffff;
	mElementCount  = 0;
	unk8           = 0;
	unkC           = 0;
	unk10.x        = 0.0f;
	unk10.y        = 0.0f;
	unk10.z        = 0.0f;
	unk1C.x        = 0.0f;
	unk1C.y        = 0.0f;
	unk1C.z        = 0.0f;
	mVtxDescList          = nullptr;
	mMatrices      = nullptr;
	mDraws         = nullptr;
	mVertexData    = 0;
	mDrawMtxData   = 0;
	mScaleFlagArray          = 0;
	mDrawMatrices  = nullptr;
	mNormMatrices  = nullptr;
	mCurrentViewNo = &j3dDefaultViewNo;
	unk30          = 0;
}

void J3DShape::calcNBTScale(const Vec& param_1, float (*param_2)[3][3],
                            float (*param_3)[3][3])
{
	for (u16 i = 0; i < mElementCount; ++i)
		mMatrices[i]->calcNBTScale(param_1, param_2, param_3);
}

int J3DShape::countBumpMtxNum() const
{
	int result = 0;
	for (u16 i = 0; i < mElementCount; ++i)
		result += mMatrices[i]->getUseMtxNum();
	return result;
}

void J3DShape::makeVtxArrayCmd()
{
	const GXVtxAttrFmtList* vtxAttr = mVertexData->getVtxAttrFmtList();
	u8 stride[12];
	void* array[12];

	for (int i = 0; i < 12; i++) {
		stride[i] = 0;
		array[i]  = 0;
	}

	for (; vtxAttr->attr != GX_VA_NULL; vtxAttr++) {
		switch (vtxAttr->attr) {
		case GX_VA_POS: {
			if (vtxAttr->type == GX_F32)
				stride[vtxAttr->attr - GX_VA_POS] = 12;
			else
				stride[vtxAttr->attr - GX_VA_POS] = 6;
			array[vtxAttr->attr - GX_VA_POS] = mVertexData->getVtxPosArray();
		} break;
		case GX_VA_NRM: {
			if (vtxAttr->type == GX_F32)
				stride[vtxAttr->attr - GX_VA_POS] = 12;
			else
				stride[vtxAttr->attr - GX_VA_POS] = 6;
			array[vtxAttr->attr - GX_VA_POS] = mVertexData->getVtxNormArray();
		} break;
		case GX_VA_CLR0:
		case GX_VA_CLR1: {
			stride[vtxAttr->attr - GX_VA_POS] = 4;
			array[vtxAttr->attr - GX_VA_POS]
			    = mVertexData->getVtxColorArray(vtxAttr->attr - GX_VA_CLR0);
		} break;
		case GX_VA_TEX0:
		case GX_VA_TEX1:
		case GX_VA_TEX2:
		case GX_VA_TEX3:
		case GX_VA_TEX4:
		case GX_VA_TEX5:
		case GX_VA_TEX6:
		case GX_VA_TEX7: {
			if (vtxAttr->type == GX_F32)
				stride[vtxAttr->attr - GX_VA_POS] = 8;
			else
				stride[vtxAttr->attr - GX_VA_POS] = 4;
			array[vtxAttr->attr - GX_VA_POS]
			    = mVertexData->getVtxTexCoordArray(vtxAttr->attr - GX_VA_TEX0);
		} break;
		default:
			break;
		}
	}
	for (GXVtxDescList* piVar5 = mVtxDescList; piVar5->attr != GX_VA_NULL; ++piVar5) {
		if ((piVar5->attr == GX_VA_NBT) && (piVar5->type != GX_NONE)) {
			unk30 = 1;
			stride[1] *= 3;
			array[1] = mVertexData->getVtxNBTArray();
		}
	}

	for (s32 i = 0; i < 0x0C; i++) {
		if (array[i] != 0)
			GDSetArray((GXAttr)(i + GX_VA_POS), array[i], stride[i]);
		else
			GDSetArrayRaw((GXAttr)(i + GX_VA_POS), nullptr, stride[i]);
	}
}

void J3DShape::makeVcdVatCmd()
{
	GDLObj list;

	GDInitGDLObj(&list, mGDCommands, 0xC0);
	__GDCurrentDL = &list;
	GDSetVtxDescv(mVtxDescList);
	makeVtxArrayCmd();
	J3DSetVtxAttrFmtv(GX_VTXFMT0, mVertexData->getVtxAttrFmtList(), unk30);
	GDPadCurr32();
	GDFlushCurrToMem();
	__GDCurrentDL = nullptr;
}

void J3DShape::loadVtxArray() const
{
	J3DLoadArrayBasePtr(GX_VA_POS, j3dSys.unk10C);
	if (unk30 == 0) {
		void* l = j3dSys.unk110;
		J3DLoadArrayBasePtr(GX_VA_NRM, l);
	}
	J3DLoadArrayBasePtr(GX_VA_CLR0, j3dSys.unk114);
}

#ifdef SMS_NATIVE_PLATFORM
// PC-native renderer (single owned path): capture this active shape's geometry into the native
// renderer's frame buffer (native/render/sms_boot_j3d_capture.cpp drains it at present). Driven by
// scene_drive.cpp's TSmJ3DScn::perform(8). WEAK so builds that link this TU but NOT the capture
// body (e.g. the j3dmesh_test/loader tests) resolve it to null and skip the hook.
extern "C" bool sb_boot_capture_j3d(J3DShape* shape) __attribute__((weak));
#endif

void J3DShape::draw() const
{
#ifdef SMS_NATIVE_PLATFORM
	if (&sb_boot_capture_j3d && sb_boot_capture_j3d(const_cast<J3DShape*>(this))) {
		// Captured natively; the GX issue below is a no-op on this platform anyway, so
		// continuing is harmless — but return to skip the per-element matrix loads /
		// mDraws[i]->draw() (also no-ops) and keep the draw path cheap when capturing.
		return;
	}
#endif
	GXCallDisplayList(mGDCommands, 0xC0);

	J3DShapeMtx::currentPipeline = unk8 >> 2 & 3;
	loadVtxArray();

#ifdef SMS_NATIVE_PLATFORM
	// mDrawMatrices/mNormMatrices are populated by the owning J3DShapePacket::
	// draw() from mDrawMtxBuf[1]/mNrmMtxBuf[1]. A shape whose owning J3DModel
	// hasn't yet had update() called (e.g. TMarDirector::setup2's early
	// perform(0xffffffff) fires drawHead before any actor perform(0x200) has
	// run update on its model) reaches here with null buffers. Skip cleanly
	// -- the next frame's update->draw cycle will supply them.
	static int s_skipDbg = -1;
	if (mDrawMatrices == nullptr || mNormMatrices == nullptr) {
		if (s_skipDbg < 0)
			s_skipDbg = getenv("SB_J3DSHAPE_SKIP_DBG") ? 1 : 0;
		if (s_skipDbg)
			OSReport("[SBDBG] J3DShape::draw skipping shape=%p mIndex=%u mDraw=%p mNrm=%p (owner J3DModel never updated)\n",
			         (void*)this, (unsigned)mIndex, (void*)mDrawMatrices, (void*)mNormMatrices);
		return;
	}
#endif

	j3dSys.setModelDrawMtx(mDrawMatrices[*mCurrentViewNo]);
	j3dSys.setModelNrmMtx(mNormMatrices[*mCurrentViewNo]);

	JRNLoadCurrentMtx(0, unk3C[0], unk3C[1], unk3C[2], unk3C[3], unk3C[4],
	                  unk3C[5], unk3C[6], unk3C[7]);

#ifdef SMS_NATIVE_PLATFORM
	// SB_SHAPE_STATS=1: once per shape, log the element (mtx-group) fan-out and
	// each element's display-list size — decides "strips lost at LOAD (too few
	// elements / truncated DLs)" vs "strips lost in the fifo parse".
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_SHAPE_STATS"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dbg) {
			static const J3DShape* seen[512];
			static int nSeen = 0;
			bool isNew = true;
			for (int s = 0; s < nSeen; ++s)
				if (seen[s] == this) { isNew = false; break; }
			if (isNew && nSeen < 512) {
				seen[nSeen++] = this;
				u32 total = 0;
				char sizes[256]; int sp = 0;
				for (u16 i = 0; i < mElementCount; ++i) {
					u32 sz = mDraws[i] ? mDraws[i]->getDisplayListSize() : 0;
					total += sz;
					if (sp < (int)sizeof(sizes) - 16)
						sp += snprintf(sizes + sp, sizeof(sizes) - sp, " %u%s", sz, mMatrices[i] ? "" : "/nomtx");
				}
				fprintf(stderr, "[shape-stats] shape=%p idx=%u elems=%u dlbytes=%u sizes=[%s ]\n",
				        (void*)this, (unsigned)mIndex, (unsigned)mElementCount, total, sizes);
			}
		}
	}
#endif

	for (u16 i = 0; i < mElementCount; ++i) {
		if (mMatrices[i])
			mMatrices[i]->load();
		if (mDraws[i])
			mDraws[i]->draw();
	}
}
