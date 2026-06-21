#include <System/ParamInst.hpp>
#include <JSystem/JGeometry.hpp>

#ifdef SMS_NATIVE_PLATFORM
// The .prm param blobs are big-endian GC assets; stream.read() raw-copies bytes
// without byteswapping (unlike readS32/read32b), so on a little-endian host
// every loaded scalar comes back byteswapped (e.g. camera mSLNearClip 10.0f ->
// 1.16e-41 denormal -> NaN near-plane -> OOB collision-grid index). Swap each
// value to host endian per type after the raw read.
namespace {
inline void param_bswap(u8&) { }
inline void param_bswap(s16& v) { v = (s16)__builtin_bswap16((u16)v); }
inline void param_bswap(s32& v) { v = (s32)__builtin_bswap32((u32)v); }
inline void param_bswap(f32& v)
{
	u32 b;
	__builtin_memcpy(&b, &v, sizeof(b));
	b = __builtin_bswap32(b);
	__builtin_memcpy(&v, &b, sizeof(b));
}
inline void param_bswap(JGeometry::TVec3<f32>& v)
{
	param_bswap(v.x);
	param_bswap(v.y);
	param_bswap(v.z);
}
} // namespace
#endif

template <typename T> void TParamT<T>::load(JSUMemoryInputStream& stream)
{
	// TODO: fakematch
	u8 discard[16];

	stream.read(&discard[8], 4);
	stream.read(&value, sizeof(T));
#ifdef SMS_NATIVE_PLATFORM
	param_bswap(value);
#endif
};

template class TParamT<u8>;
template class TParamT<s16>;
template class TParamT<s32>;
template class TParamT<f32>;
template class TParamT<JGeometry::TVec3<f32> >;
