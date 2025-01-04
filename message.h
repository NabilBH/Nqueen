#include "board_t.h"
#define MESSAGE_FORWARD 1
#define MESSAGE_KILL 2
#define MESSAGE_KILL_ACK 3
#define MESSAGE_TASKCOUNT 4

typedef struct {
	int type;
	int completedTasks;
	int solCount;
	board_t board;
} message_t;

