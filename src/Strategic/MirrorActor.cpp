#include <Strategic/MirrorActor.hpp>
#include <Map/MapMirror.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Player/MarioAccess.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JDrama/JDRDrawBufObj.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

void TMirrorActor::isInMirror() const { }

void TMirrorActor::checkIsInMirror()
{
	if (mFlags & 1) {
		mInMirror = 0;
		return;
	}

	if (mFlags & 2) {
		if (!gpMirrorModelManager->isCurrentMirrorPresent() && !(mFlags & 4)) {
			if (mMirrorModel->getShapePacket(0)->isVisible())
				SMS_HideAllShapePacket(mMirrorModel);
			mInMirror = 0;
		} else {
			if (!mMirrorModel->getShapePacket(0)->isVisible())
				SMS_ShowAllShapePacket(mMirrorModel);
			mInMirror = 1;
		}

		return;
	}

	MtxPtr mtx = mSourceModel->getAnmMtx(0);
	JGeometry::TVec3<f32> local_18;
	if (!(mFlags & 4)) {
		local_18.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	} else {
		local_18.set(*gpMarioPos);
	}

	int uVar4 = gpCubeMirror->getDataNo(gpCubeMirror->getInCubeNo(local_18));
	if (uVar4 != gpMirrorModelManager->mCurrentMirrorIndex) {
		mInMirror = 0;
	} else if (!gpMirrorModelManager->isCurrentMirrorPresent() && !(mFlags & 4)) {
		mInMirror = 0;
	} else if (gpMirrorModelManager->isCurrentMirrorPresent()
	           && !gpMirrorModelManager->isUpperThanMirrorPlane(local_18)) {
		mInMirror = 0;
	} else {
		mInMirror = 1;
	}
}

void TMirrorActor::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	if (param_1 & 2) {
		checkIsInMirror();
		if (mInMirror == 0)
			return;

		for (u16 i = 0; i < mSourceModel->getModelData()->getJointNum(); ++i)
			mMirrorModel->setAnmMtx(i, mSourceModel->getAnmMtx(i));

		for (u16 i = 0; i < unk10->getModelData()->getWEvlpMtxNum(); ++i)
			unk14->setWeightAnmMtx(i, unk10->getWeightAnmMtx(i));
	}

	if ((param_1 & 4) && mInMirror != 0)
		mMirrorModel->viewCalc();

	if ((param_1 & 0x200) && mInMirror && !(mFlags & 2)) {
#ifdef SMS_NATIVE_PLATFORM
		// SB_NO_MIRROR_ENTRY=1 (diagnostic): suppress the mirror-clone's packet
		// entry to A/B the "clone overdraws the real Mario in the main view"
		// hypothesis (2026-07-16 black patches).
		static int skip = -1;
		if (skip < 0) { const char* e = getenv("SB_NO_MIRROR_ENTRY"); skip = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (skip) return;
#endif
		mMirrorModel->entry();
	}
}

void TMirrorActor::entryMirrorDrawBufferAlways(J3DModel* model)
{
	JDrama::TDrawBufObj* dbOpa = static_cast<JDrama::TDrawBufObj*>(
	    JDrama::TNameRefGen::search("DrawBuf MirrorAlways Opa"));
	j3dSys.setDrawBuffer(dbOpa->getDrawBuffer(), 0);
	JDrama::TDrawBufObj* dbXlu = static_cast<JDrama::TDrawBufObj*>(
	    JDrama::TNameRefGen::search("DrawBuf MirrorAlways Xlu"));
	j3dSys.setDrawBuffer(dbXlu->getDrawBuffer(), 1);
	model->calc();
	model->viewCalc();
	model->entry();
}

void TMirrorActor::init(J3DModel* param_1, u16 param_2)
{
	mFlags       = param_2;
	mSourceModel = param_1;

	if (mFlags & 8)
		mMirrorModel = new SDLModel(((SDLModel*)param_1)->getSDLModelData(), 3, 1);
	else {
		J3DModelData* modelData = mSourceModel->getModelData();
		mMirrorModel            = new J3DModel(modelData, 0, 1);
	}

	JDrama::TViewObjPtrListT<JDrama::TViewObj>* mirrorScene
	    = static_cast<JDrama::TViewObjPtrListT<JDrama::TViewObj>*>(
	        JDrama::TNameRefGen::search("鏡シーン"));
	mirrorScene->getChildren().push_back(this);

	if (mFlags & 2)
		entryMirrorDrawBufferAlways(mMirrorModel);
}

TMirrorActor::TMirrorActor(const char* name)
    : JDrama::TViewObj(name)
    , mSourceModel(nullptr)
    , mMirrorModel(0)
    , mInMirror(0)
    , mFlags(0)
{
}
