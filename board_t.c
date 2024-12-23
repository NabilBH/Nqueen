#include "board_t.h"

void add_queen(board_t * b, int q) {b->pos[b->count ++] = q;}
int drop_queen(board_t * b) {return b->pos[-- b->count];}
int is_viable(board_t *b)
{
	int lastp = b->count - 1;
	int last = b->pos[lastp];
	for (int i=0; i<lastp; i++)
		if ((b->pos[i] == last) || ((lastp-i) == (last-b->pos[i])) || ((i-lastp) == (last-b->pos[i])))
			return 0;
	return 1;
}