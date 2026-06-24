#include <Map/Map.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif
#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <Map/MapModel.hpp>
#include <Map/MapWarp.hpp>
#include <Map/MapXlu.hpp>
#include <Map/MapCollisionEntry.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

TMap* gpMap;

static void initOption() { }

static void initSirena() { }

static void initMonte() { }

static void initMare() { }

static void initPinnaParco() { }

static void initPinnaBeach() { }

static void initBianco() { }

static void initDolpic() { }

static void initStageCommon() { }

static void initStage() { }

void TMap::updateDelfino() { }

void TMap::updateMonte() { }

void updateRicco() { }

void TMap::update() { }

TBGCheckData* TMap::getIllegalCheckData()
{
	return &TMapCollisionData::mIllegalCheckData;
}

bool TMap::isInArea(f32 param_1, f32 param_2) const
{
	if (-mCollisionData->mGridExtentX < param_1
	    && param_1 < mCollisionData->mGridExtentX
	    && -mCollisionData->mGridExtentY < param_2
	    && param_2 < mCollisionData->mGridExtentY)
		return true;

	return false;
}

const TBGCheckData* TMap::intersectLine(const JGeometry::TVec3<f32>& param_1,
                                        const JGeometry::TVec3<f32>& param_2,
                                        bool param_3,
                                        JGeometry::TVec3<f32>* param_4) const
{
	mCollisionData->intersectLine(param_1, param_2, param_3, param_4);
}

bool TMap::isTouchedOneWall(const JGeometry::TVec3<f32>&, f32) const { }

bool TMap::isTouchedOneWall(f32 x, f32 y, f32 z, f32 radius) const
{
	return isTouchedOneWallAndMoveXZ(&x, y, &z, radius);
}

bool TMap::isTouchedOneWallAndMoveXZ(f32* x, f32 y, f32* z, f32 radius) const
{
	TBGWallCheckRecord record(*x, y, *z, radius, 1, 0);

	int r = mCollisionData->checkWalls(&record);
	if (r != 0 ? true : false) {
		*x = record.mCenter.x;
		*z = record.mCenter.z;
		return true;
	} else {
		return false;
	}
}

bool TMap::isTouchedWallsAndMoveXZ(TBGWallCheckRecord* record) const
{
	return mCollisionData->checkWalls(record) != 0 ? true : false;
}

f32 TMap::checkRoofIgnoreWaterThrough(f32 x, f32 y, f32 z,
                                      const TBGCheckData** result) const
{
	return mCollisionData->checkRoof(
	    x, y, z, TMapCollisionData::IGNORE_WATER_THROUGH, result);
}

f32 TMap::checkRoof(f32 x, f32 y, f32 z, const TBGCheckData** result) const
{
	return mCollisionData->checkRoof(x, y, z, 0, result);
}

f32 TMap::checkRoof(const JGeometry::TVec3<f32>& pos,
                    const TBGCheckData** param_2) const
{
	return mCollisionData->checkRoof(pos.x, pos.y, pos.z, 0, param_2);
}

f32 TMap::checkGroundIgnoreWaterThrough(f32 x, f32 y, f32 z,
                                        const TBGCheckData** result) const
{
	return mCollisionData->checkGround(
	    x, y, z, TMapCollisionData::IGNORE_WATER_THROUGH, result);
}

f32 TMap::checkGroundIgnoreWaterSurface(f32 x, f32 y, f32 z,
                                        const TBGCheckData** result) const
{
	return mCollisionData->checkGround(
	    x, y, z, TMapCollisionData::IGNORE_WATER_SURFACE, result);
}

f32 TMap::checkGroundIgnoreWaterSurface(const JGeometry::TVec3<f32>& pos,
                                        const TBGCheckData** result) const
{
	return mCollisionData->checkGround(
	    pos.x, pos.y, pos.z, TMapCollisionData::IGNORE_WATER_SURFACE, result);
}

f32 TMap::checkGroundExactY(f32 x, f32 y, f32 z,
                            const TBGCheckData** result) const
{
	return mCollisionData->checkGround(x, y - -78.0f, z, 0, result);
}

f32 TMap::checkGroundExactY(const JGeometry::TVec3<f32>& pos,
                            const TBGCheckData** result) const
{
	return mCollisionData->checkGround(pos.x, pos.y - -78.0f, pos.z, 0, result);
}

f32 TMap::checkGround(const JGeometry::TVec3<f32>& pos,
                      const TBGCheckData** result) const
{
	return mCollisionData->checkGround(pos.x, pos.y, pos.z, 0, result);
}

