#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif
#include <JSystem/JAudio/JAInterface/JAISystemInterface.hpp>
#include <JSystem/JAudio/JAInterface/JAISound.hpp>
#include <JSystem/JAudio/JAInterface/JAIGlobalParameter.hpp>
#include <JSystem/JAudio/JAInterface/JAIParameters.hpp>
#include <JSystem/JAudio/JASystem/JASDvdThread.hpp>
#include <JSystem/JAudio/JASystem/JASTrackMgr.hpp>
#include <JSystem/JAudio/JASystem/JASCmdStack.hpp>
#include <types.h>

JASystem::Kernel::TPortCmd JAISystemInterface::systemPortCmd;

BOOL JAISystemInterface::checkFileExsistence(char* path)
{
	char buf[64];
	JASystem::Dvd::extendPath(buf, path);
	if (DVDConvertPathToEntrynum(buf) != -1)
		return true;
	else
		return false;
}

BOOL JAISystemInterface::checkSeqActiveFlag(u32 param_1)
{
	JASystem::TTrack* track = JASystem::TrackMgr::handleToSeq(param_1);

	if (track != 0 && track->mSeqState) {
		if (track->getChild(0) || track->getChild(1) || track->getChild(2)
		    || track->getChild(3) || track->getChild(4) || track->getChild(5)
		    || track->getChild(6) || track->getChild(7) || track->getChild(8)
		    || track->getChild(9) || track->getChild(10) || track->getChild(11)
		    || track->getChild(12) || track->getChild(13) || track->getChild(14)
		    || track->getChild(15)) {
			return track->mSeqState;
		} else {
			return false;
		}
	}
	return false;
}

JASystem::TTrack* JAISystemInterface::trackToSeqp(JAISound* param_1, u8 param_2)
{
	JASystem::TTrack* result = nullptr;
	if (param_1->mSoundID & 0x800) {
		JASystem::TTrack* track = JASystem::TrackMgr::handleToSeq(
		    param_1->getSeqParameter()->mSeqHandle);
		if (track->getChild(param_2 >> 4))
			result = track->getChild(param_2 >> 4)->getChild(param_2 & 0xF);
	} else {
		JASystem::TTrack* track = JASystem::TrackMgr::handleToSeq(
		    param_1->getSeqParameter()->mSeqHandle);
		result = track->getChild(param_2 & 0xF);
	}
	return result;
}

#ifdef SMS_NATIVE_PLATFORM
// LP64 landmine: the decomp addresses TPortArgs as a flat 4-byte-word array from &mTrack
// (`((f32*)&s->unk4)[param_3]`), which is only correct when mTrack is a 4-byte GC pointer.
// On the 64-bit host mTrack is 8 bytes, so every index >= 1 is off by one f32 slot and the
// per-parameter pushes scatter into the wrong fields (pitch->volume, pan->pitch, ...),
// leaving mTrackPitch permanently 0 -> DSP pitch 0 -> silence (2026-07-17). param_3 is the
// GC word index; map it to the named field it denotes (GC byte offset = param_3 * 4) so the
// write is layout-correct on both ABIs. Index 0 = mTrack (never a value target).
static void* sb_portarg_slot(JASystem::Kernel::TPortArgs* a, u8 idx)
{
	switch (idx) {
	case 1: return &a->mFlags;
	case 2: return &a->mTrackVolume;
	case 3: return &a->mTrackPitch;
	case 4: return &a->mTrackPan;
	case 5: return &a->mTrackFxmix;
	case 6: return &a->mTrackDolby;
	case 7: return &a->unk1C;
	case 8: return &a->unk20;
	case 9: return &a->mTrackTempo;
	default: return nullptr;
	}
}
#endif

void JAISystemInterface::setSeqPortargsF32(JAISeqUpdateData* param_1,
                                           u32 param_2, u8 param_3, f32 param_4)
{
	JAISeqUpdateData::FabricatedUnk4CStruct* s = &param_1->unk4C[param_2];

#ifdef SMS_NATIVE_PLATFORM
	void* slot = sb_portarg_slot(&s->unk4, param_3);
	if (slot)
		*(f32*)slot = param_4;
#else
	((f32*)&s->unk4)[param_3] = param_4;
#endif
}

void JAISystemInterface::setSeqPortargsPS16(JAISeqUpdateData* sud, u32 track_no,
                                            u8 arg_no, s16* value)
{
	sud->mPlayerParams[track_no].mArgsAsPS16[arg_no] = value;
}

