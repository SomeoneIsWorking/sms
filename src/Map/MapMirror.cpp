#include <JSystem/JSupport/JSUInputStream.hpp> // JSU_BE32 / JSU_BE32_INPLACE
#include <Map/MapMirror.hpp>
#include <Map/MapData.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorData.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <System/MarDirector.hpp>
#include <Player/MarioAccess.hpp>
#include <Camera/Camera.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// DEAD CODE — declared non-virtual in header, never called anywhere in the
// port tree, and NOT emitted in any binary (GMSE01/GMSJ01/GMSP01 symbol
// tables have no entry for makeMirrorViewMtx / calcEffectMtx / entry /
// calcView / getMirrorTexInfo). The empty { } bodies satisfy the header
// declaration so this TU still compiles; not gaps to port. Confirmed
// 2026-07-04 via full-tree grep + funcs.txt sweep.
void TMirrorCamera::makeMirrorViewMtx() { }

void TMirrorCamera::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	if (param_1 & 0x14) {
		C_MTXPerspective(param_2->mProjMtx.mMtx, mFovyScale * gpCamera->mFovy,
		                 gpCamera->mAspect, gpCamera->mNear, gpCamera->mFar);
		MTXCopy(mMirrorViewMtx, param_2->mViewMtx);
		param_2->mNearPlane = gpCamera->mNear;
		param_2->mFarPlane  = gpCamera->mFar;
		if (param_1 & 0x10)
			GXSetProjection(param_2->mProjMtx.mMtx, GX_PERSPECTIVE);
		GXSetAlphaUpdate(GX_TRUE);
	}
}

void TMirrorCamera::drawSetting(MtxPtr param_1)
{
	GXLoadTexObj(&mMirrorTexObj, GX_TEXMAP0);
	Mtx afStack_38;
	C_MTXLightPerspective(afStack_38, mFovyScale * gpCamera->mFovy,
	                      gpCamera->mAspect, 1.0f, -1.0f, 1.0f, 1.0f);

	Mtx afStack_68;
	MTXConcat(getMirrorViewMtx(), param_1, afStack_68);
	Mtx afStack_98;
	MTXConcat(afStack_38, afStack_68, afStack_98);
	GXLoadTexMtxImm(afStack_98, 0x1E, GX_MTX3x4);
}

void TMirrorCamera::calcEffectMtx(MtxPtr) { }

TMirrorCamera::TMirrorCamera(const char* name)
    : JDrama::TCamera(10.0f, 300000.0f, name)
    , mFovyScale(1.3f)
    , mPlaneNormal(0.0f, 0.0f, 0.0f)
    , mPlaneD(0.0f)
    , mMirrorTexResource(nullptr)
{
	mMirrorTexResource = (ResTIMG*)new (0x20)
	    u8[GXGetTexBufferSize(0x100, 0x100, 5, 0, 0) + sizeof(ResTIMG)];
	memset(mMirrorTexResource, 0, sizeof(ResTIMG));
	mMirrorTexResource->format          = GX_TF_RGB5A3;
	mMirrorTexResource->alphaEnabled    = true;
	mMirrorTexResource->width           = 0x100;
	mMirrorTexResource->height          = 0x100;
	mMirrorTexResource->minFilter       = 1;
	mMirrorTexResource->magFilter       = 1;
	mMirrorTexResource->mipmapCount     = 1;
	mMirrorTexResource->imageDataOffset = 0x20;

	GXInitTexObj(&mMirrorTexObj, (u8*)mMirrorTexResource + mMirrorTexResource->imageDataOffset,
	             mMirrorTexResource->width, mMirrorTexResource->height,
	             (GXTexFmt)mMirrorTexResource->format, GX_REPEAT, GX_REPEAT, 0);

	GXInitTexObjLOD(&mMirrorTexObj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE,
	                GX_FALSE, GX_ANISO_1);
	Vec local_20 = (Vec) { 10000.0f, 10000.0f, 10000.0f };
	Vec local_2C = (Vec) { 0.0f, 1.0f, 0.0f };
	Vec local_38 = (Vec) { 20000.0f, 20000.0f, 20000.0f };
	C_MTXLookAt(mMirrorViewMtx, &local_20, &local_2C, &local_38);
	mReflectedPos.zero();
}

