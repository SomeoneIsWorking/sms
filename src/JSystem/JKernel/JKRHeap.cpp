#include <JSystem/JKernel/JKRHeap.hpp>
#include <JSystem/JUtility/JUTAssert.hpp>
#include "dolphin/os.h"

JKRHeap* JKRHeap::sSystemHeap;
JKRHeap* JKRHeap::sCurrentHeap;
JKRHeap* JKRHeap::sRootHeap;
JKRHeapErrorHandler* JKRHeap::mErrorHandler;
void* JKRHeap::mCodeStart;
void* JKRHeap::mCodeEnd;
void* JKRHeap::mUserRamStart;
void* JKRHeap::mUserRamEnd;
u32 JKRHeap::mMemorySize;

JKRHeap::JKRHeap(void* data, u32 size, JKRHeap* parent, bool errorFlag)
    : JKRDisposer()
    , mChildTree(this)
    , mDisposerList()
{
	OSInitMutex(&mMutex);
	mSize  = size;
	mStart = (u8*)data;
	mEnd   = ((u8*)data + size);
	if (parent == nullptr) {
		becomeSystemHeap();
		becomeCurrentHeap();
	} else {
		parent->mChildTree.appendChild(&mChildTree);
		if (sSystemHeap == sRootHeap)
			becomeSystemHeap();
		if (sCurrentHeap == sRootHeap)
			becomeCurrentHeap();
	}
	mErrorFlag = errorFlag;
	if (mErrorFlag == true && mErrorHandler == nullptr)
		mErrorHandler = JKRDefaultMemoryErrorRoutine;
}

JKRHeap::~JKRHeap()
{
	mChildTree.getParent()->removeChild(&mChildTree);
	JSUTree<JKRHeap>* nextRootHeap = sRootHeap->mChildTree.getFirstChild();
	if (sCurrentHeap == this)
		sCurrentHeap = !nextRootHeap ? sRootHeap : nextRootHeap->getObject();

	if (sSystemHeap == this)
		sSystemHeap = !nextRootHeap ? sRootHeap : nextRootHeap->getObject();
}

bool JKRHeap::initArena(char** outUserRamStart, u32* outUserRamSize,
                        int numHeaps)
{
	void* arenaLo = OSGetArenaLo();
	void* arenaHi = OSGetArenaHi();
	if (arenaLo == arenaHi) {
		return false;
	}
	void* arenaStart = OSInitAlloc(arenaLo, arenaHi, numHeaps);
	arenaHi          = (u8*)OSRoundDown32B(arenaHi);
	arenaLo          = (u8*)OSRoundUp32B(arenaStart);
	u8* start        = (u8*)OSPhysicalToCached(0);
	mCodeEnd         = (u8*)arenaLo;
	mCodeStart       = (u8*)start;
	mUserRamStart    = (u8*)arenaLo;
	mUserRamEnd      = (u8*)arenaHi;
#ifdef SMS_NATIVE_PLATFORM
	// Native PC build: there is no GC physical low-memory page, so the simulated
	// memory-size field at OSPhysicalToCached(0)+0x28 is unmapped (a deref would
	// SEGV). The host arena IS our memory, so report its usable size. mCodeStart/
	// mCodeEnd/mMemorySize are informational here (only written, never read by the
	// engine), so this preserves behaviour while owning the GC-hardware dependency.
	mMemorySize      = (u32)((uintptr_t)arenaHi - (uintptr_t)arenaLo);
#else
	mMemorySize      = *(u32*)((start + 0x28));
#endif
	OSReport("%x %x %x %x %x\n", mCodeStart, mCodeEnd, mUserRamStart,
	         mUserRamEnd, mMemorySize);
	OSSetArenaLo(arenaHi);
	OSSetArenaHi(arenaHi);
	*outUserRamStart = (char*)arenaLo;
	*outUserRamSize  = (u32)((uintptr_t)arenaHi - (uintptr_t)arenaLo);
	OSReport("arenaLo=%x arenaHi=%x \n", arenaLo, arenaHi);
	return true;
}

JKRHeap* JKRHeap::becomeSystemHeap()
{
	JKRHeap* old = sSystemHeap;
	sSystemHeap  = this;
	return old;
}

JKRHeap* JKRHeap::becomeCurrentHeap()
{
	JKRHeap* old = sCurrentHeap;
	sCurrentHeap = this;
	return old;
}

void* JKRHeap::alloc(u32 byteCount, int padding, JKRHeap* heap)
{
	void* memory = nullptr;
	if (heap) {
		memory = heap->alloc(byteCount, padding);
	} else if (sCurrentHeap) {
		memory = sCurrentHeap->alloc(byteCount, padding);
	}
	return memory;
}

void JKRHeap::free(void* memory, JKRHeap* heap)
{
	if (heap != nullptr || (heap = findFromRoot(memory)) != nullptr)
		heap->free(memory);
}

