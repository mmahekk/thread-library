/*
 * ut369.h
 *
 * Exported header file for UT369, a User-Level Threading Library 
 * designed for CSC369 students.
 * 
 *  DO NOT CHANGE
 */

#ifndef _UT369_H_
#define _UT369_H_

#include <stdbool.h>

#define THREAD_MAX_THREADS 1024 /* maximum number of threads */
#define THREAD_MIN_STACK  32768 /* minimum per-thread execution stack */

typedef int Tid; /* A thread identifier */

/*
 * Valid thread identifiers (Tid) range between 0 and THREAD_MAX_THREADS-1. The
 * first thread to run must have a thread id of 0. Note that this thread is the
 * main thread, i.e., it is created before the first call to thread_create.
 *
 * Negative Tid values are used for error codes or control codes.
 */
enum {
	THREAD_INVALID = -1,
	THREAD_ANY = -2,
	THREAD_NONE = -3,
	THREAD_NOMORE = -4,
	THREAD_NOMEMORY = -5,
	THREAD_TODO = -8,		
	THREAD_KILLED = -9,		
};

/* function type for a new thread's entry point */
typedef int (* thread_entry_f)(void *);

struct config {
    const char * sched_name;
    bool preemptive;
	bool verbose;
};

/* Initializes the threading system (already implemented in ut369.c)
 * Must be called before using the threading system.
 */
void ut369_start(struct config * config);


/* Return the thread identifier of the currently running thread. */
Tid thread_id(void);


/* thread_create should create a thread that starts running the function
 * fn(arg) with the specified priority. Upon success, return the thread identifier.
 * On failure, return the following:
 *
 * THREAD_NOMORE: no more threads can be created.
 * THREAD_NOMEMORY: no more memory available to create a thread stack. 
 */
Tid thread_create(thread_entry_f fn, void *arg, int priority);


/* thread_exit should ensure that the calling thread does not run after this
 * call, i.e., this function should never return. If there are other threads
 * in the system, one of them should be run. If there are no other threads 
 * (i.e., this is the last thread invoking thread_exit), then the process
 * should call ut369_exit(). The function has no return values.
 *
 * (A3) Preemptive threads addendum:
 * The exit_code should be copied to a thread that waits for the exiting thread. 
 *
 * The exiting thread's identifier cannot be reused until thread_wait is called
 * on the thread.
 */
void thread_exit(int exit_code);


/* Kill a thread whose identifier is tid. When a thread is killed, it should not
 * run any further. The calling thread continues to execute and receives the
 * result of the call. tid can be the identifier of any available thread.
 *
 * The killed thread's identifier cannot be reused until thread_wait is called
 * on the thread.
 *
 * (A3) Preemptive threads addendum:
 * Set exit code of the killed thread to THREAD_KILLED. 
 *
 * Upon success, return the identifier of the thread that was killed. Upon
 * failure, return the following:
 *
 * THREAD_INVALID: identifier tid does not correspond to a valid thread (e.g.,
 * any negative value of tid), the thread is already dead, or it is the 
 * current thread.
 */
Tid thread_kill(Tid tid);


/* thread_yield suspends the calling thread and run the thread with
 * identifier tid. The calling thread is put in the ready queue if it can still be run.
 * tid can be the identifier of any available thread or the following constant:
 *
 * THREAD_ANY:	   run a thread in the ready queue.
 *
 * Upon success, return the identifier of the thread that was switched to when
 * the calling thread yielded. Note that this function will not return to the 
 * calling thread until it runs again later. 
 * Upon failure, the calling thread continues running, and returns the 
 * following:
 *
 * THREAD_INVALID: identifier tid does not correspond to a valid thread.
 * THREAD_NONE:    no more threads, except the caller, are available to
 *		   		   run. This can only happen in response to a call with 
 *                 tid set to THREAD_ANY.
 */
Tid thread_yield(Tid tid);


/**************************************************************************
 * (A3) API functions for preemptive threads only
 **************************************************************************/

/* in preemptive mode, the interrupt will be delivered every 200 usec */
#define SIG_INTERVAL 200

/* disable interrupt diagnostic messages */
void interrupt_quiet(void);

/* check if interrupt is enabled */
int interrupt_enabled(void);

/* waste CPU cycle for usecs (in microseconds) */
void spin(int usecs);

/* turn off interrupts while printing */
int unintr_printf(const char *fmt, ...);

/* forward declaration of type (defined in queue.c) */
struct _fifo_queue;
typedef struct _fifo_queue fifo_queue_t;

/* Suspend the calling thread and run some other thread. The calling thread is
 * put in the wait queue. Caller must disable interrupt before calling this
 * function.
 *
 * Upon success, return the identifier of the thread that was switched to when
 * the calling thread was put to sleep. Note that this function will not return
 * to the calling thread until it runs again later. 
 * Upon failure, the calling thread continues running, and returns the 
 * following:
 *
 * THREAD_INVALID: queue is invalid, e.g., it is NULL.
 * THREAD_NONE:    no more threads, other than the caller, are available to
 *                 run. 
 */
Tid thread_sleep(fifo_queue_t *queue);


/* Wake up one or more threads that are suspended in the wait queue. These
 * threads are put in the ready queue. The calling thread continues to execute
 * and receives the result of the call. When "all" is 0, then one thread is
 * woken up.  When "all" is 1, all suspended threads are woken up. Wake up
 * threads in FIFO order, i.e., first thread to sleep must be woken up
 * first. The function returns the number of threads that were woken up. It can
 * return zero if there were no suspended threads in the wait queue. 
 */
int thread_wakeup(fifo_queue_t *queue, int all);


/* Suspend the current thread until the target thread (i.e., the thread whose 
 * identifier is tid) exits. If the target thread has already exited, then
 * thread_wait() returns immediately. 
 *
 * If exit_code is not NULL, then thread_wait() copies the exit status of the
 * target thread (i.e., the value that the target thread supplied to 
 * thread_exit) into the location pointed to by exit_code.
 *
 * Upon success, returns 0, and fully free the target thread so that its 
 * identifier can be reused.
 * Upon failure, returns THREAD_INVALID. Failure can occur for the following
 * reasons:
 *      - Identifier tid is not a feasible thread id (e.g., tid < 0 or 
 *        tid >= THREAD_MAX_THREADS) 
 *      - No thread with the identifier tid could be found.
 *      - The identifier tid refers to the calling thread.
 *      - One or more other threads are already in the process of reaping
 *        the target thread.
 */
int thread_wait(Tid tid, int *exit_code);

/* Set the priority of the current thread
 */
void set_priority(int priority);

/* forward declaration of type (defined in thread.c) */
struct lock;

/* Create a blocking lock. Initially, the lock is available. 
 * Associate a wait queue with the lock so that threads that need to acquire 
 * the lock can wait in this queue. 
 */
struct lock *lock_create();


/* Destroy the lock. Be sure to check that the lock is available when it is
 * being destroyed. 
 */
void lock_destroy(struct lock *lock);


/* Acquire the lock. Calling threads should be suspended on the lock's wait
 * queue until they can acquire the lock. 
 */
void lock_acquire(struct lock *lock);


/* Release the lock. Be sure to check that the lock had been acquired by the
 * calling thread, before it is released. Wakeup all threads that are waiting 
 * to acquire the lock. 
*/
void lock_release(struct lock *lock);

#endif /* _UTHREAD_H_ */
