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
#ifdef SMS_NATIVE_PLATFORM
extern "C" int sb_boot_capture_phase();
#endif

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

TMap* gpMap;

static void initMonte()
{
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group
	    = JDrama::TNameRefGen::search<
	        JDrama::TViewObjPtrListT<JDrama::TViewObj> >("マップグループ");

	TMapStaticObj* obj = new TMapStaticObj("水インダイレクト");
	obj->init("SeaIndirect");
	group->getChildren().push_back(obj);

	if (gpMarDirector->getCurrentStage() == 0
	    || gpMarDirector->getCurrentStage() == 2
	    || gpMarDirector->getCurrentStage() == 5
	    || gpMarDirector->getCurrentStage() == 6) {
		SMS_LoadParticle("/scene/map/pollution/ms_newfire_b.jpa", 0x1DC);
		SMS_LoadParticle("/scene/map/pollution/ms_newfire_a.jpa", 0x65);
	}

	if (gpMarDirector->getCurrentStage() == 1
	    || gpMarDirector->getCurrentStage() == 3
	    || gpMarDirector->getCurrentStage() == 5
	    || gpMarDirector->getCurrentStage() == 7) {
		SMS_LoadParticle("/scene/map/map/ms_monte_yuge.jpa", 0x156);
	}
}

static void initMare()
{
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group
	    = JDrama::TNameRefGen::search<
	        JDrama::TViewObjPtrListT<JDrama::TViewObj> >("マップグループ");

	if (gpMarDirector->getCurrentStage() == 5) {
		TMapStaticObj* gate = new TMapStaticObj("マーレ５ＥＸゲート");
		gate->init("Mare5ExGate");
		group->getChildren().push_back(gate);
	}

	if (gpMarDirector->getCurrentStage() == 0) {
		SMS_LoadParticle("/scene/map/map/ms_mare_objup_a.jpa",
		                 MAP_MAP_MS_MARE_OBJUP_A);
		SMS_LoadParticle("/scene/map/map/ms_mare_objup_b.jpa",
		                 MAP_MAP_MS_MARE_OBJUP_B);
	}

	if (gpMarDirector->getCurrentStage() != 0
	    && gpMarDirector->getCurrentStage() != 0) {
		for (int i = 1; i < 8; ++i)
			TMapObjBase::newAndInitBuildingCollisionWarp(i, nullptr)->setUp();
	}

	{
		TMareEventDepressWall* event
		    = new TMareEventDepressWall("イベント(マーレへこむ壁)");
		event->init1stEvent();
		group->getChildren().push_back(event);
	}
	{
		TMareEventDepressWall* event
		    = new TMareEventDepressWall("イベント(マーレへこむ壁)");
		event->init2ndEvent();
		group->getChildren().push_back(event);
	}
	{
		TMareEventDepressWall* event
		    = new TMareEventDepressWall("イベント(マーレへこむ壁)");
		event->init3rdEvent();
		group->getChildren().push_back(event);
	}
}

#pragma dont_inline on
static void initPinnaParco()
{
	J3DModel* model = new J3DModel(
	    gpMap->getModelManager()->getJointModel(0)->getModelData(), 0, 1);
	MActor* actor = new MActor(gpMap->getModelManager()->getMActorAnmData());
	actor->setModel(model, 0);
	TMapModelActor* mapModelActor = new TMapModelActor("ピンナ鏡用地形モデル");
	mapModelActor->setActor(actor);
	TMapObjBase::joinToGroup("鏡シーン", mapModelActor);
}
#pragma dont_inline off

static void initStageCommon()
{
	JDrama::TViewObjPtrListT<JDrama::TViewObj>* group
	    = JDrama::TNameRefGen::search<
	        JDrama::TViewObjPtrListT<JDrama::TViewObj> >(
	        "インダイレクトシーン");
	JDrama::TNameRefGen::search<JDrama::TViewObjPtrListT<JDrama::TViewObj> >(
	    "マップグループ");

	if (gpMarDirector->getCurrentMap() == 4
	    || gpMarDirector->getCurrentMap() == 3
	    || gpMarDirector->getCurrentMap() == 0xD
	    || gpMarDirector->getCurrentMap() == 9
	    || gpMarDirector->getCurrentMap() == 5
	    || gpMarDirector->getCurrentMap() == 6
	    || gpMarDirector->getCurrentMap() == 0x14
	    || gpMarDirector->getCurrentMap() <= 1) {
		TMapStaticObj* sea = new TMapStaticObj("波（遠景）");
		sea->init("sea");

		TMapStaticObj* indirect = new TMapStaticObj("インダイレクト波");
		indirect->init("SeaIndirect");
		group->getChildren().push_back(indirect);

		TMapObjWaterFilter* filter
		    = new TMapObjWaterFilter("水中カメラフィルタ");
		filter->init();
		group->getChildren().push_back(filter);

		TMapObjSeaIndirect* sceneIndirect
		    = new TMapObjSeaIndirect("水中カメラインダイレクト");
		sceneIndirect->init();
		group->getChildren().push_back(sceneIndirect);
	}
	if (gpMarDirector->mMap == 2) {
		TMapObjSeaIndirect* sceneIndirect
		    = new TMapObjSeaIndirect("水中カメラインダイレクト");
		sceneIndirect->init();
		group->getChildren().push_back(sceneIndirect);
	}
}

