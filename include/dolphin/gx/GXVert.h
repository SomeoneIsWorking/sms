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

#ifdef SMS_NATIVE_PLATFORM
// Native PC build: route the raw GX write-gather pipe through sb::gxfifo so the
// GX_ORACLE render sink can replay it through Dolphin's OpcodeDecoder — the SAME
// FIFO byte stream the game would send to real GC hardware. Under NATIVE_PC the
// helpers early-out on a global bool (sb::gxfifo::s_enabled == false), so there
// is no cost to sms-boot's SDL3 renderer.
extern void sb_gx_wgfifo_u8(unsigned char v);
extern void sb_gx_wgfifo_u16(unsigned short v);
extern void sb_gx_wgfifo_u32(unsigned int v);
extern void sb_gx_wgfifo_f32(float v);

// GXCmd — raw FIFO command bytes (opcode headers, count fields).
static inline void GXCmd1u8(u8 x)   { sb_gx_wgfifo_u8(x); }
static inline void GXCmd1u16(u16 x) { sb_gx_wgfifo_u16(x); }
static inline void GXCmd1u32(u32 x) { sb_gx_wgfifo_u32(x); }

// GXParam — parameter words for register writes.
static inline void GXParam1u8(u8 x)   { sb_gx_wgfifo_u8(x); }
static inline void GXParam1u16(u16 x) { sb_gx_wgfifo_u16(x); }
static inline void GXParam1u32(u32 x) { sb_gx_wgfifo_u32(x); }
static inline void GXParam1s8(s8 x)   { sb_gx_wgfifo_u8((u8)x); }
static inline void GXParam1s16(s16 x) { sb_gx_wgfifo_u16((u16)x); }
// J2DWindow::Texture::draw writes vertex DIRECT CLR0 with GXParam1s32(-1)
// (== 0xFFFFFFFF white) rather than GXColor1u32 — same 32-bit FIFO word. Route
// it to the colour-capture seam when inside a GXBegin; else raw FIFO.
extern void sb_gx_imm_param_color_s32(int v);
static inline void GXParam1s32(s32 x) { sb_gx_imm_param_color_s32(x); }
static inline void GXParam1f32(f32 x) { sb_gx_wgfifo_f32(x); }
static inline void GXParam3f32(f32 x, f32 y, f32 z) {
    sb_gx_wgfifo_f32(x); sb_gx_wgfifo_f32(y); sb_gx_wgfifo_f32(z);
}
static inline void GXParam4f32(f32 x, f32 y, f32 z, f32 w) {
    sb_gx_wgfifo_f32(x); sb_gx_wgfifo_f32(y); sb_gx_wgfifo_f32(z); sb_gx_wgfifo_f32(w);
}
#else
// GXCmd  (raw FIFO command/param words — internal DL building, kept on the sink)
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
#endif

#ifdef SMS_NATIVE_PLATFORM
// Native PC build: the immediate-mode VERTEX writers feed the gx_imm capture seam
// (gx_imm_impl.cpp) instead of the (nonexistent) write-gather FIFO. Position/Colour go
// to the renderer; Normal/TexCoord are captured-but-unused by the 2D path today (no-op
// so they don't poke the sink); INDEXED forms (array-referenced) and the raw Cmd/Param
// words above stay on the sink. See SLICE 2 of the renderer-attach handoff.
extern void sb_gx_imm_pos(float x, float y, float z);
extern void sb_gx_imm_color_rgba(unsigned r, unsigned g, unsigned b, unsigned a);
extern void sb_gx_imm_color_u32(unsigned c);

