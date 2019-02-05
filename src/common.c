// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th 2019
// assignment 2

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <inttypes.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include "common.h"

void print_msg(struct msg *msg) {
    printf("%"PRIu32" - '%s'\n", msg->seq, msg->payload);
}

void print_msgs(struct msg_queue *msg_q) {
    for (int i = 0; i < msg_q->max_size; i++) {
        if (msg_q->msgs[i] != NULL) {
            print_msg(msg_q->msgs[i]);
        }
    }
}

/*
 * Deserialize a buffer into a message struct.
 * Memory for the struct should be allocated by the caller, but the payload
 * buffer will be malloc'd inside the method and should be free'd.
 * Return -1 if there is an error and 0 otherwise.
 */
int deserialize_msg(char *msg_buf, struct msg *msg) {
    memcpy((void*)&(msg->seq), msg_buf, sizeof (ack_t));
    msg->payload = strdup(msg_buf + sizeof (ack_t));
    return 0;
}

/*
 * Serialize a message into a buffer.  
 * Memory for the buffer is allocated in the function, and must be freed.
 * If there is an error return NULL, otherwise return the pointer to the newly
 * serialized message.
 */
char *serialize_msg(struct msg *msg) {
    char *ser_msg;

    ser_msg = malloc(sizeof (ack_t) + strlen(msg->payload));
    if (NULL == ser_msg) {
        perror("sender: malloc");
        return NULL;
    }
    // copy over the ack
    memcpy(ser_msg, (void*)&(msg->seq), sizeof (ack_t));
    // copy over the message.
    memcpy(ser_msg + sizeof (ack_t), msg->payload, strlen(msg->payload));

    return ser_msg;
}

/*
 * Initialize the message queue.
 * Return -1 if the request is invalid or if there is an error.  On success,
 * return 0.
 */
int init_msg_q(struct msg_queue *msg_q, uint32_t window_size) {
    if (NULL == msg_q) {
        fprintf(stderr, "msg_q can't be null.\n");
        return -1;
    }
    msg_q->msgs = malloc(window_size * sizeof (struct msg*));
    if (NULL == msg_q->msgs) {
        perror("init_msg_q: malloc");
        return -1;
    }
    bzero((void*) msg_q->msgs, window_size * sizeof (struct msg*));
    msg_q->max_size = window_size;
    msg_q->curr_size = 0;
    msg_q->curr_msg = 0;

    return 0;
}

/** 
 * Get the queue index for the next message.
 */
int _get_next_put(struct msg_queue *msg_q) {
    return (msg_q->curr_msg+1) % msg_q->max_size;
}

/** 
 * Add a message to the message buffer
 * The msg parameter is copied and the pointer kept internally.  The msg
 * parameter can therefore be free'd without messing up the queue.
 * The message queue size is incremented and the index of the newest message is
 * set.
 * If the request is invalid or if there is an error, return -1.  On success,
 * return 0.
 */
int add_msg(struct msg_queue *msg_q, struct msg *msg) {
    struct msg *msg_cpy;

    if (NULL == msg_q || NULL == msg_q->msgs) {
        fprintf(stderr, "Message queue failed to properly initialize.\n");
        return -1;
    }
    if (NULL == msg) {
        fprintf(stderr, "msg is NULL.\n");
        return -1;
    }
    if (msg_q->curr_size == msg_q->max_size) {
        fprintf(stderr, "queue is full.\n");
        return -1;
    }
    if (NULL == (msg_cpy = malloc(sizeof (struct msg)))) {
        perror("add_msg: malloc");
        return -1;
    }
    msg_cpy->payload = strdup(msg->payload);
    msg_cpy->seq = msg->seq;

    msg_q->msgs[msg_q->curr_msg] = msg_cpy;
    msg_q->curr_size += 1;
    msg_q->curr_msg = (msg_q->curr_msg + 1) % msg_q->max_size;

    return 0;
}

/** 
 * Remove the specified message from the queue.
 * Return -1 if the request is invalid or if the message does not exist.
 * Otherwise, return 0.
 */
int _rmv_msg(struct msg_queue *msg_q, ack_t msg_ack) {
    if (NULL == msg_q || NULL == msg_q->msgs) {
        fprintf(stderr, "Message queue failed to properly initialize.\n");
        return -1;
    }
    if (0 == msg_q->curr_size) {
        fprintf(stderr, "Messages queue is empty.\n");
        return -1;
    }
    if (msg_ack >= msg_q->max_size) {
        fprintf(stderr, "ACK %u is greater than message queue maximum size.\n", msg_ack);
        return -1;
    }
    if (NULL == msg_q->msgs[msg_ack]) {
        fprintf(stderr, "Msg from ACK %u does not exist.\n", msg_ack);
        return -1;
    }
    free(msg_q->msgs[msg_ack]->payload);
    free(msg_q->msgs[msg_ack]);
    msg_q->msgs[msg_ack] = NULL;
    msg_q->curr_size -= 1;

    return msg_ack;
}

int rmv_msg(struct msg_queue *msg_q) {
    ack_t oldest = (msg_q->curr_msg - msg_q->curr_size + msg_q->max_size) % msg_q->max_size;
    return _rmv_msg(msg_q, oldest);
}

/*
 * Free all the messages in the message queue.
 */
void free_msgs_q(struct msg_queue *msg_q) {
    for (uint32_t i = 0; i < msg_q->max_size; i++) {
        _rmv_msg(msg_q, i);
    }
}

/**
 * From Beej's Networking guide.
 */
char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            strncpy(s, "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}

/**
 * From Beej's Guide
 * get sockaddr, IPv4 or IPv6:
 */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) { // IPv4
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr); // IPv6
}
