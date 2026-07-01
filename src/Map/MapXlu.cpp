#include <Map/MapXlu.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif
#include <Map/MapModel.hpp>
#include <Map/Map.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

void TMapXlu::changeNormalJoint()
{
	for (int i = 0; i < gpMap->getRootJointModel()->getChildrenNum(); ++i)
		gpMap->getRootJointModel()->getChild(i)->stand();

	for (int i = 0; i < mPrioGroupNum; ++i)
		for (int j = 0; j < mPrioGroups[i].mObjectNum; ++j)
			gpMap->getRootJointModel()
			    ->getChild(mPrioGroups[i].mChildIdx[j])
			    ->getChild(mPrioGroups[i].mGrandchildIdx[j])
			    ->sit();
}

bool TMapXlu::changeXluJoint(int prio)
{
	if (prio >= mPrioGroupNum)
		return false;

	for (int i = 0; i < gpMap->getRootJointModel()->getChildrenNum(); ++i)
		gpMap->getRootJointModel()->getChild(i)->sit();

	for (int i = 0; i < mPrioGroups[prio].mObjectNum; ++i)
		gpMap->getRootJointModel()
		    ->getChild(mPrioGroups[prio].mChildIdx[i])
		    ->getChild(mPrioGroups[prio].mGrandchildIdx[i])
		    ->stand();

	return true;
}

void TMapXlu::init(JSUMemoryInputStream& stream)
{
	// The on-disc stream is BIG-ENDIAN; these are u32 scalars. The original decomp used the raw
	// read() (correct only on the GC's BE CPU); on an LE host that byteswaps the sit-joint table to
	// garbage (e.g. a count of 1 → 0x01000000) — so TMapXlu::changeNormalJoint/changeXluJoint sit the
	// WRONG (or no) map joints, leaving the translucent-priority / shine-shadow mask joints VISIBLE in
	// the normal map draw → the file-select overbright white volume. readU32() is BE on the GC (no-op
	// swap) and bswaps on LE — correct on both. Same fix pattern as TDrawBufObj::load / PerformList.
#ifdef SMS_NATIVE_PLATFORM
	if (const char* e = std::getenv("SB_XLU_DBG"); e && e[0] && e[0] != '0') {
		int pos = stream.getPosition(), len = stream.getLength();
		const unsigned char* cur = (const unsigned char*)stream.getCurrent();
		char hx[128]; int hn = 0;
		for (int k = 0; k < 32 && pos + k < len; ++k)
			hn += std::snprintf(hx + hn, sizeof(hx) - hn, "%02x", cur[k]);
		std::fprintf(stderr, "[xlu-init] pos=%d len=%d bytes=%s\n", pos, len, hx);
	}
#endif
	unk0 = (int)stream.readU32();
#ifdef SMS_NATIVE_PLATFORM
	if (const char* e = std::getenv("SB_XLU_DBG"); e && e[0] && e[0] != '0')
		std::fprintf(stderr, "[xlu-init] unk0(numGroups)=%d\n", unk0);
#endif
	if (unk0 != 0) {
		unk4 = new Entry[unk0];
		for (int i = 0; i < unk0; ++i) {
			Entry& entry = unk4[i];
			entry.unk0 = (int)stream.readU32();
			entry.unk4 = new u32[entry.unk0];
			entry.unk8 = new u32[entry.unk0];
			for (int j = 0; j < entry.unk0; ++j) {
				entry.unk4[j] = stream.readU32();
				entry.unk8[j] = stream.readU32();
			}
		}
	}
}

TMapXlu::TMapXlu()
    : mPrioGroupNum(0)
    , mPrioGroups(nullptr)
{
}
