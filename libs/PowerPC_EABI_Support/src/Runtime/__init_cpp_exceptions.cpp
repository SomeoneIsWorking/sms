#include "PowerPC_EABI_Support/Runtime/NMWException.h"
#include "PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h"

static int fragmentID = -2;

/* GetR2 is an MWCC `asm` function: the whole definition is a Gekko instruction
 * that copies the TOC pointer register, which no other compiler in this tree
 * can parse and no other target has. GEKKO is defined for this object by the
 * decomp's own build (configure.py, cflags_base -> cflags_runtime), and it is
 * the key dolphin/os.h already uses to select its MWCC-only inline assembly.
 * __register_fragment below is the other half of the same mechanism -- MWCC's
 * C++ exception-table registration, which has no meaning off the target. */
#ifdef GEKKO
asm static char* GetR2()
{
#ifdef __MWERKS__ // clang-format off
	nofralloc
	mr r3, r2
	blr
#endif // clang-format on
}
#endif // GEKKO

#ifdef __cplusplus
extern "C" {
#endif

void __init_cpp_exceptions()
{
#ifdef GEKKO
	if (fragmentID == -2) {
		char* R2   = GetR2();
		fragmentID = __register_fragment(_eti_init_info, R2);
	}
#endif
}

void __fini_cpp_exceptions()
{
#ifdef GEKKO
	if (fragmentID != -2) {
		__unregister_fragment(fragmentID);
		fragmentID = -2;
	}
#endif
}

#ifdef __cplusplus
}
#endif

/* These three are the C runtime's .ctors/.dtors entries: a function's address
 * placed in a section the Gekko startup code walks. There is no such section or
 * runtime on a host, and MWCC's implicit function-to-void* conversion is not C,
 * so the definitions are target-only like the rest of this file. */
#ifdef GEKKO
MWCC_SECTION(".ctors") extern void* const __init_cpp_exceptions_reference
    = __init_cpp_exceptions;
MWCC_SECTION(".dtors") extern void* const __destroy_global_chain_reference
    = __destroy_global_chain;
MWCC_SECTION(".dtors") extern void* const __fini_cpp_exceptions_reference
    = __fini_cpp_exceptions;
#endif // GEKKO
