#include <GC2D/MessageUtil.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif

void SMSMakeTextBuffer(J2DTextBox* param_1, int param_2)
{
#ifdef SMS_NATIVE_PLATFORM
	if (!param_1) // region-tolerant: textbox pane absent in a US (GMSE01) UI archive
		return;
#endif
	char* buffer = new char[param_2];
	for (int i = 0; i < param_2 - 1; ++i)
		buffer[i] = ' ';
	buffer[param_2 - 1] = '\0';
	param_1->setString(buffer);
}

const char* SMSGetMessageData(void* param_1, u32 param_2)
{
	if (!param_1)
		return nullptr;

	int local_88 = 0;
	int local_84 = 0;

	{
		JSUMemoryInputStream local_40(param_1, 0x20);
		local_40.skip(8);
		local_40.read(&local_88, 4);
		local_40.read(&local_84, 4);
	}

#ifdef SMS_NATIVE_PLATFORM
	// The BMG header dwords at [0x08] (block count) and [0x0C] (section count) are
	// stored BIG-ENDIAN. The decomp reads them RAW (correct on the GameCube's BE CPU);
	// on a little-endian host the raw value is byteswapped, and the wrong local_88 makes
	// the section-stream length `local_88 * 0x20 - 0x20` overflow to <=0 -> the stream
	// is empty -> the INF1 walk never runs -> every message lookup returns null (the
	// "Select data" banner / file-slot labels never resolve). Byteswap to match BE.
	local_88 = (int)__builtin_bswap32((u32)local_88);
	local_84 = (int)__builtin_bswap32((u32)local_84);
#endif

#ifdef SMS_NATIVE_PLATFORM
	static int s_msgdbg = -1;
	if (s_msgdbg < 0) { const char* e = getenv("SB_MSG_DBG"); s_msgdbg = (e && e[0] && e[0] != '0') ? 8 : 0; }
	const bool msgdbg = s_msgdbg > 0;
	if (msgdbg) {
		const u8* hb = (const u8*)param_1;
		fprintf(stderr, "[msgdata] bmg=%p hdr='%c%c%c%c%c%c%c%c' raw88=%d(0x%x) raw84=%d(0x%x) want=0x%x\n",
		        param_1, hb[0],hb[1],hb[2],hb[3],hb[4],hb[5],hb[6],hb[7],
		        local_88, local_88, local_84, local_84, param_2);
	}
#endif

	char trash2[0x4];

	int r30      = 0;
	u32 local_68 = 0;

	JSUMemoryInputStream local_74((u8*)param_1 + 0x20, local_88 * 0x20 - 0x20);

	const char* r31 = nullptr;

	while ((r30 == 0 || local_68 == 0) && local_74.isNotDrained()) {
		int iVar3 = local_74.readS32();
		s32 r27   = local_74.readS32();
		switch (iVar3) {
		case 'INF1': {
			u16 inf_count = local_74.readU16();
#ifdef SMS_NATIVE_PLATFORM
			if (msgdbg) fprintf(stderr, "[msgdata]   INF1 r27=%d count=%u want=0x%x\n",
			                    r27, inf_count, param_2);
#endif
			if (param_2 >= inf_count)
				return nullptr;

			u32 r24 = local_74.readU16();
			local_74.skip(4);
			r24 = param_2 * r24;
			local_74.skip(r24);
			local_74.read(&local_68, 4);
#ifdef SMS_NATIVE_PLATFORM
			// Per-message DAT1 offset is a BE u32 read raw — byteswap on a LE host.
			local_68 = __builtin_bswap32(local_68);
			if (msgdbg) fprintf(stderr, "[msgdata]   INF1 entrySize-derived r24=%u local_68(off)=0x%x\n",
			                    r24, local_68);
#endif
			if (!local_68)
				return nullptr;
			local_74.skip(r27 - r24 - 0x14);
			break;
		}

		case 'DAT1':
			// TODO: using the virtual method and letting it devirtualize
			// results in too much stack... one more getter? Or s32/int memes?
			r30 = local_74.mPosition;
			local_74.skip(r27 - 8);
			break;

		default:
			local_74.skip(r27 - 8);
			break;
		}
	}

	char trash[0x4];

	if (r30 != 0 && local_68 != 0)
		r31 = (const char*)param_1 + r30 + local_68 + 0x20;

	return r31;
}
