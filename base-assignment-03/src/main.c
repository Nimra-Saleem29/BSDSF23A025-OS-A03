#define _GNU_SOURCE
#include "shell.h"

/* completion list for readline */
const char* builtin_commands[] = {
    "cd", "exit", "help", "jobs", "history", "set", NULL
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
    rl_attempted_completion_over = 0;
    if (start == 0) return rl_completion_matches(text, command_generator);
    return NULL;
}

/* Helper: run a single statement (may contain pipes/redirs) or assignment.
 * Returns exit status or -1 on error.
 * Assignments (VARNAME=VALUE with no spaces around '=') are handled here as builtin and return 0.
 */
static int run_statement_return_status(char *stmt) {
    if (!stmt) return -1;
    char *s = trim(stmt);
    if (s[0] == '\0') return 0;

    // detect variable assignment: single token containing '=' and no spaces around =
    // we check first token only: assignments are of form NAME=value (no spaces)
    char *first_space = strpbrk(s, " \t");
    int is_assignment = 0;
    if (first_space == NULL) {
        // whole line is a single token; if contains '=' then assignment
        if (strchr(s, '=') != NULL) is_assignment = 1;
    } else {
        // there's a space — check the first token only
        size_t toklen = first_space - s;
        char tokbuf[256];
        if (toklen < sizeof(tokbuf)) {
            strncpy(tokbuf, s, toklen);
            tokbuf[toklen] = '\0';
            if (strchr(tokbuf, '=') != NULL) {
                // ensure '=' is not at start or end of token
                char *eq = strchr(tokbuf, '=');
                if (eq != NULL && (eq != tokbuf) && (*(eq + 1) != '\0')) {
                    is_assignment = 1;
                }
            }
        }
    }

    if (is_assignment) {
        // parse name and value from first token
        char *eq = strchr(s, '=');
        if (!eq) return -1;
        size_t namelen = eq - s;
        if (namelen == 0 || namelen >= 128) {
            fprintf(stderr, "Invalid variable name\n");
            return -1;
        }
        char name[128];
        strncpy(name, s, namelen);
        name[namelen] = '\0';
        // value is rest after '=' (may include quotes)
        char *value = eq + 1;
        // remove surrounding quotes if present
        char *val_trim = trim(value);
        size_t vlen = strlen(val_trim);
        char *final_val = NULL;
        if (vlen >= 2 && ((val_trim[0] == '"' && val_trim[vlen - 1] == '"') ||
                          (val_trim[0] == '\'' && val_trim[vlen - 1] == '\''))) {
            final_val = strndup(val_trim + 1, vlen - 2);
        } else {
            final_val = strdup(val_trim);
        }
        if (!final_val) return -1;
        // set variable (overwrite if exists)
        if (set_variable(name, final_val) != 0) {
            fprintf(stderr, "Failed to set variable\n");
            free(final_val);
            return -1;
        }
        free(final_val);
        return 0;
    }

    // background detection for single statement (trailing &)
    int background = 0;
    size_t L = strlen(s);
    while (L > 0 && isspace((unsigned char)s[L - 1])) { s[L - 1] = '\0'; L--; }
    if (L > 0 && s[L - 1] == '&') {
        background = 1;
        s[L - 1] = '\0';
        // trim again trailing spaces
        while (L > 1 && isspace((unsigned char)s[L - 2])) { s[L - 2] = '\0'; L--; }
    }

    // parse pipeline
    char *copy = strdup(s);
    if (!copy) return -1;
    Command cmds[MAX_CMDS];
    int num_cmds = 0;
    int ret = -1;
    if (parse_pipeline(copy, cmds, &num_cmds) == 0) {
        ret = execute_pipeline(cmds, num_cmds, background, s);
        free_commands(cmds, num_cmds);
    } else {
        fprintf(stderr, "Parse error in statement: %s\n", s);
        ret = -1;
    }
    free(copy);
    return ret;
}

/* Split by ';' and run each statement sequentially */
static void process_multi_statements(char *line) {
    char *save = NULL;
    char *stmt = strtok_r(line, ";", &save);
    while (stmt != NULL) {
        char *s = trim(stmt);
        if (s && s[0] != '\0') {
            (void) run_statement_return_status(s);
        }
        stmt = strtok_r(NULL, ";", &save);
    }
}

