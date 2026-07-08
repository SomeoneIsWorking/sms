#include <JSystem/JUtility/JUTResource.hpp>
#include <JSystem/JKernel/JKRArchive.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>
#include <string.h>
#ifdef SMS_NATIVE_PLATFORM
#include "timg_swap.h"    // big-endian ResTIMG header -> host endianness (J2D archive textures)
#include "restlut_swap.h" // big-endian ResTLUT header -> host endianness (J2D indexed pane palettes)
#endif

void* JUTResReference::getResource(JSUInputStream* stream, u32 resType,
                                   JKRArchive* archive)
{
	stream->read(&mType, 1);
	stream->read(&mNameLength, 1);
	stream->read(&mName, mNameLength);

	if (mType == RESTYPE_Unk2 || mType == RESTYPE_Unk3
	    || mType == RESTYPE_Unk4) {
		mName[mNameLength] = 0;
	}

	return getResource(resType, archive);
}

void* JUTResReference::getResource(u32 resType, JKRArchive* archive)
{
	void* res = NULL;
	switch (mType) {
	case RESTYPE_Unk1:
		break;
	case RESTYPE_Unk2:
		res = JKRArchive::getGlbResource(resType, mName, archive);
		break;
	case RESTYPE_Unk3:
		res = JKRFileLoader::getGlbResource(mName, archive);
		break;
	case RESTYPE_Unk4:
		res = JKRFileLoader::getGlbResource(mName);
		break;
	}

#ifdef SMS_NATIVE_PLATFORM
	// A 'TIMG' resource is a raw big-endian ResTIMG from the archive; swap its header to
	// host endianness once so the native J2D/JUTTexture path reads width/height/offset
	// correctly (the textured-2D render path decodes these directly). Idempotent per ptr.
	if (res && resType == 'TIMG')
		smsport::assets::restimg_swap_to_host(res);
	// A 'TLUT' resource is the standalone big-endian palette for an indexed (C4/C8)
	// J2DPicture pane; swap its numColors header field so JUTPalette::storeTLUT sizes
	// the GXInitTlutObj/GXLoadTlut upload correctly instead of reading a byte-swapped
	// garbage count. Idempotent per ptr.
	if (res && resType == 'TLUT')
		smsport::assets::restlut_swap_to_host(res);
#endif
	return res;
}
