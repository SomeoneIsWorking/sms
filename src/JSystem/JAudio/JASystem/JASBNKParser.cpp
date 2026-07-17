#include <JSystem/JAudio/JASystem/JASBNKParser.hpp>
#include <JSystem/JAudio/JASystem/JASBasicInst.hpp>
#include <JSystem/JAudio/JASystem/JASCalc.hpp>
#include <JSystem/JAudio/JASystem/JASInstRand.hpp>
#include <JSystem/JAudio/JASystem/JASInstSense.hpp>
#include <JSystem/JAudio/JASystem/JASBasicInst.hpp>
#include <JSystem/JAudio/JASystem/JASDrumSet.hpp>
#include <JSystem/JSupport.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <JSystem/sb_host_swapset.h>
#endif

namespace JASystem {

namespace BNKParser {

	u32 sUsedHeapSize = 0;

#ifdef SMS_NATIVE_PLATFORM
	// IBNK (instrument bank) is a big-endian format read through the typed structs
	// above (THeader/TInst/TOsc/TKeymap/TVmap/TPerc/TPmap/TRand/TSense). On an LE host
	// every multi-byte field must be byte-swapped in place before createBasicBank reads
	// it — the analogue of sb_wsys_swap_to_host for WSYS (JASWaveBankMgr.cpp). This is
	// the "IBNK BE swap" the cid-2 wiring was waiting on (JAIBasic.cpp:253-258).
	// Position-aware: swap an offset, then follow it. Oscillators (and other sub-structs)
	// are SHARED across instruments (findOscPtr dedups by pointer), so each followed
	// pointer is swapped at most once (tracked in `done`) — double-swapping would undo it.
	namespace {
		inline void ib16(void* p) { u8* b = (u8*)p; u8 t = b[0]; b[0] = b[1]; b[1] = t; }
		inline void ib32(void* p) { u8* b = (u8*)p; u8 t0 = b[0], t1 = b[1]; b[0] = b[3]; b[1] = b[2]; b[2] = t1; b[3] = t0; }
		inline u32  ibh32(const void* p) { return *(const u32*)p; } // read AFTER swap

