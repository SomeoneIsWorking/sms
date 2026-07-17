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
	s32 tmp;
	stream >> tmp;
	mPrioGroupNum = tmp;
#ifdef SMS_NATIVE_PLATFORM
	if (const char* e = std::getenv("SB_XLU_DBG"); e && e[0] && e[0] != '0')
		std::fprintf(stderr, "[xlu-init] mPrioGroupNum=%d\n", mPrioGroupNum);
#endif
	if (mPrioGroupNum != 0) {
		mPrioGroups = new TXluPrioGroup[mPrioGroupNum];
		for (int i = 0; i < mPrioGroupNum; ++i) {
			TXluPrioGroup& entry = mPrioGroups[i];
			stream >> tmp;
			entry.mObjectNum     = tmp;
			entry.mChildIdx      = new u32[entry.mObjectNum];
			entry.mGrandchildIdx = new u32[entry.mObjectNum];
			for (int j = 0; j < entry.mObjectNum; ++j) {
				stream >> tmp;
				entry.mChildIdx[j] = tmp;
				stream >> tmp;
				entry.mGrandchildIdx[j] = tmp;
			}
		}
	}
}

TMapXlu::TMapXlu()
    : mPrioGroupNum(0)
    , mPrioGroups(nullptr)
{
}
