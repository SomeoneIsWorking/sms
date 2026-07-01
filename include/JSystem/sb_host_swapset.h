#ifndef JSYSTEM_SB_HOST_SWAPSET_H
#define JSYSTEM_SB_HOST_SWAPSET_H
#ifdef SMS_NATIVE_PLATFORM

// A malloc-backed std::set<const void*> for the native BE->host asset swappers'
// per-blob idempotency tracking.
//
// WHY NOT a plain `static std::set<...>`: on the game thread the global operator new
// routes plain allocations into the game's JKR arena (JKRHeap::alloc). The game wipes
// that arena wholesale on every scene transition (JKRHeap::freeAll/destroy). A swapper's
// idempotency set is a PROCESS-GLOBAL that outlives any single scene, so arena-backed
// tree nodes are freed under its feet -> dangling buckets -> the NEXT insert crashes in
// std::_Rb_tree::_M_insert_unique (observed intermittently at scene loads: TSpcBinary,
// JASWaveBankMgr, and the J2D 'TIMG' set). A std::malloc-backed allocator keeps the set's
// storage on the host heap, isolated from the game's heap lifecycle — the same policy
// JKRHeap.cpp applies to all host-runtime bookkeeping.
//
// NOTE: this only fixes the CRASH. A pointer key still can't tell a fresh big-endian blob
// (allocated at a freed blob's reused address) from an already-swapped one; where a
// content check is possible (e.g. scene.ral's node count), prefer that. For blobs with a
// stable magic re-checked after the insert, a stale hit merely re-runs the (idempotent)
// swap guard, so the pointer set is acceptable.

#include <cstdlib>
#include <new>
#include <set>

namespace smsport {

template <class T> struct HostMallocAlloc {
	using value_type = T;
	HostMallocAlloc() = default;
	template <class U> HostMallocAlloc(const HostMallocAlloc<U>&) noexcept {}
	T* allocate(std::size_t n) {
		void* p = std::malloc(n * sizeof(T));
		if (!p) throw std::bad_alloc();
		return static_cast<T*>(p);
	}
	void deallocate(T* p, std::size_t) noexcept { std::free(p); }
	template <class U> bool operator==(const HostMallocAlloc<U>&) const noexcept { return true; }
	template <class U> bool operator!=(const HostMallocAlloc<U>&) const noexcept { return false; }
};

using HostPtrSet = std::set<const void*, std::less<const void*>,
                            HostMallocAlloc<const void*>>;

} // namespace smsport

#endif // SMS_NATIVE_PLATFORM
#endif // JSYSTEM_SB_HOST_SWAPSET_H
