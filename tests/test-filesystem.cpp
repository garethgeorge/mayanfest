#include <iostream>

#include "catch.hpp"

#include "diskinterface.hpp"
#include "filesystem.hpp"

TEST_CASE( "Making a filesystem should work", "[filesystem]" ) {
	Disk * disk = new Disk(512, 16);
    FileSystem * fs = new FileSystem(disk);
    fs->init();
    assert(false);
}