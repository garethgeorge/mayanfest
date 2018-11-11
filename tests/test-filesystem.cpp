#include <iostream>
#include <cstdlib>
#include <ctime>

#include "catch.hpp"

#include "diskinterface.hpp"
#include "filesystem.hpp"

const auto get_random_buffer = [](size_t size, bool nullTerminate = false) -> std::vector<char> {
	std::vector<char> buf;
	buf.resize(size + (nullTerminate ? 1 : 0));
	for (size_t i = 0; i < size;) {
		size_t j = 0;
		size_t r = rand();
		while (j < 6 && i < size) {
			buf[i] = ('a' + r % 26);
			r /= 26;

			j++;
			i++;
		}
	}
	if (nullTerminate) {
		buf[size] = '\0';
	}
	return std::move(buf);
};


TEST_CASE( "Making a filesystem should work", "[filesystem]" ) {
	constexpr uint64_t CHUNK_COUNT = 4096;
	constexpr uint64_t CHUNK_SIZE = 4096;

	std::unique_ptr<Disk> disk(new Disk(CHUNK_COUNT, CHUNK_SIZE));
	{
		std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
		fs->superblock->init(0.1);
		fs = nullptr;
	}

	{
		std::cout << "Load the filesystem from the disk" << std::endl;
		std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
		fs->superblock->load_from_disk(disk.get());
		fs = nullptr;
	}
}

TEST_CASE("INode read/write test", "[filesystem][readwrite][readwrite.orderly]") {
	const auto test_inode = [](int offset, int length) {
		std::cout << "START TESTINODE" << std::endl;
		std::unique_ptr<Disk> disk(new Disk(1024, 512));
		std::vector<char> read_back;

		std::vector<char> to_write = get_random_buffer(length);
		int64_t inode_idx = 0;

		to_write.push_back('\0');
		{
			std::cout << "CHECK POINT 1" << std::endl;
			std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
			fs->superblock->init(0.1);

			read_back.resize(to_write.size());
			
			INode inode = fs->superblock->inode_table->alloc_inode();
			inode_idx = inode.inode_table_idx;

			std::cout << "CHECK POINT 2" << std::endl;

			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());

			REQUIRE(inode.write(offset, &(to_write[0]), length) == length);
			if (inode.data.file_size < offset + length) {
				inode.data.file_size = offset + length;
			}
			std::cout << "CHECK POINT 3" << std::endl;
			fs->superblock->inode_table->set_inode(inode_idx, inode); // store back the inode
			
			REQUIRE(inode.read(offset, &(read_back[0]), length) == length);

			REQUIRE(strcmp(&to_write[0], &read_back[0]) == 0);
			fs = nullptr;
		}

		std::cout << "MIDWAY THROUGH TEST INODE" << std::endl;
		
		{
			std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
			fs->superblock->load_from_disk(disk.get());
			std::shared_ptr<Chunk> page0 = disk->get_chunk(0);

			std::vector<char> read_back1;
			read_back1.resize(to_write.size());

			INode inode = fs->superblock->inode_table->get_inode(inode_idx);
			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());
			
			REQUIRE(inode.read(offset, &(read_back1[0]), length) == length);
			REQUIRE(strcmp(&to_write[0], &read_back1[0]) == 0);

			fs = nullptr;
		}

		std::cout << "STOP TEST INODE" << std::endl;
	};

	SECTION("Can write strings of length 1 - 10000") {
		for (int i = 0; i < 10000; ++i) {
			test_inode(0, i);
		}
	}

	SECTION("Can write strings of length 100 at offsets 1 - 10000") {
		for (int i = 0; i < 10000; ++i) {
			test_inode(i, 100);
		}
	}

	SECTION("Can write strings of length 1000 at offsets 1 - 10000") {
		for (int i = 0; i < 10000; ++i) {
			test_inode(i, 1000);
		}
	}

	SECTION("Can write strings of length 2000 at offsets 1 - 10000") {
		for (int i = 0; i < 10000; ++i) {
			test_inode(i, 2000);
		}
	}
}

