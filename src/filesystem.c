#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesystem.h"

Disk *myDisk = NULL;

int main(int argc, const char *argv[]) {
	fprintf(stdout, "Hello world!\n");
	myDisk = disk_create(1024, 1024);

	disk_free(myDisk);

	return 0;
}