#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        // Handle !n re-execution
        if (cmdline[0] == '!') {
            int cmd_num = atoi(cmdline + 1);
            if (cmd_num <= 0 || cmd_num > HISTORY_SIZE || cmd_num > history_count) {
                printf("No such command in history.\n");
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(history[cmd_num - 1]);
            printf("%s\n", cmdline);
        }

        add_history(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

            for (int i = 0; arglist[i] != NULL; i++) free(arglist[i]);
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
