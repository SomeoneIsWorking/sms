
static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <Strategic/LiveActor.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <string.h>
#include <stdio.h>
#ifdef SMS_NATIVE_PLATFORM
// Explicit big-endian reads + in-place host swaps for the BE .col asset (see
// TMapCollisionBase::init). Never struct-overlay; portable across host endianness.
static inline u16 sb_be16(const u8* p) { return (u16)((p[0] << 8) | p[1]); }
static inline u32 sb_be32(const u8* p)
{
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}
static inline void sb_sw16(u8* p) { u8 t = p[0]; p[0] = p[1]; p[1] = t; }
static inline void sb_sw32(u8* p)
{
	u8 t;
	t = p[0]; p[0] = p[3]; p[3] = t;
	t = p[1]; p[1] = p[2]; p[2] = t;
}
#endif

void TMapCollisionBase::setVertexData(u32 param_1,
                                      const JGeometry::TVec3<f32>& param_2,
                                      const JGeometry::TVec3<f32>& param_3,
                                      const JGeometry::TVec3<f32>& param_4)
{
	unk4[param_1].setVertex(param_2, param_3, param_4);
	gpMapCollisionData->addCheckDataToGrid(&unk4[param_1], getUnk8());
}

static void* loadCollisionData(const char* param_1)
{
	char buffer[256];

	const char* extensionIdx = strstr(param_1, ".col");
	if (extensionIdx == nullptr) {
		if (*param_1 != '/')
			sprintf(buffer, "/scene/%s.col", param_1);
		else
			sprintf(buffer, "/scene%s.col", param_1);
		param_1 = buffer;
	} else {
		if (*param_1 != '/')
			sprintf(buffer, "/scene/%s", param_1);
	}

	return JKRGetResource(param_1);
}

void TMapCollisionBase::setAllBGType(u16 param_1)
{
	for (int i = 0; i < unkC; ++i)
		unk4[i].mBGType = param_1;
}

void TMapCollisionBase::setAllData(s16 param_1)
{
	for (int i = 0; i < unkC; ++i)
		unk4[i].mData = param_1;
}

void TMapCollisionBase::setAllActor(const TLiveActor* param_1)
{
	for (int i = 0; i < unkC; ++i)
		unk4[i].mActor = param_1;
}

// fabricated
struct CollisionDataHeader {
	/* 0x0 */ u32 unk0;
	/* 0x4 */ u32 unk4;
	/* 0x8 */ u32 unk8;
	/* 0xC */ u32 unkC;
};

