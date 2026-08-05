#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JDrama/JDRActor.hpp>
#include <JSystem/JDrama/JDRLighting.hpp>
#include <JSystem/JDrama/JDRCharacter.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <sb_log.h>
#endif

#ifdef SMS_NATIVE_PLATFORM
// SB_LOG=interp -- motion probe over the 60fps interpolation SNAPSHOT population. See the
// declaration in JDRActor.hpp for why it is out-of-line and why it is designed negative-first.
void JDrama::TActor::sbInterpMotionProbe(const char* name,
                                         const JGeometry::TVec3<f32>& prev,
                                         const JGeometry::TVec3<f32>& cur)
{
	if (!SB_LOG_ON("interp"))
		return;

	static long        s_seen = 0, s_moved = 0;
	static f32         s_maxD2 = 0.0f;
	static const char* s_maxName = "(nothing moved)";

	// COVERAGE ROSTER. moved=0 has two completely different causes -- a scene whose actors really
	// do not move, and a hook that never reaches the actors that do -- and the counters above
	// cannot tell them apart: both print 0. So the probe also records WHO it saw. If the roster
	// contains the player, coverage is demonstrated and a low moved% is a fact about the scene;
	// if it does not, moved% says nothing at all. Bounded to the first 200 ticks and 192 names so
	// the linear scan cannot become the frame cost.
	enum { kMaxNames = 4096, kCollectTicks = 200 };
	static const char* s_names[kMaxNames];
	static int         s_nNames  = 0;
	static bool        s_capped  = false;
	static bool        s_dumped  = false;
	if (name && sSbInterpTick < (unsigned long)kCollectTicks) {
		bool known = false;
		for (int i = 0; i < s_nNames; ++i)
			if (std::strcmp(s_names[i], name) == 0) { known = true; break; }
		if (!known) {
			if (s_nNames < kMaxNames)
				s_names[s_nNames++] = name;
			else
				s_capped = true;
		}
	} else if (!s_dumped && s_nNames > 0) {
		s_dumped = true;
		SB_LOGC("interp", "ROSTER: %d distinct objects snapshotted in the first %d ticks%s",
		        s_nNames, (int)kCollectTicks,
		        s_capped ? " (CAPPED at 4096 -- the real count is higher)" : "");
		for (int i = 0; i < s_nNames; ++i)
			SB_LOGC("interp", "  roster[%d] = %s", i, s_names[i]);
	}

	const f32 dx = cur.x - prev.x;
	const f32 dy = cur.y - prev.y;
	const f32 dz = cur.z - prev.z;
	const f32 d2 = dx * dx + dy * dy + dz * dz;

	++s_seen;
	if (d2 > 0.0f) {
		++s_moved;
		if (d2 > s_maxD2) {
			s_maxD2   = d2;
			s_maxName = name ? name : "(unnamed)";
		}
	}

	// Report the DENOMINATOR and the largest mover BY NAME on every line. moved=0 must be
	// readable as "0 of N", never as a bare silence that is indistinguishable from a hook that
	// was never called -- which is exactly how the previous five attempts at this looked.
	if ((s_seen % 20000) == 0)
		SB_LOGC("interp",
		        "SNAPSHOT pop: samples=%ld moved=%ld (%.1f%%) maxStep=%.3f by \"%s\" "
		        "| COVERS every object dispatched with CUE_MOVE (incl. perform() overriders); "
		        "does NOT cover objects never given the MOVE cue, nor non-transform state "
		        "(JPA particles, dash/ghost trails)",
		        s_seen, s_moved, 100.0 * (double)s_moved / (double)s_seen,
		        (double)(s_maxD2 > 0.0f ? std::sqrt(s_maxD2) : 0.0f), s_maxName);
}
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
