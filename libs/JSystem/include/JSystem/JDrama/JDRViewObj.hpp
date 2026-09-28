#ifndef JDR_VIEW_OBJ_HPP
#define JDR_VIEW_OBJ_HPP

#include <JSystem/JDrama/JDRFlag.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <JSystem/JDrama/JDRNameRef.hpp>
#include <JSystem/JGadget/std-list.hpp>

// Named perform() cues (upstream doldecomp/sms, 2026-07). These are the same
// bit values the perform(u32) flag argument always used, just named — adopted
// during the upstream rebase so new upstream TUs (GateKeeper etc.) compile and
// so our own perform() implementations can drop the magic numbers over time.
enum {
	/// Gameplay logic
	CUE_MOVE = 0x1,
	/// Animations
	CUE_CALC_ANIM = 0x2,
	/// View-dependent calculations
	CUE_CALC_VIEW = 0x4,
	/// GX commands
	CUE_DRAW = 0x8,
	/// Commit the projection matrix to GX
	CUE_SET_PROJECTION = 0x10,
	/// Commit lights to GX
	CUE_LIGHT = 0x20,
	// TODO: uncertain
	CUE_DRAW_INIT = 0x80,
	// TODO: uncertain
	CUE_DRAW_STAGE_END = 0x100,
	/// "Enter" models into draw buffers
	CUE_ENTRY = 0x200,
	/// Set draw buffers to global state
	CUE_SET_DRAW_BUFFER = 0x400,
	// TODO: uncertain
	CUE_MOVEMENT_GATE_A = 0x1000,
	// TODO: uncertain
	CUE_MOVEMENT_GATE_B = 0x2000,
	/// Advance per-model visual effects such as Mario's cap tremble.
	CUE_UPDATE_MODEL_EFFECTS = 0x10000000,

	CUE_ALL = 0xffffffff,
};

namespace JDrama {

class TViewObj : public TNameRef {
public:
	TViewObj(const char* name = "<TViewObj>")
	    : TNameRef(name)
	{
	}

	void testPerform(u32, TGraphics*);

	virtual void perform(u32, TGraphics*) = 0;

#ifdef SMS_NATIVE_PLATFORM
	// Game-native 60fps interpolation: capture this object's transform as it
	// stands BEFORE the movement phase runs, so a sub-frame can render
	// lerp(prev, cur, alpha).
	//
	// It is a SEPARATE virtual rather than a hook inside perform() on purpose:
	// 231 classes override perform() and most do not chain to their base, so
	// anything placed there silently misses them (TMario included). Default
	// no-op; TActor implements it.
	//
	// It is driven from testPerform() -- non-virtual, and the single funnel
	// EVERY container dispatches through (TPerformList::forEachPerform,
	// TViewObjPtrListT::perform, TStrategy, TObjManager/enemymanager,
	// TViewConnecter, TScreen, TDirector). Five attempts to reach actors by
	// walking the object graph from the movement list each stopped at a
	// container type that had not been anticipated, and TStrategy::perform is a
	// sixth that none of them would have found. Hooking the funnel needs no
	// knowledge of container types at all: an object that is performed passes
	// through here by construction, and nothing can override its way around a
	// non-virtual.
	virtual void sbSnapshotInterp() { }

	// Tick boundary for the snapshot above. Opened once per logic tick by
	// TMarDirector::direct() before the movement dispatch; TActor snapshots
	// only on the first dispatch of each tick, so an object performed twice
	// cannot overwrite (prev) with (cur) and silently flatten interpolation.
	static unsigned long sSbInterpTick;
	static void sbBeginInterpTick() { ++sSbInterpTick; }
#endif

public:
	/* 0xC */ TFlagT<u16> unkC;
};

}; // namespace JDrama

#endif
