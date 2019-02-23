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

int main(void)
{
	node_operations_t can_operations;
        can_operations.help_cb = can_help;
        can_operations.run_cb = can_run;

	cli_init("(cli(show (rule (can)(ip)(file)) (wl (can)(ip)(file))) (update (rule (can)(ip)(file)) (wl (can)(ip)(file))))");

	cli_register_operatios("show/rule/can", &can_operations);

	cli_run();

	return 0;
}
