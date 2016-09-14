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

#define MAX_DATA_SIZE 10000

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct remote_file {
    struct sockaddr_storage file_addr; // Remote address/location of the file
    char file_name[256]; // File name as published by the peer
    char file_location[256]; // Location of the file at peer
};

int main(int argc, char* argv[]) {
    int sockfd, numbytes;
    char buf[MAX_DATA_SIZE], args[256], args_c[256];
    char* action;
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

    while(1) {
        args[0] = 0; // Empty the string array
        scanf("%[^\n]%*c", args);
        strncpy(args_c, args, sizeof args);
        if(0 != strlen(args)) {
            printf("args len: %d\n", strlen(args));
            if(!fork()) {
                action = strtok(args_c, " ");
                if(0 == strcmp("add", action)) {
                    buf[0] = 0;
                    if(-1 == send(sockfd, "connect", 7, 0)) {
                        perror("send");
                    }

                    if(-1 == (numbytes = recv(sockfd, buf, MAX_DATA_SIZE-1, 0))) {
                        perror("peer: recv");
                        exit(1);
                    }
                    buf[numbytes] = '\0';
                    printf("peer: received '%s'\n", buf);
                } else if(0 == strcmp("publish", action)) {
                    char* file_name = strtok(NULL, " ");
                    char* file_location = strtok(NULL, " ");
                    if(0 != strlen(file_name) && 0 != strlen(file_location)) {
                        if(-1 == send(sockfd, args, strlen(args), 0)) {
                            perror("send");
                        }
                    } else {
                        printf("'publish' called without a file name or file location. Use 'help' to see proper usage");
                    }
                } else if(0 == strcmp("fetch", action)) {
                    char* file_name = strtok(NULL, " ");
                    if(0 == strlen(file_name)) {
                        printf("'fetch' called without a file name. Use 'help' to see proper usage");
                    } else {
                        if(-1 == send(sockfd, args, strlen(args), 0)) {
                            perror("send");
                        }

                        int res_code;
                        if(-1 == recv(sockfd, &res_code, sizeof res_code, 0)) {
                            perror("peer: recv");
                        }
                        printf("peer: received '%d'\n", ntohl(res_code));

                        if(200 == ntohl(res_code)) {
                            int num_matches_conv;
                            if(-1 == recv(sockfd, &num_matches_conv, sizeof num_matches_conv, 0)) {
                                perror("peer: recv");
                                exit(1);
                            }

                            if(-1 == (numbytes = recv(sockfd, buf, MAX_DATA_SIZE-1, 0))) {
                                perror("recv");
                                exit(1);
                            }
                            printf("numbytes: %d\n", numbytes);
                            printf("sizerrerwer: %d\n", ntohl(num_matches_conv)*sizeof(struct remote_file));
                            struct remote_file* fetch_result = malloc(sizeof buf);
                            printf("peer received: %s\n", &buf);
                            memcpy(&fetch_result, buf, sizeof buf);
                            // printf("fetch size: %d\n", sizeof fetch_result);
                            printf("file location: %s\n", fetch_result[0].file_location);
                            printf("its loaded\n");
                        }
                    }
                }
                exit(0);
            }
        }
    }

    return 0;
}