TEST_CASE("INode read/write test with random patterns", "[filesystem][readwrite][readwrite.random]") {
	const auto test_inode = [](INode& inode, int offset, int length) {
		std::vector<char> to_write;

		for (int i = 0; i < length; ++i) {
			to_write.push_back('a' + (rand() % 26));
		}
		to_write.push_back('\0');

		std::vector<char> read_back;
		read_back.resize(to_write.size());

		REQUIRE(inode.write(offset, &(to_write[0]), length) == length);
		REQUIRE(inode.read(offset, &(read_back[0]), length) == length);
		
		REQUIRE(strcmp(&to_write[0], &read_back[0]) == 0);
	};

	SECTION("An aggressively random test ;) -- I'm a firin mah lazors") {
		std::unique_ptr<Disk> disk(new Disk(100 * 1024, 512));
		std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
		fs->superblock->init(0.1);
		
		uint64_t seed = std::time(0);
		fprintf(stdout, "SRAND SEED WAS 0x%x\n", seed);
		srand(seed);

		int64_t bytes_to_write = (uint64_t) (disk->size_bytes() * 0.8); // good margin to write

		while (bytes_to_write > 0) {
			INode inode;
			inode.superblock = fs->superblock.get();
			REQUIRE(inode.superblock->disk == disk.get());

			for (int j = rand() % 100; j > 0; --j) { // write up to 8 segments to the same inode
				uint64_t bytes = rand() % 5000;
				uint64_t offset = rand() % 25000;
				test_inode(inode, offset, bytes);
				bytes_to_write -= (bytes / disk->chunk_size() + 1) * disk->chunk_size();
				//std::cout << "wrote " << bytes << " bytes at offset " << offset << std::endl;
			}
		}
	}
}



TEST_CASE("INode write all, then readback all, reconstruct disk, and then do it again!!!", "[filesyste][readwrite][readwrite.rwrecon]") {
	std::unique_ptr<Disk> disk(new Disk(10 * 1024, 512));
	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
	fs->superblock->init(0.1);

	const size_t FILE_SIZE = 250 * 1024; // 100 kb
	INode inode = fs->superblock->inode_table->alloc_inode();
	assert(inode.superblock == fs->superblock.get());

	std::unique_ptr<char[]> mem_file(new char[FILE_SIZE]);
	std::unique_ptr<char[]> mem_file_readback(new char[FILE_SIZE]);
	std::memset((void *)mem_file.get(), 0, FILE_SIZE);
	std::memset((void *)mem_file_readback.get(), 0, FILE_SIZE);

	for (size_t idx = 0; idx < 20000; ++idx) {
		size_t size = rand() % (16 * 1024);
		size_t offset = rand() % (FILE_SIZE - size);
		// std::cout << "writing size: " << size << " bytes at offset: " << offset << std::endl;
		std::vector<char> buffer = get_random_buffer(size);
		REQUIRE(inode.write(offset, &(buffer[0]), size) == size);
		std::memcpy((void *)(mem_file.get() + offset), &(buffer[0]), size);
	}

	// NOTE: YOU MUST WRITE INODES BACK OUT WHEN YOU ARE DONE WITH THEM 
	inode.data.file_size = FILE_SIZE;
	fs->superblock->inode_table->set_inode(inode.inode_table_idx, inode); 

	REQUIRE(inode.read(0, mem_file_readback.get(), FILE_SIZE) == FILE_SIZE);
	REQUIRE(std::memcmp(mem_file.get(), mem_file_readback.get(), FILE_SIZE) == 0);
}

