#include <Map/MapXlu.hpp>
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

	for (int i = 0; i < unk0; ++i)
		for (int j = 0; j < unk4[i].unk0; ++j)
			gpMap->getRootJointModel()
			    ->getChild(unk4[i].unk4[j])
			    ->getChild(unk4[i].unk8[j])
			    ->sit();
}

bool TMapXlu::changeXluJoint(int n)
{
	if (n >= unk0)
		return false;

	for (int i = 0; i < gpMap->getRootJointModel()->getChildrenNum(); ++i)
		gpMap->getRootJointModel()->getChild(i)->sit();

	for (int i = 0; i < unk4[n].unk0; ++i)
		gpMap->getRootJointModel()
		    ->getChild(unk4[n].unk4[i])
		    ->getChild(unk4[n].unk8[i])
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
	unk0 = (int)stream.readU32();
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
    : unk0(0)
    , unk4(nullptr)
{
}
