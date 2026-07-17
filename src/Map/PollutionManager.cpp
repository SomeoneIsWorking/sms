#include <Map/PollutionManager.hpp>
#include <Map/PollutionLayer.hpp>
#include <Map/MapEventSink.hpp>
#include <System/MarDirector.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <dolphin/os.h>
#endif

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

int TPollutionManager::mFlushTime = 200;
u8 TPollutionManager::mEdgeAlpha  = 50;

TPollutionManager* gpPollution;

void TPollutionManager::stampModel(J3DModel* model)
{
	for (int i = 0; i < getJointModelNum(); ++i)
		if (getLayer(i)->getPlaneType() == 0)
			getLayer(i)->stampModel(model);
}

void TPollutionManager::stamp(u16 stamp_type, f32 x, f32 y, f32 z, f32 size)
{
	for (int i = 0; i < getJointModelNum(); ++i) {
		TPollutionLayer* layer = getLayer(i);
		layer->stamp(stamp_type, x, y, z, size);
	}
}

void TPollutionManager::clean(f32 x, f32 y, f32 z, f32 size)
{
	if (gpMarDirector->getCurrentMap() == 1 && y < -10.0f)
		return;

	stamp(0, x, y, z, size);
}

void TPollutionManager::stampGround(u16 stamp_type, f32 x, f32 y, f32 z,
                                    f32 size)
{
	for (int i = 0; i < getJointModelNum(); ++i)
		if (getLayer(i)->getPlaneType() == 0)
			getLayer(i)->stamp(stamp_type, x, y, z, size);
}

int TPollutionManager::getPollutionType(f32 x, f32 y, f32 z) const
{
	for (int i = 0; i < getJointModelNum(); ++i)
		if (getLayer(i)->isInArea(x, y, z))
			return getLayer(i)->getPollutionType();

	return POLLUTION_TYPE_UNK10;
}

u32 TPollutionManager::getPollutionDegree() const
{
	u32 totalDegree = 0;
	for (int i = 0; i < getJointModelNum(); ++i) {
		TPollutionLayer* layer = getLayer(i);
		totalDegree += layer->getPollutionDegree();
	}
	return totalDegree;
}

void TPollutionManager::isProhibit(f32 x, f32 y, f32 z) const { }

bool TPollutionManager::isPolluted(f32 x, f32 y, f32 z) const
{
	for (int i = 0; i < getJointModelNum(); ++i) {
		TPollutionLayer* layer = getLayer(i);
		if (layer->isPolluted(x, y, z))
			return true;
	}

	return false;
}

void TPollutionManager::subtractFromYMap(f32, f32, f32) const { }

static void dummy()
{
	(Vec) { 0.0f, 0.0f, 0.0f };
	(Vec) { 1.0f, 1.0f, 1.0f };
}

bool TPollutionManager::cleanedAll() const
{
	return getPollutionDegree() < TMapEventSink::mCleanedDegree ? true : false;
}

void TPollutionManager::draw() { }

void TPollutionManager::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	if (param_1 & 0x1000000) {
		getCounterObj().countObjDegree();
	} else if (param_1 & 0x2000000) {
		u8 uVar1 = param_1 >> 0x10;
		if (uVar1 == 0)
			getCounterLayer().calcViewMtx();

		getCounterLayer().countTexDegree(uVar1);

		int last = getJointModelNum() - 1;
		if (uVar1 == last)
			getCounterLayer().resetTask();
	} else {
		TJointModelManager::perform(param_1, param_2);
	}
}

TJointModel* TPollutionManager::newJointModel(int param_1) const
{
	switch (getLayerInfo(param_1)->mPlaneType) {
	case 0:
		return new TPollutionLayer;
		break;
	case 1:
		return new TPollutionLayer;
		break;
	case 4:
		return new TPollutionLayerWallPlusZ;
		break;
	case 5:
		return new TPollutionLayerWallMinusZ;
		break;
	case 2:
		return new TPollutionLayerWallPlusX;
		break;
	case 3:
		return new TPollutionLayerWallMinusX;
		break;
	case 6:
		return new TPollutionLayerWave;
		break;
	default:
		return nullptr;
	}
}

