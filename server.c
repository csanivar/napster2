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
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>


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

in_port_t get_in_port(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

struct remote_file {
    char peer_ip[INET6_ADDRSTRLEN];
    char peer_port[8];
    char file_name[256]; // File name as published by the peer
    char file_location[256]; // Location of the file at peer
};

struct matched_list {
    struct remote_file* matches;
    int num_matches;
};

struct remote_file* file_list;
int* num_files;

struct matched_list fetch_file(char* file_name, char peer_ip[], char peer_port[]) {
    // printf("num_files: %d\n", *num_files);
    int i = 0, num_matches = 0;
    struct remote_file* matches = malloc(100*sizeof(struct remote_file));
    for(i; i<*num_files; i++) {
        // printf("file_name: %s\n", file_list[i].file_name);
        if(0 == strcmp(file_name, file_list[i].file_name)) {
            if(!(0 == strcmp(peer_ip, file_list[i].peer_ip) && 0 == strcmp(peer_port,file_list[i].peer_port))) {
                matches[num_matches] = file_list[i];
                num_matches++;
            }
        }
    }
    // printf("matches: %d\n", matches);
    struct matched_list result;
    result.matches = matches;
    result.num_matches = num_matches;
    return result; 
}

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

    file_list = mmap(NULL, 256*sizeof(struct remote_file), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    num_files = mmap(NULL, sizeof(*num_files), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    *num_files = 0;

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
            printf("new fd: %d\n", new_fd);
            perror("accept");
            continue;
        }

        if(!fork()) {
            close(sockfd);

            char peer_ip[INET6_ADDRSTRLEN];
            char peer_port[8];

            inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), peer_ip, sizeof peer_ip);
            
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
                    if(-1 == send(new_fd, "sure", 4, 0)) {
                        perror("send");
                    }
                } else if(0 == strcmp(action_req, "add")) {
                    char* peer_port_str = strtok(NULL, " ");
                    strcpy(peer_port, peer_port_str);
                    printf("server: got connection from %s:%s\n", peer_ip, peer_port_str);
                    // if(-1 == send(new_fd, "added", 5, 0)) {
                    //     perror("send");
                    // }
                } else if(0 == strcmp(action_req, "disconnect")) {
                    close(new_fd);
                    exit(0);
                } else if(0 == strcmp(action_req, "publish")) {
                    char* file_name = strtok(NULL, " ");
                    char* file_location = strtok(NULL, " ");
                    struct remote_file file_received;

                    strcpy(file_received.peer_port, peer_port);
                    strcpy(file_received.peer_ip, peer_ip);
                    strcpy(file_received.file_name, file_name);
                    strcpy(file_received.file_location, file_location);
                    file_list[*num_files] = file_received;
                    *num_files = *num_files + 1;

                    printf("file_addr: %s:%s\n", peer_ip, peer_port); 
                    printf("file_name: %s\n", file_name);
                    printf("file_location: %s\n", file_location);
                } else if(0 == strcmp(action_req, "fetch")) {
                    char* file_name = strtok(NULL, " ");
                    if(0 != strlen(file_name)) {
                        struct matched_list matched_list = fetch_file(file_name, peer_ip, peer_port);
                        int res_code;
                        if(0 == matched_list.num_matches) {
                            res_code = 404;
                        } else {
                            res_code = 200;
                        }
                        // printf("res_code: %d\n", res_code);
                        // printf("htonl res_code: %d\n", htonl(res_code));
                        // printf("ntohl res_code: %d\n", ntohl(htonl(res_code)));
                        int res_code_conv = htonl(res_code);
                        if(-1 == send(new_fd, &res_code_conv, sizeof res_code_conv, 0)) {
                            perror("send");
                        }

                        if(200 == res_code) {
                            struct remote_file fetch_result = matched_list.matches[0];

                            char* data = (char*)malloc(sizeof(struct remote_file));
                            memcpy(data, &fetch_result, sizeof(struct remote_file));
                            
                            // struct remote_file temp;
                            // memcpy(&temp, data, sizeof(struct remote_file));
                            // printf("temp: %s\n", temp.file_location);
                            // printf("temp total: %s:%d\n", temp.peer_ip, peer_port);
                            printf("file_location: %s\n", fetch_result.file_location);
                            printf("file_ip: %s\n", fetch_result.peer_ip);
                            printf("file_port: %s\n", fetch_result.peer_port);

                            int bytes_sent;
                            if(-1 == (bytes_sent = send(new_fd, data, sizeof(struct remote_file), 0))) {
                                perror("send");
                            }
                            printf("bytes_sent: %d\n", bytes_sent);
                        }
                    } else {
                        printf("BAD REQUEST");
                    }
                }
            }
        }

        close(new_fd);
    }

    return 0;
}