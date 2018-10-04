#include "catch.hpp"
#include "segment.h"

#define DISK_SIZE 128
#define CHUNK_SIZE 128
#define SEGMENT_SIZE_CHUNKS 64

TEST_CASE( "Test the methods of the segment abstraction", "[diskinterface]" ) {
	Disk *disk = disk_create(DISK_SIZE, CHUNK_SIZE);
	Segment *seg = segment_create(disk, 0, SEGMENT_SIZE_CHUNKS); 

	Byte *format_chunk_as = (Byte *)malloc(SEGMENT_SIZE_CHUNKS * CHUNK_SIZE);
	uint32_t i;
	for (i = 0; i < SEGMENT_SIZE_CHUNKS * CHUNK_SIZE; ++i) {
		*(format_chunk_as + i) = (Byte)(i % 256);
	}

	SECTION( "Check segment write works") {
		segment_write_bytes(seg, format_chunk_as, SEGMENT_SIZE_CHUNKS * CHUNK_SIZE);

		REQUIRE(memcmp(disk->data, format_chunk_as, SEGMENT_SIZE_CHUNKS * CHUNK_SIZE) == -1);

		segment_flush_to_disk(seg);

		REQUIRE(memcmp(disk->data, format_chunk_as, SEGMENT_SIZE_CHUNKS * CHUNK_SIZE) == 0);
	}

	SECTION( "Check that segment read works" ) {
		Byte *read_data = (Byte *)malloc(SEGMENT_SIZE_CHUNKS * CHUNK_SIZE);
		
		segment_read_bytes(seg, 0, SEGMENT_SIZE_CHUNKS * CHUNK_SIZE, read_data);

		REQUIRE(memcmp(read_data, format_chunk_as, SEGMENT_SIZE_CHUNKS * CHUNK_SIZE) == 0);

		free(read_data);
	}

	free(format_chunk_as);
	segment_free(seg);
	disk_free(disk);
}