/* Collect lines until a keyword (case-insensitive) appears on its own line */
static char *collect_block_until_keyword(const char *terminator) {
    size_t cap = 1024, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    while (1) {
        char *line = readline("> ");
        if (!line) { free(buf); return NULL; } // EOF
        char *t = trim(line);
        if (strcasecmp(t, terminator) == 0) { free(line); break; }
        size_t add = strlen(line) + 1;
        if (len + add + 1 > cap) {
            cap = (len + add + 1) * 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); free(line); return NULL; }
            buf = nb;
        }
        memcpy(buf + len, line, strlen(line));
        len += strlen(line);
        buf[len++] = '\n';
        buf[len] = '\0';
        free(line);
    }
    return buf;
}

/* handle if-blocks (same behavior as previous Feature-7 implementation) */
static int handle_if_block(char *tline) {
    if (!tline) return -1;
    char *after_if = tline + 2;
    while (*after_if && isspace((unsigned char)*after_if)) after_if++;
    char *condition_cmd = NULL;
    if (*after_if != '\0') {
        condition_cmd = strdup(after_if);
        if (!condition_cmd) return -1;
    } else {
        char *cline = NULL;
        while (1) {
            cline = readline("> ");
            if (!cline) { fprintf(stderr, "Unexpected EOF while reading condition\n"); return -1; }
            char *t = trim(cline);
            if (t && t[0] != '\0') break;
            free(cline);
        }
        condition_cmd = strdup(cline);
        free(cline);
        if (!condition_cmd) return -1;
    }

    while (1) {
        char *ln = readline("> ");
        if (!ln) { free(condition_cmd); fprintf(stderr, "Unexpected EOF waiting for 'then'\n"); return -1; }
        char *t = trim(ln);
        if (strcasecmp(t, "then") == 0) { free(ln); break; }
        free(ln);
    }

    char *then_block = NULL;
    int saw_else = 0;
    size_t then_cap = 0, then_len = 0;
    while (1) {
        char *ln = readline("> ");
        if (!ln) { free(condition_cmd); free(then_block); fprintf(stderr, "Unexpected EOF in then-block\n"); return -1; }
        char *t = trim(ln);
        if (strcasecmp(t, "else") == 0) { saw_else = 1; free(ln); break; }
        if (strcasecmp(t, "fi") == 0) { free(ln); break; }
        size_t add = strlen(ln) + 1;
        if (!then_block) { then_cap = add + 1; then_block = malloc(then_cap); then_len = 0; then_block[0] = '\0'; }
        if (then_len + add + 1 > then_cap) {
            then_cap = (then_len + add + 1) * 2;
            char *nb = realloc(then_block, then_cap);
            if (!nb) { free(condition_cmd); free(ln); free(then_block); return -1; }
            then_block = nb;
        }
        memcpy(then_block + then_len, ln, strlen(ln));
        then_len += strlen(ln);
        then_block[then_len++] = '\n';
        then_block[then_len] = '\0';
        free(ln);
    }

    char *else_block = NULL;
    if (saw_else) {
        else_block = collect_block_until_keyword("fi");
        if (!else_block) { free(condition_cmd); free(then_block); return -1; }
    }

    int cond_status = run_statement_return_status(condition_cmd);
    free(condition_cmd);

    if (cond_status == 0) {
        if (then_block && then_block[0] != '\0') {
            char *tmp = strdup(then_block);
            if (tmp) { process_multi_statements(tmp); free(tmp); }
        }
    } else {
        if (else_block && else_block[0] != '\0') {
            char *tmp = strdup(else_block);
            if (tmp) { process_multi_statements(tmp); free(tmp); }
        }
    }

    free(then_block);
    free(else_block);
    return 0;
}

int main(void) {
    rl_attempted_completion_function = my_completion;
    using_history();

    char *line = NULL;
    while (1) {
        reap_jobs();
        line = readline(PROMPT);
        if (!line) break; // EOF (Ctrl+D)

        char *tline = trim(line);
        if (!tline || tline[0] == '\0') { free(line); continue; }

        add_history(line);

        // Handle !n re-execution
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
            tline = trim(line);
            if (!tline || tline[0] == '\0') { free(line); continue; }
        }

        // Detect 'if' blocks
        char *copy = strdup(tline);
        if (!copy) { free(line); break; }
        char *first = strtok(copy, " \t");
        if (first && strcmp(first, "if") == 0) {
            handle_if_block(tline);
            free(copy);
            free(line);
            continue;
        }
        free(copy);

        // Otherwise treat input as possibly multiple statements separated by ';'
        char *work = strdup(tline);
        if (!work) { free(line); break; }
        process_multi_statements(work);
        free(work);
        free(line);
    }

    free_all_variables();
    printf("\nShell exited.\n");
    return 0;
}
