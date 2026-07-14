#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DPacket.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <JSystem/J3D/J3DGraphBase/Blocks/J3DTevBlocks.hpp>
#include <JSystem/J3D/J3DGraphBase/Blocks/J3DPEBlocks.hpp>
#include <JSystem/J3D/J3DGraphBase/Components/J3DBlend.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>   // SB_ENTRY_MAT entry backtrace
#include <cstring>      // SB_DBFILL_BT name match
// WEAK: only defined inside the sms-boot executable (native/render/sms_boot_j3d_capture.cpp),
// not in any linkable library — a debug-only build target that links game logic without the
// render-capture pipeline (e.g. sms-j3dload_test) must still link cleanly. Safe because every
// call site below is gated behind a getenv() debug flag, never hit in a normal test run.
extern "C" int sb_boot_capture_phase() __attribute__((weak));   // SB_DBHEAD_DBG: which pass this drawHead flush is in
extern "C" void* sb_b76_material() __attribute__((weak));       // SB_ENTRY_MAT: the b76 mask material ptr (published by capture)
extern "C" void sb_gx_get_color_alpha_update(int*, int*);  // SB_DBHEAD_PKT: live cU/aU at flush
extern "C" const char* sb_boot_drawbuf_name(const void* buf) __attribute__((weak));  // SB_ENTRY_MAT: name of the draw buffer entry lands in
// Shared cross-instrument sequence counter (sms-boot/runtime/trace_seq.cpp).
// SB_TRACE_SEQ=1: prefix dbhead flush lines with seq=N so this family
// interleaves exactly with the present-boundary/plist-order/proj logs.
extern "C" uint64_t sb_trace_seq(void) __attribute__((weak));
extern "C" unsigned VIGetRetraceCount(void) __attribute__((weak));
// SB_DBHEAD_DBG lifecycle instrument (defined near g_sbCpuDrawBufName below): forward-declared
// here so frameInit() (which appears earlier in this file) can arm the per-buffer marker.
namespace { void sb_dbfill_note_clear(const void* buf); void sb_dbfill_first_print(const void* buf, const char* how); }
#endif

J3DDrawBuffer::sortFunc J3DDrawBuffer::sortFuncTable[6] = {
	&J3DDrawBuffer::entryMatSort,     &J3DDrawBuffer::entryMatAnmSort,
	&J3DDrawBuffer::entryZSort,       &J3DDrawBuffer::entryModelSort,
	&J3DDrawBuffer::entryInvalidSort, &J3DDrawBuffer::entryNonSort,
};
J3DDrawBuffer::drawFunc J3DDrawBuffer::drawFuncTable[2] = {
	&J3DDrawBuffer::drawHead,
	&J3DDrawBuffer::drawTail,
};
int J3DDrawBuffer::entryNum;

J3DDrawBuffer::J3DDrawBuffer(u32 size)
{
	mBuffer = new (0x20) J3DPacket*[size];
	mSize   = size;

	mDrawType = DRAW_HEAD;
	mSortType = SORT_MAT;

	mZNear = 1.0f;
	mZFar  = 10000.0f;

	mZMtx           = nullptr;
	mCallBackPacket = nullptr;

	mZRatio = (mZFar - mZNear) / mSize;

	frameInit();
}

#ifdef SMS_NATIVE_PLATFORM
// SB_FI_TRACE: scene_drive registers the scene's two draw buffers here; frameInit() then
// backtraces whenever EITHER is reset, so we can find what clears the map mid-conductor-walk.
extern "C" { J3DDrawBuffer* g_sb_fi_watch[2] = { nullptr, nullptr }; }
extern "C" void sb_fi_watch_register(J3DDrawBuffer* opa, J3DDrawBuffer* xlu)
{
	g_sb_fi_watch[0] = opa;
	g_sb_fi_watch[1] = xlu;
}
#include <execinfo.h>
#include <cstdlib>
#include <cstdio>
#endif

