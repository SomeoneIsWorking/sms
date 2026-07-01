#ifndef M3DUTIL_M_ACTOR_DATA_HPP
#define M3DUTIL_M_ACTOR_DATA_HPP

#include <JSystem/JGadget/std-list.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DAnmLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <printf.h>

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
	MActorAnmDataBase(int param_1)
	{
		unk0 = param_1;
		unk8 = new const char*[unk0];
		unk4 = new u16[unk0];
		unkC = nullptr;
	}

	void sortByFileNameRaw(void**);
	void checkLower(const char*);

	// fabricated
	int getAnmNum() const { return unk0; }
	u16 getKeyCode(int i) { return unk4[i]; }
	const char* getName(int i) { return unk8[i]; }

public:
	/* 0x0 */ int unk0;
	/* 0x4 */ u16* unk4;
	/* 0x8 */ const char** unk8;
	/* 0xC */ J3DAnmBase** unkC;
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

	void loadAnmPtrArray(const char* param_1, const char* param_2)
	{
		unkC = new J3DAnmBase*[unk0];
		for (int i = 0; i < unk0; ++i) {
			char buf[256];
			if (*unk8[i] != '/') {
				char tmp[256];
				snprintf(tmp, 0xff, "%s%s", param_1, unk8[i]);
				snprintf(buf, 0xff, "%s%s", tmp, param_2);
			} else {
				snprintf(buf, 0xff, "%s%s", unk8[i], param_2);
			}
			void* res = JKRGetResource(buf);
			if (res)
				unkC[i] = J3DAnmLoaderDataBase::load(res);
		}
		char trash[8];
		sortByFileNameRaw((void**)unkC);
	}

	T* getAnmPtr(int idx) const
	{
		if (idx < unk0)
			return static_cast<T*>(unkC[idx]);
		return nullptr;
	}
};

struct MActorSubAnmInfo {
	/* 0x0 */ u16 unk0;
	/* 0x4 */ const char* unk4;
};

class MActorAnmData {
public:
	MActorAnmData();
	~MActorAnmData() { }

	void createSampleModelData(J3DModelData*);
	void addFileTable(const char*);
	void getSimpleName(const char*);
	void addFileNum(const char*);
	void init(const char*, const char**);
	void addIncidentalAnm(const char*, int);
	u32 partsNameToIdx(const char*);

	// fabricated. getUnk2C..getUnk40/getUnk48 keep their names (widely called); they now
	// return the renamed fields. The per-anm-kind counts (unkN) double as write indices
	// during addFileTable's second pass (reset to 0 after init), hence the plain `Num`.
	s32 getIncidentalAnmNum() { return mIncidentalAnmNum; }
	SampleCtrlModelData* getUnk48() { return mSampleModelData; }
	MActorAnmDataEach<J3DAnmTransformKey>* getUnk2C() { return mBckData; }
	MActorAnmDataEach<J3DAnmColorKey>* getUnk30() { return mBpkData; }
	MActorAnmDataEach<J3DAnmTexPattern>* getUnk34() { return mBtpData; }
	MActorAnmDataEach<J3DAnmTextureSRTKey>* getUnk38() { return mBtkData; }
	MActorAnmDataEach<J3DAnmTevRegKey>* getUnk3C() { return mBrkData; }
	MActorAnmDataEach<J3DAnmClusterKey>* getUnk40() { return mBlkData; }

public:
	/* 0x0 */ int mIncidentalAnmNum;  // incidental sub-BCK count (== mIncidentalAnmList len)
	/* 0x4 */ int mBckNum;            // per-kind file counts (also reused as indices)
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