void TMapCollisionBase::init(const char* param_1, u16 param_2,
                             const TLiveActor* param_3)
{
	CollisionDataHeader* hdr = (CollisionDataHeader*)loadCollisionData(param_1);

#ifdef SMS_NATIVE_PLATFORM
	// The .col asset is big-endian on disc AND its per-group records
	// (FabricatedUnk1CStruct) store 32-bit offsets in pointer-typed fields — a
	// double problem on a 64-bit LE host: the raw struct overlay misreads every
	// u32 (header unk0 count read as 0x02000000 -> 33M-iteration MTXMultVecArray
	// walk-off) AND the 0x18-byte on-disc record can't overlay the 0x28-byte
	// host struct. Fix faithfully: swap a HOST COPY of the buffer (leaving the
	// cached JKRGetResource resource pristine — TMapCollisionData::init reads the
	// same map.col via a stream and must still see BE), then RELAYOUT the group
	// records into a freshly-allocated host array with absolute 64-bit pointers.
	// Header @0x00: u32 vertCount, vecOff, groupCount, groupOff. Vec array @vecOff
	// (vertCount x 3 f32). Group records @groupOff (groupCount x 0x18): u16 unk0,
	// s16 unk2(=tri count n), u16 unk4, u32 off8/offC/off10/off14. off8 -> s16[3n]
	// vertex indices, offC/off10 -> u8[n], off14 -> s16[n] (0 if absent).
	{
		const u8* be = (const u8*)hdr;
		u32 vertCount  = sb_be32(be + 0x00);
		u32 vecOff     = sb_be32(be + 0x04);
		u32 groupCount = sb_be32(be + 0x08);
		u32 groupOff   = sb_be32(be + 0x0C);

		// Total byte extent we must copy (max end of every referenced region).
		u32 extent = 0x10;
		auto grow = [&](u32 end) { if (end > extent) extent = end; };
		grow(vecOff + vertCount * 12);
		grow(groupOff + groupCount * 0x18);
		for (u32 i = 0; i < groupCount; ++i) {
			const u8* e = be + groupOff + i * 0x18;
			u32 n   = (u16)(s16)sb_be16(e + 0x02);
			u32 o8  = sb_be32(e + 0x08), oC = sb_be32(e + 0x0C);
			u32 o10 = sb_be32(e + 0x10), o14 = sb_be32(e + 0x14);
			grow(o8 + n * 3 * 2); grow(oC + n); grow(o10 + n);
			if (o14) grow(o14 + n * 2);
		}

		u8* host = new u8[extent];
		memcpy(host, be, extent);
		// Vertex positions: vertCount x 3 f32.
		for (u32 i = 0; i < vertCount * 3; ++i) sb_sw32(host + vecOff + i * 4);
		// Per-group s16 index/data sub-arrays (u8 arrays stay as-is).
		for (u32 i = 0; i < groupCount; ++i) {
			const u8* e = be + groupOff + i * 0x18;
			s32 n   = (s16)sb_be16(e + 0x02);
			u32 o8  = sb_be32(e + 0x08), o14 = sb_be32(e + 0x14);
			for (s32 j = 0; j < n * 3; ++j) sb_sw16(host + o8 + j * 2);
			if (o14) for (s32 j = 0; j < n; ++j) sb_sw16(host + o14 + j * 2);
		}

		unk10 = vertCount;
		unk14 = (Vec*)(host + vecOff);
		unk18 = groupCount;
		FabricatedUnk1CStruct* arr
		    = new FabricatedUnk1CStruct[groupCount ? groupCount : 1];
		for (u32 i = 0; i < groupCount; ++i) {
			const u8* e = be + groupOff + i * 0x18;
			arr[i].unk0  = sb_be16(e + 0x00);
			arr[i].unk2  = (s16)sb_be16(e + 0x02);
			arr[i].unk4  = sb_be16(e + 0x04) | 0x8000;   // mark as fixed-up
			arr[i].unk8  = (s16*)(host + sb_be32(e + 0x08));
			arr[i].unkC  = (u8*)(host + sb_be32(e + 0x0C));
			arr[i].unk10 = (u8*)(host + sb_be32(e + 0x10));
			u32 o14      = sb_be32(e + 0x14);
			arr[i].unk14 = o14 ? (s16*)(host + o14) : nullptr;
		}
		unk1C = arr;

		if (param_2 & 0x8000)
			unk5C |= 0x8000;
		return;
	}
#endif

	unk10 = hdr->unk0;
	unk14 = (Vec*)((u8*)hdr + hdr->unk4);
	unk18 = hdr->unk8;
	unk1C = (FabricatedUnk1CStruct*)((u8*)hdr + hdr->unkC);

	if (param_2 & 0x8000)
		unk5C |= 0x8000;

	if (!(unk1C->unk4 & 0x8000)) {
		for (s16 i = 0; i < unk18; ++i) {
			unk1C[i].unk8  = (s16*)((intptr_t)unk1C[i].unk8 + (u8*)hdr);
			unk1C[i].unkC  = (u8*)((intptr_t)unk1C[i].unkC + (u8*)hdr);
			unk1C[i].unk10 = (u8*)((intptr_t)unk1C[i].unk10 + (u8*)hdr);

			if (unk1C[i].unk14)
				unk1C[i].unk14 = (s16*)((intptr_t)unk1C[i].unk14 + (u8*)hdr);

			unk1C[i].unk4 |= 0x8000;
		}
	}
}

