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
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "common.h"

#define PORT_SIZE 16 
#define IP_SIZE 64
#define USAGE "Usage: %s <ip address> <port number> <timeout> <window size>"
#define STDIN 0

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
    while(1 >= num_bytes) {
        num_bytes = getline(&msg_buf, &max, stdin); 
        if (1 >= num_bytes) {
            perror("Failed to read input in sender: get_line:");
        }
    }

    printf("sender: read input of %d bytes...\n", num_bytes);
    
    /* Overrite the newline character. */
    msg_buf[strcspn(msg_buf, "\n")] = '\0';

    return num_bytes;
}

int _get_ack(int sockfd, ack_t *ack, struct sockaddr_storage *their_addr, socklen_t *addr_len) {
    int num_bytes;

    if (-1 == (num_bytes = recvfrom(
                    sockfd, (void*) ack, sizeof (ack_t) , 0, 
                    (struct sockaddr *)their_addr, addr_len))) {
        perror("recvfrom");
        return -1;
    }

    if (num_bytes != sizeof (ack_t)) {
        fprintf(stderr, "Failed to recieve ACK correctly.  Got %u.\n", *ack);
        return -1;
    }

    return 0;
}

void send_loop(int sockfd, char *port, struct addrinfo *p, uint32_t window_size) {
    struct msg_queue msg_q;
    int num_bytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char msg_buf[MAXLINE] = { 0 };
    char *ser_msg;
    struct msg msg;
    fd_set master_set;
    fd_set tmp_set;
    ack_t ack;

    init_msg_q(&msg_q, window_size);

    FD_ZERO(&master_set);
    FD_ZERO(&tmp_set);
    FD_SET(STDIN, &master_set);
    FD_SET(sockfd, &master_set);

    ack_t msg_seq = 0;
    while(1) {
        tmp_set = master_set;

        printf("Enter input line for a message:\n");

        if (-1 == (select(sockfd+1, &tmp_set, NULL, NULL, NULL))) {
            perror("sender: select:");
            continue;
        }

        if (FD_ISSET(STDIN, &tmp_set)) {
            _get_line(msg_buf);
            msg.seq = msg_seq;
            msg.payload = strdup(msg_buf);
            if (-1 == add_msg(&msg_q, &msg)) {
                fprintf(stderr, "Failed to add message to buffer.");
                continue;
            }
    
            if (NULL == (ser_msg = serialize_msg(&msg))) {
                fprintf(stderr, "Failed to serialized message.\n");
                continue;
            }
            free(msg.payload);
    
            if (-1 == (num_bytes = sendto(
                            sockfd, ser_msg, strlen(msg_buf) + sizeof (ack_t), 0, p->ai_addr, p->ai_addrlen))) {
                free(ser_msg);
                perror("sender: sendto");
                fprintf(stderr, "Failed to send message to receiver.\n");
                continue;
            }
            free(ser_msg);
        
            msg_seq = (msg_seq + 1) % msg_q.max_size;
            
            printf("sender: sent %d bytes...\n", num_bytes);
        }
        else if (FD_ISSET(sockfd, &tmp_set)) {
            addr_len = sizeof their_addr;
            if (-1 == _get_ack(sockfd, &ack, &their_addr, &addr_len)) {
                fprintf(stderr, "Failed to get ack.\n");
                continue;
            }
            printf("Got ack %u\n", ack);
    
            if (-1 == rmv_msg(&msg_q)) {
                fprintf(stderr, "Failed to remove message %u from message queue.\n", ack);
            }
            printf("Removed message %u from buffer.\n", ack);
        }
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
    send_loop(sockfd, port, p, window_size);

    close(sockfd);
    
    exit(EXIT_SUCCESS);
}