		// swap one osc-envelope table: s16 triples (mode,time,value) until mode > 0xa
		// inclusive (getOscTableEndPtr semantics). mode must be swapped before testing.
		void swapOscTable(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* t = base + off;
			if (done.count(t)) return;
			done.insert(t);
			for (;;) {
				ib16(t + 0); // mode
				s16 mode = *(s16*)(t + 0);
				ib16(t + 2); // time
				ib16(t + 4); // value
				t += 6;
				if (mode > 0xa) break;
			}
		}
		void swapOsc(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* o = base + off;
			if (done.count(o)) return;
			done.insert(o);
			// TOsc: unk0 u8 @0; unk4 f32 @4; unk8/unkC u32 tableoff @8/@C; unk10/unk14 f32 @10/@14
			ib32(o + 0x04);
			ib32(o + 0x08);
			ib32(o + 0x0C);
			ib32(o + 0x10);
			ib32(o + 0x14);
			swapOscTable(base, ibh32(o + 0x08), done);
			swapOscTable(base, ibh32(o + 0x0C), done);
		}
		void swapRand(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* r = base + off;
			if (done.count(r)) return;
			done.insert(r);
			ib32(r + 0x04); // unk4 f32
			ib32(r + 0x08); // unk8 f32
		}
		void swapSense(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* s = base + off;
			if (done.count(s)) return;
			done.insert(s);
			ib32(s + 0x04); // unk4 f32
			ib32(s + 0x08); // unk8 f32
		}
		void swapVmap(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* v = base + off;
			if (done.count(v)) return;
			done.insert(v);
			ib32(v + 0x04); // unk4 u32
			ib32(v + 0x08); // unk8 f32
			ib32(v + 0x0C); // unkC f32
		}
		void swapKeymap(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* k = base + off;
			if (done.count(k)) return;
			done.insert(k);
			ib32(k + 0x04); // unk4 = velo-region count
			u32 n = ibh32(k + 0x04);
			for (u32 j = 0; j < n; ++j) {
				ib32(k + 0x08 + j * 4); // mVmapOffsets[j]
				swapVmap(base, ibh32(k + 0x08 + j * 4), done);
			}
		}
		void swapInst(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* in = base + off;
			if (done.count(in)) return;
			done.insert(in);
			ib32(in + 0x08); // unk8 f32 (vol)
			ib32(in + 0x0C); // unkC f32 (pitch)
			for (int j = 0; j < 2; ++j) { ib32(in + 0x10 + j * 4); swapOsc(base, ibh32(in + 0x10 + j * 4), done); }
			for (int j = 0; j < 2; ++j) { ib32(in + 0x18 + j * 4); swapRand(base, ibh32(in + 0x18 + j * 4), done); }
			for (int j = 0; j < 2; ++j) { ib32(in + 0x20 + j * 4); swapSense(base, ibh32(in + 0x20 + j * 4), done); }
			ib32(in + 0x28); // mKeyRegionCount
			u32 kr = ibh32(in + 0x28);
			for (u32 j = 0; j < kr; ++j) { ib32(in + 0x2C + j * 4); swapKeymap(base, ibh32(in + 0x2C + j * 4), done); }
		}
		void swapPmap(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* pm = base + off;
			if (done.count(pm)) return;
			done.insert(pm);
			ib32(pm + 0x00); // unk0 f32
			ib32(pm + 0x04); // unk4 f32
			for (int k = 0; k < 2; ++k) { ib32(pm + 0x08 + k * 4); swapRand(base, ibh32(pm + 0x08 + k * 4), done); }
			ib32(pm + 0x10); // mVeloRegionCount
			u32 vr = ibh32(pm + 0x10);
			for (u32 k = 0; k < vr; ++k) { ib32(pm + 0x14 + k * 4); swapVmap(base, ibh32(pm + 0x14 + k * 4), done); }
		}
		void swapPerc(u8* base, u32 off, smsport::HostPtrSet& done) {
			if (off == 0) return;
			u8* pc = base + off;
			if (done.count(pc)) return;
			done.insert(pc);
			ib32(pc + 0x000); // mMagic (compared to 'PER2' as host u32 after swap)
			for (int j = 0; j < 0x80; ++j) { ib32(pc + 0x088 + j * 4); swapPmap(base, ibh32(pc + 0x088 + j * 4), done); }
			// unk288[0x80] = s8 (no swap); unk308[0x80] = u16 release
			for (int j = 0; j < 0x80; ++j) ib16(pc + 0x308 + j * 2);
		}
	} // namespace

	void sb_ibnk_swap_to_host(void* data) {
		static smsport::HostPtrSet blobSwapped;
		if (!data || blobSwapped.count(data)) return;
		blobSwapped.insert(data);
		u8* base = (u8*)data;
		smsport::HostPtrSet done; // per-blob shared-substruct dedup
		// Header virtual-bank number @0x08 — registBankBNK reads *((u32*)data+2) for
		// setVir2PhyTable BEFORE parsing, so it must be host-order too.
		ib32(base + 0x08);
		// THeader: mInstOffsets[0x80] @0x024, mPercOffsets[12] @0x3B4
		for (int i = 0; i < 0x80; ++i) { ib32(base + 0x024 + i * 4); swapInst(base, ibh32(base + 0x024 + i * 4), done); }
		for (int i = 0; i < 12;   ++i) { ib32(base + 0x3B4 + i * 4); swapPerc(base, ibh32(base + 0x3B4 + i * 4), done); }
	}
#endif

