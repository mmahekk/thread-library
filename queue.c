/*
 * queue.c
 *
 * Definition of the queue structure and implemenation of its API functions.
 *
 */

#include "thread.h"
#include "queue.h"
#include <stdlib.h>
#include <assert.h>

struct _fifo_queue { // Typedef in queue.h
    unsigned int capacity; // Maximum number of nodes that can be in the queue
    unsigned int size; // Number of nodes currently in the queue
    node_item_t * head; // First node in the queue
    node_item_t * tail; // Last node in the queue
};


bool node_in_queue(node_item_t * node)
{
    assert(node != NULL);
    return node->in_queue;
}

fifo_queue_t * queue_create(unsigned capacity)
{
    if (capacity <= 0) {
        return NULL;
    }
    fifo_queue_t * queue = malloc(sizeof(fifo_queue_t));
    if (!queue) {
        return NULL;
    }
    queue->capacity = capacity;
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

void queue_destroy(fifo_queue_t * queue)
{
    assert(queue->size == 0);
    free(queue);
}

node_item_t * queue_pop(fifo_queue_t * queue)
{
    assert(queue != NULL);
    if (queue->size == 0 || queue->head == NULL) { 
        return NULL;
    }
    node_item_t * node_to_return = queue->head; // We want to return the head
    queue->head = node_to_return->next;
    queue->size--;
    if (queue->size == 0) {
        queue->tail = NULL;
    }
    node_to_return->in_queue = false;
    node_to_return->next = NULL;

    return node_to_return;
}

node_item_t * queue_top(fifo_queue_t * queue)
{
    assert(queue != NULL);
    if (queue->size == 0 || queue->head == NULL) {
        return NULL;
    }
    return queue->head;
}

int queue_push(fifo_queue_t * queue, node_item_t * node)
{
    assert(node->in_queue == false);
    assert(queue != NULL);
    assert(node != NULL);
    if (queue->size == queue->capacity) { // Check if queue is full
        return -1;
    }
    if (queue->size == 0) { // If queue is empty
        queue->head = node;
        queue->tail = node;
    } else { // If queue is not empty
        queue->tail->next = node;
        queue->tail = node;
    }
    // Update remaining fields
    node->next = NULL;
    node->in_queue = true;
    queue->size++;
    return 0;
}

node_item_t * queue_remove(fifo_queue_t * queue, int id)
{
    assert(queue != NULL);
    if (queue->size == 0 || queue->head == NULL) {
        return NULL;
    }

    node_item_t * curr = queue->head;
    node_item_t * prev = NULL;

    while (curr != NULL) {
        if (curr->id == id) {
            if (prev == NULL) { // If the node to remove is the head
                queue->head = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (queue->tail == curr) { // If the node to remove is the tail
                queue->tail = prev;
            }
            queue->size--; 
            curr->in_queue = false;
            curr->next = NULL;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

int
queue_count(fifo_queue_t * queue)
{
    if (queue == NULL) {
        return 0;
    }
    return queue->size;
}

int queue_push_sorted(fifo_queue_t *queue, node_item_t *node) {
    assert(queue != NULL);
    assert(node != NULL);
    assert(!node->in_queue);

    if (queue->size >= queue->capacity) {
        return -1; // Queue is full
    }

    if (queue->size == 0) { 
        // If queue is empty, add as the only element.
        queue->head = node;
        queue->tail = node;
        node->next = NULL;
    } else {
        // Traverse the queue to find the insertion point.
        node_item_t *current = queue->head;
        node_item_t *previous = NULL;

        while (current != NULL && current->priority <= node->priority) {
            previous = current;
            current = current->next;
        }

        if (previous == NULL) {
            // Insert at the head if it's the highest priority (lowest value).
            node->next = queue->head;
            queue->head = node;
        } else if (current == NULL) {
            // Insert at the tail if it’s the lowest priority.
            previous->next = node;
            node->next = NULL;
            queue->tail = node;
        } else {
            // Insert in the middle.
            previous->next = node;
            node->next = current;
        }
    }

    node->in_queue = true;
    queue->size++;
    return 0;
}