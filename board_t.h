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
int count_sol(board_t * b);
int pre_compute_boards(board_t* b,int depth,board_t* boards, int* board_index);
void copy_board_t(const board_t *src, board_t *dest);
#endif