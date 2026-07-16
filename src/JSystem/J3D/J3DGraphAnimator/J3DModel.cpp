#ifdef SMS_NATIVE_PLATFORM
#include <sb_log.h>
#endif
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
extern "C" void sb_gx_get_projection(int*, float*, float*);
#endif
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DMaterialAnm.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DMaterialAnm.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#include <dolphin/os/OSCache.h>
#include <math.h>

void J3DModelData::clear()
{
	unk4              = 0;
	unk8              = 0;
	unkC              = 0;
	mRootNode         = 0;
	unk14             = 0;
	unk18             = 0;
	mbHasBillboard    = 0;
	mJointNum         = 0;
	mJointNodePointer = (J3DJoint**)0x0;
	mMaterialNum      = 0;
	mMaterials        = (J3DMaterial**)0x0;
	mShapeNum         = 0;
	mShapeNodePointer = nullptr;
	unk34             = 0;
	unk38             = 0;
	unkA8             = nullptr;
	unkAC             = nullptr;
	unkA4             = 0;
	mWEvlpMtxNum      = 0;
	mWEvlpMixMtxNum   = nullptr;
	mWEvlpMixIndex    = nullptr;
	mWEvlpMixWeight   = nullptr;
	mInvJointMtx      = nullptr;
	unkB0             = nullptr;
	mMaterialName     = nullptr;
	unkB8             = nullptr;
	unk80             = 0;
}

J3DModelData::J3DModelData() { clear(); }

J3DModelData::~J3DModelData() { }

void J3DModelData::makeHierarchy(J3DNode* root_node,
                                 const J3DModelHierarchy** hierarchy)
{
	enum {
		kTypeEnd        = 0x00,
		kTypeBeginChild = 0x01,
		kTypeEndChild   = 0x02,
		kTypeJoint      = 0x10,
		kTypeMaterial   = 0x11,
		kTypeShape      = 0x12,
	};

	J3DNode* curNode = root_node;

	while (true) {
		J3DJoint* newJoint       = nullptr;
		J3DMaterial* newMaterial = nullptr;
		J3DShape* newShape       = nullptr;

		switch ((*hierarchy)->mType) {
		case kTypeBeginChild:
			++*hierarchy;
			makeHierarchy(curNode, hierarchy);
			break;
		case kTypeEndChild:
			++*hierarchy;
			return;
		case kTypeEnd:
			return;
		case kTypeJoint:
			newJoint = mJointNodePointer[((*hierarchy)++)->mValue];
			break;
		case kTypeMaterial:
			newMaterial = mMaterials[((*hierarchy)++)->mValue];
			break;
		case kTypeShape:
			newShape = mShapeNodePointer[((*hierarchy)++)->mValue];
			break;
		}

		if (newJoint != nullptr) {
			curNode = newJoint;
			if (root_node == nullptr)
				mRootNode = newJoint;
			else
				root_node->appendChild(newJoint);
		} else if (newMaterial != nullptr && root_node->getType() == 'NJNT') {
			((J3DJoint*)root_node)->addMesh(newMaterial);
		} else if (newShape != nullptr && root_node->getType() == 'NJNT') {
			((J3DJoint*)root_node)->getMesh()->addShape(newShape);
			newShape->setDrawMtxDataPointer(&mDrawMtxData);
			newShape->setVertexDataPointer(&mVertexData);
			newShape->makeVcdVatCmd();
		}
	}
}

bool J3DModelData::isDeformableVertexFormat() const
{
	GXVtxAttrFmtList* vtxAttrFmtList;
	bool bVar1 = false;
	for (vtxAttrFmtList = mVertexData.mVtxAttrFmtList;
	     vtxAttrFmtList->attr != GX_VA_NULL; ++vtxAttrFmtList) {
		switch (vtxAttrFmtList->attr) {
		case GX_VA_POS:
			if (vtxAttrFmtList->type != GX_F32
			    || vtxAttrFmtList->cnt != GX_CLR_RGBA)
				return false;
			break;
		case GX_VA_NRM:
			bVar1 = true;
			if (vtxAttrFmtList->type != GX_F32)
				return false;
			break;
		default:
			break;
		}
	}

	if (bVar1)
		return true;

	return false;
}

void J3DModelData::setMaterialTable(J3DMaterialTable* mat_table,
                                    J3DMaterialCopyFlag flag)
{
	if (flag & J3DMatCopyFlag_Material) {
		for (u16 i = 0; i < mat_table->getMaterialNum(); i++) {
			s32 nameIndex = getMaterialName()->getIndex(
			    mat_table->getMaterialName()->getName(i));
			if (nameIndex != -1)
				getMaterialNodePointer(nameIndex)->copy(
				    mat_table->getMaterialNodePointer(i));
		}
	}

	if (flag & J3DMatCopyFlag_Texture) {
		if (mat_table->getTexture()->getNum() != 0) {
			setTexture(mat_table->getTexture());
			setTextureName(mat_table->getTextureName());
		}
	}
}

int J3DModelData::entryMatColorAnimator(J3DAnmColor* anm)
{
	int ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		u16 materialID = anm->getUpdateMaterialID(i);
		if (materialID != 0xFFFF) {
			J3DMaterial* mat        = getMaterialNodePointer(materialID);
			J3DMaterialAnm* pMatAnm = mat->getMaterialAnm();
			if (pMatAnm == NULL) {
				ret = 1;
			} else {
				J3DMatColorAnm* matColorAnm = new J3DMatColorAnm(anm, i);
				pMatAnm->setMatColorAnm(0, matColorAnm);
			}
		}
	}

	return ret;
}

