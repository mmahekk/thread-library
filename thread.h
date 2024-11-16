/*
 * thread.h
 *
 * Definition of the thread structure and internal helper functions.
 * 
 * You may add more declarations/definitions in this file.
 */

#ifndef _THREAD_H_
#define _THREAD_H_

#include "ut369.h"
#include <stdbool.h>
#include <ucontext.h>

typedef enum {          
  READY = 1,          
  RUNNING = 2,          
  EXITED = 3,          
  EMPTY = 4,          
  KILLED = 5,
  SLEEPING = 6
} State;

struct thread {
    Tid id;


    struct thread *next; // Pointer to the next node
    bool in_queue;  // If in queue or not

    ucontext_t context; // Context of the thread
    void *stack; // Stack of the thread
    int exit_code; // Exit code of the thread
    State state; // State of the thread
    fifo_queue_t *current_wait_queue;  // Pointer to the queue the thread is in
    fifo_queue_t *wait_queue;  // New wait queue for thread_wait 

    int reap_count;
    int priority; 
};

// functions defined in thread.c
void thread_init(void);
void thread_end(void);

// functions defined in ut369.c
void ut369_exit(int exit_code);


#endif /* _THREAD_H_ */
