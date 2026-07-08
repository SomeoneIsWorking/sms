#ifndef JUT_RESOURCE_HPP
#define JUT_RESOURCE_HPP

#include <dolphin/types.h>

class JKRArchive;
class JSUInputStream;

class JUTResReference {
private:
	/* 0x001 */ u8 mType;
	/* 0x002 */ u8 mNameLength;
	/* 0x003 */ char mName[0x100];

public:
	enum ResType {
		RESTYPE_Null = 0,
		RESTYPE_Unk1 = 1,
		RESTYPE_Unk2 = 2,
		RESTYPE_Unk3 = 3,
		RESTYPE_Unk4 = 4,
	};
	JUTResReference() { mType = 0; }

	void* getResource(JSUInputStream*, u32, JKRArchive*);
	void* getResource(void const*, u32, JKRArchive*);
	void* getResource(u32, JKRArchive*);

	/* @fabricated: read-only diagnostic accessor (SB_J2D_PIC_DUMP), no behavior change */
	const char* getDebugName() const { return mName; }
};
// only rough size known due to
// getPointer__7J2DPaneFP20JSURandomInputStreamUlP10JKRArchive

#endif
