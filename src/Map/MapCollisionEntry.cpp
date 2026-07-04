
static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <Strategic/LiveActor.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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
	mCheckDatas[param_1].setVertex(param_2, param_3, param_4);
	gpMapCollisionData->addCheckDataToGrid(&mCheckDatas[param_1], getUnk8());
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
	for (int i = 0; i < mCheckDataNum; ++i)
		mCheckDatas[i].mBGType = param_1;
}

void TMapCollisionBase::setAllData(s16 param_1)
{
	for (int i = 0; i < mCheckDataNum; ++i)
		mCheckDatas[i].mData = param_1;
}

void TMapCollisionBase::setAllActor(const TLiveActor* param_1)
{
	for (int i = 0; i < mCheckDataNum; ++i)
		mCheckDatas[i].mActor = param_1;
}

// fabricated
struct CollisionDataHeader {
	/* 0x0 */ u32 unk0;
	/* 0x4 */ u32 unk4;
	/* 0x8 */ u32 unk8;
	/* 0xC */ u32 unkC;
};

void TMapCollisionBase::init(const char* path, u16 param_2,
                             const TLiveActor* param_3)
{
	CollisionDataHeader* hdr = (CollisionDataHeader*)loadCollisionData(path);

#ifdef SMS_NATIVE_PLATFORM
	// The .col asset is big-endian on disc AND its per-group records store
	// 32-bit offsets in pointer-typed fields — a double problem on a 64-bit
	// LE host: the raw struct overlay misreads every u32 (header count read
	// as 0x02000000 → 33M-iteration MTXMultVecArray walk-off) AND the 0x18-byte
	// on-disc record can't overlay the 0x28-byte host struct. Fix faithfully:
	// swap a HOST COPY of the buffer (leaving the cached JKRGetResource
	// pristine — TMapCollisionData::init reads the same map.col via a stream
	// and must still see BE), then RELAYOUT the group records into a freshly-
	// allocated host array with absolute 64-bit pointers. Header @0x00: u32
	// vertCount, vecOff, groupCount, groupOff. Vec array @vecOff. Group records
	// @groupOff (groupCount x 0x18): u16 mBGType, s16 mTriangleNum, u16 mFlags,
	// u32 off8/offC/off10/off14. off8 → s16[3n] indices, offC/off10 → u8[n],
	// off14 → s16[n] (0 if absent).
	{
		const u8* be = (const u8*)hdr;
		u32 vertCount  = sb_be32(be + 0x00);
		u32 vecOff     = sb_be32(be + 0x04);
		u32 groupCount = sb_be32(be + 0x08);
		u32 groupOff   = sb_be32(be + 0x0C);

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
		for (u32 i = 0; i < vertCount * 3; ++i) sb_sw32(host + vecOff + i * 4);
		for (u32 i = 0; i < groupCount; ++i) {
			const u8* e = be + groupOff + i * 0x18;
			s32 n   = (s16)sb_be16(e + 0x02);
			u32 o8  = sb_be32(e + 0x08), o14 = sb_be32(e + 0x14);
			for (s32 j = 0; j < n * 3; ++j) sb_sw16(host + o8 + j * 2);
			if (o14) for (s32 j = 0; j < n; ++j) sb_sw16(host + o14 + j * 2);
		}

		mVertexNum         = vertCount;
		mVertices          = (Vec*)(host + vecOff);
		mCollisionGroupNum = groupCount;
		TMapCollisionGroup* arr
		    = new TMapCollisionGroup[groupCount ? groupCount : 1];
		for (u32 i = 0; i < groupCount; ++i) {
			const u8* e = be + groupOff + i * 0x18;
			arr[i].mBGType         = sb_be16(e + 0x00);
			arr[i].mTriangleNum    = (s16)sb_be16(e + 0x02);
			arr[i].mFlags          = sb_be16(e + 0x04) | WAS_PATCHED;
			arr[i].mIndices        = (s16*)(host + sb_be32(e + 0x08));
			arr[i].unkC            = (u8*)(host + sb_be32(e + 0x0C));
			arr[i].unk10           = (u8*)(host + sb_be32(e + 0x10));
			u32 o14                = sb_be32(e + 0x14);
			arr[i].mAdditionalDatas = o14 ? (s16*)(host + o14) : nullptr;
		}
		mCollisionGroups = arr;

		if (getenv("SB_COL_DBG")) {
			fprintf(stderr, "[col] vertCount=%u vecOff=0x%x groupCount=%u groupOff=0x%x\n",
			        vertCount, vecOff, groupCount, groupOff);
			u32 maxIdx = 0, badIdx = 0;
			for (u32 i = 0; i < groupCount; ++i) {
				s32 n = arr[i].mTriangleNum;
				const s16* idx = arr[i].mIndices;
				s32 lo = 0x7fff, hi = -1;
				for (s32 j = 0; j < n * 3; ++j) {
					s16 v = idx[j];
					if (v > hi) hi = v;
					if (v < lo) lo = v;
					if ((u32)v >= vertCount || v < 0) ++badIdx;
				}
				if ((u32)hi > maxIdx) maxIdx = hi;
				if (i < 4)
					fprintf(stderr, "[col]  group%u n=%d off8=0x%x idxRange[%d..%d]\n",
					        i, n, (u32)((const u8*)idx - host), lo, hi);
			}
			fprintf(stderr, "[col] groups=%u maxIdxUsed=%u (vertCount=%u) outOfRangeIdx=%u\n",
			        groupCount, maxIdx, vertCount, badIdx);
		}
		if (param_2 & 0x8000)
			onFlag(FLAG_UNK8000);
		return;
	}
#endif

	mVertexNum         = hdr->unk0;
	mVertices          = (Vec*)((u8*)hdr + hdr->unk4);
	mCollisionGroupNum = hdr->unk8;
	mCollisionGroups   = (TMapCollisionGroup*)((u8*)hdr + hdr->unkC);

	if (param_2 & 0x8000)
		onFlag(FLAG_UNK8000);

	if (!(mCollisionGroups->mFlags & WAS_PATCHED)) {
		for (s16 i = 0; i < mCollisionGroupNum; ++i) {
			mCollisionGroups[i].mIndices
			    = (s16*)((intptr_t)mCollisionGroups[i].mIndices + (u8*)hdr);
			mCollisionGroups[i].unkC
			    = (u8*)((intptr_t)mCollisionGroups[i].unkC + (u8*)hdr);
			mCollisionGroups[i].unk10
			    = (u8*)((intptr_t)mCollisionGroups[i].unk10 + (u8*)hdr);

			if (mCollisionGroups[i].mAdditionalDatas)
				mCollisionGroups[i].mAdditionalDatas
				    = (s16*)((intptr_t)mCollisionGroups[i].mAdditionalDatas
				             + (u8*)hdr);

			mCollisionGroups[i].mFlags |= WAS_PATCHED;
		}
	}
}

