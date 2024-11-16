/*
 * thread.c
 *
 * Implementation of the threading library.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include "interrupt.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>

// put your global variables here
struct thread thread_array[THREAD_MAX_THREADS];
struct thread * current_thread;

/**************************************************************************
 * Cooperative threads: Refer to ut369.h and this file for the detailed 
 *                      descriptions of the functions you need to implement. 
 **************************************************************************/

/* Find an available thread slot in the thread array. */
static int
find_available_thread_index(void)
{
    for (int i = 0; i < THREAD_MAX_THREADS; i++) {
        if (thread_array[i].state == EMPTY) {
            return i;  // Return the index of the first EMPTY thread slot
        }
    }
    return THREAD_NOMORE;  // If no slots are available, return THREAD_NOMORE
}

/* Initialize the thread subsystem. */
void
thread_init(void)
{
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
        thread_array[i].id = i;  
        thread_array[i].state = EMPTY;  // Mark all threads as EMPTY initially
        thread_array[i].stack = NULL;  
        thread_array[i].in_queue = false;  
        thread_array[i].next = NULL;  
		thread_array[i].exit_code = 0;  
        thread_array[i].reap_count = 0;
        thread_array[i].wait_queue = NULL;
        thread_array[i].priority = INT_MAX;
		memset(&thread_array[i].context, 0, sizeof(ucontext_t));  
    }
    thread_array[0].wait_queue = queue_create(THREAD_MAX_THREADS);
    thread_array[0].priority = 0;
	thread_array[0].state = RUNNING;  
    current_thread = &thread_array[0];
}

/* Returns the tid of the current running thread.
This function returns the thread identifier of the 
currently running thread. The return value should lie between 0 
and THREAD_MAX_THREADS-1 (inclusive). */
Tid
thread_id(void)
{
	return current_thread->id;
}

/* Return the thread structure of the thread with identifier tid, or NULL if 
 * does not exist. Used by thread_yield and thread_wait's placeholder 
 * implementation.
 */
static struct thread * 
thread_get(Tid tid)
{
    if (tid < 0 || tid >= THREAD_MAX_THREADS) {
        return NULL;  // Invalid thread identifier
    }

    if (thread_array[tid].state == EMPTY) {
        return NULL;  // Thread does not exist
    }

    return &thread_array[tid];
}

/* Return whether the thread with identifier tid is runnable.
 * Used by thread_yield and thread_wait's placeholder implementation
 */
static bool
thread_runnable(Tid tid)
{
    if (tid < 0 || tid >= THREAD_MAX_THREADS) {
        return false;  // Tid is out of range.
    }

    if (thread_array[tid].state == READY || thread_array[tid].state == RUNNING 
	|| thread_array[tid].state == KILLED) {
        return true;
    }

    return false;
}

/* Context switch to the next thread. Used by thread_yield. */
static void
thread_switch(struct thread * next)
{
	volatile int flag = 0;

	struct thread *prev_thread = current_thread;
	current_thread = next;

	if (prev_thread->state != EXITED && prev_thread->state != SLEEPING) {
        prev_thread->state = READY;  
    }

    if (next->state != KILLED) {
        next->state = RUNNING;  
    }

	// Save the current context of the previous thread.
    int err = getcontext(&prev_thread->context);
    assert(!err);  
	if (prev_thread->state == KILLED) {
		thread_exit(THREAD_KILLED);
	}

    if (flag == 0) {
        flag = 1;  // Mark that we've saved the context.

        setcontext(&next->context);
        assert(false);  // If setcontext returns, it's an error.
	}
}

/* Voluntarily pauses the execution of current thread and invokes scheduler
 * to switch to another thread.
 */
Tid
thread_yield(Tid want_tid)
{
    int enabled = interrupt_set(false); // disable
    struct thread *next_thread = NULL;

    // Handle yielding to any thread in the ready queue
    if (want_tid == THREAD_ANY) {
        next_thread = scheduler->dequeue();
        // If no other thread is available, return THREAD_NONE
        if (next_thread == NULL) {
            interrupt_set(enabled); // enable before returning
            return THREAD_NONE;
        }
        if (next_thread->id == thread_id()) {
            interrupt_set(enabled);
            return thread_id();
        }
    }
    // Handle yielding to self (current thread)
    else if (want_tid == thread_id()) {
        assert(thread_runnable(want_tid));
        interrupt_set(enabled); // enable before returning
        return want_tid;
    }
    // Handle yielding to a specific thread ID
    else {
        next_thread = scheduler->remove(want_tid);
        if (next_thread == NULL) {
            interrupt_set(enabled);
            return THREAD_INVALID;
        }
    }

    next_thread->in_queue = false;
    // Enqueue the currently running thread if it is still runnable
    if (thread_runnable(thread_id())) {
        scheduler->enqueue(thread_get(thread_id()));
        current_thread = thread_get(thread_id());
        current_thread->in_queue = true;
    }

    thread_switch(next_thread);

    interrupt_set(enabled);
    return next_thread->id;
}

