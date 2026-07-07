#include <JSystem/JKernel/JKRSolidHeap.hpp>
#include <JSystem/JUtility/JUTConsole.hpp>
#include <macros.h>
#ifdef SMS_NATIVE_PLATFORM
#include <dolphin/os.h>
#include <cstdlib>
#include <cstddef>
#include <execinfo.h>
extern "C" void* sb_jkr_host_alloc(size_t, int);  // host-memory overflow (JKRHeap.cpp)
extern "C" bool  sb_host_free_if_tagged(void*);   // truly free a host-overflow ptr (JKRHeap.cpp)
#endif

JKRSolidHeap* JKRSolidHeap::create(u32 size, JKRHeap* parent, bool errorFlag)
{
	if (!parent)
		parent = sRootHeap;

	const u32 expHeapSize = ALIGN_NEXT(sizeof(JKRSolidHeap), 0x10);
	u32 alignedSize       = ALIGN_PREV(size, 0x10);

	void* ptr     = JKRHeap::alloc(alignedSize, 0x10, parent);
	void* dataPtr = (char*)ptr + expHeapSize;
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_JKR_DBG"))
		OSReport("[jkr] SolidHeap::create size=0x%x parent=%p -> ptr=%p dataPtr=%p "
		         "expHeapSize=0x%x alignedSize=0x%x\n",
		         size, parent, ptr, dataPtr, expHeapSize, alignedSize);
#endif
	if (ptr == nullptr)
		return nullptr;

	return new (ptr)
	    JKRSolidHeap(dataPtr, alignedSize - expHeapSize, parent, errorFlag);
}

JKRSolidHeap::JKRSolidHeap(void* data, u32 size, JKRHeap* parent,
                           bool errorFlag)
    : JKRHeap(data, size, parent, errorFlag)
{
	mFreeSize = mSize;
	mCurStart = mStart;
	mCurEnd   = mEnd;
	unk74     = nullptr;
}

JKRSolidHeap::~JKRSolidHeap() { dispose(); }

void* JKRSolidHeap::alloc(u32 size, int alignment)
{
	lock();
	if (size < 4)
		size = 4;

	void* ret;

	if (alignment >= 0) {
		ret = allocFromHead(size, alignment < 4 ? 4 : alignment);
	} else {
		ret = allocFromTail(size, -alignment < 4 ? 4 : -alignment);
	}

#ifdef SMS_NATIVE_PLATFORM
	// PC-native memory ownership: overflow to host memory when the GC-sized solid heap is
	// full, instead of returning null (which downstream code may deref). free() here is a
	// no-op for every block anyway (solid heaps free via freeAll/freeTail), so the host
	// pointer leaks like any other solid-heap block until the heap is reset.
	if (ret == nullptr) ret = sb_jkr_host_alloc(size, alignment);
#endif

	unlock();
	return ret;
}

void* JKRSolidHeap::allocFromHead(u32 size, int align)
{
	size = ALIGN_NEXT(size, align);

	void* ret = nullptr;

	char* alignedStart = (char*)ALIGN_NEXT((uintptr_t)mCurStart, align);
	u32 requiredSize   = (alignedStart - (char*)mCurStart) + size;
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_JKR_DBG") && (uintptr_t)alignedStart < 0x10000)
		OSReport("[jkr] SolidHeap %p allocFromHead DEGENERATE: mStart=%p mEnd=%p "
		         "mCurStart=%p mFreeSize=0x%x size=0x%x align=%d -> alignedStart=%p\n",
		         this, mStart, mEnd, mCurStart, mFreeSize, size, align, alignedStart);
#endif
	if (requiredSize <= mFreeSize) {
		mCurStart = (char*)mCurStart + requiredSize;
		mFreeSize -= requiredSize;
		ret = alignedStart;
	} else {
#ifdef SMS_NATIVE_PLATFORM
		// Diagnostic: name the heap + dump the exact shortfall so an LP64 heap-sizing
		// fix is precise (pointer arrays are 2x larger on the 64-bit host, so
		// 32-bit-sized SolidHeaps overflow — e.g. J3DDrawBuffer's J3DPacket*[size]).
		// NOT FATAL: JKRSolidHeap::alloc overflows to host memory on failure (and
		// sb_rarc_swap_to_host has its own heap fallback chain), so this line alone
		// never explains a crash — it flags an LP64 sizing gap worth fixing someday.
		OSReport("[jkr] SolidHeap %p full, overflowing to host: requiredSize=0x%x "
		         "(size=0x%x align=%d) mFreeSize=0x%x short=0x%x span mStart=%p mEnd=%p\n",
		         this, requiredSize, size, align, mFreeSize,
		         requiredSize - mFreeSize, mStart, mEnd);
		// SB_JKR_BT=1: one-shot caller backtrace for the per-frame leak hunt. The NPC-on scene
		// heap OOMs ~860k times on a fixed ~592 B (0x250) alloc per frame; this names the leaking
		// allocation site so the next session starts from the culprit, not a re-derivation.
		if (getenv("SB_JKR_BT")) {
			static int s_bt = 0;
			if (s_bt < 6) { ++s_bt;
				void* frames[24];
				int nf = backtrace(frames, 24);
				OSReport("[jkr-bt] OOM size=0x%x backtrace (%d frames):\n", size, nf);
				backtrace_symbols_fd(frames, nf, 2 /*stderr*/);
			}
		}
#endif
		JUTWarningConsole_f("allocFromHead: cannot alloc memory (0x%x byte).\n",
		                    requiredSize);
#ifndef SMS_NATIVE_PLATFORM
		// Native: do NOT invoke the error handler (it aborts). Return null so the caller
		// (JKRSolidHeap::alloc) overflows to host memory — PC owns memory, no fixed limit.
		if (mErrorFlag == true && mErrorHandler != nullptr) {
			(*mErrorHandler)(this, size, align);
		}
#endif
	}
	return ret;
}

