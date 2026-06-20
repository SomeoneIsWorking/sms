#include <System/Application.hpp>

// BSS
TApplication gpApplication;

int main(void) // C++ requires main to return int (decomp had void)
{
	gpApplication.initialize();
	gpApplication.proc();
	gpApplication.finalize();
	return 0;
}
