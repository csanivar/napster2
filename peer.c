//
// Created by mouli on 8/24/16.
//
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAX_DATA_SIZE 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[]) {
    int sockfd, numbytes;
    char buf[MAX_DATA_SIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if(3 != argc) {
        fprintf(stderr, "usage: peer server_ip server_port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(0 != (rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if(-1 == (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))) {
            perror("peer: socket");
            continue;
        }

        if(-1 == connect(sockfd, p->ai_addr, p->ai_addrlen)) {
            close(sockfd);
            perror("peer: connect");
            continue;
        }

        break;
    }

    if(NULL == p) {
        fprintf(stderr, "peer: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
    printf("peer: connecting to %s\n", s);

    freeaddrinfo(servinfo);

    if(-1 == (numbytes = recv(sockfd, buf, MAX_DATA_SIZE-1, 0))) {
        perror("peer: recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("peer: received '%s'\n", buf);

    close(sockfd);

    return 0;
}