int J3DModelData::entryTexMtxAnimator(J3DAnmTextureSRTKey* anm)
{
	s32 ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		if (anm->isValidUpdateMaterialID(i)) {
			u16 materialID         = anm->getUpdateMaterialID(i);
			J3DMaterial* material  = getMaterialNodePointer(materialID);
			J3DMaterialAnm* matAnm = material->getMaterialAnm();
			u16 texMtxID           = anm->getUpdateTexMtxID(i);

			if (matAnm == nullptr) {
				ret = 1;
				continue;
			}

			if (texMtxID == 0xFF)
				continue;

			if (material->getTexGenBlock()->getTexMtx(texMtxID) == NULL) {
				material->getTexGenBlock()->setTexMtx(texMtxID, new J3DTexMtx);
				material->getTexCoord(texMtxID)->setTexGenMtx(
				    (GXTexMtx)(GX_TEXMTX0 + texMtxID * 3));
			}

			J3DTexMtx* pTexMtx       = material->getTexMtx(texMtxID);
			J3DTexMtxAnm* pTexMtxAnm = new J3DTexMtxAnm(anm, i);

			pTexMtx->mInfo
			    = ((pTexMtx->mInfo) & 0x7F) | (anm->getTexMtxCalcType() << 7);

			pTexMtx->mCenter.x = anm->getSRTCenter(i)->x;
			pTexMtx->mCenter.y = anm->getSRTCenter(i)->y;
			pTexMtx->mCenter.z = anm->getSRTCenter(i)->z;

			matAnm->setTexMtxAnm(texMtxID, pTexMtxAnm);
		}
	}

	return ret;
}

int J3DModelData::entryTevRegAnimator(J3DAnmTevRegKey* anm)
{
	int ret             = 0;
	u16 cRegMaterialNum = anm->getCRegUpdateMaterialNum();
	u16 kRegMaterialNum = anm->getKRegUpdateMaterialNum();

	for (u16 i = 0; i < cRegMaterialNum; i++) {
		u16 materialID = anm->getCRegUpdateMaterialID(i);
		if (materialID != 0xFFFF) {
			J3DMaterial* mat        = getMaterialNodePointer(materialID);
			J3DMaterialAnm* pMatAnm = mat->getMaterialAnm();
			u8 colorId              = anm->getAnmCRegKeyTable()[i].mColorId;
			if (pMatAnm == NULL)
				ret = 1;
			else
				pMatAnm->setTevColorAnm(colorId, new J3DTevColorAnm(anm, i));
		}
	}

	for (u16 i = 0; i < kRegMaterialNum; i++) {
		u16 materialID = anm->getKRegUpdateMaterialID(i);
		if (materialID != 0xFFFF) {
			J3DMaterial* mat       = getMaterialNodePointer(materialID);
			J3DMaterialAnm* matAnm = mat->getMaterialAnm();
			u8 colorId             = anm->getAnmKRegKeyTable()[i].mColorId;
			if (matAnm == NULL)
				ret = 1;
			else
				matAnm->setTevKColorAnm(colorId, new J3DTevKColorAnm(anm, i));
		}
	}

	return ret;
}

int J3DModelData::removeMatColorAnimator(J3DAnmColor* anm)
{
	int ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		u16 materialID = anm->getUpdateMaterialID(i);
		if (materialID != 0xFFFF) {
			J3DMaterialAnm* matAnm
			    = getMaterialNodePointer(materialID)->getMaterialAnm();
			if (matAnm == NULL)
				ret = 1;
			else
				matAnm->setMatColorAnm(0, nullptr);
		}
	}

	return ret;
}

int J3DModelData::removeTexNoAnimator(J3DAnmTexPattern* anm)
{
	int ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		u16 materialID = anm->getUpdateMaterialID(i);
		if (materialID != 0xFFFF) {
			J3DMaterialAnm* matAnm
			    = getMaterialNodePointer(materialID)->getMaterialAnm();
			u8 texNo = anm->getAnmTable()[i].mTexNo;
			if (matAnm == nullptr)
				ret = 1;
			else
				matAnm->setTexNoAnm(texNo, nullptr);
		}
	}

	return ret;
}

int J3DModelData::removeTexMtxAnimator(J3DAnmTextureSRTKey* anm)
{
	int ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		u16 materialID         = anm->getUpdateMaterialID(i);
		J3DMaterial* pMaterial = getMaterialNodePointer(materialID);
		J3DMaterialAnm* matAnm = pMaterial->getMaterialAnm();
		u32 texMtxID           = anm->getUpdateTexMtxID(i);
		if (matAnm == nullptr) {
			ret = 1;
		} else if (texMtxID != 0xFF) {
			matAnm->setTexMtxAnm(texMtxID, nullptr);
		}
	}

	return ret;
}

int J3DModelData::removeTevRegAnimator(J3DAnmTevRegKey* anm)
{
	int ret             = 0;
	u16 cRegMaterialNum = anm->getCRegUpdateMaterialNum();
	u16 kRegMaterialNum = anm->getKRegUpdateMaterialNum();

	for (u16 i = 0; i < cRegMaterialNum; i++) {
		if (anm->getCRegUpdateMaterialID(i) != 0xFFFF) {
			J3DMaterialAnm* pMatAnm
			    = getMaterialNodePointer(anm->getCRegUpdateMaterialID(i))
			          ->getMaterialAnm();
			u8 colorId = anm->getAnmCRegKeyTable()[i].mColorId;
			if (pMatAnm == nullptr)
				ret = 1;
			else
				pMatAnm->setTevColorAnm(colorId, nullptr);
		}
	}

	for (u16 i = 0; i < kRegMaterialNum; i++) {
		if (anm->getKRegUpdateMaterialID(i) != 0xFFFF) {
			J3DMaterialAnm* pMatAnm
			    = getMaterialNodePointer(anm->getKRegUpdateMaterialID(i))
			          ->getMaterialAnm();
			u8 colorId = anm->getAnmKRegKeyTable()[i].mColorId;
			if (pMatAnm == nullptr) {
				ret = 1;
			} else {
				pMatAnm->setTevKColorAnm(colorId, nullptr);
			}
		}
	}

	return ret;
}

int J3DModelData::setMatColorAnimator(J3DAnmColor* anm, J3DMatColorAnm* anm_r)
{
	int ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		u16 materialID = anm->getUpdateMaterialID(i);
		if (materialID != 0xFFFF) {
			J3DMaterialAnm* matAnm
			    = getMaterialNodePointer(materialID)->getMaterialAnm();
			if (matAnm == NULL)
				ret = 1;
			else
				matAnm->setMatColorAnm(0, &anm_r[i]);
		}
	}

	return ret;
}