void J3DDrawBuffer::frameInit()
{
#ifdef SMS_NATIVE_PLATFORM
	if ((this == g_sb_fi_watch[0] || this == g_sb_fi_watch[1]) && getenv("SB_FI_TRACE")) {
		static int s_fi = 0;
		if (s_fi < 12) { ++s_fi;
			std::fprintf(stderr, "[fi-trace] frameInit on WATCHED scene buf %p:\n", this);
			void* frames[24]; int nf = backtrace(frames, 24);
			backtrace_symbols_fd(frames, nf, 2);
		}
	}
#endif
	for (int i = 0; i < mSize; ++i)
		mBuffer[i] = nullptr;

	mCallBackPacket = nullptr;

#ifdef SMS_NATIVE_PLATFORM
	// SB_DBHEAD_DBG: per-buffer CLEAR event, same gate/seq/name style as the [dbhead] flush
	// print below and the [dbfill-first] entry print in entryMatSort — together these three
	// pin down clear-vs-fill-vs-flush ordering for the lifecycle timeline.
	{
		static long s_dbheadAfter2 = -2;
		if (s_dbheadAfter2 == -2) {
			const char* eAfter = std::getenv("SB_DBHEAD_DBG_AFTER");
			s_dbheadAfter2 = (eAfter && eAfter[0]) ? std::atol(eAfter) : -1;
		}
		bool dbheadOn2 = false;
		if (const char* e = std::getenv("SB_DBHEAD_DBG"); e && e[0] && e[0] != '0') dbheadOn2 = true;
		if (s_dbheadAfter2 >= 0 && &VIGetRetraceCount && static_cast<long>(VIGetRetraceCount()) >= s_dbheadAfter2)
			dbheadOn2 = true;
		if (dbheadOn2) {
			const char* bn = (&sb_boot_drawbuf_name) ? sb_boot_drawbuf_name((const void*)this) : nullptr;
			sb_dbfill_note_clear((const void*)this);
			if (&sb_trace_seq) {
				std::fprintf(stderr, "[dbclear] seq=%lu buf=%p name=\"%s\"\n",
				             (unsigned long)sb_trace_seq(), (const void*)this, bn ? bn : "(unknown)");
			} else {
				std::fprintf(stderr, "[dbclear] buf=%p name=\"%s\"\n", (const void*)this, bn ? bn : "(unknown)");
			}
		}
	}
#endif
}

bool J3DDrawBuffer::entryMatSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();
#ifdef SMS_NATIVE_PLATFORM
	// SB_DBHEAD_DBG: [dbfill-first] — the FIRST packet entered into this buffer since its
	// last frameInit() clear. See sb_dbfill_first_print below; every entry* sort path calls it.
	sb_dbfill_first_print((const void*)this, "matSort");
#endif
#ifdef SMS_NATIVE_PLATFORM
	// SB_ENTRY_MAT=1: backtrace the entry of the b76 mask material's packet → names the scene object
	// that enters it into THIS buffer (and which buffer = `this`). The target material ptr comes from
	// the capture (sb_b76_material, published once b76 is seen), so it tracks the per-run heap address.
	if (const char* e = std::getenv("SB_ENTRY_MAT"); e && e[0] && e[0] != '0') {
		void* want = sb_b76_material();
		if (want && (void*)packet->getMaterial() == want) {
			// Fire per (buffer, shape) pair — each combination gets ONE line + backtrace so we
			// see every distinct draw buffer this material enters, not just the first 3 calls.
			static struct { const void* buf; const void* shp; } seen[16];
			static int nseen = 0;
			const void* shp = (const void*)packet->getShapePacket();
			bool novel = true;
			for (int i = 0; i < nseen; ++i)
				if (seen[i].buf == (const void*)this && seen[i].shp == shp) { novel = false; break; }
			if (novel && nseen < 16) {
				seen[nseen].buf = (const void*)this; seen[nseen].shp = shp; ++nseen;
				const char* bufname = (&sb_boot_drawbuf_name)
				                          ? sb_boot_drawbuf_name((const void*)this) : nullptr;
				std::fprintf(stderr, "[entry-mat] mat=%p packet=%p shape=%p buf(this)=%p buf-name=\"%s\" phase=%d backtrace:\n",
				             (void*)packet->getMaterial(), (void*)packet, shp, (const void*)this,
				             bufname ? bufname : "(unknown)",
				             sb_boot_capture_phase());
				void* fr[40]; int nf = ::backtrace(fr, 40); ::backtrace_symbols_fd(fr, nf, 2);
			}
		}
	}
