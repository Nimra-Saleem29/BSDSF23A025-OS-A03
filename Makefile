CC = gcc
CFLAGS = -Wall -g -Ibase-assignment-03/include
LDFLAGS = -lreadline

SRC = base-assignment-03/src/main.c base-assignment-03/src/shell.c base-assignment-03/src/execute.c
OBJ = obj/main.o obj/shell.o obj/execute.o
BIN = bin/myshell

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

obj/main.o: base-assignment-03/src/main.c
	$(CC) $(CFLAGS) -c base-assignment-03/src/main.c -o obj/main.o

obj/shell.o: base-assignment-03/src/shell.c
	$(CC) $(CFLAGS) -c base-assignment-03/src/shell.c -o obj/shell.o

obj/execute.o: base-assignment-03/src/execute.c
	$(CC) $(CFLAGS) -c base-assignment-03/src/execute.c -o obj/execute.o

clean:
	rm -f obj/*.o $(BIN)
