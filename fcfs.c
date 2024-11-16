/*
 * fcfs.c
 *
 * Implementation of a first-come first-served scheduler.
 * Becomes round-robin once preemption is enabled.
 */

#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

int 
fcfs_init(void)
{
    /* TODO: complete me */
    return THREAD_TODO;
}

int
fcfs_enqueue(struct thread * thread)
{
    /* TODO: complete me */
    (void)thread;
    return THREAD_TODO;
}

struct thread *
fcfs_dequeue(void)
{
    /* TODO: complete me */
    return NULL;
}

struct thread *
fcfs_remove(Tid tid)
{
    /* TODO: complete me */
    (void)tid;
    return NULL;
}

void
fcfs_destroy(void)
{
    /* TODO: complete me */
}