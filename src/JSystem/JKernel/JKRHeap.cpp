#include <JSystem/JKernel/JKRHeap.hpp>
#include <JSystem/JUtility/JUTAssert.hpp>
#include "dolphin/os.h"

#ifdef SMS_NATIVE_PLATFORM
// Defined in the SMS_NATIVE block below; truly free()s a host-overflow (tagged) pointer.
extern "C" bool sb_host_free_if_tagged(void*);
#endif

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
#ifdef SMS_NATIVE_PLATFORM
	if (sb_host_free_if_tagged(memory)) return;
#endif
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
#include <cstring>
#include <atomic>
#include <sys/mman.h>
#include <unistd.h>
// SB_HEAP_GUARD=1: electric-fence-style guard-page mode for host allocations. Each host
// block is mmap'd with its USER data flush against a trailing PROT_NONE guard page, so any
// write past the requested size faults IMMEDIATELY at the offending instruction (clean
// backtrace) instead of silently corrupting a neighbouring glibc-heap object (e.g. the
// restimg_swap idempotency set). Diagnostic only — off by default; costs a guard page per
// host allocation. Read once (getenv is slow, called on the alloc hot path).
static bool sb_heap_guard() {
	static int g = -1;
	if (g < 0) { const char* e = getenv("SB_HEAP_GUARD"); g = (e && e[0] && e[0] != '0') ? 1 : 0; }
	return g != 0;
}
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

// Game-thread identity gate (thread-local, default FALSE).
//
// The JKR heap is NOT thread-safe; the engine's cooperative single-baton scheduler
// (os_impl.cpp) guarantees only ONE game thread touches it at a time. But once a real
// GL driver (Mesa/radeonsi) is in the process — the GX->raylib switch (gx_raylib.cpp) —
// it spawns its OWN background worker threads (LLVM shader compile) that run CONCURRENTLY
// and allocate C++ objects through the GLOBAL operator new. Routing those onto the JKR
// heap corrupted it (radeonsi SIGSEGV in LLVMCreateTargetMachine) AND leaked into the
// solid heap until "SolidHeap OUT OF MEMORY". A FOREIGN thread has no business on the
// game heap, so a plain `new` from a thread we did NOT  as a game thread goes
// straight to host malloc. Game threads are marked at their entry point: the main fiber
// (boot_heap_bringup, before any game alloc) and every OSCreateThread carrier
// (os_impl thread_trampoline). Synchronous driver allocs that land on a game thread
// DURING a GL call are handled separately by the host-alloc gate above (raised around
// the raylib entry points), so Mesa/LLVM/raylib never reach the JKR heap.
static thread_local bool g_sb_is_game_thread = false;
extern "C" void sb_mark_game_thread(void) { g_sb_is_game_thread = true; }
// Clear the game-thread flag for the calling host thread. The process-main thread is marked
// by the static-init heap bringup, but once the game runs on its own thread, process-main only
// drives the SDL present loop and must NOT touch the non-thread-safe JKR heap — its `new` has to
// fall back to host malloc. (boot.cpp unmarks process-main right after spawning the game thread.)
extern "C" void sb_unmark_game_thread(void) { g_sb_is_game_thread = false; }
extern "C" int  sb_is_game_thread(void)   { return g_sb_is_game_thread ? 1 : 0; }

