#include <System/PerformList.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>   // JSU_BE32
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif

void TPerformList::forEachPerform(
    JGadget::TSingleLinkList<TPerformLink, 0>::iterator b,
    JGadget::TSingleLinkList<TPerformLink, 0>::iterator e,
    JDrama::TGraphics* param_3, u32 param_4)
{
	for (JGadget::TSingleLinkList<TPerformLink, 0>::iterator it = b; it != e;
	     ++it) {
		it->unk4->testPerform(param_4 & it->unk8, param_3);
	}
}

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
		char acStack_6c[80];
		stream.readString(acStack_6c, 80);

		JDrama::TViewObj* obj
		    = JDrama::TNameRefGen::search<JDrama::TViewObj>(acStack_6c);
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
#endif
		if (obj)
			push_back(obj, uVar5);
	}
}

void TPerformList::push_back(const char* param_1, u32 param_2)
{
	JDrama::TViewObj* obj
	    = JDrama::TNameRefGen::search<JDrama::TViewObj>(param_1);

	getChildren().Push_back(new TPerformLink(obj, param_2));
}

void TPerformList::push_back(JDrama::TViewObj* param_1, u32 param_2)
{
	getChildren().Push_back(new TPerformLink(param_1, param_2));
}
