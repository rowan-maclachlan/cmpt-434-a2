// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th 2019
// assignment 2

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "common.h"
#include "msg_queue.h"

#define PORT_SIZE 16
#define IP_SIZE 64
#define USAGE "Usage: %s <ip address> <port number> <timeout> <window size>"
#define STDIN 0

// Used to determine message timeouts
uint32_t _TIMEOUT = 0;

struct timeval poll_freq = { 1, 0 };

bool _parse_arg(const char *arg, const char *format, void *param) {
    if (0 >= sscanf(arg, format, param)) {
        fprintf(stderr, "Failed to parse %s.\n", arg);
        return false;
    }
    return true;
}

bool _init_args(int argc, char **argv, char *ip_address, char *port, uint32_t *timeout, ack_t *window_size) {
    ack_t max_window_size = 0;

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

    if (!_parse_arg(argv[3], "%lu", timeout)) {
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
    max_window_size = (ACKSIZE) - 1;
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

int _send_msg(struct msg *msg, int sockfd, struct sockaddr *p) {
    char *ser_msg = NULL;
    int num_bytes = 0;

    if (NULL == (ser_msg = serialize_msg(msg))) {
        fprintf(stderr, "Failed to serialized message.\n");
        return -1;
    }

    if (-1 == (num_bytes = sendto(sockfd,
                                  ser_msg,
                                  strlen(msg->payload) + sizeof (ack_t),
                                  0, p, sizeof(*p)))) {
        free(ser_msg);
        perror("sender: sendto");
        fprintf(stderr, "Failed to send message to receiver.\n");
        return -1;
    }
    free(ser_msg);

    printf("Send message of %d bytes  with ack %u.\n", num_bytes, msg->seq);

    return num_bytes;
}

int _resend_msg(struct msg_queue *msg_q, struct msg *msg, ack_t seq, int sockfd, struct sockaddr *p) {
    if (0 != get_msg_cpy(msg_q, msg, seq)) {
        fprintf(stderr, "ERROR Failed to get message %u.\n", seq);
        return -1;
    }
    if (0 >= _send_msg(msg, sockfd, p)) {
        fprintf(stderr, "ERROR Failed to send message %u.\n", seq);
        return -1;
    }
    return 0;
}

int _resend_msgs(ack_t expected_ack, struct msg_queue *msg_q, int sockfd, struct sockaddr *p) {
    struct msg msg;
    for (ack_t seq = expected_ack; seq < expected_ack + msg_q->curr_size; seq++) {
        _resend_msg(msg_q, &msg, seq, sockfd, p);
        free(msg.payload);
    }

    return 0;
}

void send_loop(int sockfd, char *port, struct sockaddr *p, uint32_t window_size) {
    struct msg_queue msg_q;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char msg_buf[MAXLINE] = { 0 };
    struct msg msg;
    fd_set master_set;
    fd_set tmp_set;
    int ret_val = 0;
    time_t timeout = 0;
    bool timeout_set = false;

    init_msg_q(&msg_q, window_size);

    FD_ZERO(&master_set);
    FD_ZERO(&tmp_set);
    FD_SET(STDIN, &master_set);
    FD_SET(sockfd, &master_set);

    // The ack we are timing out on depends on the ack sent by the receiver.
    // That value will be the next sequence number they are expecting to
    // receive.  Is the ack we are timing out on simply 1 less than the received
    // ack?
    ack_t rcvd_ack = 0;
    ack_t timeout_ack = 0; // Which ack are we timing out on?
    ack_t msg_seq = 0; // Which sequence number are we sending next?
    printf("Enter input line for a message:\n");
    while(1) {
        tmp_set = master_set;

        if (-1 == (select(sockfd+1, &tmp_set, NULL, NULL, &poll_freq))) {
            perror("sender: select:");
            continue;
        }

        if (FD_ISSET(STDIN, &tmp_set)) {
            _get_line(msg_buf);
            msg.seq = msg_seq;
            msg.payload = strdup(msg_buf);
            if (-1 == add_msg(&msg_q, &msg)) {
                fprintf(stderr, "Failed to add message %u to buffer.\n", msg_seq);
                continue;
            }

            if (0 >= _send_msg(&msg, sockfd, p)) {
                fprintf(stderr, "Failed to send message %u.\n", msg_seq);
                if (msg_seq != rmv_newest_msg(&msg_q)) {
                    fprintf(stderr, "Failed to remove message %u from queue.\n", msg_seq);
                    exit(1);
                }

                free(msg.payload);
                continue;
            }
            free(msg.payload);

            // We successfully sent a message.  If its the first frame of our
            // window, then we want to set the timeout for it
            if (msg_seq == timeout_ack) {
                printf("New message, setting timeout.\n");
                timeout = time(NULL);
                timeout_set = true;
            }

            msg_seq++;

        }
        else if (FD_ISSET(sockfd, &tmp_set)) {
            addr_len = sizeof their_addr;
            if (-1 == _get_ack(sockfd, &rcvd_ack, &their_addr, &addr_len)) {
                fprintf(stderr, "Failed to get ack.\n");
                continue;
            }
            printf("Got ack %u\n", rcvd_ack);
            // If the ack we received is greater than the timeout ack,
            // then we can reset the timer.  Then, the timeout ack becomes the
            // value of the last received ack.
            if (rcvd_ack > timeout_ack) {
                timeout_ack = rcvd_ack;
                // reset timeout
                timeout = time(NULL);
                printf("Received correct ack, resetting timeout.\n");
                // remove all messages with sequence values less than (not
                // quite) the received ack.
                if (-1 == (ret_val = rmv_msgs(&msg_q, rcvd_ack))) {
                    fprintf(stderr,
                            "Failed to remove all acked messages from queue.\n");
                }
                // if there are other messages on the queue,
                // keep the timeout set for them
                if (msg_q.curr_size > 0) {
                    printf("More messages in the buffer, setting timeout...\n");
                    timeout_set = true;
                }
                // If there are no other messages on the queue,
                // then we should not timeout for anything
                else {
                    printf("No more messages in the buffer, unsetting timeout.\n");
                    timeout_set = false;
                }
                printf("Removed message %d from buffer.\n", ret_val);
            }
            else {
                fprintf(stderr, "Out of order ack %u - expected %u.\n", rcvd_ack, timeout_ack);
            }
        }

        // if timeout expired,
        // The timeout expired if the current time minus the time the message
        // was sent is greater than the timeout the user requested.
        if (timeout_set && (_TIMEOUT < (time(NULL) - timeout))) {
            fprintf(stderr, "TIMEOUT\n");
            // resend full window.  Reset timer
            timeout = time(NULL);
            // In go-back-n, I only want to resend messages selectively
            if (-1 == _resend_msg(&msg_q, &msg, timeout_ack, sockfd, p)) {
                fprintf(stderr, "Failed to resend message %u.\n", timeout_ack);
            }
        }
    }
}

int main(int argc, char **argv) {
    int sockfd, status;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr dest;
    char port[PORT_SIZE] = { 0 };
    char ip_address[IP_SIZE] = { 0 };
    ack_t window_size = 0;

    if (!_init_args(argc, argv, ip_address, port, &_TIMEOUT, &window_size)) {
        fprintf(stderr, USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }
    else {
        printf("Using ip_address %s and port %s, timeout %u and window size %u.\n",
               ip_address, port, _TIMEOUT, window_size);
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

    memcpy(&dest, p->ai_addr, sizeof(*p->ai_addr));
    free(servinfo);

    printf("sender: sending messages to %s\n", ip_address);
    send_loop(sockfd, port, &dest, window_size);

    close(sockfd);

    exit(EXIT_SUCCESS);
}
