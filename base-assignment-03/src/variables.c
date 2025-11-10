#include "variables.h"

VarNode *var_head = NULL;

void set_variable(const char *name, const char *value) {
    if (!name) return;

    VarNode *cur = var_head;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            free(cur->value);
            cur->value = strdup(value ? value : "");
            return;
        }
        cur = cur->next;
    }

    VarNode *node = malloc(sizeof(VarNode));
    node->name = strdup(name);
    node->value = strdup(value ? value : "");
    node->next = var_head;
    var_head = node;
}

char *get_variable(const char *name) {
    if (!name) return NULL;
    VarNode *cur = var_head;
    while (cur) {
        if (strcmp(cur->name, name) == 0) return cur->value;
        cur = cur->next;
    }
    return NULL;
}

void print_variables(void) {
    VarNode *cur = var_head;
    while (cur) {
        printf("%s=%s\n", cur->name, cur->value);
        cur = cur->next;
    }
}

void free_variables(void) {
    VarNode *cur = var_head;
    while (cur) {
        VarNode *tmp = cur;
        cur = cur->next;
        free(tmp->name);
        free(tmp->value);
        free(tmp);
    }
    var_head = NULL;
}