TEST_CASE("Smaller version of INode write all, then readback all, reconstruct disk, and then do it again!!! but using a very high base offset", "[filesyste][readwrite][readwrite.rwrecon]") {
	const auto get_random_buffer = [](size_t size, bool nullTerminate = false) -> std::vector<char> {
		std::vector<char> buf;
		buf.reserve(size + 1);
		for (size_t i = 0; i < size; ++i) {
			buf.push_back('a' + rand() % 26);
		}
		buf.push_back('\0');
		return std::move(buf);
	};

	std::unique_ptr<Disk> disk(new Disk(10 * 1024, 512));
	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
	fs->superblock->init(0.1);

	const size_t BASE_OFFSET = 1024 * 1024;
	const size_t FILE_SIZE = 25 * 1024; // 100 kb
	INode inode = fs->superblock->inode_table->alloc_inode();
	assert(inode.superblock == fs->superblock.get());

	std::unique_ptr<char[]> mem_file(new char[FILE_SIZE]);
	std::unique_ptr<char[]> mem_file_readback(new char[FILE_SIZE]);
	std::memset((void *)mem_file.get(), 0, FILE_SIZE);
	std::memset((void *)mem_file_readback.get(), 0, FILE_SIZE);

	for (size_t idx = 0; idx < 1000; ++idx) {
		size_t size = rand() % (1 * 1024);
		size_t offset = rand() % (FILE_SIZE - size) + BASE_OFFSET;
		// std::cout << "writing size: " << size << " bytes at offset: " << offset << std::endl;
		std::vector<char> buffer = get_random_buffer(size);
		REQUIRE(inode.write(offset, &(buffer[0]), size) == size);
		std::memcpy((void *)(mem_file.get() + offset - BASE_OFFSET), &(buffer[0]), size);
	}


	// NOTE: YOU MUST WRITE INODES BACK OUT WHEN YOU ARE DONE WITH THEM 
	inode.data.file_size = FILE_SIZE;
	fs->superblock->inode_table->set_inode(inode.inode_table_idx, inode); 

	REQUIRE(inode.read(BASE_OFFSET, mem_file_readback.get(), FILE_SIZE) == FILE_SIZE);
	REQUIRE(std::memcmp(mem_file.get(), mem_file_readback.get(), FILE_SIZE) == 0);
}

/*
	TODO: test behavior with random disk sizes
*/

TEST_CASE("INode write overlapping strings", "[INode]"){
 	constexpr uint64_t CHUNK_COUNT = 1024;
 	constexpr uint64_t CHUNK_SIZE = 1024;

 	std::unique_ptr<Disk> disk(new Disk(CHUNK_COUNT, CHUNK_SIZE));
 	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
 	fs->superblock->init(0.1);

	SECTION("INodes can be written with overlapping strings and read back"){
		INode inode = fs->superblock->inode_table->alloc_inode();
		REQUIRE(fs->superblock->disk == disk.get());
		inode.superblock = fs->superblock.get();
		REQUIRE(inode.superblock->disk == disk.get());
		
		char str1[] = "ab";
		char str2[] = "cd";
		char buf[4];
		std::memset(buf, 0, 4);
		inode.write(0, str1, sizeof(str1) - 1);
		inode.write(1, str2, sizeof(str2) - 1);

		fs->superblock->inode_table->set_inode(0, inode);
		inode = fs->superblock->inode_table->get_inode(0);

		inode.read(0, buf, 3);
		REQUIRE(strcmp(buf, "acd") == 0);
	}

	SECTION("INodes can be re-written with overlapping strings and read back"){
		INode inode = fs->superblock->inode_table->alloc_inode();
		REQUIRE(fs->superblock->disk == disk.get());
		inode.superblock = fs->superblock.get();
		REQUIRE(inode.superblock->disk == disk.get());
		
		char str1[] = "ab";
		char str2[] = "cd";
		char buf[4];
		std::memset(buf, 0, 4);
		inode.write(0, str1, sizeof(str1) - 1);
		inode.write(1, str2, sizeof(str2) - 1);
		//rewrite
		inode.write(0, str1, sizeof(str1) - 1);
		inode.write(1, str2, sizeof(str2) - 1);

		fs->superblock->inode_table->set_inode(0, inode);
		inode = fs->superblock->inode_table->get_inode(0);

		inode.read(0, buf, 3);
		REQUIRE(strcmp(buf, "acd") == 0);
	}

	SECTION("INodes can be written with overlapping strings beginning at random offsets and read back"){
		INode inode = fs->superblock->inode_table->alloc_inode();
		REQUIRE(fs->superblock->disk == disk.get());
		inode.superblock = fs->superblock.get();
		REQUIRE(inode.superblock->disk == disk.get());
		
		char str1[] = "abcd";
		char str2[] = "efgh";
		char buf[6];
		std::memset(buf, 0, 6);
		inode.write(1022, str1, sizeof(str1) - 1);
		inode.write(1023, str2, sizeof(str2) - 1);

		fs->superblock->inode_table->set_inode(0, inode);
		inode = fs->superblock->inode_table->get_inode(0);

		inode.read(1022, buf, 5);
		REQUIRE(strcmp(buf, "aefgh") == 0);
	}

	SECTION("INodes can be re-written with overlapping strings beginning at random offsets and read back"){
		INode inode = fs->superblock->inode_table->alloc_inode();
		REQUIRE(fs->superblock->disk == disk.get());
		inode.superblock = fs->superblock.get();
		REQUIRE(inode.superblock->disk == disk.get());
		
		char str1[] = "abcd";
		char str2[] = "efgh";
		char buf[6];
		std::memset(buf, 0, 6);
		inode.write(1022, str1, sizeof(str1) - 1);
		inode.write(1023, str2, sizeof(str2) - 1);
		// rewrite
		inode.write(1022, str1, sizeof(str1) - 1);
		inode.write(1023, str2, sizeof(str2) - 1);

		fs->superblock->inode_table->set_inode(0, inode);
		inode = fs->superblock->inode_table->get_inode(0);

		inode.read(1022, buf, 5);
		REQUIRE(strcmp(buf, "aefgh") == 0);
	}
}