void TPollutionManager::setDataAddress(TPollutionManager::TPollutionInfo* info)
{
	(void)0;
	// pointer patching ewwww
	info->mLayerInfos
	    = (TPollutionLayerInfo*)((u8*)info->mLayerInfos + (uintptr_t)info);
	mLayerInfos = info->mLayerInfos;
	for (int i = 0; i < mJointModelNum; ++i)
		mLayerInfos[i].mHeightMap += (uintptr_t)info;
}

void TPollutionManager::initPollutionInfo()
{
	if (TPollutionInfo* info
	    = (TPollutionInfo*)JKRGetResource("/scene/map/ymap.ymp")) {
#ifdef SMS_NATIVE_PLATFORM
		// STOPGAP: skip the pollution (落書き) data init because the
		// ymap.ymp file overlay is not yet ported for the host. ymap.ymp is
		// a packed BE blob overlaid directly as TPollutionInfo /
		// TPollutionLayerInfo, with 32-bit offsets typed as native
		// pointers (the native-bmd-load-lp64-overlay class). Proper fix =
		// sb_ymp_swap_to_host + narrow mLayerInfos/mHeightMap to u32
		// offsets. Pollution is the PARKED subsystem; leaving
		// mJointModelNum = 0 is a clean no-op, TPollutionManager::load
		// guards everything on it. Guard BEFORE setDataAddress so the raw
		// LP64-broken pointer patching doesn't run.
		(void)info;
		OSReport("[pollution] STOPGAP: ymap.ymp overlay not ported for host "
		         "-> skipping pollution setup (no goo). mJointModelNum=0.\n");
		mJointModelNum = 0;
		return;
#endif
		mJointModelNum = info->mLayerCount;
		setDataAddress(info);


		if (gpMarDirector->getCurrentMap() == 0x9
		    && gpMarDirector->getCurrentStage() != 0x7) {
			static const char* mare_name_table[] = {
				"pollution00", "pollution01", "pollution02", "pollution03",
				"pollution04", "pollution05", "pollution06", "pollutionA",
				"pollutionB",  nullptr,
			};
			initJointModel("scene/map/pollution", mare_name_table);
		} else {
			static const char* name_table[] = {
				"pollution00", "pollution01", "pollution02", "pollution03",
				"pollution04", "pollution05", "pollution06", "pollution07",
				"pollution08", "pollution09", "pollution10", "pollution11",
				"pollution12", "pollution13", "pollution14", "pollution15",
				"pollution16", "pollution17", "pollution18", "pollution19",
				nullptr,
			};
			initJointModel("scene/map/pollution", name_table);
		}
	}
}

void TPollutionManager::load(JSUMemoryInputStream& stream)
{
	TJointModelManager::load(stream);

	initPollutionInfo();

	if (mJointModelNum != 0) {
		mDefaultPolluteStampTex
		    = (ResTIMG*)JKRGetResource("/common/map/pollute.bti");
		mDefaultCleanStampTex
		    = (ResTIMG*)JKRGetResource("/common/map/clean.bti");

		getCounterLayer().init(getJointModelNum(), 15, 5);

		for (int i = 0; i < getJointModelNum(); ++i)
			getCounterLayer().registerLayer(getLayer(i),
			                                &getLayer(i)->mCounter);

		gpPollution->getCounterObj().init(30);

		getCounterLayer().registerTexStamp(0, 0xff, mDefaultCleanStampTex);
		getCounterLayer().registerTexStamp(1, 0xff, mDefaultPolluteStampTex);
	}
}

TPollutionManager::TPollutionManager(const char* name)
    : TJointModelManager(name)
    , mLayerInfos(0)
    , mDefaultPolluteStampTex(0)
    , mDefaultCleanStampTex(0)
    , unk20C(0)
{
	gpPollution = this;
}
