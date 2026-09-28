#include "dolphin/__ppc_eabi_init.h"
#include "dolphin/base/PPCArch.h"

#ifdef __cplusplus
extern "C" {
#endif

void __OSPSInit();
void __OSCacheInit();

/* The two functions below are MWCC `asm` functions: the whole definition is
 * Gekko assembly that MWCC emits into the .init section, and no other compiler
 * in this tree can parse the construct, let alone the instructions. GEKKO is
 * defined for this object by the decomp's own build (configure.py, cflags_base
 * -> cflags_dolphin), and it is the key dolphin/os.h already uses to select its
 * MWCC-only inline assembly. Everything else in this file is ordinary C++ and
 * is compiled either way. */
#ifdef GEKKO
asm void __init_hardware(void)
{
#ifdef __MWERKS__ // clang-format off
	nofralloc
	mfmsr r0
	ori r0, r0, 0x2000
	mtmsr r0

	mflr r31
	bl __OSPSInit
	bl __OSCacheInit
	mtlr r31
	blr
#endif // clang-format on
}

asm void __flush_cache(register void* address, register unsigned int size)
{
#ifdef __MWERKS__ // clang-format off
	nofralloc
	lis r5,	~0
	ori r5, r5, ~14
	and r5, r5, r3
	subf r3, r5, r3
	add r4, r4, r3

loop:
	dcbst r0, r5
	sync
	icbi r0, r5
	addic r5, r5, 8
	subic. r4, r4, 8
	bge loop
	isync
	blr
#endif // clang-format on
}
#endif // GEKKO

static void __init_cpp();

void __init_user() { __init_cpp(); }

typedef void (*voidfunctionptr)(); // pointer to function returning void
MWCC_SECTION(".init") extern voidfunctionptr _ctors[];
MWCC_SECTION(".init") extern voidfunctionptr _dtors[];

static void __init_cpp()
{
	voidfunctionptr* constructor;
	for (constructor = _ctors; *constructor; constructor++) {
		(*constructor)();
	}
}

void _ExitProcess(void) { PPCHalt(); }

#ifdef __cplusplus
}
#endif
