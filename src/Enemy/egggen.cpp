#include <Enemy/EggGen.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <Player/Mario.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <dolphin/mtx.h>

// TEggGenerator — Yoshi-egg generator. RE'd US GMSE01 (wide-RE sweep 2026-07-17).
// TEggGenerator : TSpineEnemy; TEggGenManager : TEnemyManager. No nerves.

// ---------------------------------------------------------------------------
// TEggGenManager
// ---------------------------------------------------------------------------
TEggGenManager::TEggGenManager(const char* name)
    : TEnemyManager(name)
{
}

TEggGenManager::~TEggGenManager() {}

void TEggGenManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TSpineEnemyParams("/enemy/egggen.prm");
	TEnemyManager::load(stream);
}

void TEggGenManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "gene_egg_model1.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

// ---------------------------------------------------------------------------
// TEggGenerator
// ---------------------------------------------------------------------------
TEggGenerator::TEggGenerator(const char* name)
    : TSpineEnemy(name)
{
	onLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_UNK8);
}

TEggGenerator::~TEggGenerator() {}

void TEggGenerator::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("gene_egg_model1.bmd", 0);

	initHitActor(0x02000001, 1, 0x80000000, 10.0f, 10.0f, 10.0f, 10.0f);
	mMActor->setBckFromIndex(0);

	// Yaw fixup: -90deg, normalized to [0,360).
	mRotation.x -= 90.0f;
	while (mRotation.x >= 360.0f)
		mRotation.x -= 360.0f;
	while (mRotation.x < 0.0f)
		mRotation.x += 360.0f;
}

void TEggGenerator::control()
{
	// Within 500 units of Mario, play the hatch anim while his Yoshi is still an egg.
	if (PSVECSquareDistance((Vec*)&mPosition, (Vec*)&gpMarioOriginal->mPosition) < 250000.0f) {
		if (gpMarioOriginal->mYoshi->mState == TYoshi::STATE_EGG)
			mMActor->setBckFromIndex(0);
	}
}