void JKRHeap::freeAll()
{
	JUT_WARNING_F(417, !mInitFlag, "freeAll in heap %x", this);
	JSUListIterator<JKRDisposer> iterator(&mDisposerList);
	while (iterator = mDisposerList.getFirst(),
	       iterator != mDisposerList.getEnd()) {
		iterator->~JKRDisposer();
	}
}

s32 JKRHeap::changeGroupID(u8 newGroupID) { return 0; }

u8 JKRHeap::getCurrentGroupId() { return 0; }

JKRHeap* JKRHeap::findFromRoot(void* ptr)
{
	if (sRootHeap != nullptr)
		return sRootHeap->find(ptr);

	return nullptr;
}

JKRHeap* JKRHeap::find(void* memory) const
{
	if ((mStart <= memory) && (memory <= mEnd)) {
		if (mChildTree.getNumChildren() != 0) {
			for (JSUTreeIterator<JKRHeap> iterator(mChildTree.getFirstChild());
			     iterator != mChildTree.getEndChild(); ++iterator) {
				JKRHeap* result = iterator->find(memory);
				if (result) {
					return result;
				}
			}
		}
		return const_cast<JKRHeap*>(this);
	}
	return nullptr;
}

void JKRHeap::dispose_subroutine(uintptr_t begin, uintptr_t end)
{
	JSUListIterator<JKRDisposer> last_iterator;
	JSUListIterator<JKRDisposer> next_iterator;
	JSUListIterator<JKRDisposer> iterator;
	for (iterator = mDisposerList.getFirst();
	     iterator != mDisposerList.getEnd(); iterator = next_iterator) {
		JKRDisposer* disposer = iterator.getObject();

		if ((void*)begin <= disposer && disposer < (void*)end) {
			iterator->~JKRDisposer();
			if (last_iterator
			    == JSUListIterator<JKRDisposer>(
			        (JSULink<JKRDisposer>*)nullptr)) {
				next_iterator = mDisposerList.getFirst();
				continue;
			}

			next_iterator = last_iterator;
			next_iterator++;
			continue;
		}

		last_iterator = iterator;
		next_iterator = iterator;
		next_iterator++;
	}
}

bool JKRHeap::dispose(void* memory, u32 size)
{
	uintptr_t begin = (uintptr_t)memory;
	uintptr_t end   = (uintptr_t)memory + size;
	dispose_subroutine(begin, end);
	return false;
}

void JKRHeap::dispose(void* begin, void* end)
{
	dispose_subroutine((uintptr_t)begin, (uintptr_t)end);
}

void JKRHeap::dispose()
{
	JSUListIterator<JKRDisposer> iterator(&mDisposerList);
	while (iterator = mDisposerList.getFirst(),
	       iterator != mDisposerList.getEnd()) {
		iterator->~JKRDisposer();
	}
}

void JKRHeap::copyMemory(void* dst, void* src, u32 size)
{
	u32 count = (size + 3) / 4;

	u32* dst_32 = (u32*)dst;
	u32* src_32 = (u32*)src;
	while (count-- > 0)
		*dst_32++ = *src_32++;
}

void JKRDefaultMemoryErrorRoutine(void* heap, u32 size, int alignment)
{
	OSErrorLine(694, "abort\n");
}

#ifdef SMS_NATIVE_PLATFORM
#include <cstdlib>
#include <cstdint>
// =============================================================================
// Native host-runtime isolation. The single global operator-new override forces
// EVERY native allocation into the JKR heap — not just game objects, but the
// HOST-RUNTIME ones too: std::thread state, std container nodes, the nvk
// renderer + glslang, os_impl's NativeThread. On hardware there is no such host
// runtime; here they drain the JKR root heap to 0, so a later allocation (game
// or host) returns null -> nondeterministic null-deref / std::thread terminate
// (seen: TApplication::mountStageArchive unk30==null, AudioThread::start abort).
//
// Policy: an EXPLICIT-heap placement (`new (heap, align)`) stays STRICT — it
// returns null on failure so a genuinely under-sized solid heap fails fast at
// the real cause (the LP64 sizing bug), never silently masked. A PLAIN `new`
// (heap == nullptr, which is what the host runtime AND general game allocations
// use via sCurrentHeap) falls back to malloc when the JKR heap is full/absent,
// so host bookkeeping is malloc-backed and isolated from the game heap. delete
// routes each pointer to its true owner by address range (findFromRoot), so the
// JKR and malloc pools never cross. Fallbacks are counted + logged (fail-loud).
// =============================================================================
static uint64_t g_native_malloc_fallbacks = 0;

// Host-allocation gate: while raised (thread-local), a PLAIN `new` is routed
// UNCONDITIONALLY to host malloc instead of the JKR heap. The os_impl thread layer
// raises it around std::thread construction so the thread's internal _State_impl
// (which libstdc++ allocates via the global operator new) is malloc-backed and
// therefore survives the game's JKRHeap freeAll()/destroy() on scene transitions —
// otherwise a thread carrier created before a transition is wiped and crashes the
// next time that thread is touched (seen: SIGSEGV in execute_native_thread_routine
// after mountStageArchive). Counted via the same fallback counter (fail-loud).
static thread_local int g_sb_host_alloc_gate = 0;
extern "C" void sb_host_alloc_push(void) { ++g_sb_host_alloc_gate; }
extern "C" void sb_host_alloc_pop(void)  { if (g_sb_host_alloc_gate > 0) --g_sb_host_alloc_gate; }

