#include <JSystem/JDrama/JDRDrawBufObj.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif

using namespace JDrama;

TDrawBufObj::TDrawBufObj()
    : TViewObj("<DrawBufObj>")
    , mDrawBuffer(nullptr)
    , mDrawBufferSize(0)
    , unk18(7)
{
}

TDrawBufObj::TDrawBufObj(u32 param_1, u32 param_2, const char* name)
    : TViewObj(name)
    , mDrawBufferSize(param_2)
    , unk18(param_1)
{
	mDrawBuffer = new J3DDrawBuffer(mDrawBufferSize);
}

void TDrawBufObj::load(JSUMemoryInputStream& stream)
{
	TNameRef::load(stream);
	// The on-disc stream is BIG-ENDIAN; these are u32 scalars, so use the byteswap-
	// aware readU32() (not the raw read(), which copies BE bytes into a host LE u32 ->
	// a garbage huge mDrawBufferSize -> J3DDrawBuffer over-allocates and overflows the
	// SolidHeap). On GC (non-native) readU32 is a no-op, matching the original read().
	unk18           = stream.readU32();
	mDrawBufferSize = stream.readU32();
	mDrawBuffer = new J3DDrawBuffer(mDrawBufferSize);
}

void TDrawBufObj::perform(u32 param_1, TGraphics* param_2)
{
	if (param_1 & 0x80)
		mDrawBuffer->frameInit();

	if (param_1 & 0x400) {
		if (unk18 & 3)
			j3dSys.setDrawBuffer(mDrawBuffer, 0);

		if (unk18 & 4)
			j3dSys.setDrawBuffer(mDrawBuffer, 1);
	}

	if (param_1 & 8) {
		j3dSys.unk4C = unk18;
#ifdef SMS_NATIVE_PLATFORM
		{
			static int dbg = -1;
			if (dbg < 0) { const char* e = getenv("SB_J3D_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
			static long s_n = 0;
			if (dbg && (++s_n % 2000) == 0)
				fprintf(stderr, "[drawbuf] TDrawBufObj::perform draw() calls=%ld\n", s_n);
		}
#endif
		mDrawBuffer->draw();
	}
}
