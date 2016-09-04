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

#define SERVER_PORT "9967"
#define BACKLOG 10
#define MAX_DATA_SIZE 200

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct remote_file {
    char addr[INET6_ADDRSTRLEN]; // Remote address/location of the file
    char file_name[256]; // File name as published by the peer
    char file_location[256]; // Location of the file at peer
};

int main(void) {
    int sockfd, new_fd, numbytes;
    char buf[MAX_DATA_SIZE], buf_c[MAX_DATA_SIZE]; // buffer and buffer copy
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    struct remote_file file_list[256];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rv = getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo)) !=0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = servinfo->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if(NULL == p) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if(-1 == listen(sockfd, BACKLOG)) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(-1 == sigaction(SIGCHLD, &sa, NULL)) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connection....\n");

    while(1) {
        sin_size = sizeof client_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);

        if(-1 == new_fd) {
            perror("accept");
            continue;
        }
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if(!fork()) {
            close(sockfd);
            while(1) {
                if(-1 == (numbytes = recv(new_fd, buf, MAX_DATA_SIZE-1, 0))) {
                    perror("server: recv");
                }
                if(0 == numbytes) {
                    close(new_fd);
                    exit(0);
                }

                buf[numbytes] = '\0';
                printf("server: received '%s'\n", buf);
                memcpy(buf_c, buf, sizeof buf);

                char* action_req = strtok(buf_c, " ");
                if(0 == strcmp(action_req, "connect")) {
                    if(-1 == send(new_fd, "Sure", 4, 0)) {
                        perror("send");
                    }
                } else if(0 == strcmp(action_req, "disconnect")) {
                    close(new_fd);
                    exit(0);
                } else if(0 == strcmp(action_req, "publish")) {
                    char* file_name = strtok(NULL, " ");
                    char* file_location = strtok(NULL, " ");
                    printf("file_name: %s\n", file_name);
                    printf("file_location: %s\n", file_location);
                }
            }
        }

        close(new_fd);
    }

    return 0;
}