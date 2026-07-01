#include <M3DUtil/MActorData.hpp>
#include <M3DUtil/SampleCtrlModel.hpp>
#include <JSystem/JKernel/JKRFileFinder.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>

static int to_upper_hack(int c)
{
	if (c >= 'a' && c <= 'z')
		return c + ('A' - 'a');

	return c;
}

static int strcmp_ignore_case(const char* fst, const char* snd)
{
	while (*fst != '\0' && *snd != '\0') {
		if (to_upper_hack(*fst) > to_upper_hack(*snd))
			return -1;

		if (to_upper_hack(*fst) < to_upper_hack(*snd))
			return 1;

		++fst;
		++snd;
	}

	if (to_upper_hack(*fst) > to_upper_hack(*snd))
		return -1;

	if (to_upper_hack(*fst) < to_upper_hack(*snd))
		return 1;

	return 0;
}

void MActorAnmDataBase::checkLower(const char* param_1)
{
	for (int i = 0; i < unk0; ++i) {
		if (strcmp_ignore_case(param_1, unk8[i])) {
			// assert?
		}
	}
}

void MActorAnmDataBase::sortByFileNameRaw(void** param_1)
{
	if (unk0 > 1) {
		for (int i = 1; i < unk0; ++i) {
			int j;

			const char* str = unk8[i];
			u16 key         = unk4[i];
			void* prm       = param_1[i];

			for (j = i - 1; j >= 0; --j) {

				if (strcmp_ignore_case(str, unk8[j]) < 0)
					break;

				unk8[j + 1]    = unk8[j];
				unk4[j + 1]    = unk4[j];
				param_1[j + 1] = param_1[j];
			}

			unk8[j + 1]    = str;
			unk4[j + 1]    = key;
			param_1[j + 1] = prm;
		}
	}
}

MActorAnmData::MActorAnmData()
{
	mBckData = nullptr;
	mBpkData = nullptr;
	mBtpData = nullptr;
	mBtkData = nullptr;
	mBrkData = nullptr;
	mBlkData = nullptr;

	unk44            = 0;
	mSampleModelData = nullptr;

	// mIncidentalAnmNum is the count of INCIDENTAL sub-BCK anims (the mIncidentalAnmList
	// length); MActor's ctor sizes `unk10 = new MActorAnmBck*[getIncidentalAnmNum()]` by it
	// and fills it by iterating mIncidentalAnmList. The list is only populated by
	// addIncidentalAnm (an unimplemented decomp stub here), so with no incidental anims it
	// must read 0. The decomp ctor omitted this initializer, so on a non-zeroed heap
	// allocation it held garbage -> a huge/garbage unk10[] whose unaligned tail entries were
	// never assigned -> MActor::updateInSubBck dereferenced an uninitialized MActorAnmBck* and
	// crashed (intermittently, per heap contents). An empty incidental-anm list is exactly 0.
	mIncidentalAnmNum = 0;
	mBckNum = 0;
	mBlkNum = 0;
	mBpkNum = 0;
	mBtpNum = 0;
	mBtkNum = 0;
	mBrkNum = 0;
}

u16 MActorCalcKeyCode(const char* name)
{
	u32 result = 0;
	while (*name != '\0') {
		result = *name++ + result * 5;
	}
	return result;
}

u32 MActorAnmData::partsNameToIdx(const char* name)
{
	typedef JGadget::TList<MActorSubAnmInfo>::iterator I;
	u32 idx = 0;
	for (I it = mIncidentalAnmList.begin(), e = mIncidentalAnmList.end(); it != e; ++idx, ++it)
		if (strcmp(it->unk4, name) == 0)
			return idx;
	return -1;
}

