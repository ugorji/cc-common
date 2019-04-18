#include <string>
#include "conn.h"

namespace ugorji { 
namespace conn { 

// Mock implementation of conn.h.
// Used to give us temporary cross-platform builds.

void Server::work() {
}

void Server::waitFor(bool joinThreads, bool untilStopped) {
}
    
void Server::readClientFd(int fd) {
}

void Server::writeClientFd(int fd, slice_bytes* sb) {
}

void Server::runLoop() {}

bool Server::shuttingDown() {
}

void Server::addFd(int sockfd, std::string* err) {
}

void Server::removeFd(int sockfd, std::string* err) {
}

void Server::acceptClients(bool continueOnError, std::string* err) {
}

void Server::start(std::string* err) {
}

void Server::stop() {
}

Server::~Server() {
    stop();
}

} //close namespace conn
} // close namespace ugorji

