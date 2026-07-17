#include <Enemy/DebuTelesa.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/TakeActor.hpp>
#include <Enemy/Telesa.hpp>
#include <Strategic/ObjModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <System/Particles.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>

// TDebuTelesa (fat Boo). RE'd US GMSE01 (wide-RE sweep 2026-07-17). TDebuTelesa : TSmallEnemy.

// ---- params ----
TDebuTelesaSaveLoadParams::TDebuTelesaSaveLoadParams(const char* path)
    : TSmallEnemyParams(path)
{
	mSLAttackRadius.set(240);
	mSLAttackHeight.set(330);
	mSLDamageRadius.set(220);
	mSLDamageHeight.set(300);
	unk2CC = 0.0f;
	unk2D0 = 0.0f;
}

// ---- manager ----
TDebuTelesaManager::TDebuTelesaManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TDebuTelesaManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TDebuTelesaSaveLoadParams("/enemy/debuTelesa.prm");
	TSmallEnemyManager::load(stream);
}

void TDebuTelesaManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "debuTelesa.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TDebuTelesaManager::clipEnemies(JDrama::TGraphics* graphics)
{
	// DebuTelesa overrides only the clip fov; approximate with the base frustum clip
	// (exact fov tuning uses an uncertain camera global — deferred, cosmetic cull only).
	TSmallEnemyManager::clipEnemies(graphics);
}

// ---- actor ----
TDebuTelesa::TDebuTelesa(const char* name)
    : TSmallEnemy(name)
{
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TDebuTelesa::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	setMActorAndKeeper();

	mSpine->initWith(&TNerveDebuTelesaWait::theNerve());

	initHitActor(0x10000033, 1, 0x80000000, 0.0f, 0.0f, 0.0f, 0.0f);
	offHitFlag(1);
	initAnmSound();

	mYodareJointIdx = getModel()->getModelData()->getJointName()->getIndex("null_yodare");
	mRHandJointIdx  = getModel()->getModelData()->getJointName()->getIndex("jnt_Rhand");
}

void TDebuTelesa::calcRootMatrix()
{
	TSpineEnemy::calcRootMatrix();

	if (mHolder != nullptr) // held/bound -> no drool effects
		return;
	if (mSpine->getLatestNerve() == &TNerveSmallEnemyDie::theNerve())
		return;

	// Track the right-hand joint position (used by the drool effect).
	MtxPtr jm = getModel()->getAnmMtx(mRHandJointIdx);
	mRHandPos.set(jm[0][3], jm[1][3], jm[2][3]);
	// STOPGAP: the two looping drool particles (E_SMS_EFFECT_LOOP_INDIRECT 0x124 @ jnt_Rhand,
	// 0x187 @ null_yodare) are deferred — the SMS_EasyEmitParticle overload/enumerand mapping
	// needs verification. Cosmetic; the enemy behaves correctly without them.
}

BOOL TDebuTelesa::receiveMessage(THitActor* sender, u32 msg)
{
	switch (msg) {
	case 0:
	case 1:
	case 0xc:
		return FALSE;
	case 0xb:
		if (gpMSound->gateCheck(0x2938))
			MSoundSESystem::MSoundSE::startSoundNpcActor(0x2938, &mPosition, 0, nullptr, 0, 4);
		break;
	default:
		break;
	}
	return TSmallEnemy::receiveMessage(sender, msg);
}

void TDebuTelesa::kill() { TSmallEnemy::kill(); }

static const char* debutelesa_bastable[] = {
	"/scene/DebuTelesa/bas/debuTelesa_wait.bas",
};
const char** TDebuTelesa::getBasNameTable() const { return debutelesa_bastable; }

void TDebuTelesa::reset() {}
void TDebuTelesa::behaveToWater(THitActor*) {}
bool TDebuTelesa::changeByJuice() { return false; }
bool TDebuTelesa::isCollidMove(THitActor*) { return false; }
bool TDebuTelesa::doKeepDistance() { return true; }
void TDebuTelesa::attackToMario() { sendAttackMsgToMario(); }
void TDebuTelesa::setDeadAnm() { getMActor()->getFrameCtrl(0)->init(1); }

DEFINE_NERVE(TNerveDebuTelesaWait, TLiveActor)
{
	TDebuTelesa* self = static_cast<TDebuTelesa*>(spine->getBody());
	if (spine->getTime() == 0) {
		self->getMActor()->setBck("debutelesa_wait");
		self->setCurAnmSound();
	}
	return FALSE;
}