// --- Host-malloc overflow accounting + hard cap -------------------------------------------
// Every host fallback used to LEAK: operator delete -> JKRHeap::free no-ops a pointer that
// is outside every JKR arena, so it was never freed. That is fine for one-time bookkeeping,
// but the per-frame scene-capture (std::vectors under sb_host_alloc gate) and a heap-full
// game retry loop allocate continuously -> unbounded host growth -> Linux OOM. Two fixes:
//   (1) TAG each host allocation with a header so JKRHeap::free can recognise and truly
//       free() it (sb_host_free_if_tagged). The per-frame leak then disappears.
//   (2) A hard cap on OUTSTANDING tagged bytes; exceeding it FAILS FAST (OSPanic) naming the
//       cap + the live total, instead of silently driving the host to OOM.
// SB_HOST_ALLOC_CAP_MB overrides the cap (default 2048 MB). The cap bounds how much memory
// the game (incl. JKR-overflow + capture) can hold at once on the host.
static const uint64_t SB_HOST_TAG_MAGIC  = 0x53424845415021ULL; // "SBHEAP!"
static const uint64_t SB_HOST_GUARD_MAGIC = 0x53424847554152ULL; // "SBHGUAR" (guard-page block)
struct SbHostHdr { uint64_t magic; void* base; uint64_t bytes; uint64_t pad; };
static std::atomic<uint64_t> g_host_outstanding{0};

static uint64_t sb_host_cap_bytes()
{
	static uint64_t cap = 0;
	if (cap == 0) {
		uint64_t mb = 2048;
		if (const char* e = getenv("SB_HOST_ALLOC_CAP_MB")) {
			unsigned long v = strtoul(e, nullptr, 10);
			if (v > 0) mb = v;
		}
		cap = mb * 1024ull * 1024ull;
	}
	return cap;
}

