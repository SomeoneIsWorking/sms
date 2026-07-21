#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <macros.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

TBGCheckData TMapCollisionData::mIllegalCheckData;
TMapCollisionData* gpMapCollisionData;

TBGCheckList::TBGCheckList()
    : mNext(nullptr)
    , unk8(0)
{
}

// TODO: inline deferred or nah?
TBGCheckListWarp::TBGCheckListWarp()
    : unkC(0)
    , unk10(0)
    , unk12(0)
{
}

void TMapCollisionData::initGrid(TBGCheckListRoot* roots)
{
	int x = unk10;
	while (x--) {
		roots->unk0[0].setNext(nullptr);
		roots->unk0[1].setNext(nullptr);
		roots->unk0[2].setNext(nullptr);
		roots++;
	}
}

void TMapCollisionData::initMoveCollision()
{
	unk3C = unk20 - 1;
	memset(unk18, 0, unk10 * sizeof(*unk18));
}

void TMapCollisionData::initAllCheckDataAndList() { }

void TMapCollisionData::init(JSUMemoryInputStream& stream)
{
	s32 value;
	stream >> value;
	unk8 = value;
	stream >> value;
	unkC = value;
	stream >> value;
	unk1C = value;
	stream >> value;
	unk20 = value;
	stream >> value;
	unk24 = value;

	// NOTE (2026-07-21, corrected): there used to be an explicit
	// __builtin_bswap32() on unk8/unkC/unk1C/unk20/unk24 here, justified by "the
	// raw read(&value,4) above copies bytes without swapping". That justification
	// is FALSE for the code above — these are typed `stream >> value` reads, and
	// JSUInputStream::operator>>(s32&) already byteswaps BE asset bytes to host
	// endian (see JSUInputStream.hpp). The extra swap was therefore a DOUBLE swap
	// that put the counts back into big-endian, so unk1C/unk20/unk24 became huge
	// garbage and `new TBGCheckData[unk1C]` walked off the heap and died in the
	// element ctor — the exact failure the old comment described, caused by its
	// own "fix". Removed; operator>> is the single swap point.

	mGridExtentX = (unk8 / 2) * 1024.0f;
	mGridExtentY = (unkC / 2) * 1024.0f;

	unk10 = unk8 * unkC;

	unk14 = new TBGCheckListRoot[unk10];
	unk18 = new TBGCheckListRoot[unk10];
	unk28 = new TBGCheckData[unk1C];
	unk2C = new TBGCheckList[unk20];
	unk30 = new TBGCheckListWarp[unk24];

	initGrid(unk14);

	unk34 = 0;
	unk38 = 0;
	unk40 = 0;

	initMoveCollision();

	for (int i = 0; i < 256; ++i)
		unk42[i] = 9999;
}

TMapCollisionData::TMapCollisionData()
    : mGridExtentX(0.0f)
    , mGridExtentY(0.0f)
    , unk8(0)
    , unkC(0)
    , unk10(0)
    , unk14(nullptr)
    , unk18(nullptr)
    , unk1C(0)
    , unk20(0)
    , unk24(0)
    , unk28(nullptr)
    , unk2C(nullptr)
    , unk30(nullptr)
    , unk34(0)
    , unk38(0)
    , unk3C(0)
    , unk40(0)
    , unk242(0)
    , mGroundPlane(nullptr)
{
	JGeometry::TVec3<float> v1(-32767.0f, -32767.0f, -32767.0f);
	JGeometry::TVec3<float> v2(-32767.0f, -32767.0f, 32767.0f);
	JGeometry::TVec3<float> v3(32767.0f, -32767.0f, 32767.0f);
	mIllegalCheckData.setVertex(v1, v2, v3);
	mIllegalCheckData.mBGType = 0;
	mIllegalCheckData.mFlags |= BG_CHECK_FLAG_ILLEGAL;
	gpMapCollisionData = this;
}
