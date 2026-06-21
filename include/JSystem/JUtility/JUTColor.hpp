#ifndef JUT_COLOR_HPP
#define JUT_COLOR_HPP

#include "dolphin/gx/GXStruct.h"

namespace JUtility {

struct TColor : public GXColor {
	TColor(u8 r, u8 g, u8 b, u8 a) { set(r, g, b, a); }
	TColor() { set(0xffffffff); }
	TColor(u32 u32Color) { set(u32Color); }
	TColor(GXColor color) { set(color); }

	// TColor(const TColor& other) { *(GXColor*)this = *(GXColor*)&other; }

	operator u32() const { return toUInt32(); }
	// Byte-explicit RGBA (0xRRGGBBAA) — the GameCube order GX hardware (and source
	// literals) expect. The original `*(u32*)&r` is an endian-dependent type-pun: it
	// yields 0xRRGGBBAA on the big-endian GC but 0xAABBGGRR on a little-endian PC,
	// scrambling every colour on the native build (e.g. a black fade renders red).
	// This is portable and matches the GC result on both ends. set(u32) is its inverse.
	u32 toUInt32() const {
		return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a;
	}

	void set(u8 cR, u8 cG, u8 cB, u8 cA)
	{
		r = cR;
		g = cG;
		b = cB;
		a = cA;
	}

	void set(u32 u32Color) {   // inverse of toUInt32 (byte-explicit 0xRRGGBBAA)
		r = (u8)(u32Color >> 24); g = (u8)(u32Color >> 16);
		b = (u8)(u32Color >> 8);  a = (u8)u32Color;
	}
	void set(GXColor gxColor) { *(GXColor*)&r = gxColor; }
	GXColor get() const { return *this; }
};

} // namespace JUtility

#endif
