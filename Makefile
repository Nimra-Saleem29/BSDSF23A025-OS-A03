# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -Ibase-assignment-03/include

# Directories
SRC_DIR = base-assignment-03/src
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/shell.c $(SRC_DIR)/execute.c

# Object files
OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/shell.o $(OBJ_DIR)/execute.o

# Executable
TARGET = $(BIN_DIR)/myshell

# Default target
all: $(TARGET)

# Rule to build executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create obj directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean compiled files
clean:
	rm -rf $(OBJ_DIR)/*.o $(TARGET)
