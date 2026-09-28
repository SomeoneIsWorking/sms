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
	for (int i = 0; i < mAnmNum; ++i) {
		if (strcmp_ignore_case(param_1, mAnmNames[i])) {
			// assert?
		}
	}
}

MActorAnmDataBase::MActorAnmDataBase(int anm_num)
{
	mAnmNum      = anm_num;
	mAnmNames    = new const char*[mAnmNum];
	mAnmKeyCodes = new u16[mAnmNum];
	mAnimations  = nullptr;
}

void MActorAnmDataBase::sortByFileNameRaw(void** anms)
{
	if (mAnmNum > 1) {
		for (int i = 1; i < mAnmNum; ++i) {
			int j;

			const char* str = mAnmNames[i];
			u16 key         = mAnmKeyCodes[i];
			void* prm       = anms[i];

			for (j = i - 1; j >= 0; --j) {

				if (strcmp_ignore_case(str, mAnmNames[j]) < 0)
					break;

				mAnmNames[j + 1]    = mAnmNames[j];
				mAnmKeyCodes[j + 1] = mAnmKeyCodes[j];
				anms[j + 1]         = anms[j];
			}

			mAnmNames[j + 1]    = str;
			mAnmKeyCodes[j + 1] = key;
			anms[j + 1]         = prm;
		}
	}
}

MActorAnmData::MActorAnmData()
{
	mBckAnms = nullptr;
	mBpkAnms = nullptr;
	mBtpAnms = nullptr;
	mBtkAnms = nullptr;
	mBrkAnms = nullptr;
	mBlkAnms = nullptr;

	unk44            = 0;
	mSampleModelData = nullptr;

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
	for (I it = unk1C.begin(), e = unk1C.end(); it != e; ++idx, ++it)
		if (strcmp((*it).unk4, name) == 0)
			return idx;
	return -1;
}

void MActorAnmData::addIncidentalAnm(const char* parts_name, int joint_index)
{
	MActorSubAnmInfo info;
	info.unk0 = joint_index;
	info.unk4 = parts_name;
	++unk0;
	unk1C.push_back(info);
}

void MActorAnmData::init(const char* anm_folder, const char** additional_files)
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
	// findFirstFile returns null when the model/anim directory isn't present in
	// the mounted archives (a US (GMSE01) asset the JP/PAL decomp expects, or a
	// map-object resource dir absent on this stage). The original do/while
	// assumes a non-null finder; guard it so the actor simply gets no
	// animations instead of crashing.
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
		mBckAnms = new MActorAnmDataEach<J3DAnmTransformKey>(mBckNum);
	if (mBpkNum > 0)
		mBpkAnms = new MActorAnmDataEach<J3DAnmColorKey>(mBpkNum);
	if (mBtpNum > 0)
		mBtpAnms = new MActorAnmDataEach<J3DAnmTexPattern>(mBtpNum);
	if (mBtkNum > 0)
		mBtkAnms = new MActorAnmDataEach<J3DAnmTextureSRTKey>(mBtkNum);
	if (mBrkNum > 0)
		mBrkAnms = new MActorAnmDataEach<J3DAnmTevRegKey>(mBrkNum);
	if (mBlkNum > 0)
		mBlkAnms = new MActorAnmDataEach<J3DAnmClusterKey>(mBlkNum);

	mBckNum = 0;
	mBlkNum = 0;
	mBpkNum = 0;
	mBtpNum = 0;
	mBtkNum = 0;
	mBrkNum = 0;

	fileFinder = JKRFileLoader::findFirstFile(thing2);
#ifdef SMS_NATIVE_PLATFORM
	if (fileFinder) // see above: directory may be absent in the mounted
	                // archives
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

	if (mBckAnms)
		mBckAnms->loadAnmPtrArray2(anmFolder, ".bck");
	if (mBpkAnms)
		mBpkAnms->loadAnmPtrArray2(anmFolder, ".bpk");
	if (mBtpAnms)
		mBtpAnms->loadAnmPtrArray2(anmFolder, ".btp");
	if (mBtkAnms)
		mBtkAnms->loadAnmPtrArray2(anmFolder, ".btk");
	if (mBrkAnms)
		mBrkAnms->loadAnmPtrArray2(anmFolder, ".brk");
	if (mBlkAnms)
		mBlkAnms->loadAnmPtrArray2(anmFolder, ".blk");
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

char* MActorAnmData::getSimpleName(const char* file_name)
{
	u32 length = strlen(file_name) - (strlen(strrchr(file_name, '.')) - 1);
	char* simple_name = new char[length];
	snprintf(simple_name, length, "%s", file_name);
	return simple_name;
}

void MActorAnmData::addFileTable(const char* param_1)
{
	if (strstr(param_1, ".bck") != nullptr) {
		char* simple_name               = getSimpleName(param_1);
		mBckAnms->mAnmNames[mBckNum]    = simple_name;
		mBckAnms->mAnmKeyCodes[mBckNum] = MActorCalcKeyCode(simple_name);
		++mBckNum;
	}

	if (strstr(param_1, ".bpk") != nullptr) {
		char* simple_name               = getSimpleName(param_1);
		mBpkAnms->mAnmNames[mBpkNum]    = simple_name;
		mBpkAnms->mAnmKeyCodes[mBpkNum] = MActorCalcKeyCode(simple_name);
		++mBpkNum;
	}

	if (strstr(param_1, ".btp") != nullptr) {
		char* simple_name               = getSimpleName(param_1);
		mBtpAnms->mAnmNames[mBtpNum]    = simple_name;
		mBtpAnms->mAnmKeyCodes[mBtpNum] = MActorCalcKeyCode(simple_name);
		++mBtpNum;
	}

	if (strstr(param_1, ".btk") != nullptr) {
		char* simple_name               = getSimpleName(param_1);
		mBtkAnms->mAnmNames[mBtkNum]    = simple_name;
		mBtkAnms->mAnmKeyCodes[mBtkNum] = MActorCalcKeyCode(simple_name);
		++mBtkNum;
	}

	if (strstr(param_1, ".brk") != nullptr) {
		char* simple_name               = getSimpleName(param_1);
		mBrkAnms->mAnmNames[mBrkNum]    = simple_name;
		mBrkAnms->mAnmKeyCodes[mBrkNum] = MActorCalcKeyCode(simple_name);
		++mBrkNum;
	}

	if (strstr(param_1, ".blk") != nullptr) {
		char* simple_name               = getSimpleName(param_1);
		mBlkAnms->mAnmNames[mBlkNum]    = simple_name;
		mBlkAnms->mAnmKeyCodes[mBlkNum] = MActorCalcKeyCode(simple_name);
		++mBlkNum;
	}
}

void MActorAnmData::createSampleModelData(J3DModelData* data)
{
	mSampleModelData = new SampleCtrlModelData(data);
}