void JAISystemInterface::setSeqPortargsU32(JAISeqUpdateData* sud, u32 track_no,
                                           u8 arg_no, u32 value)
{
	JAISeqUpdateData::FabricatedUnk4CStruct* s = &param_1->unk4C[param_2];

#ifdef SMS_NATIVE_PLATFORM
	void* slot = sb_portarg_slot(&s->unk4, param_3);
	if (slot)
		*(u32*)slot = param_4;
#else
	((u32*)&s->unk4)[param_3] = param_4;
#endif
}

JAISeqParameter* JAISystemInterface::rootInit(JAISeqUpdateData* param_1)
{
	JAISound* sound = param_1->mSound;
	JASystem::TTrack* track
	    = JASystem::TrackMgr::handleToSeq(sound->getSeqParameter()->mSeqHandle);
	outerInit(param_1, track, JAIGlobalParameter::getParamSeqTrackMax(), 0xffff,
	          0);
	return sound->getSeqParameter();
}

void JAISystemInterface::trackInit(JAISeqUpdateData* sud)
{
	JAISound* sound = sud->mSound;
	u32 trackCnt    = 0x10;
	if (sound->mSoundID & 0x800)
		trackCnt = JAIGlobalParameter::getParamSeqTrackMax();

	for (u32 i = 0; i < trackCnt; ++i)
		if (!(sud->mTrackInitFlags & (1 << i))) {
			JASystem::TTrack* track = trackToSeqp(sound, i);
			outerInit(sud, track, i, 0xffff, 0);
		}
}

void JAISystemInterface::outerInit(JAISeqUpdateData* sud, void* track,
                                   u32 track_no, u16 param_4, u8 param_5)
{
	if (!track)
		return;
#ifdef SMS_NATIVE_PLATFORM
	// trackToSeqp()/handleToSeq() can hand back a stale/wild track pointer (a freed handle or a
	// reused pool slot) for a sequence slot that no longer exists; queuing an SE port command
	// for it makes portCmdMain later deref freed memory -> SIGSEGV (2026-07-17). Reject any
	// track that is not a live slot of the static track pool before it is used.
	if (!JASystem::TrackMgr::isPoolTrack(static_cast<const JASystem::TTrack*>(param_2)))
		return;
#endif

	JASystem::Kernel::TPortArgs* args = &sud->mPlayerParams[track_no].mArgs;
	JASystem::TTrack* trackCasted     = (JASystem::TTrack*)track;

	sud->mPlayerParams[track_no].mTrack = trackCasted;
	args->mTrack                        = (JASystem::TTrack*)track;
	sud->mPlayerParams[track_no].mCmd.setPortCmd(&setSePortParameter, args);

	JASystem::TTrack::TOuterParam* outer = trackCasted->getOuterParam();

	if (track_no == JAIGlobalParameter::getParamSeqTrackMax()) {
		args->mTrackVolume = sud->mSeqVolume;
		args->mTrackPitch  = sud->mSeqPitch;
		args->mTrackFxmix  = sud->mSeqFxmix;
		args->mTrackPan    = sud->mSeqPan;
		args->mTrackDolby  = sud->mSeqDolby;
		args->mTrackTempo  = sud->mSeqTempo;
		args->mFlags       = 0xff;
		outer->onSwitch(JASystem::TTrack::UPDATE_Tempo);
	} else {
		JAISeqParameter* pJVar3 = sud->mSound->getSeqParameter();
		args->mTrackVolume      = pJVar3->mTrackVolume[track_no].mCurrentValue;
		args->mTrackPitch       = pJVar3->mTrackPitch[track_no].mCurrentValue;
		args->mTrackFxmix       = pJVar3->mTrackFxmix[track_no].mCurrentValue;
		args->mTrackPan         = pJVar3->mTrackPan[track_no].mCurrentValue;
		args->mTrackDolby       = pJVar3->mTrackDolby[track_no].mCurrentValue;
		args->unk20             = 0;
		args->mFlags            = 0x7f;
		trackCasted->muteTrack(pJVar3->mMuteBits[track_no].mCurrent);
	}
	outer->onSwitch(JASystem::TTrack::UPDATE_Volume);
	outer->onSwitch(JASystem::TTrack::UPDATE_Pitch);
	outer->onSwitch(JASystem::TTrack::UPDATE_Fxmix);
	outer->onSwitch(JASystem::TTrack::UPDATE_Pan);
	outer->onSwitch(JASystem::TTrack::UPDATE_Dolby);

	if ((param_4 & 1) == 0)
		outer->setParam(JASystem::TTrack::UPDATE_Volume, 0.0);

	if ((param_4 & 2) == 0)
		outer->setParam(JASystem::TTrack::UPDATE_Pitch, 0.0);

	if ((param_4 & 4) == 0)
		outer->setParam(JASystem::TTrack::UPDATE_Fxmix, 0.0);

	if ((param_4 & 8) == 0)
		outer->setParam(JASystem::TTrack::UPDATE_Pan, 0.0);

	if ((param_4 & 0x10) == 0)
		outer->setParam(JASystem::TTrack::UPDATE_Dolby, 0.0);

#ifdef SMS_NATIVE_PLATFORM
	if (std::getenv("SB_DBG_AUDIO")) {
		static int n = 0;
		if (n < 6) { ++n;
			std::fprintf(stderr, "[audio] outerInit queue: param_1=%p unk4C=%p slot=%u args=%p track=%p\n",
			             (void*)param_1, (void*)param_1->unk4C, param_3, (void*)args, (void*)track);
		}
	}
#endif
	param_1->unk4C[param_3].unk2C.addPortCmdOnce();
}

