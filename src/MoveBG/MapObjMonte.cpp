#include <MoveBG/MapObjMonte.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapCollisionEntry.hpp>
#include "sms_boot_jumpmushroom.h"

// Native port of TJumpMushroom::load (@0x801f57a0). RE via scratch/disasm.py.
// ジャンプきのこ — the yellow "jump mushroom" trampolines (Delfino / Pianta Village).
// Extends TMapObjBase::load. After the base has parsed the standard header, the RE
// reads a 4-byte-aligned collision-record from the stream and forwards its low 16 bits
// (sign-extended per PPC extsh) to TMapCollisionBase::setAllData(s16) — this is the id
// used to look up the mushroom's per-instance BG-check data (jump direction, spring
// coefficient) at collision time. Top 2 bytes of the record are padding.
//
// The stream is read as a raw 4-byte block on PPC BE; on our LE host we go through
// JSUInputStream::readU32() so the numeric value is the same, then extract the low 16
// bits via the pure helper (unit-tested in jump_mushroom_test).
//
// SDA scan (tools/dol_sda.py 0x801f57a0): no SDA references.
void TJumpMushroom::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	const std::uint32_t raw = stream.readU32();
	if (mMapCollisionManager != nullptr) {
		mMapCollisionManager->getUnk8()->setAllData(
		    sb::jump_mushroom_collision_id_from_serialized(raw));
	}
}
