#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JDrama/JDRActor.hpp>
#include <JSystem/JDrama/JDRLighting.hpp>
#include <JSystem/JDrama/JDRCharacter.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif

void JDrama::TActor::load(JSUMemoryInputStream& stream)
{
	TPlacement::load(stream);

	stream.read(&mRotation.x, sizeof(f32));
	stream.read(&mRotation.y, sizeof(f32));
	stream.read(&mRotation.z, sizeof(f32));
	stream.read(&mScaling.x, sizeof(f32));
	stream.read(&mScaling.y, sizeof(f32));
	stream.read(&mScaling.z, sizeof(f32));
#ifdef SMS_NATIVE_PLATFORM
	// Scene data is big-endian and stream.read() raw-copies without swapping
	// (same bug class as TPlacement::load's mPosition, already fixed there).
	// A BE 1.0 scale reads back as a ~4.6e-41 denormal -> the model's base
	// scale collapses every joint rotation row to ~0 -> geometry degenerates
	// to a point and never rasterizes (invisible title backdrop, 2026-07-07).
	{
		f32* comps[6] = { &mRotation.x, &mRotation.y, &mRotation.z,
			              &mScaling.x, &mScaling.y, &mScaling.z };
		for (int i = 0; i < 6; ++i) {
			u32 b;
			__builtin_memcpy(&b, comps[i], 4);
			b = __builtin_bswap32(b);
			__builtin_memcpy(comps[i], &b, 4);
		}
	}
#endif

	char str[0x50];
	stream.readString(str, 0x50);

	unk3C = TNameRefGen::search<TCharacter>(str);

	TLightMap* lightMap = new TLightMap;

	unk40 = lightMap;
	lightMap->load(stream);
}

void JDrama::TActor::issueGXLight(u32 param_1, JDrama::TGraphics* param_2)
{
#ifdef SMS_NATIVE_PLATFORM
	// DIAG (SB_ACTOR_LIGHT_DBG): which actors carry a populated per-actor TLightMap, and how
	// many lights does it select? This is the faithful GX-light source the value oracle sees.
	static const char* dbg = std::getenv("SB_ACTOR_LIGHT_DBG");
	if (dbg && dbg[0] && dbg[0] != '0' && unk40) {
		static int shown = 0;
		if (shown < 40) {
			++shown;
			TLightMap* lm = (TLightMap*)unk40;
			std::fprintf(stderr, "[actor-light] actor='%s' lightMap=%p count=%d\n",
			             getName() ? getName() : "?", (void*)lm, lm->mLightInfoCount);
			for (int i = 0; i < lm->mLightInfoCount && i < 8; ++i)
				std::fprintf(stderr, "    info[%d] slot=%u obj=%p name='%s'\n",
				             i, lm->mLightInfos[i].unk0, (void*)lm->mLightInfos[i].unk4,
				             lm->mLightInfos[i].unk4 ? lm->mLightInfos[i].unk4->getName() : "?");
		}
	}
#endif
	if (unk40 != nullptr)
		unk40->perform(param_1 | 0x20, param_2);
}

void JDrama::TActor::perform(u32 param_1, TGraphics* param_2)
{
	if (param_1 & 0x8)
		issueGXLight(param_1, param_2);
}

JDrama::TActor::~TActor() { }

void JDrama::TActor::JSGGetTranslation(Vec* v) const { *v = mPosition; }

void JDrama::TActor::JSGSetTranslation(const Vec& v)
{
	mPosition.x = v.x;
	mPosition.y = v.y;
	mPosition.z = v.z;
}

void JDrama::TActor::JSGGetScaling(Vec* v) const { *v = mScaling; }

void JDrama::TActor::JSGSetScaling(const Vec& v)
{
	mScaling.x = v.x;
	mScaling.y = v.y;
	mScaling.z = v.z;
}

void JDrama::TActor::JSGGetRotation(Vec* v) const { *v = mRotation; }

void JDrama::TActor::JSGSetRotation(const Vec& v)
{
	mRotation.x = v.x;
	mRotation.y = v.y;
	mRotation.z = v.z;
}

int JDrama::TActor::getType() const { return 1; }
