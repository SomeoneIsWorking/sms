#include <MoveBG/MapObjRicco.hpp>

// Native port of TCraneCargo::control (@0x801ce218). RE: scratch/decomp_next5/801ce218.c.
// The crane cargo boxes in Ricco Harbor. Each tick, zero the inherited TLeanBlock::unk158
// velocity-ish Vec3f then defer to TMapObjBase::control for the ordinary map-object update.
// The RE stores SDA2[-0x28d0] (= 0.0f) into all three components before the base call —
// effectively "wipe last-tick residual then let the base recompute." The base call goes
// directly to TMapObjBase::control (0x801b00f8, NOT TLeanBlock's own 0x801c3d74 override),
// so we skip TLeanBlock's version by qualifying with TMapObjBase.
//
// SDA scan (tools/dol_sda.py 0x801ce218):
//   SDA2[-0x28d0] = 0.0f
void TCraneCargo::control()
{
	unk158.setAll(0.0f);
	TMapObjBase::control();
}
