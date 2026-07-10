#include <GC2D/MessageLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp> // JSU_BE32
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>

TMessageLoader::TMessageLoader()
    : unk0(0)
    , unk4(0)
{
}

TMessageLoader::TMessageLoader(const char* param_1)
    : unk0(0)
    , unk4(0)
{
	u8* res = (u8*)JKRGetResource(param_1);
	if (res) {
		u32 a;
		u32 b;
		readHeader(&a, &b, res);
		unk4 = parseBlock(a, b, res + 0x20);
		// NOTE: assert but in an if?
		if (unk4)
			(void)unk4;
	}
}

u32 TMessageLoader::loadMessageData(const char* param_1)
{
	u8* res = (u8*)JKRGetResource(param_1);
	if (!res)
		return -1;

	// BMG header dwords are big-endian on disc; raw pointer reads don't swap.
	unk4 = parseBlock((int)JSU_BE32(*(u32*)(res + 0x8)) * 32,
	                  (int)JSU_BE32(*(u32*)(res + 0xC)), res + 0x20);
	if (!unk4)
		return -1;

	return unk2;
}

void TMessageLoader::readHeader(u32* a, u32* b, void* header)
{
	u32* casted = (u32*)header;

	// BMG header dwords are big-endian on disc; raw pointer reads don't swap.
	*a = JSU_BE32(*(casted + 2)) * 32;
	*b = JSU_BE32(*(casted + 3));
}

void* TMessageLoader::parseBlock(u32 param_1, u32 param_2, void* param_3)
{
	JSUMemoryInputStream local_5c(param_3, param_1);

	void* result;

	for (int i = 0; i < param_2; ++i) {
		int local_74;
		local_5c.read(&local_74, 4);
		// Block FourCC ('INF1'/'DAT1'/'STR1') is stored big-endian; the raw
		// read(&x,4) does not swap, so on a LE host every case missed and the
		// parse fell through to default (JSU raw-read class).
		local_74 = (int)JSU_BE32((u32)local_74);

		int local_70;

		switch (local_74) {
		case 'INF1': {
			local_70 = readInfoBlock(local_5c.getCurrent());
			local_5c.skip(4);
			break;
		}

		case 'DAT1':
			local_5c.read(&local_70, 4);
			// BE block size, raw read -> swap.
			local_70 = (int)JSU_BE32((u32)local_70);
			result   = local_5c.getCurrent();
			break;

		case 'STR1':
			local_70 = 0;
			break;

		default:
			local_70 = 0;
			break;
		}

		local_5c.skip(local_70 - 8);
	}

	return result;
}

TMessageLoader::EntryInfo* TMessageLoader::getMessageEntry(u32 param_1)
{
	EntryInfo* result;
	if (u16(param_1) >= unk0)
		result = nullptr;
	else
		result = &unk8[param_1];

	return result;
}

int TMessageLoader::readInfoBlock(void* data)
{
	int length = *(int*)data;
	// BE block length via raw pointer read -> swap.
	length = (int)JSU_BE32((u32)length);
	data   = (u8*)data + 4;
	JSUMemoryInputStream local_38(data, length - 8);
	local_38.read(&unk0, 2);
	// BE u16 message count, raw read -> swap (JSU raw-read class).
	unk0 = JSU_BE16(unk0);
	local_38.readU16();
	unk2 = local_38.readU16();
	local_38.skip(2);

	for (int i = 0; i < unk0; ++i) {
		local_38.read(&unk8[i], sizeof(EntryInfo));
		// EntryInfo scalars (u32 data offset + two s16 frame bounds) are BE on
		// disc and were read raw -> swap each; unk8[] name bytes stay raw.
		unk8[i].unk0 = JSU_BE32(unk8[i].unk0);
		unk8[i].unk4 = (s16)JSU_BE16((u16)unk8[i].unk4);
		unk8[i].unk6 = (s16)JSU_BE16((u16)unk8[i].unk6);
	}

	return length;
}