static u8 getVertexFormat(const J3DModelData* model_data, GXAttr attr)
{
	const GXVtxAttrFmtList* list
	    = model_data->getVertexData().getVtxAttrFmtList();
	for (; list->attr != GX_VA_NULL; ++list)
		if (list->attr == attr)
			return list->type;
	return 0xff;
}

void TMirrorModel::setPlane()
{
	MtxPtr mtx = mMActor->unk4->mBaseMtx;
	MTXMultVec(mtx, &mPlanePoint, &mPlanePoint);
	MTXMultVecSR(mtx, &mPlaneNormal, &mPlaneNormal);
	VECNormalize(&mPlaneNormal, &mPlaneNormal);
	mPlaneD = -VECDotProduct(&mPlaneNormal, &mPlanePoint);
	mMirrorCamera->setMirrorPlane(mPlaneNormal.x, mPlaneNormal.y, mPlaneNormal.z, mPlaneD);
}

void TMirrorModel::initPlaneInfo()
{
	u8 posComp = getVertexFormat(mMActor->getModel()->getModelData(), GX_VA_POS);

	if (posComp == GX_S16) {
		S16Vec* v = (S16Vec*)mMActor->getModel()
		                ->getModelData()
		                ->getVertexData()
		                .getVtxPosArray();
		mPlanePoint.x = v->x;
		mPlanePoint.y = v->y;
		mPlanePoint.z = v->z;
	} else {
		Vec* v = (Vec*)mMActor->getModel()
		             ->getModelData()
		             ->getVertexData()
		             .getVtxPosArray();
		mPlanePoint.x = v->x;
		mPlanePoint.y = v->y;
		mPlanePoint.z = v->z;
	}

	u8 normComp = getVertexFormat(mMActor->getModel()->getModelData(), GX_VA_NRM);

	if (normComp == GX_S16) {
		S16Vec* v = (S16Vec*)mMActor->getModel()
		                ->getModelData()
		                ->getVertexData()
		                .getVtxNormArray();
		// BUG: probably meant to do a float division here?
		mPlaneNormal.x = v->x / 16384;
		mPlaneNormal.y = v->y / 16384;
		mPlaneNormal.z = v->z / 16384;
	} else if (normComp == GX_F32) {
		Vec* v = (Vec*)mMActor->getModel()
		             ->getModelData()
		             ->getVertexData()
		             .getVtxNormArray();
		mPlaneNormal.x = v->x;
		mPlaneNormal.y = v->y;
		mPlaneNormal.z = v->z;
	} else {
		mPlaneNormal.x = 0.0f;
		mPlaneNormal.y = 1.0f;
		mPlaneNormal.z = 0.0f;
	}
}

void TMirrorModel::entry() { }

void TMirrorModel::calcView() { }

void TMirrorModel::getMirrorTexInfo() { }

inline static void identity34(MtxPtr mtx)
{
	mtx[2][3] = 0.0f;
	mtx[1][3] = 0.0f;
	mtx[0][3] = 0.0f;
	mtx[1][2] = 0.0f;
	mtx[0][2] = 0.0f;
	mtx[2][1] = 0.0f;
	mtx[0][1] = 0.0f;
	mtx[2][0] = 0.0f;
	mtx[1][0] = 0.0f;
	mtx[2][2] = 1.0f;
	mtx[1][1] = 1.0f;
	mtx[0][0] = 1.0f;
}

void TMirrorModel::init(const char* name)
{
	mMActor = SMS_MakeMActorWithAnmData(name,
	                                    gpMirrorModelManager->getMirrorAnmData(),
	                                    2,
	                                    J3DMLF_MaterialPEFull
	                                        | J3DMLF_UseUniqueMaterials
	                                        | (1 << J3DMLF_TevStageNumShift));

	TPosition3f local_44;
	local_44.identity();
	mMActor->getModel()->setBaseTRMtx(local_44);
	mMActor->calc();
	mMActor->getModel()->getModelData()->getMaterialNodePointer(0)->change();

	if (!gpMirrorModelManager->mMirrorCamera)
		gpMirrorModelManager->findMirrorCamera();
	mMirrorCamera = gpMirrorModelManager->mMirrorCamera;

	initPlaneInfo();
}