#endif

	J3DTexture* texture = j3dSys.getTexture();
	u32 hash;
	u16 texNo = packet->getMaterial()->getTexNo(0);
	if (texNo == 0xFFFF) {
		hash = 0;
	} else {
		hash = (u32)(uintptr_t)texture->getResTIMG(texNo);
	}

	if (packet->unk3C & 0x80000000) {
#ifdef SMS_NATIVE_PLATFORM
		// PC-engine draw-list invariant: this is the "no-merge, always prepend to
		// slot 0" path for unsorted/indirect materials (unk3C bit31). Unlike the
		// hashed branch below (which dedups via isSame), it does NOT guard against
		// re-entering the SAME packet. When an object enters a model via both
		// entry() and update() in one frameInit window (e.g. TShimmer::perform does
		// unk48->entry() for flag bit 0x4 AND unk48->update() for bit 0x200, and the
		// indirect-scene flag 0x40000204 sets both), the packet gets prepended twice.
		//
		// The original eeccde1 fix only checked the SLOT HEAD (mBuffer[0] == packet),
		// which catches a packet entered twice CONSECUTIVELY (the TShimmer self-loop).
		// But with the NPC population MANY objects enter twice and interleave: enter P
		// (head=P), enter Q (head=Q, Q->next=P), enter P AGAIN — P is no longer the
		// head, so the head-check misses it, P->next=Q and head=P → P->next=Q,
		// Q->next=P = a MULTI-node cycle → drawHead()'s walk spins forever (the
		// SB_NPC_ON gameplay hang; SB_DRAWBUF_CHECK reports "CYCLE ... selfloop=0").
		// The correct invariant is "enter each packet at most once per frameInit
		// window": skip the prepend if the packet is ALREADY anywhere in the slot
		// chain (re-entering an entered packet is a pure no-op). The chain is the
		// unsorted/indirect set (small), so the walk is cheap and can never form any
		// cycle. See debug_journal/2026-06-25_delfino_indirect_drawbuf_selfloop.md.
		for (J3DPacket* p = mBuffer[0]; p; p = p->getNextPacket())
			if (p == packet)
				return true;
#endif
		packet->setNextPacket(mBuffer[0]);
		mBuffer[0] = packet;
		return true;
	} else {
		u32 slot = hash & (mSize - 1);
		if (mBuffer[slot] == NULL) {
			mBuffer[slot] = packet;
			return true;
		} else {
			for (J3DMatPacket* pkt = (J3DMatPacket*)mBuffer[slot]; pkt != NULL;
			     pkt               = (J3DMatPacket*)pkt->getNextPacket()) {
				if (pkt->isSame(packet)) {
					pkt->addShapePacket(packet->getShapePacket());
					return false;
				}
			}

			packet->setNextPacket(mBuffer[slot]);
			mBuffer[slot] = packet;
			return true;
		}
	}
}