int J3DModelData::setTexNoAnimator(J3DAnmTexPattern* anm, J3DTexNoAnm* anm_r)
{
	int ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		u16 materialID = anm->getUpdateMaterialID(i);
		if (materialID != 0xFFFF) {
			J3DMaterialAnm* pMatAnm
			    = getMaterialNodePointer(materialID)->getMaterialAnm();
			u8 texNo = anm->getAnmTable()[i].mTexNo;
			if (pMatAnm == NULL)
				ret = 1;
			else
				pMatAnm->setTexNoAnm(texNo, &anm_r[i]);
		}
	}

	return ret;
}

int J3DModelData::setTexMtxAnimator(J3DAnmTextureSRTKey* anm,
                                    J3DTexMtxAnm* tex_anm,
                                    J3DTexMtxAnm* dual_anm_r)
{
	int ret         = 0;
	u16 materialNum = anm->getUpdateMaterialNum();

	for (u16 i = 0; i < materialNum; i++) {
		u16 materialID          = anm->getUpdateMaterialID(i);
		J3DMaterial* pMaterial  = getMaterialNodePointer(materialID);
		J3DMaterialAnm* pMatAnm = pMaterial->getMaterialAnm();
		u16 texMtxID            = anm->getUpdateTexMtxID(i);
		if (pMatAnm == NULL) {
			ret = 1;
		} else if (texMtxID != 0xFF) {
			J3DTexMtx* pTexMtx = pMaterial->getTexMtx(texMtxID);
			pTexMtx->mInfo
			    = ((pTexMtx->mInfo) & 0x7F) | (anm->getTexMtxCalcType() << 7);

			pTexMtx->mCenter.x = anm->getSRTCenter(i)->x;
			pTexMtx->mCenter.y = anm->getSRTCenter(i)->y;
			pTexMtx->mCenter.z = anm->getSRTCenter(i)->z;

			pMatAnm->setTexMtxAnm(texMtxID, &tex_anm[i]);
		}
	}

	return ret;
}

int J3DModelData::setTevRegAnimator(J3DAnmTevRegKey* pAnm,
                                    J3DTevColorAnm* pCRegAnmR,
                                    J3DTevKColorAnm* pKRegAnmR)
{
	s32 ret             = 0;
	u16 cRegMaterialNum = pAnm->getCRegUpdateMaterialNum();
	u16 kRegMaterialNum = pAnm->getKRegUpdateMaterialNum();

	for (u16 i = 0; i < cRegMaterialNum; i++) {
		if (pAnm->getCRegUpdateMaterialID(i) != 0xFFFF) {
			J3DMaterialAnm* pMatAnm
			    = getMaterialNodePointer(pAnm->getCRegUpdateMaterialID(i))
			          ->getMaterialAnm();
			u8 colorId = pAnm->getAnmCRegKeyTable()[i].mColorId;
			if (pMatAnm == NULL)
				ret = 1;
			else
				pMatAnm->setTevColorAnm(colorId, &pCRegAnmR[i]);
		}
	}

	for (u16 i = 0; i < kRegMaterialNum; i++) {
		if (pAnm->getKRegUpdateMaterialID(i) != 0xFFFF) {
			J3DMaterialAnm* pMatAnm
			    = getMaterialNodePointer(pAnm->getKRegUpdateMaterialID(i))
			          ->getMaterialAnm();
			u8 colorId = pAnm->getAnmKRegKeyTable()[i].mColorId;
			if (pMatAnm == NULL) {
				ret = 1;
			} else {
				pMatAnm->setTevKColorAnm(colorId, &pKRegAnmR[i]);
			}
		}
	}

	return ret;
}

J3DModel::J3DModel() { initialize(); }

J3DModel::J3DModel(J3DModelData* model_data, u32 model_flag,
                   u32 mtx_buffer_flag)
{
	initialize();
	entryModelData(model_data, model_flag, mtx_buffer_flag);
}

J3DModel::~J3DModel() { }

void J3DModel::initialize()
{

	unkC        = nullptr;
	unk8        = 0;
	mModelData  = nullptr;
	mDeformData = 0;
	mSkinDeform = nullptr;
	mBaseScale.x     = 1.0;
	mBaseScale.y     = 1.0;
	mBaseScale.z     = 1.0;

	MTXIdentity(mBaseMtx);

	mScaleFlagArr     = nullptr;
	mEvlpScaleFlagArr = nullptr;
	mNodeMatrices     = nullptr;
	mWEvlpMtx             = nullptr;

	mDrawMtxBuf[0] = nullptr;
	mDrawMtxBuf[1] = nullptr;
	mNrmMtxBuf[0]  = nullptr;
	mNrmMtxBuf[1]  = nullptr;
	mBumpMtxArr[0] = nullptr;
	mBumpMtxArr[1] = nullptr;

	mCurrentViewNo = 0;
	mVertexBuffer  = nullptr;
	mMatPackets    = nullptr;
	mShapePackets  = nullptr;
	unk9C          = nullptr;
	unk90          = nullptr;
	unk94          = nullptr;
}

