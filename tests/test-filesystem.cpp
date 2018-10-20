#include <iostream>

#include "catch.hpp"

#include "diskinterface.hpp"
#include "filesystem.hpp"

TEST_CASE( "Making a filesystem should work", "[filesystem]" ) {
    constexpr uint64_t CHUNK_COUNT = 4096;
    constexpr uint64_t CHUNK_SIZE = 4096;

    std::unique_ptr<Disk> disk(new Disk(CHUNK_COUNT, CHUNK_SIZE));
    std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
    fs->superblock->init(0.1);
}