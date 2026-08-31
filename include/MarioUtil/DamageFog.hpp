#ifndef MARIOUTIL_DAMAGEFOG_HPP
#define MARIOUTIL_DAMAGEFOG_HPP

#include <dolphin/types.h>

namespace SMSDamageFog {

struct Range {
	f32 start;
	f32 end;
};

// GMSE01 SMS_ResetDamageFogEffect @ 0x802264d8 moves the fog ramp to the
// final unit of camera depth. The fog object stays enabled, but does not
// affect ordinarily visible geometry.
inline Range resetRange(f32 cameraFar)
{
	Range range = { cameraFar - 1.0f, cameraFar };
	return range;
}

// GMSE01 SMS_AddDamageFogEffect @ 0x80226584 centers a 1200-unit fog band on
// the actor's view-space depth. Both ends share the same 300-unit sine pulse.
inline Range activeRange(f32 viewZ, f32 pulse)
{
	const f32 offset = 300.0f * pulse;
	Range range      = { -viewZ - 700.0f + offset, -viewZ + 500.0f + offset };
	return range;
}

// Retail multiplies the director frame counter by 0x888, then keeps the low
// 16 bits as the signed-angle index into JMath's sine table.
inline s16 waveAngle(u32 frame) { return static_cast<s16>(frame * 0x888); }

} // namespace SMSDamageFog

#endif
