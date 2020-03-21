//
//  networkMonitor.cpp
//  Assignment 1
//
//

#include <iostream>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <vector>


const int MAX_BUF = 50;
const int MAX_CLIENT = 5;
char socketPath[MAX_BUF] = "/tmp/networkMonitor/";
const char interfaceMonitor[MAX_BUF] = "./intfMonitor";
char buf[MAX_BUF];
bool isRunning = true;
int num_intface;
int masterFd, clientFDs[MAX_CLIENT];

enum exit_code { ERR_ARG = 1, ERR_SIG, ERR_SOCK, ERR_CONNECT, ERR_OPEN_DIR, ALERT };


typedef void (*sighandler_t)(int);
static void signalHandler(int signal);
void handleError(std::string msg1, std::string msg2 = "", exit_code err = ALERT, int fd = -1);



//------------------------------------- MAIN ------------------------------------------//
int main(int argc, char *argv[]) {
    // register signal handler
    sighandler_t err = signal(SIGINT, signalHandler);
    if (err == SIG_ERR) handleError("could not creat a signal handler", "", ERR_SIG);
    
    // query the user
    std::vector<std::string> interfaces;
    ssize_t status = -1;
    while (status == -1) {
        std::cout << "Enter the number and the name of the interfaces to be monitored: ";
        bzero(buf, MAX_BUF);
        std::cin >> buf;
        num_intface = atoi(buf);
        if (num_intface == 0) { /// either is zero or user didn't enter an int
            handleError("Please also specify the number of interfaces");
            status = -1;
        }
        else {
            bzero(buf, MAX_BUF);
            status = read(STDIN_FILENO, buf, MAX_BUF); /// already took the int
            if (status == -1) handleError("Please try again");
        }
    }
    while(interfaces.size() < num_intface) interfaces.push_back(std::strtok(buf, " "));
    
    // setup socket communication
    num_intface = num_intface > MAX_CLIENT ? MAX_CLIENT : num_intface;
    int newSocketFd, maxFd;
    int clientCounter = 0;
    fd_set activeFDs, readFDs;
    FD_ZERO(&activeFDs);
    FD_ZERO(&readFDs);
    /// create master socket
    masterFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (masterFd == -1) handleError("error on creating socket", "", ERR_SOCK);
    struct sockaddr_un serverAddr;
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, socketPath, sizeof(serverAddr.sun_path) - 1);
    
    // bind the socket to the local socket file
    status = bind(masterFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (status == -1) handleError("error on binding socket", "", ERR_CONNECT, masterFd);
    
    // listen for clients to connect
    status = listen(masterFd, num_intface);
    if (status == -1) handleError("error on start listening", "", ERR_CONNECT, masterFd);
    
    // adding masterFd to the set fd_set
    FD_SET(masterFd, &activeFDs);
    maxFd = masterFd;
    
    // spwan interfaceMonitor childs
    pid_t tempPid;
    for (int i = 0; i < num_intface; i++) {
        tempPid = fork();
        if (tempPid == 0) {
            execl(interfaceMonitor, interfaceMonitor, interfaces[i].c_str(), NULL);
            /// this process will die after the above line, so we never need to check the main process (ex. isParent)
        }
        else if (tempPid == -1) handleError("could not monitor", interfaces[i]);
        /// else --> is parent --> loop
    }
    
    
    while(isRunning) {
        readFDs = activeFDs;
        // select from up to maxFd + 1 sockets
        status = select(maxFd + 1, &readFDs, NULL, NULL, NULL);
        if (status == -1) handleError("couldn't select client");
        
        if (FD_ISSET(masterFd, &readFDs)) { /// this is a new connection request
            newSocketFd = accept(masterFd, NULL, NULL);
            if (newSocketFd == -1) handleError("couldn't accept client");
            else { // add the client to the active list and communicate
                clientFDs[clientCounter] = newSocketFd;
                FD_SET(clientFDs[clientCounter], &activeFDs);
                
                // setup maxFd and counter
                if (maxFd < clientFDs[clientCounter]) maxFd = clientFDs[clientCounter];
                ++clientCounter;
            }
        }
        else { // this is a former client --> find the client and communicate
            for (int i = 0; i < num_intface; i++) {
                if (FD_ISSET(clientFDs[i], &readFDs)) { /// found
                    bzero(buf, MAX_BUF);
                    status = read(clientFDs[i], buf, MAX_BUF);
                    if (status == -1) handleError("couldn't read from interface", interfaces[i]);
                    else {  /// folow the protocol
                        if (strcmp(buf, "Ready") == 0) {
                            status = write(clientFDs[i], "Monitor", 8);
                            if (status == -1) handleError("couldn't instruct to interface", interfaces[i]);
                        }
                        else if (strcmp(buf, "Link Down") == 0) {
                            status = write(clientFDs[i], "Set Link Up", 8);
                            if (status == -1) handleError("couldn't instruct to interface", interfaces[i]);
                        }
                        else if (strcmp(buf, "Done") == 0) {
                            close(clientFDs[i]);
                        }
                    }
                }
            }
        }
    }
    

    return 0;
}


static void signalHandler(int signal) {
    switch(signal) {
        case SIGINT:
            isRunning = false;
            for (int i = 0; i < num_intface; i++) {
                write(clientFDs[i], "Shut Down", 10);
                close(clientFDs[i]);
            }
            close(masterFd);
        break;
    default:
            std::cout << "Unexpected exit." << std::endl;
    }
}

void handleError(std::string msg1, char* msg2, exit_code err, int fd) {
    std::cout << msg1;
    if (msg2) std::cout << msg1;
    std::cout << std::endl;
    if (fd > 0) close(fd); /// fd has a default value of -1
    if (err != ALERT) {
        strerror(errno);
        exit(err);
    }
}

