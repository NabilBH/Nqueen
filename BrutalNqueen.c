#define _POSIX_C_SOURCE 199309L
#define MY_TAG 1234
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include "queue.h"
#include "board_t.h"
#include "message.h"

double get_microseconds_from_epoch()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)(tv.tv_sec) * 1e6 + (double)(tv.tv_usec);
}

int write_to_file(int world_rank, double time_taken,double totalWait){
	char filename[100];
	time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(filename, "mpi_results_%d-%02d-%02d_%02d-%02d-%02d.csv",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min,tm.tm_sec);

    // Write the output to the CSV file
    FILE *file = fopen(filename, "a");  // Open the CSV file in append mode
    if (file == NULL) {
        fprintf(stderr, "Could not open file for writing\n");
        return 1;
    }
	if(world_rank == 0){
		totalWait = 0;
	}
    // Write the world_rank and time_taken to the CSV file
    fprintf(file, "%d,%lf,%lf\n",world_rank, time_taken/1e6, totalWait/1e6);

    // Close the file
    fclose(file);
	return 0;
}

typedef struct{
	int expectedTasks;
    Queue* queue;
	int running;
	int solCount;
	int world_size; 
	int world_rank;
} ThreadArgs;

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
			case MESSAGE_FORWARD:		
				enqueue(thArgs->queue,message.board);
				break;
			case MESSAGE_KILL: {
					message_t m = {MESSAGE_KILL_ACK, .completedTasks=0, .solCount=0, .board={0} };
					thArgs->running = 0;
					MPI_Send(&m, sizeof(message_t), MPI_BYTE, 0, MY_TAG, MPI_COMM_WORLD);
					alive = 0;
				}
				break;
			case MESSAGE_TASKCOUNT: {
				//Check for completed tasks
				totalTasks += message.completedTasks;
				thArgs->solCount += message.solCount;
				if(totalTasks == thArgs->expectedTasks && thArgs->expectedTasks != 0){
					message_t m = { .type=MESSAGE_KILL, .completedTasks=0, .solCount=0, .board={0} };
					for (int i=1; i<thArgs->world_size; i++) {
						MPI_Send(&m, sizeof(message_t), MPI_BYTE, i, MY_TAG, MPI_COMM_WORLD);
					}
				}
			}
				break;
			case MESSAGE_KILL_ACK:{
				acknowledged +=1;
				if(acknowledged == thArgs->world_size-1){
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

int distribute_work(board_t* partialBoards, int nbrWorkers,int initBoardDepth,int boardSize){
	printf("board size : %d\n", boardSize);
	// Array pour stocker les partial boards
	int board_index = 0; 	
	board_t b = { .full = boardSize, .count = 0 };

	int preComputed= pre_compute_boards(&b, initBoardDepth, partialBoards, &board_index);
	printf("Pre computed Tasks %d\n", preComputed);

	//distribute partial boards
	for (int i = 0; i < preComputed; i++) {
		//exclude master node from tasks
		int target_worker = (i % (nbrWorkers-1)) + 1; 
		message_t partial_board = {MESSAGE_FORWARD, .board=partialBoards[i]};
		int mpi_err=MPI_Send(&partial_board, sizeof(message_t), MPI_BYTE, target_worker, MY_TAG, MPI_COMM_WORLD);
		if (mpi_err != MPI_SUCCESS) {
            fprintf(stderr, "Error: MPI_Send failed to worker %d.\n", target_worker);
            return -1;
        }
	}

	return preComputed;
}

int main(int argc, char** argv)
{	
	int queens = 8;
	// Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 < argc) {
                queens = atoi(argv[i + 1]);
                if (queens <= 0) {
                    fprintf(stderr, "Error: Number of queens must be a positive integer.\n");
                    MPI_Finalize();
                    return EXIT_FAILURE;
                }
                i++; // Skip the value for -n
            } else {
                fprintf(stderr, "Error: Missing value for -n.\n");
                MPI_Finalize();
                return EXIT_FAILURE;
            }
        }
    }

    // Debugging output to verify number of queens

	int world_size = 0;
	int world_rank = 0;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	

	if (world_rank == 0) {
        printf("Number of queens: %d\n", queens);
    }
	Queue* q = create_queue(5000);
	ThreadArgs thArgs = { 
		.expectedTasks=0, 
		.queue=q,
		.running=1, 
		.solCount=0, 
		.world_rank= world_rank,
		.world_size= world_size 
		};

	pthread_t receiver;
	int ct = 0;
	double start = 0;
	int taskDone = 0;
	double startWait = 0;
	double endWait = 0;
	double totalWait = 0;
	int waiting = 0;
	pthread_create(&receiver, 0, main_thread_loop_receive, &thArgs);
	start = get_microseconds_from_epoch();
	if(world_rank == 0){

		int depth = 2;
		int MAX_BOARDS = 5000;

		board_t partialBoards[MAX_BOARDS];

		thArgs.expectedTasks = distribute_work(partialBoards,world_size,depth,queens);
		printf("DOne");
	} 
	while(world_rank!=0 && thArgs.running == 1)
	{
		if(is_empty(q)){
			if(taskDone != 0){		
				startWait = get_microseconds_from_epoch();
				waiting = 1;
				// potential end of tasks
				message_t calculatedTasks = {MESSAGE_TASKCOUNT, .completedTasks = taskDone, .solCount=ct};
				MPI_Send(&calculatedTasks, sizeof(message_t), MPI_BYTE, 0, MY_TAG, MPI_COMM_WORLD);
				
				taskDone = 0;
				ct=0;
			}
		} else {
			board_t task = dequeue(q);;
			ct += count_sol(&task);
			taskDone += 1;
			if (waiting){
				endWait = get_microseconds_from_epoch();
				if(endWait - startWait < 0){
					printf("Error wait time \n");
				}
				totalWait +=(endWait-startWait);
				waiting = 0;
			}

		}
	}

	void* dummy;
	pthread_join(receiver,&dummy);
	destroy_queue(q);
	MPI_Finalize();
	double end = get_microseconds_from_epoch();
	if (waiting){
		endWait = get_microseconds_from_epoch(),
		totalWait += (endWait - startWait);
	}
	if (world_rank == 0){
		printf("Queens=%d, Solutions=%d\n", queens, thArgs.solCount);
	}

	double time_taken = (end-start);
	write_to_file(world_rank,time_taken,totalWait);
	return 0;
}
