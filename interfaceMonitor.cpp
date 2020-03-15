//
//  interfaceMonitor.cpp
//  Assignment 1
//
//

#include <iostream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#include <errno.h>

enum err_code { ERR_OPEN_DIR = 1, ERR_FORK };

const char path[] = "/sys/class/net";
const int MAX_BUF = 64;

void handleError(char* msg, err_code err);

int main(int argc, char *argv[]) {
    DIR* dp = opendir(path);
    if (dp == NULL) handleError("Could not open interfaces directory.", ERR_OPEN_DIR);
    struct dirent* dirp;
    int num_interface = 0;
    std::vector<int> pids;
    int pid;
    // missing operstate, carrier_up_count, carrier_down_count
    int tx_bytes, tx_dropped, tx_errors, tx_packets;
    int rx_bytes, rx_dropped, rx_errors, rx_packets;
    std::ifstream ifs;
    char subPath[MAX_BUF];
    while( (dirp = readdir(dp)) ) {
        ++num_interface;
        pid = fork();
        if (pid == -1) handleError("Could not open interfaces directory.", ERR_FORK);
        else if (pid == 0) { /// child
            sprintf(subPath, "/sys/class/net/%s/statistics/tx_bytes", dirp->d_name);
        }
    }
    
    
    return 0;
}


void handleError(char* msg, err_code err) {
    std::cout << msg << strerror(errno) << std::endl;
    exit(err);
}