void J3DModel::entryModelData(J3DModelData* param_1, u32 param_2, u32 param_3)
{
	mModelData = param_1;
	if (param_1->getJointNum()) {
		mScaleFlagArr = new u8[param_1->getJointNum()];
		if (param_1->getWEvlpMtxNum())
			mEvlpScaleFlagArr = new u8[param_1->getWEvlpMtxNum()];
		mNodeMatrices = new Mtx[param_1->getJointNum()];
	}

	if (param_1->getWEvlpMtxNum())
		mWEvlpMtx = new Mtx[param_1->getWEvlpMtxNum()];

	if (param_3 != 0) {
		for (int i = 0; i < 2; ++i) {
			mDrawMtxBuf[i] = new Mtx*[param_3];
			mNrmMtxBuf[i]  = new Mtx33*[param_3];
			mBumpMtxArr[i] = nullptr;
		}
	}

	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < param_3; ++j) {
			if (param_1->getDrawMtxNum()) {
				mDrawMtxBuf[i][j] = new (0x20) Mtx[param_1->getDrawMtxNum()];
				mNrmMtxBuf[i][j]  = new (0x20) Mtx33[param_1->getDrawMtxNum()];
#ifdef SMS_NATIVE_PLATFORM
				// SB_LOG=j3dbuf: buffer->model registry, to match aurora's
				// [pnzero] zero-matrix upload array bases to their model.
				SB_LOGC("j3dbuf", "model=%p buf[%d][view=%d] draw=%p nrm=%p drawMtxNum=%d joints=%d views=%d",
				        (void*)this, i, j, (void*)mDrawMtxBuf[i][j], (void*)mNrmMtxBuf[i][j],
				        (int)param_1->getDrawMtxNum(), (int)param_1->getJointNum(), (int)param_3);
				// SB_LOG=j3dbt: one-shot creation backtrace per big skinned
				// model — names WHO creates the extra Mario-body models that
				// viewCalc without ever running the envelope blend.
				if (i == 0 && j == 0 && param_1->getDrawMtxNum() > 100 && sb_log_enabled("j3dbt")) {
					sb_logf("j3dbt", "entryModelData model=%p drawMtxNum=%d backtrace:", (void*)this,
					        (int)param_1->getDrawMtxNum());
					void* fr[24];
					int nf = backtrace(fr, 24);
					backtrace_symbols_fd(fr, nf, 2);
				}
#endif
			}
		}
	}

	if (param_1->getShapeNum()) {
		mShapePackets = new J3DShapePacket[param_1->getShapeNum()];

		for (int i = 0; i < param_1->getShapeNum(); ++i)
			mShapePackets[i].unk14 = param_1->getShapeNodePointer(i);
	}

	if (param_1->getMaterialNum()) {
		mMatPackets = new J3DMatPacket[param_1->getMaterialNum()];

		for (int i = 0; i < param_1->getMaterialNum(); ++i) {
			mMatPackets[i].unk38 = param_1->getMaterialNodePointer(i);
			mMatPackets[i].addShapePacket(
			    &mShapePackets
			        [param_1->getMaterialNodePointer(i)->getShape()->mIndex]);
			mMatPackets[i].mTexture = param_1->getTexture();

			if (param_2 & 0x20000) {
				J3DMaterial* mat     = param_1->getMaterialNodePointer(i);
				u32 dlSize           = mat->countDLSize();
				mMatPackets[i].unk30 = mat->newSharedDisplayList(dlSize);
			} else {
				J3DMaterial* mat     = param_1->getMaterialNodePointer(i);
				u32 dlSize           = mat->countDLSize();
				J3DMatPacket* packet = &mMatPackets[i];
				packet->unk30        = new J3DDisplayListObj;
				packet->unk30->newDisplayList(dlSize);
			}
		}
	}

	u16 totalBumpMatrices     = 0;
	u16 totalMatsWithNbtScale = 0;
	for (int i = 0; i < param_1->mMaterialNum; ++i) {
		J3DMaterial* mat      = mModelData->getMaterialNodePointer(i);
		J3DNBTScale* nbtScale = mat->getTexGenBlock()->getNBTScale();
		if (nbtScale->mbHasScale == 1) {
			totalBumpMatrices += mat->getShape()->countBumpMtxNum();
			++totalMatsWithNbtScale;
		}
	}

	if (totalBumpMatrices != 0 && param_3 != 0) {
		for (int i = 0; i < 2; ++i) {
			mBumpMtxArr[i] = new Mtx33**[totalMatsWithNbtScale];
		}
	}

	for (int i = 0; i < 2; ++i) {
		u32 nextBumpMtx = 0;
		for (int j = 0; j < param_1->mMaterialNum; ++j) {
			J3DMaterial* mat      = mModelData->getMaterialNodePointer(j);
			J3DNBTScale* nbtScale = mat->getTexGenBlock()->getNBTScale();
			if (nbtScale->mbHasScale == 1) {
				mBumpMtxArr[i][nextBumpMtx] = new Mtx33*[param_3];

				mat->getShape()->mBumpMtxOffset = nextBumpMtx;
				++nextBumpMtx;
			}
		}
	}

	for (int i = 0; i < 2; ++i) {
		int j2 = 0;
		for (int j = 0; j < param_1->mMaterialNum; ++j) {
			J3DMaterial* mat      = mModelData->getMaterialNodePointer(j);
			J3DNBTScale* nbtScale = mat->getTexGenBlock()->getNBTScale();

			if (nbtScale->mbHasScale != 1)
				continue;

			for (int k = 0; k < param_3; ++k)
				mBumpMtxArr[i][j2][k]
				    = new (0x20) Mtx33[param_1->getDrawMtxNum()];
			++j2;
		}
	}

	if (totalMatsWithNbtScale != 0) {
		mModelData->unk18 = 1;
	}

	mVertexBuffer = new J3DVertexBuffer(&param_1->getVertexData());
}

void J3DModel::lock()
{
	int matNum = mModelData->getMaterialNum();

	for (int i = 0; i < matNum; ++i)
		mMatPackets[i].lock();
}

void J3DModel::unlock()
{
	int matNum = mModelData->getMaterialNum();

	for (int i = 0; i < matNum; ++i)
		mMatPackets[i].unlock();
}

void J3DModel::makeDL()
{
	j3dSys.setModel(this);
	j3dSys.setTexture(mModelData->getTexture());
	for (u16 i = 0; i < mModelData->getMaterialNum(); ++i) {
		j3dSys.setMatPacket(&mMatPackets[i]);
		mModelData->getMaterialNodePointer(i)->makeDisplayList();
	}
}

