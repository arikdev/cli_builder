#ifndef _NODE_H
#define _NODE_H

typedef struct node node_t;

void print_node(node_t *node);
node_t *read_node(char *buf);
char *node_get_value(node_t *node);
void *node_get_data(node_t *node);
node_t *node_get_path(node_t **node, char *path);
node_t *node_get_parent(node_t *node);
node_t *node_get_son(node_t *node);  
node_t *node_get_next(node_t *node);  
int node_path_set_data(node_t **node, char *path, void *data);

#endif
