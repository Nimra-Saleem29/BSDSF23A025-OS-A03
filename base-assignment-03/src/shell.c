#define _GNU_SOURCE
#include "shell.h"

// ---------------------- Utility: trim ----------------------
char *trim(char *s) {
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) { *end = '\0'; end--; }
    return s;
}

// ---------------------- Parsing: parse_pipeline ----------------------
// Splits a command line by '|' into Command structs.
// Caller should pass a writable string (e.g., strdup(line)) because strtok_r modifies it.
// Returns 0 on success, -1 on parse error.
int parse_pipeline(char *line, Command *cmds, int *num_cmds) {
    if (!line || !cmds || !num_cmds) return -1;
    *num_cmds = 0;

    char *save_seg = NULL;
    char *segment = strtok_r(line, "|", &save_seg);
    while (segment != NULL) {
        if (*num_cmds >= MAX_CMDS) {
            fprintf(stderr, "Error: too many commands in pipeline (max %d)\n", MAX_CMDS);
            return -1;
        }
        Command *cmd = &cmds[*num_cmds];
        // init command
        for (int i = 0; i < MAX_ARGS; ++i) cmd->argv[i] = NULL;
        cmd->input_file = NULL;
        cmd->output_file = NULL;

        char *seg = trim(segment);
        if (seg[0] == '\0') { fprintf(stderr, "Parse error: empty command in pipeline\n"); return -1; }

        char *save_tok = NULL;
        char *tok = strtok_r(seg, " \t\r\n", &save_tok);
        int ai = 0;
        while (tok != NULL) {
            if (strcmp(tok, "<") == 0) {
                tok = strtok_r(NULL, " \t\r\n", &save_tok);
                if (!tok) { fprintf(stderr, "Parse error: expected filename after '<'\n"); return -1; }
                cmd->input_file = strdup(tok);
            } else if (strcmp(tok, ">") == 0) {
                tok = strtok_r(NULL, " \t\r\n", &save_tok);
                if (!tok) { fprintf(stderr, "Parse error: expected filename after '>'\n"); return -1; }
                cmd->output_file = strdup(tok);
            } else {
                if (ai >= MAX_ARGS - 1) { fprintf(stderr, "Error: too many arguments\n"); return -1; }
                cmd->argv[ai++] = strdup(tok);
            }
            tok = strtok_r(NULL, " \t\r\n", &save_tok);
        }
        cmd->argv[ai] = NULL;

        if (cmd->argv[0] == NULL) { fprintf(stderr, "Parse error: empty command\n"); return -1; }

        (*num_cmds)++;
        segment = strtok_r(NULL, "|", &save_seg);
    }
    return 0;
}

// ---------------------- Free allocated strings inside commands ----------------------
void free_commands(Command *cmds, int num_cmds) {
    for (int i = 0; i < num_cmds; ++i) {
        for (int j = 0; j < MAX_ARGS && cmds[i].argv[j] != NULL; ++j) {
            free(cmds[i].argv[j]);
            cmds[i].argv[j] = NULL;
        }
        if (cmds[i].input_file) { free(cmds[i].input_file); cmds[i].input_file = NULL; }
        if (cmds[i].output_file) { free(cmds[i].output_file); cmds[i].output_file = NULL; }
    }
}

// ---------------------- Built-ins ----------------------
int handle_builtin(char **arglist) {
    if (!arglist || !arglist[0]) return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        exit(0);
    } else if (strcmp(arglist[0], "cd") == 0) {
        if (!arglist[1]) fprintf(stderr, "cd: missing argument\n");
        else if (chdir(arglist[1]) != 0) perror("cd");
        return 1;
    } else if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n"
               " exit - exit shell\n"
               " cd <dir> - change directory\n"
               " help - display this message\n"
               " jobs - list background jobs\n"
               " history - show command history\n");
        return 1;
    } else if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    } else if (strcmp(arglist[0], "history") == 0) {
        HIST_ENTRY **hist_list = history_list();
        if (hist_list) {
            for (int i = 0; hist_list[i]; ++i) printf("%d %s\n", i + 1, hist_list[i]->line);
        }
        return 1;
    }
    return 0;
}

