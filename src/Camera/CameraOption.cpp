#include <Camera/CameraOption.hpp>
#include <Camera/Camera.hpp>
#include <Camera/CameraMapTool.hpp>
#include <Camera/CubeMapTool.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Camera/cameralib.hpp>
#include <Player/MarioAccess.hpp>
#include <JSystem/JMath.hpp>

static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

TCameraOption* gpCameraOption;
const char* cLoadCamName = "ロードカメラ";

void CPolarSubCamera::chaseOptionCamera_(f32 param_1)
{
	CLBChaseConstantSpecifyFrame<f32>(&mPosition.x, mCurrentTarget.mPosition.x,
	                                  param_1);
	CLBChaseConstantSpecifyFrame<f32>(&mPosition.y, mCurrentTarget.mPosition.y,
	                                  param_1);
	CLBChaseConstantSpecifyFrame<f32>(&mPosition.z, mCurrentTarget.mPosition.z,
	                                  param_1);
	CLBChaseConstantSpecifyFrame<f32>(&mTarget.x, mCurrentTarget.mTarget.x,
	                                  param_1);
	CLBChaseConstantSpecifyFrame<f32>(&mTarget.y, mCurrentTarget.mTarget.y,
	                                  param_1);
	CLBChaseConstantSpecifyFrame<f32>(&mTarget.z, mCurrentTarget.mTarget.z,
	                                  param_1);
}

void CPolarSubCamera::ctrlOptionCamera_()
{
	JGeometry::TVec3<f32> probe;

	if (gpCameraOption->mIntroChaseTimer > 0) {
		chaseOptionCamera_(gpCameraOption->mIntroChaseTimer);
		gpCameraOption->mIntroChaseTimer--;
	} else if (gpCameraOption->mLoadPanTimer > 0) {
		chaseOptionCamera_(gpCameraOption->mLoadPanTimer);
		gpCameraOption->mLoadPanTimer--;
	} else if (!(gpCameraOption->mFlags & 0x2)) {
		probe = *gpMarioPos;
		probe.y += 75.0f;
		int cubeNo = gpCubeCamera->getInCubeNo(probe);
		if (cubeNo >= 0) {
			TCubeCameraInfo* info
			    = (TCubeCameraInfo*)&(*gpCubeCamera->unk14)[cubeNo];
			TCameraMapTool* tool = info->unk38;
			if (tool != nullptr && tool != unk70) {
				gpCameraOption->mFlags ^= 0x1;
				unk70 = tool;
				unk70->calcPosAndAt(&mCurrentTarget.mPosition,
				                    &mCurrentTarget.mTarget);
				gpCameraOption->mCubePanTimer = gpCameraOption->mCubePanFrames;
			}
		}

		if (gpCameraOption->mCubePanTimer > 0) {
			chaseOptionCamera_(gpCameraOption->mCubePanTimer);
			gpCameraOption->mCubePanTimer--;
		} else if (gpCameraOption->mUpDownPanTimer > 0) {
			chaseOptionCamera_(gpCameraOption->mUpDownPanTimer);
			gpCameraOption->mUpDownPanTimer--;
		}
	}

	unk124.x = mPosition.x;
	unk124.y = mPosition.y;
	unk124.z = mPosition.z;
	unk148.x = mTarget.x;
	unk148.y = mTarget.y;
	unk148.z = mTarget.z;
	mFovy    = gpCameraOption->mFovy;

#ifdef SMS_NATIVE_PLATFORM
	{
		static int sCamDbg = -1;
		if (sCamDbg < 0) { const char* e = getenv("SB_CAM_DBG"); sCamDbg = (e && *e && *e != '0') ? 1 : 0; }
		static int sCalls = 0; sCalls++;
		if (sCamDbg && (sCalls <= 5 || sCalls % 100 == 0)) {
			fprintf(stderr, "[camopt] n=%d pos=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f) introTimer=%d loadTimer=%d cubeTimer=%d updownTimer=%d flags=0x%x fovy=%.1f destPos=(%.1f,%.1f,%.1f)\n",
			        sCalls, mPosition.x, mPosition.y, mPosition.z,
			        mTarget.x, mTarget.y, mTarget.z,
			        gpCameraOption->mIntroChaseTimer, gpCameraOption->mLoadPanTimer,
			        gpCameraOption->mCubePanTimer, gpCameraOption->mUpDownPanTimer,
			        gpCameraOption->mFlags, mFovy,
			        gpCameraOption->mDestPos->x, gpCameraOption->mDestPos->y, gpCameraOption->mDestPos->z);
		}
	}
#endif
}

TCameraOption::TCameraOption(JGeometry::TVec3<f32> param1,
                             JGeometry::TVec3<f32>* param2)
{
	mFlags          = 2;
	mFovy           = 40.0f;
	unk8            = 300;
	mIntroChaseTimer = 300;
	mLoadPanFrames  = 120;
	mLoadPanTimer   = 0;
	mCubePanFrames  = 80;
	mCubePanTimer   = 0;
	mUpDownPanFrames = 60;
	mUpDownPanTimer = 0;
	mTitlePos.set(0.0f, 0.0f, 0.0f);
	mLoadPos.set(0.0f, 0.0f, 0.0f);
	mUpPos.set(0.0f, 0.0f, 0.0f);
	mDestPos = param2;

	s16 v1 = CLBRoundf<s16>(DEG2SHORTANGLE(-73.0f));
	s16 v2 = CLBRoundf<s16>(DEG2SHORTANGLE(54.0f));
	CLBPolarToCross(param1, &mTitlePos, 1000.0f, v2, v1);
	param2->set(mTitlePos);

	// TODO: inline doesn't work?!
	TCameraMapTool* tool = (TCameraMapTool*)gpCamMapToolTable->searchF(
	    JDrama::TNameRef::calcKeyCode(cLoadCamName), cLoadCamName);

	if (tool != nullptr) {
		JGeometry::TVec3<f32> origin;
		tool->calcPosAndAt(&origin, &mLoadPos);
		s16 a = CLBRoundf<s16>(DEG2SHORTANGLE(tool->getYaw()));
		s16 b = CLBRoundf<s16>(DEG2SHORTANGLE(60.0f));
		CLBPolarToCross(origin, &mUpPos, 1000.0f, b, a);
	}
}

void TCameraOption::moveToLoadFromTitle()
{
	mDestPos->set(mLoadPos);
	mLoadPanTimer = mLoadPanFrames;
	mFlags &= ~0x2;
}

void TCameraOption::moveToTitleFromLoad() { }

void TCameraOption::moveToUp()
{
	mDestPos->set(mUpPos);
	mUpDownPanTimer = mUpDownPanFrames;
}

void TCameraOption::moveToDown()
{
	mDestPos->set(mLoadPos);
	mUpDownPanTimer = mUpDownPanFrames;
}
