#ifndef FUSEINTERFACE_H
#define FUSEINTERFACE_H

#include <fuse.h>

static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
	return 0;
}

static struct fuse_operations operations = {
    //.getattr	= do_getattr,
    .readdir	= do_readdir,
    //.read	= do_read,
};

#endif
