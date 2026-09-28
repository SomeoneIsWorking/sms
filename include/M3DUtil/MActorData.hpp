#ifndef M3DUTIL_M_ACTOR_DATA_HPP
#define M3DUTIL_M_ACTOR_DATA_HPP

#include <JSystem/JGadget/std-list.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DAnmLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <stdio.h>

class J3DModelData;
class J3DAnmTransformKey;
class J3DAnmColorKey;
class J3DAnmTexPattern;
class J3DAnmTextureSRTKey;
class J3DAnmTevRegKey;
class J3DAnmClusterKey;
class SampleCtrlModelData;

class MActorAnmDataBase {
public:
	MActorAnmDataBase(int anm_num);

	void sortByFileNameRaw(void** anms);
	void checkLower(const char*);

	// fabricated
	int getAnmNum() const { return mAnmNum; }
	u16 getKeyCode(int i) { return mAnmKeyCodes[i]; }
	const char* getName(int i) { return mAnmNames[i]; }

public:
	/* 0x0 */ int mAnmNum;
	/* 0x4 */ u16* mAnmKeyCodes;
	/* 0x8 */ const char** mAnmNames;
	/* 0xC */ J3DAnmBase** mAnimations;
};

template <class T> class MActorAnmDataEach : public MActorAnmDataBase {
public:
	MActorAnmDataEach(int param_1)
	    : MActorAnmDataBase(param_1)
	{
	}

	// TODO: fake, get rid of it
	void loadAnmPtrArray2(const char* param_1, const char* param_2)
	{
		loadAnmPtrArray(param_1, param_2);
	}

	void loadAnmPtrArray(const char* directory, const char* extension)
	{
		mAnimations = new J3DAnmBase*[mAnmNum];
		for (int i = 0; i < mAnmNum; ++i) {
			char buf[256];
			if (*mAnmNames[i] != '/') {
				char tmp[256];
				snprintf(tmp, 0xff, "%s%s", directory, mAnmNames[i]);
				snprintf(buf, 0xff, "%s%s", tmp, extension);
			} else {
				snprintf(buf, 0xff, "%s%s", mAnmNames[i], extension);
			}
			void* res = JKRGetResource(buf);
			if (res)
				mAnimations[i] = J3DAnmLoaderDataBase::load(res);
		}

		sortByFileNameRaw((void**)mAnimations);
	}

	T* getAnmPtr(int idx) const
	{
		if (idx < mAnmNum)
			return static_cast<T*>(mAnimations[idx]);
		return nullptr;
	}
};

struct MActorSubAnmInfo {
	/* 0x0 */ u16 unk0;
	/* 0x4 */ const char* unk4;
};

/**
 * @brief A library of animations shared by multiple MActors.
 */
class MActorAnmData {
public:
	MActorAnmData();
	~MActorAnmData() { }

	void createSampleModelData(J3DModelData*);
	void addFileTable(const char*);
	char* getSimpleName(const char*);
	void addFileNum(const char*);
	void init(const char*, const char**);
	void addIncidentalAnm(const char*, int);
	u32 partsNameToIdx(const char*);

	// Fabricated accessors named for the animation resource each typed table
	// owns, added by 791df919 ("Name MActor animation data accessors") and
	// kept by the 2026-09-28 upstream merge's call sites, which were held at
	// our side while this header was taken from upstream. The per-anm-kind
	// counts double as write indices during addFileTable's second pass.
	s32 getIncidentalAnmNum() { return mIncidentalAnmNum; }
	SampleCtrlModelData* getSampleModelData() { return mSampleModelData; }
	MActorAnmDataEach<J3DAnmTransformKey>* getBckData() { return mBckData; }
	MActorAnmDataEach<J3DAnmColorKey>* getBpkData() { return mBpkData; }
	MActorAnmDataEach<J3DAnmTexPattern>* getBtpData() { return mBtpData; }
	MActorAnmDataEach<J3DAnmTextureSRTKey>* getBtkData() { return mBtkData; }
	MActorAnmDataEach<J3DAnmTevRegKey>* getBrkData() { return mBrkData; }
	MActorAnmDataEach<J3DAnmClusterKey>* getBlkData() { return mBlkData; }

public:
	/* 0x0 */ int mIncidentalAnmNum; // incidental sub-BCK count (==
	                                 // mIncidentalAnmList len)
	/* 0x4 */ int mBckNum; // per-kind file counts (also reused as indices)
	/* 0x8 */ int mBlkNum;
	/* 0xC */ int mBpkNum;
	/* 0x10 */ int mBtpNum;
	/* 0x14 */ int mBtkNum;
	/* 0x18 */ int mBrkNum;
	/* 0x1C */ JGadget::TList<MActorSubAnmInfo> mIncidentalAnmList;
	/* 0x2C */ MActorAnmDataEach<J3DAnmTransformKey>* mBckData;
	/* 0x30 */ MActorAnmDataEach<J3DAnmColorKey>* mBpkData;
	/* 0x34 */ MActorAnmDataEach<J3DAnmTexPattern>* mBtpData;
	/* 0x38 */ MActorAnmDataEach<J3DAnmTextureSRTKey>* mBtkData;
	/* 0x3C */ MActorAnmDataEach<J3DAnmTevRegKey>* mBrkData;
	/* 0x40 */ MActorAnmDataEach<J3DAnmClusterKey>* mBlkData;
	/* 0x44 */ u32 unk44;
	/* 0x48 */ SampleCtrlModelData* mSampleModelData;
};

u16 MActorCalcKeyCode(const char* name);

#endif