static void initStage()
{
	if (gpMarDirector->getCurrentStage() > 9)
		return;

	initStageCommon();

	switch (gpMarDirector->getCurrentMap()) {
	case 1: { // Bianco
		if (gpMarDirector->getCurrentStage() == 5
		    || gpMarDirector->getCurrentStage() == 9)
			break;
		TMapObjBase::newAndInitBuildingCollisionWarp(1, nullptr)->setUp();
		TMapObjBase::newAndInitBuildingCollisionWarp(2, nullptr)->setUp();
		break;
	}
	case 2: // Ricco
		if (gpMarDirector->getCurrentStage() == 0)
			break;
		TMapObjBase::newAndInitBuildingCollisionWarp(1, nullptr)->setUp();
		TMapObjBase::newAndInitBuildingCollisionWarp(2, nullptr)->setUp();
		break;
	case 9: // Mare
		initMare();
		break;
	case 8: // Monte
		initMonte();
		break;
	case 6: // Pinna
		if (gpMarDirector->getCurrentStage() == 0)
			break;
		TMapObjBase::newAndInitBuildingCollisionWarp(1, nullptr)->setUp();
		break;
	case 5: // Sirena
		SMS_LoadParticle("/scene/mapObj/SandSteam.jpa", 0x6A);
		break;
	case 13: // Pinna Parco
		initPinnaParco();
		break;
	case 15: { // Option
		TMapObjOptionWall* wall = new TMapObjOptionWall("オプション用壁");
		wall->init();
		TMapObjBase::joinToGroup("マップグループ", wall);
		break;
	}
	}
}

void TMap::updateDelfino()
{
	int cube = gpCubeArea->unk1C;
	if (cube != mWarp->unk8) {
		if (cube != -1)
			mWarp->changeModel(cube);
		else if (gpMarDirector->getCurrentStage() != 0)
			mWarp->changeModel(3);
	}
}

void TMap::updateMonte()
{
	if (gpMarDirector->getCurrentStage() == 1
	    || gpMarDirector->getCurrentStage() == 3
	    || gpMarDirector->getCurrentStage() == 5
	    || gpMarDirector->getCurrentStage() == 7)
		gpMarioParticleManager->emit(0x156, &gpMapObjManager->unk44, 1, this);
}

static void updateRicco()
{
	static JGeometry::TVec3<f32> pos(1815.0f, 1500.0f, 1550.0f);
	SMSGetMSound()->startSoundActor(0x3000, &pos, 0, nullptr, 0, 4);
}

void TMap::update()
{
	switch (gpMarDirector->mMap) {
	case 3:
		updateRicco();
		break;

	case 8: // Monte
		updateMonte();
		break;

	case 7:
		updateDelfino();
		break;
	}

	if (gpMarDirector->unk124 != 0)
		return;

	if (gpCamera->isDemoCamera())
		return;

	if (gpMarDirector->getCurrentMap() == 0x39
	    || gpMarDirector->getCurrentMap() == 0x10)
		return;

	if (SMS_CheckMarioFlag(MARIO_FLAG_VISIBLE))
		return;

	const JGeometry::TVec3<f32>& camPos = gpCamera->getUnk124();
	f32 height = gpMapObjWave->getHeight(camPos.x, camPos.y, camPos.z);
	if (height == gpCamera->getUnk124().y || gpCamera->getUnk124().y > height) {
		if (!unk20) {
			unk20 = 1;
			MSSeCallBack::setWaterCameraFir(false);
		}
	} else if (unk20) {
		unk20 = 0;
		MSSeCallBack::setWaterCameraFir(true);
	}
}

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

