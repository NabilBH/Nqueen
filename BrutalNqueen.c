#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>

double get_microseconds_from_epoch()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)(tv.tv_sec) * 1e6 + (double)(tv.tv_usec);
}

#define MAX 20

typedef struct {
	int full;
	int count;
	int pos[MAX];
} board_t;

int count_count_sol = 0;
int world_size, world_rank;
int requested = 0;
int requester = 0;
int counted = 0;

#define MY_TAG 1234



#define MESSAGE_REQUEST 1
#define MESSAGE_FORWARD 2
#define MESSAGE_KILL 3
#define MESSAGE_KILL_ACK 4

typedef struct {
	int type;
	board_t board;
} message_t;


static void add_queen(board_t * b, int q)	{b->pos[b->count ++] = q;}
static int drop_queen(board_t * b)		{return b->pos[-- b->count];}

static int is_viable(board_t *b)
{
	int lastp = b->count - 1;
	int last = b->pos[lastp];
	for (int i=0; i<lastp; i++)
		if ((b->pos[i] == last) || ((lastp-i) == (last-b->pos[i])) || ((i-lastp) == (last-b->pos[i])))
			return 0;
	return 1;
}

static int count_sol(board_t * b)
{
	count_count_sol ++;
	if (b->count == b->full)
		return 1;
	int ct = 0;
	for (int i = 0; i < b->full; i ++) {
		add_queen(b, i);
		if (is_viable(b)) {
			if (requested) {
				message_t m = {MESSAGE_FORWARD, *b};
				MPI_Send(&m, sizeof(message_t), MPI_BYTE, requester, MY_TAG, MPI_COMM_WORLD);
				requested = 0;
			} else
				ct += count_sol(b);
		}
		drop_queen(b);
	}
	return ct;
}

static int pre_compute_boards(board_t* b,int depth,board_t* boards, int* board_index){
	if (b->count == b->full || depth == 0)
	{
		 // Copy the current board state into the array
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

void* main_thread_loop_receive(void* p)
{
	message_t message;
	MPI_Status status;
	int alive = 1;
	while (alive) {
		MPI_Recv(&message, sizeof(message_t), MPI_BYTE, MPI_ANY_SOURCE, MY_TAG, MPI_COMM_WORLD, &status);
		switch (message.type) {
			case MESSAGE_REQUEST:
				requested = 1;
				requester = status.MPI_SOURCE;
				break;
			case MESSAGE_FORWARD:
				counted += count_sol(&message.board);
				break;
			case MESSAGE_KILL: {
					message_t m = {MESSAGE_KILL_ACK};
					if (status.MPI_SOURCE != world_rank)
						MPI_Send(&m, sizeof(message_t), MPI_BYTE, status.MPI_SOURCE, MY_TAG, MPI_COMM_WORLD);
					alive = 0;
					printf("dying %d\n", world_rank);
				}
				break;
			default:
				printf("UNKNOWN???");
				break;
		}
	}
	return 0;
}

int MAX_BOARDS = 100;

int main(int argc, char** argv)
{
	int i;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	printf("Hello world from rank %d out of %d processors\n", world_rank, world_size);
	 // Array to store precomputed boards
	pthread_t receiver;
	pthread_create(&receiver, 0, main_thread_loop_receive, 0);
	board_t b = { .full = world_size, .count = 0 };

	if(world_rank == 0){
		int board_index = 0; 	
		int depth = 2; 
		board_t boards[MAX_BOARDS];
		printf("world ranks %d \n ", world_rank);
		int expectedTasks = pre_compute_boards(&b,depth,boards,&board_index);
		printf("Pre computed Tasks %d\n", expectedTasks);
	}
	
	return;
	add_queen(&b, world_rank);
	double start = get_microseconds_from_epoch();
	int ct = count_sol(&b);
	double end = get_microseconds_from_epoch();


	int tot;
	printf("count_count_sol=%d, time=%lf\n", count_count_sol, (end-start)/1e6);

	MPI_Reduce(&ct, &tot, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

	message_t m = {MESSAGE_REQUEST};
	MPI_Send(&m, sizeof(message_t), MPI_BYTE, rand() % world_size, MY_TAG, MPI_COMM_WORLD);

	if (world_rank == 0) {
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
