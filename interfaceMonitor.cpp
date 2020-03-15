//
//  interfaceMonitor.cpp
//  Assignment 1
//
//

#include <iostream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <vector>
#include <string>

typedef void (*sighandler_t)(int);
enum exit_err_code { ERR_SIG = 1, ERR_SOCK, ERR_CONNECT, ERR_OPEN_DIR,
                     ERR_FORK, IGNORE };

const char* statsTitle[] { "tx_bytes", "tx_dropped", "tx_errors", "tx_packets",
                           "rx_bytes", "rx_dropped", "rx_errors", "rx_packets" };
enum stats_index         { tx_bytes, tx_dropped, tx_errors, tx_packets,
                           rx_bytes, rx_dropped, rx_errors, rx_packets };
// MISSING operstate, carrier_up_count, carrier_down_count


const int MAX_BUF = 255;
const int NUM_STAT = 8; /// number of words in statsTitle --------------> NEEDS EDITING WHEN missing RESOLVED
const char netPath[] = "/sys/class/net";
const char statPath[] = "/sys/class/net/%s/statistics/";
char socketPath[MAX_BUF] = "/tmp/interfaceMonitor/";
char path[MAX_BUF], temp[MAX_BUF];
bool isRunning = true;
int sockFd;


void handleError(std::string msg1, char* msg2, exit_err_code err = IGNORE, int fd = -1);
void loadStatistics(char* interfaceName, int* stats);

static void signalHandler(int signal);

int main(int argc, char *argv[]) {
    // register signal handler
    sighandler_t err = signal(SIGINT, signalHandler);
    if (err == SIG_ERR)
        handleError("could not creat a signal handler", nullptr, ERR_SIG);
    
    // setup socket communication
    sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd == -1)
        handleError("error on creating socket", nullptr, ERR_SOCK);
    struct sockaddr_un sockAddr;
    bzero(&sockAddr, sizeof(sockAddr));
    sockAddr.sun_family = AF_UNIX;
    bzero(temp, MAX_BUF);
    sprintf(temp,"%d", getpid());
    strcat(socketPath,temp);
    strncpy(sockAddr.sun_path, socketPath, sizeof(sockAddr.sun_path) - 1);
    
    // connect to local socket
    int status = connect(sockFd, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
    if (status == -1)
        handleError("error on connection", nullptr, ERR_CONNECT);
    
    // inform server that interfaceMonitor is ready
    status = write(sockFd, "READY", 6);
    if (status == -1)
        handleError("error on connection", nullptr, ERR_CONNECT);
    
    // read the instructed interfaces to monitor
    bzero(temp, MAX_BUF);
    status = read(sockFd, temp, MAX_BUF);
    if (status == -1)
        handleError("error on connection", nullptr, ERR_CONNECT);
    std::vector<std::string> instructedInterfaces;
    char* word;
    while( (word = strtok(temp, " ")) ) instructedInterfaces.push_back(std::string(word));
    
    // opend the /sys/class/net directory to monitor stats
    DIR* dp = opendir(netPath);
    if (dp == NULL)
        handleError("could not open interfaces directory.", nullptr, ERR_OPEN_DIR);
    struct dirent* dirp;
    std::vector<int> childPids;
    pid_t pid = getpid();
    int statistics[NUM_STAT];
    while( (dirp = readdir(dp)) ) { /// find interfaces in directory and spwan child to monitor
        bool found = false;
        for (int i = 0; i < instructedInterfaces.size() && !found; i++) {
            if ( instructedInterfaces[i] == std::string(dirp->d_name) ) found = true;
        }
        if (found) { /// this interface was requested to be monitored
            pid = fork();
            if (pid == -1)
                handleError("could not open interfaces directory.", nullptr, ERR_FORK);
            else if (pid == 0) { /// child needs to keep monitor until gets intrupted by signal
                while (isRunning) {
                    loadStatistics(dirp->d_name, statistics);
                    // print
                    // check if interface down -> report -> set up if requested
                }
            }
            else childPids.push_back(pid);
        }
    }
    
    // here parnt checks if any child goes down, it restart the process again
    while (isRunning && pid > 0) {
        pid = wait(&status);
        bool found = false;
        for (int i = 0; i < childPids.size() && !found; i++) {
            if (pid == childPids[i]) found = true;
            childPids[i] = fork();
            if (childPids[i] == 0) {
                //  MONITOR
            }
        }
    }
    
    return 0;
}


void handleError(std::string msg1, char* msg2, exit_err_code err, int fd) {
    std::cout << msg1;
    if(msg2) std::cout << msg2;
    std::cout << std::endl;
    if (fd > 0) close(fd);
    if (err != IGNORE) {
        strerror(errno);
        exit(err);
    }
}

void loadStatistics(char* interfaceName, int* stats) {
    bzero(path, MAX_BUF);
    strcpy(path, statPath);
    sprintf(path, path, interfaceName);
    std::ifstream ifs;
    for (int i = 0; i < NUM_STAT; i++) {
        bzero(temp, MAX_BUF);
        strcpy(temp, path);
        strcat(temp,statsTitle[i]);
        ifs.open(temp);
        if(ifs.is_open()) {
            ifs >> stats[i];
            ifs.close();
        }
        else handleError("couldn't open a statistic for ", interfaceName);
    }
}

static void sigHandler(int signal) {
    switch(signal) {
        case SIGINT:
            isRunning = false;
            close(sockFd);
        break;
    default:
            std::cout << "Unexpected exit." << std::endl;
    }
}