TMirrorModel::TMirrorModel()
    : mMActor(nullptr)
    , mMirrorCamera(0)
    , mPlaneD(0.0f)
{
	mPlanePoint.zero();
	mPlaneNormal.zero();
}

void TMirrorModelObj::setPlane()
{
	MtxPtr mtx = mMActor->getModel()->getAnmMtx(0);
	Vec* v     = (Vec*)mMActor->getModel()
	             ->getModelData()
	             ->getVertexData()
	             .getVtxPosArray();

	JGeometry::TVec3<f32> local_18;
	local_18.x = v->x;
	local_18.y = v->y;
	local_18.z = v->z;

	mPlaneNormal.x = mtx[0][1];
	mPlaneNormal.y = mtx[1][1];
	mPlaneNormal.z = mtx[2][1];

	MTXMultVec(mtx, &local_18, &local_18);
	mPlaneD = -VECDotProduct(mPlaneNormal, local_18);
	mMirrorCamera->setMirrorPlane(mPlaneNormal.x, mPlaneNormal.y, mPlaneNormal.z, mPlaneD);
}

void TMirrorModelObj::calc()
{
	mMActor->getModel()->setAnmMtx(0, mSourceModel->getAnmMtx(0));
}

void TMirrorModelObj::init(const char* name)
{
	TMirrorModel::init(name);
	gpMirrorModelManager->registerObjMirror(this);
}

TMirrorModelManager* gpMirrorModelManager;

bool TMirrorModelManager::isUpperThanMirrorPlane(
    const JGeometry::TVec3<f32>& param_1) const
{
	const JGeometry::TVec3<f32>* normal
	    = mCurrentMirrorIndex != -1 ? &mMirrorModels[mCurrentMirrorIndex]->getNormalVec() : nullptr;

	f32 d   = mCurrentMirrorIndex != -1 ? mMirrorModels[mCurrentMirrorIndex]->getD() : 0.0f;
	f32 dot = normal->dot(param_1);

	return dot + d < -50.0f ? false : true;
}

bool TMirrorModelManager::isInMirror(JGeometry::TVec3<f32>& param_1) const
{
	return gpCubeMirror->getDataNo(gpCubeMirror->getInCubeNo(param_1)) != -1
	           ? true
	           : false;
}

void TMirrorModelManager::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	JGeometry::TVec3<f32> local_44 = *gpMarioPos;
	mCurrentMirrorIndex = gpCubeMirror->getDataNo(gpCubeMirror->getInCubeNo(local_44));
	if (!(mCurrentMirrorIndex != -1 ? true : false)
	    && !gpMarioGroundPlane[0]->checkFlag(BG_CHECK_FLAG_ILLEGAL)) {
		mMirrorCamera->mPlaneNormal = gpMarioGroundPlane[1]->mNormal;
		mMirrorCamera->mPlaneD      = gpMarioGroundPlane[1]->mPlaneDistance;

		JGeometry::TVec3<f32> local_7C;
		local_7C.set(mMirrorCamera->mPlaneNormal);
		f32 fVar4 = (local_7C.dot(gpCamera->unk124) - -mMirrorCamera->mPlaneD) * -2.0f;
		mMirrorCamera->mReflectedPos.scaleAdd(fVar4, gpCamera->unk124, local_7C);
		// TODO: awful vector math, one of unused functions inlined
	}

	if (mCurrentMirrorIndex != -1) {
		if (param_1 & 2)
			mMirrorModels[mCurrentMirrorIndex]->calc();

		if (param_1 & 4)
			mMirrorModels[mCurrentMirrorIndex]->mMActor->viewCalc();

		if (param_1 & 0x200) {
			mMirrorModels[mCurrentMirrorIndex]->setPlane();

			// TODO: awful vector math, one of unused functions inlined
		}
	}
}

