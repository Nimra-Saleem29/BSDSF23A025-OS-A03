#ifndef SHELL_H
#define SHELL_H

#define _GNU_SOURCE
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

typedef struct {
    char *argv[MAX_ARGS];   // argv for execvp, NULL terminated
    char *input_file;       // filename for '<'
    char *output_file;      // filename for '>'
} Command;

typedef struct {
    pid_t pid;
    char *cmdline;
} Job;

/* Parsing and memory */
int parse_pipeline(char *line, Command *cmds, int *num_cmds);
void free_commands(Command *cmds, int num_cmds);
char *trim(char *s);

/* Execution */
/* execute_pipeline:
 *  - if background == 0: returns exit code (0..255) of pipeline, or -1 on parse/exec error.
 *  - if background == 1: registers background job and returns 0 on success or -1 on error.
 */
int execute_pipeline(Command *cmds, int num_cmds, int background, const char *orig_cmdline);

/* Builtins */
int handle_builtin_status(char **arglist, int *status);

/* Job management */
int add_job(pid_t pid, const char *cmdline);
int remove_job_by_pid(pid_t pid);
void print_jobs(void);
void reap_jobs(void);

#endif // SHELL_H