// TEST_CASE( "INodes read/write should work on a small disk with reasonably sized writes (NOTE: inodes not properly allocated, THIS DOES NOT TEST INODE TABLE)", "[inodes]" ) {
// 	constexpr uint64_t CHUNK_COUNT = 1024;
// 	constexpr uint64_t CHUNK_SIZE = 1024;

// 	std::unique_ptr<Disk> disk(new Disk(CHUNK_COUNT, CHUNK_SIZE));
// 	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
// 	fs->superblock->init(0.1);

// 	SECTION("INodes can be initialized and stored in INode table"){
// 		INode inode;
// 		fs->superblock->inode_table->set_inode(0, inode);
// 	}

// 	SECTION("INodes can be initialized, stored, and retrieved"){
// 		INode inode;
// 		inode.data.UID = 123;
// 		fs->superblock->inode_table->set_inode(1, inode);
// 		INode inode2 = fs->superblock->inode_table->get_inode(1);
// 		REQUIRE(inode2.data.UID == 123);
// 	}

// 	SECTION("INodes can be written/read short strings") {
// 		try{
// 			INode inode;
// 			REQUIRE(fs->superblock->disk == disk.get());
// 			inode.superblock = fs->superblock.get();
// 			REQUIRE(inode.superblock->disk == disk.get());
			
// 			char str[] = "hello there!";
// 			REQUIRE(inode.write(0, str, sizeof(str)) == sizeof(str));

// 			char buf[sizeof(str)];
// 			REQUIRE(inode.read(0, buf, sizeof(str)) == sizeof(str));
// 			REQUIRE(strcmp(buf, str) == 0);
// 		}catch(const FileSystemException &e){
// 			std::cout<< e.message << std::endl;
// 		}
// 	}

// 	SECTION("INodes can be written short strings at large offsets (a few pages)"){
// 		try{
// 			std::cout << std::endl << std::endl;
// 			INode inode;
// 			REQUIRE(fs->superblock->disk == disk.get());
// 			inode.superblock = fs->superblock.get();
// 			REQUIRE(inode.superblock->disk == disk.get());
			
// 			char str[] = "hello there!";
// 			REQUIRE(inode.write(10 * 1024, str, sizeof(str)) == sizeof(str));

// 			char buf[sizeof(str)];
// 			REQUIRE(inode.read(10 * 1024, buf, sizeof(str)) == sizeof(str));
// 			REQUIRE(strcmp(buf, str) == 0);
// 		}catch(const FileSystemException &e){
// 			std::cout<< e.message << std::endl;
// 		}catch(const DiskException &e) {
// 			std::cout<< e.message << std::endl;
// 		}
// 	}

