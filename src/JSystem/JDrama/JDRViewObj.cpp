#include <JSystem/JDrama/JDRViewObj.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" unsigned VIGetRetraceCount(void) __attribute__((weak));
// Shared cross-instrument sequence counter (sms-boot/runtime/trace_seq.cpp).
// SB_TRACE_SEQ=1: prefix plist-order lines with seq=N so this family
// interleaves exactly with the present-boundary/proj/drawbuf-flush logs.
extern "C" uint64_t sb_trace_seq(void) __attribute__((weak));
#endif

void JDrama::TViewObj::testPerform(u32 cue, JDrama::TGraphics* graphics)
{
#ifdef SMS_NATIVE_PLATFORM
	// Draw-phase keystone trace: for "camera 1" log the RAW incoming flag, the unkC mask,
	// and what survives — tells us whether ctrl bit 0x1 is passed-then-masked or never passed.
	static int dbg = -1;
	if (dbg < 0) { const char* e = getenv("SB_J3D_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
	if (dbg) {
		const char* nm = getName();
		if (nm && std::strcmp(nm, "camera 1") == 0) {
			static long n = 0;
			u32 raw = param_1, mask = unkC.get(), surv = param_1 & ~mask;
			if ((++n % 200) == 0 || n <= 8)
				std::fprintf(stderr, "[testPerform camera1] n=%ld raw=0x%x unkC=0x%x survives=0x%x ctrl_b0=%d\n",
				             n, raw, mask, surv, (surv & 1) != 0);
		}
	}
#endif
	if (((param_1 & 0x1000) != 0) && unkC.check(0x1000)) {
		param_1 = param_1 & ~0x1;
	}
	if (((cue & CUE_MOVEMENT_GATE_B) != 0) && unkC.check(CUE_MOVEMENT_GATE_B)) {
		cue = cue & ~CUE_MOVE;
	}
	param_1 &= ~unkC.get();
	if (param_1) {
#ifdef SMS_NATIVE_PLATFORM
		// SB_PLIST_ORDER_DBG(_AFTER=<retrace>): per-dispatch order trace -- names the
		// exact TViewObj (by NameRef name) about to run its perform() virtual, so a
		// GXSetProjection callback captured elsewhere (e.g. SB_PROJ_DBG_AFTER's
		// backtrace, which only resolves generically to
		// TPerformList::forEachPerform/testPerform, not the dispatched object) can be
		// correlated to WHICH scene object issued it, in exact perform-list order.
		{
			static int dbg = -1;
			static long afterThresh = -1;
			if (dbg < 0) {
				const char* e = getenv("SB_PLIST_ORDER_DBG");
				dbg = (e && e[0] && e[0] != '0') ? 1 : 0;
				const char* eAfter = getenv("SB_PLIST_ORDER_DBG_AFTER");
				afterThresh = (eAfter && eAfter[0]) ? atol(eAfter) : -1;
			}
			if (dbg || afterThresh >= 0) {
				unsigned retrace = (&VIGetRetraceCount) ? VIGetRetraceCount() : 0;
				if (dbg || static_cast<long>(retrace) >= afterThresh) {
					static long n = 0;
					++n;
					const char* nm = getName();
					static int traceSeqOn = -1;
					if (traceSeqOn < 0) {
						const char* eSeq = getenv("SB_TRACE_SEQ");
						traceSeqOn = (eSeq && eSeq[0] && eSeq[0] != '0') ? 1 : 0;
					}
					if (traceSeqOn && &sb_trace_seq) {
						std::fprintf(stderr, "[plist-order] seq=%lu n=%ld retrace=%u name=\"%s\" flags=0x%x\n",
						             (unsigned long)sb_trace_seq(), n, retrace, nm ? nm : "<null>", param_1);
					} else {
						std::fprintf(stderr, "[plist-order] n=%ld retrace=%u name=\"%s\" flags=0x%x\n", n, retrace,
						             nm ? nm : "<null>", param_1);
					}
				}
			}
		}
#endif
		perform(param_1, param_2);
	}
}
