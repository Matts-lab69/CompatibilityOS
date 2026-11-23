/* SPDX-License-Identifier: MIT */
#include "message.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>

static intptr_t add(unsigned id, uintptr_t wparam, intptr_t lparam, void *context)
{
    struct cpos_queue *q = context;
    assert(pthread_equal(q->owner, pthread_self()));
    return id + (intptr_t)wparam + lparam;
}

static void *send_one(void *context)
{
    struct cpos_queue *q = context;
    intptr_t result;
    assert(cpos_dispatch(q) == EPERM);
    assert(cpos_send(q, 7, 5, -3, add, q, &result) == 0);
    assert(result == 9);
    return NULL;
}

int main(void)
{
    struct cpos_queue q;
    pthread_t sender;
    intptr_t result;
    assert(cpos_queue_init(&q) == 0);
    assert(cpos_send(&q, 1, 2, 3, add, &q, &result) == 0 && result == 6);
    assert(pthread_create(&sender, NULL, send_one, &q) == 0);
    assert(cpos_dispatch(&q) == 0);
    assert(pthread_join(sender, NULL) == 0);
    cpos_queue_close(&q);
    assert(cpos_dispatch(&q) == ECANCELED);
    assert(cpos_send(&q, 1, 2, 3, add, &q, &result) == ECANCELED);
    cpos_queue_destroy(&q);
    puts("message queue: owner, synchronous reply and close passed");
    return 0;
}
