#include <string.h>

#include "diskinterface.h"

Disk *disk_create(size_t chunk_count, size_t chunk_size) {
	Disk *disk = (Disk *)malloc(sizeof(Disk));
	disk->chunk_count = chunk_count;
	disk->chunk_size = chunk_size;
	disk->data = (Byte *)malloc(sizeof(Byte) * chunk_size * chunk_count);
	memset((void *)disk->data, 0, sizeof(Byte) * chunk_size * chunk_count);
	return disk;
}

void disk_free(Disk *disk) {
	free((void *)(disk->data));
	free((void *)disk);
}

size_t disk_get_size_bytes(Disk *disk) {
	return disk->chunk_count * disk->chunk_size;
}

int disk_read_chunk(Disk *disk, size_t block_idx, Byte *outbuf) {
	if (block_idx >= disk->chunk_size)
		return -1;
	memcpy((void *)outbuf, (void *)(disk->data + disk->chunk_size * block_idx), disk->chunk_size);
	return 0;
}

int disk_write_chunk(Disk *disk, size_t block_idx, Byte *data) {
	if (block_idx >= disk->chunk_size)
		return -1;
	memcpy((void *)(disk->data + disk->chunk_size * block_idx), (void *)(data), disk->chunk_size);
	return 0;
}