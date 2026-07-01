#include <JSystem/JAudio/JASystem/JASWaveBankMgr.hpp>
#include <JSystem/JAudio/JASystem/JASSystemHeap.hpp>
#include <JSystem/JAudio/JASystem/JASCalc.hpp>
#include <JSystem/JAudio/JASystem/JASWSParser.hpp>
#include <JSystem/JAudio/JASystem/JASWaveBank.hpp>
#include <JSystem/JAudio/JASystem/JASBasicWaveBank.hpp>
#include <JSystem/JAudio/JASystem/JASSimpleWaveBank.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>

#ifdef SMS_NATIVE_PLATFORM
#include <JSystem/sb_host_swapset.h>
// =============================================================================
// Native PC build: WSYS wave-bank blobs (AAF chunk id 3) are GC BIG-ENDIAN, but
// WSParser reads them by casting raw bytes to host structs via
// JSUConvertOffsetToPtr (offsets/counts are u32, params are u32/s16/f32) -> every
// field is misread on a little-endian host (header_length 0x20 -> 0x20000000 ->
// wild pointer). The WSYS structs carry NO inline runtime pointers (only file
// offsets), so unlike RARC we can swap the whole metadata graph IN PLACE. Wave
// PCM/AFC data is not in this blob (it lives in the .aw arcs), so nothing else
// needs converting here.
//   THeader:        +0x10 WINF off, +0x14 WBCT off
//   WBCT(TCtrlGroup): +0x08 group count, +0x0C scene-offset[] (one per group)
//   SCNE(TCtrlScene): +0x0C ctrl off
//   C-DF(TCtrl):      +0x04 wave count, +0x08 ctrl-wave-offset[]
//   TCtrlWave:        +0x00 packed id/size
//   WINF(TWaveArchiveBank): +0x08 archive-offset[] (one per group)
//   TWaveArchive:     name[0x70], +0x70 wave count, +0x74 wave-offset[]
//   TWave:            +0x04 f32 rate, +0x08..+0x1C u32, +0x20/+0x22 s16, +0x28 s32
// =============================================================================
namespace {
inline void wsys_sw16(void* p) { u8* b = (u8*)p; u8 t = b[0]; b[0] = b[1]; b[1] = t; }
inline void wsys_sw32(void* p)
{
	u8* b = (u8*)p;
	u8 t;
	t = b[0]; b[0] = b[3]; b[3] = t;
	t = b[1]; b[1] = b[2]; b[2] = t;
}
inline u32 wsys_h32(const void* p) { return *(const u32*)p; } // host read AFTER swap

// Swap a WSYS blob to host endian in place. Idempotent per blob pointer.
void sb_wsys_swap_to_host(void* data)
{
	// Host-malloc-backed (see JSystem/sb_host_swapset.h): a plain static std::set's nodes
	// would be JKR-arena-allocated on the game thread and freed by per-scene freeAll ->
	// dangling -> crash on a later insert.
	static smsport::HostPtrSet swapped;
	if (!data || swapped.count(data))
		return;
	swapped.insert(data);

	u8* base = (u8*)data;
	wsys_sw32(base + 0x10); // WINF offset
	wsys_sw32(base + 0x14); // WBCT offset
	u32 winf = wsys_h32(base + 0x10);
	u32 wbct = wsys_h32(base + 0x14);

	wsys_sw32(base + wbct + 0x08); // group count
	u32 groupCount = wsys_h32(base + wbct + 0x08);
	for (u32 g = 0; g < groupCount; ++g)
		wsys_sw32(base + wbct + 0x0C + g * 4);

	wsys_sw32(base + winf + 0x04); // archive count (parallel to group count)
	for (u32 g = 0; g < groupCount; ++g)
		wsys_sw32(base + winf + 0x08 + g * 4);

	for (u32 g = 0; g < groupCount; ++g) {
		u32 scene = wsys_h32(base + wbct + 0x0C + g * 4);
		wsys_sw32(base + scene + 0x0C);
		u32 ctrl = wsys_h32(base + scene + 0x0C);
		wsys_sw32(base + ctrl + 0x04);
		u32 waveCount = wsys_h32(base + ctrl + 0x04);
		for (u32 w = 0; w < waveCount; ++w) {
			wsys_sw32(base + ctrl + 0x08 + w * 4);
			u32 cw = wsys_h32(base + ctrl + 0x08 + w * 4);
			wsys_sw32(base + cw + 0x00); // TCtrlWave.unk0
		}
		u32 arc = wsys_h32(base + winf + 0x08 + g * 4);
		wsys_sw32(base + arc + 0x70); // per-archive wave count
		for (u32 w = 0; w < waveCount; ++w) {
			wsys_sw32(base + arc + 0x74 + w * 4);
			u32 wav = wsys_h32(base + arc + 0x74 + w * 4);
			wsys_sw32(base + wav + 0x04); // f32 rate
			wsys_sw32(base + wav + 0x08); // mOffset
			wsys_sw32(base + wav + 0x0C);
			wsys_sw32(base + wav + 0x10);
			wsys_sw32(base + wav + 0x14);
			wsys_sw32(base + wav + 0x18);
			wsys_sw32(base + wav + 0x1C);
			wsys_sw16(base + wav + 0x20);
			wsys_sw16(base + wav + 0x22);
			wsys_sw32(base + wav + 0x28);
		}
	}
}
} // namespace
#endif

