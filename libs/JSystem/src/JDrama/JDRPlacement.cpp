#include <JSystem/JDrama/JDRPlacement.hpp>

#ifdef SMS_NATIVE_PLATFORM
#include <cstdlib>
#include <cstdio>
#endif

using namespace JDrama;

void TPlacement::load(JSUMemoryInputStream& stream)
{
	TNameRef::load(stream);
	stream.read(&mPosition.x, sizeof(f32));
	stream.read(&mPosition.y, sizeof(f32));
	stream.read(&mPosition.z, sizeof(f32));
#ifdef SMS_NATIVE_PLATFORM
	// The placement (.blo/scene) data is big-endian; stream.read() raw-copies bytes
	// WITHOUT byteswapping (unlike readS32/read32b), so every position component
	// comes back byteswapped on the LE host — a real 100.0 -> a ~0 denormal, which
	// is why every object appeared to load at (0,0,0). Swap to host endian.
	for (f32* c = &mPosition.x; c <= &mPosition.z; ++c) {
		u32 b;
		__builtin_memcpy(&b, c, 4);
		b = __builtin_bswap32(b);
		__builtin_memcpy(c, &b, 4);
	}
#endif
#ifdef SMS_NATIVE_PLATFORM
	// SB_PLACE_DBG: trace every loaded placement (name + position) to ground-truth
	// where file-select objects (file blocks, option wall) sit in the scene.
	if (getenv("SB_PLACE_DBG"))
		fprintf(stderr, "[place] %-28s pos(%.1f %.1f %.1f)\n",
		        mName ? mName : "(null)", mPosition.x, mPosition.y, mPosition.z);
#endif
}
