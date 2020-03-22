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
#include <signal.h>
#include <string>
#include <vector>


const int MAX_BUF = 50;
const int MAX_CLIENT = 10;
char socketPath[MAX_BUF] = "/tmp/networkMonitor";
const char interfaceMonitor[MAX_BUF] = "./intfMonitor";
bool isRunning = true;
int num_intface;
int masterFd, clientFDs[MAX_CLIENT];
fd_set activeFDs, readFDs;

enum exit_code { DONE, ERR_ARG, ERR_SIG, ERR_SOCK, ERR_CONNECT, ERR_OPEN_DIR, ALERT };


typedef void (*sighandler_t)(int);
static void signalHandler(int signal);
void handleError1(std::string msg, exit_code err = ALERT, int fd = -1);
void handleError2(std::string msg1, std::string msg2, exit_code err = ALERT, int fd = -1);



//------------------------------------- MAIN ------------------------------------------//
int main(int argc, char *argv[]) {
    // register signal handler
    sighandler_t err = signal(SIGINT, signalHandler);
    if (err == SIG_ERR) handleError1("could not creat a signal handler", ERR_SIG);
    
    // query the user
    char buf[MAX_BUF];
    std::vector<std::string> interfaces;
    ssize_t status = -1;
    while (status == -1 && isRunning) {
        /// isRunning here makes this part managable by SIGINT, if happens before user entry
        std::cout << "Enter the number and the names of the interfaces, space separated: ";
        /// example of user input: 4 eth0  ip6tnl0  lo  tunl0
        bzero(buf, MAX_BUF);
        status = scanf("%s",buf);
        num_intface = atoi(buf);
        if (num_intface == 0 || status <= 0) { /// either is zero or user didn't enter an integer
            handleError1("Please also specify the number of interfaces");
            handleError1("example: 4 eth0  ip6tnl0  lo  tunl0");
            status = -1;
        }
        else {
            while(interfaces.size() < num_intface && status > 0) {
                bzero(buf, MAX_BUF);
                status = scanf("%s",buf);
                interfaces.push_back(buf);
            }
            status = 0;
        }
    }
    
    // setup socket communication
    num_intface = num_intface > MAX_CLIENT ? MAX_CLIENT : num_intface;
    int newSocketFd, maxFd;
    int clientCounter = 0;
    FD_ZERO(&activeFDs);
    FD_ZERO(&readFDs);
    /// create master socket
    masterFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (masterFd == -1) handleError1("error on creating socket", ERR_SOCK);
    /// set up server address
    struct sockaddr_un serverAddr;
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, socketPath, sizeof(serverAddr.sun_path) - 1);
    unlink(socketPath);
    
    // bind the socket to the local socket file
    status = bind(masterFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (status == -1) handleError1("error on binding socket", ERR_CONNECT, masterFd);
    
    // listen for clients to connect
    status = listen(masterFd, num_intface);
    if (status == -1) handleError1("error on start listening", ERR_CONNECT, masterFd);
    
    // adding masterFd to the set fd_set
    FD_SET(masterFd, &activeFDs);
    maxFd = masterFd;
    
    // spawn childs to run interfaceMonitor
    pid_t tempPid;
    for (int i = 0; i < num_intface; i++) {
        tempPid = fork();
        if (tempPid == 0) {
            execl(interfaceMonitor, interfaceMonitor, interfaces[i].c_str(), NULL);
            /// this process will die after the above line, so we never need to check the main process (ex. isParent)
        }
        else if (tempPid == -1) handleError2("could not monitor", interfaces[i]);
        /// else --> is parent --> loop
    }
    
    
    while(isRunning) {
        readFDs = activeFDs;
        // select from up to maxFd + 1 sockets
        status = select(maxFd + 1, &readFDs, NULL, NULL, NULL);
        if (status == -1) handleError1("couldn't select client");
        
        if (FD_ISSET(masterFd, &readFDs)) { /// this is a new connection request, accept and add to the list
            newSocketFd = accept(masterFd, NULL, NULL);
            if (newSocketFd == -1) handleError1("couldn't accept client");
            else { // add the client to the active list
                clientFDs[clientCounter] = newSocketFd;
                FD_SET(clientFDs[clientCounter], &activeFDs);
                
                // setup maxFd and counter
                if (maxFd < clientFDs[clientCounter]) maxFd = clientFDs[clientCounter];
                ++clientCounter;
            }
        }
        else { // this is a former client --> find the client and communicate
            bool found = false;
            for (int i = 0; i < num_intface && !found; i++) {
                if (FD_ISSET(clientFDs[i], &readFDs)) { /// found
                    found = true;
                    bzero(buf, MAX_BUF);
                    status = read(clientFDs[i], buf, MAX_BUF);
                    if (status == -1) handleError2("couldn't read from interface", interfaces[i]);
                    else {  /// follow the protocol
                        if (strcmp(buf, "Ready") == 0) {
                            status = write(clientFDs[i], "Monitor", 8);
                            if (status == -1) handleError2("couldn't instruct to interface", interfaces[i]);
                        }
                        else if (strcmp(buf, "Link Down") == 0) {
                            status = write(clientFDs[i], "Set Link Up", 12);
                            if (status == -1) handleError2("couldn't instruct to interface", interfaces[i]);
                        }
                        else if (strcmp(buf, "Done") == 0) {
                            FD_CLR(clientFDs[i], &readFDs);
                            close(clientFDs[i]);
                        }
                    } /// end of protocol
                } /// end of found
            } /// end of loop
        } /// end of former client's request handling
    } /// end of isRunning
    

    return DONE;
}


/* takes a interuption signal and respond to it by closing and
  cleaning up the resources and informing the slaves clients
  in case the program has been intrupted by ctrl-c
*/
static void signalHandler(int signal) {
    switch(signal) {
        case SIGINT:
            isRunning = false;
            for (int i = 0; i < num_intface; i++) {
                write(clientFDs[i], "Shut Down", 10);
                FD_CLR(clientFDs[i], &readFDs);
                close(clientFDs[i]);
            }
            unlink(socketPath);
            close(masterFd);
        break;
    default:
            std::cout << "Unexpected exit." << std::endl;
    }
}

/* takes a message, an optional error code and an optional file descriptor
   and handle the error by proper printing the message and potentionaly the
   standard system error message. It may also close the socket and exit if
   the error was not only an alert to the user
*/
void handleError1(std::string msg, exit_code err, int fd) {
    if (isRunning) { /// isRunning flag prevents unnecessary error handling while signal handler is shutting down
        std::cout << msg << std::endl;
        if (fd > 0) close(fd); /// fd has a default value of -1
        if (err != ALERT) {
            strerror(errno);
            exit(err);
            /// non of the scenarios that call this function needs to close `interfaceMonitor`
            /// because either the `interfaceMonitor` process has not been created yet, or
            /// the error is not general and does not try to make `networkMonitor` to exit
        }
    }
}

// simply concatinate two messages and call the handleError1
void handleError2(std::string msg1, std::string msg2, exit_code err, int fd) {
    if (isRunning) { /// isRunning flag prevents unnecessary error handling while signal handler is shutting down
        std::string msg = msg1 + " " + msg2;
        handleError1(msg, err, fd);
    }
}
