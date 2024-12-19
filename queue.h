#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "board_t.h"

// Queue structure definition
typedef struct {
    board_t* data;        // Pointer to the queue's data
    int front, rear;  // Indices for the front and rear
    int size;         // Current size of the queue
    int capacity;     // Maximum capacity of the queue
    pthread_mutex_t lock; // Mutex for thread-safe operations
} Queue;

// Function declarations
Queue* create_queue(int capacity);
void enqueue(Queue* q, board_t item);
board_t dequeue(Queue* q);
void destroy_queue(Queue* q);

#endif // QUEUE_H
