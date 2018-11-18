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
#include <limits.h>
#include <memory>
#include <unistd.h>

#include "filesystem.hpp"

std::mutex lock_g;
std::unique_ptr<Disk> disk = nullptr;
std::unique_ptr<FileSystem> fs = nullptr;
SuperBlock *superblock = nullptr;


struct UnixError : public std::exception {
	const int errorcode;
	UnixError(int errorcode) : errorcode(errorcode) { };
};

bool can_read_inode(struct fuse_context *ctx, INode& inode) {
	return 
		S_IROTH & inode.data.permissions || // anone can read
		((inode.data.UID == ctx->uid) && (S_IRUSR & inode.data.permissions)) || // owner can read
		((inode.data.GID == ctx->gid) && (S_IRGRP & inode.data.permissions)); // group can read
}

bool can_write_inode(struct fuse_context *ctx, INode& inode) {
	return 
		S_IWOTH & inode.data.permissions || // anyone can write
		((inode.data.UID == ctx->uid) && (S_IWUSR & inode.data.permissions)) || // owner can write
		((inode.data.GID == ctx->gid) && (S_IWGRP & inode.data.permissions)); // group can write
}

bool can_exec_inode(struct fuse_context *ctx, INode& inode) {
	return 
		S_IXOTH & inode.data.permissions || // anyone can exec
		((inode.data.UID == ctx->uid) && (S_IXUSR & inode.data.permissions)) || // owner can exec
		((inode.data.GID == ctx->gid) && (S_IXGRP & inode.data.permissions)); // group can exec
}

std::shared_ptr<INode> resolve_path(const char *path) {
	struct fuse_context *ctx = fuse_get_context();
	std::shared_ptr<INode> inode = superblock->inode_table->get_inode(superblock->root_inode_index);

	if (strcmp(path, "/") == 0) {
		// special case to handle root dir
		return inode;
	}

	assert(path[0] == '/');
	path += 1; // skip the / at the beginning 
	char path_segment[PATH_MAX]; // big buffer to hold segments of the path

	const char *seg_end = nullptr;
	while (seg_end = strstr(path, "/")) {
		if (inode->get_type() != S_IFDIR) {
			throw UnixError(ENOTDIR);
		}

		strncpy(path_segment, path, seg_end - path - 1);

		IDirectory dir(*inode); // load the directory for the inode
		std::unique_ptr<IDirectory::DirEntry> entry = dir.get_file(path_segment);
		if (entry == nullptr) {
			throw UnixError(ENOENT);
		}

		inode = superblock->inode_table->get_inode(entry->data.inode_idx);
		if (!can_read_inode(ctx, *inode)) {
			// this code might as well check that we have access to the path
			fprintf(stdout, "resolve_path found that access is denied to this directory\n");
			throw UnixError(EACCES);
		}
		path = seg_end + 1;
	}

	IDirectory dir(*inode);
	std::unique_ptr<IDirectory::DirEntry> entry = dir.get_file(path);
	if (entry == nullptr)
		throw UnixError(ENOENT);

	return superblock->inode_table->get_inode(entry->data.inode_idx);
}

static int myfs_getattr(const char *path, struct stat *stbuf)
{
	std::lock_guard<std::mutex> g(lock_g);
	int res = 0;
	try{
		std::shared_ptr<INode> inode;
		mode_t type;

		memset(stbuf, 0, sizeof(struct stat));
		inode = resolve_path(path);
		std::cout << path << std::endl;
		stbuf->st_mode = inode->get_type() | inode->data.permissions;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		stbuf->st_ino = inode->inode_table_idx;
		stbuf->st_size = inode->data.file_size;
		stbuf->st_nlink = 1;
	}catch(const UnixError &e){
		return -e.errorcode;
	}
	//res = -ENOENT;

	return res;
}

static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	std::lock_guard<std::mutex> g(lock_g);
	fprintf(stdout, "myfs_readdir(%s, ...)", path);

	try {
		struct fuse_context *ctx = fuse_get_context();
		fprintf(stdout, "\tuid: %d gid: %d pid: %d trying to readdir %s\n", 
			ctx->uid, ctx->gid, ctx->pid, path);
		// fprintf(stdout, "\t\tumask: %d\n", ctx->umask);

		std::shared_ptr<INode> dir_inode = resolve_path(path);
		if (!can_read_inode(ctx, *dir_inode)) {
			// NOTE: I think this should be handled elsewhere, but that is okay
			throw UnixError(EACCES);
		}

		IDirectory dir(*dir_inode);
		std::unique_ptr<IDirectory::DirEntry> entry = nullptr;
		while(entry = dir.next_entry(entry)) {
			if (filler(buf, entry->filename, NULL, 0) != 0)
				return -ENOMEM;
		}

	} catch (const UnixError& e) {
		return -e.errorcode;
	}

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
