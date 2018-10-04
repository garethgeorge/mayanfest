#include "catch.hpp"

#include "diskinterface.h"

TEST_CASE( "Test the methods of the disk interface", "[diskinterface]" ) {
	Disk *disk = disk_create(16, 16);

	uint32_t i;
	Byte buf[16];
	for (i = 0; i < 4; ++i) {
		memcpy(buf + i * 4, &i, sizeof(uint32_t));
	}

	SECTION( "Disk can get the size in bytes") {
		REQUIRE(disk_get_size_bytes(disk) == 16 * 16);
	}

	SECTION( "Disk should be initalized as all zero's") {
		uint32_t i;
		Byte buf2[16];
		memset(buf2, 0, 16);
		for (i = 0; i < 16; ++i) {
			disk_read_chunk(disk, i, buf);
			REQUIRE(memcmp(buf, buf2, 16) == 0);
		}
	}

	SECTION( "Write something out to the memory we have allocated ") {
		uint32_t i;
		for (i = 0; i < 16; ++i) {
			// NOTE: all of our functions are required to return 0 on success
			REQUIRE(disk_write_chunk(disk, i, buf) == 0);
		}
	}

	SECTION( "Read something back from memory, confirm it is what we wrote in") {
		uint32_t i;
		for (i = 0; i < 16; ++i) {
			// NOTE: all of our functions are required to return 0 on success
			REQUIRE(disk_write_chunk(disk, i, buf) == 0);
		}

		Byte buf2[16];
		for (i = 0; i < 16; ++i) {
			disk_read_chunk(disk, i, buf2);
			REQUIRE(memcmp(buf, buf2, 16) == 0);
		}
	}

	SECTION( "Should be able to free a disk") {
		disk = disk_create(16, 16);
		disk_free(disk);
	}
}
