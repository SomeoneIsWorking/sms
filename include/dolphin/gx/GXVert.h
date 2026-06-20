#ifndef _DOLPHIN_GX_GXVERT_H_
#define _DOLPHIN_GX_GXVERT_H_

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SMS_NATIVE_PLATFORM
// Native PC build: the GameCube write-gather pipe (MMIO at 0xCC008000) does not
// exist. The native renderer consumes the J3D object model directly and ignores the
// GX FIFO command stream, so every GXWGFifo write is dead output here — point it at a
// host scratch sink (a bit-bucket) so the inline writers don't fault on unmapped MMIO.
#include <stdint.h>
extern volatile unsigned char sb_gx_wgfifo_sink[32];
#define GXFIFO_ADDR ((uintptr_t)sb_gx_wgfifo_sink)
#else
#define GXFIFO_ADDR 0xCC008000
#endif

typedef union {
	u8 u8;
	u16 u16;
	u32 u32;
	u64 u64;
	s8 s8;
	s16 s16;
	s32 s32;
	s64 s64;
	f32 f32;
	f64 f64;
} PPCWGPipe;

#ifdef __MWERKS__
volatile PPCWGPipe GXWGFifo : GXFIFO_ADDR;
#else
#define GXWGFifo (*(volatile PPCWGPipe*)GXFIFO_ADDR)
#endif

#if DEBUG

// external functions

#define FUNC_1PARAM(name, T) void name##1##T(T x);
#define FUNC_2PARAM(name, T) void name##2##T(T x, T y);
#define FUNC_3PARAM(name, T) void name##3##T(T x, T y, T z);
#define FUNC_4PARAM(name, T) void name##4##T(T x, T y, T z, T w);
#define FUNC_INDEX8(name)    void name##1x8(u8 x);
#define FUNC_INDEX16(name)   void name##1x16(u16 x);

#else

// inline functions

#define FUNC_1PARAM(name, T)                                                   \
	static inline void name##1##T(T x) { GXWGFifo.T = x; }

#define FUNC_2PARAM(name, T)                                                   \
	static inline void name##2##T(T x, T y)                                    \
	{                                                                          \
		GXWGFifo.T = x;                                                        \
		GXWGFifo.T = y;                                                        \
	}

#define FUNC_3PARAM(name, T)                                                   \
	static inline void name##3##T(T x, T y, T z)                               \
	{                                                                          \
		GXWGFifo.T = x;                                                        \
		GXWGFifo.T = y;                                                        \
		GXWGFifo.T = z;                                                        \
	}

#define FUNC_4PARAM(name, T)                                                   \
	static inline void name##4##T(T x, T y, T z, T w)                          \
	{                                                                          \
		GXWGFifo.T = x;                                                        \
		GXWGFifo.T = y;                                                        \
		GXWGFifo.T = z;                                                        \
		GXWGFifo.T = w;                                                        \
	}

#define FUNC_INDEX8(name)                                                      \
	static inline void name##1x8(u8 x) { GXWGFifo.u8 = x; }

#define FUNC_INDEX16(name)                                                     \
	static inline void name##1x16(u16 x) { GXWGFifo.u16 = x; }

#endif

// GXCmd
FUNC_1PARAM(GXCmd, u8)
FUNC_1PARAM(GXCmd, u16)
FUNC_1PARAM(GXCmd, u32)

// GXParam
FUNC_1PARAM(GXParam, u8)
FUNC_1PARAM(GXParam, u16)
FUNC_1PARAM(GXParam, u32)
FUNC_1PARAM(GXParam, s8)
FUNC_1PARAM(GXParam, s16)
FUNC_1PARAM(GXParam, s32)
FUNC_1PARAM(GXParam, f32)
FUNC_3PARAM(GXParam, f32)
FUNC_4PARAM(GXParam, f32)

// GXPosition
FUNC_3PARAM(GXPosition, f32)
FUNC_3PARAM(GXPosition, u8)
FUNC_3PARAM(GXPosition, s8)
FUNC_3PARAM(GXPosition, u16)
FUNC_3PARAM(GXPosition, s16)
FUNC_2PARAM(GXPosition, f32)
FUNC_2PARAM(GXPosition, u8)
FUNC_2PARAM(GXPosition, s8)
FUNC_2PARAM(GXPosition, u16)
FUNC_2PARAM(GXPosition, s16)
FUNC_INDEX16(GXPosition)
FUNC_INDEX8(GXPosition)

// GXNormal
FUNC_3PARAM(GXNormal, f32)
FUNC_3PARAM(GXNormal, s16)
FUNC_3PARAM(GXNormal, s8)
FUNC_INDEX16(GXNormal)
FUNC_INDEX8(GXNormal)

// GXColor
FUNC_4PARAM(GXColor, u8)
FUNC_1PARAM(GXColor, u32)
FUNC_3PARAM(GXColor, u8)
FUNC_1PARAM(GXColor, u16)
FUNC_INDEX16(GXColor)
FUNC_INDEX8(GXColor)

// GXTexCoord
FUNC_2PARAM(GXTexCoord, f32)
FUNC_2PARAM(GXTexCoord, s16)
FUNC_2PARAM(GXTexCoord, u16)
FUNC_2PARAM(GXTexCoord, s8)
FUNC_2PARAM(GXTexCoord, u8)
FUNC_1PARAM(GXTexCoord, f32)
FUNC_1PARAM(GXTexCoord, s16)
FUNC_1PARAM(GXTexCoord, u16)
FUNC_1PARAM(GXTexCoord, s8)
FUNC_1PARAM(GXTexCoord, u8)
FUNC_INDEX16(GXTexCoord)
FUNC_INDEX8(GXTexCoord)

// GXMatrixIndex
FUNC_1PARAM(GXMatrixIndex, u8)

#undef FUNC_1PARAM
#undef FUNC_2PARAM
#undef FUNC_3PARAM
#undef FUNC_4PARAM
#undef FUNC_INDEX8
#undef FUNC_INDEX16

#ifdef __cplusplus
}
#endif

#endif
