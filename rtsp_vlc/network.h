#ifndef NETWORK_H
#define NETWORK_H

int createTcpSocket();

int createUdpSocket();
int bindSocketAddr(int sockfd, const char* ip, int port);
int acceptClient(int sockfd, char* ip, int* port);
int serverSocketAddr(int sockfd);
#endif // NETWORK_H
