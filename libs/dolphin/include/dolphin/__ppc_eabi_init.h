/* MWCC_SECTION / MWCC_REGISTER are the MWCC spellings of the section attribute
 * and the register storage class, defined by <dolphin/types.h>. MWCC_REGISTER
 * is used only in this declaration: it is what binds __flush_cache's arguments
 * to r3/r4, which its asm body reads by name, and it has no effect on the
 * declaration itself. */
#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

MWCC_SECTION(".init") extern void __init_hardware(void);
MWCC_SECTION(".init") extern void
__flush_cache(MWCC_REGISTER void* address, MWCC_REGISTER unsigned int size);

#ifdef __cplusplus
}
#endif
