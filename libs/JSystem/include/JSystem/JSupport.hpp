#ifndef J_SUPPORT_HPP
#define J_SUPPORT_HPP

#include <dolphin/types.h>
#include <stdint.h>

template <typename T> T* JSUConvertOffsetToPtr(const void* ptr, u32 offset)
{
	// A u32 offset compared against `nullptr` compiles under the decomp's C++98
	// build, where `nullptr` is a macro for 0, and is ill-formed under the C++17
	// this port builds the decomp as, where `nullptr` really is std::nullptr_t.
	// The zero test is the same test on both toolchains, so it is spelled as one.
	if (offset == 0) {
		return nullptr;
	} else {
		return (T*)((uintptr_t)ptr + offset);
	}
}

template <typename T>
T* JSUConvertOffsetToPtr(const void* ptr, const void* offset)
{
	if (offset == nullptr) {
		return nullptr;
	} else {
		return (T*)((uintptr_t)ptr + (uintptr_t)offset);
	}
}

inline u16 JSULoHalf(u32 in) { return in & 0xffff; }
inline u8 JSULoByte(u16 in) { return in & 0xff; }
inline u8 JSUHiByte(u16 in) { return in >> 8; }

#endif