// GXPosition — value forms -> capture (2-component forms set z = 0).
static inline void GXPosition3f32(f32 x, f32 y, f32 z) { sb_gx_imm_pos(x, y, z); }
static inline void GXPosition3u8(u8 x, u8 y, u8 z)     { sb_gx_imm_pos(x, y, z); }
static inline void GXPosition3s8(s8 x, s8 y, s8 z)     { sb_gx_imm_pos(x, y, z); }
static inline void GXPosition3u16(u16 x, u16 y, u16 z) { sb_gx_imm_pos(x, y, z); }
static inline void GXPosition3s16(s16 x, s16 y, s16 z) { sb_gx_imm_pos(x, y, z); }
static inline void GXPosition2f32(f32 x, f32 y)        { sb_gx_imm_pos(x, y, 0); }
static inline void GXPosition2u8(u8 x, u8 y)           { sb_gx_imm_pos(x, y, 0); }
static inline void GXPosition2s8(s8 x, s8 y)           { sb_gx_imm_pos(x, y, 0); }
static inline void GXPosition2u16(u16 x, u16 y)        { sb_gx_imm_pos(x, y, 0); }
static inline void GXPosition2s16(s16 x, s16 y)        { sb_gx_imm_pos(x, y, 0); }
FUNC_INDEX16(GXPosition)
FUNC_INDEX8(GXPosition)

// GXNormal — captured-but-unused by the 2D path (no lighting on immediate verts yet).
static inline void GXNormal3f32(f32, f32, f32) {}
static inline void GXNormal3s16(s16, s16, s16) {}
static inline void GXNormal3s8(s8, s8, s8) {}
FUNC_INDEX16(GXNormal)
FUNC_INDEX8(GXNormal)

// GXColor — value forms -> capture.
static inline void GXColor4u8(u8 r, u8 g, u8 b, u8 a) { sb_gx_imm_color_rgba(r, g, b, a); }
static inline void GXColor1u32(u32 c)                 { sb_gx_imm_color_u32(c); }
static inline void GXColor3u8(u8 r, u8 g, u8 b)       { sb_gx_imm_color_rgba(r, g, b, 0xff); }
// 16-bit packed colour (RGBA4444) — expand each nibble to 8 bits.
static inline void GXColor1u16(u16 c) {
	sb_gx_imm_color_rgba(((c >> 12) & 0xf) * 0x11, ((c >> 8) & 0xf) * 0x11,
	                     ((c >> 4) & 0xf) * 0x11, (c & 0xf) * 0x11);
}
FUNC_INDEX16(GXColor)
FUNC_INDEX8(GXColor)

// GXTexCoord — captured into the gx_imm seam for textured 2D (J2D windows/pictures/text).
// Float forms arrive pre-normalized (0..1); integer forms push raw bits the seam dequants
// via the bound TEX0 vtx-attr fmt (imm_texcoord_scale). 1-component forms pair S then T.
extern void sb_gx_imm_texcoord_f2(float s, float t);
extern void sb_gx_imm_texcoord_f1(float v);
extern void sb_gx_imm_texcoord_i2(unsigned s, unsigned t);
extern void sb_gx_imm_texcoord_i1(unsigned v);
static inline void GXTexCoord2f32(f32 s, f32 t) { sb_gx_imm_texcoord_f2(s, t); }
static inline void GXTexCoord2s16(s16 s, s16 t) { sb_gx_imm_texcoord_i2((u16)s, (u16)t); }
static inline void GXTexCoord2u16(u16 s, u16 t) { sb_gx_imm_texcoord_i2(s, t); }
static inline void GXTexCoord2s8(s8 s, s8 t)    { sb_gx_imm_texcoord_i2((u8)s, (u8)t); }
static inline void GXTexCoord2u8(u8 s, u8 t)    { sb_gx_imm_texcoord_i2(s, t); }
static inline void GXTexCoord1f32(f32 v)        { sb_gx_imm_texcoord_f1(v); }
static inline void GXTexCoord1s16(s16 v)        { sb_gx_imm_texcoord_i1((u16)v); }
static inline void GXTexCoord1u16(u16 v)        { sb_gx_imm_texcoord_i1(v); }
static inline void GXTexCoord1s8(s8 v)          { sb_gx_imm_texcoord_i1((u8)v); }
static inline void GXTexCoord1u8(u8 v)          { sb_gx_imm_texcoord_i1(v); }
FUNC_INDEX16(GXTexCoord)
FUNC_INDEX8(GXTexCoord)

#else

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

#endif // SMS_NATIVE_PLATFORM

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