void* JKRSolidHeap::allocFromTail(u32 size, int align)
{
	size = ALIGN_NEXT(size, align);

	void* ret = nullptr;

	char* alignedEnd = (char*)ALIGN_PREV((uintptr_t)mCurEnd - size, align);
	u32 requiredSize = (char*)mCurEnd - alignedEnd;
	if (requiredSize <= mFreeSize) {
		mCurEnd = (char*)mCurEnd - requiredSize;
		mFreeSize -= requiredSize;
		ret = alignedEnd;
	} else {
		JUTWarningConsole_f("allocFromTail: cannot alloc memory (0x%x byte).\n",
		                    requiredSize);
#ifndef SMS_NATIVE_PLATFORM
		if (mErrorFlag == true && mErrorHandler != nullptr) {
			(*mErrorHandler)(this, size, align);
		}
#endif
	}
	return ret;
}

void JKRSolidHeap::free(void* ptr)
{
#ifdef SMS_NATIVE_PLATFORM
	// SolidHeap-overflow allocations went to host memory (sb_jkr_host_alloc) and are tagged;
	// truly free them (a real SolidHeap genuinely cannot free individual blocks). See JKRHeap.cpp.
	if (sb_host_free_if_tagged(ptr)) return;
#endif
	JUTWarningConsole_f("free: cannot free memory block (%08x)\n", ptr);
}

void JKRSolidHeap::freeAll()
{
	lock();
	JKRHeap::freeAll();
	mFreeSize = mSize;
	mCurStart = mStart;
	mCurEnd   = mEnd;
	unk74     = nullptr;
	unlock();
}

void JKRSolidHeap::freeTail()
{
	lock();
	if (mCurEnd != mEnd)
		dispose(mCurEnd, mEnd);
	mFreeSize += (u8*)mEnd - (u8*)mCurEnd;
	mCurEnd = mEnd;
	// more stuff, unk74 has size 18
	for (UnknownStruct* s = unk74; s != nullptr; s = s->unk10) {
		s->unkC = mEnd;
	}
	unlock();
}

s32 JKRSolidHeap::resize(void* ptr, u32 size)
{
	JUTWarningConsole_f("resize: cannot resize memory block (%08x: %d)\n", ptr,
	                    size);
	return -1;
}

s32 JKRSolidHeap::getSize(void* ptr)
{
	JUTWarningConsole_f("getSize: cannot get memory block size (%08x)\n", ptr);
	return -1;
}

bool JKRSolidHeap::check()
{
	lock();

	u32 checkedFreeSize = mFreeSize + ((u8*)mCurStart - (u8*)mStart)
	                      + ((u8*)mEnd - (u8*)mCurEnd);
	bool valid = true;
	if (checkedFreeSize != mSize) {
		valid = false;
		JUTWarningConsole_f("check: bad total memory block size (%08X, %08X)\n",
		                    mSize, checkedFreeSize);
	}

	unlock();
	return valid;
}

bool JKRSolidHeap::dump()
{
	JUTReportConsole("\nJKRSolidHeap dump\n");
	bool ret = check();
	lock();
	u32 checkedFreeSize = mFreeSize + ((u8*)mCurStart - (u8*)mStart)
	                      + ((u8*)mEnd - (u8*)mCurEnd);
	JUTReportConsole("attr  address:   size\n");
	JUTReportConsole_f("head %08x: %08x\n", mStart,
	                   (u8*)mCurStart - (u8*)mStart);
	JUTReportConsole_f("tail %08x: %08x\n", mCurEnd, (u8*)mEnd - (u8*)mCurEnd);

	float pcnt = 100.0f * ((float)checkedFreeSize / (float)mSize);
	JUTReportConsole_f("%d / %d bytes (%6.2f%%) used\n", checkedFreeSize, mSize,
	                   pcnt);
	unlock();
	return ret;
}

void JKRSolidHeap::state_register(TState* state, u32 param_1) const
{
	setState_u32ID_(state, param_1);
	setState_uUsedSize_(state, getUsedSize_((JKRSolidHeap*)this));
	// impossible to properly figure out unless new debug
	// builds of jsystem games are discovered
	// + it really doesn't matter
	char trash[0x10];
	u32 checkCode = (u32)(uintptr_t)mCurStart;
	checkCode += (u32)((uintptr_t)mCurEnd * 3);
	setState_u32CheckCode_(state, checkCode);
}

bool JKRSolidHeap::state_compare(const TState& fst, const TState& snd) const
{
	bool result = true;
	if (fst.getCheckCode() != snd.getCheckCode()) {
		result = false;
	}
	if (fst.getUsedSize() != snd.getUsedSize()) {
		result = false;
	}
	return result;
}
