#include <Map/Map.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif
#include <Camera/Camera.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <Map/MapModel.hpp>
#include <Map/MapWarp.hpp>
#include <Map/MapXlu.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <MoveBG/MapObjWave.hpp>
#include <MSound/MSound.hpp>
#include <System/MarDirector.hpp>
#ifdef SMS_NATIVE_PLATFORM
// WEAK: only defined inside sms-boot (native/render/sms_boot_j3d_capture.cpp) — a test target
// linking this file without the render-capture pipeline (e.g. sms-j3dload_test) must still link.
// The sole call site below is already gated behind a getenv() debug flag.
extern "C" int sb_boot_capture_phase() __attribute__((weak));
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

// Native port of TMap::update (@0x80189bd0, 612B). Called from TMap::perform
// when flag&1 is set. Dispatch is on gpMarDirector->mMap:
//
//   mMap == 7:   stage-warp target-change dispatch. RE'd but deferred — the
//                driving singleton at SDA1[-0x70dc] is not yet named (its
//                field +0x1c holds the "expected warp target id" that
//                mWarp->unk8 is compared against). Also used by
//                TLiveManager::setFlagOutOfCube, TelesaManager, and
//                TMario::perform, so shared stage-info scope.
//
//   mMap == 3:   one-shot copy of a rodata Vec3 (1815, 1500, 1550) — read
//                from SDA2 at -0x446c/-0x4468/-0x4464 — into a .bss cache
//                (guarded by a file-static u8 flag at SDA1[-0x6324]),
//                then dispatch MSoundSE::startSoundActor(0x3000, &cache,
//                0, nullptr, 0, 4) once per frame if MSound::gateCheck
//                allows. Deferred — needs SDA1[-0x6324] pinned to a
//                real symbol.
//
//   mMap == 8 && unk7D∈{1,3,5,7}: gpMarioParticleManager->emit(0x156,
//                gpMapObjManager->unk44, 1, this). Deferred — depends on
//                port of TMarioParticleManager::emit (@0x8028856c) which
//                is unresolved at ABI level.
//
// Water-camera-immersion detection (the only branch that fires at title
// map==15, gated by unk124==0 not-in-demo-mode + the SDA1[-0x6094] state
// singleton's first-u32 bit-1 clear + map ≠ 57 and ≠ 16): pin the camera
// pos against the water surface height, and on air↔water transitions
// notify MSSeCallBack to raise/lower the water filter timer. This is
// ported faithfully below.
void TMap::update()
{
	if (gpMarDirector->unk124 != 0)
		return; // talk/demo mode — no immersion tracking

	CPolarSubCamera* cam = gpCamera;

	bool skipImmersion = true;
	if (!cam->isSimpleDemoCamera() && cam->mMode != CAMERA_MODE_COUNT)
		skipImmersion = false;
	if (skipImmersion)
		return;

	u8 mapId = gpMarDirector->mMap;
	if (mapId == 57 || mapId == 16)
		return;

	// TODO: SDA1[-0x6094] holds a singleton whose first u32 field, when bit-1
	// is set, gates OUT the water-immersion tracking (likely a cinematic /
	// pollution-cleared / cutscene marker). Not yet pinned to a named
	// symbol. At title (mMap==15), the .sbss is zero-initialized so this
	// check treats bit-1 as clear (proceed with immersion). Once the
	// singleton is named, replace this guard with a real read.
	// if (*((u32*)gpUnkSDA1_6094) & 2) return;

	f32 waterY = gpMapObjWave->getHeight(cam->unk124.x, cam->unk124.y,
	                                     cam->unk124.z);
	f32 camY = cam->unk124.y;

	if (waterY <= camY) {
		// camera above/at water surface
		if (unk20 == 0) {
			unk20 = 1;
			MSSeCallBack::setWaterCameraFir(false);
		}
	} else if (unk20 != 0) {
		// camera below water surface, was previously above
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
				             mXlu ? mXlu->getPrioGroupNum() : -1,
				             (&sb_boot_capture_phase) ? sb_boot_capture_phase() : -1); }
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