void J3DModel::setSkinDeform(J3DSkinDeform* pSkinDeform,
                             J3DDeformAttachFlag flags)
{
	mSkinDeform = pSkinDeform;

	if (pSkinDeform == nullptr) {
		unk8 &= ~0x4;
		unk8 &= ~0x8;
	} else {
		unk8 |= 0x4;
		unk8 |= 0x8;
		mSkinDeform->initMtxIndexArray(mModelData);
		mVertexBuffer->copyTransformedVtxArray();
	}
}

#pragma dont_inline on
void J3DModel::calcWeightEnvelopeMtx()
{
#ifdef SMS_NATIVE_PLATFORM
	// PORT (decomp coverage): the retail/base body is empty and no other TU writes the weighted
	// envelope matrices (mWEvlpMtx) — so envelope-skinned joints stayed at stale/world matrices and a
	// per-vertex-skinned mesh (Mario's body/FLUDD mount) smeared. The EVP1 data IS loaded by
	// J3DModelLoader::readEnvelop (unk88=influence count, unk8C=joint index, unk90=weight,
	// unk94=inverse-bind / mpInvJointMtx). Compute the canonical J3D envelope blend at the existing,
	// already-wired call site (calc() runs this before viewCalc reads mWEvlpMtx):
	//   mWEvlpMtx[e] = Σ_k weight_k · ( mNodeMatrices[idx_k] · invBind[idx_k] )
	// (joint world pose × inverse-bind = the skin matrix; the weighted sum is the envelope matrix).
	if (!mModelData)
		return;
	const u16 evlpNum = mModelData->getWEvlpMtxNum();
	if (evlpNum == 0 || !mModelData->mWEvlpMixMtxNum || !mModelData->mWEvlpMixIndex || !mModelData->mWEvlpMixWeight
	    || !mModelData->mInvJointMtx || !mWEvlpMtx)
		return;
	u32 k = 0;   // running index into the flattened index/weight arrays
	for (u16 e = 0; e < evlpNum; ++e) {
		const u8 num = mModelData->mWEvlpMixMtxNum[e];
		Mtx acc;
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 4; ++c)
				acc[r][c] = 0.0f;
		for (u8 j = 0; j < num; ++j, ++k) {
			const u16 mtxIdx = mModelData->mWEvlpMixIndex[k];
			const f32 w      = mModelData->mWEvlpMixWeight[k];
			Mtx skin;
			MTXConcat(getAnmMtx(mtxIdx), mModelData->mInvJointMtx[mtxIdx], skin);
			for (int r = 0; r < 3; ++r)
				for (int c = 0; c < 4; ++c)
					acc[r][c] += w * skin[r][c];
		}
		MTXCopy(acc, mWEvlpMtx[e]);
#ifdef SMS_NATIVE_PLATFORM
		// SB_LOG=evlp: per-envelope blend inputs when the result rotation is
		// zero — splits "no influences / zero weights" from "zero anm joint
		// matrices" (2026-07-16 Mario black patches: 28 zero envelope-mapped
		// draw entries).
		if (acc[0][0] == 0.f && acc[0][1] == 0.f && acc[0][2] == 0.f) {
			float wsum = 0.f;
			u32 kk = k - num;
			int maxIdx = -1;
			for (u8 j = 0; j < num; ++j) {
				wsum += mModelData->mWEvlpMixWeight[kk + j];
				if (mModelData->mWEvlpMixIndex[kk + j] > maxIdx) maxIdx = mModelData->mWEvlpMixIndex[kk + j];
			}
			SB_LOGC("evlp", "model=%p e=%d/%d num=%d wsum=%.3f maxJntIdx=%d joints=%d anm0[maxIdx]=%.4f",
			        (void*)this, (int)e, (int)evlpNum, (int)num, wsum, maxIdx,
			        (int)mModelData->getJointNum(),
			        maxIdx >= 0 ? getAnmMtx((u16)maxIdx)[0][0] : 0.f);
		}
#endif
	}
#endif
}
#pragma dont_inline off

void J3DModel::update()
{
	j3dSys.setModel(this);

	if (checkFlag(4)) {
		j3dSys.onFlag(0x4);
	} else {
		j3dSys.offFlag(0x4);
	}

	if (checkFlag(0x8)) {
		j3dSys.onFlag(0x8);
	} else {
		j3dSys.offFlag(0x8);
	}

	mVertexBuffer->frameInit();

	if (unk9C != nullptr)
		unk9C->calc(mModelData);

	if (mDeformData != NULL)
		mDeformData->deform(this);

	if (unk90 != nullptr)
		unk90->calc(this);

	if (unk94 != nullptr)
		unk94->calc(this);

	j3dSys.setCurrentMtxCalc(mModelData->unk14);
	mModelData->unk14->init(mBaseScale, mBaseMtx);

	j3dSys.setTexture(mModelData->getTexture());
	mModelData->unk14->recursiveUpdate(mModelData->mRootNode);

	calcWeightEnvelopeMtx();

	if (mSkinDeform)
		mSkinDeform->deform(this);

	if (unkC)
		unkC(this, 0);
}

void J3DModel::calc()
{
#ifdef SMS_NATIVE_PLATFORM
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_J3D_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		static long s_n = 0;
		if (dbg && (++s_n % 1000) == 0) {
			MtxPtr v = j3dSys.getViewMtx();
			int pt; float pj[6]; float vp[6];
			sb_gx_get_projection(&pt, pj, vp);
			fprintf(stderr,
			    "[j3dmodel] calc() calls=%ld view=[%.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f] "
			    "projType=%d proj=[%.3f %.3f %.3f %.3f %.3f %.3f] vp=[%.0f %.0f %.0f %.0f]\n",
			    s_n, v[0][0],v[0][1],v[0][2],v[0][3], v[1][0],v[1][1],v[1][2],v[1][3], v[2][0],v[2][1],v[2][2],v[2][3],
			    pt, pj[0],pj[1],pj[2],pj[3],pj[4],pj[5], vp[0],vp[1],vp[2],vp[3]);
		}
	}