/* Fully clean up a thread structure and make its tid available for reuse.
 * Used by thread_wait's placeholder implementation
 */
static void
thread_destroy(struct thread * dead)
{
	if (dead->stack != NULL || dead->id != 0) {
        free(dead->stack);
        dead->stack = NULL;
    }

	dead->state = EMPTY;
    dead->exit_code = 0;
    dead->in_queue = false;
    dead->next = NULL;
    dead->current_wait_queue = NULL;

    if (dead->wait_queue != NULL) {
        queue_destroy(dead->wait_queue);
        dead->wait_queue = NULL;
    }

    dead->priority = INT_MAX;
    dead->reap_count = 0;

    memset(&dead->context, 0, sizeof(ucontext_t));
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
static void
thread_stub(int (*thread_main)(void *), void *arg)
{
	struct thread * th = current_thread;
	if (th->state == KILLED) {
		thread_exit(th->exit_code);
	}
	
    interrupt_set(true);
	int ret = thread_main(arg); // call thread_main() function with arg
	thread_exit(ret);
}

Tid
thread_create(int (*fn)(void *), void *parg, int priority)
{
    int enabled = interrupt_set(false);
    // Find an available thread slot in the thread array.
    int index = find_available_thread_index();
    if (index == THREAD_NOMORE) {
        interrupt_set(enabled);
        return THREAD_NOMORE;  
    }
	struct thread *new_thread = &thread_array[index];
    new_thread->priority = priority;
    new_thread->wait_queue = queue_create(THREAD_MAX_THREADS);

    void *raw_stack = malloc(THREAD_MIN_STACK + 16);
    if (!raw_stack) {
        interrupt_set(enabled);
        return THREAD_NOMEMORY;  // Memory allocation failed.
	}

	int err = getcontext(&new_thread->context);
    assert(!err);

	new_thread->state = READY;

	new_thread->stack = raw_stack;
	new_thread->context.uc_mcontext.gregs[REG_RIP] = (unsigned long)thread_stub;
	new_thread->context.uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;    // First argument
    new_thread->context.uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;  // Second argument
	new_thread->context.uc_mcontext.gregs[REG_RSP] = (unsigned long)raw_stack + THREAD_MIN_STACK + 15;
    new_thread->context.uc_mcontext.gregs[REG_RSP] -= (new_thread->context.uc_mcontext.gregs[REG_RSP] - 8) % 16;
    
    int error_check = scheduler->enqueue(new_thread);
    if (error_check != 0) {
        free(new_thread->stack);  // Free the stack memory if enqueue fails.
        new_thread->state = EMPTY;  
        memset(&new_thread->context, 0, sizeof(ucontext_t));  
        interrupt_set(enabled);
        return THREAD_NOMORE;  
    }

    new_thread->in_queue = true;

    if (scheduler->realtime) {
        int ret = thread_yield(THREAD_ANY);
        assert(ret >= 0);
    }

    interrupt_set(enabled);
	return new_thread->id;
}

Tid
thread_kill(Tid tid)
{
    int enabled = interrupt_set(false);
    if (tid < 0 || tid >= THREAD_MAX_THREADS) {
        interrupt_set(enabled);
        return THREAD_INVALID; 
    }

    // Cannot kill the current running thread.
    if (tid == thread_id()) {
        interrupt_set(enabled);
        return THREAD_INVALID;
    }

    struct thread *target_thread = thread_get(tid);
    
    if (target_thread == NULL || target_thread->state == EMPTY) {
        interrupt_set(enabled);
        return THREAD_INVALID;  
    }

    if (target_thread->state == KILLED || target_thread->state == EXITED) {
        interrupt_set(enabled);
        return tid;
    }

    if (target_thread->state == SLEEPING) {
        queue_remove(target_thread->current_wait_queue, tid);
        target_thread->current_wait_queue = NULL;
        scheduler->enqueue(target_thread);
        target_thread->in_queue = true;
        target_thread->state = KILLED;
        target_thread->exit_code = THREAD_KILLED;
        if (scheduler->realtime) {
            thread_yield(THREAD_ANY);
        }
    }

    target_thread->state = KILLED;
	target_thread->exit_code = THREAD_KILLED;

    interrupt_set(enabled);
    return tid;  
}

void
thread_exit(int exit_code)
{
    int enabled = interrupt_set(false);
    current_thread->state = EXITED;  // Mark the current thread as exited.
    current_thread->exit_code = exit_code;

    // printf("thread %d: exit code is %d\n", current_thread->id, exit_code);
    if (current_thread->wait_queue != NULL) {
        // Wake up all threads in the wait queue and set reap_count
        current_thread->reap_count = thread_wakeup(current_thread->wait_queue, 1);  
    } else {
        // If no threads are waiting, reap_count remains 0
        current_thread->reap_count = 0;
    }

    // Remove the exited thread from the scheduler.
    // scheduler->remove(current_thread->id);  

    struct thread *next_thread = scheduler->dequeue();
    if (next_thread == NULL) {
        ut369_exit(exit_code);
        assert(0);
    }

    next_thread->in_queue = false;
    thread_switch(next_thread);

    assert(0);  // This point should never be reached.

    interrupt_set(enabled);
}

/* Clean-up logic to unload the threading system. Used by ut369.c. You may 
 * assume all threads are either freed or in the zombie state when this is 
 */
void
thread_end(void)
{
    for (int i = 0; i < THREAD_MAX_THREADS; i++) {
        struct thread *th = &thread_array[i];
        if (th->state != EMPTY) {
            thread_destroy(th);
        }
    }
}

/**************************************************************************
 * Preemptive threads: Refer to ut369.h for the detailed descriptions of 
 *                     the functions you need to implement. 
 **************************************************************************/

Tid
thread_wait(Tid tid, int *exit_code)
{
    int enabled = interrupt_set(false);
    // If tid refers to the calling thread, return THREAD_INVALID
    
    struct thread *target_thread = thread_get(tid);

    // If the target thread doesn't exist or is invalid, return THREAD_INVALID
    if (target_thread == NULL || target_thread->state == EMPTY) {
        interrupt_set(enabled);
        return THREAD_INVALID;
    }

    if (tid == thread_id()) {
        interrupt_set(enabled);
        return THREAD_INVALID;
    }
 
    if (target_thread->state == EXITED) {
        // Case: Thread has exited, and this is the first reaper
        if (target_thread->reap_count == 0) {
            if (exit_code != NULL) {
                *exit_code = target_thread->exit_code;
                current_thread->exit_code = *exit_code;
            }
            thread_destroy(target_thread);  // Only one reaper, destroy immediately
            interrupt_set(enabled);
            return 0;
        } else {
            // Reaping process already in progress by other threads
            interrupt_set(enabled);
            return THREAD_INVALID;
        }
    }

    thread_sleep(target_thread->wait_queue);

    // After waking up, handle the exit code and decrease the reap_count
    if (exit_code != NULL) {
        *exit_code = target_thread->exit_code;
        current_thread->exit_code = *exit_code;
    }

    // Last reaper should destroy the thread
    if (target_thread->reap_count == 1) {
        thread_destroy(target_thread);
    }

    struct thread *test = scheduler->dequeue();

    if (!test) {
        interrupt_set(enabled);
        return 0;
    }

    target_thread->reap_count--;
    if (test->id == current_thread->id) {
        thread_destroy(target_thread);
        interrupt_set(enabled);
        return 0;
    }

    scheduler->enqueue(test);

    interrupt_set(enabled);
    return 0;
}

Tid
thread_sleep(fifo_queue_t *queue)
{
    int enabled = interrupt_set(false);
    
    // Check if the queue is NULL
    if (queue == NULL) {
        interrupt_set(enabled); // Re-enable interrupts before returning
        return THREAD_INVALID;
    }

    current_thread->state = SLEEPING;
    struct thread *next_thread = NULL;
    next_thread = scheduler->dequeue();
    // If no other thread is available, return THREAD_NONE
    if (next_thread == NULL) {
        current_thread->state = RUNNING;
        interrupt_set(enabled); // enable before returning
        return THREAD_NONE;
    }
    current_thread->state = RUNNING;
    next_thread->in_queue = false;

    struct thread *prev_thread = current_thread;
    if (queue_push(queue, prev_thread) == -1) {
        interrupt_set(enabled);
        return THREAD_INVALID;
    };
    prev_thread->in_queue = false; // even tho it's in wait queue, it's not in ready
    prev_thread->state = SLEEPING;
    prev_thread->current_wait_queue = queue; // Track the queue it's waiting on

    thread_switch(next_thread);

    interrupt_set(enabled);
    return next_thread->id;
}

/* When the 'all' parameter is 1, wake up all threads waiting in the queue.
 * returns whether a thread was woken up on not. 
 */
int
thread_wakeup(fifo_queue_t *queue, int all)
{
    int enabled = interrupt_set(false);

    // Check if the queue is NULL or invalid
    if (queue == NULL) {
        interrupt_set(enabled); // Re-enable interrupts before returning
        return 0;
    }

    int woken_threads = 0;
    struct thread *woken_thread = NULL;

    // If 'all' is 0, wake up only the first thread in the wait queue
    if (all == 0) {
        woken_thread = queue_pop(queue);
        if (woken_thread != NULL) {
            woken_thread->state = READY;
            scheduler->enqueue(woken_thread);
            woken_thread->in_queue = true;
            woken_thread->current_wait_queue = NULL;
            woken_threads = 1;  // Only one thread woken up
        }
    } 

    // If 'all' is 1, wake up all threads in the wait queue
    else if (all == 1) {
        while ((woken_thread = queue_pop(queue)) != NULL) {
            woken_thread->state = READY;
            scheduler->enqueue(woken_thread);
            woken_thread->in_queue = true;
            woken_thread->current_wait_queue = NULL;
            woken_threads++;  // Count how many threads are woken up
        }
    }

    if (scheduler->realtime) {
        thread_yield(THREAD_ANY);
    }

    interrupt_set(enabled);
    return woken_threads;
}

void
set_priority(int priority)
{
    int enabled = interrupt_set(false); // Disable interrupts

    current_thread->priority = priority; // Update priority

    // Check if a higher-priority thread should preempt the current thread
    // struct thread *next_thread = scheduler->dequeue();
    // if (next_thread && next_thread->id != current_thread->id) {
    //     scheduler->enqueue(next_thread);
    //     thread_yield(next_thread->id);
    // }
    if (scheduler->realtime) {
        thread_yield(THREAD_ANY);
    }

    interrupt_set(enabled); // Re-enable interrupts
}

struct lock {
    bool held;                 // True if lock is acquired, false otherwise
    Tid holder;                // Thread ID of the lock holder
    fifo_queue_t *wait_queue;  // Queue for waiting threads
};

struct lock *
lock_create()
{
    struct lock *lock = malloc(sizeof(struct lock));
    if (lock == NULL) {
        return NULL; 
    }

    lock->held = false; // Lock is initially available
    lock->holder = THREAD_NONE; 
    lock->wait_queue = queue_create(THREAD_MAX_THREADS);

    if (lock->wait_queue == NULL) {
        free(lock);
        return NULL; 
    }

    return lock;
}

void
lock_destroy(struct lock *lock)
{
    assert(lock != NULL);
    assert(lock->held == false); // Ensure the lock is not held

    queue_destroy(lock->wait_queue);
    free(lock);
}

void
lock_acquire(struct lock *lock)
{
    assert(lock != NULL);

    int enabled = interrupt_set(false); 

    // Check if the lock is already held
    while (lock->held) {
        // Add current thread to wait queue and put it to sleep
        thread_sleep(lock->wait_queue);
    }

    // Acquire the lock
    lock->held = true;
    lock->holder = thread_id(); 

    interrupt_set(enabled); 
}

void
lock_release(struct lock *lock)
{
    assert(lock != NULL);
    assert(lock->held == true); 
    assert(lock->holder == thread_id()); 

    int enabled = interrupt_set(false); 

    // Release the lock
    lock->held = false;
    lock->holder = THREAD_NONE;

    thread_wakeup(lock->wait_queue, 0);

    interrupt_set(enabled); // Re-enable interrupts
}
