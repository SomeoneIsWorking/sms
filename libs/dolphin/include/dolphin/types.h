#ifndef _DOLPHIN_TYPES_H_
#define _DOLPHIN_TYPES_H_

typedef signed char s8;
typedef unsigned char u8;
typedef signed short int s16;
typedef unsigned short int u16;
typedef signed long s32;
typedef unsigned long u32;
typedef signed long long int s64;
typedef unsigned long long int u64;

typedef float f32;
typedef double f64;

typedef char* Ptr;

typedef int BOOL;

#define FALSE 0
#define TRUE  1

#if defined(__MWERKS__)
#define AT_ADDRESS(addr) : (addr)
#elif defined(__GNUC__)
// #define AT_ADDRESS(addr) __attribute__((address((addr))))
#define AT_ADDRESS(addr) // was removed in GCC. define in linker script instead.
#else
#error unknown compiler
#endif

/* MWCC places the PowerPC EABI runtime's linker symbols and entry points with
 * a section attribute, and binds the parameters of its `asm` functions to the
 * argument registers. Neither is C, and no compiler other than MWCC parses
 * them, so a translation unit that spells them literally cannot be parsed by
 * anything else.
 *
 * GEKKO is not a macro invented here: the decomp's own build defines it for
 * every middleware object (configure.py, cflags_base), and it is the key
 * dolphin/os.h already uses to select its MWCC-only inline assembly. Under
 * GEKKO these expand to exactly the tokens that were written before, so the
 * Gekko build sees byte-identical declarations. */
#ifdef GEKKO
#define MWCC_SECTION(s) __declspec(section s)
#define MWCC_REGISTER register
#else
#define MWCC_SECTION(s)
#define MWCC_REGISTER
#endif

#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
