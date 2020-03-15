//
//  interfaceMonitor.cpp
//  Assignment 1
//
//

#include <iostream>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#include <errno.h>



const char path[] = "/sys/class/net";

int main(int argc, char *argv[]) {
    DIR* dp = opendir(path);
    if (dp == NULL) {
        std::cout << "Could not open interfaces directory.";
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    struct dirent* dirp;
    int num_interface = 0;
    std::vector<int> pids;
    int pid;
    while( (dirp = readdir(dp)) ) {
        ++num_interface;
        pid = fork();
        if (pid == -1) {
            std::cout << "Could not open interfaces directory.";
            std::cout << strerror(errno) << std::endl;
            return 1;
        }
        else if (pid == 0) { /// child
            
        }
    }
    
    
    return 0;
}