f32 TMap::checkGround(f32 x, f32 y, f32 z, const TBGCheckData** result) const
{
	return mCollisionData->checkGround(x, y, z, 0, result);
}

void TMap::changeModel(s16 param_1) const { mWarp->changeModel(param_1); }

void TMap::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	if (param_1 & 1) {
		update();
		mCollisionData->initMoveCollision();
		mWarp->watchToWarp();
	}

	if (param_1 & 0x200) {
		if ((param_1 & 0x2000000)) {
			if (!mXlu->changeXluJoint(1))
				return;
		} else if ((param_1 & 0x4000000)) {
			if (!mXlu->changeXluJoint(0))
				return;
		} else {
			mXlu->changeNormalJoint();
		}
	}

	if (param_1 & 8)
		draw(param_1, param_2);

	mModelManager->perform(param_1, param_2);
}

void TMap::loadAfter()
{
	JDrama::TViewObj::loadAfter();
	initStage();
}

void TMap::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);
	mXlu->init(stream);
	mModelManager->init();
	mCollisionData->init(stream);
	mWarp->initModel();
	mWarp->init(stream);
	mModelManager->mCollision->setUp();
#ifdef SMS_NATIVE_PLATFORM
	if (getenv("SB_DEATH_DBG")) {
		const TBGCheckData* g = nullptr;
		fprintf(stderr, "[mapcol] gridExtent(%.0f,%.0f) numTri=%u added=%u\n",
		        mCollisionData->mGridExtentX, mCollisionData->mGridExtentY,
		        mCollisionData->unk1C, mCollisionData->unk34);
		{
			TMapCollisionBase* cb = mModelManager->mCollision;
			fprintf(stderr, "[mapcol] static vtxCount=%u (>=350:%d skips MTX xform) "
			        "unk20 trans(%.1f %.1f %.1f) raw vtx0(%.1f %.1f %.1f)\n",
			        cb->unk10, (int)(cb->unk10 >= 0x15E),
			        cb->unk20[0][3], cb->unk20[1][3], cb->unk20[2][3],
			        cb->unk14 ? cb->unk14[0].x : -1, cb->unk14 ? cb->unk14[0].y : -1,
			        cb->unk14 ? cb->unk14[0].z : -1);
		}
		// Find a GROUND triangle (normal.y>0.9) and test checkGround at its
		// centroid — settles whether checkGround can find linked triangles.
		const TBGCheckData* tg = mCollisionData->unk28;
		for (int i = 0; i < (int)mCollisionData->unk34; ++i) {
			const TBGCheckData& t = tg[i];
			if (t.mNormal.y > 0.9f) {
				f32 cx = (t.mPoint1.x + t.mPoint2.x + t.mPoint3.x) / 3.0f;
				f32 cz = (t.mPoint1.z + t.mPoint2.z + t.mPoint3.z) / 3.0f;
				f32 cy = (t.mPoint1.y + t.mPoint2.y + t.mPoint3.y) / 3.0f;
				const TBGCheckData* r = nullptr;
				f32 gy = checkGround(cx, cy + 200.0f, cz, &r);
				fprintf(stderr, "[mapcol] GROUND tri%d centroid(%.0f %.0f %.0f) "
				        "-> checkGround=%.1f found=%s\n", i, cx, cy, cz, gy,
				        (gy < 9000000.0f) ? "YES" : "NO");
				break;
			}
		}
		(void)g;
		const TBGCheckData* tris = mCollisionData->unk28;
		for (int i = 0; i < 6 && i < (int)mCollisionData->unk34; ++i) {
			const TBGCheckData& t = tris[i];
			fprintf(stderr, "[mapcol]   tri%d bg=%d minY=%.0f maxY=%.0f "
			        "p1(%.0f %.0f %.0f) p2(%.0f %.0f %.0f) p3(%.0f %.0f %.0f) n(%.2f %.2f %.2f)\n",
			        i, (int)t.mBGType, t.mMinY, t.mMaxY,
			        t.mPoint1.x, t.mPoint1.y, t.mPoint1.z,
			        t.mPoint2.x, t.mPoint2.y, t.mPoint2.z,
			        t.mPoint3.x, t.mPoint3.y, t.mPoint3.z,
			        t.mNormal.x, t.mNormal.y, t.mNormal.z);
		}
	}
#endif
}

TMap::TMap(const char* name)
    : JDrama::TViewObj(name)
{
	mCollisionData = new TMapCollisionData;
	mModelManager  = new TMapModelManager("地形モデル管理");
	mWarp          = new TMapWarp;
	mXlu           = new TMapXlu;
	unk20          = 0;

	gpMap = this;
}

TMap::~TMap() { }
