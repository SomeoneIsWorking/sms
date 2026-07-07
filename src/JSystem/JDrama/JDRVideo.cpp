#include <JSystem/JDrama/JDRVideo.hpp>
#include <JSystem/JDrama/JDRRenderMode.hpp>
#include <dolphin/vi.h>
#include <dolphin/os.h>
#include <types.h>

using namespace JDrama;

#ifdef SMS_NATIVE_PLATFORM
// PC single-thread frame boundary — sms-boot/runtime/frame_seam.cpp.
extern "C" void sb_frame_present(unsigned retraces);
#endif

TVideo::TVideo()
{
	mCurFrameBuffer   = nullptr;
	mNextFrameBuffer  = nullptr;
	mLastRetraceTime  = OSGetTick();
	mNextRetraceIndex = VIGetRetraceCount() + 1;

	mNextRenderMode.viTVmode  = (VITVMode)-1;
	mNextRenderMode.fbWidth   = 0;
	mNextRenderMode.efbHeight = 0;
	mNextRenderMode.xfbHeight = 0;
	mNextRenderMode.viXOrigin = 0;
	mNextRenderMode.viYOrigin = 0;
	mNextRenderMode.viWidth   = 0;
	mNextRenderMode.viHeight  = 0;
	mNextRenderMode.xFBmode   = (VIXFBMode)-1;

	mCurRenderMode = mNextRenderMode;
}

void TVideo::setNextXFB(const void* fb) { mNextFrameBuffer = fb; }

void TVideo::waitForRetrace(u16 param_1)
{
#ifdef SMS_NATIVE_PLATFORM
	// PC single-thread frame boundary. This is the game's once-per-frame
	// scan-out point; sb_frame_present (sms-boot/runtime/frame_seam.cpp)
	// ends the Aurora frame (GX fifo drain + present), pumps events, begins
	// the next frame, and paces to param_1 NTSC fields of wall clock. The
	// VI dance below (XFB pointers, TV-mode settle) has no host meaning —
	// Aurora owns scan-out — but VIConfigure still forwards the render mode.
	if (!IsEqualRenderModeVIParams(mCurRenderMode, mNextRenderMode))
		VIConfigure(&mNextRenderMode);
	mCurRenderMode  = mNextRenderMode;
	mCurFrameBuffer = mNextFrameBuffer;
	sb_frame_present(param_1 ? param_1 : 1);
	mLastRetraceTime  = OSGetTick();
	mNextRetraceIndex = param_1 + VIGetRetraceCount();
	return;
#endif
	while (mNextRetraceIndex - (int)VIGetRetraceCount() > 1)
		VIWaitForRetrace();

	if (!IsEqualRenderModeVIParams(mCurRenderMode, mNextRenderMode)) {
		VIConfigure(&mNextRenderMode);
		if (mCurRenderMode.viTVmode != mNextRenderMode.viTVmode) {
			VISetBlack(1);
			mCurFrameBuffer = 0;
			VIFlush();
			VIWaitForRetrace();
			s32 uVar11 = mCurRenderMode.viTVmode & 3;
			if (((uVar11 == 2) && ((mNextRenderMode.viTVmode & 3) != 2))
			    || ((uVar11 != 2 && ((mNextRenderMode.viTVmode & 3) == 2)))) {

				for (int i = 0; i < 0x3C; ++i)
					VIWaitForRetrace();
			}
		}
	}

	if (mCurFrameBuffer != mNextFrameBuffer) {
		if (mNextFrameBuffer != nullptr) {
			VISetNextFrameBuffer((void*)mNextFrameBuffer);
			VISetBlack(0);
		} else {
			VISetBlack(1);
		}
	}

	mCurRenderMode  = mNextRenderMode;
	mCurFrameBuffer = mNextFrameBuffer;
	VIFlush();
	VIWaitForRetrace();
	mLastRetraceTime  = OSGetTick();
	mNextRetraceIndex = param_1 + VIGetRetraceCount();
}
