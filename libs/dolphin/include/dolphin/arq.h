#ifndef _DOLPHIN_ARQ_H_
#define _DOLPHIN_ARQ_H_

#include <dolphin/ar.h>

#define ARQ_DMA_ALIGNMENT 32

#define ARQ_TYPE_MRAM_TO_ARAM ARAM_DIR_MRAM_TO_ARAM
#define ARQ_TYPE_ARAM_TO_MRAM ARAM_DIR_ARAM_TO_MRAM

#define ARQ_PRIORITY_LOW  0
#define ARQ_PRIORITY_HIGH 1

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

struct ARQRequest {
	/* 0x00 */ struct ARQRequest* next;
	/* 0x04 */ u32 owner;
	/* 0x08 */ u32 type;
	/* 0x0C */ u32 priority;
	/* 0x10 */ u32 source;
	/* 0x14 */ u32 dest;
	/* 0x18 */ u32 length;
	/* 0x1C */ ARQCallback callback;
};

typedef struct ARQRequest ARQRequest;

#ifdef __cplusplus
extern "C" {
#endif

void ARQInit(void);
void ARQReset(void);
void ARQPostRequest(struct ARQRequest* request, u32 owner, u32 type,
                    u32 priority, ARMemAddr source, ARMemAddr dest, u32 length,
                    ARQCallback callback);
void ARQRemoveRequest(struct ARQRequest* request);
void ARQRemoveOwnerRequest(u32 owner);
void ARQFlushQueue(void);
void ARQSetChunkSize(u32 size);
u32 ARQGetChunkSize(void);

#ifdef __cplusplus
}
#endif

#endif
