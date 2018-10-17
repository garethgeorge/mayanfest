#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif


#include <string.h>
#include <assert.h>

#include "diskinterface.h"

#define BITMAP_GET(bitmap, idx) ((*(bitmap + idx / 8) >> (idx % 8)) & 1)
#define BITMAP_SET(bitmap, idx) *(bitmap + idx / 8) = *(bitmap + idx) | (1 << idx % 8)
#define BITMAP_CLEAR(bitmap, idx) *(bitmap + idx / 8) = *(bitmap + idx) & ~(1 << idx % 8)

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
	Size storage_size;
};

typedef struct SuperBlock SuperBlock;

extern void serializeSize(Size value, Byte *outbuf);
extern Size deserializeSize(Byte *inbuf);

extern void superblock_create(Disk *disk, SuperBlock *superblock);
extern void superblock_load(Disk *disk, SuperBlock *superblock);
extern void superblock_flush(SuperBlock *superblock);
extern void superblock_free(SuperBlock *superblock);

struct FileSystem {
	Disk *disk;
	SuperBlock superblock;
};

typedef struct FileSystem FileSystem;

extern FileSystem *filesystem_create(Disk *disk);
extern FileSystem *filesystem_load(Disk *disk);
extern void filesystem_free(FileSystem *fs);

// allocates storage as close as possible in size to the size requested and returns it
struct ChunkBuffer {
	Size start_idx;
	Size length;
};
typedef struct ChunkBuffer ChunkBuffer;

extern ChunkBuffer filesystem_alloc_buffer(FileSystem *fs, Size space_needed);
extern void filesystem_free_buffer(FileSystem *fs, ChunkBuffer alloc);


#ifdef __cplusplus
}
#endif

#endif