void TMap::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		update();
		mCollisionData->initMoveCollision();
		mWarp->watchToWarp();
	}

	if (param_1 & 0x200) {
#ifdef SMS_NATIVE_PLATFORM
		if (const char* e = std::getenv("SB_MAPXLU_DBG"); e && e[0] && e[0] != '0') {
			static int n = 0; if (n < 12) { ++n;
				std::fprintf(stderr, "[mapxlu] TMap::perform flag=0x%x branch=%s xluCount=%d phase=%d\n",
				             param_1,
				             (param_1 & 0x2000000) ? "changeXluJoint(1)"
				             : (param_1 & 0x4000000) ? "changeXluJoint(0)" : "changeNormalJoint",
				             mXlu ? mXlu->unk0 : -1, sb_boot_capture_phase()); }
		}
#endif
		if ((param_1 & 0x2000000)) {
			if (!mXlu->changeXluJoint(1))
				return;
		} else if ((cue & CUE_SEMITRANSPARENT_PRIO_1)) {
			if (!mXlu->changeXluJoint(0))
				return;
		} else {
			mXlu->changeNormalJoint();
		}
	}

	if (cue & CUE_DRAW)
		draw(cue, graphics);

	mModelManager->perform(cue, graphics);
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
		fprintf(stderr, "[mapcol] gridExtent(%.0f,%.0f) numTri=%u added=%u numList=%u listUsed=%u\n",
		        mCollisionData->mGridExtentX, mCollisionData->mGridExtentY,
		        mCollisionData->unk1C, mCollisionData->unk34,
		        mCollisionData->unk20, mCollisionData->unk38);
		// Mario's option rail is x846-1800/z-1000; the disc map.col has a flat y=100
		// floor there. Probe it directly + walk the cell's ground list.
		for (int xi = 0; xi < 2; ++xi) {
			f32 rx = xi ? 1400.0f : 846.0f, rz = -1000.0f;
			const TBGCheckData* rr = nullptr;
			f32 ry = checkGround(rx, 500.0f, rz, &rr);
			int cgx = (int)((rx + mCollisionData->mGridExtentX) * (1.0f / 1024));
			int cgz = (int)((rz + mCollisionData->mGridExtentY) * (1.0f / 1024));
			int ln = 0;
			for (const TBGCheckList* p =
			         mCollisionData->getGridRoot14(cgx, cgz).getGroundList();
			     p && ln < 9999; p = p->getNext())
				++ln;
			fprintf(stderr, "[mapcol] RAIL checkGround(%.0f,%.0f)=%.1f cell(x%d z%d) groundListLen=%d\n",
			        rx, rz, ry, cgx, cgz, ln);
		}
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
				// Walk the centroid cell's ground list directly to see if tri is linked.
				int gridX = (int)((cx + mCollisionData->mGridExtentX) * (1.0f / 1024));
				int gridZ = (int)((cz + mCollisionData->mGridExtentY) * (1.0f / 1024));
				const TBGCheckList* h =
				    mCollisionData->getGridRoot14(gridX, gridZ).getGroundList();
				int nlist = 0;
				for (const TBGCheckList* p = h; p && nlist < 9999; p = p->getNext())
					++nlist;
				fprintf(stderr, "[mapcol] GROUND tri%d centroid(%.0f %.0f %.0f) cell(x%d z%d) "
				        "-> checkGround=%.1f found=%s | groundListLen=%d\n", i, cx, cy, cz,
				        gridX, gridZ, gy, (gy < 9000000.0f) ? "YES" : "NO", nlist);
				// Dump each linked triangle + which point-in-tri edge test fails for the centroid.
				for (const TBGCheckList* p = h; p; p = p->getNext()) {
					const TBGCheckData* d = p->unk8;
					f32 e1 = (d->mPoint1.z - cz) * (d->mPoint2.x - d->mPoint1.x)
					         - (d->mPoint1.x - cx) * (d->mPoint2.z - d->mPoint1.z);
					f32 e2 = (d->mPoint2.z - cz) * (d->mPoint3.x - d->mPoint2.x)
					         - (d->mPoint2.x - cx) * (d->mPoint3.z - d->mPoint2.z);
					f32 e3 = (d->mPoint3.z - cz) * (d->mPoint1.x - d->mPoint3.x)
					         - (d->mPoint3.x - cx) * (d->mPoint1.z - d->mPoint3.z);
					fprintf(stderr, "[mapcol]    listTri p1(%.0f %.0f)p2(%.0f %.0f)p3(%.0f %.0f) "
					        "minY=%.0f e1=%.0f e2=%.0f e3=%.0f inside=%d\n",
					        d->mPoint1.x, d->mPoint1.z, d->mPoint2.x, d->mPoint2.z,
					        d->mPoint3.x, d->mPoint3.z, d->mMinY, e1, e2, e3,
					        (e1 >= -1 && e2 >= -1 && e3 >= -1));
				}
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
