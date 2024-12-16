# Compiler
CC = mpicc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11 -g -pthread

# Source files and output
SRC = brutalNqueen.c
TARGET = bin/brutalNQueen

# Default rule
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Clean rule to remove the executable
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean
