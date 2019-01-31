// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th 2019
// assignment 2

#include <arpa/inet.h>
#include <errno.h> 
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "common.h"

#define PORT_SIZE 16 
#define IP_SIZE 64
#define USAGE "Usage: %s <ip address> <port number> <timeout> <window size>"

bool _parse_arg(const char *arg, const char *format, void *param) {
    if (0 >= sscanf(arg, format, param)) {
        fprintf(stderr, "Failed to parse %s.\n", arg);
        return false;
    }
    return true;
}

bool _init_args(int argc, char **argv, char *ip_address, char *port, uint32_t *timeout, uint32_t *window_size) {
    uint16_t max_window_size = 0;

    if (argc != 5) {
        fprintf(stderr, USAGE, argv[0]);
        return false;
    }
    if (!_parse_arg(argv[1], "%s", ip_address)) {
        fprintf(stderr, "Invalid IP.\n");
        return false;
    }
    ip_address[IP_SIZE-1] = '\0';
    if (!_parse_arg(argv[2], "%s", port)) {
        fprintf(stderr, "Invalid port.\n");
        return false;
    }

    port[PORT_SIZE-1] = '\0';
    if (!_parse_arg(argv[3], "%u", timeout)) {
        fprintf(stderr, "Invalid timeout.\n");
        return false;
    }
    if (0 == *timeout) {
        fprintf(stderr, "Invalid timeout.\n");
        return false;
    }
    if (!_parse_arg(argv[4], "%u", window_size)) {
        fprintf(stderr, "Invalid window size.\n");
        return false;
    }
    /* The max window size is the largest value we can fit in SEQ_BYTES bits.
     * If shift left 1 that many times, we can subtract 1 to get the max window. */
    max_window_size = (1 << (SEQ_BYTES * CHAR_BIT)) - 1;
    if (*window_size > max_window_size) {
        fprintf(stderr, "Window size too large.\n");
        return false;
    }

    return true;
}

int _get_line(char *msg_buf) {
    size_t max = MAXLINE;
    int num_bytes = 0;
    while(0 >= num_bytes) {
        printf("Enter input line for message:\n");
        num_bytes = getline(&msg_buf, &max, stdin); 
        if (0 >= num_bytes) {
            perror("Failed to read input in sender: get_line:");
        }
    }

    printf("sender: read input of %d bytes...\n", num_bytes);
    
    /* Overrite the newline character. */
    msg_buf[strcspn(msg_buf, "\n")] = '\0';

    return num_bytes;
}

void send_loop(int sockfd, char *port, struct addrinfo *p) {
    int num_bytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN] = { 0 };
    char msg_buf[MAXLINE] = { 0 };

    while(1) {
        num_bytes = _get_line(msg_buf);
        if (-1 == (num_bytes = sendto(
                        sockfd, msg_buf, num_bytes, 0, p->ai_addr, p->ai_addrlen))) {
            perror("sender: sendto");
            exit(1);
        }
        
        printf("sender: sent %d bytes...\n", num_bytes);
    
        addr_len = sizeof their_addr;
        if ((num_bytes = recvfrom(sockfd, msg_buf, MAXLINE-1 , 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
    
        printf("sender: got packet from %s\n",
                inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s));
    }
}

int main(int argc, char **argv) {
    int sockfd, status;
    struct addrinfo hints, *servinfo, *p;
    char port[PORT_SIZE] = { 0 };
    char ip_address[IP_SIZE] = { 0 };
    uint32_t timeout = 0;
    uint32_t window_size = 0;
    
    if (!_init_args(argc, argv, ip_address, port, &timeout, &window_size)) {
        fprintf(stderr, USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }
    else {
        printf("Using ip_address %s and port %s, timeout %d and window size %d.\n",
               ip_address, port, timeout, window_size);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((status = getaddrinfo(ip_address, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }
    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("sender: socket");
            continue; 
        }
        break; 
    }
    if (p == NULL) {
        fprintf(stderr, "sender: failed to create socket\n");
        return 2;
    }
    free(servinfo);

    printf("sender: sending messages to %s\n", ip_address);
    send_loop(sockfd, port, p);

    close(sockfd);
    
    exit(EXIT_SUCCESS);
}
