# Compiler
CC = mpicc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c99 -g -pthread -lrt

# Source files and output
SRC = brutalNqueen.c queue.c board_t.c
TARGET = bin/brutalNQueen

# Default rule
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRC) | bin
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Rule to create the bin directory
bin:
	mkdir -p bin

# Clean rule to remove the executable and bin directory
clean:
	rm -f $(TARGET)
	rm -rf bin

# Phony targets
.PHONY: all clean bin
