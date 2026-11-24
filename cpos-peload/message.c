/* SPDX-License-Identifier: MIT */
#include "message.h"
#include "../cpos-kernel/cpos_abi.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <stddef.h>

#define CPOS_EVENT_WAIT_SLICE_MS 100

static int event_op(struct cpos_queue *q, unsigned operation, unsigned timeout_ms)
{
    struct cpos_object_op op = {.size = sizeof(op), .operation = operation,
                                .handle = q->event_handle, .timeout_ms = timeout_ms};
    if (ioctl(q->event_fd, CPOS_IOC_OBJECT_OP, &op) == 0) return 0;
    return errno;
}

static int event_op_values(int fd, uint64_t handle, unsigned operation, unsigned timeout_ms)
{
    struct cpos_object_op op = {.size = sizeof(op), .operation = operation,
                                .handle = handle, .timeout_ms = timeout_ms};
    if (ioctl(fd, CPOS_IOC_OBJECT_OP, &op) == 0) return 0;
    return errno;
}

static void disable_event_locked(struct cpos_queue *q)
{
    q->event_fd = -1;
    q->event_handle = 0;
}

static void signal_event_locked(struct cpos_queue *q)
{
    if (q->event_fd >= 0 && event_op(q, CPOS_OP_SIGNAL, 0)) disable_event_locked(q);
}

struct cpos_request {
    struct cpos_request *next;
    pthread_cond_t reply;
    unsigned id;
    uintptr_t wparam;
    intptr_t lparam, result;
    cpos_message_handler handler;
    void *context;
    int done, error;
};

int cpos_queue_init(struct cpos_queue *q)
{
    int error = pthread_mutex_init(&q->lock, NULL);
    if (error) return error;
    error = pthread_cond_init(&q->ready, NULL);
    if (error) { pthread_mutex_destroy(&q->lock); return error; }
    q->owner = pthread_self();
    q->head = q->tail = NULL;
    q->pending = 0;
    q->closed = 0;
    q->event_fd = -1;
    q->event_handle = 0;
    return 0;
}

int cpos_queue_use_event(struct cpos_queue *q, int event_fd, uint64_t handle)
{
    int error = 0;
    if (event_fd < 0 || !handle) return EINVAL;
    pthread_mutex_lock(&q->lock);
    if (q->closed || q->pending) error = EBUSY;
    else if (q->event_fd >= 0) error = EALREADY;
    else { q->event_fd = event_fd; q->event_handle = handle; }
    pthread_mutex_unlock(&q->lock);
    return error;
}

void cpos_queue_close(struct cpos_queue *q)
{
    pthread_mutex_lock(&q->lock);
    q->closed = 1;
    while (q->head) {
        struct cpos_request *r = q->head;
        q->head = r->next;
        r->error = ECANCELED;
        r->done = 1;
        pthread_cond_signal(&r->reply);
    }
    q->tail = NULL;
    q->pending = 0;
    pthread_cond_broadcast(&q->ready);
    signal_event_locked(q);
    pthread_mutex_unlock(&q->lock);
}

void cpos_queue_destroy(struct cpos_queue *q)
{
    cpos_queue_close(q);
    pthread_cond_destroy(&q->ready);
    pthread_mutex_destroy(&q->lock);
}

int cpos_send(struct cpos_queue *q, unsigned id, uintptr_t wparam, intptr_t lparam,
              cpos_message_handler handler, void *context, intptr_t *result)
{
    struct cpos_request r = {.id = id, .wparam = wparam, .lparam = lparam,
                             .handler = handler, .context = context};
    int error, cancel_state;
    if (!handler || !result) return EINVAL;
    if (pthread_equal(q->owner, pthread_self())) {
        pthread_mutex_lock(&q->lock);
        error = q->closed ? ECANCELED : 0;
        pthread_mutex_unlock(&q->lock);
        if (!error) *result = handler(id, wparam, lparam, context);
        return error;
    }
    error = pthread_cond_init(&r.reply, NULL);
    if (error) return error;
    /* A queued request has stack lifetime; only close or dispatch may release it. */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
    pthread_mutex_lock(&q->lock);
    if (q->closed) error = ECANCELED;
    else if (q->pending == 64) error = EAGAIN;
    else {
        if (q->tail) q->tail->next = &r;
        else q->head = &r;
        q->tail = &r;
        ++q->pending;
        pthread_cond_signal(&q->ready);
        signal_event_locked(q);
        while (!r.done) pthread_cond_wait(&r.reply, &q->lock);
        error = r.error;
        if (!error) *result = r.result;
    }
    pthread_mutex_unlock(&q->lock);
    pthread_cond_destroy(&r.reply);
    pthread_setcancelstate(cancel_state, NULL);
    return error;
}

int cpos_dispatch(struct cpos_queue *q)
{
    struct cpos_request *r;
    intptr_t result;
    int cancel_state, error;
    if (!pthread_equal(q->owner, pthread_self())) return EPERM;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
    pthread_mutex_lock(&q->lock);
    while (!q->head && !q->closed) {
        if (q->event_fd >= 0) {
            int fd = q->event_fd;
            uint64_t handle = q->event_handle;
            error = event_op_values(fd, handle, CPOS_OP_RELEASE, 0);
            if (error) {
                disable_event_locked(q);
                continue;
            }
            if (!q->head && !q->closed) {
                pthread_mutex_unlock(&q->lock);
                error = event_op_values(fd, handle, CPOS_OP_WAIT, CPOS_EVENT_WAIT_SLICE_MS);
                pthread_mutex_lock(&q->lock);
                if (error && error != ETIMEDOUT && error != EINTR) disable_event_locked(q);
            }
        } else pthread_cond_wait(&q->ready, &q->lock);
    }
    r = q->head;
    if (r) {
        q->head = r->next;
        if (!q->head) q->tail = NULL;
        --q->pending;
    }
    pthread_mutex_unlock(&q->lock);
    if (r) {
        result = r->handler(r->id, r->wparam, r->lparam, r->context);
        pthread_mutex_lock(&q->lock);
        r->result = result;
        r->done = 1;
        pthread_cond_signal(&r->reply);
        pthread_mutex_unlock(&q->lock);
    }
    pthread_setcancelstate(cancel_state, NULL);
    return r ? 0 : ECANCELED;
}
