#include "catch.hpp"	

extern "C" {
	#include "filesystem.h"
	#include "diskinterface.h"
}

TEST_CASE( "Test the methods of the SuperBlock", "[diskinterface]" ) {
	Disk *disk = disk_create(16, 16);
	SuperBlock superblock;
	superblock_create(disk, &superblock);
	superblock_flush(&superblock);
}