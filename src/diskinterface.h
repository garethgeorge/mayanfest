#ifndef DISKINTERFACE_H
#define DISKINTERFACE_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef uint8_t Byte;
typedef uint64_t Size;

struct Disk {
	Size chunk_count;
	Size chunk_size;
	Byte *data;
};

typedef struct Disk Disk;

extern Disk *disk_create(Size chunk_count, Size chunk_size);
extern void disk_free(Disk *disk);
extern Size disk_get_size_bytes(Disk *disk); // return size of disk in bytes
extern int disk_read_chunk(Disk *disk, Size block_idx, Byte *outdata);
extern int disk_write_chunk(Disk *disk, Size block_idx, Byte *data);

#ifdef __cplusplus
}
#endif

#endif