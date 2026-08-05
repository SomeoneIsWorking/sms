#include <System/PerformList.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>   // JSU_BE32
#ifdef SMS_NATIVE_PLATFORM
#include <sb_log.h>
#include <cstdio>
#include <cstdlib>
#endif

#ifdef SMS_NATIVE_PLATFORM
long TPerformList::sSbRecurseCount = 0;
#endif

void TPerformList::forEachPerform(
    JGadget::TSingleLinkList<TPerformLink, 0>::iterator b,
    JGadget::TSingleLinkList<TPerformLink, 0>::iterator e,
    JDrama::TGraphics* graphics, u32 cue)
{
	for (JGadget::TSingleLinkList<TPerformLink, 0>::iterator it = b; it != e;
	     it++) {
		it->perform(cue, graphics);
	}
}

#ifdef SMS_NATIVE_PLATFORM
// Game-native 60fps interpolation: capture every child's transform before the movement phase.
//
// Called EXPLICITLY from TMarDirector::direct() rather than inferred from a phase bit inside
// forEachPerform -- the first attempt gated on `(mask & link->unk8) & 1` and never fired, because
// the movement list's dispatch mask is not that simple. Guessing at bit semantics produced a hook
// that silently did nothing; an explicit call cannot.
//
// Dispatched through a virtual on TViewObj so it reaches every object whatever it overrides: 231
// classes override perform() and most do not chain to their base, so a hook inside perform()
// misses them (TMario included).
void TPerformList::snapshotInterp()
{
	long n = 0;
	for (JGadget::TSingleLinkList<TPerformLink, 0>::iterator it = getChildren().begin();
	     it != getChildren().end(); ++it) {
		it->unk4->sbSnapshotInterp();
		++n;
	}
	// A snapshot pass that visits nothing is indistinguishable from one that works, so say how
	// many objects it actually reached at this level.
	SB_LOG_EVERY("interp", 600, "snapshotInterp visited %ld direct children | nested-list recursions so far=%ld",
	             n, sSbRecurseCount);
	// NAME them. Three attempts have now guessed at what sits in this list and been wrong; the
	// list can simply say what it holds.
	{
		static int s_named = 0;
		if (!s_named) {
			s_named = 1;
			for (JGadget::TSingleLinkList<TPerformLink, 0>::iterator it = getChildren().begin();
			     it != getChildren().end(); ++it) {
				const char* nm = it->unk4 ? it->unk4->getName() : "(null)";
				SB_LOGC("interp", "  movement child: %s", nm ? nm : "(unnamed)");
			}
		}
	}
}
#endif

void TPerformList::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	forEachPerform(getChildren().begin(), getChildren().end(), param_2,
	               param_1);
}

void TPerformList::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);

#ifdef SMS_NATIVE_PLATFORM
	// Own env (SB_PL_DBG): this dump is ~150 lines per scene load — keep it off the
	// per-frame SB_J3D_DBG channel.
	static int dbg = -1;
	if (dbg < 0) { const char* e = getenv("SB_PL_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
#endif
	while (stream.getLength() - stream.getPosition() > 0) {
		stream.readString(elementName, 80);

		obj = JDrama::TNameRefGen::search<JDrama::TViewObj>(elementName);

		u32 value = stream.readU32();

		// TODO: feels fake and stack is missing, needs more tinkering
		u32 value;
		stream.read(&value, 4);
		// The perform-list data file stores each entry's filter as a big-endian dword;
		// JSUInputStream::read is a raw readData (no byteswap — see the JSU_BE16 usage in
		// readString). On a little-endian host the raw read yields a byteswapped filter
		// (e.g. the calc op bit 0x2 read as 0x2000000, movement 0x1 as 0x1000000), which
		// no actor/manager acts on → the whole movement/calc perform pass was inert and
		// NPC calcRootMatrix never ran. JSU_BE32 is a no-op on big-endian, bswap on LE.
		value = JSU_BE32(value);
		u32 uVar5 = value;
		if (value & 1)
			uVar5 = value | 0x3000;
#ifdef SMS_NATIVE_PLATFORM
		// Draw-phase keystone: dump every (list, entry, filter) so we can see which loaded
		// list registers "camera 1" and whether any entry carries the control bit 0x1.
		if (dbg)
			fprintf(stderr, "[plload] list='%s' entry='%s' filter=0x%x eff=0x%x found=%d\n",
			        getName() ? getName() : "?", acStack_6c, value, uVar5, obj != nullptr);
		// FAIL LOUD (not fail fast -- GC ships PerformLists.bin with entries that are
		// legitimately absent depending on scene/stage config, e.g. optional emitter
		// groups). Silently dropping a name-search miss here previously hid the TMapObjWave
		// ("波") registration-order bug for an unbounded number of sessions; log every drop
		// unconditionally (not gated on SB_PL_DBG) so the next miss is visible without
		// re-deriving this diagnostic.
		if (!obj)
			fprintf(stderr,
			        "[plload] DROPPED: list='%s' entry='%s' not found in NameRef tree "
			        "-- perform-list load will not dispatch to it\n",
			        getName() ? getName() : "?", acStack_6c);
#endif
		if (obj)
			push_back(obj, value);
	}
}

void TPerformList::push_back(const char* param_1, u32 param_2)
{
	JDrama::TViewObj* obj
	    = (JDrama::TViewObj*)JDrama::TNameRefGen::search2(param_1);

	Push_back(new TPerformLink(obj, param_2));
}

void TPerformList::push_back(JDrama::TViewObj* param_1, u32 param_2)
{
	Push_back(new TPerformLink(param_1, param_2));
}