	TBasicBank* createBasicBank(void* data)
	{
		JKRHeap* heap      = TBank::getCurrentHeap();
		const u32 freeSize = heap->getFreeSize();
		THeader* header    = (THeader*)data;
		TBasicBank* bank   = new (heap, 0) TBasicBank();
		if (bank == nullptr) {
			return nullptr;
		}
		bank->setInstCount(0x100);

		for (int i = 0; i < 0x80; i++) {
			TInst* instRaw
			    = JSUConvertOffsetToPtr<TInst>(header, header->mInstOffsets[i]);
			if (instRaw != nullptr) {
				TBasicInst* instp = new (heap, 0) TBasicInst();
				instp->unk4       = instRaw->unk8;
				instp->unk8       = instRaw->unkC;

				instp->setOscCount(2);
				for (int oscIndex = 0, j = 0; j < 2; j++) {
					TOsc* oscRaw2 = JSUConvertOffsetToPtr<TOsc>(
					    header, instRaw->mOscOffsets[j]);
					TOsc* oscRaw = oscRaw2;
					if (oscRaw != nullptr) {
						TOscillator::Osc_* osc
						    = findOscPtr(bank, header, oscRaw);
						if (osc == nullptr) {
							osc           = new (heap, 0) TOscillator::Osc_;
							osc->unk0     = oscRaw->unk0;
							osc->unk4     = oscRaw->unk4;
							s16* oscTable = JSUConvertOffsetToPtr<s16>(
							    header, oscRaw->unk8);
							if (oscTable != nullptr) {
								s32 tableLength
								    = getOscTableEndPtr(oscTable) - oscTable;
								osc->unk8 = new (heap, 0) s16[tableLength];
								Calc::bcopy(oscTable, osc->unk8,
								            tableLength * sizeof(s16));
							} else {
								osc->unk8 = nullptr;
							}
							oscTable = JSUConvertOffsetToPtr<s16>(header,
							                                      oscRaw->unkC);
							if (oscTable != nullptr) {
								s32 tableLength
								    = getOscTableEndPtr(oscTable) - oscTable;
								osc->unkC = new (heap, 0) s16[tableLength];
								Calc::bcopy(oscTable, osc->unkC,
								            tableLength * sizeof(s16));
							} else {
								osc->unkC = nullptr;
							}
							osc->unk10 = oscRaw->unk10;
							osc->unk14 = oscRaw->unk14;
						}
						instp->setOsc(oscIndex, osc);
						oscIndex++;
					}
				}

				instp->setEffectCount(4);
				for (int j = 0; j < 2; j++) {
					TRand* randRaw = JSUConvertOffsetToPtr<TRand>(
					    header, instRaw->mRandOffsets[j]);
					if (randRaw != nullptr) {
						TInstRand* randp = new (heap, 0) TInstRand;
						randp->setTarget(randRaw->unk0);
						randp->unk8 = randRaw->unk4;
						randp->unkC = randRaw->unk8;
						instp->setEffect(j, randp);
					}
				}
				for (int j = 0; j < 2; j++) {
					TSense* senseRaw = JSUConvertOffsetToPtr<TSense>(
					    header, instRaw->mSenseOffsets[j]);
					if (senseRaw != nullptr) {
						TInstSense* sensep = new (heap, 0) TInstSense;
						sensep->setTarget(senseRaw->unk0);
						sensep->setParams(senseRaw->unk1, senseRaw->unk2,
						                  senseRaw->unk4, senseRaw->unk8);
						instp->setEffect(j + 2, sensep);
					}
				}

				instp->setKeyRegionCount(instRaw->mKeyRegionCount);
				for (int j = 0; j < instRaw->mKeyRegionCount; j++) {
					TBasicInst::TKeymap* instKeymap = instp->getKeyRegion(j);
					TKeymap* keymapRaw = JSUConvertOffsetToPtr<TKeymap>(
					    header, instRaw->mKeymapOffsets[j]);
					instKeymap->unk0 = keymapRaw->unk0;
					instKeymap->setVeloRegionCount(keymapRaw->unk4);
					for (int k = 0; k < keymapRaw->unk4; k++) {
						TVeloRegion* instVeloRegion
						    = instKeymap->getVeloRegion(k);
						TVmap* vmapRaw = JSUConvertOffsetToPtr<TVmap>(
						    header, keymapRaw->mVmapOffsets[k]);
						instVeloRegion->unk0 = vmapRaw->unk0;
						instVeloRegion->unk4 = vmapRaw->unk4 & 0xFFFF;
						instVeloRegion->unk8 = vmapRaw->unk8;
						instVeloRegion->unkC = vmapRaw->unkC;
					}
				}
				bank->setInst(i, instp);
			}
		}

		for (int i = 0; i < 12; i++) {
			TPerc* percRaw
			    = JSUConvertOffsetToPtr<TPerc>(header, header->mPercOffsets[i]);
			if (percRaw != nullptr) {
				TDrumSet* setp = new (heap, 0) TDrumSet;
				for (int j = 0; j < 0x80; j++) {
					TPmap* pmapRaw = JSUConvertOffsetToPtr<TPmap>(
					    header, percRaw->mPmapOffsets[j]);
					if (pmapRaw != nullptr) {
						TDrumSet::TPerc* drumSetPerc = setp->getPerc(j);
						drumSetPerc->unk0            = pmapRaw->unk0;
						drumSetPerc->unk4            = pmapRaw->unk4;
						if (percRaw->mMagic == 'PER2') {
							drumSetPerc->unk8 = percRaw->unk288[j] / 127.0f;
							drumSetPerc->setRelease(percRaw->unk308[j]);
						}
						drumSetPerc->setEffectCount(2);
						for (int effectIndex = 0, k = 0; k < 2; k++) {
							TRand* randRaw = JSUConvertOffsetToPtr<TRand>(
							    header, pmapRaw->mRandOffsets[k]);
							if (randRaw != nullptr) {
								TInstRand* randp = new (heap, 0) TInstRand();
								randp->setTarget(randRaw->unk0);
								randp->unk8 = randRaw->unk4;
								randp->unkC = randRaw->unk8;
								drumSetPerc->setEffect(effectIndex, randp);
								effectIndex++;
							}
						}
						drumSetPerc->setVeloRegionCount(
						    pmapRaw->mVeloRegionCount);
						for (int k = 0; k < pmapRaw->mVeloRegionCount; k++) {
							TVeloRegion* instVeloRegion
							    = drumSetPerc->getVeloRegion(k);
							TVmap* vmapRaw = JSUConvertOffsetToPtr<TVmap>(
							    header, pmapRaw->mVeloRegionOffsets[k]);
							instVeloRegion->unk0 = vmapRaw->unk0;
							instVeloRegion->unk4 = vmapRaw->unk4 & 0xFFFF;
							instVeloRegion->unk8 = vmapRaw->unk8;
							instVeloRegion->unkC = vmapRaw->unkC;
						}
					}
				}
				bank->setInst(i + 0xE4, setp);
			}
		}
		sUsedHeapSize += freeSize - heap->getFreeSize();
		return bank;
	}

	TOscillator::Osc_* findOscPtr(TBasicBank* bank, THeader* header, TOsc* osc)
	{
		u32* instOffsets = header->mInstOffsets - 1;
		for (int i = 0; i < 128; i++) {
			TInst* instRaw
			    = JSUConvertOffsetToPtr<TInst>(header, instOffsets[i + 1]);
			if (instRaw != nullptr) {
				for (int j = 0; j < 2; j++) {
					TOsc* oscRaw = JSUConvertOffsetToPtr<TOsc>(
					    header, instRaw->mOscOffsets[j]);
					if (oscRaw == osc) {
						JASystem::TInst* inst = bank->getInst(i);
						if (inst != nullptr) {
							TInstParam param;
							inst->getParam(60, 127, &param);
							if (j < param.mOscCount) {
								return param.mOscData[j];
							}
						}
					}
				}
			}
		}
		return nullptr;
	}

	s16* getOscTableEndPtr(s16* ptr)
	{
		s16 v1;
		do {
			v1 = *ptr;
			ptr += 3;
		} while (v1 <= 0xa);
		return ptr;
	}

	u32 getUsedHeapSize() { return 0; }

} // namespace BNKParser

} // namespace JASystem
