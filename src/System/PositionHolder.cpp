#include <JSystem/JSupport/JSUInputStream.hpp> // JSU_BE32 / JSU_BE32_INPLACE
#include <System/PositionHolder.hpp>

TNameRefAryT<TStagePositionInfo>* gpPositionHolder;

void TStagePositionInfo::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);
	stream.read(&unkC.x, sizeof(f32));
	stream.read(&unkC.y, sizeof(f32));
	stream.read(&unkC.z, sizeof(f32));
	// BE floats on disc; raw read(&x,4) does not swap (JSU raw-read class).
	JSU_BE32_INPLACE(&unkC.x);
	JSU_BE32_INPLACE(&unkC.y);
	JSU_BE32_INPLACE(&unkC.z);

	stream.readF32();
	stream.readF32();
	stream.readF32();
	stream.readF32();
	stream.readF32();
	stream.readF32();
}
