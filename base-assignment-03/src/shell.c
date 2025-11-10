#define _GNU_SOURCE
#include "shell.h"

/* ----------------- Utility: trim ----------------- */
char *trim(char *s) {
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) { *end = '\0'; end--; }
    return s;
}

/* ----------------- Parsing: parse_pipeline -----------------
 * Splits by '|' and builds Command[] with argv[], input_file, output_file.
 * Caller must pass a writable string (we use strdup in caller).
 * Returns 0 on success, -1 on parse error.
 */
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

/* ----------------- Free memory inside commands ----------------- */
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

/* ----------------- Variables handling ----------------- */
/* Linked list of VarNode */
static VarNode *vars_head = NULL;

static VarNode *find_var(const char *name) {
    VarNode *cur = vars_head;
    while (cur) {
        if (strcmp(cur->name, name) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

int set_variable(const char *name, const char *value) {
    if (!name) return -1;
    VarNode *node = find_var(name);
    if (node) {
        /* replace value */
        free(node->value);
        node->value = value ? strdup(value) : strdup("");
        return 0;
    }
    /* create new node */
    VarNode *n = malloc(sizeof(VarNode));
    if (!n) return -1;
    n->name = strdup(name);
    n->value = value ? strdup(value) : strdup("");
    n->next = vars_head;
    vars_head = n;
    return 0;
}

const char *get_variable(const char *name) {
    VarNode *n = find_var(name);
    if (!n) return NULL;
    return n->value;
}

void print_variables(void) {
    VarNode *cur = vars_head;
    if (!cur) { printf("No variables set.\n"); return; }
    while (cur) {
        printf("%s=%s\n", cur->name, cur->value ? cur->value : "");
        cur = cur->next;
    }
}

void free_all_variables(void) {
    VarNode *cur = vars_head;
    while (cur) {
        VarNode *tmp = cur;
        cur = cur->next;
        free(tmp->name);
        free(tmp->value);
        free(tmp);
    }
    vars_head = NULL;
}

/* Expand variables in a single token which may contain a leading '$' only.
 * For now we support tokens that are exactly $VAR or "${VAR}".
 * Returns a newly allocated string (caller should free) or NULL on error.
 * If variable undefined, replace with empty string.
 */
static char *expand_token(const char *token) {
    if (!token) return NULL;
    if (token[0] != '$') return strdup(token);

    /* handle ${VAR} */
    if (token[1] == '{') {
        const char *end = strchr(token + 2, '}');
        if (!end) {
            /* malformed, return token unchanged */
            return strdup(token);
        }
        size_t nlen = end - (token + 2);
        char name[256];
        if (nlen >= sizeof(name)) return strdup("");
        strncpy(name, token + 2, nlen);
        name[nlen] = '\0';
        const char *val = get_variable(name);
        return strdup(val ? val : "");
    }

    /* $VAR simple form */
    const char *name = token + 1;
    const char *val = get_variable(name);
    return strdup(val ? val : "");
}

/* Expand variables in all cmds/argv in-place: free old argv strings and replace with expanded ones.
 * Returns 0 on success, -1 on error.
 */
int expand_vars_in_commands(Command *cmds, int num_cmds) {
    for (int i = 0; i < num_cmds; ++i) {
        for (int j = 0; j < MAX_ARGS && cmds[i].argv[j] != NULL; ++j) {
            char *orig = cmds[i].argv[j];
            if (orig[0] == '$') {
                char *expanded = expand_token(orig);
                if (!expanded) return -1;
                free(orig);
                cmds[i].argv[j] = expanded;
            } else {
                /* no change */
            }
        }
    }
    return 0;
}

/* ----------------- Builtins with status -----------------
 * If builtin handled, return 1 and set *status to 0..255; else return 0.
 * Also adds 'set' builtin to list variables, and supports assignment detection externally.
 */
int handle_builtin_status(char **arglist, int *status) {
    if (!arglist || !arglist[0]) return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        *status = 0;
        free_all_variables();
        exit(0);
    } else if (strcmp(arglist[0], "cd") == 0) {
        if (!arglist[1]) {
            fprintf(stderr, "cd: missing argument\n");
            *status = 1;
        } else {
            if (chdir(arglist[1]) != 0) { perror("cd"); *status = 1; }
            else *status = 0;
        }
        return 1;
    } else if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n"
               " exit - exit shell\n"
               " cd <dir> - change directory\n"
               " help - display this message\n"
               " jobs - list background jobs\n"
               " history - show command history\n"
               " set - list variables\n");
        *status = 0;
        return 1;
    } else if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        *status = 0;
        return 1;
    } else if (strcmp(arglist[0], "history") == 0) {
        HIST_ENTRY **hist = history_list();
        if (hist) {
            for (int i = 0; hist[i]; ++i) printf("%d %s\n", i + 1, hist[i]->line);
        }
        *status = 0;
        return 1;
    } else if (strcmp(arglist[0], "set") == 0) {
        print_variables();
        *status = 0;
        return 1;
    }
    return 0;
}

