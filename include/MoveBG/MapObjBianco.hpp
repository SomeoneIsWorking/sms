#ifndef MOVE_BG_MAP_OBJ_BIANCO_HPP
#define MOVE_BG_MAP_OBJ_BIANCO_HPP

#include <MoveBG/MapObjBase.hpp>

class JAISound;

// Bianco Hills water/wind decorations. RE'd US GMSE01 (wide-RE sweep 2026-07-17).
// TBiancoWatermill (big horizontal wheel) ported; the vertical watermill, bell, and mini
// windmill in this TU have uncertain string/matrix/water-physics parts — deferred.
class TBiancoWatermill : public TMapObjBase {
public:
	TBiancoWatermill(const char* name = "水車（ビアンコ大）");

	virtual void initMapObj();
	virtual void control();
	virtual u32 touchWater(THitActor*);
	void turnByEnemy(THitActor*, const TBGCheckData*);

public:
	/* 0x138 */ f32 mSpinSpeed;
	/* 0x13C */ JAISound* mSound;
};

#endif
