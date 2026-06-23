#include "NPC/NpcBase.hpp"
#include "Strategic/LiveManager.hpp"
#include <System/MarNameRefGen.hpp>

JDrama::TNameRef* TMarNameRefGen::getNameRef_NPC(const char* name) const
{
#ifdef SMS_NATIVE_PLATFORM
	// STOPGAP: register the NPC *Managers* (MonteMManager/.../KinopioManager) so
	// these NPCs get a non-null mManager, because the NPC population needs its
	// managers to set mManager before TBaseNPC::load -> setIndividualDifference_
	// dereferences it (mManager->unk28). Until the managers are wired (their
	// getNameRef cases are TODO'd below), creating the individual TBaseNPCs here
	// yields orphan NPCs with mManager==null -> SEGV in setIndividualDifference_.
	// The NPCs are CHARACTERS, not plaza geometry, so we exclude the whole NPC
	// population to reach a rendering plaza first; genObject tolerates the
	// resulting null (skips the object). Set SB_NPC_ON=1 to re-enable creation
	// once the managers are registered.
	{
		static int npc_on = -1;
		if (npc_on < 0) {
			const char* e = getenv("SB_NPC_ON");
			npc_on = (e && e[0] && e[0] != '0') ? 1 : 0;
		}
		if (!npc_on)
			return nullptr;
	}
#endif
	// NOTE: TBaseNPC's first arg here screams enum. probably worth seeing what
	// they could mean beyond the associated strings here
	if (strcmp(name, "NPCMonteM") == 0)
		return new TBaseNPC(0x04000001U);

	if (strcmp(name, "NPCMonteMA") == 0)
		return new TBaseNPC(0x04000002U);

	if (strcmp(name, "NPCMonteMB") == 0)
		return new TBaseNPC(0x04000003U);

	if (strcmp(name, "NPCMonteMC") == 0)
		return new TBaseNPC(0x04000004U);

	if (strcmp(name, "NPCMonteMD") == 0)
		return new TBaseNPC(0x04000005U);

	if (strcmp(name, "NPCMonteME") == 0)
		return new TBaseNPC(0x04000006U);

	if (strcmp(name, "NPCMonteMF") == 0)
		return new TBaseNPC(0x04000007U);

	if (strcmp(name, "NPCMonteMG") == 0)
		return new TBaseNPC(0x04000008U);

	if (strcmp(name, "NPCMonteMH") == 0)
		return new TBaseNPC(0x04000009U);

	if (strcmp(name, "NPCMonteW") == 0)
		return new TBaseNPC(0x0400000AU);

	if (strcmp(name, "NPCMonteWA") == 0)
		return new TBaseNPC(0x0400000BU);

	if (strcmp(name, "NPCMonteWB") == 0)
		return new TBaseNPC(0x0400000CU);

	if (strcmp(name, "NPCMonteWC") == 0)
		return new TBaseNPC(0x0400000DU);

	if (strcmp(name, "NPCMareM") == 0)
		return new TBaseNPC(0x0400000EU);

	if (strcmp(name, "NPCMareMA") == 0)
		return new TBaseNPC(0x0400000FU);

	if (strcmp(name, "NPCMareMB") == 0)
		return new TBaseNPC(0x04000010U);

	if (strcmp(name, "NPCMareMC") == 0)
		return new TBaseNPC(0x04000011U);

	if (strcmp(name, "NPCMareMD") == 0)
		return new TBaseNPC(0x04000012U);

	if (strcmp(name, "NPCMareW") == 0)
		return new TBaseNPC(0x04000013U);

	if (strcmp(name, "NPCMareWA") == 0)
		return new TBaseNPC(0x04000014U);

	if (strcmp(name, "NPCMareWB") == 0)
		return new TBaseNPC(0x04000015U);

	if (strcmp(name, "NPCKinopio") == 0)
		return new TBaseNPC(0x04000016U);

	if (strcmp(name, "NPCKinojii") == 0)
		return new TBaseNPC(0x04000017U);

	if (strcmp(name, "NPCPeach") == 0)
		return new TBaseNPC(0x04000018U);

	if (strcmp(name, "NPCRaccoonDog") == 0)
		return new TBaseNPC(0x04000019U);

	if (strcmp(name, "NPCSunflowerL") == 0)
		return new TBaseNPC(0x0400001AU);

	if (strcmp(name, "NPCSunflowerS") == 0)
		return new TBaseNPC(0x0400001BU);

	if (strcmp(name, "NPCDummy") == 0)
		return new TBaseNPC(0x0400001CU);

	if (strcmp(name, "NPCBoard") == 0)
		return new TBaseNPC(0x0400001DU);

	// TODO:
	// if ( strcmp(name, "MonteMManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMAManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMBManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMCManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMDManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMEManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMFManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMGManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteMHManager") == 0 )
	//     return new TMonteMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteWManager") == 0 )
	//     return new TMonteWBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteWAManager") == 0 )
	//     return new TMonteWBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteWBManager") == 0 )
	//     return new TMonteWBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MonteWCManager") == 0 )
	//     return new TMonteWBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MareMManager") == 0 )
	//     return new TMareMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MareMAManager") == 0 )
	//     return new TMareMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MareMBManager") == 0 )
	//     return new TMareMBaseManager( "?");

	// TODO:
	// if ( strcmp(name, "MareMCManager") == 0 )
	//     return new TMareMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MareMDManager") == 0 )
	//     return new TMareMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MareWManager") == 0 )
	//     return new TMareMBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MareWAManager") == 0 )
	//     return new TMareWBaseManager("?");

	// TODO:
	// if ( strcmp(name, "MareWBManager") == 0 )
	//     return new TMareWBaseManager("?");

	// TODO:
	// if ( strcmp(name, "KinopioManager") == 0 )
	//     return new TNPCManager("?");

	// TODO:
	// if ( strcmp(name, "KinojiiManager") == 0 )
	//     return new TNPCManager("?");

	// TODO:
	// if ( strcmp(name, "PeachManager") == 0 )
	//     return new TNPCManager("?");

	// TODO:
	// if ( strcmp(name, "RaccoonDogManager") == 0 )
	//     return new TNPCManager("?");

	// TODO:
	// if ( strcmp(name, "SunflowerLManager") == 0 )
	//     return new TNPCManager("?");

	// TODO:
	// if ( strcmp(name, "SunflowerSManager") == 0 )
	//     return new TNPCManager  ("?");

	// TODO:
	// if ( strcmp(name, "MareJellyFish") == 0 )
	//     return new TMareJellyFishManager("?");

	// TODO:
	// if (strcmp(name, "BoardNpcManager") == 0)
	// 	return new TBoardNpcManager("?");

	return nullptr;
}
