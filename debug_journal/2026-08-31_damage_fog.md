# Damage fog behavior

`SMS_ResetDamageFogEffect` and `SMS_AddDamageFogEffect` were empty despite being
called by Mario, his cap and hand models, Enemy Mario, and Gate Keeper.

Ghidra decompilation of GMSE01 at `0x802264d8` (size `0xa4`) and `0x80226584`
(size `0x120`), cross-checked against the generated Sunbright recomp bodies,
established the following behavior:

- reset writes the camera near/far planes into every material fog block and
  moves its ramp to `[cameraFar - 1, cameraFar]`;
- active damage fog transforms the actor position through the current view
  matrix and writes a 1,200-unit fog band centered around that view depth;
- both ends of that band receive the same `300 * sin(frame * 0x888)` pulse,
  with the sine angle truncated to signed 16 bits.

`include/MarioUtil/DamageFog.hpp` owns the pure formulas so the shipping body
and Sunbright's numeric control use the same implementation. The native Clang
build and the bounded guarded decomp run reached a clean exit. Matching-MWCC
comparison remains unavailable here because the configured decomp targets are
GMSJ01/GMSP01 and their required original disc is not present.
