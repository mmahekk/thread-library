/*
 * prio.c
 *
 * Implementation of a priority scheduler (A3)
 */

#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

extern struct thread *current_thread;
static fifo_queue_t *prio_queue;
extern int queue_push_sorted(fifo_queue_t *, node_item_t *);

// static bool isAscending(void) {
//     if (prio_queue == NULL) {
//         return true;  // An empty queue is considered sorted
//     }

//     struct thread *prev_thread = (struct thread *)queue_top(prio_queue);
//     struct thread *current_node = prev_thread->next;

//     while (current_node != NULL) {
//         struct thread *current_thread = (struct thread *)current_node;
//         if (prev_thread->priority > current_thread->priority) {
//             return false;  // Queue is not sorted
//         }
//         prev_thread = current_thread;
//         current_node = current_node->next;
//     }

//     return true;  // Queue is sorted
// }

int 
prio_init(void)
{
    prio_queue = queue_create(THREAD_MAX_THREADS);
    if (prio_queue == NULL) {
        return THREAD_NOMEMORY;  
    }
    return 0;
}

int
prio_enqueue(struct thread * thread)
{
    assert(thread != NULL);
    int ret = queue_push_sorted(prio_queue, (node_item_t *)thread);
    if (ret != 0) {
        return THREAD_NOMORE;  // Queue is full
    }

    return 0;
}

struct thread *
prio_dequeue(void)
{
    struct thread *top_thread = queue_top(prio_queue);
    if (top_thread == NULL) {
        return NULL;
    }
    bool runnable;
    if (current_thread->state == KILLED || current_thread->state == RUNNING || current_thread->state == READY) {
        runnable = true;
    } else {
        runnable = false;
    }

    if (runnable && current_thread->priority < top_thread->priority) {
        return current_thread;
    }

    return (struct thread *)queue_pop(prio_queue);
}

struct thread *
prio_remove(Tid tid)
{
    return (struct thread *)queue_remove(prio_queue, tid);
}

void
prio_destroy(void)
{
    queue_destroy(prio_queue);
}

