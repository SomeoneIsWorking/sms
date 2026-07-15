#include <MarioUtil/MtxUtil.hpp>

#include <cstdio>   // snprintf (glibc's <printf.h> below doesn't provide it)
#include <printf.h>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <Camera/Camera.hpp>
#include <dolphin/mtx.h>

void TMultiMtxEffect::setup(J3DModel* model, const char* prmLocation)
{
	mModel        = model;
	mMtxEffectTbl = new TMtxEffectBase*[mNumBones];

	for (u16 i = 0; i < mNumBones; ++i) {
		char* path = new char[0x40];
		snprintf(path, 0x40, "/%s/MtxEffect%d.prm", prmLocation, mBoneIDs[i]);

		switch (mMtxEffectType[i]) {
		// each case braced: C++ forbids a later case label jumping over a
		// case-scoped variable initialization (MWcc allowed it).
		case TMTX_EFFECT_TIME_LAG: {
			TMtxTimeLag* timeLag = new TMtxTimeLag(path);
			model->getModelData()
			    ->getJointNodePointer(mBoneIDs[i])
			    ->setCallBack(TMtxTimeLagCallBack);
			model->getModelData()
			    ->getJointNodePointer(mBoneIDs[i])
			    ->setCallBackUserData(timeLag);
			mMtxEffectTbl[i] = timeLag;
			break;
		}
		case TMTX_EFFECT_SWING_RZ: {
			TMtxSwingRZ* swingRz = new TMtxSwingRZ(path);
			model->getModelData()
			    ->getJointNodePointer(mBoneIDs[i])
			    ->setCallBack(TMtxSwingRZCallBack);
			model->getModelData()
			    ->getJointNodePointer(mBoneIDs[i])
			    ->setCallBackUserData(swingRz);
			mMtxEffectTbl[i] = swingRz;
			break;
		}
		case TMTX_EFFECT_SWING_RZ_REVERSE_XZ: {
			TMtxSwingRZ* swingRzReverse = new TMtxSwingRZ(path);
			model->getModelData()
			    ->getJointNodePointer(mBoneIDs[i])
			    ->setCallBack(TMtxSwingRZReverseXZCallBack);
			model->getModelData()
			    ->getJointNodePointer(mBoneIDs[i])
			    ->setCallBackUserData(swingRzReverse);
			mMtxEffectTbl[i] = swingRzReverse;
			break;
		}
		}
	}
	for (int i = 0; i < mNumBones; ++i)
		mMtxEffectTbl[i]->onFlag(2);
}

void TMultiMtxEffect::setUserArea()
{
	for (u16 i = 0; i < mNumBones; i++) {
		mModel->getModelData()
		    ->getJointNodePointer(mBoneIDs[i])
		    ->setCallBackUserData(mMtxEffectTbl[i]);
	}
}

// Cold RE of US 0x8022ba74 (empty in decomp, absent from funcs.txt; found via
// caller TShimmer::perform). Builds the projected-texgen "effect" matrix used by
// J3DTexMtx::setEffectMtx: a camera perspective projection with the depth (z)
// row replaced by {0,0,-1,0} so the effect samples in projected space.
//
// Native adaptation of a latent decomp bug: retail runs C_MTXPerspective (a 4x4
// write) directly into the caller's buffer and then also writes row 3 — but the
// callers declare a 3x4 `Mtx` (setEffectMtx consumes only 3x4), so retail writes
// 16 bytes past that buffer. Harmless on PPC (stack padding); on a native x86-64
// stack it corrupts adjacent locals (deterministic SIGSEGV in a downstream
// perform). We produce the SAME observable 3x4 (rows 0-1 from the projection,
// row 2 = the effect fixup) via a local Mtx44, and drop the vestigial, never-read
// row 3 — no overflow. See debug_journal/2026-07-15_sms_getlightperspective_re.md.
void SMS_GetLightPerspectiveForEffectMtx(MtxPtr m)
{
	Mtx44 proj;
	C_MTXPerspective(proj, gpCamera->getFovy(), gpCamera->getAspect(),
	                 gpCamera->getNear(), gpCamera->getFar());
	for (int c = 0; c < 4; ++c) {
		m[0][c] = proj[0][c];
		m[1][c] = proj[1][c];
	}
	m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = -1.0f; m[2][3] = 0.0f;
}
