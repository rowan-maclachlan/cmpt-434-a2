// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 11th 2019
// assignment 2

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <netdb.h>
#include <inttypes.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include "msg_queue.h"

#define MSG_Q_UNINIT "message queue not properly initialized."

#define MSG_Q_VAL(x) (NULL == x || NULL == x->msgs)

/*
 * Display a message's sequence number and payload as a string.
 */
void print_msg(struct msg *msg) {
    printf("%"PRIu32" - '%s'\n", msg->seq, msg->payload);
}

/*
 * Display the message contents of the message queue.
 */
void print_msgs(struct msg_queue *msg_q) {
    if (MSG_Q_VAL(msg_q)) {
        fprintf(stderr, "Error: print_msgs: %s\n", MSG_Q_UNINIT);
        return;
    }
    for (int i = 0; i < msg_q->max_size; i++) {
        if (msg_q->msgs[i] != NULL) {
            print_msg(msg_q->msgs[i]);
        }
        else {
            printf(" # - #\n");
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
    if (NULL == msg || NULL == msg->payload) {
        fprintf(stderr, "Message not properly initialized.\n");
        return NULL;
    }

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
 * Initialize the message queue.  The msg_q should be passed to free_msgs_q after
 * it is no longer needed.
 * Return -1 if the request is invalid or if there is an error.  On success,
 * return 0.
 */
int init_msg_q(struct msg_queue *msg_q, uint32_t window_size) {
    if (NULL == msg_q) {
        fprintf(stderr, "Error: init_msg_q: %s\n", MSG_Q_UNINIT);
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
    msg_q->next_msg = 0;

    return 0;
}

/** 
 * Get the queue index where we should put the next message.
 */
int _get_next_put(struct msg_queue *msg_q) {
    return (msg_q->next_msg+1) % msg_q->max_size;
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

    if (MSG_Q_VAL(msg_q)) {
        fprintf(stderr, "Error: add_msg: %s\n", MSG_Q_UNINIT);
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

    msg_q->msgs[msg_q->next_msg] = msg_cpy;
    msg_q->curr_size += 1;
    msg_q->next_msg = (msg_q->next_msg + 1) % msg_q->max_size;

    return 0;
}

/** 
 * Remove the specified message from the queue.
 * Return -1 if the request is invalid or if the message does not exist.
 * Otherwise, return the message sequence value of the removed message.
 */
int _rmv_msg(struct msg_queue *msg_q, ack_t window_offset) {
    ack_t msg_seq = 0;
    if (NULL == msg_q->msgs[window_offset]) {
        fprintf(stderr, "Msg from ACK %u does not exist.\n", window_offset);
        return -1;
    }
    msg_seq = msg_q->msgs[window_offset]->seq;
    free(msg_q->msgs[window_offset]->payload);
    free(msg_q->msgs[window_offset]);
    msg_q->msgs[window_offset] = NULL;
    msg_q->curr_size -= 1;

    return msg_seq;
}

/*
 * Remove the newest message from the queue.
 * Returns the sequence value of the message which was removed, or -1 on
 * failure.
 */
int rmv_newest_msg(struct msg_queue *msg_q) {
    if (MSG_Q_VAL(msg_q)) {
        fprintf(stderr, "Error: rmv_msg: %s\n", MSG_Q_UNINIT);
        return -1;
    }
    if (0 == msg_q->curr_size) {
        fprintf(stderr, "Messages queue is empty.\n");
        return -1;
    }
    ack_t newest = (msg_q->next_msg + msg_q->max_size - 1) % msg_q->max_size;
    ack_t msg_seq = _rmv_msg(msg_q, newest);
    msg_q->next_msg = newest;

    return msg_seq;
}

/*
 * Remove the oldest message from the queue.
 * Returns the sequence value of the message which was removed, or -1 on
 * failure.
 */
int rmv_msg(struct msg_queue *msg_q) {
    if (MSG_Q_VAL(msg_q)) {
        fprintf(stderr, "Error: rmv_msg: %s\n", MSG_Q_UNINIT);
        return -1;
    }
    if (0 == msg_q->curr_size) {
        fprintf(stderr, "Messages queue is empty.\n");
        return -1;
    }
    ack_t oldest = (msg_q->next_msg - msg_q->curr_size + msg_q->max_size) % msg_q->max_size;
    return _rmv_msg(msg_q, oldest);
}

ack_t get_earliest_unrcvd(struct msg_queue *msg_q, ack_t ack) {
    struct msg *msg;

    ack_t oldest_i = (msg_q->next_msg - msg_q->curr_size + msg_q->max_size) % msg_q->max_size;
    for (ack_t i = oldest_i; i < msg_q->curr_size + msg_q->max_size; i++) {
        int index = i % msg_q->max_size;
        if (NULL == (msg = msg_q->msgs[index])) {
            return ack + 1;
        }
        if (msg_q->msgs[index]->seq != ack + 1) {
            // then they are not in sequence, so ack+1 is the earliest
            // unreceived sequence number.
            return ack + 1;
        }
        ack++;
    }

    // If we never found an out of order value, then 
    return ack;
}

/*
 * Remove all messages with sequence values less that ack.
 * Returns the number of messages removed from the queue, or -1 on failure. 
 */
int rmv_msgs(struct msg_queue *msg_q, ack_t ack) {
    if (MSG_Q_VAL(msg_q)) {
        fprintf(stderr, "Error: rmv_msg: %s\n", MSG_Q_UNINIT);
        return -1;
    }
    if (0 == msg_q->curr_size) {
        fprintf(stderr, "Messages queue is empty.\n");
        return 0;
    }
    ack_t oldest = (msg_q->next_msg + msg_q->max_size - msg_q->curr_size) % msg_q->max_size;

    int num_msgs = 0;
    int orig_size = msg_q->curr_size;
    for (int i = oldest; i < oldest + orig_size; i++) {
        int index = i % msg_q->max_size;
        if (NULL == msg_q->msgs[index]) {
            fprintf(stderr, "Message at index %d is NULL!\n", index);
        }
        if (msg_q->msgs[index]->seq < ack) {
            if (-1 == _rmv_msg(msg_q, index)) {
                fprintf(stderr, "Failed to remove message with index %u.\n", index);
            }
            else {
                num_msgs++;
            }
        }

    }

    return num_msgs;
}

int get_msg_cpy(struct msg_queue *msg_q, struct msg *return_msg, ack_t seq) {
    struct msg *msg = NULL;
    if (MSG_Q_VAL(msg_q)) {
        fprintf(stderr, "Error: get_msg_cpy: %s\n", MSG_Q_UNINIT);
        return -1;
    }
    if (NULL == return_msg) {
        fprintf(stderr, "Error: get_msg_cpy: msg cannot be null\n");
        return -1;
    }
    if (0 == msg_q->curr_size) {
        fprintf(stderr, "Messages queue is empty.\n");
        return -1;
    }

    ack_t oldest = (msg_q->next_msg - msg_q->curr_size + msg_q->max_size) % msg_q->max_size;
    for (ack_t i = oldest; i < msg_q->curr_size + oldest; i++) {
        // working forwards from the oldest - get progressively larger indices,
        // but don't look at more messages than there are in the queue.
        if (NULL == (msg = msg_q->msgs[i % msg_q->max_size])) {
            fprintf(stderr, "Error: get_msg_cpy: found NULL msg where I expected a message.\n");
            return -1;
        }
        if (msg->seq == seq) { // This is the desired msg
            return_msg->seq = msg->seq;
            return_msg->payload = strdup(msg->payload);
            return 0;
        }
    }

    fprintf(stderr, "No such message with sequence number %u\n", seq);

    return -1;
}

/*
 * Free all the messages in the message queue.
 */
void free_msgs_q(struct msg_queue *msg_q) {
    if (MSG_Q_VAL(msg_q)) {
        fprintf(stderr, "Error: free_msgs_q: %s\n", MSG_Q_UNINIT);
        return;
    }
    for (uint32_t i = 0; i < msg_q->max_size; i++) {
        _rmv_msg(msg_q, i);
    }
}
