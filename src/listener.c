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
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "common.h"
#include "msg_queue.h"

#define MYPORT "32000"
#define USAGE "Usage: %s <percent chance of loss> <r max>"

uint32_t PROB_LOSS = 0;

bool _init_args(int argc, char **argv, uint32_t *r_max) {
    if (argc != 3) {
        return false;
    }
    if (0 >= sscanf(argv[1], "%u", &PROB_LOSS)) {
        fprintf(stderr, "Failed to parse %s.\n", argv[1]);
        return false;
    }
    if (PROB_LOSS > 99) {
        fprintf(stderr, "Invalid probability loss %s.\n", argv[1]);
        return false;
    }

    if (0 >= sscanf(argv[2], "%u", r_max)) {
        fprintf(stderr, "Failed to parse %s.\n", argv[2]);
        return false;
    }

    return true;
}

int _send_ack(ack_t ack, int sockfd, struct sockaddr_storage *their_addr, socklen_t their_addr_len) {
    int numbytes = 0;

    if ((rand() % 100) < PROB_LOSS) {
        fprintf(stderr, "ACK lost...\n");
        return numbytes;
    }
    if (-1 == (numbytes =
               sendto(sockfd, (void*)&ack, sizeof (ack_t), 0,
               (struct sockaddr *)their_addr,
               their_addr_len))) {
        perror("listener: sendto");
        exit(1);
    }

    return numbytes;
}

bool _msg_correct(struct msg *msg, ack_t ack) {
    return msg->seq == ack;
}

bool _msg_retransmission(struct msg *msg, ack_t ack) {
    return (msg->seq == ack - 1);
}

bool _msg_uncorrupted() {
    char *msg_buf = NULL;
    size_t max = MAXLINE;
    int num_bytes = 0;
    msg_buf = malloc(MAXLINE);
    while(1 >= num_bytes) {
        printf("Is this message uncorrupted? [Y/N]:\n");
        num_bytes = getline(&msg_buf, &max, stdin);
        if (1 == num_bytes) {
            fprintf(stderr, "Invalid input, try again:\n");
        }
        else if (1 > num_bytes) {
            perror("Failed to read input in sender: get_line:");
        }
    }
    bool ret = msg_buf[0] == 'Y' || msg_buf[0] == 'y';
    free(msg_buf);
    return ret;
}

void listen_loop(int sockfd, uint32_t r_max) {
    int numbytes = 0;
    char msg_buf[MAXLINE] = { 0 };
    struct sockaddr_storage their_addr;
    socklen_t their_addr_len = 0;
    struct msg msg;
    ack_t earliest_ack = 0; // This is the initial ack
    struct msg_queue msg_q; // to buffer the received messages

    if (-1 == init_msg_q(&msg_q, r_max)) {
        exit(EXIT_FAILURE);
    }

    printf("listener: waiting to recvfrom...\n");
    their_addr_len = sizeof their_addr;
    while(1) {
        bzero(msg_buf, MAXLINE);
        if (-1 == (numbytes = recvfrom(
                       sockfd, msg_buf, MAXLINE-1 , 0,
                       (struct sockaddr *)&their_addr,
                       &their_addr_len))) {
            perror("recvfrom");
            exit(1);
        }

        printf("listener: got packet...\n");

        deserialize_msg(msg_buf, &msg);

        print_msg(&msg);
        if (_msg_correct(&msg, earliest_ack)) { // msg has expected ack
            if (_msg_uncorrupted()) {
                // The ack we send here should reflect the sequence number of
                // the earliest message we have not yet received.  This means
                // looking through the messages we have buffered.  All messages
                // in sequence should be deleted from the queue.
                earliest_ack = get_earliest_unrcvd(&msg_q, earliest_ack);
                // Remove all message up to but not including 'earliest ack'
                if (-1 == rmv_msgs(&msg_q, earliest_ack)) {
                    fprintf(stderr, "Failed to remove buffered messages.\n");
                }
                // send the next expected message.
                _send_ack(earliest_ack, sockfd, &their_addr, their_addr_len);
                printf("listener: sent ack %u\n", earliest_ack);
            }
        }
        else if (_msg_retransmission(&msg, earliest_ack)) { // msg is a retransmission
            // If the message is a retransmission, we should still send the
            // sequence number of the earliest message we have not yet received.
            fprintf(stderr, " ^^^ RE-TRANSMISSION ^^^\n");
            _send_ack(earliest_ack, sockfd, &their_addr, their_addr_len);
            printf("listener: sent ack %u\n", msg.seq);
        }
        else { // msg is out of order
            // The message is out of order, so it should be buffered in the
            // message queue.  If the queue is full, then it must be dropped.
            // If the sequence number of the message is greater than the value
            // of the earliest message we have not yet received by a difference
            // of r_max, then we cannot buffer the message, because the queue is
            // full.
            fprintf(stderr, " ^^^ OUT OF ORDER ^^^\n");
            if (-1 == add_msg(&msg_q, &msg)) {
                fprintf(stderr, "Failed to buffer message %u.\n", msg.seq);
            }
        }
        free(msg.payload);
    }
}

int main(int argc, char *argv[]) {
    int sockfd, status;
    struct addrinfo hints, *server_addr, *p;
    uint32_t r_max = 0;

    // initialize pseudo-random seed
    srand(time(NULL));

    if (!_init_args(argc, argv, &r_max)) {
        fprintf(stderr, USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }
    else {
        printf("Using loss probability of %u/100 r max of %u.\n", PROB_LOSS, r_max);
    }

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

    listen_loop(sockfd, r_max);

    close(sockfd);

    return 0;
}

