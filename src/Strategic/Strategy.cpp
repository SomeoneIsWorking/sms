#include <Strategic/Strategy.hpp>
#include <Strategic/ObjHitCheck.hpp>
#include <macros.h>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif

TStrategy* gpStrategy;

void TIdxGroupObj::loadSuper(JSUMemoryInputStream& stream)
{
	JDrama::TViewObjPtrListT<THitActor>::loadSuper(stream);
	stream.read(&unk20, 4);
#ifdef SMS_NATIVE_PLATFORM
	// unk20 is the big-endian group index read via the raw read(&x,4) (no
	// byteswap). It indexes TStrategy::unk10[16] in TStrategy::load
	// (unk10[ref->unk20] = ref); left unswapped it is huge garbage -> OOB wild
	// write -> SEGV. Swap to host.
	unk20 = __builtin_bswap32(unk20);
#endif
}

TStrategy::TStrategy(const char* name)
    : JDrama::TViewObj(name)
    , unk50(0)
{
	for (int i = 0; i < ARRAY_COUNT(unk10); ++i)
		unk10[i] = nullptr;
}

void TStrategy::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);
	new TObjHitCheck();

	int count = stream.readU32();
	for (int i = 0; i < count; ++i) {

		JSUMemoryInputStream stream2(nullptr, 0);
		TIdxGroupObj* ref
		    = (TIdxGroupObj*)JDrama::TNameRef::genObject(stream, stream2);
		if (ref) {
			unk10[15] = ref;
			ref->load(stream2);
			unk10[15] = nullptr;

			unk10[ref->unk20] = ref;
		}
	}

	gpStrategy = this;
}

void TStrategy::loadAfter()
{
	JDrama::TViewObj::loadAfter();
	for (int i = 0; i < 16; ++i)
		if (unk10[i])
			unk10[i]->loadAfter();
}

JDrama::TNameRef* TStrategy::searchF(u16 key, const char* name)
{
	JDrama::TNameRef* ref = JDrama::TViewObj::searchF(key, name);
	if (ref)
		return ref;

	for (int i = 0; i < ARRAY_COUNT(unk10); ++i) {
		if (unk10[i]) {
			JDrama::TNameRef* r = unk10[i]->searchF(key, name);
			if (r)
				return r;
		}
	}

	return nullptr;
}

void TStrategy::perform(u32 param_1, JDrama::TGraphics* param_2)
{

	if ((param_1 & 3) != 0) {
		if (unk10[0] != (TIdxGroupObj*)0x0)
			unk10[0]->testPerform(param_1, param_2);

		if (unk10[3] != (TIdxGroupObj*)0x0)
			unk10[3]->testPerform(param_1, param_2);

		if (unk10[4] != (TIdxGroupObj*)0x0)
			unk10[4]->testPerform(param_1, param_2);

		if (unk10[0xb] != (TIdxGroupObj*)0x0)
			unk10[0xb]->testPerform(param_1, param_2);

		if (unk10[6] != (TIdxGroupObj*)0x0)
			unk10[6]->testPerform(param_1, param_2);

		if (unk10[9] != (TIdxGroupObj*)0x0)
			unk10[9]->testPerform(param_1, param_2);
	}

#ifdef SMS_NATIVE_PLATFORM
	// SB_STRAT_DUMP=1: one-shot — list each walked group's index, child count, and child names,
	// to see whether NPC actors are entered into the scene buffer via the strategy group walk.
	if ((param_1 & 8) && getenv("SB_STRAT_DUMP")) {
		static int once = 0;
		if (once < 1) { ++once;
			static const int gi[] = { 0, 3, 4, 5, 0xb, 6, 9 };
			for (int g : gi) {
				TIdxGroupObj* grp = unk10[g];
				if (!grp) { std::fprintf(stderr, "[strat] group[%d] = null\n", g); continue; }
				long n = 0;
				for (auto it = grp->getChildren().begin(); it != grp->getChildren().end(); ++it) ++n;
				std::fprintf(stderr, "[strat] group[%d] '%s' children=%ld\n", g, grp->getName(), n);
				long k = 0;
				for (auto it = grp->getChildren().begin(); it != grp->getChildren().end() && k < 40; ++it, ++k)
					std::fprintf(stderr, "[strat]    child[%ld] %s\n", k, it->getName());
			}
		}
	}
#endif

	if ((param_1 & 8) != 0) {
		if (unk10[0] != (TIdxGroupObj*)0x0)
			unk10[0]->testPerform(param_1, param_2);

		if (unk10[3] != (TIdxGroupObj*)0x0)
			unk10[3]->testPerform(param_1, param_2);

		if (unk10[4] != (TIdxGroupObj*)0x0)
			unk10[4]->testPerform(param_1, param_2);

		if (unk10[5] != (TIdxGroupObj*)0x0)
			unk10[5]->testPerform(param_1, param_2);

		if (unk10[0xb] != (TIdxGroupObj*)0x0)
			unk10[0xb]->testPerform(param_1, param_2);

		if (unk10[6] != (TIdxGroupObj*)0x0)
			unk10[6]->testPerform(param_1, param_2);

#ifdef SMS_NATIVE_PLATFORM
		// FAITHFUL FIX (map-gone-with-NPCs root cause): do NOT draw the NPC group (unk10[9],
		// "ＮＰＣグループ") here. On real hardware the draw is dispatched by the master GX perform
		// list (MarDirectorPreEntry::preEntry), which NEVER pushes ストラテジ — it draws the NPCs
		// exclusively through "コンダクター" (0x204) into the dedicated ChrOpa/ChrXlu buffers. The
		// NPC actors are scene-tree children of ＮＰＣグループ AND  to their TLiveManager
		// (drawn by the conductor), so sms-boot's hand-driven scene->perform(0x8) — which walks the
		// WHOLE scene tree (both ストラテジ and コンダクター) into one shared draw buffer — entered
		// every NPC's J3DMatPacket TWICE per frame. A J3DMatPacket has a single `next` pointer:
		// the conductor's re-entry calls drawClear() (nulls next) and entryMatSort's isSame-dedup
		// returns without re-linking, truncating the shared chain at the (head-most) NPC packets and
		// orphaning the entire map (collapsed to ~16 shapes, Mario+sky only). Drawing the NPCs only
		// via the conductor (the real-HW path) restores single-entry, so the map AND NPCs render.
		// The map itself stays drawn via ストラテジ's マップグループ (unk10[0]); only group[9] —
		// which the perform list never draws via ストラテジ — is suppressed here.
		if (false)
#endif
		if (unk10[9] != (TIdxGroupObj*)0x0)
			unk10[9]->testPerform(param_1, param_2);
	}
}
