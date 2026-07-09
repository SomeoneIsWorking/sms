#include <JSystem/JDrama/JDRNameRef.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstring>
#include <cstdlib>
#include <dolphin/os.h>
#endif

using namespace JDrama;

u16 TNameRef::calcKeyCode(const char* name)
{
	u32 result = 0;
	while ((s32)(*(u8*)name) != 0) {
		u32 tmp = result * 3;
		result  = *(u8*)name++ + tmp;
	}
	return result;
}

const char* TNameRef::getType(JSUMemoryInputStream& param_1,
                              JSUMemoryInputStream& param_2)
{
	u32 x = param_1.readU32();

	// TODO: wtf is happening here
	param_2.setBuffer((u8*)param_1.mBuffer + param_1.mPosition, x - 4);
	param_1.skip(x - 4);

	u32 len = param_2.readU16();
	return param_2.readString();
}

TNameRef* TNameRef::genObject(JSUMemoryInputStream& param_1,
                              JSUMemoryInputStream& param_2)
{
	const char* type = getType(param_1, param_2);
#ifdef SMS_NATIVE_PLATFORM
	if (type == nullptr) {
		OSPanic(__FILE__, __LINE__,
		        "TNameRef::genObject: getType returned NULL "
		        "(input stream at %p pos=%d) -- likely empty/short read from DVD",
		        (void*)param_1.mBuffer, (int)param_1.mPosition);
	}
	if (getenv("SB_NAMEREF_DBG"))
		OSReport("[SBDBG] TNameRef::genObject type=\"%s\"\n", type);
#endif
	TNameRef* obj    = TNameRefGen::getInstance()->getNameRef(type);
#ifdef SMS_NATIVE_PLATFORM
	// FAIL FAST: getNameRef returning null means the scene asset references a
	// type no getNameRef case creates. The list loaders silently skip the null
	// child (JDRNameRefPtrList.hpp:29), so the object never enters the tree and a
	// later setup search() returns null -> null deref three frames away (e.g.
	// setup2's TCardLoad "データロード"). Crash here naming the missing type.
	if (obj == nullptr && type != nullptr && type[0] != '\0') {
		// Known-unimplemented types: classes not yet decompiled (TODO cases in
		// the getNameRef tables). The object is skipped; tolerated rather than
		// panicked so boot proceeds, but logged so it's never invisible. Any
		// OTHER null type is a real bug (missing getNameRef case) -> fail fast.
		static const char* const kUnimplemented[] = {
			"MapObjFlagManager",
			// STOPGAP: the CHARACTER/CREATURE population (NPCs, animals, boats,
			// seals, gatekeepers) and their managers are not yet wired -- the
			// individual NPCs need their *Manager registered to set mManager
			// before TBaseNPC::load (see MarNameRefGen_NPC.cpp). These are
			// characters, not plaza GEOMETRY, so we exclude them to reach a
			// rendering plaza first. PROPER FIX:  the managers (their
			// getNameRef cases are TODO'd) and wire NPC<->manager. The "NPC"
			// prefix below covers every individual NPC name (all nulled in
			// getNameRef_NPC under SB_NPC_ON); these are the non-"NPC" creatures
			// and the manager type names referenced by the Delfino Plaza scene.
			"AnimalBird", "AnimalBirdManager",
			"BoardNpcManager",
			"FishoidA", "FishoidB", "FishoidManager",
			"FruitsBoat", "FruitsBoatManager",
			"GateKeeper", "GateKeeperManager",
			"OrangeSeal", "SealManager",
			"KinojiiManager", "KinopioManager", "PeachManager",
			"MonteMManager", "MonteMAManager", "MonteMBManager", "MonteMCManager",
			"MonteMDManager", "MonteMEManager", "MonteMFManager", "MonteMGManager",
			"MonteMHManager", "MonteWManager", "MonteWAManager", "MonteWBManager",
			"MonteWCManager", "MareMManager", "MareMAManager", "MareMBManager",
			"MareMCManager", "MareMDManager", "MareWManager", "MareWAManager",
			"MareWBManager", "RaccoonDogManager", "SunflowerLManager",
			"SunflowerSManager", "MareJellyFish",
			// Decorative plaza flag -- TMapObjFlag class is not decompiled.
			"MapObjFlag",
			// Delfino Airport (stage 0) -- same STOPGAP as the plaza set above:
			// unported NPC/creature manager found by SB_GENOBJ_SKIP_ALL survey on
			// the airport0.arc scene graph. Distinct variant of FruitsBoatManager
			// above.
			"FruitsBoatManagerB",
		};
		bool known = false;
		// All individual NPC actor names (NPC*) are deliberately nulled in
		// getNameRef_NPC while the NPC population is excluded -> tolerate by prefix.
		if (strncmp(type, "NPC", 3) == 0)
			known = true;
		for (unsigned i = 0; !known
		     && i < sizeof(kUnimplemented) / sizeof(*kUnimplemented);
		     ++i) {
			if (strcmp(type, kUnimplemented[i]) == 0) {
				known = true;
				break;
			}
		}
		// Survey mode (SB_GENOBJ_SKIP_ALL=1): skip+log EVERY unknown type instead
		// of panicking, so one boot enumerates the whole missing-type set for a new
		// scene (e.g. the plaza) rather than rediscovering them one panic per run.
		// Diagnostic only — leave the default fail-fast on for normal runs.
		static int skip_all = -1;
		if (skip_all < 0) {
			const char* e = getenv("SB_GENOBJ_SKIP_ALL");
			skip_all = (e && e[0] && e[0] != '0') ? 1 : 0;
		}
		if (known) {
			OSReport("[genObject] skipping unimplemented type \"%s\"\n", type);
		} else if (skip_all) {
			OSReport("[genObject-survey] MISSING type \"%s\"\n", type);
		} else {
			OSPanic(__FILE__, __LINE__,
			        "TNameRef::genObject: no getNameRef case for type \"%s\"",
			        type);
		}
	}
#endif
	return obj;
}

TNameRef::~TNameRef() { }

int TNameRef::getType() const { return 0; }

void TNameRef::load(JSUMemoryInputStream& stream)
{
	mKeyCode = stream.readU16();
	mName    = stream.readString();
}

void TNameRef::save(JSUMemoryOutputStream&) { }

void TNameRef::loadAfter() { }

TNameRef* TNameRef::searchF(u16 key, const char* name)
{
	bool match = false;
	if (mKeyCode == key && strcmp(mName, name) == 0)
		match = true;

	if (match)
		return this;
	else
		return nullptr;
}
