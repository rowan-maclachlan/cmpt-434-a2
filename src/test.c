#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "msg_queue.h"

int _add_msg(struct msg_queue *msg_q, uint32_t seq) {
    int ret = 0;
    struct msg msg;
    msg.seq = seq;
    msg.payload = strdup("This is a message.");
    ret = add_msg(msg_q, &msg);
    free(msg.payload);

    return ret;
}

int _add_msgs(struct msg_queue *msg_q) {
    for (uint32_t i = 0; i < msg_q->max_size; i++) {
        if (-1 == _add_msg(msg_q, i)) {
            fprintf(stderr, "Failed to add_msg %u.\n", i);
            return -1;
        }
    }
    return 0;
}

int _rmv_msgs(struct msg_queue *msg_q) {
    for (uint32_t i = 0; i < msg_q->max_size; i++) {
        if (-1 == rmv_msg(msg_q)) {
            fprintf(stderr, "Failed to rmv_msg %u.\n", i);
            return -1;
        }
    }
    return 0;
}

int _add_rmv_msgs(struct msg_queue *msg_q) {
    if (-1 == _add_msgs(msg_q)) {
        return -1;
    }
    if (-1 != _add_msg(msg_q, msg_q->max_size)) {
        fprintf(stderr, "Added too many messages (%u) successfully.\n", msg_q->max_size);
        return -1;
    }
    else {
        printf("Didn't add too many messages.\n");
    }

    if (-1 == _rmv_msgs(msg_q)) {
        return -1;
    }
    if (-1 != rmv_msg(msg_q)) {
        fprintf(stderr, "Removed too many messages (%u) successfully.\n", msg_q->max_size);
        return -1;
    }

    if (-1 == _add_msgs(msg_q)) {
        return -1;
    }
    if (-1 != _add_msg(msg_q, msg_q->max_size)) {
        fprintf(stderr, "Added too many messages (%u) successfully.\n", msg_q->max_size);
        return -1;
    }
    else {
        printf("Didn't add too many messages.\n");
    }

    for (uint32_t i = 0; i < msg_q->max_size / 2; i++) {
        if (-1 == rmv_msg(msg_q)) {
            fprintf(stderr, "Failed to rmv_msg %u.\n", i);
            return -1;
        }
    }

    return 0;
}

void print_buffer(char *buf, int buf_len) {
    for (int i = 0; i < buf_len; i++)
    {
            printf("%02X", buf[i]);
    }
    printf("\n");
}

int ser_des_msg(void) {
    struct msg msg;
    char *msg_buf;
    char *test_msg = "This is a message.";

    msg.seq = 0;
    msg.payload = strdup(test_msg);
    if (NULL == (msg_buf = serialize_msg(&msg))) {
        return -1;
    }
    free(msg.payload);
    deserialize_msg(msg_buf, &msg);
    free(msg_buf);

    free(msg.payload);

    return 0;
}

void get_msg_test(struct msg_queue *msg_q) {
    struct msg msg;
    free_msgs_q(msg_q);
    assert(msg_q->curr_size == 0);

    _add_msg(msg_q, 0);
    _add_msg(msg_q, 1);
    _add_msg(msg_q, 2);

    assert(-1 != get_msg_cpy(msg_q, &msg, 2));
    assert(msg.seq == 2);
    free(msg.payload);
    
    rmv_msg(msg_q); // remove oldest message (seq # 0)
    assert(-1 == get_msg_cpy(msg_q, &msg, 0));
    assert(-1 != get_msg_cpy(msg_q, &msg, 1));
    assert(msg.seq == 1);
}

void rmv_newest_test(struct msg_queue *msg_q) {
    struct msg msg;
    free_msgs_q(msg_q);
    assert(msg_q->curr_size == 0);

    _add_msg(msg_q, 0);
    _add_msg(msg_q, 1);
    _add_msg(msg_q, 2);
    free(msg.payload);

    assert(-1 != get_msg_cpy(msg_q, &msg, 2));
    assert(2 == rmv_newest_msg(msg_q));
    assert(1 == rmv_newest_msg(msg_q));
    assert(0 == rmv_newest_msg(msg_q));
}

int main(void) {
    struct msg_queue msg_q;

    if (-1 == init_msg_q(&msg_q, 8)) {
        fprintf(stderr, "init_msg_q failed.\n");
        fprintf(stderr, " ### TESTS FAILED ### \n");
        exit(1);
    }
    else {
        printf("init_msg_q succeeded.\n");
    }

    if (-1 == _add_rmv_msgs(&msg_q)) {
        fprintf(stderr, " ### TESTS FAILED ### \n");
        exit(1);
    }
    else {
        printf("_add_rmv_msgs test passed.\n");
    }

    free_msgs_q(&msg_q);

    if (-1 == ser_des_msg()) {
        fprintf(stderr, " ### TESTS FAILED ### \n");
        exit(1);
    }
    else {
        printf("ser_des_msg test passed.\n");
    }

    if (-1 == _add_msg(&msg_q, 0)) {
        fprintf(stderr, " ### TESTS FAILED ### \n");
        exit(1);
    }
    if (-1 == _add_msg(&msg_q, 1)) {
        fprintf(stderr, " ### TESTS FAILED ### \n");
        exit(1);
    }
    if (-1 == rmv_msg(&msg_q)) {
        fprintf(stderr, " ### TESTS FAILED ### \n");
        exit(1);
    }
    if (-1 == rmv_msg(&msg_q)) {
        fprintf(stderr, " ### TESTS FAILED ### \n");
        exit(1);
    }

    get_msg_test(&msg_q);

    rmv_newest_test(&msg_q);

    return 0;

}
