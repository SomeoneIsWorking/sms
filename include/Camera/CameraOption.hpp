#ifndef CAMERA_CAMERA_OPTION_HPP
#define CAMERA_CAMERA_OPTION_HPP

#include <JSystem/JGeometry/JGVec3.hpp>

class TCameraOption {
public:
	TCameraOption(JGeometry::TVec3<f32>, JGeometry::TVec3<f32>*);

	void moveToLoadFromTitle();
	void moveToTitleFromLoad();
	void moveToUp();
	void moveToDown();

public:
	// Named from the title/file-select camera flow (CameraOption.cpp ctrlOptionCamera_ +
	// the moveTo* triggers, gated by TCardLoad::perform). Each "Frames" is the constant
	// duration; the matching "Timer" counts down to 0 in ctrlOptionCamera_ while the camera
	// CLB-chases mCurrentTarget over that many frames. mFlags bit 0x2 = still in the title
	// (intro) phase; cleared by moveToLoadFromTitle. bit 0x1 = a cube-camera handoff toggle.
	/* 0x0 */ u8 mFlags;                       // unk0 (bit2=title phase, bit1=cube handoff)
	/* 0x4 */ f32 mFovy;                       // mFovYunk4 — option-camera field of view
	/* 0x8 */ s16 unk8;                        // (=300 in ctor; not yet identified)
	/* 0xA */ s16 mIntroChaseTimer;            // unkA — title intro pan (300→0); gates file-select
	/* 0xC */ s16 mLoadPanFrames;              // unkC — duration copied into mLoadPanTimer (=120)
	/* 0xE */ s16 mLoadPanTimer;               // unkE — title→file-select pan (set by moveToLoadFromTitle)
	/* 0x10 */ s16 mCubePanFrames;             // unk10 — duration copied into mCubePanTimer (=80)
	/* 0x12 */ s16 mCubePanTimer;              // unk12 — cube-camera handoff pan
	/* 0x14 */ s16 mUpDownPanFrames;           // unk14 — duration copied into mUpDownPanTimer (=60)
	/* 0x16 */ s16 mUpDownPanTimer;            // unk16 — moveToUp/moveToDown pan
	/* 0x18 */ JGeometry::TVec3<f32> mTitlePos;// unk18 — title (intro) camera placement
	/* 0x24 */ JGeometry::TVec3<f32> mLoadPos; // unk24 — file-load camera placement
	/* 0x30 */ JGeometry::TVec3<f32> mUpPos;   // unk30 — "up" camera placement
	/* 0x3C */ JGeometry::TVec3<f32>* mDestPos;// unk3C — external dest position the chase writes
};

extern TCameraOption* gpCameraOption;

#endif
