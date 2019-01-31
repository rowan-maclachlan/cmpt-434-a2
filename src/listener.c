// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th 2019
// assignment 2

#include <arpa/inet.h>
#include <errno.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "common.h"

#define MYPORT "8080"

void listen_loop(int sockfd) {
    int numbytes = 0;
    char s[INET6_ADDRSTRLEN] = { 0 };
    char msg_buf[MAXLINE] = { 0 };
    char *msg = "ACK";
    struct sockaddr_storage their_addr;
    socklen_t their_addr_len = 0;

    printf("listener: waiting to recvfrom...\n");
    their_addr_len = sizeof their_addr;
    while(1) {
        if (-1 == (numbytes = recvfrom(
                       sockfd, msg_buf, MAXLINE-1 , 0,
                       (struct sockaddr *)&their_addr,
                       &their_addr_len))) {
            perror("recvfrom");
            exit(1);
        }

        printf("listener: got packet from %s\n",
                inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));

        printf("listener: packet is %d bytes long\n", numbytes);
        msg_buf[numbytes] = '\0';
        printf("listener: packet contains \"%s\"\n", msg_buf);

        if (-1 == (numbytes =
                   sendto(sockfd, msg, strlen(msg), 0,
                   (struct sockaddr *)&their_addr,
                   their_addr_len))) {
            perror("listener: sendto");
            exit(1);
        }
        printf("listener: sent %d bytes to %s\n", numbytes, s);
    }
}

int main() {
    int sockfd, status;
    struct addrinfo hints, *server_addr, *p;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((status = getaddrinfo(NULL, MYPORT, &hints, &server_addr)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }
    
    // loop through all the results and bind to the first we can
    for(p = server_addr; p != NULL; p = p->ai_next) {
        if (-1 == (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))) {
            perror("listener: socket");
            continue; 
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break; 
    }
    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(server_addr);

    char ip[INET6_ADDRSTRLEN] = { 0 };
    get_ip_str(p->ai_addr, ip, INET6_ADDRSTRLEN);
    printf("Listening from IP address %s and on port %s.\n", ip, MYPORT);

    listen_loop(sockfd);

    close(sockfd);
    
    return 0;
}