void TMirrorModelManager::findMirrorCamera()
{
	mMirrorCamera = JDrama::TNameRefGen::search<TMirrorCamera>("鏡カメラ");
}

void TMirrorModelManager::loadAfter()
{
	if (!mMirrorCamera)
		findMirrorCamera();

	for (int i = 0; i < mMirrorModelCount; ++i) {
		J3DTexture* texture
		    = mMirrorModels[i]->getMActor()->getModel()->getModelData()->getTexture();

		// This looks like setResTIMG but isn't???

		const ResTIMG& source = *mMirrorCamera->getMirrorTexResource();
		ResTIMG& target       = texture->mResources[0];

		target = source;
		target.imageDataOffset
		    = (u32)((uintptr_t)&source + source.imageDataOffset - (uintptr_t)&target);
	}
}

void TMirrorModelManager::registerObjMirror(TMirrorModel* model)
{
	mMirrorModels[mMirrorModelCount] = model;
	mMirrorModelCount++;
}

void TMirrorModelManager::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);
	int local_28;
	int local_2C;
	int local_30;
	stream.read(&local_28, 4);
	stream.read(&local_2C, 4);
	stream.read(&local_30, 4);
	// BE dword on disc; raw read(&x,4) does not swap (JSU raw-read class).
	local_28 = (int)JSU_BE32((u32)local_28);
	local_2C = (int)JSU_BE32((u32)local_2C);
	local_30 = (int)JSU_BE32((u32)local_30);
	mTotalMirrorSlots = local_28 + local_2C + local_30 * 2;

	if (mTotalMirrorSlots != 0) {
		mMirrorAnmData = new MActorAnmData;
		mMirrorModels  = new TMirrorModel*[mTotalMirrorSlots];
		for (int i = 0; i < local_28; ++i) {
			mMirrorModels[i] = new TMirrorModel;
			char acStack_130[0x100];
			if (gpMarDirector->getCurrentMap() == 7) {
				static const char* table[] = { "205", nullptr };
				snprintf(acStack_130, 0x100, "/scene/map/mirror/mirror%s.bmd",
				         table[i]);
			} else {
				snprintf(acStack_130, 0x100, "/scene/map/mirror/mirror%02d.bmd",
				         i);
			}
			mMirrorModels[i]->init(acStack_130);
			++mMirrorModelCount;
		}
	}
}

TMirrorModelManager::TMirrorModelManager(const char* name)
    : JDrama::TViewObj(name)
    , mMirrorModelCount(0)
    , mTotalMirrorSlots(0)
    , mCurrentMirrorIndex(-1)
    , mMirrorModels(nullptr)
    , mMirrorCamera(0)
    , unk28(0)
{
	gpMirrorModelManager = this;
}

void TMirrorMapDrawBuf::perform(u32 param_1, JDrama::TGraphics* param_2)
{
#ifdef SMS_NATIVE_PLATFORM
	// SB_MIRRORBUF_DBG: which named draw-buffers are TMirrorMapDrawBuf, and the mirror gate state.
	// If "DrawBuf MapXlu" appears here, the ph6 mask overdraw = broken mirror gate
	// (mCurrentMirrorIndex should be -1 in file-select → the draw(0x8) should be SUPPRESSED;
	// native evidently lets it through).
	if (const char* e = std::getenv("SB_MIRRORBUF_DBG"); e && e[0] && e[0] != '0') {
		static int n = 0; if (n < 60) { ++n;
			std::fprintf(stderr, "[mirrorbuf] name='%s' flag=0x%x mCurrentMirrorIndex=%d draws=%d\n",
			             getName() ? getName() : "?", param_1,
			             gpMirrorModelManager->mCurrentMirrorIndex,
			             (!(param_1 & 8) || gpMirrorModelManager->mCurrentMirrorIndex != -1) ? 1 : 0);
		}
	}
#endif
	if (!(param_1 & 8) || (gpMirrorModelManager->mCurrentMirrorIndex != -1 ? true : false))
		JDrama::TDrawBufObj::perform(param_1, param_2);
}