void TMapCollisionStatic::setUp()
{
	if (unk4)
		return;

	offFlag(1);

	u16 thing = 0;
	if (unk10 >= 0x15E)
		thing |= 4;
	TMapCollisionBase::initCheckData(-9999, thing, unk60);
}

void TMapCollisionStatic::init(const char* param_1, u16 param_2,
                               const TLiveActor* param_3)
{
	TMapCollisionBase::init(param_1, param_2, param_3);
	unk8 = 0;
	if (!(param_2 & 2)) {
		initCheckData(-9999, param_2, param_3);
	} else {
		unk60 = param_3;
		onFlag(1);
	}
}

TMapCollisionStatic::TMapCollisionStatic()
    : unk60(nullptr)
{
}

void TMapCollisionMove::move()
{
	if (checkFlag(0x1))
		return;

	if (checkFlag(0x4000)) {
		setList();
		return;
	}

	if (checkFlag(0x8000)) {
		JGeometry::TVec3<f32> local_18;
		local_18.x = unk20[0][3];
		local_18.y = unk20[1][3];
		local_18.z = unk20[2][3];
		TMapCollisionBase::updateTrans(local_18);
	} else {
		TMapCollisionBase::update();
	}
}

void TMapCollisionMove::moveTrans(const JGeometry::TVec3<f32>& param_1)
{
	if (checkFlag(0x1))
		return;

	if (checkFlag(0x4000)) {
		setList();
		return;
	}

	TMapCollisionBase::updateTrans(param_1);
}

void TMapCollisionMove::init(u32 param_1, u16 bg_type, s16 data,
                             const TLiveActor* actor)
{
	unk8 = 1;
	unkC = param_1;
	unk4 = gpMapCollisionData->allocCheckData(getUnkC());
	for (int i = 0; i < getUnkC(); ++i) {
		unk4[i].mBGType = bg_type;
		unk4[i].mData   = data;
		unk4[i].mActor  = actor;
	}
}

void TMapCollisionMove::init(const char* param_1, u16 param_2,
                             const TLiveActor* param_3)
{
	TMapCollisionBase::init(param_1, param_2, param_3);
	unk8 = 1;
	TMapCollisionBase::initCheckData(-9999, param_2 | 2, param_3);
	onFlag(1);
}

TMapCollisionMove::TMapCollisionMove() { }

void TMapCollisionWarp::setUp()
{
	if (!checkFlag(1))
		return;

	offFlag(1);

	unk60 = gpMapCollisionData->getEntryID();

	if (checkFlag(0x8000)) {
		JGeometry::TVec3<f32> local_18(unk20[0][3], unk20[1][3], unk20[2][3]);
		TMapCollisionBase::updateTrans(local_18);
	} else {
		TMapCollisionBase::update();
	}

	unk64 = gpMapCollisionData->getEntryRelatedThing(unk60);
}

void TMapCollisionWarp::setUpTrans(const JGeometry::TVec3<f32>& param_1)
{
	if (!checkFlag(1))
		return;

	offFlag(1);

	unk60 = gpMapCollisionData->getEntryID();
	TMapCollisionBase::updateTrans(param_1);
	unk64 = gpMapCollisionData->getEntryRelatedThing(unk60);
}

void TMapCollisionWarp::remove()
{
	if (!(checkFlag(1) ? false : true))
		return;

	onFlag(1);
	gpMapCollisionData->removeCheckListData(unk60, unk64);
}

void TMapCollisionWarp::init(const char* param_1, u16 param_2,
                             const TLiveActor* param_3)
{
	TMapCollisionBase::init(param_1, param_2, param_3);
	unk8 = 2;
	TMapCollisionBase::initCheckData(-9999, param_2 | 2, param_3);
	onFlag(1);
}

TMapCollisionWarp::TMapCollisionWarp()
    : unk60(0)
    , unk64(0)
{
}
