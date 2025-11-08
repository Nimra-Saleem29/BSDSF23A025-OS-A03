#include "shell.h"

int execute(char** arglist) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return -1;
    } 
    else if (pid == 0) { // Child
        if (execvp(arglist[0], arglist) == -1) {
            perror("execvp failed");
        }
        exit(EXIT_FAILURE);
    } 
    else { // Parent
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
}
