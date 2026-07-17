#include <Animal/AnimalBase.hpp>
#include <Animal/AnimalManager.hpp>
#include <Animal/AnimalSave.hpp>
#include <Animal/AnimalNerve.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/PathNode.hpp>
#include <Strategic/LiveManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/Map.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Camera/cameralib.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <Strategic/Strategy.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <stdlib.h>

// Uniform [0, 1) from the game rand(). Written as the DECOMP SOURCE idiom
// rand()*(1/(RAND_MAX+1)) (which the GC compiler folds to *1/32768, RAND_MAX==0x7FFF),
// NOT the disasm-folded literal 1/32768. Correct today under the consistent libc regime;
// becomes bit-deterministic-vs-GC automatically once the decomp MSL rand.c is compiled
// with RAND_MAX==32767 (see debug_journal/2026-07-17_rand_lcg_libc_not_gc.md).
static inline f32 sAnmRand01() { return (f32)rand() * (1.0f / (f32)(RAND_MAX + 1)); }

// Native port of TAnimalBase::loadAfter (US GMSE01 @0x80008bec, size 0x48). RE'd from disasm
// (workflow 2026-07-17, verified vs the binary). TAnimalBase's TU is unnamed in the US map, so
// the address was located by structural fingerprint in the 0x80007c80..0x8000abc4 gap and
// confirmed instruction-by-instruction. After the empty base loadAfter, seagulls
// (actor-type 0x800001, the カモメ/kamome animal) register a positional random-play SE.
void TAnimalBase::loadAfter()
{
	JDrama::TNameRef::loadAfter();

	if (getActorType() == 0x800001) {
		MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_KAMOME_SOLO,
		                                          &mPosition);
	}
}

// TAnimalBase::receiveMessage (US GMSE01 @0x80008be0, JP size 0x8) — body is exactly
// `li r3,0; blr`: the animal handles no messages, unconditionally returns FALSE (does
// not chain to the base). Faithful on JP and US.
BOOL TAnimalBase::receiveMessage(THitActor* /* sender */, u32 /* msg */)
{
	return 0;
}

// TAnimalBase::calcRootMatrix (US GMSE01 @0x80008be8, JP size 0x4) — a single `blr`:
// an intentional empty override that suppresses the base root-matrix computation
// (animals position their model via the walk/perform path, not the actor root matrix).
void TAnimalBase::calcRootMatrix()
{
}

// TAnimalBase::load (US GMSE01 @0x80008c34, JP size 0xA0). RE'd from disasm (wide-RE
// workflow 2026-07-17, adversarially verified). One scene placement loads this template
// then spawns a FLOCK: read a big-endian count from the stream and create count-1 sibling
// clones sharing this actor's type + name, each scattered by initNoLoad_.
void TAnimalBase::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);

	s32 num;
	stream.read(&num, sizeof(num));
#ifdef SMS_NATIVE_PLATFORM
	// JSUInputStream::read raw-copies bytes with NO byteswap; the flock count is
	// big-endian on disc (same trap as TPlacement::load). Un-swapped, a real 3 reads as
	// 0x03000000 = ~50M -> a runaway spawn loop / OOM. Swap to host endian.
	num = __builtin_bswap32(num);
#endif

	// Spawn num-1 siblings. num==0 -> bound -1, guard holds, zero clones (no underflow).
	for (s32 i = 0; i < num - 1; ++i) {
		TAnimalBase* clone = new TAnimalBase(getActorType(), getName());
		initNoLoad_(clone);
	}
}

// TAnimalBase::initNoLoad_ (US GMSE01 @0x80008cd4, JP size 0x288). RE'd + verified.
// Scatters a bare-constructed clone around this template and finishes its init. rand() is
// the game LCG; scatter is X/Z +/-500, Y up (+1000) for seagulls / down (-250) for ground
// animals, heading template.y +/-75deg wrapped to [0,360), scale + character + graph shared.
void TAnimalBase::initNoLoad_(TAnimalBase* other)
{
	other->mPosition.x = mPosition.x + 1000.0f * (sAnmRand01() - 0.5f);
	other->mPosition.z = mPosition.z + 1000.0f * (sAnmRand01() - 0.5f);

	if (getActorType() == 0x800001) // 0x800001 = seagull (カモメ/kamome), flies -> scatter up
		other->mPosition.y = mPosition.y + 1000.0f * sAnmRand01();
	else                            // ground animal -> settle slightly down
		other->mPosition.y = mPosition.y - 250.0f * sAnmRand01();

	other->mScaling = mScaling;

	other->mRotation.x = 0.0f;
	f32 heading = mRotation.y + 150.0f * (sAnmRand01() - 0.5f);
	while (heading >= 360.0f)
		heading -= 360.0f;
	while (heading < 0.0f)
		heading += 360.0f;
	other->mRotation.y = heading;
	other->mRotation.z = 0.0f;

	other->unk3C = unk3C;                                 // shared TCharacter*
	other->unk124->setGraph(unk124->getGraph());         // share the graph web (plain ptr copy)

	other->mGroundPlane = TMap::getIllegalCheckData();
	other->init(mManager);

	// FAITHFUL QUIRK (US bytes, verified): registers `this` (the TEMPLATE), not `other`
	// (the clone), into the 敵グループ hit-check list — the insert operand is the stack
	// slot holding `this` (stored at entry, never overwritten). So the group accumulates
	// N-1 duplicate template pointers; clones are not added here. Do NOT "fix" to `other`.
	JDrama::TNameRefGen::search<TIdxGroupObj>("敵グループ")->add(this);
}

