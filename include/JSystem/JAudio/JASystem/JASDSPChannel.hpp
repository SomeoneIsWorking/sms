#ifndef JASDSPCHANNEL_HPP
#define JASDSPCHANNEL_HPP

#include <dolphin/types.h>
#include <types.h>

namespace JASystem {

namespace DSPInterface {
	class DSPBuffer;
}

class TChannel;

class TDSPChannel {
private:
public:
	TDSPChannel()
	    : unkC(nullptr)
	    , unk10(nullptr)
	{
	}
	~TDSPChannel() { }

	static void initAll();
	static void updateAll();
	// param2/param carries a TChannel* (the logical-channel back-link stored in
	// unk8). LP64: it MUST be pointer-width — the decomp typed it u32 (fine on
	// 32-bit GC), which truncates a 64-bit host pointer and makes
	// getLogicalChannel() return garbage -> SIGSEGV in updatecallDSPChannel once
	// the DSP pipeline is actually driven (2026-07-17). uintptr_t == u32 on GC.
	static TDSPChannel* alloc(u32 param1, uintptr_t param2);
	static int free(TDSPChannel* channel, uintptr_t param);
	TDSPChannel* getHandle(u32 handle);
	u32 getNumUse();
	u32 getNumFree();
	void setLimitDSP(f32 limit);
	f32* getHistory();

	void init(u8 param);
	BOOL allocate(uintptr_t param); // LP64: holds a TChannel* (see alloc/free above)
	void free();
	void play();
	void stop();
	void pause();
	void restart();
	bool forceStop();
	void forceDelete();
	static JASystem::TDSPChannel* getLower();
	static JASystem::TDSPChannel* getLowerActive();
	static BOOL breakLower(u8 param);
	static BOOL breakLowerActive(u8 param);

	BOOL isUnk1One() const { return unk1 == 1 ? TRUE : FALSE; }

	static TDSPChannel* DSPCH;
	static u32 smnUse;
	static u32 smnFree;

	// fake, stolen from tww
	TChannel* getLogicalChannel()
	{
		if (unk10 != nullptr) {
			return (TChannel*)unk8; // (TWW) ?? is this userdata?
		} else {
			return nullptr;
		}
	}

public:
	/* 0x0 */ u8 unk0;
	/* 0x0 */ u8 unk1;
	/* 0x0 */ u8 unk2;
	/* 0x0 */ u8 unk3;
	/* 0x4 */ u16 unk4;
	/* 0x6 */ u16 unk6;
	/* 0x8 */ uintptr_t unk8; // LP64: holds a TChannel* back-link (was u32 = truncating on host)
	/* 0xC */ DSPInterface::DSPBuffer* unkC;
	/* 0x10 */ int (*unk10)(TDSPChannel*, u32);
};

} // namespace JASystem

#endif // JASDSPCHANNEL_HPP
