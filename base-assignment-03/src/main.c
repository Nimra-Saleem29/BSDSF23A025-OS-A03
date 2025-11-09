#define _GNU_SOURCE
#include "shell.h"

// Completion builtins
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
    rl_attempted_completion_over = 0; // allow filename completion as well
    if (start == 0) return rl_completion_matches(text, command_generator);
    return NULL;
}

int main(void) {
    rl_attempted_completion_function = my_completion;
    using_history();

    char *line = NULL;
    while (1) {
        // Reap finished background jobs so job list stays up-to-date and no zombies
        reap_jobs();

        line = readline(PROMPT);
        if (!line) break; // EOF (Ctrl+D)

        // Skip empty lines
        char *tline = trim(line);
        if (!tline || tline[0] == '\0') { free(line); continue; }

        // Add non-empty to history
        add_history(line);

        // Handle !n re-execution BEFORE splitting by ';'
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

        // Split line by semicolon for sequential execution
        char *save_stmt = NULL;
        char *stmt = strtok_r(line, ";", &save_stmt);
        while (stmt != NULL) {
            char *s = trim(stmt);
            if (s && s[0] != '\0') {
                // Check for background flag (&) at end
                int background = 0;
                size_t L = strlen(s);
                // Trim trailing whitespace first
                while (L > 0 && isspace((unsigned char)s[L - 1])) { s[L - 1] = '\0'; L--; }
                if (L > 0 && s[L - 1] == '&') {
                    background = 1;
                    s[L - 1] = '\0';
                    // trim again trailing spaces
                    while (L > 1 && isspace((unsigned char)s[L - 2])) { s[L - 2] = '\0'; L--; }
                }

                // parse pipeline (parse_pipeline modifies string) -> pass a strdup
                char *to_parse = strdup(s);
                Command cmds[MAX_CMDS];
                int num_cmds = 0;
                if (parse_pipeline(to_parse, cmds, &num_cmds) == 0) {
                    execute_pipeline(cmds, num_cmds, background, s);
                    free_commands(cmds, num_cmds);
                } else {
                    fprintf(stderr, "Parse error in statement: %s\n", s);
                }
                free(to_parse);
            }
            stmt = strtok_r(NULL, ";", &save_stmt);
        }

        free(line);
    }

    printf("\nShell exited.\n");
    return 0;
}