bool J3DDrawBuffer::entryMatAnmSort(J3DMatPacket* packet)
{
	J3DMaterialAnm* pMaterialAnm = packet->unk44;
	u32 slot                     = (u32)((uintptr_t)pMaterialAnm & (mSize - 1));

	if (pMaterialAnm == NULL) {
		return entryMatSort(packet);
	} else {
		packet->drawClear();
		packet->getShapePacket()->drawClear();
#ifdef SMS_NATIVE_PLATFORM
		sb_dbfill_first_print((const void*)this, "matAnmSort");  // SB_DBHEAD_DBG lifecycle
#endif
		if (mBuffer[slot] == NULL) {
			mBuffer[slot] = packet;
			return true;
		} else {
			for (J3DMatPacket* pkt = (J3DMatPacket*)mBuffer[slot]; pkt != NULL;
			     pkt               = (J3DMatPacket*)pkt->getNextPacket()) {
				if (pkt->unk44 == pMaterialAnm) {
					pkt->addShapePacket(packet->getShapePacket());
					return false;
				}
			}

			packet->setNextPacket(mBuffer[slot]);
			mBuffer[slot] = packet;
			return true;
		}
	}
}

bool J3DDrawBuffer::entryZSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();
#ifdef SMS_NATIVE_PLATFORM
	sb_dbfill_first_print((const void*)this, "zSort");  // SB_DBHEAD_DBG lifecycle
#endif

	Vec tmp;
	tmp.x = mZMtx[0][3];
	tmp.y = mZMtx[1][3];
	tmp.z = mZMtx[2][3];

	f32 value = -J3DCalcZValue(j3dSys.getViewMtx(), tmp);

	u32 idx;
	if (mZNear + mZRatio < value) {
		if (mZFar - mZRatio > value) {
			idx = value / mZRatio;
		} else {
			idx = mSize - 1;
		}
	} else {
		idx = 0;
	}

	idx = (mSize - 1) - idx;
	packet->setNextPacket(mBuffer[idx]);
	mBuffer[idx] = packet;

	return true;
}

bool J3DDrawBuffer::entryModelSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();
#ifdef SMS_NATIVE_PLATFORM
	sb_dbfill_first_print((const void*)this, "modelSort");  // SB_DBHEAD_DBG lifecycle
#endif

	if (mCallBackPacket != nullptr) {
		mCallBackPacket->addChildPacket(packet);
		return true;
	}

	return false;
}

bool J3DDrawBuffer::entryInvalidSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();
#ifdef SMS_NATIVE_PLATFORM
	sb_dbfill_first_print((const void*)this, "invalidSort");  // SB_DBHEAD_DBG lifecycle
#endif

	if (mCallBackPacket != nullptr) {
		mCallBackPacket->addChildPacket(packet->getShapePacket());
		return true;
	}

	return false;
}

bool J3DDrawBuffer::entryNonSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();
#ifdef SMS_NATIVE_PLATFORM
	sb_dbfill_first_print((const void*)this, "nonSort");  // SB_DBHEAD_DBG lifecycle
#endif

	packet->setNextPacket(mBuffer[0]);
	mBuffer[0] = packet;

	return true;
}

bool J3DDrawBuffer::entryImm(J3DPacket* packet, u16 index)
{
#ifdef SMS_NATIVE_PLATFORM
	sb_dbfill_first_print((const void*)this, "imm");  // SB_DBHEAD_DBG lifecycle
#endif
	packet->setNextPacket(mBuffer[index]);
	mBuffer[index] = packet;

	return true;
}

#ifdef SMS_NATIVE_PLATFORM
extern "C" const char* sb_boot_drawbuf_name(const void* buf);
#ifdef SMS_AURORA
#include <dolphin/gx/GXAurora.h>
#endif
#endif

// TEMP crosshatch-hunt: CPU-side "which drawbuf is entry()-ing right now" name,
// distinct from sb_gx_last_marker() (that one only updates when aurora's FIFO
// parser later PROCESSES the GXInsertDebugMarker command, i.e. at end-of-frame
// drain — it lags a whole frame behind the CPU-side material/loadTexNo calls
// made here, so it can't be used to attribute a loadTexNo() call to the
// DrawBuf that issued it). This one is set synchronously, in CPU order.
#ifdef SMS_NATIVE_PLATFORM
static const char* g_sbCpuDrawBufName = "";
extern "C" const char* sb_cpu_drawbuf_name() { return g_sbCpuDrawBufName; }