#endif
	j3dSys.setModel(this);

	if (checkFlag(4)) {
		j3dSys.onFlag(0x4);
	} else {
		j3dSys.offFlag(0x4);
	}

	if (checkFlag(0x8)) {
		j3dSys.onFlag(0x8);
	} else {
		j3dSys.offFlag(0x8);
	}

	mVertexBuffer->frameInit();

	if (unk9C != nullptr)
		unk9C->calc(mModelData);

	if (mDeformData != NULL)
		mDeformData->deform(this);

	if (unk90 != nullptr)
		unk90->calc(this);

	if (unk94 != nullptr)
		unk94->calc(this);

	j3dSys.setCurrentMtxCalc(mModelData->unk14);
	mModelData->unk14->init(mBaseScale, mBaseMtx);
	j3dSys.setTexture(mModelData->getTexture());
	mModelData->unk14->recursiveCalc(mModelData->mRootNode);
#ifdef SMS_NATIVE_PLATFORM
	// SB_CALC_DBG=1: base matrix in vs root anm matrix out — localizes where a
	// zero/garbage rotation enters (actor base vs the joint-tree recursion).
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_CALC_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dbg) {
			static long n = 0;
			++n;
			if ((n % 400) == 0 || n <= 12) {
				const float* b = (const float*)mBaseMtx; // mBaseMtx
				const float* a = (const float*)getAnmMtx(0);
				fprintf(stderr,
				        "[calc-dbg] n=%ld model=%p base0=[%.3f %.3f %.3f %.1f] baseS=(%g %g %g) anm0=[%.3f %.3f %.3f %.1f] mtxCalc=%p\n",
				        n, (void*)this, b[0], b[1], b[2], b[3], mBaseScale.x, mBaseScale.y, mBaseScale.z,
				        a[0], a[1], a[2], a[3], (void*)mModelData->unk14);
			}
			// One-shot backtrace per model whose base matrix has a zero rotation
			// row — names the actor perform() path that failed to build it.
			{
				const float* b = (const float*)mBaseMtx;
				if (fabsf(b[0]) < 1e-20f && fabsf(b[1]) < 1e-20f && fabsf(b[2]) < 1e-20f) {
					static const void* seen[8];
					static int nseen = 0;
					bool novel = true;
					for (int i = 0; i < nseen; ++i) if (seen[i] == (void*)this) { novel = false; break; }
					if (novel && nseen < 8) {
						seen[nseen++] = (void*)this;
						fprintf(stderr, "[calc-zero-base] model=%p row0=(%g %g %g) trans=(%.1f %.1f %.1f) backtrace:\n",
						        (void*)this, b[0], b[1], b[2], b[3], b[7], b[11]);
						void* fr[24]; int nf = backtrace(fr, 24); backtrace_symbols_fd(fr, nf, 2);
					}
				}
			}
		}
	}
#endif

	calcWeightEnvelopeMtx();

	if (mSkinDeform)
		mSkinDeform->deform(this);

	if (unkC)
		unkC(this, 0);
}

void J3DModel::entry()
{
#ifdef SMS_NATIVE_PLATFORM
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_J3D_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		static long s_n = 0;
		if (dbg && (++s_n % 1000) == 0)
			fprintf(stderr, "[j3dmodel] entry() calls=%ld (this=%p)\n", s_n, (void*)this);
	}
#endif

	j3dSys.setModel(this);

	if (unk8 & 0x4 ? 1 : 0) {
		j3dSys.mFlags |= 0x4;
	} else {
		j3dSys.mFlags &= ~0x4;
	}

	if (unk8 & 0x8 ? 1 : 0) {
		j3dSys.mFlags |= 0x8;
	} else {
		j3dSys.mFlags &= ~0x8;
	}

	j3dSys.setTexture(mModelData->getTexture());

	mModelData->unk14->recursiveEntry(mModelData->mRootNode);
}

