#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <limits.h>
#include <memory>
#include <unistd.h>
#include <libgen.h>
#include <math.h>

#include "filesystem.hpp"

int main() {
	fprintf(stdout, "does nothing at the moment.\n");

	const size_t CHUNK_COUNT =  1024;
	const size_t CHUNK_SIZE = 4096;

	std::unique_ptr<FileSystem> fs = nullptr;
	std::unique_ptr<Disk> disk = nullptr;
	SuperBlock *superblock = nullptr;

	int fh = open("realdisk.myanfest", O_RDWR | O_CREAT);
	truncate("realdisk.myanfest", CHUNK_COUNT * CHUNK_SIZE);
	disk = std::unique_ptr<Disk>(new Disk(CHUNK_COUNT, CHUNK_SIZE, MAP_FILE | MAP_SHARED, fh));
	fs = std::unique_ptr<FileSystem>(new FileSystem(disk.get()));
	fs->superblock->init(0.1);
	superblock = fs->superblock.get();

	fs = nullptr;
	disk = nullptr;

	fprintf(stdout, "disk successfully initialized");
}
