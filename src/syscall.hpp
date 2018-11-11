#ifndef SYSCALL_HPP
#define SYSCALL_HPP

#include <filesystem.hpp>
#include <vector>
#include <string>

struct MockSyscalls {
    FileSystem * fs;

    std::vector<std::string> parse_path(const char * pathname) {
        if(*pathname != '/') {
            throw FileSystemException("relative file paths not supported");
        } else {
            pathname += 1;
        }

        const char * delimiter;
        int iteration = 0;
        std::vector<std::string> parsed_path;
        while(delimiter = strchr(pathname, '/') ) {
            if(delimiter != pathname) {
                parsed_path.push_back(std::string(pathname, delimiter - pathname));
                iteration++;
            }
            pathname = delimiter + 1;
            //Check for directory exists
        }
        if(strlen(pathname) > 0) {
            parsed_path.push_back(std::string(pathname));
        }
        return parsed_path;
    }

    int mknod(const char * pathname) {
        
        return 0;
    }
};






#endif