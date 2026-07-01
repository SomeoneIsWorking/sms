#include <Enemy/Generator.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>

// Native port of TOneShotGenerator::load (@0x8008f710). RE: scratch/decomp_next3/8008f710.c.
// Reads two null-terminated names from the scene-load stream. Storage is at +0x70 (first
// read) and +0x68 (second read); the RE writes to the higher offset first, so preserve the
// exact call order to match any state-observing code that might depend on partial-load
// snapshots.
//
// SDA scan (tools/dol_sda.py 0x8008f710): no SDA references at all — pure stream I/O over
// engine primitives, so nothing to look up.
void TOneShotGenerator::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	mSpawnKey1 = stream.readString();   // stored at +0x70
	mSpawnKey2 = stream.readString();   // stored at +0x68
}
