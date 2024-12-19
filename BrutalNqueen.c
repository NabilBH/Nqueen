#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include "queue.h"
#include "board_t.h"
#include<unistd.h>

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
int counted = 0;
int kill = 0;
#define MY_TAG 1234
#define MESSAGE_REQUEST 1
#define MESSAGE_FORWARD 2
#define MESSAGE_KILL 3
#define MESSAGE_KILL_ACK 4
#define MESSAGE_TASKCOUNT 5

typedef struct {
	int type;
	board_t board;
	int completedTasks;
} message_t;

typedef struct{
	int expectedTasks;
    Queue* queue;
} ThreadArgs;

static int count_sol(board_t * b)
{
	count_count_sol ++;
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
				ct += pre_compute_boards(b, depth - 1,boards,board_index);
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
	while (alive) {
		MPI_Recv(&message, sizeof(message_t), MPI_BYTE, MPI_ANY_SOURCE, MY_TAG, MPI_COMM_WORLD, &status);
		switch (message.type) {
			case MESSAGE_REQUEST:
				requested = 1;
				requester = status.MPI_SOURCE;
				break;
			case MESSAGE_FORWARD:
				printf("Enqueue from %d\n",world_rank);
				
				enqueue(thArgs->queue,message.board);
				//counted += count_sol(&message.board);
				break;
			case MESSAGE_KILL: {
					message_t m = {MESSAGE_KILL_ACK};
					if (status.MPI_SOURCE != world_rank)
						MPI_Send(&m, sizeof(message_t), MPI_BYTE, status.MPI_SOURCE, MY_TAG, MPI_COMM_WORLD);
					alive = 0;
					printf("dying %d\n", world_rank);
				}
				break;
			case MESSAGE_TASKCOUNT: {
				//Check for completed tasks
				totalTasks += message.completedTasks;
				if(message.completedTasks!=0){
					printf("Received Task %d\n", message.completedTasks);
					printf("toalTasks %d\n",totalTasks);
				}
				//printf("Total tasks received at Master %d \n ",totalTasks);
				if(totalTasks == thArgs->expectedTasks && thArgs->expectedTasks != 0){
					printf("Killing\n");
					kill = 1;
				}
			}
			default:
				//printf("UNKNOWN???");
				break;
		}
	}
	return 0;
}

int MAX_BOARDS = 100;




int main(int argc, char** argv)
{
	//int i;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	printf("Hello world from rank %d out of %d processors\n", world_rank, world_size);
	
	Queue* q = create_queue(10);
	ThreadArgs thArgs = { .expectedTasks = 0, .queue =q};
	
	pthread_t receiver;
	board_t b = { .full = world_size, .count = 0 };
	int ct = 0;
	pthread_create(&receiver, 0, main_thread_loop_receive, &thArgs);

	if(world_rank == 0){
		// Array pour stocker les partial boards
		int board_index = 0; 	
		int depth = 2; 
		board_t partialBoards[MAX_BOARDS];
		thArgs.expectedTasks = pre_compute_boards(&b,depth,partialBoards,&board_index);

		printf("Pre computed Tasks %d\n", thArgs.expectedTasks);
		
		//distribute partial boards
		for (int i = 0; i < thArgs.expectedTasks; ++i) {
            int target_worker = (i % (world_size - 1)); 
			message_t partial_board = {MESSAGE_FORWARD, .board=partialBoards[i]};
            MPI_Send(&partial_board, sizeof(message_t), MPI_BYTE, target_worker, MY_TAG, MPI_COMM_WORLD);
        }
	} 
	int taskDone = 0;
	// while  q is not empty
	int working = 1;
	while(working)
	{
		if(is_empty(q)){
			// potential end of tasks
			message_t calculatedTasks = {MESSAGE_TASKCOUNT,.completedTasks = taskDone};
            MPI_Send(&calculatedTasks, sizeof(message_t), MPI_BYTE, 0, MY_TAG, MPI_COMM_WORLD);
			taskDone = 0;
		} else {
			board_t task = dequeue(q);
			// printf("Board for worker %d\n",world_rank);
			// for (int i = 0; i < task.count; i++) {
			// 	printf( "QUEEN at Row %d, Column %d\n",i, task.pos[i]);
    		// }
			
			ct = count_sol(&task);
			printf("Solutions found %d from worker %d \n",ct, world_rank);
			taskDone += 1;
		}

	}
	
	// add_queen(&b, world_rank);
	// double start = get_microseconds_from_epoch();
	// int ct = count_sol(&b);
	// double end = get_microseconds_from_epoch();
	int tot;
	//printf("count_count_sol=%d, time=%lf\n", count_count_sol, (end-start)/1e6);

	MPI_Reduce(&ct, &tot, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	printf("Reduced");
	//message_t m = {MESSAGE_REQUEST};
	//MPI_Send(&m, sizeof(message_t), MPI_BYTE, rand() % world_size, MY_TAG, MPI_COMM_WORLD);

	if (world_rank == 0 && kill == 1) {
		printf("nq=%d sol=%d\n", world_size, tot);
		message_t m = {MESSAGE_KILL};
		message_t r;
		MPI_Status status;
		for (int i=0; i<world_size; i++) {
			MPI_Send(&m, sizeof(message_t), MPI_BYTE, i, MY_TAG, MPI_COMM_WORLD);
			if (i)
				MPI_Recv(&r, sizeof(message_t), MPI_BYTE, i, MY_TAG, MPI_COMM_WORLD, &status);
		}
	}

	MPI_Finalize();

	printf("counted=%d\n", counted);
}
