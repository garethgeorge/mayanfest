#ifndef SEGMENT_H
#define SEGMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct Segment {
	Disk *disk;

	size_t index; // the segment index
	size_t size_in_chunks; // size of the segment in chunks
	size_t size_in_bytes;
	size_t bytes_used;

	Byte *data;
};

typedef struct Segment Segment;

extern Segment *segment_create(Disk *disk, size_t segment_idx, size_t segment_size);
extern int segment_free(Segment *segment);
extern int segment_flush_to_disk(Segment *segment);
extern int segment_get_start_chunk(size_t segment_idx, size_t segment_size);
extern void segment_write_bytes(Segment *seg, Byte *data, size_t data_len);
extern void segment_write_bytes(Segment *seg, size_t offset, size_t length, Byte *outdata);

#ifdef __cplusplus
}
#endif

#endif 