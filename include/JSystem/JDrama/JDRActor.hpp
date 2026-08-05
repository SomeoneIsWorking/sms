#ifndef JDR_ACTOR_HPP
#define JDR_ACTOR_HPP

#include <JSystem/JDrama/JDRPlacement.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <JSystem/JStage/JSGActor.hpp>

namespace JDrama {

class TCharacter;

/**
 * @brief A scene graph object which is visible and has a position, rotation &
 * scale.
 * @details Basically, anything you can see on the scene and say "yep, that's a
 * thing": enemies, npcs, map objects, the player, etc.
 */
class TActor : public TPlacement, public JStage::TActor {
public:
	TActor(const char* name)
	    : TPlacement(name)
	{
		mScaling.setAll(1.0f);
		mRotation.setAll(0.0f);

		unk3C = nullptr;
		unk40 = nullptr;
	}

	~TActor();

	virtual int getType() const { return 1; }
	virtual void load(JSUMemoryInputStream&);
	void issueGXLight(u32, JDrama::TGraphics*);

	virtual void perform(u32, TGraphics*);

	virtual void JSGGetTranslation(Vec*) const;
	virtual void JSGSetTranslation(const Vec&);
	virtual void JSGGetScaling(Vec*) const;
	virtual void JSGSetScaling(const Vec&);
	virtual void JSGGetRotation(Vec*) const;
	virtual void JSGSetRotation(const Vec&);

	// fabricated
	const JGeometry::TVec3<f32>& getRotation() const { return mRotation; }
	const JGeometry::TVec3<f32>& getScaling() const { return mScaling; }

	void setCharacter(TCharacter* character) { unk3C = character; }

public:
	/* 0x24 */ JGeometry::TVec3<f32> mScaling;
	/* 0x30 */ JGeometry::TVec3<f32> mRotation;
	/* 0x3C */ TCharacter* unk3C;
	/* 0x40 */ TViewObj* unk40;

#ifdef SMS_NATIVE_PLATFORM
	// Interpolation `prev`: this actor's transform before the current tick's movement. Lives on
	// TActor rather than TLiveActor so it covers map objects and anything else that moves, not
	// only live actors. Appended and native-only, so the guest-offset comments above stay correct.
	void sbSnapshotInterp() override {
		// ONCE per logic tick. An object can be dispatched with the MOVE cue more than once in a
		// tick (a manager may perform its actors as well as the list holding it); a second
		// snapshot would copy the post-movement transform over (prev), making prev == cur and
		// collapsing interpolation to a no-op -- which renders exactly like a correct
		// implementation and so would never show up as a visual defect.
		if (mSbPrevTick == sSbInterpTick)
			return;
		// Motion probe over the SNAPSHOT population -- every object dispatched with CUE_MOVE,
		// TMario included. Called HERE, on entry, because mSbPrevPosition still holds the PREVIOUS
		// tick's transform until the lines below overwrite it, so (cur - prev) is exactly the step
		// interpolation would have to cover.
		//
		// Out-of-line (JDRActor.cpp) so this header stays free of the sb_log dependency -- it is
		// included by boot_stubs TUs that do not carry shims/ on their include path.
		if (mSbPrevValid)
			sbInterpMotionProbe(getName(), mSbPrevPosition, mPosition);
		mSbPrevTick     = sSbInterpTick;
		mSbPrevPosition = mPosition;
		mSbPrevRotation = mRotation;
		mSbPrevValid    = true;
	}
	JGeometry::TVec3<f32> mSbPrevPosition;
	JGeometry::TVec3<f32> mSbPrevRotation;
	bool mSbPrevValid = false;   // nothing may interpolate from an unwritten transform
	unsigned long mSbPrevTick = ~0UL;

	// SB_LOG=interp. Designed negative-first: an inert prev/cur pair renders identically to a
	// working one, so "interpolation changed nothing" cannot by itself distinguish a correct
	// implementation from a snapshot that captures nothing. The line it prints therefore always
	// carries its DENOMINATOR and NAMES the largest mover -- five earlier attempts at this hook
	// silently reached no actor, and a named biggest-mover is what makes that visible rather than
	// inferable.
	static void sbInterpMotionProbe(const char* name,
	                                const JGeometry::TVec3<f32>& prev,
	                                const JGeometry::TVec3<f32>& cur);
#endif
};

} // namespace JDrama

#endif