static void* sb_host_malloc(size_t n, int alignment)
{
	size_t a = alignment <= 0 ? 16 : (size_t)alignment;
	if (n == 0) n = 1;
	void* m;
	if (a <= 16) {
		m = std::malloc(n);
	} else {
		// posix_memalign needs a power-of-two >= sizeof(void*); JKR aligns are.
		if (posix_memalign(&m, a, n) != 0) m = nullptr;
	}
	uint64_t c = ++g_native_malloc_fallbacks;
	if (getenv("SB_JKR_DBG") && (c <= 8 || (c & (c - 1)) == 0))
		OSReport("[heap] host malloc fallback #%llu size=%zu align=%d "
		         "(JKR plain heap full/absent)\n",
		         (unsigned long long)c, n, alignment);
	return m;
}
static inline void* sb_plain_new(size_t n, int alignment)
{
	// Host-isolation gate raised -> straight to malloc (skip the JKR heap entirely),
	// so host-runtime state never lives on a heap the game later wipes.
	if (g_sb_host_alloc_gate) return sb_host_malloc(n, alignment);
	void* p = JKRHeap::alloc(n, alignment, nullptr);
	return p ? p : sb_host_malloc(n, alignment);
}

void* operator new(size_t byteCount) { return sb_plain_new(byteCount, 4); }
void* operator new(size_t byteCount, int alignment)
{
	return sb_plain_new(byteCount, alignment);
}
void* operator new(size_t byteCount, JKRHeap* heap, int alignment)
{
	// explicit heap -> strict (fail fast on a real solid-heap sizing bug)
	return JKRHeap::alloc(byteCount, alignment, heap);
}
void* operator new[](size_t byteCount) { return sb_plain_new(byteCount, 4); }
void* operator new[](size_t byteCount, int alignment)
{
	return sb_plain_new(byteCount, alignment);
}
void* operator new[](size_t byteCount, JKRHeap* heap, int alignment)
{
	return JKRHeap::alloc(byteCount, alignment, heap);
}

// delete keeps the decomp behavior: JKRHeap::free no-ops when findFromRoot does
// not own the pointer. A malloc-fallback pointer therefore LEAKS rather than
// risking a wrong libc free() — using findFromRoot to route the free is unsafe
// during heap bring-up, when a JKR pointer can be allocated before sRootHeap is
// wired (its owning heap is then not reachable from root, so findFromRoot==null
// and free() on a JKR pointer aborts with "invalid size"). The fallback only
// fires on JKR exhaustion (bounded host bookkeeping), so the leak is bounded;
// the SB_JKR_DBG counter surfaces the volume. A self-describing tagged free can
// replace this if the count proves large.
void operator delete(void* memory) { JKRHeap::free(memory, nullptr); }
void operator delete[](void* memory) { JKRHeap::free(memory, nullptr); }
#else
void* operator new(size_t byteCount)
{
	return JKRHeap::alloc(byteCount, 4, nullptr);
}
void* operator new(size_t byteCount, int alignment)
{
	return JKRHeap::alloc(byteCount, alignment, nullptr);
}
void* operator new(size_t byteCount, JKRHeap* heap, int alignment)
{
	return JKRHeap::alloc(byteCount, alignment, heap);
}

void* operator new[](size_t byteCount)
{
	return JKRHeap::alloc(byteCount, 4, nullptr);
}
void* operator new[](size_t byteCount, int alignment)
{
	return JKRHeap::alloc(byteCount, alignment, nullptr);
}
void* operator new[](size_t byteCount, JKRHeap* heap, int alignment)
{
	return JKRHeap::alloc(byteCount, alignment, heap);
}

// this is not needed without the other pragma and asm bs
void operator delete(void* memory) { JKRHeap::free(memory, nullptr); }
void operator delete[](void* memory) { JKRHeap::free(memory, nullptr); }
#endif

void JKRHeap::state_register(JKRHeap::TState* p, u32) const
{
#line 1132
	JUT_ASSERT(p != 0);
	JUT_ASSERT(p->getHeap() == this);
}

bool JKRHeap::state_compare(const JKRHeap::TState& r1,
                            const JKRHeap::TState& r2) const
{
#line 1141
	JUT_ASSERT(r1.getHeap() == r2.getHeap());
	return (r1.getCheckCode() == r2.getCheckCode());
}

void JKRHeap::state_dump(const TState& state) const
{
	JUT_LOG_F(1165, "check-code : 0x%08x", state.getCheckCode());
	JUT_LOG_F(1166, "id         : 0x%08x", state.getId());
	JUT_LOG_F(1167, "used size  : %u", state.getUsedSize());
}
