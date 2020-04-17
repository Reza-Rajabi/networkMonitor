//
//  interfaceMonitor.cpp
//  
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
#include <fcntl.h>
#include <net/if.h>
#include <string>


const int MAX_BUF = 50;
const int NUM_STAT = 11;
const int INTERVAL = 1;
char netPath[MAX_BUF] = "/sys/class/net/"; /// interface name will be added here
char socketPath[MAX_BUF] = "/tmp/networkMonitor";
bool isRunning = true;
int sockFd;

enum exit_code { DONE, ERR_ARG, ERR_SIG, ERR_SOCK, ERR_CONNECT, ERR_OPEN_DIR, ALERT };
enum state { UP = 1, DOWN = 0, UNKNOWN = -1 };
struct stats {
    char subPath[MAX_BUF];
    char title[MAX_BUF/2];
    int val;
};

stats STAT[] = {
    {"/operstate"         , "state"     , 0}, /// will map an int to `up` , `down` , and `unknown` state
    {"/carrier_up_count"  , "up_count"  , 0},
    {"/carrier_down_count", "down_count", 0},
    {"/statistics/"       , "rx_bytes"  , 0},
    {"/statistics/"       , "rx_dropped", 0},
    {"/statistics/"       , "rx_errors" , 0},
    {"/statistics/"       , "rx_packets", 0},
    {"/statistics/"       , "tx_bytes"  , 0},
    {"/statistics/"       , "tx_dropped", 0},
    {"/statistics/"       , "tx_errors" , 0},
    {"/statistics/"       , "tx_packets", 0}
};


typedef void (*sighandler_t)(int);
static void signalHandler(int signal);
void handleError(std::string msg, exit_code err = ALERT, int fd = -1);



//------------------------------------- MAIN ------------------------------------------//
int main(int argc, char *argv[]) {
    // check the path to the requested interface
    if (argc != 2) handleError("Interface name not provided", ERR_ARG);
    strcat(netPath, argv[1]);
    
    // register signal handler
    sighandler_t err = signal(SIGINT, signalHandler);
    if (err == SIG_ERR) handleError("could not creat a signal handler", ERR_SIG);
    
    // setup socket communication
    sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd == -1) handleError("error on creating socket", ERR_SOCK);
    struct sockaddr_un sockAddr;
    bzero(&sockAddr, sizeof(sockAddr));
    sockAddr.sun_family = AF_UNIX;
    strncpy(sockAddr.sun_path, socketPath, sizeof(sockAddr.sun_path) - 1);
    
    // connect to local socket
    ssize_t status = connect(sockFd, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
    if (status == -1) handleError("error on connection", ERR_CONNECT, sockFd);
    
    // inform server the readiness and read the instruction
    char buf[MAX_BUF];
    status = write(sockFd, "Ready", 6);
    if (status == -1) handleError("error on connection", ERR_CONNECT, sockFd);
    bzero(buf, MAX_BUF);
    status = read(sockFd, buf, MAX_BUF);
    if (status == -1) handleError("error on connection", ERR_CONNECT, sockFd);
    if (strcmp(buf, "Monitor") != 0)
        handleError("didn't undrestand the instruction", ERR_CONNECT, sockFd);
    else {
        ssize_t status = write(sockFd, "Monitoring", 11);
        if (status == -1) handleError("error on connection", ERR_CONNECT, sockFd);
    }
    
    // MUST HAVE NON-BLOCKING READ AFTER THIS POINT to get `Shout Down` at any time
    /// setup for non-blocking read using select()
    fd_set coming;
    FD_ZERO(&coming);
    FD_SET(sockFd, &coming);
    
    // open the /sys/class/net/<interface> directory and monitor stats
    std::ifstream ifs;
    struct ifreq ifr;
    while (isRunning) { /// to handle the non-blocking read
        while (isRunning) { /// to handle the report per time interval
            // report
            std::cout << "Interface: " << argv[1] << " ";
            for (int i = 0; i < NUM_STAT; i++) {
                bzero(buf, MAX_BUF);
                strcpy(buf, netPath);
                strcat(buf, STAT[i].subPath);
                if (i > 2) strcat(buf,STAT[i].title);
                ifs.open(buf);
                if(ifs.is_open()) {
                    if (i == 0) { /// this is `operstate` info
                        bzero(buf, MAX_BUF);
                        ifs >> buf;
                        if (strcmp(buf, "up") == 0) STAT[i].val = UP;
                        else if (strcmp(buf, "down") == 0) STAT[i].val = DOWN;
                        else STAT[i].val = UNKNOWN;
                    }
                    else ifs >> STAT[i].val;
                    ifs.close();
                    std::cout << STAT[i].title << ": ";
                    if (i == 0) { /// this is `operstate` info
                        if (STAT[i].val == UP) std::cout << "up ";
                        else if (STAT[i].val == DOWN) std::cout << "down ";
                        else std::cout << "unknown ";
                    }
                    else std::cout << STAT[i].val << " ";
                }
                else std::cout << STAT[i].title << " "; // "couldn't open " <<
                if (i == NUM_STAT-1) std::cout << std::endl << std::endl;
            }
            
            // check if interface down
            if (STAT[0].val == DOWN) {
                status = write(sockFd, "Link Down", 10);
                if (status == -1) handleError("error on connection", ERR_CONNECT, sockFd);
            }
            
            
            sleep(INTERVAL);
        }
        
        
        ///---------------------------------------------- IN THE OUTER LOOP --------------------------------------///
        
        // using select to have a non-blocking read
        status = select(sockFd+1, &coming, NULL, NULL, NULL);
        if (status == -1 || !FD_ISSET(sockFd, &coming))
            bzero(buf, MAX_BUF);
        read(sockFd, buf, MAX_BUF); /// non-blocking read
        
        // respond
        if ( strcmp(buf, "Shut Down") == 0 ) signalHandler(SIGINT);
        if (strcmp(buf, "Set Link Up") == 0) {
            bzero(&ifr, sizeof(ifr));
            strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
            ifr.ifr_ifru.ifru_flags |= IFF_UP;
            int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
            status = ioctl(socketFd, SIOCSIFFLAGS, &ifr);
            if (status == -1 || socketFd == -1) strerror(errno);
        }
    }

    
    return DONE;
}


/* takes a interuption signal and respond to it by closing and
   cleaning up the resources and informing the master server
   in case the program has been intrupted by ctrl-c or recieved
   a shut down message from the server
 */
static void signalHandler(int signal) {
    switch(signal) {
        case SIGINT:
            write(sockFd, "Done", 5); /// will not check for error or feedback
            isRunning = false;
            close(sockFd);
        break;
    default:
            std::cout << "Unexpected exit." << std::endl;
    }
}

/* takes a message, an optional error code and an optional file descriptor
   and handle the error by proper printing the message and potentionaly the
   standard system error message. It may also close the socket and exit if
   the error error was not only an alert to the user
*/
void handleError(std::string msg, exit_code err, int fd) {
    if (isRunning) { /// isRunning flag prevents unnecessary error handling while signal handler is shutting down
        std::cout << msg << std::endl;
        if (fd > 0) close(fd); /// fd has a default value of -1
        if (err != ALERT) {
            strerror(errno);
            exit(err);
        }
    }
}
