#include <stdio.h>
#include <cli.h>

static void can_help(void)
{
        printf("[rule=X] [tuple=X]");
}

static void can_run(char *buf)
{
        printf("\r\n>>>.XXX can run buf:%s:\n\r", buf);
}

static void handle_exit(char *buf)
{
	cli_set_run(0);
}

int main(void)
{
	node_operations_t can_operations;
	node_operations_t exit_operations;

        can_operations.help_cb = can_help;
        can_operations.run_cb = can_run;
        exit_operations.help_cb = NULL;
        exit_operations.run_cb = handle_exit;

	cli_init("(cli(show (rule (can)(ip)(file)) (wl (can)(ip)(file))) (update (rule (can)(ip)(file)) (wl (can)(ip)(file)))(exit))");

	cli_register_operatios("show/rule/can", &can_operations);
	cli_register_operatios("exit", &exit_operations);

	cli_run();

	return 0;
}
