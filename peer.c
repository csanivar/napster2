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
#define BACKLOG 10

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

in_port_t get_in_port(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

int get_my_port(int listenfd) {
    struct sockaddr_in serv_addr;
    socklen_t len = sizeof(serv_addr);
    if (getsockname(listenfd, (struct sockaddr *)&serv_addr, &len) == -1) {
        perror("getsockname");
    } else {
        return ntohs(serv_addr.sin_port);
    }

    return -1;
}

struct remote_file {
    char peer_ip[INET6_ADDRSTRLEN];
    char peer_port[8];
    char file_name[256]; // File name as published by the peer
    char file_location[256]; // Location of the file at peer
};


int main(int argc, char* argv[]) {
    int sockfd, numbytes, new_fd, listenfd, my_port;
    char buf[MAX_DATA_SIZE], sendBuff[MAX_DATA_SIZE], args[256], args_c[256];
    struct sockaddr_in serv_addr;
    char* action;
    struct addrinfo hints, *servinfo, *p, *peerinfo;
    int rv;
    char s[INET6_ADDRSTRLEN];
    socklen_t sin_size;
    struct sockaddr_storage client_addr;

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

    //Make this peer listen on a port to let other peers connect to it
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff)); 

    //Bind to 'listenfd'
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 
    //Listen on 'listenfd'
    if(-1 == listen(listenfd, BACKLOG)) {
        perror("listen");
        exit(1);
    }

    //Get the port this peer is listening on
    my_port = get_my_port(listenfd);

    //Send the listening port to the server
    char* my_port_str = malloc(sizeof my_port);
    sprintf(my_port_str, "%d", my_port);

    char* conn_req = malloc(16);
    strcpy(conn_req, "add ");
    strcat(conn_req, my_port_str);
    send(sockfd, conn_req, sizeof(conn_req), 0);
    printf("sockfd1: %d\n", sockfd);
    
    //Do the listening in a child process
    if(!fork()) {
        close(sockfd);
        while(1) {
            sin_size = sizeof client_addr;
            new_fd = accept(listenfd, (struct sockaddr *)&client_addr, &sin_size);
            
            if(-1 == new_fd) {
                perror("accept");
                continue;
            }

            if(!fork()) {
                close(listenfd);

                char peer_ip[INET6_ADDRSTRLEN];
                int peer_port;

                inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), peer_ip, sizeof peer_ip);
                peer_port = ntohs(get_in_port((struct sockaddr *)&client_addr));
                printf("server: got connection from %s:%d\n", peer_ip, peer_port);
            }
        }
    }

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
                    char* my_port_str = malloc(sizeof my_port);
                    sprintf(my_port_str, "%d", my_port);

                    char* conn_req = malloc(16);
                    strcpy(conn_req, "connect ");
                    strcat(conn_req, my_port_str);
                    send(sockfd, conn_req, 16, 0);
                    if(-1 == send(sockfd, conn_req, 16, 0)) {
                        perror("send");
                    }

                    buf[0] = 0;
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
                        printf("sockfd2: %d\n", sockfd);
                        if(-1 == recv(sockfd, &res_code, 16, 0)) {
                            perror("peer: recv");
                        }
                        printf("peer: received res_code '%d'\n", res_code);
                        printf("peer: received res_code '%d'\n", ntohl(res_code));

                        if(200 == ntohl(res_code)) {
                            if(-1 == (numbytes = recv(sockfd, buf, MAX_DATA_SIZE-1, 0))) {
                                perror("recv");
                                exit(1);
                            }
                            struct remote_file fetch_result;
                            memcpy(&fetch_result, buf, numbytes);
                            
                            printf("file addr: %s:%s\n", fetch_result.peer_ip, fetch_result.peer_port);
                            printf("file location: %s\n", fetch_result.file_location);
                            
                        }
                    }
                }
                exit(0);
            }
        }
    }

    return 0;
}