void MActorAnmData::init(const char* param_1, const char** param_2)
{
	char thing[256];
	int uMVar1;

	if (*param_1 != '/')
		uMVar1 = snprintf(thing, 0xff, "%s%s", "/", param_1);
	else
		uMVar1 = snprintf(thing, 0xff, "%s", param_1);

	if (uMVar1 < 0 || uMVar1 > 0xfe)
		return;

	char thing2[256];
	snprintf(thing2, 0xff, "%s%s", thing, "/");

	JKRFileFinder* fileFinder = JKRFileLoader::findFirstFile(thing2);

	JKRFileFinder* finder = fileFinder;
#ifdef SMS_NATIVE_PLATFORM
	// findFirstFile returns null when the model/anim directory isn't present in the
	// mounted archives (a US (GMSE01) asset the JP/PAL decomp expects, or a map-object
	// resource dir absent on this stage). The original do/while assumes a non-null
	// finder; guard it so the actor simply gets no animations instead of crashing.
	if (finder)
#endif
	do {
		addFileNum(finder->mBase.mFileName);
	} while (finder->findNextFile());

	if (param_2 != nullptr)
		for (int i = 0; i == 0 || param_2[i] != nullptr; ++i)
			addFileNum(param_2[i]);

	delete fileFinder;

	if (mBckNum > 0)
		mBckData = new MActorAnmDataEach<J3DAnmTransformKey>(mBckNum);
	if (mBpkNum > 0)
		mBpkData = new MActorAnmDataEach<J3DAnmColorKey>(mBpkNum);
	if (mBtpNum > 0)
		mBtpData = new MActorAnmDataEach<J3DAnmTexPattern>(mBtpNum);
	if (mBtkNum > 0)
		mBtkData = new MActorAnmDataEach<J3DAnmTextureSRTKey>(mBtkNum);
	if (mBrkNum > 0)
		mBrkData = new MActorAnmDataEach<J3DAnmTevRegKey>(mBrkNum);
	if (mBlkNum > 0)
		mBlkData = new MActorAnmDataEach<J3DAnmClusterKey>(mBlkNum);

	mBckNum = 0;
	mBlkNum = 0;
	mBpkNum = 0;
	mBtpNum = 0;
	mBtkNum = 0;
	mBrkNum = 0;

	fileFinder = JKRFileLoader::findFirstFile(thing2);
#ifdef SMS_NATIVE_PLATFORM
	if (fileFinder) // see above: directory may be absent in the mounted archives
#endif
	do {
		strstr(fileFinder->mBase.mFileName, "#");
		addFileTable(fileFinder->mBase.mFileName);
	} while (fileFinder->findNextFile());

	if (param_2 != nullptr && *param_2 != nullptr) {
		for (int i = 0; i == 0 || param_2[i] != nullptr; ++i)
			addFileTable(param_2[i]);
	}

	delete fileFinder;

	// loadAnmPtrArray concatenates param_1 + storedName (+ ext) with NO separator,
	// so param_1 must be the slash-TERMINATED directory (thing2 = "<dir>/"), not the
	// bare directory (thing = "<dir>"). Passing `thing` built e.g.
	// "/yoshiyoshi_born_tx.btp" -> findVolume looks up a volume literally named
	// "yoshiyoshi_born_tx.btp" -> null -> every anim slot left unloaded -> null
	// deref in MActorAnmBtp::setTexNoAnmFullPtr. (Decomp transcription bug; wrong on
	// GC too, hence unguarded.)
	if (mBckData)
		mBckData->loadAnmPtrArray2(thing2, ".bck");
	if (mBpkData)
		mBpkData->loadAnmPtrArray2(thing2, ".bpk");
	if (mBtpData)
		mBtpData->loadAnmPtrArray2(thing2, ".btp");
	if (mBtkData)
		mBtkData->loadAnmPtrArray2(thing2, ".btk");
	if (mBrkData)
		mBrkData->loadAnmPtrArray2(thing2, ".brk");
	if (mBlkData)
		mBlkData->loadAnmPtrArray2(thing2, ".blk");
}

void MActorAnmData::addFileNum(const char* name)
{
	if (strstr(name, ".bck"))
		++mBckNum;
	if (strstr(name, ".bpk"))
		++mBpkNum;
	if (strstr(name, ".btp"))
		++mBtpNum;
	if (strstr(name, ".btk"))
		++mBtkNum;
	if (strstr(name, ".brk"))
		++mBrkNum;
	if (strstr(name, ".blk"))
		++mBlkNum;
}

