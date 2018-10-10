#include <string.h>

#include "segment.h"

Segment* segment_create(Disk *disk, Size segment_idx, Size segment_size) {
	Segment *seg = (Segment *)malloc(sizeof(Segment));
	seg->disk = disk;
	seg->index = segment_idx;
	seg->size_in_chunks = segment_size;
	seg->size_in_bytes = segment_size * disk->chunk_size;
	seg->bytes_used = 0;
	seg->data = (Byte *)malloc(segment_size * disk->chunk_size);

	Size start_idx = segment_get_start_chunk(segment_idx, segment_size);
	Size chunk_idx = 0;
	for (; chunk_idx < segment_size; ++chunk_idx) {
		int errno = disk_read_chunk(disk, 
			chunk_idx + start_idx, 
			seg->data + chunk_idx * disk->chunk_size);
		if (errno < 0)
			return NULL;
	}
	
	return seg;
}

int segment_free(Segment *seg) {
	if (segment_flush_to_disk(seg) < 0)
		return -1;
	free((void *)seg->data);
	free((void *)seg);
	return 0;
}

int segment_flush_to_disk(Segment *seg) {
	const Disk *disk = seg->disk;
	Size start_idx = segment_get_start_chunk(seg->index, seg->size_in_chunks);
	Size chunk_idx = 0;
	for (; chunk_idx < seg->size_in_chunks; chunk_idx++) {
		int errno = disk_write_chunk(seg->disk, 
			chunk_idx + start_idx,
			seg->data + chunk_idx * disk->chunk_size);
		if (errno < 0)
			return errno;
	}
	return 0;
}

int segment_get_start_chunk(Size segment_idx, Size segment_size) {
	return segment_idx * segment_size;
}

void segment_write_bytes(Segment *seg, Byte *data, Size data_len) {
	memcpy((void *)(seg->data + seg->bytes_used), (void *)data, data_len);
	seg->bytes_used += data_len;
}
void segment_read_bytes(Segment *seg, Size offset, Size length, Byte *outdata) {
	memcpy(outdata, seg->data + offset, length);
}