static void* sb_host_malloc(size_t n, int alignment)
{
	size_t a = alignment <= 0 ? 16 : (size_t)alignment;
	if (a < 16) a = 16;
	if (n == 0) n = 1;

	// Enforce the cap on outstanding host bytes BEFORE allocating. Fail fast (the project's
	// hard rule) so a runaway allocation crashes here, naming the cause, not far downstream.
	const uint64_t total = (uint64_t)n + a + sizeof(SbHostHdr);
	uint64_t live = g_host_outstanding.load(std::memory_order_relaxed);
	if (live + total > sb_host_cap_bytes()) {
		OSPanic(__FILE__, __LINE__,
		        "[heap] HOST ALLOC CAP EXCEEDED: outstanding=%llu MB + req=%zu would pass cap=%llu MB "
		        "(raise SB_HOST_ALLOC_CAP_MB or fix the leak/runaway alloc)\n",
		        (unsigned long long)(live >> 20), n,
		        (unsigned long long)(sb_host_cap_bytes() >> 20));
		return nullptr;
	}

	if (sb_heap_guard()) {
		// Guard-page mode: [ payload pages (RW) ][ 1 guard page (PROT_NONE) ]. Place the
		// user data so it ENDS flush against the guard page; any store past user+n faults.
		const size_t page = (size_t)sysconf(_SC_PAGESIZE);
		const size_t need = sizeof(SbHostHdr) + n + a;      // header + data + alignment slack
		const size_t payload = ((need + page - 1) / page) * page;
		const size_t maplen  = payload + page;               // + guard page
		void* region = mmap(nullptr, maplen, PROT_READ | PROT_WRITE,
		                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (region == MAP_FAILED) {
			OSPanic(__FILE__, __LINE__, "[heap] guard mmap failed for %zu bytes\n", n);
			return nullptr;
		}
		mprotect((char*)region + payload, page, PROT_NONE); // trailing guard
		uintptr_t rw_end = (uintptr_t)region + payload;
		uintptr_t user = (rw_end - n) & ~(uintptr_t)(a - 1); // flush data end to guard, then align down
		// Shadow header immediately before user (guaranteed >= region: payload includes hdr+a
		// slack), so the SAME sb_host_free_if_tagged path finds it via (user - sizeof hdr).
		SbHostHdr* h = (SbHostHdr*)(user - sizeof(SbHostHdr));
		h->magic = SB_HOST_GUARD_MAGIC;
		h->base  = region;
		h->bytes = maplen;
		h->pad   = 0;
		g_host_outstanding.fetch_add(maplen, std::memory_order_relaxed);
		++g_native_malloc_fallbacks;
		return (void*)user;
	}

	// Over-allocate room for the header + alignment slack; place the user pointer at the
	// next `a`-aligned address past the header so (user - sizeof hdr) >= base.
	void* base = std::malloc((size_t)total);
	if (!base) {
		OSPanic(__FILE__, __LINE__, "[heap] host malloc returned NULL for %zu bytes\n", n);
		return nullptr;
	}
	uintptr_t raw  = (uintptr_t)base + sizeof(SbHostHdr);
	uintptr_t user = (raw + (a - 1)) & ~(uintptr_t)(a - 1);
	SbHostHdr* h = (SbHostHdr*)(user - sizeof(SbHostHdr));
	h->magic = SB_HOST_TAG_MAGIC;
	h->base  = base;
	h->bytes = total;
	h->pad   = 0;
	g_host_outstanding.fetch_add(total, std::memory_order_relaxed);

	uint64_t c = ++g_native_malloc_fallbacks;
	// Cache the debug-gate env once: getenv linear-scans environ (strncmp per var) and this is the
	// hot host-allocation path — calling it per alloc dominated the NPC-scene render (gdb showed
	// sb_host_malloc->getenv->strncmp as a top sample). Read it exactly once.
	static const bool jkr_dbg = getenv("SB_JKR_DBG") != nullptr;
	if (jkr_dbg && (c <= 8 || (c & (c - 1)) == 0))
		OSReport("[heap] host malloc fallback #%llu size=%zu align=%d outstanding=%llu MB "
		         "(JKR plain heap full/absent)\n",
		         (unsigned long long)c, n, alignment,
		         (unsigned long long)(g_host_outstanding.load() >> 20));
	return (void*)user;
}

// Detect + truly free a host-tagged pointer. Returns true if it owned `p`. Reading the 32
// bytes immediately before a non-tagged (JKR) pointer is safe — JKR blocks carry their own
// header there — and a 56-bit magic makes a false positive astronomically unlikely.
extern "C" bool sb_host_free_if_tagged(void* p)
{
	if (!p) return false;
	SbHostHdr* h = (SbHostHdr*)((uintptr_t)p - sizeof(SbHostHdr));
	if (h->magic == SB_HOST_GUARD_MAGIC) {
		void* base = h->base; uint64_t len = h->bytes;
		g_host_outstanding.fetch_sub(len, std::memory_order_relaxed);
		h->magic = 0;
		munmap(base, (size_t)len);
		return true;
	}
	if (h->magic != SB_HOST_TAG_MAGIC) return false;
	g_host_outstanding.fetch_sub(h->bytes, std::memory_order_relaxed);
	h->magic = 0;          // poison so a double free is not mistaken for a live tag
	std::free(h->base);
	return true;
}

// Exported so JKRExpHeap::alloc (and any other heap) can overflow to host memory instead
// of aborting (JKRDefaultMemoryErrorRoutine) when its fixed GC-sized arena is exhausted.
// PC-native: we own memory; a GameCube heap's fixed size is not a hard limit here. The
// returned pointer lies OUTSIDE every JKR heap's [mStart,mEnd], so JKRExpHeap::free no-ops
// it (bounded leak) — identical to the operator-new plain-fallback policy above.
extern "C" void* sb_jkr_host_alloc(size_t n, int alignment) { return sb_host_malloc(n, alignment); }
static inline void* sb_plain_new(size_t n, int alignment)
{
	// Host-isolation gate raised, OR the caller is NOT a registered game thread
	// (a Mesa/LLVM/driver worker spawned by the GL backend) -> straight to malloc,
	// skipping the non-thread-safe JKR heap entirely. Both keep host-runtime / driver
	// state off a heap the game wipes AND off a heap a foreign thread must never race.
	if (g_sb_host_alloc_gate || !g_sb_is_game_thread) return sb_host_malloc(n, alignment);
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
void operator delete(void* memory)
{
	if (sb_host_free_if_tagged(memory)) return;
	JKRHeap::free(memory, nullptr);
}
void operator delete[](void* memory)
{
	if (sb_host_free_if_tagged(memory)) return;
	JKRHeap::free(memory, nullptr);
}
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
