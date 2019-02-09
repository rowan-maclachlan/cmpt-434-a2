#pragma once
// Rowan MacLachlan
// rdm695 22265820
// CMPT 434 Derek Eager
// February 7th, 6pm
// assignment 2

#include <sys/types.h>
#include <netinet/in.h>
#include <stdbool.h>

#define MAXLINE 1024

bool timeout(time_t timeout);

void set_timeout(time_t *timeout);

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

void *get_in_addr(struct sockaddr *sa);
