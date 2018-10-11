#include "catch.hpp"	

extern "C" {
	#include "filesystem.h"
	#include "diskinterface.h"
}

TEST_CASE( "Test the methods of the SuperBlock", "[diskinterface]" ) {
	Disk *disk = disk_create(256, 8);
	SuperBlock superblock;
	superblock_create(disk, &superblock);
	superblock_flush(&superblock);
	superblock_free(&superblock);
	superblock_load(disk, &superblock);
	superblock_free(&superblock);
}

TEST_CASE("Test the methods of the filesystem", "[diskinterface]") {
	Disk *disk = disk_create(256, 8);
	FileSystem *fs = filesystem_create(disk);

	SECTION( "It should be possible to allocate a LARGE segment of memory from the disk") {
		Size alloc_size = fs->superblock.storage_size / fs->disk->chunk_size;
		ChunkBuffer alloc = filesystem_alloc_buffer(
			fs, alloc_size);
		REQUIRE(alloc.length == alloc_size);
		// not enough space to service this allocation
		ChunkBuffer allocBad = filesystem_alloc_buffer(
			fs, alloc_size);
		REQUIRE(alloc.length == 0);
	}
}