void MActorAnmData::addFileTable(const char* param_1)
{
	char* pcVar1;
	size_t sVar2;
	size_t sVar3;
	u16 uVar4;
	u32 uVar5;

	pcVar1 = (char*)strstr(param_1, ".bck");
	if (pcVar1 != (char*)0x0) {
		sVar2  = strlen(param_1);
		pcVar1 = (char*)strrchr(param_1, 0x2e);
		sVar3  = strlen(pcVar1);
		uVar5  = sVar2 - (sVar3 - 1);
		pcVar1 = new char[uVar5];
		snprintf(pcVar1, uVar5, "%s", param_1);
		uVar4             = 0;
		mBckData->unk8[mBckNum] = pcVar1;
		while (*pcVar1 != '\0') {
			uVar4 = *pcVar1++ + uVar4 * 5;
		}
		mBckData->unk4[mBckNum] = uVar4;
		++mBckNum;
	}

	pcVar1 = (char*)strstr(param_1, ".bpk");
	if (pcVar1 != (char*)0x0) {
		sVar2  = strlen(param_1);
		pcVar1 = (char*)strrchr(param_1, 0x2e);
		sVar3  = strlen(pcVar1);
		uVar5  = sVar2 - (sVar3 - 1);
		pcVar1 = new char[uVar5];
		snprintf(pcVar1, uVar5, "%s", param_1);
		uVar4             = 0;
		mBpkData->unk8[mBpkNum] = pcVar1;
		while (*pcVar1 != '\0') {
			uVar4 = *pcVar1++ + uVar4 * 5;
		}
		mBpkData->unk4[mBpkNum] = uVar4;
		++mBpkNum;
	}

	pcVar1 = (char*)strstr(param_1, ".btp");
	if (pcVar1 != (char*)0x0) {
		sVar2  = strlen(param_1);
		pcVar1 = (char*)strrchr(param_1, 0x2e);
		sVar3  = strlen(pcVar1);
		uVar5  = sVar2 - (sVar3 - 1);
		pcVar1 = new char[uVar5];
		snprintf(pcVar1, uVar5, "%s", param_1);
		uVar4              = 0;
		mBtpData->unk8[mBtpNum] = pcVar1;
		while (*pcVar1 != '\0') {
			uVar4 = *pcVar1++ + uVar4 * 5;
		}
		mBtpData->unk4[mBtpNum] = uVar4;
		++mBtpNum;
	}

	pcVar1 = (char*)strstr(param_1, ".btk");
	if (pcVar1 != (char*)0x0) {
		sVar2  = strlen(param_1);
		pcVar1 = (char*)strrchr(param_1, 0x2e);
		sVar3  = strlen(pcVar1);
		uVar5  = sVar2 - (sVar3 - 1);
		pcVar1 = new char[uVar5];
		snprintf(pcVar1, uVar5, "%s", param_1);
		uVar4              = 0;
		mBtkData->unk8[mBtkNum] = pcVar1;
		while (*pcVar1 != '\0') {
			uVar4 = *pcVar1++ + uVar4 * 5;
		}
		mBtkData->unk4[mBtkNum] = uVar4;
		++mBtkNum;
	}

	pcVar1 = (char*)strstr(param_1, ".brk");
	if (pcVar1 != (char*)0x0) {
		sVar2  = strlen(param_1);
		pcVar1 = (char*)strrchr(param_1, 0x2e);
		sVar3  = strlen(pcVar1);
		uVar5  = sVar2 - (sVar3 - 1);
		pcVar1 = new char[uVar5];
		snprintf(pcVar1, uVar5, "%s", param_1);
		uVar4              = 0;
		mBrkData->unk8[mBrkNum] = pcVar1;
		while (*pcVar1 != '\0') {
			uVar4 = *pcVar1++ + uVar4 * 5;
		}
		mBrkData->unk4[mBrkNum] = uVar4;
		++mBrkNum;
	}

	pcVar1 = (char*)strstr(param_1, ".blk");
	if (pcVar1 != (char*)0x0) {
		sVar2  = strlen(param_1);
		pcVar1 = (char*)strrchr(param_1, 0x2e);
		sVar3  = strlen(pcVar1);
		uVar5  = sVar2 - (sVar3 - 1);
		pcVar1 = new char[uVar5];
		snprintf(pcVar1, uVar5, "%s", param_1);
		uVar4             = 0;
		mBlkData->unk8[mBlkNum] = pcVar1;
		while (*pcVar1 != '\0') {
			uVar4 = *pcVar1++ + uVar4 * 5;
		}
		mBlkData->unk4[mBlkNum] = uVar4;
		++mBlkNum;
	}
}

void MActorAnmData::createSampleModelData(J3DModelData* data)
{
	mSampleModelData = new SampleCtrlModelData(data);
}
