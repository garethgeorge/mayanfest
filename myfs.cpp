/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall myfs.c `pkg-config fuse --cflags --libs` -o myfs
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <mutex>

#include "filesystem.hpp"

std::mutex lock_g;
std::unique_ptr<Disk> disk = nullptr;
std::unique_ptr<FileSystem> fs = nullptr;
SuperBlock *superblock = nullptr;

static int myfs_getattr(const char *path, struct stat *stbuf)
{
	std::lock_guard<std::mutex> g(lock_g);
	int res = 0;

	//memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}else{
	//} else if (strcmp(path, myfs_path) == 0) {
	//	stbuf->st_mode = S_IFREG | 0444;
	//	stbuf->st_nlink = 1;
	//	stbuf->st_size = strlen(myfs_str);
	//} else
		res = -ENOENT;
	}

	return res;
}

static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	std::lock_guard<std::mutex> g(lock_g);

	if(strcmp(path, "/") == 0){
		INode dir_inode = superblock->inode_table->get_inode(superblock->root_inode_index);
		IDirectory dir(dir_inode);
		std::unique_ptr<IDirectory::DirEntry> entry = nullptr;
		while(entry = dir.next_entry(entry)){
			filler(buf, entry->filename, NULL, 0);
		}
	}else{
		return -ENOENT;
	}

	//(void) offset;
	//(void) fi;

	//if (strcmp(path, "/") != 0)
	//	return -ENOENT;

	//filler(buf, ".", NULL, 0);
	//filler(buf, "..", NULL, 0);
	//filler(buf, myfs_path + 1, NULL, 0);

	return 0;
}

static int myfs_open(const char *path, struct fuse_file_info *fi)
{
	std::lock_guard<std::mutex> g(lock_g);

	//printf("open called!!!");

	//if (strcmp(path, myfs_path) != 0)
	//	return -ENOENT;

	//if ((fi->flags & 3) != O_RDONLY)
	//	return -EACCES;

	//return 0;
	return -ENOENT;
}

static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	std::lock_guard<std::mutex> g(lock_g);

	//size_t len;
	//(void) fi;
	//if(strcmp(path, myfs_path) != 0)
	//	return -ENOENT;

	//len = strlen(myfs_str);
	//if (offset < len) {
	//	if (offset + size > len)
	//		size = len - offset;
	//	memcpy(buf, myfs_str + offset, size);
	//} else
	//	size = 0;

	//return size;
	return 0;
}

int main(int argc, char *argv[])
{
	
	disk = std::unique_ptr<Disk>(new Disk(1024*1024*1024 / 512, 512));
	fs = std::unique_ptr<FileSystem>(new FileSystem(disk.get()));
	fs->superblock->init(0.1);
	superblock = fs->superblock.get();
	
	static struct fuse_operations myfs_oper;
	myfs_oper.getattr = myfs_getattr;
	myfs_oper.readdir = myfs_readdir;
	myfs_oper.open = myfs_open;
	myfs_oper.read = myfs_read;


	return fuse_main(argc, argv, &myfs_oper, NULL);
}
