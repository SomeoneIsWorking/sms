#include <JSystem/JDrama/JDRPlacement.hpp>

#ifdef SMS_NATIVE_PLATFORM
#include <cstdlib>
#include <cstdio>
#endif

using namespace JDrama;

void TPlacement::load(JSUMemoryInputStream& stream)
{
	char pad[0x10];
	TNameRef::load(stream);
	stream.read(&mPosition.x, sizeof(f32));
	stream.read(&mPosition.y, sizeof(f32));
	stream.read(&mPosition.z, sizeof(f32));
#ifdef SMS_NATIVE_PLATFORM
	// SB_PLACE_DBG: trace every loaded placement (name + position) to ground-truth
	// where file-select objects (file blocks, option wall) sit in the scene.
	if (getenv("SB_PLACE_DBG"))
		fprintf(stderr, "[place] %-28s pos(%.1f %.1f %.1f)\n",
		        mName ? mName : "(null)", mPosition.x, mPosition.y, mPosition.z);
#endif
}
