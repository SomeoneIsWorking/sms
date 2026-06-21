#ifndef JPA_SWAP_H
#define JPA_SWAP_H

// =============================================================================
// Native PC build: JPA1 (.jpa) particle resources are GameCube BIG-ENDIAN. The
// decomp loaders read them by casting raw file bytes to host-endian scalars
// (JPABaseShape/ExtraShape/SweepShape/ExTexShape ctors via *(T*)(data+off);
// JPAKeyFrameAnime float table via raw f32*) OR via JSUMemoryInputStream
// (BEM1/FLD1), which itself byteswaps the typed readers (readS16/readU16) but
// NOT the raw read(&x,N) byte-copies. So on a little-endian host every
// multi-byte field read by a RAW path is misread (header block-count/offsets
// -> wild walk; emitter/shape scalars -> garbage).
//
// sb_jpa_swap_to_host swaps, IN PLACE, exactly the fields the loaders read via a
// RAW path, and LEAVES the fields BEM1/FLD1 read via the auto-swapping
// JSUInputStream typed readers (else they'd double-swap). It therefore needs NO
// edits to the decomp loaders. It is idempotent (no-op once host-endian) and
// only acts on a recognised BE JPA1 magic; an unknown magic is left untouched so
// the caller's fail-fast magic check (OSPanic) fires at the real cause.
//
// This is the pure, unit-tested shipping function (native/platform/tests/
// jpa_swap_test.cpp asserts the swapped offsets == the loaders' read offsets).
// The TIMG inside a TEX1 block is a J3D texture handled by the texture path; it
// is NOT swapped here (do not double-swap). Field offsets are documented against
// the consuming ctor/loader so they cannot silently drift.
// =============================================================================

#include <cstdint>
#include <cstring>

