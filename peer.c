//
// Created by mouli on 8/24/16.
// Acts as a peer in the p2p-system
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
#include <dirent.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_DATA_SIZE 10000
#define BACKLOG 10
#define FILE_BUF_SIZE 4096

// struct to store remote_file data
struct remote_file {
    char peer_ip[INET6_ADDRSTRLEN];
    char peer_port[8];
    char file_name[256]; // File name as published by the peer
    char file_location[256]; // Location of the file at peer
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// get the port number from the sockaddr
in_port_t get_in_port(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

// get the port assosciated with the listenfd
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

// helper to download given remote_file to the given download_path
void download_file(struct remote_file file_req, char* download_path) {
    struct addrinfo hints, *servinfo, *p;
    int rv, fetchfd;
    char s[INET6_ADDRSTRLEN];
    char* fetch_req = malloc(100);
    int f, n, file_cnt = 30;
    void* file_buf;
    char* file_path = malloc(100);

    file_buf = (void*)malloc(FILE_BUF_SIZE);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    printf("peer_ip: %s\n", file_req.peer_ip);
    printf("peer_port: %s\n", file_req.peer_port);
    if(0 != (rv = getaddrinfo(file_req.peer_ip, file_req.peer_port, &hints, &servinfo))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if(-1 == (fetchfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))) {
            perror("peer: socket");
            continue;
        }

        if(-1 == connect(fetchfd, p->ai_addr, p->ai_addrlen)) {
            close(fetchfd);
            perror("peer: connect");
            continue;
        }

        break;
    }

    if(NULL == p) {
        fprintf(stderr, "peer: failed to connect\n");
        return;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
    printf("peer: connecting to %s\n", s);
    freeaddrinfo(servinfo);

    strcpy(fetch_req, "download ");
    strcat(fetch_req, file_req.file_name);
    if(-1 == send(fetchfd, fetch_req, 100, 0)) {
        perror("send");
    }

    //Build the file_path where the file will be downloaded
    strcpy(file_path, download_path);
    strcat(file_path, "/");
    strcat(file_path, file_req.file_name);

    f = open(file_path, O_CREAT|O_RDWR, 0755);
    while(1) {
        n = read(fetchfd, file_buf, FILE_BUF_SIZE);
        if(n<=0) {
            break;
        }
        write(f, file_buf, n);
    }

    free(file_buf);
    close(f);
    close(fetchfd);
    printf("\n%s finished downloading\n", file_req.file_name);
}

void upload_file(int connfd, char* file_name, char* download_path) {
    char* file_path = malloc(100);
    int f, n;
    void* file_buf;

    file_buf = (void*)malloc(FILE_BUF_SIZE);

    strcpy(file_path, download_path);
    strcat(file_path, "/");
    strcat(file_path, file_name);

    printf("\nfile_path: %s\n", file_path);
    f = open(file_path, O_RDONLY);
    if(f<0) {
        printf("\nUnable to open requested file\n");
        return;
    }

    printf("\nUploading file: %s\n", file_name);
    while(1) {
        n = read(f, file_buf, FILE_BUF_SIZE);
        printf("n: %d\n", n);
        if(n<=0) {
            break;
        }
        send(connfd, (const void*)file_buf, n, 0);
    }
    printf("Upload finished\n");

    free(file_buf);
    close(f);
}

// publish the given file to the server.
void publish_file_to_server(int connfd, char* file_name, char* file_location) {
    if(0 != strlen(file_name) && 0 != strlen(file_location)) {
        char publ_req[200];
        strcpy(publ_req, "publish ");
        strcat(publ_req, file_name);
        strcat(publ_req, " ");
        strcat(publ_req, file_location);
        printf("publ_req: %s\n", publ_req);
        if(-1 == send(connfd, publ_req, 200, 0)) {
            perror("send");
        }
    } else {
        printf("file_name or file_location is invalid");
    }
}

// publish all the files in the given path to the server
void publish_files(int connfd, char* publish_path) {
    printf("\npublishing files started\n");
    DIR *d;
    struct dirent *dir;
    d = opendir(publish_path);
    if(d) {
        while((dir = readdir(d)) != NULL) {
            if(dir->d_type == DT_REG) {
                char* file_name = dir->d_name;
                char* file_location = malloc(100);
                strcpy(file_location, publish_path);
                strcat(file_location, "/");
                strcat(file_location, file_name);
                printf("file found: %s\n\n", file_name);
                publish_file_to_server(connfd, file_name, file_location);
                sleep(0.5);
            }
        }

        closedir(d);
    }
    printf("\npublishing files finished\n");
}

int main(int argc, char* argv[]) {
    int sockfd, numbytes, new_fd, listenfd, my_port;
    char buf[MAX_DATA_SIZE], args[256], args_c[256];
    struct sockaddr_in serv_addr;
    char* action;
    struct addrinfo hints, *servinfo, *p, *peerinfo;
    int rv;
    char s[INET6_ADDRSTRLEN];
    socklen_t sin_size;
    struct sockaddr_storage client_addr;
    char* download_path;

    if(4 != argc) {
        fprintf(stderr, "usage: peer <server_ip> <server_port> <abs_share_path>\n");
        exit(1);
    }

    download_path = argv[3];
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

    //Bind to 'listenfd'
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 
    //Listen on 'listenfd'
    if(-1 == listen(listenfd, BACKLOG)) {
        perror("listen");
        exit(1);
    }

    //Get the port this peer is listening on
    my_port = get_my_port(listenfd);
    printf("my_port: %d\n", my_port);

    //Send the listening port to the server
    char* my_port_str = malloc(sizeof my_port);
    sprintf(my_port_str, "%d", my_port);
    printf("my_port_str: %s\n", my_port_str);

    char* conn_req = malloc(20);
    strcpy(conn_req, "add ");
    strcat(conn_req, my_port_str);
    send(sockfd, conn_req, 20, 0);
    printf("conn_req: %s\n", conn_req);
    
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

                buf[0] = 0;
                if(-1 == (numbytes = recv(new_fd, buf, MAX_DATA_SIZE-1, 0))) {
                    perror("peer: recv");
                    exit(1);
                }
                buf[numbytes] = '\0';
                printf("peer: received '%s'\n", buf);

                char* action_req = strtok(buf, " ");
                if(0 == strcmp("download", action_req)) {
                    char* download_file_name = strtok(NULL, " ");
                    upload_file(new_fd, download_file_name, download_path);
                }
                exit(0);
            }
            close(new_fd);
        }
    } else {
        //Close the listenfd in the parent process as the child is doing the listening
        close(listenfd);

        sleep(0.5);
        //Publish files which you are sharing
        publish_files(sockfd, download_path);

        //Wait and read and respond to the user actions
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
                        if(0 == strcmp(file_name, "ALL")) {
                            publish_files(sockfd, download_path);
                        }
                        publish_file_to_server(sockfd, file_name, file_location);
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
                            if(-1 == (numbytes = recv(sockfd, &res_code, 16, 0))) {
                                perror("peer: recv");
                            }
                            printf("numbytes before 200: %d\n", numbytes);
                            printf("peer: received res_code '%d'\n", ntohl(res_code));

                            if(200 == ntohl(res_code)) {
                                memset(buf, 0, MAX_DATA_SIZE);
                                if(-1 == (numbytes = recv(sockfd, buf, MAX_DATA_SIZE-1, 0))) {
                                    perror("recv");
                                    exit(1);
                                }
                                printf("numbytes after 200: %d\n", numbytes);
                                struct remote_file fetch_result;
                                memcpy(&fetch_result, buf, numbytes);

                                printf("file addr: %s:%s\n", fetch_result.peer_ip, fetch_result.peer_port);
                                printf("file location: %s\n", fetch_result.file_location);
                                if(!fork()) {
                                    close(sockfd);
                                    download_file(fetch_result, download_path);
                                    exit(0);
                                }
                            } else if(400 == ntohl(res_code)) {
                                printf("\nUh-oh! Requested file is not found!!\n");
                            }
                        }
                    }
                    exit(0);
                }
            }
        }
    }

    return 0;
}
