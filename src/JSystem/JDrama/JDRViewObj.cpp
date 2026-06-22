#include <JSystem/JDrama/JDRViewObj.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
	cue &= ~unkC.get();
	if (cue) {
		perform(cue, graphics);
	}
}
