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
#include <sys/ioctl.h>
#include <net/if.h>
#include <string>


const int MAX_BUF = 50;
const int NUM_STAT = 11;
char netPath[MAX_BUF] = "/sys/class/net/"; /// interface name will be added here
char socketPath[MAX_BUF] = "/tmp/interfaceMonitor/";
char path[MAX_BUF], box[MAX_BUF];
bool isRunning = true;
int sockFd;

enum exit_code { ERR_ARG = 1, ERR_SIG, ERR_SOCK, ERR_CONNECT, ERR_OPEN_DIR, ALERT };
enum state { UP = 1, DOWN = 0, UNKNOWN = -1 };
struct stat {
    char subPath[MAX_BUF];
    char title[MAX_BUF];
    int val;
};

stat STAT[] = {
    {"/operstate"         , "state"     , 0}, /// will map an int to `up` , `down` , and `unknown` state
    {"/carrier_up_count"  , "up_count"  , 0},
    {"/carrier_down_count", "down_count", 0},
    {"/statistics/"       , "tx_bytes"  , 0},
    {"/statistics/"       , "tx_packets", 0},
    {"/statistics/"       , "tx_dropped", 0},
    {"/statistics/"       , "tx_errors" , 0},
    {"/statistics/"       , "rx_bytes"  , 0},
    {"/statistics/"       , "rx_packets", 0},
    {"/statistics/"       , "rx_dropped", 0},
    {"/statistics/"       , "rx_errors" , 0},
};


typedef void (*sighandler_t)(int);
static void signalHandler(int signal);
void handleError(std::string msg1, char* msg2, exit_code err = ALERT, int fd = -1);
void talk(const char* toSay, char* answer);



//------------------------------------- MAIN ------------------------------------------//
int main(int argc, char *argv[]) {
    // check the path to the requested interface
    if (argc != 2) handleError("Interface name not provided", NULL, ERR_ARG);
    strcat(netPath, argv[1]);
    
    // register signal handler
    sighandler_t err = signal(SIGINT, signalHandler);
    if (err == SIG_ERR) handleError("could not creat a signal handler", NULL, ERR_SIG);
    
    // setup socket communication
    sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd == -1) handleError("error on creating socket", NULL, ERR_SOCK);
    struct sockaddr_un sockAddr;
    bzero(&sockAddr, sizeof(sockAddr));
    sockAddr.sun_family = AF_UNIX;
    bzero(box, MAX_BUF);
    sprintf(box,"%d", getpid());
    strcat(socketPath,box);
    strncpy(sockAddr.sun_path, socketPath, sizeof(sockAddr.sun_path) - 1);
    
    // connect to local socket
    int status = connect(sockFd, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
    if (status == -1) handleError("error on connection", NULL, ERR_CONNECT);
    
    // inform server the readiness and read the instruction
    talk("Ready", box);
    if (strcmp(box, "Monitor") != 0)
        handleError("didn't undrestand the instruction", NULL, ERR_CONNECT);
    else talk("Monitoring", NULL);
    
    // open the /sys/class/net/<interface> directory and monitor stats
    std::ifstream ifs;
    struct ifreq ifr;
    std::cout << "Interface: " << argv[1] << " ";
    while (isRunning) {
        for (int i = 0; i < NUM_STAT; i++) {
            bzero(path, MAX_BUF);
            strcpy(path, netPath);
            strcat(path, STAT[i].subPath);
            if (i > 2) strcat(path,STAT[i].title);
            ifs.open(path);
            if(ifs.is_open()) {
                if (i == 0) { /// this is `operstate` info
                    bzero(box, MAX_BUF);
                    ifs >> box;
                    if (strcmp(box, "up") == 0) STAT[i].val = UP;
                    else if (strcmp(box, "down") == 0) STAT[i].val = DOWN;
                    else STAT[i].val = UNKNOWN;
                }
                else ifs >> STAT[i].val;
                ifs.close();
                std::cout << STAT[i].title << ": " << STAT[i].val << " ";
                if (i == 2 || i == NUM_STAT) std::cout << std::endl;
            }
            else handleError("\ncouldn't open a statistic file", NULL, ERR_OPEN_DIR);
        }
        std::cout << std::endl;
        
        // check if interface down -> report -> set up if requested
        if (STAT[0].val == DOWN) {
            talk("Link Down", box);
            if (strcmp(box, "Set Link Up") == 0) {
                bzero(&ifr, sizeof(ifr));
                strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
                ifr.ifr_ifru.ifru_flags |= IFF_UP;
                int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
                status = ioctl(socketFd, SIOCSIFFLAGS, &ifr);
                if (status == -1 || socketFd == -1) strerror(errno);
            }
        }
        
        sleep(1);
    }

    
    return 0;
}


static void signalHandler(int signal) {
    switch(signal) {
        case SIGINT:
            isRunning = false;
            close(sockFd);
        break;
    default:
            std::cout << "Unexpected exit." << std::endl;
    }
}

void handleError(std::string msg1, char* msg2, exit_code err, int fd) {
    std::cout << msg1;
    if(msg2) std::cout << msg2;
    std::cout << std::endl;
    if (fd > 0) close(fd); /// fd has a default value of -1
    if (err != ALERT) {
        strerror(errno);
        exit(err);
    }
}

void talk(const char* toSay, char* answer) {
    int status = write(sockFd, toSay, strlen(toSay)+1);
    if (status == -1) handleError("error on connection", NULL, ERR_CONNECT);
    
    if (answer) { /// !NULL
        bzero(answer, MAX_BUF);
        status = read(sockFd, answer, MAX_BUF);
        if (status == -1) handleError("error on connection", NULL, ERR_CONNECT);
    }
}
