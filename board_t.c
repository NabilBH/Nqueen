#include "board_t.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void add_queen(board_t * b, int q) 
{
	if (b->count > MAX) {
		printf("Error: Too many queens placed %d queens\n", b->count);
		exit(EXIT_FAILURE);
	}
	b->pos[b->count ++] = q;
}

int drop_queen(board_t * b) 
{
	return b->pos[-- b->count];

}

int is_viable(board_t *b)
{
	int lastp = b->count - 1;
	int last = b->pos[lastp];
	for (int i=0; i<lastp; i++)
		if ((b->pos[i] == last) || ((lastp-i) == (last-b->pos[i])) || ((i-lastp) == (last-b->pos[i])))
			return 0;
	return 1;
}

int count_sol(board_t * b)
{
	if (b->count == b->full)
		return 1;
	int ct = 0;
	for (int i = 0; i < b->full; i ++) {
		add_queen(b, i);
		if (is_viable(b)) {

			ct += count_sol(b);
		}
		drop_queen(b);
	}
	return ct;
}

int pre_compute_boards(board_t* b,int depth,board_t* boards, int* board_index){
	if (!b || !boards || !board_index) {
        fprintf(stderr, "Error: Null pointer in pre_compute_boards.\n");
        return 0;
    }
    if (b->full < 0 || b->full > MAX) {
        fprintf(stderr, "Error: Invalid board size in pre_compute_boards.\n");
        return 0;
    }
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