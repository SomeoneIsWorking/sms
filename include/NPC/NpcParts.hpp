#ifndef NPC_PARTS_HPP
#define NPC_PARTS_HPP

#include <JSystem/JDrama/JDRGraphics.hpp>

class J3DGXColorS10;
class TBaseNPC;
class MActor;
class TSharedParts;

class TNpcParts {
public:
	TNpcParts(u32, const J3DGXColorS10*, TBaseNPC*);

	void addJellyFishParts(f32);
	void setPartsAnmFrame(f32);
	MActor* getPartsMActor(int, int);
	void partsFrameUpdate();
	void partsPerform(u32, JDrama::TGraphics*);

public:
	// [12][2] = 12 part slots × 2 sub-models. Every real access indexes it this way:
	// the ctor's main loop writes unk0[i<12][j<2], partsFrameUpdate uses &unk0[5][1], and
	// getPartsMActor indexes unk0[part][submodel]. (The decomp header originally had this
	// as [2][12], and the ctor's CLEAR loop used the matching [i<2][j<12] bounds — but the
	// real indexing is [12][2], so the [2][12] form made the ctor's writes run far out of
	// bounds and corrupt the adjacent heap, e.g. a freshly-new'd TSharedParts. Total element
	// count (24 ptrs -> 0x60) is unchanged, matching the original struct size.)
	/* 0x0 */ TSharedParts* unk0[12][2];
	/* 0x60 */ TBaseNPC* unk60;
};

#endif
