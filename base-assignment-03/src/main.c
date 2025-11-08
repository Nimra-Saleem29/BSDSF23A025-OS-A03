#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "shell.h"

// List of built-in commands
const char* builtin_commands[] = {
    "cd", "exit", "help", "jobs", "history", NULL
};

// Generator function for command completion
char* command_generator(const char* text, int state) {
    static int list_index, len;
    const char* name;

    if (!state) { // first call
        list_index = 0;
        len = strlen(text);
    }

    while ((name = builtin_commands[list_index++])) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

// Completion function for readline
char** my_completion(const char* text, int start, int end) {
    rl_attempted_completion_over = 0; // 0: allow filename completion too

    // If start of line, complete commands
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }
    return NULL; // fallback to filename completion
}

int main() {
    char* cmdline;
    char** arglist;

    // Set the custom tab completion function
    rl_attempted_completion_function = my_completion;

    while ((cmdline = readline(PROMPT)) != NULL) {
        if (strlen(cmdline) == 0) {
            free(cmdline);
            continue;
        }

        // Add to history
        add_history(cmdline);

        // Handle !n re-execution
        if (cmdline[0] == '!') {
            int cmd_num = atoi(cmdline + 1);
            HIST_ENTRY **hist_list = history_list();
            if (!hist_list || cmd_num <= 0 || cmd_num > history_length) {
                printf("No such command in history.\n");
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(hist_list[cmd_num - 1]->line);
            printf("%s\n", cmdline);
        }

        arglist = tokenize(cmdline);
        if (arglist) {
            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

            for (int i = 0; arglist[i] != NULL; i++)
                free(arglist[i]);
            free(arglist);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
