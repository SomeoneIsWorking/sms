#include <M3DUtil/M3UModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cmath>
#include <dolphin/os.h>
#endif

J3DMtxCalc* M3UModelCommon::getMtxCalc(const M3UMtxCalcSetInfo& param_1)
{
	// Same type as in J3DNewMtxCalcAnm
	// TODO: Extract to enum?
	switch (param_1.mAnmType) {
	case 0:
		return &unk10[param_1.mMtxCalcIdx];
	case 1:
		return &unk14[param_1.mMtxCalcIdx];
	}
	return nullptr;
}

void M3UModel::changeMtxCalcAnmTransform(int param_1, u8 param_2)
{
	M3UMtxCalcSetInfo& ptr = unk14[param_1];
	ptr.mAnmTransformIdx   = param_2;

	J3DFrameCtrl& ctrl = unkC[ptr.mFrameCalcIdx];
	ctrl.setEnd(unk4->unk4[param_2]->getFrameMax());
	ctrl.setFrame(0.0f);
}

void M3UModel::changeAnmTexPattern(int param_1, u8 param_2)
{
	Unk1CStruct& tmp = unk1C[param_1];
	tmp.unk0         = param_2;

	J3DFrameCtrl& ctrl = getFrameCtrl(tmp.unk1);
	ctrl.setEnd(unk4->unk8[param_2]->getFrameMax());
	ctrl.setFrame(0.0f);
}

void M3UModel::updateInMotion()
{
	// Unused stack
	// volatile u32 padding[12];
	for (int i = 0; i < unk10; i++) {
		M3UMtxCalcSetInfo& info   = unk14[i];
		J3DAnmTransform* anmTrans = unk4->unk4[info.mAnmTransformIdx];
		J3DFrameCtrl& frameCtrl   = getFrameCtrl(info.mFrameCalcIdx);
		frameCtrl.update();

		J3DJoint* jnt = unk8->mModelData->getJointNodePointer(info.mJntIdx);
		if (info.mMtxCalcIdx == 0xff) {
			jnt->setMtxCalc(nullptr);
			continue;
		}
		f32 currentFrame = frameCtrl.getFrame();
		anmTrans->setFrame(currentFrame);

		// Possibly inlined? Feels like it fits more in M3UModelCommon
		switch (info.mAnmType) {
		case 0:
			unk4->unk10[info.mMtxCalcIdx].mOne[0] = anmTrans;
			break;
		case 1:
			unk4->unk14[info.mMtxCalcIdx].mOne[0] = anmTrans;
			break;
		}

		jnt->setMtxCalc(unk4->getMtxCalc(unk14[i]));
	}
}

void M3UModel::updateInTexPatternAnm()
{
	if (unk1C)
		getFrameCtrl(unk1C->unk1).update();
}

void M3UModel::updateIn()
{
	updateInMotion();
	if (unk1C != nullptr) {
		getFrameCtrl(unk1C->unk1).update();
	}
}

void M3UModel::updateOut()
{
	for (int i = 0; i < unk10; i++) {
		M3UMtxCalcSetInfo& unk = unk14[i];
		unk8->mModelData->getJointNodePointer(unk.mJntIdx)->setMtxCalc(nullptr);
	}
}

void M3UModel::entryIn()
{
	if (unk1C != nullptr) {
		Unk1CStruct& tmp        = unk1C[0];
		J3DFrameCtrl& frameCtrl = getFrameCtrl(tmp.unk1);
		if (tmp.unk0 != 0xff) {
			J3DAnmTexPattern* pattern = unk4->unk8[tmp.unk0];
			pattern->setFrame(frameCtrl.getFrame());
			unk8->mModelData->setTexNoAnimator(pattern, unk4->unkC[tmp.unk0]);
		}
	}
}

void M3UModel::entryOut()
{
	if (unk1C != nullptr && unk1C->unk0 != 0xff)
		unk8->mModelData->removeTexNoAnimator(unk4->unk8[unk1C->unk0]);
}

void M3UModel::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	if (param_1 & 2) {
		updateIn();
		unk8->calc();
		updateOut();
#ifdef SMS_NATIVE_PLATFORM
		// FAIL FAST: a NaN joint matrix here is the root cause of every downstream
		// joint-skinned render failure (TMarioCap's "TOO BA" letters at file-select was
		// the first surface). The cap inherits a NaN base TR from getAnmMtx(headJoint),
		// its shapes project to garbage NDC. Crash here naming the joint + how much of
		// the matrix is NaN, so the root cause is read straight from the panic instead
		// of chasing it through a downstream render frame. See debug_journal/
		// 2026-06-24_mariocap_nan_skeleton_root_cause.md. Opt-out: SB_ALLOW_NAN_JOINTS=1.
		{
			static int allow = -1;
			if (allow < 0) { const char* e = ::getenv("SB_ALLOW_NAN_JOINTS");
				allow = (e && e[0] && e[0] != '0') ? 1 : 0; }
			if (!allow && unk8 && unk8->mModelData) {
				const u16 nj = unk8->mModelData->getJointNum();
				for (u16 j = 0; j < nj; ++j) {
					MtxPtr m = unk8->getAnmMtx(j);
					// Translation column carries the propagated NaN first; check the
					// whole 3x4 for completeness (a NaN-in/translate-out rotation also
					// produces NaN in r0..r2).
					int nan_t = (!std::isfinite(m[0][3]) || !std::isfinite(m[1][3])
					           || !std::isfinite(m[2][3])) ? 1 : 0;
					int nan_r = 0;
					for (int r = 0; r < 3 && !nan_r; ++r)
						for (int c = 0; c < 3; ++c)
							if (!std::isfinite(m[r][c])) { nan_r = 1; break; }
					if (nan_t || nan_r) {
						OSPanic(__FILE__, __LINE__,
						        "M3UModel::perform(2): NaN joint matrix at joint %u/%u "
						        "(nan_t=%d nan_r=%d) - anim/skeleton calc produced "
						        "invalid matrices. m=[r0(%f,%f,%f,%f) r1(%f,%f,%f,%f) "
						        "r2(%f,%f,%f,%f)] this=%p model=%p",
						        (unsigned)j, (unsigned)nj, nan_t, nan_r,
						        m[0][0],m[0][1],m[0][2],m[0][3],
						        m[1][0],m[1][1],m[1][2],m[1][3],
						        m[2][0],m[2][1],m[2][2],m[2][3],
						        (void*)this, (void*)unk8);
					}
				}
			}
		}
#endif
	}

	if (param_1 & 4) {
		unk8->viewCalc();
	}

	if (param_1 & 0x200) {
		entryIn();
		unk8->entry();
		entryOut();
	}
}