// TAnimalBase::init (US GMSE01 @0x80008f70, JP size 0x290). RE'd + verified. Spawn-time
// init for a walking/flying animal: register with the manager, build the MActor from all
// BMDs, set THitActor + TSpineEnemy tuning, seed the default wander nerve, pick the next
// graph node, allocate the {0,1} frame timer, set turn speed, and stagger the base/track-3
// animation start frames across the flock (deterministic by instance-index, or random).
void TAnimalBase::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(manager);
	mMActor       = mMActorKeeper->createMActorFromAllBmd(3);

	initHitActor(getActorType(), 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(1);

	mBodyScale  = 1.0f;
	mMarchSpeed = 0.0f;
	mBodyRadius = 10.0f;
	mWallRadius = 20.0f;
	mLiveFlag  |= 0x38;

	mSpine->initWith(&TNerveAnimalGraphWander::theNerve());
	unk124->reset();                        // mPrevIdx = -1
	goToShortestNextGraphNode();

	mFrameTimer    = new int[2];
	mFrameTimer[0] = 0;
	mFrameTimer[1] = 1;

	mMActor->setBckFromIndex(0);
	mMarchSpeed = 0.0f;

	TAnimalSaveIndividual* save = static_cast<TAnimalManagerBase*>(manager)->mAnimalSave;
	mTurnSpeed = save->mSLWalkTurnSpeed.get() * SMSGetAnmFrameRate();

	// Stagger the base animation (track 0) start frame across the flock.
	if (J3DFrameCtrl* fc = mMActor->getFrameCtrl(0)) {
		s32 shared = save->mSLSharedAnmNum.get();
		f32 frac   = (shared == 0) ? sAnmRand01()
		                           : (f32)(getInstanceIndex() % shared) / (f32)shared;
		fc->setFrame(frac * fc->getEnd());
	}
	// Randomize the secondary animation (track 3) start frame.
	if (J3DFrameCtrl* fc3 = mMActor->getFrameCtrl(3))
		fc3->setFrame(sAnmRand01() * fc3->getEnd());
}

// TAnimalBase::resetRandomCurPathNode (US GMSE01 @0x8000868c, JP size 0x21C). RE'd +
// verified. Picks a fresh random goal point around the current goal node and reseats both
// current + reserved path nodes to it, clearing the queued path stack. No-op when the goal
// is actor-anchored (unk0 != null, e.g. tracking Mario).
void TAnimalBase::resetRandomCurPathNode()
{
	const TPathNode& node = getUnkF4();
	if (node.unk0 != nullptr)
		return;

	JGeometry::TVec3<f32> point = node.getPoint();

	point.x += 1000.0f * (sAnmRand01() - 0.5f);
	point.z += 1000.0f * (sAnmRand01() - 0.5f);

	if (getActorType() == 0x800001) {       // seagull: flies
		if (point.y <= 1000.0f)
			point.y += 1000.0f * sAnmRand01();          // low -> bias up only
		else
			point.y += 1000.0f * (sAnmRand01() - 0.5f); // high -> symmetric
	} else {                                // ground animal: dip target slightly
		point.y -= 250.0f * sAnmRand01();
	}

	setGoalPath(TPathNode(point));
}

// TAnimalBase::getRotationFlyToDir (US GMSE01 @0x8000849c, JP size 0x1F0). RE'd + verified.
// Steers a flying animal's Euler rotation toward the rotation that aligns local +Z with
// `dir`: yaw turns toward target by at most `maxTurn`, roll banks proportionally (clamped
// +/-45), pitch chases the target; roll/pitch approach at 0.1*speed. Pure math on the two
// passed vectors — references no object fields.
void TAnimalBase::getRotationFlyToDir(JGeometry::TVec3<f32>* rotation,
                                      const JGeometry::TVec3<f32>& dir,
                                      f32 speed, f32 maxTurn)
{
	JGeometry::TVec3<f32> rot = MsGetRotFromZaxis(dir);

	f32 targetYaw = MsAngleWrap(rot.y);
	f32 dYaw      = MsClamp(MsAngleDiff(targetYaw, rotation->y), -maxTurn, maxTurn);
	rotation->y   = MsAngleWrap(rotation->y + dYaw);

	f32 bank = MsClamp(-30.0f * dYaw, -45.0f, 45.0f);
	CLBChaseGeneralConstantSpecifySpeed<f32>(&rotation->z, bank, 0.1f * speed);

	rot.x       = MsWrap(rot.x, -180.0f, 180.0f);
	rotation->x = MsWrap(rotation->x, -180.0f, 180.0f);
	CLBChaseGeneralConstantSpecifySpeed<f32>(&rotation->x, rot.x, 0.1f * speed);
}
