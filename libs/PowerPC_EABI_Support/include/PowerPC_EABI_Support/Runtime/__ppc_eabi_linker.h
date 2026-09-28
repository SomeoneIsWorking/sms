#ifndef __PPC_EABI_LINKER
#define __PPC_EABI_LINKER

/* MWCC_SECTION is the MWCC section attribute, spelled so a compiler that is not
 * MWCC can still parse the declarations below. It is defined by
 * <dolphin/types.h>, the header that already keys the rest of the SDK's
 * compiler-specific spellings (AT_ADDRESS) off the toolchain; this library
 * already includes it (MetroTRK/trk.h, Runtime/__va_arg.c). */
#include "dolphin/types.h"

MWCC_SECTION(".init") extern char _stack_addr[];
MWCC_SECTION(".init") extern char _stack_end[];
MWCC_SECTION(".init") extern char _heap_addr[];
MWCC_SECTION(".init") extern char _heap_end[];
MWCC_SECTION(".init") extern const char _fextabindex_rom[];
MWCC_SECTION(".init") extern char _fextabindex[];
MWCC_SECTION(".init") extern char _eextabindex[];

MWCC_SECTION(".init") extern char _SDA_BASE_[];

MWCC_SECTION(".init") extern char _SDA2_BASE_[];

typedef struct __rom_copy_info {
	char* rom;
	char* addr;
	unsigned int size;
} __rom_copy_info;

MWCC_SECTION(".init") extern __rom_copy_info _rom_copy_info[];

typedef struct __bss_init_info {
	char* addr;
	unsigned int size;
} __bss_init_info;

MWCC_SECTION(".init") extern __bss_init_info _bss_init_info[];

typedef struct __eti_init_info {
	void* eti_start;
	void* eti_end;
	void* code_start;
	unsigned long code_size;
} __eti_init_info;

MWCC_SECTION(".init") extern __eti_init_info _eti_init_info[];
MWCC_SECTION(".init") extern const char _f_init_rom[];
MWCC_SECTION(".init") extern char _f_init[];
MWCC_SECTION(".init") extern char _e_init[];
MWCC_SECTION(".init") extern const char _f_text_rom[];
MWCC_SECTION(".init") extern char _f_text[];
MWCC_SECTION(".init") extern char _e_text[];
MWCC_SECTION(".init") extern const char _f_rodata_rom[];
MWCC_SECTION(".init") extern char _f_rodata[];
MWCC_SECTION(".init") extern char _e_rodata[];
MWCC_SECTION(".init") extern const char _fextab_rom[];
MWCC_SECTION(".init") extern char _fextab[];
MWCC_SECTION(".init") extern char _eextab[];
MWCC_SECTION(".init") extern const char _f_data_rom[];
MWCC_SECTION(".init") extern char _f_data[];
MWCC_SECTION(".init") extern char _e_data[];
MWCC_SECTION(".init") extern char _f_bss[];
MWCC_SECTION(".init") extern char _e_bss[];
MWCC_SECTION(".init") extern const char _f_sdata_rom[];
MWCC_SECTION(".init") extern char _f_sdata[];
MWCC_SECTION(".init") extern char _e_sdata[];
MWCC_SECTION(".init") extern char _f_sbss[];
MWCC_SECTION(".init") extern char _e_sbss[];
MWCC_SECTION(".init") extern const char _f_sdata2_rom[];
MWCC_SECTION(".init") extern char _f_sdata2[];
MWCC_SECTION(".init") extern char _e_sdata2[];
MWCC_SECTION(".init") extern char _f_sbss2[];
MWCC_SECTION(".init") extern char _e_sbss2[];
MWCC_SECTION(".init") extern const char _f_PPC_EMB_sdata0_rom[];
MWCC_SECTION(".init") extern char _f_PPC_EMB_sdata0[];
MWCC_SECTION(".init") extern char _e_PPC_EMB_sdata0[];
MWCC_SECTION(".init") extern char _f_PPC_EMB_sbss0[];
MWCC_SECTION(".init") extern char _e_PPC_EMB_sbss0[];

#endif // __PPC_EABI_LINKER
