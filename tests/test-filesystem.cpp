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

TEST_CASE( "INodes read/write should work on a small disk with reasonably sized writes", "[inodes]" ) {
	constexpr uint64_t CHUNK_COUNT = 1024;
	constexpr uint64_t CHUNK_SIZE = 1024;

	std::unique_ptr<Disk> disk(new Disk(CHUNK_COUNT, CHUNK_SIZE));
	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
	fs->superblock->init(0.1);

	SECTION("INodes can be initialized and stored in INode table"){
		INode inode;
		fs->superblock->inode_table->set_inode(0, inode);
	}

	SECTION("INodes can be initialized, stored, and retrieved"){
		INode inode;
		inode.data.UID = 123;
		fs->superblock->inode_table->set_inode(1, inode);
		INode inode2 = fs->superblock->inode_table->get_inode(1);
		REQUIRE(inode2.data.UID == 123);
	}

	SECTION("INodes can be written/read short strings") {
		try{
			INode inode;
			REQUIRE(fs->superblock->disk == disk.get());
			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());
			
			char str[] = "hello there!";
			REQUIRE(inode.write(0, str, sizeof(str)) == sizeof(str));

			char buf[sizeof(str)];
			REQUIRE(inode.read(0, buf, sizeof(str)) == sizeof(str));
			REQUIRE(strcmp(buf, str) == 0);
		}catch(const FileSystemException &e){
			std::cout<< e.message << std::endl;
		}
	}

	SECTION("INodes can be written short strings at large offsets (a few pages)"){
		try{
			std::cout << std::endl << std::endl;
			INode inode;
			REQUIRE(fs->superblock->disk == disk.get());
			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());
			
			char str[] = "hello there!";
			REQUIRE(inode.write(10 * 1024, str, sizeof(str)) == sizeof(str));

			char buf[sizeof(str)];
			REQUIRE(inode.read(10 * 1024, buf, sizeof(str)) == sizeof(str));
			REQUIRE(strcmp(buf, str) == 0);
		}catch(const FileSystemException &e){
			std::cout<< e.message << std::endl;
		}catch(const DiskException &e) {
			std::cout<< e.message << std::endl;
		}
	}

	SECTION("INodes can be written short strings at large offsets (a few pages)"){
		try{
			std::cout << std::endl << std::endl;
			INode inode;
			REQUIRE(fs->superblock->disk == disk.get());
			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());
			
			char str[] = "hello there!";
			REQUIRE(inode.write(10 * 1024, str, sizeof(str)) == sizeof(str));

			char buf[sizeof(str)];
			REQUIRE(inode.read(10 * 1024, buf, sizeof(str)) == sizeof(str));
			REQUIRE(strcmp(buf, str) == 0);
		}catch(const FileSystemException &e){
			std::cout<< e.message << std::endl;
		}catch(const DiskException &e) {
			std::cout<< e.message << std::endl;
		}
	}

	SECTION("INodes can be written short strings at VERY LARGE offsets (a few pages)"){
		try{
			std::cout << std::endl << std::endl;
			INode inode;
			REQUIRE(fs->superblock->disk == disk.get());
			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());
			
			char str[] = "hello there!";
			REQUIRE(inode.write(10 * 1024 * 1024, str, sizeof(str)) == sizeof(str));

			char buf[sizeof(str)];
			REQUIRE(inode.read(10 * 1024 * 1024, buf, sizeof(str)) == sizeof(str));
			REQUIRE(strcmp(buf, str) == 0);
		}catch(const FileSystemException &e){
			std::cout<< e.message << std::endl;
		}catch(const DiskException &e) {
			std::cout<< e.message << std::endl;
		}
	}

	SECTION("Can write a 10KB file"){
		try{
			std::cout << std::endl << std::endl;
			INode inode;
			REQUIRE(fs->superblock->disk == disk.get());
			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());
			
			char str[] = "hello there!hello there!hello there!hello there!hello there!hello there!hello there!hello there!hello there!hello there!hello there!hello there!hello there!";
			for (size_t i = 0; i < 10 * 1024; i += sizeof(str)) {
				REQUIRE(inode.write(i, str, sizeof(str)) == sizeof(str));	
			}
		}catch(const FileSystemException &e){
			std::cout<< e.message << std::endl;
		}catch(const DiskException &e) {
			std::cout<< e.message << std::endl;
		}
	}
}