// 	SECTION("INodes can be written short strings at large offsets (a few pages)"){
// 		try{
// 			std::cout << std::endl << std::endl;
// 			INode inode;
// 			REQUIRE(fs->superblock->disk == disk.get());
// 			inode.superblock = fs->superblock.get();
// 			REQUIRE(inode.superblock->disk == disk.get());
			
// 			char str[] = "hello there!";
// 			REQUIRE(inode.write(10 * 1024, str, sizeof(str)) == sizeof(str));

// 			char buf[sizeof(str)];
// 			REQUIRE(inode.read(10 * 1024, buf, sizeof(str)) == sizeof(str));
// 			REQUIRE(strcmp(buf, str) == 0);
// 		}catch(const FileSystemException &e){
// 			std::cout<< e.message << std::endl;
// 		}catch(const DiskException &e) {
// 			std::cout<< e.message << std::endl;
// 		}
// 	}

// 	SECTION("INodes can be written short strings at VERY LARGE offsets (a few hundred pages)"){
// 		std::cout << std::endl << std::endl;
// 		INode inode;
// 		REQUIRE(fs->superblock->disk == disk.get());
// 		inode.superblock = fs->superblock.get();
// 		REQUIRE(inode.superblock->disk == disk.get());
		
// 		char str[] = "hello there!";
// 		REQUIRE(inode.write(10 * 1024 * 1024, str, sizeof(str)) == sizeof(str));

// 		char buf[sizeof(str)];
// 		REQUIRE(inode.read(10 * 1024 * 1024, buf, sizeof(str)) == sizeof(str));
// 		REQUIRE(strcmp(buf, str) == 0);
// 	}

// }

// TEST_CASE( "INodes read/write should work on a large disk with big writes (NOTE: inodes not properly allocated, THIS DOES NOT TEST INODE TABLE)", "[inodes]" ) {
// 	constexpr uint64_t CHUNK_COUNT = 300 * 1024;
// 	constexpr uint64_t CHUNK_SIZE = 1024;

// 	std::unique_ptr<Disk> disk(new Disk(CHUNK_COUNT, CHUNK_SIZE));
// 	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
// 	fs->superblock->init(0.1);

// 	// SECTION("Can write a 10KB file & read it back successfully"){
// 	// 	std::cout << std::endl << std::endl;
// 	// 	INode inode;
// 	// 	REQUIRE(fs->superblock->disk == disk.get());
// 	// 	inode.superblock = fs->superblock.get();
// 	// 	REQUIRE(inode.superblock->disk == disk.get());
		
// 	// 	char str[] = "Blanditiis pariatur alias distinctio aut. "
// 	// 		"Occaecati et incidunt nesciunt dolorum est id excepturi. "
// 	// 		"Sunt quod sapiente consequatur voluptate rerum voluptatum "
// 	// 		"harum.";
// 	// 	for (size_t i = 0; i < 10 * 1024; i += sizeof(str)) {
// 	// 		REQUIRE(inode.write(i, str, sizeof(str)) == sizeof(str));
// 	// 	}

// 	// 	char buf[sizeof(str)];
		
// 	// 	for (size_t i = 0; i < 10 * 1024; i += sizeof(str)) {
// 	// 		REQUIRE(inode.read(i, buf, sizeof(str)) == sizeof(str));
// 	// 		REQUIRE(strcmp(buf, str) == 0);
// 	// 	}
// 	// }

// 	// SECTION("Can write a 100KB file & read it back successfully"){
// 	// 	std::cout << std::endl << std::endl;
// 	// 	INode inode;
// 	// 	REQUIRE(fs->superblock->disk == disk.get());
// 	// 	inode.superblock = fs->superblock.get();
// 	// 	REQUIRE(inode.superblock->disk == disk.get());
		
// 	// 	char str[] = "Blanditiis pariatur alias distinctio aut. "
// 	// 		"Occaecati et incidunt nesciunt dolorum est id excepturi. "
// 	// 		"Sunt quod sapiente consequatur voluptate rerum voluptatum "
// 	// 		"harum.";
// 	// 	for (size_t i = 0; i < 100 * 1024; i += sizeof(str)) {
// 	// 		if (inode.write(i, str, sizeof(str)) != sizeof(str)) {
// 	// 			REQUIRE(false);
// 	// 		}
// 	// 	}