namespace smsport {
namespace jpa_detail {

inline uint16_t be16(const uint8_t* p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
inline uint32_t be32(const uint8_t* p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
	     | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
inline void sw16(uint8_t* p)
{
	uint8_t t = p[0];
	p[0]      = p[1];
	p[1]      = t;
}
inline void sw32(uint8_t* p)
{
	uint8_t t;
	t = p[0]; p[0] = p[3]; p[3] = t;
	t = p[1]; p[1] = p[2]; p[2] = t;
}
// f32 has the same byte order as u32 — reversing the 4 bytes converts it.

// bounds-checked in-place swaps relative to a block base + its size.
inline void s16_at(uint8_t* blk, uint32_t off, uint32_t size)
{
	if (off + 2 <= size) sw16(blk + off);
}
inline void s32_at(uint8_t* blk, uint32_t off, uint32_t size)
{
	if (off + 4 <= size) sw32(blk + off);
}

constexpr uint32_t fourcc(char a, char b, char c, char d)
{
	return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16)
	     | ((uint32_t)(uint8_t)c << 8) | (uint32_t)(uint8_t)d;
}

// --- BSP1 (JPABaseShape::JPABaseShape, JPABaseShape.cpp) ---------------------
inline void swap_BSP1(uint8_t* b, uint32_t n)
{
	// offset fields used to locate the anim arrays (consumed before swap below)
	uint16_t prmKeyOff = be16(b + 0x14);  // makeColorTable base (data+s16@0x14)
	uint16_t envKeyOff = be16(b + 0x16);  // makeColorTable base (data+s16@0x16)
	uint8_t  prmCount  = (0x62 < n) ? b[0x62] : 0;  // unk85 (prm key count)
	uint8_t  envCount  = (0x63 < n) ? b[0x63] : 0;  // unk86 (env key count)

	s16_at(b, 0x12, n);  // mTextureIndices src offset (memcpy base)
	s16_at(b, 0x14, n);  // prm color anim key offset
	s16_at(b, 0x16, n);  // env color anim key offset
	s32_at(b, 0x18, n);  // mBaseSizeY (f32)
	s32_at(b, 0x1C, n);  // mBaseSizeX (f32)
	s16_at(b, 0x20, n);  // mLoopOffset
	// 0x22..0x26 u8 (mType/mDirType/mRotType + flag bytes) — no swap
	s16_at(b, 0x5C, n);  // mColorRegAnmMaxFrm (drives makeColorTable alloc size)
	// 0x5E..0x63 u8; GXColor @0x64,0x68 stay byte order — no swap
	s16_at(b, 0x80, n);  // mTexStaticTransX (fix)
	s16_at(b, 0x82, n);  // mTexStaticScaleX
	s16_at(b, 0x84, n);  // mTexStaticTransY
	s16_at(b, 0x86, n);  // mTexStaticScaleY
	s16_at(b, 0x88, n);  // mTilingX
	s16_at(b, 0x8A, n);  // mTilingY
	s16_at(b, 0x8C, n);  // mTexScrollTransX
	s16_at(b, 0x8E, n);  // mTexScrollScaleX
	s16_at(b, 0x90, n);  // mTexScrollTransY
	s16_at(b, 0x92, n);  // mTexScrollScaleY
	s16_at(b, 0x94, n);  // mTexScrollRotate
	// 0x96 u8 — no swap

	// JPAColorRegAnmKey { s16 unk0; GXColor unk2; } stride 6 — swap unk0 only.
	for (uint32_t k = 0; k < prmCount; ++k)
		s16_at(b, prmKeyOff + k * 6, n);
	for (uint32_t k = 0; k < envCount; ++k)
		s16_at(b, envKeyOff + k * 6, n);
}

// --- ESP1 (JPAExtraShape::JPAExtraShape, JPAExtraShape.cpp) ------------------
inline void swap_ESP1(uint8_t* b, uint32_t n)
{
	static const uint16_t s16off[] = {
		0x14, 0x16, 0x18, 0x1A, 0x1C, 0x20, 0x22, 0x24, 0x26,
		0x34, 0x36, 0x38, 0x3A, 0x3E, 0x42, 0x44, 0x48, 0x4C,
		0x5A, 0x5C, 0x5E, 0x60, 0x62,
	};
	for (uint32_t i = 0; i < sizeof(s16off) / sizeof(s16off[0]); ++i)
		s16_at(b, s16off[i], n);
	// 0x1E,0x1F,0x40,0x41,0x4A,0x4B,0x4E,0x64 u8 — no swap
}

// --- SSP1 (JPASweepShape::JPASweepShape, JPASweepShape.cpp) ------------------
inline void swap_SSP1(uint8_t* b, uint32_t n)
{
	static const uint16_t s16off[] = {
		0x14, 0x16, 0x18, 0x30, 0x32, 0x34, 0x48, 0x4A, 0x54, 0x60,
	};
	for (uint32_t i = 0; i < sizeof(s16off) / sizeof(s16off[0]); ++i)
		s16_at(b, s16off[i], n);
	s32_at(b, 0x28, n);  // unk8 (f32)
	s32_at(b, 0x2C, n);  // unkC (f32)
	s32_at(b, 0x4C, n);  // mScaleY (f32)
	s32_at(b, 0x50, n);  // mScaleX (f32)
	// 0x10,0x11,0x12,0x1A,0x36,0x44..0x47,0x56,0x57 u8; GXColor @0x58,0x5C — no swap
}

// --- ETX1 (JPAExTexShape::JPAExTexShape, JPAExTexShape.cpp) ------------------
inline void swap_ETX1(uint8_t* b, uint32_t n)
{
	static const uint16_t s16off[] = {
		0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C,  // mIndTexMtx fix entries
	};
	for (uint32_t i = 0; i < sizeof(s16off) / sizeof(s16off[0]); ++i)
		s16_at(b, s16off[i], n);
	// 0x10,0x11,0x1E,0x1F,0x20,0x30,0x33 u8 — no swap
}

// --- KFA1 (JPAKeyFrameAnime; runtime read in JPABaseEmitter::calcKeyFrameAnime)
// raw f32 table at +0x20, frame_num (u8 @0x10) entries of 4 floats each.
inline void swap_KFA1(uint8_t* b, uint32_t n)
{
	uint8_t frameNum = (0x10 < n) ? b[0x10] : 0;  // animeData[0x10]
	// 0x10/0x12 u8 flags — no swap
	uint32_t floats = (uint32_t)frameNum * 4;
	for (uint32_t i = 0; i < floats; ++i)
		s32_at(b, 0x20 + i * 4, n);  // keyFrames[]
}

// --- BEM1 (JPABaseEmitter::loadBaseEmitterBlock, JPAEmitter.cpp) -------------
// JSUMemoryInputStream: swap ONLY the RAW read(&x,N) fields; the readS16 fields
// auto-swap in the reader, so leave them BE (else double-swap).
inline void swap_BEM1(uint8_t* b, uint32_t n)
{
	// raw f32 / u32 reads
	s32_at(b, 0x0C, n);  // mScale.x
	s32_at(b, 0x10, n);  // mScale.y
	s32_at(b, 0x14, n);  // mScale.z
	s32_at(b, 0x18, n);  // mTrans.x
	s32_at(b, 0x1C, n);  // mTrans.y
	s32_at(b, 0x20, n);  // mTrans.z
	s32_at(b, 0x30, n);  // mChildSpawnRate (f32)
	s32_at(b, 0x50, n);  // unk1FC (f32)
	s32_at(b, 0x54, n);  // unk200 (f32)
	s32_at(b, 0x58, n);  // unk204 (f32)
	s32_at(b, 0x5C, n);  // unk208 (f32)
	s32_at(b, 0x60, n);  // unk1C8 (f32)
	s32_at(b, 0x6C, n);  // mEmitFlags (u32)
	s32_at(b, 0x70, n);  // mKeyAnmTypeMask (u32)
	// raw s16 reads (read(&x,2) / read(&mRot,6) / read(&fixVec,6))
	s16_at(b, 0x24, n);  // mRot.x (S16Vec)
	s16_at(b, 0x26, n);  // mRot.y
	s16_at(b, 0x28, n);  // mRot.z
	s16_at(b, 0x2E, n);  // mVolumeSubdivision
	s16_at(b, 0x38, n);  // mStartFrame
	s16_at(b, 0x3A, n);  // mVolumeSize
	s16_at(b, 0x40, n);  // mBaseLifetime
	s16_at(b, 0x64, n);  // fixVec.x (emitter direction)
	s16_at(b, 0x66, n);  // fixVec.y
	s16_at(b, 0x68, n);  // fixVec.z
	// readS16-consumed (auto-swap, leave BE): 0x34,0x36,0x3C,0x3E,0x42,0x44,
	// 0x46,0x48,0x4A,0x4C,0x4E,0x6A
	// u8 (no swap): 0x2A mVolumeType, 0x2B mEmitInterval
}

// --- FLD1 (JPABaseField::loadFieldBlock, JPAField.cpp) -----------------------
// JSUMemoryInputStream: after skip(0xC) + five 1-byte reads + skip(2) the cursor
// sits at block-relative 0x13, then twelve RAW read(&f32,4) floats, then four
// readS16 (auto-swap, leave BE). u8 fields (0x0C..0x10) and the mMaxDistanceSq
// 1-byte read (0x10) stay as bytes.
inline void swap_FLD1(uint8_t* b, uint32_t n)
{
	for (uint32_t i = 0; i < 12; ++i)
		s32_at(b, 0x13 + i * 4, n);  // unk10,unk14,mMaxDistance,unk18.xyz,
		                             // unk24.xyz,unk30,unk34,unk38 (f32)
	// readS16-consumed (auto-swap, leave BE): 0x43,0x45,0x47,0x49 (fade timings)
}

}  // namespace jpa_detail
}  // namespace smsport

// In-place BE->host swap of one freshly-loaded .jpa (JPA1) buffer. Idempotent;
// no-op on an already-host or unrecognised magic.
inline void sb_jpa_swap_to_host(uint8_t* buf)
{
	using namespace smsport::jpa_detail;
	if (buf == nullptr) return;

	uint32_t magic = be32(buf);  // natural FourCC order of the on-disk bytes
	if (magic != fourcc('J', 'E', 'F', 'F'))
		return;  // not a BE JPA1 (already host, or genuinely bad) -> leave it

	// header: JPABinaryHeader { u32 unk0,unk4,unk8,unkC }; unkC = block count.
	uint32_t blockCount = be32(buf + 0x0C);
	sw32(buf + 0x00);  // 'JEFF'
	sw32(buf + 0x04);  // 'jpa1'
	sw32(buf + 0x08);  // unk8
	sw32(buf + 0x0C);  // unkC (block count)

	uint32_t off = 0x20;  // first block (loader starts its walk at 0x20)
	for (uint32_t i = 0; i < blockCount; ++i) {
		uint32_t type       = be32(buf + off);       // block FourCC (still BE)
		uint32_t nextOffset = be32(buf + off + 4);   // block size (still BE)
		sw32(buf + off);       // type -> host (loader compares as host int)
		sw32(buf + off + 4);   // nextOffset/size -> host

		uint8_t* blk = buf + off;
		switch (type) {
		case 0x42535031 /*'BSP1'*/: swap_BSP1(blk, nextOffset); break;
		case 0x45535031 /*'ESP1'*/: swap_ESP1(blk, nextOffset); break;
		case 0x53535031 /*'SSP1'*/: swap_SSP1(blk, nextOffset); break;
		case 0x45545831 /*'ETX1'*/: swap_ETX1(blk, nextOffset); break;
		case 0x4B464131 /*'KFA1'*/: swap_KFA1(blk, nextOffset); break;
		case 0x42454D31 /*'BEM1'*/: swap_BEM1(blk, nextOffset); break;
		case 0x464C4431 /*'FLD1'*/: swap_FLD1(blk, nextOffset); break;
		// 'TEX1' (texture: name string + TIMG handled by texture path),
		// 'TDB1' and any other: header swapped above, body untouched.
		default: break;
		}

		if (nextOffset == 0) break;  // avoid infinite loop on corrupt size
		off += nextOffset;
	}
}

#endif
