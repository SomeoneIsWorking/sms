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
#else
typedef u32 ARQRequestRef;
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

#define ARQ_DMA_ALIGNMENT 32

#define ARAM_DIR_MRAM_TO_ARAM 0x00
#define ARAM_DIR_ARAM_TO_MRAM 0x01

#define ARStartDMARead(mmem, aram, len)                                        \
	ARStartDMA(ARAM_DIR_ARAM_TO_MRAM, mmem, aram, len)
#define ARStartDMAWrite(mmem, aram, len)                                       \
	ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, mmem, aram, len)

typedef struct ARQRequest ARQRequest;

#define ARQ_TYPE_MRAM_TO_ARAM ARAM_DIR_MRAM_TO_ARAM
#define ARQ_TYPE_ARAM_TO_MRAM ARAM_DIR_ARAM_TO_MRAM

#define ARQ_PRIORITY_LOW  0
#define ARQ_PRIORITY_HIGH 1

#ifdef __cplusplus
extern "C" {
#endif

// ar.c
ARQCallback ARRegisterDMACallback(ARQCallback callback);
u32 ARGetDMAStatus(void);
void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);
u32 ARAlloc(u32 length);
u32 ARFree(u32* length);
int ARCheckInit(void);
u32 ARInit(u32* stack_index_addr, u32 num_entries);
void ARReset(void);
void ARSetSize(void);
u32 ARGetBaseAddress(void);
u32 ARGetSize(void);

// arq.c
void ARQInit(void);
void ARQReset(void);
void ARQPostRequest(struct ARQRequest* request, u32 owner, u32 type,
                    u32 priority, u32 source, u32 dest, u32 length,
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