// 	// 	char buf[sizeof(str)];
		
// 	// 	for (size_t i = 0; i < 100 * 1024; i += sizeof(str)) {
// 	// 		if (inode.read(i, buf, sizeof(str)) != sizeof(str)) {
// 	// 			REQUIRE(false);
// 	// 		}
// 	// 		if (strcmp(buf, str) != 0) {
// 	// 			REQUIRE(false);
// 	// 		}
// 	// 	}
// 	// }

// 	// SECTION("Can write a single, very VERY large string all at once"){
// 	// 	std::cout << std::endl << std::endl;
// 	// 	INode inode;
// 	// 	REQUIRE(fs->superblock->disk == disk.get());
// 	// 	inode.superblock = fs->superblock.get();
// 	// 	REQUIRE(inode.superblock->disk == disk.get());

// 	// 	char *message = (char *)malloc(10 * 1024 + 1);
// 	// 	for (size_t i = 0; i < 10 * 1024; ++i) {
// 	// 		message[i] = 'a' + rand() % 26;
// 	// 	}
// 	// 	message[10 * 1024] = 0;

// 	// 	std::cout << message << std::endl;

// 	// 	REQUIRE(inode.write(0, message, 10 * 1024) == 10 * 1024);
		
// 	// 	char *buffer = (char *)malloc(10 * 1024 + 1);
// 	// 	REQUIRE(inode.read(0, buffer, 10 * 1024) == 10 * 1024);
// 	// 	buffer[10 * 1024] = 0; // null terminate it
		
// 	// 	std::cout << "READ BACK: " << buffer << std::endl;
// 	// 	REQUIRE(strcmp(buffer, message) == 0);
// 	// }

// 	// const auto test_helper = [](int offset, int length) {
// 	// 	std::unique_ptr<Disk> disk(new Disk(1024, 1024));
// 	// 	std::unique_ptr<FileSystem> fs(new FileSystem(disk.get()));
// 	// 	fs->superblock->init(0.1);

// 	// 	INode inode;
// 	// 	REQUIRE(fs->superblock->disk == disk.get());
// 	// 	inode.superblock = fs->superblock.get();
// 	// 	REQUIRE(inode.superblock->disk == disk.get());

// 	// 	char *message = (char *)malloc(length + 1);
// 	// 	for (size_t i = 0; i < 10 * 1024; ++i) {
// 	// 		message[i] = 'a' + rand() % 26;
// 	// 	}
// 	// 	message[length] = 0;

// 	// 	REQUIRE(inode.write(offset, message, length) == length);
		
// 	// 	char *buffer = (char *)malloc(length + 1);
// 	// 	REQUIRE(inode.read(offset, buffer, length) == length);
// 	// 	buffer[length] = 0; // null terminate it
		
// 	// 	REQUIRE(strcmp(buffer, message) == 0);
// 	// };

// 	// SECTION("Can write files of sizes from 1 byte to 10KB") {
// 	// 	for (int i = 1; i < 10000; ++i) {
// 	// 		test_helper(0, i);
// 	// 	}
// 	// }

// 	// SECTION("Can write data with byte offsets from 1 to 10K Bytes of length 100 Bytes") {
// 	// 	for (int i = 0; i < 10000; ++i) {
// 	// 		test_helper(i, 100);
// 	// 	}
// 	// }

// 	// SECTION("Can write data with byte offsets from 1 to 10K Bytes of length 2048 Bytes") {
// 	// 	for (int i = 0; i < 10000; ++i) {
// 	// 		test_helper(i, 2048);
// 	// 	}
// 	// }

// 	// SECTION("Can write data with byte offsets from 1 to 10K Bytes of length 4312 Bytes") {
// 	// 	for (int i = 0; i < 10000; ++i) {
// 	// 		test_helper(i, 4312);
// 	// 	}
// 	// }

// 	// TODO: add test for case where disk runs out of space, see what happens
// }
