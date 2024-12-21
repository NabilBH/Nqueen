#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

// Initialize the queue
Queue* create_queue(int capacity) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->data = (board_t*)malloc(capacity * sizeof(board_t));
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    q->capacity = capacity;
    pthread_mutex_init(&q->lock, NULL); // Initialize the mutex
    return q;
}

int is_empty(Queue* q) {
    int result;
    pthread_mutex_lock(&q->lock);  // Lock the mutex to ensure thread-safety
    result = (q->size == 0);  // Check if the queue is empty
    pthread_mutex_unlock(&q->lock);  // Unlock the mutex
    return result;
}

// Enqueue an item
void enqueue(Queue* q, board_t item) {
    pthread_mutex_lock(&q->lock); // Lock the queue
    if (q->size == q->capacity) {
        printf("Queue is full!\n");
    } else {
        q->rear = (q->rear + 1) % q->capacity;
        q->data[q->rear] = item;
        q->size++;
    }
    pthread_mutex_unlock(&q->lock); // Unlock the queue
}

// Dequeue an item
board_t dequeue(Queue* q) {
    pthread_mutex_lock(&q->lock); // Lock the queue
    board_t item;
    if (q->size == 0) {
        printf("Queue is empty!\n");
    } else {
        item = q->data[q->front];
        q->front = (q->front + 1) % q->capacity;
        q->size--;
    }
    pthread_mutex_unlock(&q->lock); // Unlock the queue
    return item;
}

void destroy_queue(Queue* q) {
    pthread_mutex_destroy(&q->lock); // Destroy the mutex
    free(q->data);                   // Free the data array
    free(q);                         // Free the queue structure
}