void J3DModel::viewCalc()
{
	swapDrawMtx();
	swapNrmMtx();

	if (checkFlag(1)) {
		for (u16 i = 0; i < mModelData->getDrawFullWgtMtxNum(); i++) {
			MTXCopy(getAnmMtx(mModelData->getDrawMtxIndex(i)), getDrawMtx(i));
		}
		for (u16 i = 0; i < mModelData->getWEvlpMtxNum(); i++) {
			MTXCopy(mWEvlpMtx[i],
			        getDrawMtx(mModelData->getDrawFullWgtMtxNum() + i));
		}
	} else {
		MtxPtr viewMtx = j3dSys.getViewMtx();
#ifdef SMS_NATIVE_PLATFORM
		// SB_VIEWCALC_DBG=1: print the concat inputs (view matrix row 0 +
		// node matrix 0 row 0) — discriminates "view rotation is zero" from
		// "anm/node matrices are zero" when draw matrices come out degenerate.
		{
			static int dbg = -1;
			if (dbg < 0) { const char* e = getenv("SB_VIEWCALC_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
			if (dbg) {
				static long n = 0;
				++n;
				if ((n % 400) == 0 || n <= 20) {
					const float* v = (const float*)viewMtx;
					const float* a = (const float*)mNodeMatrices;
					const J3DTransformInfo& ti
					    = mModelData->getJointNodePointer(0)->getTransformInfo();
					fprintf(stderr,
					        "[viewcalc] n=%ld model=%p view0=[%.3f %.3f %.3f %.1f] view1=[%.3f %.3f %.3f %.1f] "
					        "anm0=[%.3f %.3f %.3f %.1f] j0Scale=(%g %g %g) j0Rot=(%d %d %d) j0T=(%g %g %g)\n",
					        n, (void*)this, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
					        a[0], a[1], a[2], a[3], ti.mScale.x, ti.mScale.y, ti.mScale.z,
					        (int)ti.mRotation.x, (int)ti.mRotation.y, (int)ti.mRotation.z,
					        ti.mTranslate.x, ti.mTranslate.y, ti.mTranslate.z);
				}
			}
		}
#endif
		if (mModelData->getDrawFullWgtMtxNum() != 0) {
			J3DMTXConcatArrayIndexedSrc(
			    viewMtx, mNodeMatrices, mModelData->mDrawMtxData.mDrawMtxIndex,
			    getDrawMtxPtr(), mModelData->getDrawFullWgtMtxNum());
		}
		if (mModelData->getDrawMtxNum() > mModelData->getDrawFullWgtMtxNum()) {
			// Envelope draw slots [fullWgt, DrawMtxNum) reference the wEvlp envelope
			// matrices BY INDEX (mDrawMtxIndex, repeats allowed) — Mario has 86
			// envelope entries over 43 envelope matrices. The previous bulk
			// sequential ArrayConcat(count=wEvlpNum) left every slot past
			// fullWgt+wEvlpNum ZERO (garbage nrm palette -> black patches on skinned
			// draws). Use the indexed concat exactly like the joint path above.
			const u16 fullWgt = mModelData->getDrawFullWgtMtxNum();
#ifdef SMS_NATIVE_PLATFORM
			// SB_LOG=drwidx: one-shot dump of the envelope-range DRW1 tables
			// (flag + index per entry) — checks whether index-mapped envelope
			// fill reads past the wEvlp array (heap zeros -> black patches).
			SB_LOG_ONCE("drwidx", "model=%p drawMtxNum=%d fullWgt=%d wEvlp=%d",
			            (void*)this, (int)mModelData->getDrawMtxNum(), (int)fullWgt,
			            (int)mModelData->getWEvlpMtxNum());
			if (sb_log_enabled("drwidx")) {
				static const void* dumped[8];
				static int nd = 0;
				bool seen = false;
				for (int s = 0; s < nd; ++s) if (dumped[s] == (void*)this) seen = true;
				if (!seen && nd < 8 && mModelData->getDrawMtxNum() > 60) {
					dumped[nd++] = (void*)this;
					for (u16 i = 0; i < mModelData->getDrawMtxNum(); ++i)
						sb_logf("drwidx", "model=%p entry=%d flag=%d idx=%d", (void*)this, (int)i,
						        (int)mModelData->getDrawMtxFlag(i),
						        (int)mModelData->mDrawMtxData.mDrawMtxIndex[i]);
				}
			}
#endif
			J3DMTXConcatArrayIndexedSrc(
			    viewMtx, (const float(*)[3][4])getWeightAnmMtx(0),
			    mModelData->mDrawMtxData.mDrawMtxIndex + fullWgt,
			    getDrawMtxPtr() + fullWgt,
			    (u32)(mModelData->getDrawMtxNum() - fullWgt));
		}
	}

	calcNrmMtx();
	calcBBoard();
	calcBumpMtx();
	DCStoreRange(getDrawMtxPtr(), mModelData->getDrawMtxNum() * sizeof(Mtx));
	DCStoreRange(getNrmMtxPtr(), mModelData->getDrawMtxNum() * sizeof(Mtx33));
	prepareShapePackets();
}

void J3DModel::calcNrmMtx()
{
	// TODO: probably a fakematch, the references make 0 sense

#ifdef SMS_NATIVE_PLATFORM
	// SB_LOG=nrmmtx: report zero draw matrices per model (garbage-nrm suspect).
	{
		int zeroCnt = 0, firstZero = -1;
		for (u16 i = 0; i < mModelData->getDrawMtxNum(); i++) {
			const float* m = &mDrawMtxBuf[1][mCurrentViewNo][i][0][0];
			if (m[0] == 0.f && m[1] == 0.f && m[2] == 0.f && m[4] == 0.f && m[5] == 0.f && m[6] == 0.f) {
				++zeroCnt;
				if (firstZero < 0) firstZero = i;
			}
		}
		if (zeroCnt)
			SB_LOGC("nrmmtx", "model=%p drawMtxBuf=%p zeroDrawMtx=%d/%d first=%d fullWgt=%d wEvlp=%d joints=%d",
			             (void*)this, (void*)mDrawMtxBuf[1][mCurrentViewNo], zeroCnt,
			             (int)mModelData->getDrawMtxNum(), firstZero,
			             (int)mModelData->getDrawFullWgtMtxNum(), (int)mModelData->getWEvlpMtxNum(),
			             (int)mModelData->getJointNum());
	}
#endif
	for (u16 i = 0; i < mModelData->getDrawMtxNum(); i++) {
		if (mModelData->getDrawMtxFlag(i) == 0) {
			if (getScaleFlag(mModelData->getDrawMtxIndex(i)) == 1) {
				Mtx& drawMtx = mDrawMtxBuf[1][mCurrentViewNo][i];
				J3DPSMtx33CopyFrom34(drawMtx, mNrmMtxBuf[1][mCurrentViewNo][i]);
			} else {
				Mtx33& nrmMtx = mNrmMtxBuf[1][mCurrentViewNo][i];
				J3DPSCalcInverseTranspose(mDrawMtxBuf[1][mCurrentViewNo][i],
				                          nrmMtx);
			}
		} else {
			if (getEnvScaleFlag(mModelData->getDrawMtxIndex(i)) == 1) {
				Mtx& drawMtx = mDrawMtxBuf[1][mCurrentViewNo][i];
				J3DPSMtx33CopyFrom34(drawMtx, mNrmMtxBuf[1][mCurrentViewNo][i]);
			} else {
				Mtx33& nrmMtx = mNrmMtxBuf[1][mCurrentViewNo][i];
				J3DPSCalcInverseTranspose(mDrawMtxBuf[1][mCurrentViewNo][i],
				                          nrmMtx);
			}
		}
	}
}

void J3DModel::calcBumpMtx()
{
	if (mModelData->unk18 != 1)
		return;

	s32 nextBumpMtx = 0;
	for (s32 i = 0; i < mModelData->getMaterialNum(); i++) {
		J3DMaterial* pMaterial = getModelData()->getMaterialNodePointer(i);
		if (pMaterial->getNBTScale()->mbHasScale == 1) {

			pMaterial->getShape()->calcNBTScale(
			    *pMaterial->getNBTScale()->getScale(), getNrmMtxPtr(),
			    getBumpMtxPtr(nextBumpMtx));

			DCStoreRange(getBumpMtxPtr(nextBumpMtx),
			             mModelData->getDrawMtxNum() * sizeof(Mtx33));
			nextBumpMtx++;
		}
	}
}

void J3DModel::calcBBoard()
{
	// TP debug confirms that they were literally choosing at random
	// whether to call getModelData() or access mModelData directly,
	// so this might be real.

	if (!getModelData()->checkBBoardFlag())
		return;

	for (u16 i = 0; i < mModelData->getDrawMtxNum(); i++) {
		if (getModelData()->getDrawMtxFlag(i) != 0)
			continue;

		u16 index = getModelData()->getDrawMtxIndex(i);

		if (mModelData->getJointNodePointer(index)->getMtxType()
		    == J3DJntMtxType_BBoard) {
			MtxPtr drawMtx = getDrawMtx(i);

			f32 sx = std::sqrtf(drawMtx[0][0] * drawMtx[0][0]
			                    + drawMtx[1][0] * drawMtx[1][0]
			                    + drawMtx[2][0] * drawMtx[2][0]);
			f32 sy = std::sqrtf(drawMtx[0][1] * drawMtx[0][1]
			                    + drawMtx[1][1] * drawMtx[1][1]
			                    + drawMtx[2][1] * drawMtx[2][1]);
			f32 sz = std::sqrtf(drawMtx[0][2] * drawMtx[0][2]
			                    + drawMtx[1][2] * drawMtx[1][2]
			                    + drawMtx[2][2] * drawMtx[2][2]);

			drawMtx[0][0] = sx;
			drawMtx[0][1] = 0.0f;
			drawMtx[0][2] = 0.0f;

			drawMtx[1][0] = 0.0f;
			drawMtx[1][1] = sy;
			drawMtx[1][2] = 0.0f;

			drawMtx[2][0] = 0.0f;
			drawMtx[2][1] = 0.0f;
			drawMtx[2][2] = sz;

			Mtx33& nrmMtx = getNrmMtx(i);

			nrmMtx[0][0] = 1.0f / sx;
			nrmMtx[0][1] = 0.0f;
			nrmMtx[0][2] = 0.0f;

			nrmMtx[1][0] = 0.0f;
			nrmMtx[1][1] = 1.0f / sy;
			nrmMtx[1][2] = 0.0f;

			nrmMtx[2][0] = 0.0f;
			nrmMtx[2][1] = 0.0f;
			nrmMtx[2][2] = 1.0f / sz;
		} else if (mModelData->getJointNodePointer(index)->getMtxType()
		           == J3DJntMtxType_YBBoard) {
			MtxPtr drawMtx = getDrawMtx(i);

			f32 sx = std::sqrtf(drawMtx[0][0] * drawMtx[0][0]
			                    + drawMtx[1][0] * drawMtx[1][0]
			                    + drawMtx[2][0] * drawMtx[2][0]);
			f32 sy = std::sqrtf(drawMtx[0][1] * drawMtx[0][1]
			                    + drawMtx[1][1] * drawMtx[1][1]
			                    + drawMtx[2][1] * drawMtx[2][1]);
			f32 sz = std::sqrtf(drawMtx[0][2] * drawMtx[0][2]
			                    + drawMtx[1][2] * drawMtx[1][2]
			                    + drawMtx[2][2] * drawMtx[2][2]);

			Vec axisX, axisY, axisZ;

			axisX.x = 1.0f;
			axisX.y = 0.0f;
			axisX.z = 0.0f;

			axisY.x = drawMtx[0][1];
			axisY.y = drawMtx[1][1];
			axisY.z = drawMtx[2][1];

			VECCrossProduct(&axisX, &axisY, &axisZ);
			VECNormalize(&axisY, &axisY);
			VECNormalize(&axisZ, &axisZ);

			drawMtx[0][0] = axisX.x * sx;
			drawMtx[0][1] = axisY.x * sy;
			drawMtx[0][2] = axisZ.x * sz;

			drawMtx[1][0] = axisX.y * sx;
			drawMtx[1][1] = axisY.y * sy;
			drawMtx[1][2] = axisZ.y * sz;

			drawMtx[2][0] = axisX.z * sx;
			drawMtx[2][1] = axisY.z * sy;
			drawMtx[2][2] = axisZ.z * sz;

			J3DPSCalcInverseTranspose(drawMtx, getNrmMtx(i));
		}
	}
}

void J3DModel::prepareShapePackets()
{
	u16 shapeNum = mModelData->getShapeNum();

	for (u16 i = 0; i < shapeNum; i++) {
		J3DShapePacket* pkt = &mShapePackets[i];

		J3DShape* shape = mModelData->getShapeNodePointer(i);
		shape->setScaleFlagArray(mScaleFlagArr);

		if (checkFlag(0x4))
			shape->onFlag(0x4);
		else
			shape->offFlag(0x4);

		if (checkFlag(0x8) && !shape->checkFlag(0x10))
			shape->onFlag(0x8);
		else
			shape->offFlag(0x8);

		pkt->unk24 = mVertexBuffer->unk2C;
		pkt->unk28 = mVertexBuffer->unk30;
		pkt->unk2C = mVertexBuffer->unk34;
		pkt->setDrawMtx(mDrawMtxBuf[1]);
		pkt->setNrmMtx(mNrmMtxBuf[1]);
		pkt->setCurrentViewNoPtr(&mCurrentViewNo);
	}

	for (s32 i = 0; i < mModelData->getMaterialNum(); i++) {
		J3DMaterial* pMaterial = mModelData->getMaterialNodePointer(i);
		if (pMaterial->getTexGenBlock()->getNBTScale()->mbHasScale == 1) {
			u16 shapeIdx    = pMaterial->getShape()->getIndex();
			u32 bumpMtxOffs = pMaterial->getShape()->getBumpMtxOffset();
			mShapePackets[shapeIdx].setNrmMtx(mBumpMtxArr[1][bumpMtxOffs]);
		}
	}
}