// ---------------------- Job management ----------------------
static Job jobs[MAX_JOBS];
static int jobs_count = 0;

int add_job(pid_t pid, const char *cmdline) {
    if (jobs_count >= MAX_JOBS) return -1;
    jobs[jobs_count].pid = pid;
    jobs[jobs_count].cmdline = cmdline ? strdup(cmdline) : strdup("(bg)");
    jobs_count++;
    return 0;
}

int remove_job_by_pid(pid_t pid) {
    for (int i = 0; i < jobs_count; ++i) {
        if (jobs[i].pid == pid) {
            free(jobs[i].cmdline);
            // shift remaining jobs left
            for (int j = i; j < jobs_count - 1; ++j) jobs[j] = jobs[j + 1];
            jobs_count--;
            return 0;
        }
    }
    return -1;
}

void print_jobs(void) {
    if (jobs_count == 0) { printf("No background jobs.\n"); return; }
    for (int i = 0; i < jobs_count; ++i) {
        printf("[%d] PID=%d  %s\n", i + 1, jobs[i].pid, jobs[i].cmdline);
    }
}

// Reap finished background children using WNOHANG and remove them from jobs[]
void reap_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // If pid is in jobs list, remove it
        remove_job_by_pid(pid);
        // You can optionally print status info here:
        // if (WIFEXITED(status)) printf("Job PID %d exited with %d\n", pid, WEXITSTATUS(status));
    }
}

// ---------------------- Execution: pipelines + redirection + background ----------------------
// Execute pipeline of num_cmds commands. If background==1, do not wait and register job (first child's PID).
int execute_pipeline(Command *cmds, int num_cmds, int background, const char *orig_cmdline) {
    if (num_cmds <= 0) return -1;

    // If single command and builtin, handle in shell (unless background requested)
    if (num_cmds == 1 && !background && handle_builtin(cmds[0].argv)) {
        return 0;
    }

    int n = num_cmds;
    int pipefds[2 * (n - 1)]; // for n commands, n-1 pipes -> 2*(n-1) fds
    pid_t pids[n];

    // create pipes
    for (int i = 0; i < n - 1; ++i) {
        if (pipe(pipefds + 2 * i) < 0) {
            perror("pipe");
            // close any created fds
            for (int k = 0; k < 2 * i; ++k) close(pipefds[k]);
            return -1;
        }
    }

    for (int i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // cleanup fds
            for (int k = 0; k < 2 * (n - 1); ++k) close(pipefds[k]);
            return -1;
        }
        if (pid == 0) {
            // Child process

            // If not first, set stdin to read end of previous pipe
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) { perror("dup2 stdin"); exit(1); }
            }

            // If not last, set stdout to write end of current pipe
            if (i < n - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) { perror("dup2 stdout"); exit(1); }
            }

            // Close all pipe fds in child
            for (int k = 0; k < 2 * (n - 1); ++k) close(pipefds[k]);

            // Input redirection
            if (cmds[i].input_file) {
                int fd = open(cmds[i].input_file, O_RDONLY);
                if (fd < 0) { perror("open input"); exit(1); }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 infile"); close(fd); exit(1); }
                close(fd);
            }

            // Output redirection
            if (cmds[i].output_file) {
                int fd = open(cmds[i].output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("open output"); exit(1); }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 outfile"); close(fd); exit(1); }
                close(fd);
            }

            // Execute
            execvp(cmds[i].argv[0], cmds[i].argv);
            // If execvp returns -> error
            perror("execvp");
            exit(1);
        } else {
            // parent
            pids[i] = pid;
        }
    }

    // Parent closes all pipe fds
    for (int k = 0; k < 2 * (n - 1); ++k) close(pipefds[k]);

    if (background) {
        // Register job using first child's PID (simple pipeline leader)
        if (add_job(pids[0], orig_cmdline) < 0) {
            fprintf(stderr, "Warning: job list full, cannot track background job\n");
        } else {
            printf("[bg] started PID %d\n", pids[0]);
        }
        // Note: do NOT wait for children in background
        return 0;
    } else {
        // Foreground: wait for all children
        int status;
        for (int i = 0; i < n; ++i) waitpid(pids[i], &status, 0);
        return 0;
    }
}
