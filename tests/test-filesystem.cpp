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

TEST_CASE( "Inodes should work", "[inodes]" ) {
	constexpr uint64_t CHUNK_COUNT = 4096;
	constexpr uint64_t CHUNK_SIZE = 4096;

	std::unique_ptr<Disk> disk(new Disk(CHUNK_COUNT, CHUNK_SIZE));
	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
	fs->superblock->init(0.1);

	SECTION("Inodes can be initialized and stored in INode table"){
		INode inode;
		fs->superblock->inode_table->set_inode(0, inode);
	}

	SECTION("Inodes can be initialized, stored, and retrieved"){
		INode inode;
		inode.data.UID = 123;
		fs->superblock->inode_table->set_inode(1, inode);
		INode inode2 = fs->superblock->inode_table->get_inode(1);
		REQUIRE(inode2.data.UID == 123);
	}

	SECTION("Inodes can be initialized, stored, and retrieved"){
		try{
			INode inode;
			inode.superblock = fs->superblock.get();
			char str[] = "hello there!";
			inode.write(0, str, sizeof(str));
		}catch(const FileSystemException &e){
			std::cout<< e.message << std::endl;
		}
	}
}