// SB_DBHEAD_DBG lifecycle instrument: per-buffer "cleared, not yet filled this frame" marker
// so entryMatSort can print [dbfill-first] exactly once per buffer per clear (avoids flooding
// stderr with one line per shape/packet entered — a single scene buffer can hold hundreds).
namespace {
	struct DbFillMark { const void* buf; bool pendingFirstFill; };
	constexpr int kDbFillMax = 64;
	DbFillMark g_dbfill[kDbFillMax];
	int g_dbfill_n = 0;
	// frameInit() calls this on every clear: arms the marker so the next entry into this
	// buffer prints [dbfill-first].
	void sb_dbfill_note_clear(const void* buf) {
		for (int i = 0; i < g_dbfill_n; ++i)
			if (g_dbfill[i].buf == buf) { g_dbfill[i].pendingFirstFill = true; return; }
		if (g_dbfill_n < kDbFillMax) { g_dbfill[g_dbfill_n].buf = buf; g_dbfill[g_dbfill_n].pendingFirstFill = true; ++g_dbfill_n; }
	}
	// Every entry* sort path calls this on every packet add: returns true (and disarms) only
	// the FIRST time it's called since the buffer's last clear.
	bool sb_dbfill_check_and_clear_first(const void* buf) {
		for (int i = 0; i < g_dbfill_n; ++i)
			if (g_dbfill[i].buf == buf) {
				if (g_dbfill[i].pendingFirstFill) { g_dbfill[i].pendingFirstFill = false; return true; }
				return false;
			}
		// Buffer never seen a frameInit() clear under this instrument (e.g. constructed
		// before the gate turned on) — treat first entry seen at all as "first fill".
		if (g_dbfill_n < kDbFillMax) { g_dbfill[g_dbfill_n].buf = buf; g_dbfill[g_dbfill_n].pendingFirstFill = false; ++g_dbfill_n; }
		return true;
	}
	// SB_DBHEAD_DBG (same gates as the [dbhead] flush print): one [dbfill-first] line per
	// buffer per clear window, tagged with WHICH entry sort path filled it first. Composes
	// with [dbclear]/[dbhead] into a clear→fill→flush lifecycle timeline without flooding
	// stderr (a scene buffer can take hundreds of entries per frame).
	void sb_dbfill_first_print(const void* buf, const char* how) {
		bool on = false;
		if (const char* e = std::getenv("SB_DBHEAD_DBG"); e && e[0] && e[0] != '0') on = true;
		static long s_after = -2;
		if (s_after == -2) {
			const char* eAfter = std::getenv("SB_DBHEAD_DBG_AFTER");
			s_after = (eAfter && eAfter[0]) ? std::atol(eAfter) : -1;
		}
		if (s_after >= 0 && &VIGetRetraceCount && static_cast<long>(VIGetRetraceCount()) >= s_after)
			on = true;
		if (!on || !sb_dbfill_check_and_clear_first(buf))
			return;
		const char* bn = (&sb_boot_drawbuf_name) ? sb_boot_drawbuf_name(buf) : nullptr;
		if (&sb_trace_seq) {
			std::fprintf(stderr, "[dbfill-first] seq=%lu buf=%p via=%s name=\"%s\"\n",
			             (unsigned long)sb_trace_seq(), buf, how, bn ? bn : "(unknown)");
		} else {
			std::fprintf(stderr, "[dbfill-first] buf=%p via=%s name=\"%s\"\n", buf, how, bn ? bn : "(unknown)");
		}
		// SB_DBFILL_BT=<substring>: backtrace the first fill of any buffer whose name
		// contains the substring — names the code path (actor/group perform) that enters
		// packets into that buffer. Cap 6 traces per run.
		if (const char* want = std::getenv("SB_DBFILL_BT"); want && want[0] && bn && ::strstr(bn, want)) {
			static int s_bt = 0;
			if (s_bt < 6) {
				++s_bt;
				void* fr[40]; int nf = ::backtrace(fr, 40); ::backtrace_symbols_fd(fr, nf, 2);
			}
		}
	}
}
#endif

