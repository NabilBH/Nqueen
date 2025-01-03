#define _POSIX_C_SOURCE 199309L
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include "queue.h"
#include "board_t.h"
#include <unistd.h>
#include <time.h> // for nanosleep()

double get_microseconds_from_epoch()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)(tv.tv_sec) * 1e6 + (double)(tv.tv_usec);
}

int count_count_sol = 0;
int world_size, world_rank;
int requested = 0;
int requester = 0;
#define MY_TAG 1234
#define MESSAGE_REQUEST 1
#define MESSAGE_FORWARD 2
#define MESSAGE_KILL 3
#define MESSAGE_KILL_ACK 4
#define MESSAGE_TASKCOUNT 5

typedef struct {
	int type;
	int completedTasks;
	int solCount;
	board_t board;
} message_t;

typedef struct{
	int expectedTasks;
    Queue* queue;
	int running;
	int solCount;
} ThreadArgs;

static int count_sol(board_t * b)
{
	//count_count_sol ++;
	if (b->count == b->full)
		return 1;
	int ct = 0;
	for (int i = 0; i < b->full; i ++) {
		add_queen(b, i);
		if (is_viable(b)) {
			// if (requested) {
			// 	message_t m = {MESSAGE_FORWARD, *b};
			// 	MPI_Send(&m, sizeof(message_t), MPI_BYTE, requester, MY_TAG, MPI_COMM_WORLD);
			// 	requested = 0;
			// } else
			ct += count_sol(b);
		}
		drop_queen(b);
	}
	return ct;
}

static int pre_compute_boards(board_t* b,int depth,board_t* boards, int* board_index){
	if (b->count == b->full || depth == 0)
	{
        boards[*board_index] = *b;
        (*board_index)++;
		return 1 ;
	}
	int ct = 0;
	for (int i = 0; i < b->full; i ++) {
			add_queen(b, i);
			if (is_viable(b)) {
				ct += pre_compute_boards(b, depth - 1, boards,board_index);
			}
			drop_queen(b);
		}
	return ct;
}

void* main_thread_loop_receive(void* args)
{
	message_t message;
	MPI_Status status;
	ThreadArgs* thArgs = (ThreadArgs*)args;

	int totalTasks = 0;
	int alive = 1;
	int acknowledged = 0;
	while (alive) {
		MPI_Recv(&message, sizeof(message_t), MPI_BYTE, MPI_ANY_SOURCE, MY_TAG, MPI_COMM_WORLD, &status);
		switch (message.type) {
			case MESSAGE_REQUEST:
				requested = 1;
				requester = status.MPI_SOURCE;
				break;
			case MESSAGE_FORWARD:		
				//printf("Enqueue in worker %d\n",world_rank);
				enqueue(thArgs->queue,message.board);
				break;
			case MESSAGE_KILL: {
					message_t m = {MESSAGE_KILL_ACK};
					thArgs->running = 0;
					MPI_Send(&m, sizeof(message_t), MPI_BYTE, 0, MY_TAG, MPI_COMM_WORLD);

					alive = 0;
					printf("%d worker dying \n", world_rank);
				}
				break;
			case MESSAGE_TASKCOUNT: {
				//Check for completed tasks
				totalTasks += message.completedTasks;
				thArgs->solCount += message.solCount;

				printf("%d: Received from worker %d completed tasks / Running total :%d \n", world_rank,message.completedTasks, totalTasks);
				if(totalTasks == thArgs->expectedTasks && thArgs->expectedTasks != 0){
					printf("%d: Killing all\n", world_rank);
					message_t m = {MESSAGE_KILL};

					for (int i=1; i<world_size; i++) {
						MPI_Send(&m, sizeof(message_t), MPI_BYTE, i, MY_TAG, MPI_COMM_WORLD);
					}
				}
			}
				break;
			case MESSAGE_KILL_ACK:{
				acknowledged +=1;
				if(acknowledged == world_size-1){
					alive = 0;	
				}
			}
				break;
			default:
				printf("UNKNOWN???");
				break;
		}
	}
	return 0;
}



int main(int argc, char** argv)
{
	int MAX_BOARDS = 3500;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	
	Queue* q = create_queue(250);
	ThreadArgs thArgs = { .expectedTasks=0, .queue=q, .running=1, .solCount=0 };
	pthread_t receiver;
	int ct = 0;
	double start = 0;
	int emptyQueueHits = 0;
	int taskDone = 0;
	int totalLocalTasks = 0;
	int delay_sec = 0; 
	double delayNano = 100000;
	struct timespec req;
    req.tv_sec = delay_sec;
    req.tv_nsec = delayNano;

	pthread_create(&receiver, 0, main_thread_loop_receive, &thArgs);
	start = get_microseconds_from_epoch();

	if(world_rank == 0){
		board_t b = { .full = world_size, .count = 0 };
		printf("board size : %d\n", world_size);
		// Array pour stocker les partial boards
		int board_index = 0; 	
		int depth = 3; 
		board_t partialBoards[MAX_BOARDS];
		int preComputed= pre_compute_boards(&b, depth, partialBoards, &board_index);
		thArgs.expectedTasks = preComputed;
		printf("Pre computed Tasks %d\n", thArgs.expectedTasks);
		int target_worker = 0;
		//distribute partial boards
		for (int i = 0; i < preComputed; ++i) {
			//exclude master node from tasks
            target_worker = (i % (world_size - 1)) + 1; 
			//printf("board %d\n",i);
			message_t partial_board = {MESSAGE_FORWARD, .board=partialBoards[i]};
            MPI_Send(&partial_board, sizeof(message_t), MPI_BYTE, target_worker, MY_TAG, MPI_COMM_WORLD);
        }
		printf("Distribute work finished\n");
	} 
	
	while(world_rank!=0 && thArgs.running == 1)
	{
		if(is_empty(q)){
			if(taskDone != 0){			
				// potential end of tasks
				message_t calculatedTasks = {MESSAGE_TASKCOUNT, .completedTasks = taskDone, .solCount=ct};
				MPI_Send(&calculatedTasks, sizeof(message_t), MPI_BYTE, 0, MY_TAG, MPI_COMM_WORLD);
				taskDone = 0;
				ct=0;
				//baseDelay = 100000; // useconds
				req.tv_nsec = delayNano;
				req.tv_sec = delay_sec;

			} else {
				emptyQueueHits+=1;	
				//printf("%d: Waiting for %ld seconds and %ld nanoseconds...\n",world_rank, req.tv_sec, req.tv_nsec);
				nanosleep(&req, NULL); 
				//sleep(1);
				req.tv_nsec *= 5;
				req.tv_sec *= 2;
		
			}
		} else {
			//printf("%d: dequeue\n",world_rank);
			board_t task = dequeue(q);;
			
			ct += count_sol(&task);
			taskDone += 1;
			totalLocalTasks+=1;
			req.tv_nsec = delayNano;
			req.tv_sec = delay_sec;

		}
	}

	void* dummy;
	pthread_join(receiver,&dummy);
	destroy_queue(q);
	MPI_Finalize();
	double end = get_microseconds_from_epoch();

	printf("%d: time=%lf\n", world_rank, (end-start)/1e6);

	if (world_rank == 0){
		printf("Queens=%d, Solutions=%d\n", world_size, thArgs.solCount);
	}
	return 0;
}
