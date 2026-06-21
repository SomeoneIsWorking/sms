#include <JSystem/JDrama/JDRDrawBufObj.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>

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

void TDrawBufObj::perform(u32 cue, TGraphics* graphics)
{
	if (cue & CUE_DRAW_INIT)
		mDrawBuffer->frameInit();

	if (cue & CUE_SET_DRAW_BUFFER) {
		if (unk18 & 3)
			j3dSys.setDrawBuffer(mDrawBuffer, 0);

		if (unk18 & 4)
			j3dSys.setDrawBuffer(mDrawBuffer, 1);
	}

	if (cue & CUE_DRAW) {
		j3dSys.unk4C = unk18;
		mDrawBuffer->draw();
	}
}
