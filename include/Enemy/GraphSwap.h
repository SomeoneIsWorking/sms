#ifndef ENEMY_GRAPH_SWAP_H
#define ENEMY_GRAPH_SWAP_H

// =============================================================================
// Native PC build: the stage rail graph (/scene/map/scene.ral) is GameCube
// BIG-ENDIAN. TGraphGroup/TGraphWeb (Enemy/graph.cpp) read it by plain struct
// field access on the raw resource buffer (unk0[i].mNodeNum, railNode->
// mConnectionNum, railNode->mConnections[j], mPeriods[j], ...) — no
// JSUInputStream, so nothing auto-swaps. On a little-endian host every field is
// misread: a node count like 0xB0 reads as 0xB0000000, so `new TGraphNode[count]`
// asks the conductor SolidHeap for ~2.9 GB and OSPanics (JKRSolidHeap OOM).
//
// sb_ral_swap_to_host swaps, IN PLACE, exactly the fields graph.cpp reads, so it
// needs NO edits to the loader. Layout (matches Enemy/Graph.hpp):
//   GraphDesc[] (0xC each): { s32 mNodeNum; u32 mNameOffset; u32 mRailNodesOffset }
//     — array terminated by mNodeNum == 0 (read big-endian to find the end).
//   TRailNode[] (0x44 each) at base+mRailNodesOffset, mNodeNum entries:
//     0x00 S16Vec mPosition (3x s16) · 0x06 s16 mConnectionNum · 0x08 u32 mFlags
//     0x0C u16 mPitch · 0x0E u16 mYaw · 0x10 u16 mRoll · 0x12 s16 mSpeed
//     0x14 u16 mConnections[8] · 0x24 f32 mPeriods[8]
//   Name strings (at mNameOffset) are bytes — not swapped.
// It is bounds-checked against the resource size and guarded against a
// double-swap (the cached resource pointer is remembered), so a stage reload that
// hands back the same buffer is a no-op.
// =============================================================================

#include <cstdint>

namespace smsport {
namespace ral_detail {

inline uint32_t be32(const uint8_t* p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8)
	     | (uint32_t)p[3];
}
inline void sw16(uint8_t* p) { uint8_t t = p[0]; p[0] = p[1]; p[1] = t; }
inline void sw32(uint8_t* p)
{
	uint8_t t;
	t = p[0]; p[0] = p[3]; p[3] = t;
	t = p[1]; p[1] = p[2]; p[2] = t;
}

constexpr uint32_t kGraphDescSize = 0xC;
constexpr uint32_t kRailNodeSize  = 0x44;

// Swap one TRailNode in place (caller guarantees [node, node+0x44) is in bounds).
inline void swap_rail_node(uint8_t* n)
{
	sw16(n + 0x00); sw16(n + 0x02); sw16(n + 0x04); // mPosition x/y/z
	sw16(n + 0x06);                                  // mConnectionNum
	sw32(n + 0x08);                                  // mFlags
	sw16(n + 0x0C); sw16(n + 0x0E); sw16(n + 0x10);  // mPitch/mYaw/mRoll
	sw16(n + 0x12);                                  // mSpeed
	for (int j = 0; j < 8; ++j)
		sw16(n + 0x14 + j * 2);                      // mConnections[8]
	for (int j = 0; j < 8; ++j)
		sw32(n + 0x24 + j * 4);                      // mPeriods[8]
}

} // namespace ral_detail

// Idempotency by CONTENT, not by pointer. A pointer cache is UNSOUND across scene
// reloads: the JKR heap frees a stage's scene.ral and the next stage's is allocated at
// the SAME address, so a cached pointer wrongly reports "already swapped" and the fresh
// big-endian buffer is read raw (mNodeNum 18 -> 0x12000000 -> ~2.9 GB `new TGraphNode[]`
// -> SolidHeap OOM). Instead, inspect the first GraphDesc's mNodeNum: a real rail graph
// has a small node count. If the HOST-order (little-endian) read is already a sane small
// value, the buffer is already swapped; if only the BIG-endian read is sane, it still
// needs swapping. This is self-describing and immune to address reuse.
namespace ral_detail {
inline uint32_t le32(const uint8_t* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
	     | ((uint32_t)p[3] << 24);
}
} // namespace ral_detail

// A plausible rail-graph node count is well under this bound; anything larger read as a
// count is a byte-order artefact. (Real SMS stages top out at a few hundred nodes.)
constexpr uint32_t kRalMaxSaneNodeNum = 0x10000;

inline bool ral_already_swapped(void* data)
{
	using namespace ral_detail;
	const uint8_t* d = (const uint8_t*)data;
	uint32_t hostNodeNum = le32(d + 0x0);   // desc[0].mNodeNum as the host would read it
	// 0 = empty graph (terminator first) -> nothing to swap either way. A small nonzero
	// host-order count means the header is already in host order (already swapped).
	return hostNodeNum != 0 && hostNodeNum < kRalMaxSaneNodeNum;
}

// Returns true if it swapped (or the buffer was already host-side / empty).
inline bool sb_ral_swap_to_host(void* data, uint32_t size)
{
	using namespace ral_detail;
	if (!data || size < kGraphDescSize)
		return true;
	if (ral_already_swapped(data))
		return true;
	uint8_t* base = (uint8_t*)data;

	// Pass 1: read each GraphDesc big-endian to find the terminator and the rail
	// arrays, swap the descs in place.
	uint32_t descOff = 0;
	while (descOff + kGraphDescSize <= size) {
		uint8_t* d         = base + descOff;
		uint32_t nodeNumBE = be32(d + 0x0);
		if (nodeNumBE == 0)
			break; // terminator
		uint32_t railOffBE = be32(d + 0x8);
		sw32(d + 0x0); // mNodeNum
		sw32(d + 0x4); // mNameOffset
		sw32(d + 0x8); // mRailNodesOffset

		// Pass 2 for this desc: swap its TRailNode array.
		for (uint32_t i = 0; i < nodeNumBE; ++i) {
			uint32_t nodeOff = railOffBE + i * kRailNodeSize;
			if (nodeOff + kRailNodeSize > size)
				break; // out of bounds — stop (malformed/over-read guard)
			swap_rail_node(base + nodeOff);
		}
		descOff += kGraphDescSize;
	}
	return true;
}

} // namespace smsport

#endif
