#include <JSystem/JDrama/JDRViewport.hpp>
#include <dolphin/gx/GXCull.h>
#ifdef SMS_NATIVE_PLATFORM
#include <JSystem/JSupport/JSUInputStream.hpp> // JSU_BE32
#endif

using namespace JDrama;

TViewport::TViewport(const TRect& param_1, const char* name)
    : TViewObj(name)
    , unk10(param_1)
    , unk20(0)
{
}

void TViewport::perform(u32 cue, TGraphics* graphics)
{
	if (!(cue & (CUE_DRAW | CUE_DRAW_INIT)))
		return;

	graphics->setViewport(unk10, 0.0f, 1.0f);

	if (unk20 & 1)
		return;

	TRect rect(graphics->getDisplayRect());
	rect.intersect(unk10);
	graphics->setScissor(rect);
}

void TViewport::load(JSUMemoryInputStream& stream)
{
	TNameRef::load(stream);
	stream.read(&unk10.x1, sizeof(int));
	stream.read(&unk10.y1, sizeof(int));
	stream.read(&unk10.x2, sizeof(int));
	stream.read(&unk10.y2, sizeof(int));
#ifdef SMS_NATIVE_PLATFORM
	// The .ral stores these as big-endian dwords; raw read(&x,4) does not
	// byteswap (same class as TPerformList::load's filter — see JSU_BE32
	// there). Misread rects fed GXSetViewport 65536x65536 with a 256x256
	// scissor, clipping the whole 3D scene to a corner patch at title.
	unk10.x1 = JSU_BE32(unk10.x1);
	unk10.y1 = JSU_BE32(unk10.y1);
	unk10.x2 = JSU_BE32(unk10.x2);
	unk10.y2 = JSU_BE32(unk10.y2);
#endif
}
