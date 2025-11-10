#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarNode {
    char *name;
    char *value;
    struct VarNode *next;
} VarNode;

VarNode *var_head;

// Variable management functions
void set_variable(const char *name, const char *value);
char *get_variable(const char *name);
void print_variables(void);
void free_variables(void);

#endif // VARIABLES_H