void TMapCollisionStatic::setUp()
{
	if (mCheckDatas)
		return;

	offFlag(FLAG_NEEDS_SETUP);

	u16 thing = 0;
	if (mVertexNum >= 350)
		thing |= 4;
	TMapCollisionBase::initCheckData(-9999, thing, mOwnerActor);
}

void TMapCollisionStatic::init(const char* param_1, u16 param_2,
                               const TLiveActor* param_3)
{
	TMapCollisionBase::init(param_1, param_2, param_3);
	mKind = KIND_STATIC;
	if (!(param_2 & 2)) {
		initCheckData(-9999, param_2, param_3);
	} else {
		mOwnerActor = param_3;
		onFlag(FLAG_NEEDS_SETUP);
	}
}

TMapCollisionStatic::TMapCollisionStatic()
    : mOwnerActor(nullptr)
{
}

void TMapCollisionMove::move()
{
	if (checkFlag(FLAG_NEEDS_SETUP))
		return;

	if (checkFlag(FLAG_UNK4000)) {
		setList();
		return;
	}

	if (checkFlag(FLAG_UNK8000)) {
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
	if (checkFlag(FLAG_NEEDS_SETUP))
		return;

	if (checkFlag(FLAG_UNK4000)) {
		setList();
		return;
	}

	TMapCollisionBase::updateTrans(param_1);
}

void TMapCollisionMove::init(u32 param_1, u16 bg_type, s16 data,
                             const TLiveActor* actor)
{
	mKind         = 1;
	mCheckDataNum = param_1;
	mCheckDatas   = gpMapCollisionData->allocCheckData(getUnkC());
	for (int i = 0; i < getUnkC(); ++i) {
		mCheckDatas[i].mBGType = bg_type;
		mCheckDatas[i].mData   = data;
		mCheckDatas[i].mActor  = actor;
	}
}

void TMapCollisionMove::init(const char* param_1, u16 param_2,
                             const TLiveActor* param_3)
{
	TMapCollisionBase::init(param_1, param_2, param_3);
	mKind = 1;
	TMapCollisionBase::initCheckData(-9999, param_2 | 2, param_3);
	onFlag(FLAG_NEEDS_SETUP);
}

TMapCollisionMove::TMapCollisionMove() { }

void TMapCollisionWarp::setUp()
{
	if (!checkFlag(FLAG_NEEDS_SETUP))
		return;

	offFlag(FLAG_NEEDS_SETUP);

	mEntryId = gpMapCollisionData->getEntryID();

	if (checkFlag(FLAG_UNK8000)) {
		JGeometry::TVec3<f32> local_18(unk20[0][3], unk20[1][3], unk20[2][3]);
		TMapCollisionBase::updateTrans(local_18);
	} else {
		TMapCollisionBase::update();
	}

	mEntrySize = gpMapCollisionData->getEntrySize(mEntryId);
}

void TMapCollisionWarp::setUpTrans(const JGeometry::TVec3<f32>& param_1)
{
	if (!checkFlag(FLAG_NEEDS_SETUP))
		return;

	offFlag(FLAG_NEEDS_SETUP);

	mEntryId = gpMapCollisionData->getEntryID();
	TMapCollisionBase::updateTrans(param_1);
	mEntrySize = gpMapCollisionData->getEntrySize(mEntryId);
}

void TMapCollisionWarp::remove()
{
	if (!isSetUp())
		return;

	onFlag(FLAG_NEEDS_SETUP);
	gpMapCollisionData->removeCheckListData(mEntryId, mEntrySize);
}

void TMapCollisionWarp::init(const char* param_1, u16 param_2,
                             const TLiveActor* param_3)
{
	TMapCollisionBase::init(param_1, param_2, param_3);
	mKind = 2;
	TMapCollisionBase::initCheckData(-9999, param_2 | 2, param_3);
	onFlag(FLAG_NEEDS_SETUP);
}

TMapCollisionWarp::TMapCollisionWarp()
    : mEntryId(0)
    , mEntrySize(0)
{
}
