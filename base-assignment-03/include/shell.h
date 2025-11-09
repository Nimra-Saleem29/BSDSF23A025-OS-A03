#ifndef SHELL_H
#define SHELL_H

// Standard headers needed by multiple files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#define PROMPT "FCIT> "
#define MAX_ARGS 64
#define MAX_CMDS 16
#define MAX_JOBS 128

// Represents a single simple command in a pipeline
typedef struct {
    char *argv[MAX_ARGS];   // argv for execvp, NULL terminated
    char *input_file;       // filename after '<' or NULL
    char *output_file;      // filename after '>' or NULL
} Command;

// Represents a background job
typedef struct {
    pid_t pid;              // PID of pipeline leader
    char *cmdline;          // allocated copy of original command string
} Job;

// Parsing and execution
int parse_pipeline(char *line, Command *cmds, int *num_cmds);
void free_commands(Command *cmds, int num_cmds);
// execute_pipeline runs the pipeline; if background==1, it registers a job and returns immediately
int execute_pipeline(Command *cmds, int num_cmds, int background, const char *orig_cmdline);

// Built-ins
int handle_builtin(char **arglist);

// Job management
int add_job(pid_t pid, const char *cmdline);
int remove_job_by_pid(pid_t pid);
void print_jobs(void);
void reap_jobs(void);

// Utility
char *trim(char *s);

#endif // SHELL_H
