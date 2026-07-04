#include <System/DrawSyncManager.hpp>
#include <dolphin/gx.h>

TDrawSyncManager* TDrawSyncManager::smInstance;

class TFifo {
public:
	TFifo(int capacity)
	    : mCapacity(capacity)
	    , mReadIdx(0)
	    , mWriteIdx(0)
	{
		mData = new void*[mCapacity + 1];
	}

	int getLoopIdx(u32 index)
	{
		if (index >= mCapacity + 1)
			index = 0;
		return index;
	}

	// fabricated
	int size()
	{
		if (mReadIdx <= mWriteIdx)
			return mWriteIdx - mReadIdx;
		else
			return mWriteIdx + mCapacity + 1 - mReadIdx;
	}

	// fabricated
	void advanceReadIdx() { mReadIdx = getLoopIdx(mReadIdx + 1); }

	// fabricated
	void* read() { return mData[getLoopIdx(mReadIdx + 1)]; }

	// fabricated
	void push(void* value)
	{
		mData[mWriteIdx] = value;
		mWriteIdx        = getLoopIdx(mWriteIdx + 1);
	}

private:
	/* 0x0 */ void** mData;
	/* 0x4 */ int mCapacity;
	/* 0x8 */ int mReadIdx;
	/* 0xC */ u32 mWriteIdx;
};

TDrawSyncManager* TDrawSyncManager::start(u32 param_1, u32 param_2, s32 param_3)
{
	if (smInstance == nullptr)
		smInstance = new TDrawSyncManager(param_1, param_2, param_3);
	return smInstance;
}

void TDrawSyncManager::end() { }

void TDrawSyncManager::drawSyncCallback(u16 param_1)
{
	if (smInstance != nullptr)
		smInstance->drawSyncCallbackSub(param_1);
}

void* TDrawSyncManager::threadFunc(void* param_1)
{
	TDrawSyncManager* self = (TDrawSyncManager*)param_1;
	for (;;) {
		void* msg;
		OSReceiveMessage(&self->mMessageQueue, &msg, 1);
		if ((size_t)msg >= 0x80000000) {
			self->mFifo->push(msg);

			u32 iVar1 = self->mFifo->size();

			if (iVar1 == 2)
				GXEnableBreakPt(msg);
		} else {
			if ((size_t)msg >= 0x10000)
				break;

			self->mFifo->advanceReadIdx();

			u32 iVar1 = self->mFifo->size();

			if (iVar1 != 0) {
				if (iVar1 == 1)
					GXDisableBreakPt();
				else if (iVar1 >= 2)
					GXEnableBreakPt(self->mFifo->read());
			}
		}
	}
	return 0;
}

TDrawSyncManager::TDrawSyncManager(u32 param_1, u32 param_2, s32 param_3)
    : mCallbacks(param_1)
{
	mFlags = 0;
	OSCreateThread(&mProcessingThread, &threadFunc, this,
	               new u8[0x1000] + 0x1000, 0x1000, param_3, 0);
	OSInitMessageQueue(&mMessageQueue, (OSMessage*)new u8[0x50], 0x14);
	mFifo = new TFifo(param_2);
	OSResumeThread(&mProcessingThread);
}

TDrawSyncManager::~TDrawSyncManager() { }

void TDrawSyncManager::setCallback(u32 param_1, u16 param_2, u16 param_3,
                                   TDrawSyncCallback* param_4)
{
	mCallbacks[param_1] = TDrawSyncTokenRange(param_2, param_3, param_4);
}

#ifdef SMS_NATIVE_PLATFORM
// On the native platform the renderer captures J3D shapes SYNCHRONOUSLY — there is no GP FIFO
// and no asynchronous GPU, so GXEnableBreakPt/GXDisableBreakPt are no-ops (native/platform/
// gx_fb_impl.cpp) and the whole CPU↔GPU breakpoint-pacing protocol this manager implements is
// vestigial. Feeding the threadFunc message queue / "fabricated" FIFO with real GX breakpoint
// traffic (which the corrected big-endian perform-list filters now generate) crashed it: its
// 32-bit token classification + FIFO indexing are LP64-incompatible (SIGSEGV at threadFunc+0xea,
// a near-null deref from the corrupted FIFO). sunbright already replaced this manager natively for
// the same reason (sms_drawsync_lossproof.cpp). Here we suppress the pacing message sends so the
// threadFunc stays idle; the registered drawSyncCallback dispatch (the real retire work) still
// runs. This is faithful to the platform, not a workaround for a logic bug.
#define SMS_NATIVE_DRAWSYNC_PACE_OFF 1
#endif

void TDrawSyncManager::drawSyncCallbackSub(u16 param_1)
{
	if (param_1 == 0) {
#ifndef SMS_NATIVE_DRAWSYNC_PACE_OFF
		if (!(mFlags & 2))
			OSSendMessage(&mMessageQueue, (void*)(size_t)param_1, 1);
#endif
		return;
	}

	for (TDrawSyncTokenRange* it = mCallbacks.begin(); it != mCallbacks.end();
	     ++it)
		if (it->mCallback != nullptr && it->mRangeStart <= param_1
		    && param_1 <= it->mRangeEnd) {
			it->mCallback->drawSyncCallback(param_1);
#ifndef SMS_NATIVE_DRAWSYNC_PACE_OFF
			if (!(mFlags & 2))
				OSSendMessage(&mMessageQueue, (void*)(size_t)param_1, 1);
#endif
			return;
		}
}

void TDrawSyncManager::pushBreakPoint()
{
	if (mFlags & 3)
		return;

	GXFlush();
#ifndef SMS_NATIVE_DRAWSYNC_PACE_OFF
	void* readPtr;
	void* writePtr;
	GXGetFifoPtrs(GXGetCPUFifo(), &readPtr, &writePtr);
	OSSendMessage(&mMessageQueue, writePtr, 1);
#endif
}
