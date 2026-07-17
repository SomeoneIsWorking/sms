#include <Animal/AnimalBase.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>

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
