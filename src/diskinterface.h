#ifndef DISKINTERFACE_H
#define DISKINTERFACE_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef uint8_t Byte;

struct Disk {
	size_t chunk_count;
	size_t chunk_size;
	Byte *data;
};

typedef struct Disk Disk;

extern Disk *disk_create(size_t chunk_count, size_t chunk_size);
extern void disk_free(Disk *disk);
extern size_t disk_get_size_bytes(Disk *disk); // return size of disk in bytes
extern int disk_read_chunk(Disk *disk, size_t block_idx, Byte *outbuf);
extern int disk_write_chunk(Disk *disk, size_t block_idx, Byte *data);

#ifdef __cplusplus
}
#endif

#endif