void J3DDrawBuffer::draw() const
{
#ifdef SMS_AURORA
	// Draw-identity marker: every GX draw the fifo processes after this
	// belongs to THIS buffer until the next marker (SB_DRAW_DUMP prints it).
	{
		const char* nm = sb_boot_drawbuf_name((const void*)this);
		g_sbCpuDrawBufName = nm ? nm : "buf?";
		GXInsertDebugMarker(nm ? nm : "buf?");
	}
#endif
#ifdef SMS_NATIVE_PLATFORM
	// SB_DRAWBUF_STATS=1: sampled per-buffer packet counts at draw time — the
	// discriminator between "buffer never filled" (count 0 while entry runs)
	// and "packets drawn but invisible" (count > 0; chase GX state instead).
	{
		static long dbgStart = -2;
		if (dbgStart == -2) {
			const char* e = getenv("SB_DRAWBUF_STATS"); // =<startCall>; buffer-draws run ~26/frame
			dbgStart = (e && e[0] && e[0] != '0') ? atol(e) : -1;
			if (dbgStart >= 0 && dbgStart < 200) dbgStart = 200;
		}
		if (dbgStart >= 0) {
			static long s_call = 0;
			++s_call;
			if (s_call > dbgStart && s_call <= dbgStart + 60) { // one window, every buffer draw
				u32 pkts = 0, shpTotal = 0, shpEnabled = 0, shpNullShape = 0;
				for (u32 i = 0; i < mSize; i++)
					for (J3DPacket* p = mBuffer[i]; p; p = p->getNextPacket()) {
						++pkts;
						for (J3DShapePacket* sp = ((J3DMatPacket*)p)->getShapePacket(); sp;
						     sp = (J3DShapePacket*)sp->unk4) {
							++shpTotal;
							if (sp->unk30 != 0) ++shpEnabled;
							if (sp->unk14 == nullptr) ++shpNullShape;
						}
					}
				const char* nm = sb_boot_drawbuf_name((const void*)this);
				fprintf(stderr,
				        "[bufstat] call=%ld buf=%p '%s' packets=%u shp=%u enabled=%u nullShape=%u\n",
				        s_call, (const void*)this, nm ? nm : "?", pkts, shpTotal, shpEnabled,
				        shpNullShape);
			}
		}
	}
#endif
	drawFunc func = drawFuncTable[mDrawType];
	(this->*func)();
}

