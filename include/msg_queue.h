#pragma once
// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th, 6pm
// assignment 2

#include <sys/types.h>

#define SEQ_BYTES 1

typedef uint8_t ack_t;

// Max value of a sequence number...
#define ACKSIZE 255 

struct msg {
    ack_t seq;
    char *payload;
};

struct msg_queue {
    // Array of pointers to messages.
    struct msg **msgs;
    // maximum size of the queue
    uint32_t max_size; 
    // current size of the queue
    uint32_t curr_size; 
    // Index of the next available queue slot for a message.
    uint32_t next_msg; 
};

void print_msg(struct msg *msg);

void print_msgs(struct msg_queue *msg_q);

int deserialize_msg(char *msg_buf, struct msg *msg);

char *serialize_msg(struct msg *msg);

void free_msgs_q(struct msg_queue *msg_q);

int init_msg_q(struct msg_queue *msg_q, uint32_t window_size);

int add_msg(struct msg_queue *msg_q, struct msg *msg);

ack_t get_earliest_unrcvd(struct msg_queue *msg_q, ack_t ack);

int rmv_msgs(struct msg_queue *msg_q, ack_t ack);

int rmv_newest_msg(struct msg_queue *msg_q);

int rmv_msg(struct msg_queue *msg_q);

/* Get a copy a message */
int get_msg_cpy(struct msg_queue *msg_q, struct msg *msg, ack_t seq);
