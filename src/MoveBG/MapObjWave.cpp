#include <MoveBG/MapObjWave.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>

#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#endif

// Partial faithful port of TMapObjWave (the "波の表現" water-surface manager).
//
// RE source: Ghidra decomp of the GMSE01 binary —
//   getHeight    @ 0x801dd568   (this file)
//   getWaveHeight@ 0x801dd694   (this file)
//   load         @ 0x801dcc08   (NOT YET PORTED — still stubbed in classes2_stubs.cpp)
//   perform      @ 0x801dce60   (NOT YET PORTED — still stubbed in ring3_stubs.cpp)
//   updateHeightAndAlpha @ 0x801dcf04 (NOT YET PORTED)
//
// SDA2 constants resolved live from the running game (r2/_SDA2_BASE_ = 0x80416BA0):
//   SDA2[-0x24b0] = 50.0f   (y bias added before the ground query)
//   SDA2[-0x24b8] = 0.15915507f ( = 1/(2*pi), the wave space->phase scale)
//   SDA2[-0x2530] = 0.0f    (the "no wave" default surface height)
//
// Scope note: getHeight's SEA-water branch (BG types 0x102/0x103) layers an animated wave
// displacement (getWaveHeight) on top of the static water plane. That displacement is driven
// by per-frame wave state (member fields m24/m28/m3c/m40/m64/m68/m94) populated by load() and
// advanced by perform()/updateHeightAndAlpha() — a subsystem that is only exercised/verifiable
// in the Delfino sea and is therefore deferred (the project ports in boot order: title ->
// file-select -> gameplay). Until that lands, the sea branch returns the static plane height
// and logs once. The NON-water path (the file-select beach: Mario stands on BG type 0x8701)
// and the non-sea water-surface path are fully faithful here.

extern TMap* gpMap;

f32 TMapObjWave::getHeight(float x, float y, float z) const
{
	const TBGCheckData* bg = nullptr;
	// @801dd568: query the ground column at (x, 50 + y, z). checkGroundExactY adds its own
	// +78 internally; 50.0f is SDA2[-0x24b0].
	f32 h = gpMap ? gpMap->checkGroundExactY(x, 50.0f + y, z, &bg) : y;

	// Default (the BG under the sample point is NOT a water surface): the passed-in y is
	// returned unchanged — the object simply rests at its own world height.
	if (!bg || !bg->isWaterSurface())
		return y;

	// Over a water surface: the surface plane's height.
	if (bg->isSea()) {
#ifdef SMS_NATIVE_PLATFORM
		// SEA_WATER / DAMAGING_SEA_WATER add an animated wave bob in the original (the
		// load()/perform() wave state machine). Deferred — see scope note above.
		static bool s_warned = false;
		if (!s_warned) {
			s_warned = true;
			std::fprintf(stderr, "[wave] NOTE: sea-water animated wave displacement not yet "
			             "ported (getHeight @801dd568 load/perform path); using static plane "
			             "height %.1f at (%.0f,%.0f,%.0f)\n", h, x, y, z);
		}
#endif
	}
	return h;
}

f32 TMapObjWave::getWaveHeight(float /*x*/, float /*z*/) const
{
	// @801dd694: m3c*sinf(m24*(x/2pi)+m64) + m40*sinf(m28*(z/2pi)+m68), with the wave
	// coefficients/phases set by load()/perform(). Those are not yet ported (see getHeight's
	// scope note), so this returns the SDA2[-0x2530] "no wave" default until that lands.
	return 0.0f;
}
