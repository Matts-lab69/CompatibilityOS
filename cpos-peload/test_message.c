/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include "message.h"
#include "../cpos-kernel/cpos_abi.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

struct order_log { unsigned ids[2]; unsigned count; };
struct send_args { struct cpos_queue *queue; unsigned id; int status; struct order_log *log; };

static intptr_t record_id(unsigned id, uintptr_t wparam, intptr_t lparam, void *context)
{
    (void)wparam;
    (void)lparam;
    struct order_log *log = context;
    assert(log->count < 2);
    log->ids[log->count++] = id;
    return (intptr_t)id;
}

static void *send_id(void *context)
{
    struct send_args *args = context;
    intptr_t result = -1;
    args->status = cpos_send(args->queue, args->id, 0, 0, record_id, args->log, &result);
    if (!args->status) assert(result == (intptr_t)args->id);
    return NULL;
}

static void await_pending(struct cpos_queue *q, unsigned count)
{
    pthread_mutex_lock(&q->lock);
    while (q->pending != count) pthread_cond_wait(&q->ready, &q->lock);
    pthread_mutex_unlock(&q->lock);
}

static void run_tests(int event_fd, uint64_t event_handle)
{
    struct cpos_queue q;
    pthread_t sender;
    pthread_t ordered[2];
    struct order_log log = {0};
    struct send_args args[2] = {{.queue = &q, .id = 11, .log = &log},
                                {.queue = &q, .id = 12, .log = &log}};
    struct send_args closing = {.queue = &q, .id = 13};
    intptr_t result;
    assert(cpos_queue_init(&q) == 0);
    if (event_fd >= 0) assert(cpos_queue_use_event(&q, event_fd, event_handle) == 0);
    assert(cpos_send(&q, 1, 2, 3, add, &q, &result) == 0 && result == 6);
    assert(pthread_create(&sender, NULL, send_one, &q) == 0);
    assert(cpos_dispatch(&q) == 0);
    assert(pthread_join(sender, NULL) == 0);
    assert(pthread_create(&ordered[0], NULL, send_id, &args[0]) == 0);
    await_pending(&q, 1);
    assert(pthread_create(&ordered[1], NULL, send_id, &args[1]) == 0);
    await_pending(&q, 2);
    assert(cpos_dispatch(&q) == 0);
    assert(cpos_dispatch(&q) == 0);
    assert(pthread_join(ordered[0], NULL) == 0);
    assert(pthread_join(ordered[1], NULL) == 0);
    assert(args[0].status == 0 && args[1].status == 0);
    assert(log.count == 2 && log.ids[0] == 11 && log.ids[1] == 12);
    assert(pthread_create(&sender, NULL, send_id, &closing) == 0);
    await_pending(&q, 1);
    cpos_queue_close(&q);
    assert(pthread_join(sender, NULL) == 0);
    assert(closing.status == ECANCELED);
    assert(cpos_dispatch(&q) == ECANCELED);
    assert(cpos_send(&q, 1, 2, 3, add, &q, &result) == ECANCELED);
    assert(q.event_fd == event_fd);
    cpos_queue_destroy(&q);
    puts("message queue: owner, FIFO order, synchronous reply and queued close passed");
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        run_tests(-1, 0);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--event") == 0) {
        int fd = open("/dev/compat", O_RDWR | O_CLOEXEC);
        struct cpos_object_request req = {.size = sizeof(req), .type = CPOS_OBJECT_EVENT};
        assert(fd >= 0);
        assert(ioctl(fd, CPOS_IOC_OBJECT_CREATE, &req) == 0);
        run_tests(fd, req.handle);
        {
            struct cpos_object_request drop = {.size = sizeof(drop), .handle = req.handle};
            assert(ioctl(fd, CPOS_IOC_OBJECT_DROP, &drop) == 0);
        }
        assert(close(fd) == 0);
        puts("message queue: guest event backend remained enabled");
        return 0;
    }
    fprintf(stderr, "usage: %s [--event]\n", argv[0]);
    return 2;
}
