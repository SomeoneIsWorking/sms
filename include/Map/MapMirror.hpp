#ifndef MAP_MAP_MIRROR_HPP
#define MAP_MAP_MIRROR_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JDrama/JDRCamera.hpp>
#include <JSystem/JDrama/JDRDrawBufObj.hpp>

struct ResTIMG;
class MActor;
class MActorAnmData;
class J3DModel;

class TMirrorCamera : public JDrama::TCamera {
public:
	TMirrorCamera(const char* name = "鏡用カメラ");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void makeMirrorViewMtx();
	void drawSetting(MtxPtr);
	void calcEffectMtx(MtxPtr);

	MtxPtr getMirrorViewMtx() { return mMirrorViewMtx; }
	void setMirrorPlane(f32 x, f32 y, f32 z, f32 dot)
	{
		mPlaneNormal.set(x, y, z);
		mPlaneD = dot;
	}

	const ResTIMG* getMirrorTexResource() const { return mMirrorTexResource; }

public:
	/* 0x30 */ Mtx mMirrorViewMtx;
	/* 0x60 */ GXTexObj mMirrorTexObj;
	/* 0x80 */ f32 mFovyScale;                        // 1.3f — MirrorCamera fov = mFovyScale * gpCamera->mFovy
	/* 0x84 */ JGeometry::TVec3<f32> mPlaneNormal;
	/* 0x90 */ f32 mPlaneD;
	/* 0x94 */ ResTIMG* mMirrorTexResource;
	/* 0x98 */ JGeometry::TVec3<f32> mReflectedPos;
};

class TMirrorModel {
public:
	void initPlaneInfo();
	void entry();
	void calcView();
	void getMirrorTexInfo();

	virtual void init(const char*);
	virtual void calc() { }
	virtual void setPlane();
	virtual const JGeometry::TVec3<f32>& getNormalVec() const { return mPlaneNormal; }
	virtual f32 getD() const { return mPlaneD; }

	TMirrorModel();

	// fabricated
	MActor* getMActor() { return mMActor; }

public:
	/* 0x4 */ MActor* mMActor;
	/* 0x8 */ TMirrorCamera* mMirrorCamera;
	/* 0xC */ JGeometry::TVec3<f32> mPlanePoint;
	/* 0x18 */ JGeometry::TVec3<f32> mPlaneNormal;
	/* 0x24 */ f32 mPlaneD;
};

class TMirrorModelObj : public TMirrorModel {
public:
	virtual void init(const char*);
	virtual void calc();
	virtual void setPlane();

public:
	/* 0x28 */ J3DModel* mSourceModel;
};

class TMirrorModelManager;

extern TMirrorModelManager* gpMirrorModelManager;

class TMirrorModelManager : public JDrama::TViewObj {
public:
	TMirrorModelManager(const char* name = "鏡表示モデル管理");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	bool isUpperThanMirrorPlane(const JGeometry::TVec3<f32>&) const;
	bool isInMirror(JGeometry::TVec3<f32>&) const;
	void findMirrorCamera();
	void registerObjMirror(TMirrorModel*);

	// fabricated
	MActorAnmData* getMirrorAnmData() { return mMirrorAnmData; }
	bool isCurrentMirrorPresent() { return mCurrentMirrorIndex != -1 ? true : false; }

public:
	/* 0x10 */ int mMirrorModelCount;
	/* 0x14 */ int mTotalMirrorSlots;
	/* 0x18 */ int mCurrentMirrorIndex;                // -1 = no active mirror; else index into mMirrorModels — the "mirror gate"
	/* 0x1C */ TMirrorModel** mMirrorModels;
	/* 0x20 */ MActorAnmData* mMirrorAnmData;
	/* 0x24 */ TMirrorCamera* mMirrorCamera;           // cached from findMirrorCamera()
	/* 0x28 */ u16 unk28;
};

class TMirrorMapDrawBuf : public JDrama::TDrawBufObj {
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
};

#endif
