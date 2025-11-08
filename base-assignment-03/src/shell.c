#include "shell.h"

// Tokenize the command line
char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;
        if (*cp == '\0') break;

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) len++;
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) {
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

// Handle built-in commands
int handle_builtin(char** arglist) {
    if (arglist[0] == NULL) return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        exit(0);
    }
    else if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(arglist[1]) != 0)
                perror("cd");
        }
        return 1;
    }
    else if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("exit - exit shell\n");
        printf("cd <dir> - change directory\n");
        printf("help - display this message\n");
        printf("history - list commands\n");
        printf("jobs - placeholder\n");
        return 1;
    }
    else if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }
    else if (strcmp(arglist[0], "history") == 0) {
        HIST_ENTRY **hist_list = history_list();
        if (hist_list) {
            for (int i = 0; hist_list[i]; i++)
                printf("%d %s\n", i + 1, hist_list[i]->line);
        }
        return 1;
    }

    return 0; // Not a built-in
}

// Execute external commands
int execute(char** arglist) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return -1;
    }
    else if (pid == 0) {
        // Child process
        if (execvp(arglist[0], arglist) < 0) {
            perror("execvp failed");
            exit(1);
        }
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
}
