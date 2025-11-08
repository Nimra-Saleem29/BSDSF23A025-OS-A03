#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "shell.h"

// Builtins completion list (same as before)
const char* builtin_commands[] = {
    "cd", "exit", "help", "jobs", "history", NULL
};

static char* command_generator(const char* text, int state) {
    static int idx, len;
    const char *name;
    if (!state) { idx = 0; len = strlen(text); }
    while ((name = builtin_commands[idx++])) {
        if (strncmp(name, text, len) == 0) return strdup(name);
    }
    return NULL;
}

static char** my_completion(const char* text, int start, int end) {
    // Allow both builtins and filename completion: if at start, try builtin, else fallback to filenames
    rl_attempted_completion_over = 0;
    if (start == 0) return rl_completion_matches(text, command_generator);
    return NULL;
}

int main() {
    rl_attempted_completion_function = my_completion;
    using_history();

    char *line = NULL;
    while (1) {
        line = readline(PROMPT);
        if (!line) break; // Ctrl+D

        // skip empty
        if (strlen(line) == 0) { free(line); continue; }

        // history
        add_history(line);

        // Handle !n re-execution (before parsing)
        if (line[0] == '!') {
            int n = atoi(line + 1);
            HIST_ENTRY **hist = history_list();
            if (!hist || n <= 0 || n > history_length) {
                printf("No such command in history.\n");
                free(line);
                continue;
            }
            char *exec_line = strdup(hist[n - 1]->line);
            free(line);
            line = exec_line;
            printf("%s\n", line);
        }

        // parse pipeline: parse_pipeline modifies string, so pass a strdup
        char *line_copy = strdup(line);
        Command cmds[MAX_CMDS];
        int num_cmds = 0;
        if (parse_pipeline(line_copy, cmds, &num_cmds) == 0) {
            // If single command and builtin, execute in shell (handled in execute_pipeline)
            execute_pipeline(cmds, num_cmds);
            free_commands(cmds, num_cmds);
        } else {
            fprintf(stderr, "Parse error\n");
        }
        free(line_copy);
        free(line);
    }

    printf("\nShell exited.\n");
    return 0;
}
