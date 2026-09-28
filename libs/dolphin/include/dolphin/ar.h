#ifndef _DOLPHIN_AR_H_
#define _DOLPHIN_AR_H_

#include <dolphin/types.h>

// The ARQ completion callback receives the ARQRequest pointer as its argument. On
// GC that is a 32-bit value; on a 64-bit host a real main-memory pointer (e.g.
// JKRAMCommand's leading ARQRequest, which JKRAramPiece::doneDMA casts straight
// back to JKRAMCommand*) cannot round-trip through a u32 -> it truncates to a wild
// pointer. Widen the userdata to pointer width natively so the round-trip is
// lossless (the SDK-signature widening the inert-ARAM seam note prescribed).
#ifdef SMS_NATIVE_PLATFORM
#include <stdint.h>
typedef uintptr_t ARQRequestRef;
// A main-memory address handed to the ARAM DMA. On GC it is a 32-bit pointer; on a
// 64-bit host a real JKRHeap allocation cannot round-trip through a u32 -> it would
// truncate to a wild pointer and the ARAM<->mainRAM copy would read/write garbage
// (the inert-ARAM-seam note's "widen the SDK signatures" fix, made real now that the
// 2D/ARAM archive path genuinely DMAs through ARAM). The ARAM-side address stays a
// small offset (fits u32) but is carried in the same widened slot for symmetry.
typedef uintptr_t ARMemAddr;
#else
typedef u32 ARQRequestRef;
typedef u32 ARMemAddr;
#endif
typedef void (*ARQCallback)(ARQRequestRef pointerToARQRequest);

// ARQRequest itself, and every ARQ* entry point that takes or returns one, is
// declared in <dolphin/arq.h> (commit "Split ARQ declarations into
// dolphin/arq.h as in the SDK"). It used to be defined here as well, which is
// harmless on a case-insensitive filesystem and a redefinition of every field on
// a case-sensitive one, so this header keeps the forward declaration the SDK
// split leaves behind and nothing more.
struct ARQRequest;

#define ARQ_DMA_ALIGNMENT 32

#define ARAM_DIR_MRAM_TO_ARAM 0x00
#define ARAM_DIR_ARAM_TO_MRAM 0x01

#define ARStartDMARead(mmem, aram, len)                                        \
	ARStartDMA(ARAM_DIR_ARAM_TO_MRAM, mmem, aram, len)
#define ARStartDMAWrite(mmem, aram, len)                                       \
	ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, mmem, aram, len)

typedef struct ARQRequest ARQRequest;

// arq.c lives in <dolphin/arq.h> too, so the ARQ* constants and entry points
// are declared there alongside the request struct they operate on. They were
// duplicated here before that split.

#ifdef __cplusplus
extern "C" {
#endif

// ar.c
ARQCallback ARRegisterDMACallback(ARQCallback callback);
u32 ARGetDMAStatus(void);
void ARStartDMA(u32 type, ARMemAddr mainmem_addr, u32 aram_addr, u32 length);
u32 ARAlloc(u32 length);
u32 ARFree(u32* length);
int ARCheckInit(void);
u32 ARInit(u32* stack_index_addr, u32 num_entries);
void ARReset(void);
void ARSetSize(void);
u32 ARGetBaseAddress(void);
u32 ARGetSize(void);

#ifdef __cplusplus
}
#endif

#endif
