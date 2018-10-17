#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"

Disk *myDisk = NULL;

void serializeSize(Size value, Byte *outbuf) {
	Size i;
	for (i = 0; i < 8; ++i) {
		*outbuf = value & 0xFF;
		value >>= 8;
		i++;
		outbuf++;
	}
}

extern Size deserializeSize(Byte *inbuf) {
	Size i;
	Size value;
	for (i = 0; i < 8; ++i) {
		value <<= 8;
		value |= *inbuf & 0xFF;
		i++;
	}
	return value;
}

void superblock_create(Disk *disk, SuperBlock *superblock) {
	memset((void *)superblock, 0, sizeof(SuperBlock));
	superblock->disk = disk;

	// precompute the size of the whole superblock
	Size properties[4];
	superblock->chunk_usage_bitmap_size = 
		disk->chunk_count / 8 + (disk->chunk_count % 8 == 0 ? 0 : 1);

	superblock->superblock_size =  sizeof(properties) + superblock->chunk_usage_bitmap_size;
	superblock->superblock_size_chunks = superblock->superblock_size / disk->chunk_size + 1;
	superblock->storage_offset = superblock->superblock_size / disk->chunk_size + 1;
	superblock->storage_size = disk->chunk_count - superblock->storage_offset;

	// malloc the buffer into which the superblock is located 
	superblock->superblock_buffer = (Byte *)malloc(superblock->superblock_size);
	memset(superblock->superblock_buffer, 0, superblock->superblock_size);

	// now set the properties of the superblock to their defaults
	superblock->chunk_usage_bitmap_offset = sizeof(properties);

	memset(superblock->superblock_buffer, 0, superblock->superblock_size);
	properties[0] = superblock->superblock_size;
	properties[1] = superblock->chunk_usage_bitmap_offset;
	properties[2] = superblock->chunk_usage_bitmap_size;
	properties[3] = superblock->storage_offset;

	// finally flush this out to the buffer so it matches the values in sizes
	Size superblock_buffer_idx = 0;
	Size props_idx;
	for (props_idx = 0; props_idx < sizeof(properties) / sizeof(Size); ++props_idx) {
		serializeSize(properties[props_idx], superblock->superblock_buffer + superblock_buffer_idx);
		superblock_buffer_idx += sizeof(Size);
	}

	// NOTE: it is IMPERATIVE THAT THIS BE WRITTEN IMMEDIATELY AFTER PROPERTIES
	assert(superblock->chunk_usage_bitmap_offset == superblock_buffer_idx);
	superblock->chunk_usage_bitmap_offset = superblock_buffer_idx;
	superblock->chunk_usage_bitmap = superblock->superblock_buffer + superblock->chunk_usage_bitmap_offset;
	superblock_buffer_idx += superblock->chunk_usage_bitmap_size;
}

void superblock_load(Disk *disk, SuperBlock *superblock) {
	memset((void *)superblock, 0, sizeof(SuperBlock));
	superblock->disk = disk;

	// read in the properties
	Size properties[4];

	Byte *firstchunk = (Byte *)malloc(disk->chunk_size);
	disk_read_chunk(disk, 0, firstchunk);
	Size props_idx;
	for (props_idx = 0; props_idx < sizeof(properties) / sizeof(Size); ++props_idx) {
		serializeSize(properties[props_idx], firstchunk);
		firstchunk += sizeof(Size);
	}
	
	superblock->superblock_size = properties[0];
	superblock->superblock_size_chunks = superblock->superblock_size / disk->chunk_size + 1;
	superblock->chunk_usage_bitmap_offset = properties[1];
	superblock->chunk_usage_bitmap_size = properties[2];
	superblock->storage_offset = properties[3];
	superblock->storage_size = disk->chunk_count - superblock->storage_offset;

	// read in the buffer 
	superblock->superblock_buffer = (Byte *)malloc(superblock->superblock_size_chunks * disk->chunk_size);
	memset(superblock->superblock_buffer, 0, superblock->superblock_size_chunks * disk->chunk_size);
	Size chunk_idx = 0;
	for (; chunk_idx < superblock->superblock_size_chunks; ++chunk_idx) {
		disk_read_chunk(disk, chunk_idx, superblock->superblock_buffer + chunk_idx * disk->chunk_size);
	}
	
	superblock->chunk_usage_bitmap = superblock->superblock_buffer + superblock->chunk_usage_bitmap_offset;
}

void superblock_flush(SuperBlock *superblock) {
	Disk *disk = superblock->disk;
	Size chunk_idx = 0;
	for (; chunk_idx < superblock->superblock_size_chunks; ++chunk_idx) {
		disk_write_chunk(disk, chunk_idx, superblock->superblock_buffer + chunk_idx * disk->chunk_size);
	}
}

void superblock_free(SuperBlock *superblock) {
	free(superblock->superblock_buffer);
}

FileSystem *filesystem_create(Disk *disk) {
	FileSystem *fs = (FileSystem *)malloc(sizeof(FileSystem));
	fs->disk = disk;
	superblock_create(disk, &(fs->superblock));
	return fs;
}

FileSystem *filesystem_load(Disk *disk) {
	FileSystem *fs = (FileSystem *)malloc(sizeof(FileSystem));
	fs->disk = disk;
	superblock_load(disk, &(fs->superblock));
	return fs;
}

void filesystem_free(FileSystem *fs) {
	superblock_free(&(fs->superblock));
	free(fs);
}

ChunkBuffer filesystem_alloc_buffer(FileSystem *fs, Size space_needed) {
	Size idx;
	for (idx = fs->superblock.storage_offset; idx < fs->disk->chunk_count; ++idx) {
		fprintf(stdout, "CHECK BIT %d: %d\n", idx, BITMAP_GET(fs->superblock.chunk_usage_bitmap, idx));
		if (BITMAP_GET(fs->superblock.chunk_usage_bitmap, idx) == 0) {
			ChunkBuffer allocation;
			allocation.start_idx = idx;
			allocation.length = 1;

			// TODO: make this work correctly
			while (idx < fs->disk->chunk_count && 
				   allocation.length < space_needed && 
				   BITMAP_GET(fs->superblock.chunk_usage_bitmap, idx) == 0) {
				BITMAP_SET(fs->superblock.chunk_usage_bitmap, idx);
				fprintf(stdout, "BITMAP GET GET GET %d: %d\n", idx, BITMAP_GET(fs->superblock.chunk_usage_bitmap, idx));
				allocation.length++;
				idx++;
			}
			return allocation;
		}
	}
	
	ChunkBuffer allocation;
	allocation.start_idx = 0;
	allocation.length = 0;
	return allocation;
}


void filesystem_free_buffer(FileSystem *fs, ChunkBuffer alloc) {
	Size idx;
	const Size end_idx = alloc.start_idx + alloc.length;
	for (idx = alloc.start_idx; idx < end_idx; ++idx) {
#ifdef DEBUG 
		assert(BITMAP_GET(fs->superblock.chunk_usage_bitmap, idx));
#endif
		BITMAP_CLEAR(fs->superblock.chunk_usage_bitmap, idx);
	}
}
