#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define PROMPT "FCIT> "
#define MAX_ARGS 64
#define MAX_CMDS 16
#define HISTORY_SIZE 200

// Command structure to represent one simple command in a pipeline
typedef struct {
    char *argv[MAX_ARGS];   // NULL terminated argv for execvp
    char *input_file;       // filename after '<' or NULL
    char *output_file;      // filename after '>' or NULL
} Command;

// Parse the given line into an array of Command, returns 0 on success
int parse_pipeline(char *line, Command *cmds, int *num_cmds);

// Execute the pipeline of commands
int execute_pipeline(Command *cmds, int num_cmds);

// Built-in handler (returns 1 if a builtin was handled, 0 otherwise)
int handle_builtin(char **arglist);

// Utility to free memory allocated inside Command array
void free_commands(Command *cmds, int num_cmds);

#endif // SHELL_H
