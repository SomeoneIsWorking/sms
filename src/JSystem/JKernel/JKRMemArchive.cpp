#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <JSystem/JKernel/JKRDecomp.hpp>
#include <JSystem/JKernel/JKRDvdRipper.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#include <JSystem/JUtility/JUTAssert.hpp>
#include <string.h>

#ifdef SMS_NATIVE_PLATFORM
// =============================================================================
// Native PC build: RARC archives are GC BIG-ENDIAN, but the decomp parses them
// by casting raw file bytes to host-endian structs -> every multi-byte field is
// misread on a little-endian host (e.g. header_length 0x00000020 -> 0x20000000,
// giving a wild mArcInfoBlock). Two boundaries must be crossed at load:
//   1. ENDIAN: swap the RARC metadata (header, data-info, dir-entries) in place
//      to host endianness. String table + file data stay BE (their own loaders
//      handle them: strings are bytes, BMD/etc. go through bmd_swap at use).
//   2. LP64 LAYOUT: SDIFileEntry has a trailing `void* mData` (a real runtime
//      heap pointer the engine stores per-resource), so sizeof = 0x18 on LP64
//      while the FILE packs entries at a 0x14 stride. mData cannot be narrowed
//      (DvdArchive/AramArchive store separately-allocated buffers there, and
//      findPtrResource compares it to a host pointer). So we build a host-stride
//      SIDE ARRAY of SDIFileEntry from the packed 0x14 BE table and point
//      mFileEntries at it; it is freed on unmount.
// =============================================================================
namespace {
inline void sw16(void* p)
{
	u8* b = (u8*)p;
	u8 t  = b[0];
	b[0]  = b[1];
	b[1]  = t;
}
inline void sw32(void* p)
{
	u8* b = (u8*)p;
	u8 t;
	t = b[0]; b[0] = b[3]; b[3] = t;
	t = b[1]; b[1] = b[2]; b[2] = t;
}
inline u16 be16(const u8* p) { return (u16)(((u16)p[0] << 8) | p[1]); }
inline u32 be32(const u8* p)
{
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

// Swap RARC metadata in place (header/data-info/dir-entries) and return a
// heap-allocated host-stride file-entry side array (count = num_file_entries).
// Must be called exactly once per freshly-loaded RARC buffer.
JKRArchive::SDIFileEntry* sb_rarc_swap_to_host(void* buffer, JKRHeap* heap)
{
	JKRArchive::SArcHeader* h = (JKRArchive::SArcHeader*)buffer;
	// SArcHeader: 8 x u32 (signature included -> the 'RARC' compare then matches
	// on a host reading the swapped bytes as host-endian).
	sw32(&h->signature);
	sw32(&h->file_length);
	sw32(&h->header_length);
	sw32(&h->file_data_offset);
	sw32(&h->file_data_length);
	sw32(&h->_14);
	sw32(&h->_18);
	sw32(&h->_1C);

	JKRArchive::SArcDataInfo* di
	    = (JKRArchive::SArcDataInfo*)((u8*)h + h->header_length);
	sw32(&di->num_nodes);
	sw32(&di->node_offset);
	sw32(&di->num_file_entries);
	sw32(&di->file_entry_offset);
	sw32(&di->string_table_length);
	sw32(&di->string_table_offset);
	sw16(&di->nextFreeFileID);
	// isSyncIDs is a u8 bool -> no swap.

	u8* infoBase = (u8*)&di->num_nodes;

	// Dir entries are 0x10 on both archs (no pointer) -> swap in place.
	JKRArchive::SDIDirEntry* dirs
	    = (JKRArchive::SDIDirEntry*)(infoBase + di->node_offset);
	for (u32 i = 0; i < di->num_nodes; i++) {
		sw32(&dirs[i].mType);
		sw32(&dirs[i].mOffset);
		sw16(&dirs[i]._08);
		sw16(&dirs[i].mNum);
		sw32(&dirs[i].mFirstIdx);
	}

	// File entries: packed 0x14 BE in the blob -> host 0x18 side array.
	u32 n      = di->num_file_entries;
	u8* feBase = infoBase + di->file_entry_offset;
	JKRArchive::SDIFileEntry* side = (JKRArchive::SDIFileEntry*)JKRHeap::alloc(
	    n * sizeof(JKRArchive::SDIFileEntry), 4, heap);
	for (u32 i = 0; i < n; i++) {
		const u8* src              = feBase + (size_t)i * 0x14;
		side[i].mFileID            = be16(src + 0x00);
		side[i].mHash              = be16(src + 0x02);
		side[i].mFlagsAndNameOffset = be32(src + 0x04);
		side[i].mDataOffset        = be32(src + 0x08);
		side[i].mSize              = be32(src + 0x0C);
		side[i].mData              = nullptr;
	}
	return side;
}
} // namespace
#endif

JKRMemArchive::JKRMemArchive() { }

JKRMemArchive::JKRMemArchive(s32 entryNum,
                             JKRArchive::EMountDirection mountDirection)
    : JKRArchive(entryNum, MOUNT_MEM)
{
	mMountDirection = mountDirection;
	open(entryNum, mMountDirection);

	mVolumeType = 'RARC';
	mVolumeName = mStrTable + mDirectories->mOffset;

	JSULink<JKRFileLoader>* fileLoaderLinkPtr = &mFileLoaderLink;
	getVolumeList().prepend(fileLoaderLinkPtr);
	mIsMounted = true;
}

JKRMemArchive::JKRMemArchive(void* buffer, u32 bufferSize,
                             JKRMemBreakFlag memBreakFlag)
    : JKRArchive((s32)(intptr_t)buffer, MOUNT_MEM) // WTF?
{
	open(buffer, bufferSize, memBreakFlag);

	mVolumeType = 'RARC';
	mVolumeName = mStrTable + mDirectories->mOffset;

	JSULink<JKRFileLoader>* fileLoaderLinkPtr = &mFileLoaderLink;
	getVolumeList().prepend(fileLoaderLinkPtr);

	mIsMounted = true;
}

JKRMemArchive::~JKRMemArchive()
{
	if (mIsMounted == true) {
		if (mIsOpen) {
			if (mArcHeader)
				JKRFreeToHeap(mHeap, mArcHeader);
		}
#ifdef SMS_NATIVE_PLATFORM
		if (mFileEntries) { // host-stride side array (open() built it)
			JKRHeap::free(mFileEntries, mHeap);
			mFileEntries = nullptr;
		}
#endif
		getVolumeList().remove(&mFileLoaderLink);
		mIsMounted = false;
	}
}

void JKRMemArchive::fixedInit(s32 param_1)
{
	mIsMounted  = false;
	mMountMode  = 1;
	mMountCount = 1;
	_54         = 2;
	mHeap       = JKRHeap::sCurrentHeap;
	mEntryNum   = param_1;
	if (sCurrentVolume)
		return;
	sCurrentVolume = this;
	setCurrentDirID(0);
}

bool JKRMemArchive::mountFixed(void* param_1, JKRMemBreakFlag param_2)
{
	if (check_mount_already((s32)(intptr_t)param_1)) {
		return false;
	}
	fixedInit((s32)(intptr_t)param_1);
	if (!open(param_1, 0xffff, param_2)) {
		return false;
	}
	mVolumeType = 'RARC';
	mVolumeName = mStrTable + mDirectories->mOffset;
	sVolumeList.prepend(&mFileLoaderLink);
	mIsOpen    = param_2 == 1 ? true : false; // ewwwww
	mIsMounted = true;
	return true;
}

void JKRMemArchive::unmountFixed()
{
	JUT_ASSERT(337, isMounted());
	JUT_ASSERT(340, mMountCount == 1);
	if (sCurrentVolume == this) {
		sCurrentVolume = nullptr;
	}
	if (mIsOpen && mArcHeader) {
		JKRHeap::free(mArcHeader, mHeap);
	}
#ifdef SMS_NATIVE_PLATFORM
	if (mFileEntries) { // host-stride side array (open() built it)
		JKRHeap::free(mFileEntries, mHeap);
		mFileEntries = nullptr;
	}
#endif
	sVolumeList.remove(&mFileLoaderLink);
	mIsMounted = false;
}

bool JKRMemArchive::open(s32 entryNum,
                         JKRArchive::EMountDirection mountDirection)
{
	mArcHeader      = nullptr;
	mArcInfoBlock   = nullptr;
	mArchiveData    = nullptr;
	mDirectories    = nullptr;
	mFileEntries    = nullptr;
	mStrTable       = nullptr;
	mIsOpen         = false;
	mMountDirection = mountDirection;

	if (mMountDirection == JKRArchive::MOUNT_DIRECTION_HEAD) {
		u32 loadedSize;
		mArcHeader = (SArcHeader*)JKRDvdRipper::loadToMainRAM(
		    entryNum, nullptr, EXPAND_SWITCH_DECOMPRESS, 0, mHeap,
		    JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, (int*)&mCompression);
	} else {
		u32 loadedSize;
		mArcHeader = (SArcHeader*)JKRDvdRipper::loadToMainRAM(
		    entryNum, nullptr, EXPAND_SWITCH_DECOMPRESS, 0, mHeap,
		    JKRDvdRipper::ALLOC_DIRECTION_BACKWARD, 0, (int*)&mCompression);
	}

	if (!mArcHeader) {
		mMountMode = UNKNOWN_MOUNT_MODE;
	} else {
#ifdef SMS_NATIVE_PLATFORM
		SDIFileEntry* nativeFileEntries = sb_rarc_swap_to_host(
		    mArcHeader, mHeap ? mHeap : JKRHeap::sCurrentHeap);
#endif
		JUT_ASSERT(418, mArcHeader->signature == 'RARC');
		mArcInfoBlock
		    = (SArcDataInfo*)((u8*)mArcHeader + mArcHeader->header_length);
		mDirectories = (SDIDirEntry*)((u8*)&mArcInfoBlock->num_nodes
		                              + mArcInfoBlock->node_offset);
		mFileEntries = (SDIFileEntry*)((u8*)&mArcInfoBlock->num_nodes
		                               + mArcInfoBlock->file_entry_offset);
		mStrTable    = (char*)((u8*)&mArcInfoBlock->num_nodes
                            + mArcInfoBlock->string_table_offset);

		mArchiveData = (u8*)((uintptr_t)mArcHeader + mArcHeader->header_length
		                     + mArcHeader->file_data_offset);
#ifdef SMS_NATIVE_PLATFORM
		mFileEntries = nativeFileEntries; // host-stride side array (see top)
#endif
		mIsOpen      = true;
	}

	return (mMountMode == UNKNOWN_MOUNT_MODE) ? false : true;
}

bool JKRMemArchive::open(void* buffer, u32 bufferSize, JKRMemBreakFlag flag)
{
	mArcHeader = (SArcHeader*)buffer;
#ifdef SMS_NATIVE_PLATFORM
	SDIFileEntry* nativeFileEntries
	    = sb_rarc_swap_to_host(buffer, JKRHeap::findFromRoot(buffer));
#endif
	JUT_ASSERT(471, mArcHeader->signature == 'RARC');
	mArcInfoBlock
	    = (SArcDataInfo*)((u8*)mArcHeader + mArcHeader->header_length);
	mDirectories = (SDIDirEntry*)((u8*)&mArcInfoBlock->num_nodes
	                              + mArcInfoBlock->node_offset);
	mFileEntries = (SDIFileEntry*)((u8*)&mArcInfoBlock->num_nodes
	                               + mArcInfoBlock->file_entry_offset);
	mStrTable    = (char*)((u8*)&mArcInfoBlock->num_nodes
                        + mArcInfoBlock->string_table_offset);
	mArchiveData = (u8*)(((uintptr_t)mArcHeader + mArcHeader->header_length)
	                     + mArcHeader->file_data_offset);
#ifdef SMS_NATIVE_PLATFORM
	mFileEntries = nativeFileEntries; // host-stride side array (see top)
#endif
	mIsOpen      = (flag == MBF_1) ? true : false; // mIsOpen might be u8
	mHeap        = JKRHeap::findFromRoot(buffer);
	mCompression = JKR_COMPRESSION_NONE;
	return true;
}

void* JKRMemArchive::fetchResource(SDIFileEntry* fileEntry, u32* resourceSize)
{
	JUT_ASSERT(535, isMounted());
	if (!fileEntry->mData) {
		fileEntry->mData = mArchiveData + fileEntry->mDataOffset;
	}

	if (resourceSize) {
		*resourceSize = fileEntry->mSize;
	}

	return fileEntry->mData;
}

void* JKRMemArchive::fetchResource(void* buffer, u32 bufferSize,
                                   SDIFileEntry* fileEntry, u32* resourceSize)
{
	JUT_ASSERT(575, isMounted());
	bufferSize    = ALIGN_PREV(bufferSize, 0x20);
	u32 srcLength = ALIGN_NEXT(fileEntry->mSize, 0x20);
	if (srcLength > bufferSize) {
		srcLength = bufferSize;
	}

	if (fileEntry->mData != nullptr) {
		JKRHeap::copyMemory(buffer, fileEntry->mData, srcLength);
	} else {
		JKRCompression compression = JKRArchive::convertAttrToCompressionType(
		    fileEntry->mFlagsAndNameOffset >> 24);
		srcLength = fetchResource_subroutine(
		    (u8*)mArchiveData + fileEntry->mDataOffset, srcLength, (u8*)buffer,
		    bufferSize, compression);
	}

	if (resourceSize) {
		*resourceSize = srcLength;
	}

	return buffer;
}

void JKRMemArchive::removeResourceAll()
{
	JUT_ASSERT(622, isMounted());

	if (mArcInfoBlock == nullptr)
		return;
	if (mMountMode == MOUNT_MEM)
		return;

	// !@bug: looping over file entries without incrementing the fileEntry
	// pointer. Thus, only the first fileEntry will clear/remove the resource
	// data.
	SDIFileEntry* fileEntry = mFileEntries;
	for (int i = 0; i < mArcInfoBlock->num_file_entries; i++) {
		if (fileEntry->mData) {
			fileEntry->mData = nullptr;
		}
	}
}

bool JKRMemArchive::removeResource(void* resource)
{
	JUT_ASSERT(653, isMounted());

	SDIFileEntry* fileEntry = findPtrResource(resource);
	if (!fileEntry)
		return false;

	fileEntry->mData = nullptr;
	return true;
}

u32 JKRMemArchive::fetchResource_subroutine(u8* src, u32 srcLength, u8* dst,
                                            u32 dstLength, int compression)
{
	dstLength = ALIGN_PREV(dstLength, 0x20);
	srcLength = ALIGN_NEXT(srcLength, 0x20);

	switch (compression) {
	case JKR_COMPRESSION_NONE: {
		if (srcLength > dstLength) {
			srcLength = dstLength;
		}

		JKRHeap::copyMemory(dst, src, srcLength);
		return srcLength;
	}
	case JKR_COMPRESSION_YAY0:
	case JKR_COMPRESSION_YAZ0: {
		int expandedSize = JKRDecompExpandSize(src);
		srcLength        = expandedSize;
		if (expandedSize > dstLength) {
			srcLength = dstLength;
		}

		JKRDecompress(src, dst, srcLength, 0);
		return srcLength;
	}
	default:
		OSPanic(__FILE__, 720, ":::??? bad sequence\n");
		break;
	}

	return 0;
}
