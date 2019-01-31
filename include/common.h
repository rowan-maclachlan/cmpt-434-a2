#ifndef COMMON_H
#define COMMON_H

// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th, 6pm
// assignment 2

#include <sys/types.h>

#define MAXLINE 1024 
#define SEQ_BYTES 1
#define SEQ_MASK 0x0f

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

void *get_in_addr(struct sockaddr *sa);

#endif // COMMON_H
