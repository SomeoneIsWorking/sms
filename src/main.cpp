#include <System/Application.hpp>

// BSS
TApplication gpApplication;

#ifndef SMS_NATIVE_PLATFORM
// Under SMS_NATIVE_PLATFORM=1, the entry lives in sms-boot/render_gc/aurora_bridge.cpp:
// it initializes Aurora (WebGPU/Dawn + SDL3) first, then hands off to TApplication.
// Defining main() twice would duplicate-link.
int main(void) // C++ requires main to return int (decomp had void)
{
	gpApplication.initialize();
	gpApplication.proc();
	gpApplication.finalize();
	return 0;
}
#endif
