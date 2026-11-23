/* SPDX-License-Identifier: MIT */
#include "message.h"
#include <errno.h>
#include <stddef.h>

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
    return 0;
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
    int cancel_state;
    if (!pthread_equal(q->owner, pthread_self())) return EPERM;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
    pthread_mutex_lock(&q->lock);
    while (!q->head && !q->closed) pthread_cond_wait(&q->ready, &q->lock);
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
