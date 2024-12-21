#ifndef board_t_h
#define board_t_h

#define MAX 20

typedef struct {
	int full;
	int count;
	int pos[MAX];
} board_t;


void add_queen(board_t * b, int q);
int drop_queen(board_t * b);
int is_viable(board_t *b);
#endif