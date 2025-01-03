#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "board_t.h"

typedef struct {
    board_t* data;        // Pointer to the queue's data
    int front, rear;  // Indices for the front and rear
    int size;         // Current size of the queue
    int capacity;     // Maximum capacity of the queue
    pthread_mutex_t lock; // Mutex for thread-safe operations
} Queue;

Queue* create_queue(int capacity);
void enqueue(Queue* q, board_t item);
board_t dequeue(Queue* q);
void destroy_queue(Queue* q);
int is_empty(Queue* q);
#endif 
