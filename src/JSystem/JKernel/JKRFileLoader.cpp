#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <ctype.h>
#include <macros.h>
#ifdef SMS_NATIVE_PLATFORM
#include <string.h>
#include "timg_swap.h"
#endif

JSUList<JKRFileLoader> JKRFileLoader::sVolumeList;
JKRFileLoader* JKRFileLoader::sCurrentVolume;

JKRFileLoader::JKRFileLoader()
    : JKRDisposer()
    , mFileLoaderLink(this)
{
	mVolumeName = nullptr;
	mVolumeType = 0;
	mMountCount = 0;
}

JKRFileLoader::~JKRFileLoader()
{
	if (sCurrentVolume == this)
		sCurrentVolume = nullptr;
}

void JKRFileLoader::unmount()
{
	if (mMountCount != 0) {
		if (--mMountCount == 0)
			delete this;
	}
}

JKRFileLoader* JKRFileLoader::getVolume(const char* name)
{
	for (JSUListIterator<JKRFileLoader> it = sVolumeList.getFirst();
	     it != sVolumeList.getEnd(); ++it) {
		if (strcmp(name, it->mVolumeName) == 0)
			return it.getObject();
	}

	return nullptr;
}

void JKRFileLoader::changeDirectory(const char* dir)
{
	JKRFileLoader* vol = findVolume(&dir);
	if (vol)
		vol->becomeCurrent(dir);
}

void* JKRFileLoader::getGlbResource(const char* path)
{
	JKRFileLoader* loader = findVolume(&path);
	void* res = (loader == nullptr) ? nullptr : loader->getResource(path);
#ifdef SMS_NATIVE_PLATFORM
	// A `.bti` file IS a standalone big-endian ResTIMG. Game code loads these by path
	// — e.g. (ResTIMG*)JKRGetResource("/scene/.../foo.bti") for NPC pollution decals
	// and shared textures injected via J3DTexture::setResTIMG — bypassing the archive
	// 'TIMG'-resType swap in JUTResReference::getResource. Without a swap the header's
	// width/height read as the byteswap of the real value (a 128 reads 0x8000=32768),
	// the texel offset goes wild, and the native renderer rejects the texmap to white
	// (the SB_NPC_ON white-NPC bug). Swap the header here, the natural by-path chokepoint;
	// idempotent per pointer so a shared resource swaps exactly once. Header scalars only
	// (the texel/palette pixel data stays GC-native for the format decoder).
	if (res) {
		size_t n = strlen(path);
		if (n >= 4 && strcasecmp(path + n - 4, ".bti") == 0)
			smsport::assets::restimg_swap_to_host(res);
	}
#endif
	return res;
}

void* JKRFileLoader::getGlbResource(const char* name, JKRFileLoader* fileLoader)
{
	void* resource = nullptr;
	if (fileLoader) {
		return fileLoader->getResource(0, name);
	}

	JSUList<JKRFileLoader>& volumeList = getVolumeList();
	for (JSUListIterator<JKRFileLoader> it = volumeList.getFirst();
	     it != volumeList.getEnd(); ++it) {
		resource = it->getResource(0, name);
		if (resource)
			break;
	}
	return resource;
}

long JKRFileLoader::getResSize(void* resourceBuffer, JKRFileLoader* fileLoader)
{
	long ret = -1; // TODO: this feels wrong, but it matches, so whatever?

	if (fileLoader != nullptr)
		return fileLoader->getResSize(resourceBuffer);

	for (JSUListIterator<JKRFileLoader> it = sVolumeList.getFirst();
	     it != sVolumeList.getEnd(); ++it) {
		ret = it.getObject()->getResSize(resourceBuffer);
		if (ret >= 0)
			break;
	}

	return ret;
}

JKRFileLoader* JKRFileLoader::findVolume(const char** volumeName)
{
	if (*volumeName[0] != '/') {
		return sCurrentVolume;
	}

	char volumeNameBuffer[0x101];
	*volumeName = fetchVolumeName(volumeNameBuffer,
	                              ARRAY_COUNT(volumeNameBuffer), *volumeName);

	for (JSUListIterator<JKRFileLoader> it = sVolumeList.getFirst();
	     it != sVolumeList.getEnd(); ++it) {
		if (strcmp(volumeNameBuffer, it->mVolumeName) == 0)
			return it.getObject();
	}
	return nullptr;
}

JKRFileFinder* JKRFileLoader::findFirstFile(const char* volumeName)
{
	JKRFileFinder* ret = nullptr;

	JKRFileLoader* vol = findVolume(&volumeName);
	if (vol)
		ret = vol->getFirstFile(volumeName);

	return ret;
}

const char* JKRFileLoader::fetchVolumeName(char* buffer, long bufferSize,
                                           const char* path)
{
	static char rootPath[] = "/";
	if (strcmp(path, "/") == 0) {
		strcpy(buffer, rootPath);
		return rootPath;
	} else {
		path++;
		while (*path != 0 && *path != '/') {
			if (1 < bufferSize) {
				*buffer = _tolower(*path);
				buffer++;
				bufferSize--;
			}
			path++;
		}
		buffer[0] = '\0';
		if (path[0] == '\0')
			path = rootPath;
	}

	return path;
}