void JAISystemInterface::setPortParameter(JASystem::Kernel::TPortArgs* args,
                                          JASystem::TTrack* track, u32 param_3,
                                          u32 param_4)
{
	if ((args->mFlags & (1 << param_4)) != 0) {
		JASystem::TTrack::TOuterParam* outer = track->getOuterParam();
		outer->setParam(param_3, (&args->mTrackVolume)[param_4]);
		args->mFlags ^= 1 << param_4;
	}
}

void JAISystemInterface::setSePortParameter(JASystem::Kernel::TPortArgs* args)
{
	JASystem::TTrack* track = args->mTrack;
	if (!track)
		return;
#ifdef SMS_NATIVE_PLATFORM
	// USE-AFTER-FREE guard (2026-07-17): args->mTrack is a REUSED field — a port command can be
	// drained by portCmdMain (aiCallback, same updateDSP) after the track it named has closed
	// or its pool slot been reused, so mTrack (and its mOuterParam) may be stale/wild. Validate
	// against the static track pool FIRST (before any deref), then the play-state / outer param.
	// A wild `track` here otherwise reads unmapped memory (const fault 0x1746f5168).
	if (!JASystem::TrackMgr::isPoolTrack(track) || track->unk3C4 == 0
	    || track->getOuterParam() == nullptr)
		return;
#endif

	setPortParameter(args, track, JASystem::TTrack::UPDATE_Volume, 0);
	setPortParameter(args, track, JASystem::TTrack::UPDATE_Pitch, 1);
	setPortParameter(args, track, JASystem::TTrack::UPDATE_Pan, 2);
	setPortParameter(args, track, JASystem::TTrack::UPDATE_Fxmix, 3);
	setPortParameter(args, track, JASystem::TTrack::UPDATE_Tempo, 7);
	setPortParameter(args, track, JASystem::TTrack::UPDATE_Dolby, 4);

	if ((args->mFlags & 0x40) != 0 && args->unk20 != 0)
		track->setInterrupt(5);
}

void* JAISystemInterface::JAIouterP(void*) { return nullptr; }

void* JAISystemInterface::JAIouterSW(void*) { return nullptr; }

void JAISystemInterface::setAudioFrameParameter(JASystem::Kernel::TPortArgs*) {
}

int JAISystemInterface::setSeqData(JASystem::TTrack* param_1, u8* param_2,
                                   s32 param_3,
                                   JASystem::Player::SEQ_PLAYMODE param_4)
{
	if (param_1 == nullptr) {
		BOOL enable = OSDisableInterrupts();
		param_1     = JASystem::TrackMgr::getNewTrack();
		OSRestoreInterrupts(enable);
		if (param_1 == nullptr)
			return -1;
	} else {
		param_1->setInnerMemory(0);
	}

	return param_1->setSeqData(param_2, param_3, param_4);
}

BOOL JAISystemInterface::startSeq(u32 param_1)
{
	JASystem::TTrack* track = JASystem::TrackMgr::handleToSeq(param_1);
	if (param_1 == -1)
		return 0;
	else if (track == nullptr)
		return 0;
	else
		return track->startSeq();
}

BOOL JAISystemInterface::stopSeq(s32 param_1)
{
	if (param_1 == -1)
		return 0;

	JASystem::TTrack* track = JASystem::TrackMgr::handleToSeq(param_1);

	if (track == nullptr)
		return 0;
	else
		return track->stopSeq();
}

BOOL JAISystemInterface::writePortApp(u32 param_1, u32 param_2, u16 param_3)
{
	JASystem::TTrack* track = JASystem::TrackMgr::handleToSeq(param_1);

	if (track == nullptr)
		return 0;
	else
		return track->writePortApp(param_2, param_3);
}

BOOL JAISystemInterface::readPortApp(u32 param_1, u32 param_2, u16* param_3)
{
	JASystem::TTrack* track = JASystem::TrackMgr::handleToSeq(param_1);

	if (track == nullptr)
		return 0;
	else
		return track->readPortApp(param_2, param_3);
}
