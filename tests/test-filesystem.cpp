#include <iostream>

#include "catch.hpp"

#include "diskinterface.hpp"
#include "filesystem.hpp"

TEST_CASE( "Making a filesystem should work", "[filesystem]" ) {
    constexpr uint64_t CHUNK_COUNT = 4096;
    constexpr uint64_t CHUNK_SIZE = 4096;
	Disk * disk = new Disk(CHUNK_COUNT, CHUNK_SIZE);
    FileSystem * fs = new FileSystem(disk);
    fs->init();
}