void J3DDrawBuffer::drawHead() const
{
#ifdef SMS_NATIVE_PLATFORM
	// SB_DRAWBUF_CHECK=1: opt-in defensive cycle detector. A packet entered twice
	// into the same slot via a no-merge prepend self-links (pkt->next = mBuffer[slot]
	// = pkt), making this walk spin forever. entryMatSort now guards against that
	// (the real fix), so this is OFF by default and kept only as a tripwire for any
	// future no-merge entry path that re-introduces a self-link. Floyd tortoise/hare
	// per slot; names the offending packet + buffer, then aborts.
	{
		static int chk = -1;
		if (chk < 0) { const char* e = getenv("SB_DRAWBUF_CHECK"); chk = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (chk) {
			for (u32 i = 0; i < mSize; i++) {
				J3DPacket* slow = mBuffer[i];
				J3DPacket* fast = mBuffer[i];
				while (fast && fast->getNextPacket()) {
					slow = slow->getNextPacket();
					fast = fast->getNextPacket()->getNextPacket();
					if (slow == fast) {
						fprintf(stderr,
						        "[drawbuf] CYCLE buf=%p mBuffer[%u]: packet=%p vtable=%p "
						        "selfloop=%d sortType=%d\n",
						        (const void*)this, i, (void*)slow,
						        slow ? *(void**)slow : nullptr,
						        (int)(mBuffer[i] == mBuffer[i]->getNextPacket()),
						        (int)mSortType);
						abort();
					}
				}
			}
		}
	}
#endif
#ifdef SMS_NATIVE_PLATFORM
	// SB_DBHEAD_DBG: trace each drawHead flush — buffer ptr + phase + packet count. Cross-ref the
	// buffer ptr with SB_DRAWBUF_INV to see WHICH named buffer (e.g. MapXlu) is flushed in WHICH pass.
	// This nails the MapXlu-mask pass-routing divergence (native flushes it in ph1+ph6; GC in main).
	// SB_DBHEAD_DBG_AFTER=<retrace>: same threshold pattern as SB_PROJ_DBG_AFTER/
	// SB_PLIST_ORDER_DBG_AFTER — unthresholded this fires every flush of every buffer every
	// frame from frame 0, which floods stderr heavily enough to dominate a paced run's
	// wall-clock. Set alone (without SB_DBHEAD_DBG) to get only the settled window.
	static long s_dbheadAfter = -2;
	if (s_dbheadAfter == -2) {
		const char* eAfter = std::getenv("SB_DBHEAD_DBG_AFTER");
		s_dbheadAfter = (eAfter && eAfter[0]) ? std::atol(eAfter) : -1;
	}
	bool dbheadOn = false;
	if (const char* e = std::getenv("SB_DBHEAD_DBG"); e && e[0] && e[0] != '0') dbheadOn = true;
	if (s_dbheadAfter >= 0 && &VIGetRetraceCount && static_cast<long>(VIGetRetraceCount()) >= s_dbheadAfter)
		dbheadOn = true;
	if (dbheadOn) {
		long np = 0; for (u32 i = 0; i < mSize; i++)
			for (J3DPacket* p = mBuffer[i]; p; p = p->getNextPacket()) ++np;
		// SB_DBHEAD_MAT=1: also print each packet's J3DMaterial low-24 bits (cast to J3DMatPacket) so the
		// ph6 buffer that holds the sea-MASK material (c97c48, key eb5c8e74) is named — distinguishes
		// the real MapXlu (b1f7bc) from the two unnamed ph6 buffers. One-shot per (phase,buf) at settle.
		static int s_matN = 0;
		if (np > 0 && std::getenv("SB_DBHEAD_MAT") && &sb_boot_capture_phase && sb_boot_capture_phase() == 6 && s_matN < 12) {
			++s_matN; char mats[200]; int mn = 0; mats[0] = 0;
			for (u32 i = 0; i < mSize; i++)
				for (J3DPacket* p = mBuffer[i]; p && mn < 180; p = p->getNextPacket())
					mn += std::snprintf(mats + mn, sizeof(mats) - mn, " %06x",
					        (unsigned)((uintptr_t)static_cast<J3DMatPacket*>(p)->getMaterial() & 0xffffff));
			std::fprintf(stderr, "[dbhead-mat] phase=6 buf=%p packets=%ld mats=%s\n",
			             (const void*)this, np, mats);
		}
		if (np > 0) {
			const char* bn = (&sb_boot_drawbuf_name)
			                     ? sb_boot_drawbuf_name((const void*)this)
			                     : nullptr;
			static int traceSeqOn = -1;
			if (traceSeqOn < 0) {
				const char* eSeq = std::getenv("SB_TRACE_SEQ");
				traceSeqOn = (eSeq && eSeq[0] && eSeq[0] != '0') ? 1 : 0;
			}
			if (traceSeqOn && &sb_trace_seq) {
				std::fprintf(stderr, "[dbhead] seq=%lu phase=%d buf=%p packets=%ld name=\"%s\"\n",
				             (unsigned long)sb_trace_seq(),
				             (&sb_boot_capture_phase) ? sb_boot_capture_phase() : -1, (const void*)this, np,
				             bn ? bn : "(unknown)");
			} else {
				std::fprintf(stderr, "[dbhead] phase=%d buf=%p packets=%ld name=\"%s\"\n",
				             (&sb_boot_capture_phase) ? sb_boot_capture_phase() : -1, (const void*)this, np,
				             bn ? bn : "(unknown)");
			}
		}
	}
	// SB_DBHEAD_PKT=1: at each flush of a buffer that holds the sea-mask material (c97c48), print the
	// live GXSetColorUpdate/AlphaUpdate + every packet's (phase, model, modelData, material) so ph1 vs
	// ph6 MapXlu contents + the pass-level colorUpdate can be compared directly (the overbright question:
	// does GC's ph6 buffer really lack the mask, or is native's cU wrong?). Gated to a settled window.
	if (const char* e = std::getenv("SB_DBHEAD_PKT"); e && e[0] && e[0] != '0') {
		bool hasMask = false;
		for (u32 i = 0; i < mSize; i++)
			for (J3DPacket* p = mBuffer[i]; p; p = p->getNextPacket())
				if (((uintptr_t)static_cast<J3DMatPacket*>(p)->getMaterial() & 0xffffff) == 0xc97c48)
					hasMask = true;
		static int s_pktN = 0;
		if (hasMask && s_pktN < 24) {
			++s_pktN;
			int cu = 1, au = 1; sb_gx_get_color_alpha_update(&cu, &au);
			std::fprintf(stderr, "[dbhead-pkt] phase=%d buf=%p cU=%d aU=%d\n",
			             sb_boot_capture_phase(), (const void*)this, cu, au);
			for (u32 i = 0; i < mSize; i++)
				for (J3DPacket* p = mBuffer[i]; p; p = p->getNextPacket()) {
					J3DMatPacket* mp = static_cast<J3DMatPacket*>(p);
					J3DMaterial* mat = mp->getMaterial();
					int tevN = -1, bm = -1, sf = -1, df = -1;
					int cr = -1, cg = -1, cb = -1, ca = -1;
					if (mat) {
						if (J3DTevBlock* tb = mat->getTevBlock()) tevN = tb->getTevStageNum();
						if (J3DPEBlock* pe = mat->getPEBlock())
							if (J3DBlend* bl = pe->getBlend()) {
								bm = bl->mBlendMode; sf = bl->mSrcFactor; df = bl->mDstFactor;
							}
						if (J3DGXColorS10* c1 = mat->getTevColor(1)) {
							cr = c1->color.r; cg = c1->color.g; cb = c1->color.b; ca = c1->color.a;
						}
					}
					std::fprintf(stderr, "    pkt slot=%u packet=%p mat=%06x shapePkt=%p"
					             " tev=%d blend=%d/%d/%d reg1=%d,%d,%d,%d\n", i,
					             (void*)mp,
					             (unsigned)((uintptr_t)mat & 0xffffff),
					             (void*)mp->getShapePacket(),
					             tevN, bm, sf, df, cr, cg, cb, ca);
				}
		}
	}
#endif
	for (u32 i = 0; i < mSize; i++) {
		for (J3DPacket* packet = mBuffer[i]; packet != nullptr;
		     packet            = packet->getNextPacket()) {
			packet->draw();
		}
	}
}

void J3DDrawBuffer::drawTail() const
{
	for (int i = mSize - 1; i >= 0; i--) {
		for (J3DPacket* packet = mBuffer[i]; packet != nullptr;
		     packet            = packet->getNextPacket()) {
			packet->draw();
		}
	}
}
