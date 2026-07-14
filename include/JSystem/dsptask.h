#ifndef JSYSTEM_DSPTASK_H
#define JSYSTEM_DSPTASK_H

#include <dolphin/types.h>

void DspBoot(void (*)(void*));
int DSPSendCommands2(u32*, u32, void (*)(u16));
int DspStartWork(u32, void (*)(u16));
void DspFinishWork(u16);
// bufL/bufR are LIVE HOST POINTERS to the DSPBuf triple-buffer (JASDSPBuf.cpp),
// not on-disk data — on real PowerPC hardware these are 32-bit register values
// (valid, since GC pointers are 32-bit); truncating a 64-bit LP64 heap pointer
// through a u32 here is the same class of bug as every other "LP64 file-overlay"
// narrowing fix in this codebase (see reference/sms shims/CLAUDE.md), except this
// is a live in-process pointer, not a serialized offset, so it must never be
// narrowed at all. uintptr_t widens correctly on both ILP32 (GC) and LP64 (host).
void DsyncFrame2(u32 subframes, uintptr_t bufL, uintptr_t bufR);

#endif
