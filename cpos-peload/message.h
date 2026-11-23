/* SPDX-License-Identifier: MIT */
#ifndef CPOS_MESSAGE_H
#define CPOS_MESSAGE_H
#include <pthread.h>
#include <stdint.h>

struct cpos_request;
struct cpos_queue {
    pthread_mutex_t lock;
    pthread_cond_t ready;
    pthread_t owner;
    struct cpos_request *head, *tail;
    unsigned pending;
    int closed;
};
typedef intptr_t (*cpos_message_handler)(unsigned, uintptr_t, intptr_t, void *);
int cpos_queue_init(struct cpos_queue *queue);
/* Close may be called concurrently; destroy only after all callers join. */
void cpos_queue_close(struct cpos_queue *queue);
void cpos_queue_destroy(struct cpos_queue *queue);
int cpos_send(struct cpos_queue *queue, unsigned id, uintptr_t wparam, intptr_t lparam,
              cpos_message_handler handler, void *context, intptr_t *result);
/* Blocks on the owner thread until one message is dispatched or close occurs. */
int cpos_dispatch(struct cpos_queue *queue);
#endif
