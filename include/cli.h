#ifndef CLI_H
#define CLI_H

#define CLI_SUCCESS 0
#define CLI_ERROR  -1

typedef struct {
        void (*help_cb)(void);
        void (*run_cb)(char *buf);
} node_operations_t;

int cli_init(char *buf);
void cli_run(void);
void cli_set_run(int i_is_run);
int cli_register_operatios(char *path, node_operations_t *operation);

#endif