namespace JASystem {

int WaveBankMgr::sTableSize        = 0;
TWaveBank** WaveBankMgr::sWaveBank = 0;

void WaveBankMgr::init(int tableSize)
{
	sWaveBank = new (JASDram, 0) TWaveBank*[tableSize];
	Calc::bzero(sWaveBank, tableSize * sizeof(TWaveBank*));
	sTableSize = tableSize;
}

TWaveBank* WaveBankMgr::getWaveBank(int bankIndex)
{
	return sWaveBank[bankIndex];
}

bool WaveBankMgr::registWaveBank(int bankIndex, TWaveBank* waveBank)
{
	sWaveBank[bankIndex] = waveBank;
	return true;
}

bool WaveBankMgr::registWaveBankWS(int bankIndex, void* waveBankData)
{
#ifdef SMS_NATIVE_PLATFORM
	sb_wsys_swap_to_host(waveBankData);
#endif
	TWaveBank* bank;
	if (WSParser::getGroupCount(waveBankData) == 1)
		bank = WSParser::createSimpleWaveBank(waveBankData);
	else
		bank = WSParser::createBasicWaveBank(waveBankData);

	if (!bank)
		return false;

	return registWaveBank(bankIndex, bank);
}

bool WaveBankMgr::loadWave(int bankIndex, int waveIndex)
{
	TWaveBank* bank = getWaveBank(bankIndex);
	if (!bank)
		return false;

	if (bank->getType() == 'BSIC') {
		TBasicWaveBank::TWaveGroup* group
		    = ((TBasicWaveBank*)bank)->getWaveGroup(waveIndex);
		if (!group)
			return false;

		if (!WaveArcLoader::loadWave(group))
			return false;

		((TBasicWaveBank*)bank)->incWaveTable(group);

		return true;
	} else if (bank->getType() == 'SMPL') {
		TSimpleWaveBank* casted = (TSimpleWaveBank*)bank;
		return WaveArcLoader::loadWave(casted);
	}

	return false;
}

bool WaveBankMgr::eraseWave(int bankIndex, int waveIndex)
{

	TWaveBank* bank = getWaveBank(bankIndex);
	if (!bank)
		return false;

	if (bank->getType() == 'BSIC') {
		TBasicWaveBank::TWaveGroup* group
		    = ((TBasicWaveBank*)bank)->getWaveGroup(waveIndex);
		if (!group)
			return false;

		if (!WaveArcLoader::eraseWave(group))
			return false;

		((TBasicWaveBank*)bank)->decWaveTable(group);
		return true;
	} else if (bank->getType() == 'SMPL') {
		TSimpleWaveBank* casted = (TSimpleWaveBank*)bank;
		return WaveArcLoader::eraseWave(casted);
	}

	return false;
}

u32 WaveBankMgr::getUsedHeapSize() { return 0; }

} // namespace JASystem
