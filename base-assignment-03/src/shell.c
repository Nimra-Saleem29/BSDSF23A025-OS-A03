#include "shell.h"

// Helper: trim leading/trailing spaces (in-place)
static char *trim(char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0'; end--;
    }
    return s;
}

// Split line into segments by '|' into cmds[].
// For each segment, parse tokens and fill Command fields.
// Returns 0 on success, -1 on parse error.
int parse_pipeline(char *line, Command *cmds, int *num_cmds) {
    *num_cmds = 0;
    if (!line) return -1;

    // Duplicate line so strtok doesn't modify caller's original (caller should pass strdup if needed)
    char *ln = line;

    // Use strtok_r to split by '|'
    char *saveptr_seg = NULL;
    char *segment = strtok_r(ln, "|", &saveptr_seg);
    while (segment != NULL) {
        if (*num_cmds >= MAX_CMDS) {
            fprintf(stderr, "Error: too many commands in pipeline (max %d)\n", MAX_CMDS);
            return -1;
        }
        Command *cmd = &cmds[*num_cmds];
        // initialize
        for (int i = 0; i < MAX_ARGS; ++i) cmd->argv[i] = NULL;
        cmd->input_file = NULL;
        cmd->output_file = NULL;

        // Trim segment
        segment = trim(segment);
        if (segment[0] == '\0') {
            fprintf(stderr, "Parse error: empty command in pipeline\n");
            return -1;
        }

        // Tokenize by whitespace within the segment
        char *saveptr_tok = NULL;
        char *tok = strtok_r(segment, " \t\r\n", &saveptr_tok);
        int argi = 0;
        while (tok != NULL) {
            if (strcmp(tok, "<") == 0) {
                // next token must be filename
                tok = strtok_r(NULL, " \t\r\n", &saveptr_tok);
                if (!tok) { fprintf(stderr, "Parse error: expected filename after '<'\n"); return -1; }
                cmd->input_file = strdup(tok);
            } else if (strcmp(tok, ">") == 0) {
                tok = strtok_r(NULL, " \t\r\n", &saveptr_tok);
                if (!tok) { fprintf(stderr, "Parse error: expected filename after '>'\n"); return -1; }
                cmd->output_file = strdup(tok);
            } else {
                if (argi >= MAX_ARGS - 1) {
                    fprintf(stderr, "Error: too many arguments (max %d)\n", MAX_ARGS - 1);
                    return -1;
                }
                cmd->argv[argi++] = strdup(tok);
            }
            tok = strtok_r(NULL, " \t\r\n", &saveptr_tok);
        }
        cmd->argv[argi] = NULL;

        if (cmd->argv[0] == NULL) {
            fprintf(stderr, "Parse error: empty command\n");
            return -1;
        }

        (*num_cmds)++;
        segment = strtok_r(NULL, "|", &saveptr_seg);
    }

    return 0;
}

// Free memory allocated for Command array (argv strings and redirection strings)
void free_commands(Command *cmds, int num_cmds) {
    for (int i = 0; i < num_cmds; ++i) {
        Command *c = &cmds[i];
        for (int j = 0; j < MAX_ARGS && c->argv[j] != NULL; ++j) {
            free(c->argv[j]);
            c->argv[j] = NULL;
        }
        if (c->input_file) { free(c->input_file); c->input_file = NULL; }
        if (c->output_file) { free(c->output_file); c->output_file = NULL; }
    }
}

// Simple check for built-in commands. Only used for single non-piped commands.
// Returns 1 if builtin handled (executed), 0 otherwise.
int handle_builtin(char **arglist) {
    if (!arglist || !arglist[0]) return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        exit(0);
    } else if (strcmp(arglist[0], "cd") == 0) {
        if (!arglist[1]) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(arglist[1]) != 0) perror("cd");
        }
        return 1;
    } else if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("exit - exit shell\n");
        printf("cd <dir> - change directory\n");
        printf("help - display this message\n");
        printf("jobs - list jobs (placeholder)\n");
        printf("history - show command history\n");
        return 1;
    } else if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    } else if (strcmp(arglist[0], "history") == 0) {
        HIST_ENTRY **hist_list = history_list();
        if (hist_list) {
            for (int i = 0; hist_list[i]; ++i)
                printf("%d %s\n", i + 1, hist_list[i]->line);
        }
        return 1;
    }

    return 0;
}

// Execute pipeline: cmds[0..num_cmds-1]
// - If num_cmds == 1 and it's a builtin, run builtin in shell (handle_builtin).
// - Otherwise set up pipes and forks, handle < and > in children.
int execute_pipeline(Command *cmds, int num_cmds) {
    if (num_cmds <= 0) return -1;

    // Single command: check built-in
    if (num_cmds == 1) {
        if (handle_builtin(cmds[0].argv)) return 0;
    }

    int n = num_cmds;
    int pipefds[2 * (n - 1)]; // store pipe fds: for i-th pipe: pipefds[2*i], pipefds[2*i+1]

    // Create pipes
    for (int i = 0; i < n - 1; ++i) {
        if (pipe(pipefds + 2*i) < 0) {
            perror("pipe");
            return -1;
        }
    }

    pid_t pids[n];
    for (int i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // close fds
            for (int k = 0; k < 2*(n-1); ++k) close(pipefds[k]);
            return -1;
        }
        if (pid == 0) {
            // Child process

            // If not first command, redirect stdin to read end of previous pipe
            if (i > 0) {
                int fd_in = pipefds[(i-1)*2];
                if (dup2(fd_in, STDIN_FILENO) < 0) { perror("dup2 stdin"); exit(1); }
            }

            // If not last command, redirect stdout to write end of current pipe
            if (i < n-1) {
                int fd_out = pipefds[i*2 + 1];
                if (dup2(fd_out, STDOUT_FILENO) < 0) { perror("dup2 stdout"); exit(1); }
            }

            // Close all pipe fds inherited
            for (int k = 0; k < 2*(n-1); ++k) close(pipefds[k]);

            // Handle input redirection for this command (overrides pipe if both present)
            if (cmds[i].input_file) {
                int fd = open(cmds[i].input_file, O_RDONLY);
                if (fd < 0) { perror("open input"); exit(1); }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 infile"); close(fd); exit(1); }
                close(fd);
            }

            // Handle output redirection for this command (overrides pipe if both present)
            if (cmds[i].output_file) {
                int fd = open(cmds[i].output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("open output"); exit(1); }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 outfile"); close(fd); exit(1); }
                close(fd);
            }

            // Execute command
            execvp(cmds[i].argv[0], cmds[i].argv);
            // If execvp returns, it's an error
            perror("execvp");
            exit(1);
        } else {
            // Parent: store pid
            pids[i] = pid;
        }
    }

    // Parent closes all pipe fds
    for (int k = 0; k < 2*(n-1); ++k) close(pipefds[k]);

    // Wait for all children
    int status = 0;
    for (int i = 0; i < n; ++i) {
        waitpid(pids[i], &status, 0);
    }
    return 0;
}
