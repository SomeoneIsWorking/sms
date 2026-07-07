#include <JSystem/JDrama/JDRSmJ3DScn.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JDrama/JDRLighting.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif

using namespace JDrama;

TSmJ3DScn::TSmJ3DScn(const char* name, s32 draw_bufs)
    : TViewObjPtrListT(name)
{
	mLightMap = nullptr;

	mDrawBufferCount = draw_bufs;
	mDrawBuffers     = new J3DDrawBuffer*[mDrawBufferCount];
	for (int i = 0; i < mDrawBufferCount; ++i)
		mDrawBuffers[i] = new J3DDrawBuffer(0x400);
}

void TSmJ3DScn::perform(u32 cue, TGraphics* graphics)
{
	if (cue & (CUE_MOVE | CUE_CALC_ANIM)) {
		TViewObjPtrListT::perform(cue, graphics);
	}

#ifdef SMS_NATIVE_PLATFORM
	// SB_J3D_DBG: is the scene draw bit (8) ever delivered, and to which scene?
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_J3D_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dbg) {
			static long calls = 0, draws = 0;
			++calls;
			if (param_1 & 8) ++draws;
			if ((calls % 400) == 0 || ((param_1 & 8) && draws <= 3))
				fprintf(stderr, "[smj3dscn] this=%p '%s' calls=%ld draws(bit8)=%ld param=0x%x bufs=%d\n",
				        (void*)this, getName() ? getName() : "?", calls, draws, param_1,
				        (int)mDrawBufferCount);
		}
	}
#endif

	if (param_1 & 8) {
		if (mLightMap)
			mLightMap->perform(CUE_LIGHT, graphics);

		MTXCopy(graphics->mViewMtx, j3dSys.getViewMtx());
		j3dSys.drawInit();
		for (int i = 0; i < mDrawBufferCount; ++i)
			mDrawBuffers[i]->frameInit();

		j3dSys.setDrawBuffer(mDrawBuffers[0], 0);
		j3dSys.setDrawBuffer(mDrawBuffers[1], 1);
		TViewObjPtrListT::perform(cue | (CUE_CALC_VIEW | CUE_ENTRY), graphics);
		j3dSys.setUnk4C(3);
		mDrawBuffers[0]->draw();
		j3dSys.setUnk4C(4);
		mDrawBuffers[1]->draw();
	}
}

void TSmJ3DScn::loadSuper(JSUMemoryInputStream& stream)
{
	TViewObjPtrListT::loadSuper(stream);
	mLightMap = new TLightMap;
	mLightMap->load(stream);
}
