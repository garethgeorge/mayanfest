#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif


#include <string.h>
#include <assert.h>

#include "diskinterface.h"

#define BITMAP_GET(bitmap, idx) ((*(bitmap + idx / 8) >> (idx % 8)) & 1)
#define BITMAP_SET(bitmap, idx) *(bitmap + idx / 8) = (BITMAP_GET(bitmap, idx) | (1 << idx % 8))
#define BITMAP_CLEAR(bitmap, idx) *(bitmap + idx / 8) = (BITMAP_GET(bitmap, idx) & ~(1 << idx % 8))

#define CHUNKBIT_GET(fs, idx) BITMAP_GET(fs->chunk_usage_bitmap, idx - fs->bufferspace_offset)

#define FS_USAGEBITMAP_OFFSET 1

struct SuperBlock {
	Disk *disk;
	Size superblock_size;
	Size superblock_size_chunks;
	Size chunk_usage_bitmap_offset;
	Size chunk_usage_bitmap_size;
	Byte *chunk_usage_bitmap;
	Byte *superblock_buffer;

	Size storage_offset;
};

typedef struct SuperBlock SuperBlock;

extern void serializeSize(Size value, Byte *outbuf);
extern Size deserializeSize(Byte *inbuf);

extern void superblock_create(Disk *disk, SuperBlock *superblock);
extern void superblock_load(Disk *disk, SuperBlock *superblock);
extern void superblock_flush(SuperBlock *superblock);


struct Filesystem {
	Disk *disk;
	Byte *chunk_usage_bitmap;
	Size bufferspace_offset;
};

typedef struct Filesystem Filesystem;

/*
	The layout of the filesystem
	 - the first disk size / (chunk_size * 8) bytes of the disk are allocated to 
	   a bitmap which records which chunks are currently in use
	   NOTE: this size is rounded up to the nearest multiple of 1 chunk
*/

// extern Size filesystem_read_chunksequence(Filesystem *fs, Size start_idx, Size stop_idx, Byte *outbuf);

// extern Size filesystem_write_chunksequence(Filesystem *fs, Size start_idx, Size stop_idx, Byte *inbuf);


#ifdef __cplusplus
}
#endif

#endif