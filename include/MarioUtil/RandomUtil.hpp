#ifndef MARIO_UTIL_RANDOM_UTIL_HPP
#define MARIO_UTIL_RANDOM_UTIL_HPP

#include <dolphin/types.h>
#include <stdlib.h>

// NOTE: the entire file and all functions in it
// is made-up. Seems we got unlucky and all of these
// helpers got completely inlined.

// RAND_MAX + 1 IS INT ARITHMETIC AND OVERFLOWS ON THE HOST.
//
// The GC SDK's RAND_MAX is small, so `RAND_MAX + 1` is an ordinary int there and this expression
// yields the intended 1/(RAND_MAX+1). On a host where RAND_MAX is INT_MAX (glibc: 2147483647) the
// same addition overflows to -2147483648, the scale becomes -4.66e-10, and MsRandF() returns a
// value in (-1, 0) — ALWAYS NEGATIVE. Measured over 2e6 draws: min -0.999999, max -5.6e-07.
//
// That is not a cosmetic difference. Every randomised quantity in the game came out below its
// intended range, and `int rnd = MsRandF() * num` indexes ARRAYS FROM BELOW: TGraphWeb::
// getRandomNextIndex does exactly that on railNode->mConnections, returning garbage as a node
// index, which TGraphTracer::moveTo then dereferences — an intermittent SIGSEGV in Delfino Plaza
// (~50% of runs) whose randomness is the RNG itself.
//
// Computing the divisor in FLOAT cannot overflow and is bit-identical wherever the int version was
// already well-defined (RAND_MAX 32767 -> 1/32768 either way), so this changes no behaviour on the
// original target — it only stops the host from corrupting.
// this is found in code so much that I am almost
// certain that it is real
#ifdef SMS_NATIVE_PLATFORM
inline f32 MsRandF() { return rand() * (1.f / ((f32)RAND_MAX + 1.f)); }
#else
inline f32 MsRandF() { return rand() * (1.f / (RAND_MAX + 1)); }
#endif

// TODO: fake!!! need to analyze a bunch of callsites,
// smallEnemy kind of implies a random interval class or
// something like that
#ifdef SMS_NATIVE_PLATFORM
inline f32 MsRandF(f32 l, f32 r)
{
	// Same overflow as MsRandF() above: unfixed, this returned values BELOW l rather than in [l,r).
	return rand() * (1.f / ((f32)RAND_MAX + 1.f)) * (r - l) + l;
}
#else
inline f32 MsRandF(f32 l, f32 r)
{
	return rand() * (1.f / (RAND_MAX + 1)) * (r - l) + l;
}
#endif

#ifdef SMS_NATIVE_PLATFORM
inline int MsRandI(int l, int r)
{
	// Same overflow as MsRandF() above.
	int rnd = rand() * (1.f / ((f32)RAND_MAX + 1.f)) * (r - l);
	return 1 + l + rnd;
}
#else
inline int MsRandI(int l, int r)
{
	int rnd = rand() * (1.f / (RAND_MAX + 1)) * (r - l);
	return 1 + l + rnd;
}
#endif

// A random-in-range helper. Only ever used fully inlined, so the only trace
// left in the binary is the two UNUSED dtors for TMsRange<f32> / TMsRange<s32>.
template <typename T> class TMsRange {
public:
	TMsRange(T min, T max)
	    : mMin(min)
	    , mMax(max)
	{
	}

	void set(T min, T max)
	{
		mMin = min;
		mMax = max;
	}

	T rand() const
	{
		T range = mMax - mMin;
		return mMin + (T)(range * MsRandF());
	}

	// real AND required
	// Without it, codegen is wrong for `TMsRange::rand` AND every single class
	// inheriting from TSmallEnemyParams. Why? No idea.
	~TMsRange() { }

public:
	/* 0x0 */ T mMin;
	/* 0x4 */ T mMax;
};

#endif
