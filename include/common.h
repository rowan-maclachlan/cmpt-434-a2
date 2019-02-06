#ifndef COMMON_H
#define COMMON_H

// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th, 6pm
// assignment 2

#include <sys/types.h>
#include <netinet/in.h>

#define MAXLINE 1024 
#define SEQ_BYTES 1
#define SEQ_MASK 0x0f

typedef uint32_t ack_t;

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

int rmv_msg(struct msg_queue *msg_q);

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

void *get_in_addr(struct sockaddr *sa);

#endif // COMMON_H
