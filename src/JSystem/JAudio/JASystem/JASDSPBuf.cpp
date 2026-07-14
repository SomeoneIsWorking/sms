#include <JSystem/JAudio/JASystem/JASDSPBuf.hpp>
#include <JSystem/JAudio/JASystem/JASProbe.hpp>
#include <JSystem/JAudio/JASystem/JASDSPInterface.hpp>
#include <JSystem/JAudio/JASystem/JASCallback.hpp>
#include <JSystem/JAudio/JASystem/JASDSPChannel.hpp>
#include <JSystem/JAudio/JASystem/JASRate.hpp>
#include <JSystem/JAudio/JASystem/JASSystemHeap.hpp>
#include <JSystem/JAudio/JASystem/JASAudioThread.hpp>
#include <JSystem/dspproc.h>
#include <JSystem/dsptask.h>
#include <dolphin/os/OSCache.h>

namespace JASystem {

namespace DSPBuf {
	static s16* dsp_buf[3];

	static u8 write_buffer      = 0;
	static u8 read_buffer       = 0;
	static u8 dspstatus         = 0;
	static int dac_sync_counter = 0;
} // namespace DSPBuf

void DSPBuf::updateDSP()
{
	++dac_sync_counter;
	Kernel::probeStart(3, "SFR-UPDATE");
	DSPInterface::invalChannelAll();
	Kernel::subframeCallback();
	TDSPChannel::updateAll();
	DSPReleaseHalt();
	Kernel::aiCallback();
	Kernel::probeFinish(3);
}

// Missing-return decomp gap (same bug class as the setupThreadFunc -O2
// fallthrough, debug_journal/2026-07-*): on PPC, process()'s return value is
// already sitting in r3 when mixDSP() falls off the end, so it reads as a tail
// call by ABI accident and the original build "works". Under a host x86-64
// compiler that is undefined behavior — verified live: Release build returned a
// garbage s16* here, which vframeWork() then interleaved into dac[], SIGSEGV in
// DSPBuf::mixDSP. process()'s DSPBUF_EVENTS_UNK2 branch is the only one that
// returns a real value (the two others return nullptr, matching mixDSP's own
// implicit-null-on-fallthrough result for those cases anyway).
s16* DSPBuf::mixDSP(s32 param) { return process(DSPBUF_EVENTS_UNK2); }

void DSPBuf::finishDSPFrame() { process(DSPBUF_EVENTS_UNK1); }

s16* DSPBuf::process(DSPBUF_EVENTS event)
{
	u32 samples = Kernel::getFrameSamples();
	switch (event) {
	case DSPBUF_EVENTS_UNK0: {
		write_buffer = 2;
		read_buffer  = 0;
		for (u32 i = 0; i < 3; ++i) {
			dsp_buf[i]
			    = (s16*)Kernel::allocFromSysDram(samples * 2 * sizeof(s16));
			for (u32 j = 0; j < 2 * samples; ++j)
				dsp_buf[i][j] = 0;
			DCStoreRange(dsp_buf[i], samples * 2 * sizeof(s16));
		}
		dspstatus = 0;
		break;
	}
	case DSPBUF_EVENTS_UNK1: {
		u8 nextWriteBuf  = write_buffer + 1;
		u32 frameSamples = Kernel::getFrameSamples();
		if (nextWriteBuf == 3)
			nextWriteBuf = 0;

		if (nextWriteBuf == read_buffer) {
			dspstatus = 0;
		} else {
			write_buffer = nextWriteBuf;
			AudioThread::setDSPSyncCount(Kernel::getSubFrames());
			Kernel::probeStart(7, "DSP-MAIN");
			// bufL/bufR are live host pointers into dsp_buf — pass full pointer
			// width (see JSystem/dsptask.h). The original PPC code narrowed these
			// to u32 registers (correct on 32-bit GC hardware); on an LP64 host a
			// u32 cast here truncates the real heap address and DsyncFrame2
			// dereferences garbage (verified crash: SIGSEGV in DsyncFrame2's
			// memset, fault address 0xf06cf500 — the low 32 bits of a real >32-bit
			// heap pointer).
			DsyncFrame2(Kernel::getSubFrames(), (uintptr_t)dsp_buf[write_buffer],
			            (uintptr_t)&dsp_buf[write_buffer][frameSamples]);
			dspstatus = 1;
			updateDSP();
		}
		break;
	}
	case DSPBUF_EVENTS_UNK2: {
		u8 nextReadBuf   = read_buffer + 1;
		u32 frameSamples = Kernel::getFrameSamples();
		if (nextReadBuf == 3)
			nextReadBuf = 0;

		if (nextReadBuf == write_buffer) {
			u32 i;

			s16 var1 = dsp_buf[read_buffer][frameSamples / 2 - 1];
			s16 var2 = dsp_buf[read_buffer][frameSamples - 1];

			for (i = 0; i < frameSamples; ++i)
				dsp_buf[read_buffer][i] = var1;
			for (i = frameSamples; i < frameSamples * 2; ++i)
				dsp_buf[read_buffer][i] = var2;

			if (dspstatus == 0)
				finishDSPFrame();
		} else {
			read_buffer = nextReadBuf;
			DCInvalidateRange(dsp_buf[read_buffer],
			                  frameSamples * 2 * sizeof(s16));
			if (dspstatus == 0)
				finishDSPFrame();
		}

		return dsp_buf[read_buffer];
	}
	}

	return nullptr;
}

} // namespace JASystem
