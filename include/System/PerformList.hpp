#ifndef SYSTEM_PERFORM_LIST_HPP
#define SYSTEM_PERFORM_LIST_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JGadget/singlelinklist.hpp>

class TPerformLink {
public:
	TPerformLink(JDrama::TViewObj* param_1, u32 param_2)
	    : mPerformer(param_1)
	    , mCueFilter(param_2)
	{
	}

	void perform(u32 cue, JDrama::TGraphics* graphics)
	{
		mPerformer->testPerform(mCueFilter & cue, graphics);
	}

public:
	/* 0x0 */ JGadget::TSingleLinkListNode unk0;
	/* 0x4 */ JDrama::TViewObj* mPerformer;
	/* 0x8 */ u32 mCueFilter;
};

// NOTE: fabricated name, but a class of this shape must exist. Two independent
// results need one more class between TPerformList and TSingleNodeLinkList than
// a direct derivation gives:
//   * TPerformList::~TPerformList holds two nested `if (subobject != 0)` guards
//     before it calls ~TSingleNodeLinkList, and MWCC emits exactly one guard
//     per inlined destructor level;
//   * TMarDirector::TMarDirector calls Initialize_ out of line five times, and
//     Initialize_ is only pushed past the last inline pass with the extra
//     level.
// Both go to 100% with it.
class TPerformLinkList : public JGadget::TSingleLinkList<TPerformLink, 0> { };

class TPerformList : public JDrama::TViewObj, public TPerformLinkList {
public:
	TPerformList() { }
	TPerformList(const char* name)
	    : JDrama::TViewObj(name)
	{
	}

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32, JDrama::TGraphics*);

	// Filters are &ed with the first param in perform
	void push_back(JDrama::TViewObj* object, u32 filter);
	void push_back(const char* name, u32 filter);

#ifdef SMS_NATIVE_PLATFORM
	// Snapshot every child's transform for 60fps interpolation; see the definition.
	void snapshotInterp();
	// The perform lists are a TREE -- a child can itself be a TPerformList. Without this override
	// the base no-op stops the walk at the first nested list and every grandchild is silently
	// skipped, which is exactly how the first version of this hook ended up snapshotting nothing.
	void sbSnapshotInterp() override { ++sSbRecurseCount; snapshotInterp(); }
	// Does the recursion actually descend into the group objects? 0 means the groups are not
	// TPerformLists and the walk stops at them.
	static long sSbRecurseCount;
#endif
	void forEachPerform(JGadget::TSingleLinkList<TPerformLink, 0>::iterator,
	                    JGadget::TSingleLinkList<TPerformLink, 0>::iterator,
	                    JDrama::TGraphics*, u32);

	JGadget::TSingleLinkList<TPerformLink, 0>& getChildren() { return *this; }
};

#endif