/* ----------------- Job management ----------------- */
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

/* Reap finished background children (non-blocking) */
void reap_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job_by_pid(pid);
    }
}

/* ----------------- Execution: pipelines & redirection -------------
 * execute_pipeline returns:
 *   - if foreground: the exit code (0..255) of the last process in the pipeline
 *   - if background: 0 if job registered successfully (we do not block)
 *   - on error: -1
 */
int execute_pipeline(Command *cmds, int num_cmds, int background, const char *orig_cmdline) {
    if (num_cmds <= 0) return -1;

    // Expand variables before executing
    if (expand_vars_in_commands(cmds, num_cmds) != 0) {
        fprintf(stderr, "Variable expansion error\n");
        return -1;
    }

    // If single command and it's a builtin and foreground, handle and return status
    if (num_cmds == 1 && !background) {
        int bstatus = 0;
        if (handle_builtin_status(cmds[0].argv, &bstatus)) {
            return bstatus & 0xFF;
        }
    }

    int n = num_cmds;
    int pipefds[2 * (n - 1)];
    pid_t pids[n];

    // create pipes
    for (int i = 0; i < n - 1; ++i) {
        if (pipe(pipefds + 2 * i) < 0) {
            perror("pipe");
            for (int k = 0; k < 2 * i; ++k) close(pipefds[k]);
            return -1;
        }
    }

    for (int i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            for (int k = 0; k < 2 * (n - 1); ++k) close(pipefds[k]);
            return -1;
        }
        if (pid == 0) {
            // Child
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) { perror("dup2 stdin"); exit(1); }
            }
            if (i < n - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) { perror("dup2 stdout"); exit(1); }
            }
            // close all pipe fds in child
            for (int k = 0; k < 2 * (n - 1); ++k) close(pipefds[k]);

            // input redirect
            if (cmds[i].input_file) {
                int fd = open(cmds[i].input_file, O_RDONLY);
                if (fd < 0) { perror("open input"); exit(1); }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 infile"); close(fd); exit(1); }
                close(fd);
            }
            // output redirect
            if (cmds[i].output_file) {
                int fd = open(cmds[i].output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("open output"); exit(1); }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 outfile"); close(fd); exit(1); }
                close(fd);
            }

            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("execvp");
            exit(1);
        } else {
            // parent
            pids[i] = pid;
        }
    }

    // parent closes all fds
    for (int k = 0; k < 2 * (n - 1); ++k) close(pipefds[k]);

    if (background) {
        if (add_job(pids[0], orig_cmdline) < 0) {
            fprintf(stderr, "Warning: job list full; cannot track background job\n");
        } else {
            printf("[bg] started PID %d\n", pids[0]);
        }
        return 0;
    } else {
        int last_status = 0;
        int status;
        for (int i = 0; i < n; ++i) {
            pid_t w = waitpid(pids[i], &status, 0);
            if (w == pids[i]) {
                last_status = status;
            }
        }
        if (WIFEXITED(last_status)) return WEXITSTATUS(last_status);
        if (WIFSIGNALED(last_status)) return 128 + WTERMSIG(last_status);
        return last_status & 0xFF;
    }
}
