#ifndef SYSCALL_HPP
#define SYSCALL_HPP

#include <filesystem.hpp>

struct MockSyscalls {
    FileSystem * fs;

    int mknod(const char * pathname) {
        if(*pathname != '/') {
            throw FileSystemException("relative file paths not supported");
        } else {
            pathname += 1;
        }
        const char * delimiter;
        char path_segment[256];
        path_segment[0] = '\0';
        int iteration = 0;
        while(delimiter = strchr(pathname, '/')) {
            strncpy(path_segment, pathname, delimiter - pathname);
            path_segment[delimiter - pathname] = '\0';   
            std::cout << iteration << ", pathname: " << pathname << std::endl;
            std::cout << iteration << ", segment: " << path_segment << std::endl;
            std::cout << iteration << ", delimiter: " << delimiter << std::endl;
            if(delimiter[1] == '\0') {
                path_segment[delimiter - pathname - 1] = '\0';
                break;
            }       
            iteration++;
            pathname = delimiter + 1;
            //Check for directory exists
        }
        std::cout << iteration << ", pathname: " << pathname << std::endl;
        std::cout << iteration << ", segment: " << path_segment << std::endl;
        std::cout << iteration << ", delimiter: " << delimiter << std::endl;

        //We found path, directory to be created is path_segment
        //INode node = fs->superblock->inode_table->alloc_inode();
        //uint64_t file_size = 0;
        //uint64_t reference_count = 1;

        return 0;
    }
};






#endif