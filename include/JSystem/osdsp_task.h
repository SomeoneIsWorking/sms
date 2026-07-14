#ifndef JSYSTEM_OSDSP_TASK_H
#define JSYSTEM_OSDSP_TASK_H

#include <dolphin/types.h>

// see JSystem/dsptask.h — bufL/bufR are live host pointers, must not be
// truncated to u32 on LP64 (matches the dsptask.h declaration actually used).
void DsyncFrame2(u32, uintptr_t, uintptr_t);
BOOL Dsp_Running_Check();
void Dsp_Running_Start();

#endif
