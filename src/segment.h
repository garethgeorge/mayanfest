#ifndef SEGMENT_H
#define SEGMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "diskinterface.h"

struct Segment {
	Disk *disk;

	Size index; // the segment index
	Size size_in_chunks; // size of the segment in chunks
	Size size_in_bytes;
	Size bytes_used;

	Byte *data;
};

typedef struct Segment Segment;

#define SEGMENT_MANAGER_CACHE_SIZE 32
struct SegmentManager {
	Disk *disk;
	Size segment_size_chunks; // segment size in chunks
	Size cur_seg_idx;
	// TODO: implement a segment cache, perhaps impor RB tree class
	Segment *segments[SEGMENT_MANAGER_CACHE_SIZE];
};

extern Segment *segment_create(Disk *disk, Size segment_idx, Size segment_size);
extern int segment_free(Segment *segment);
extern int segment_flush_to_disk(Segment *segment);
extern int segment_get_start_chunk(Size segment_idx, Size segment_size);
extern void segment_write_bytes(Segment *seg, Byte *data, Size data_len);
extern void segment_read_bytes(Segment *seg, Size offset, Size length, Byte *outdata);

#ifdef __cplusplus
}
#endif

#endif 
