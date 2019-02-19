#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <termios.h>
#include <sys/stat.h>
#include <pwd.h>
#include <cli.h>
#include <cli_node.h>

#define NUM_OF_RULES 4096
#define MAX_BUF_SIZE 10000
#define NUM_OF_CMD_ENTRIES 100

#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define CLEAR_RIGHT "\033[K"
#define COLOR_RESET "\033[0m"

#define RULE "rule"
#define TUPLE "tuple"
#define FILENAME "file"
#define PERM "perm"
#define PROGRAM "program"
#define USER "user"
#define ACTION "action"
#define SRC_IP "src_ip"
#define SRC_NETMASK "src_netmask"
#define DST_IP "dst_ip"
#define DST_NETMASK "dst_netmask"
#define IP_PROTO "proto"
#define SRC_PORT "src_port"
#define SDT_PORT "dst_port"
#define CAN_MSG "can_mid"
#define DIRECTION "direction"
#define INTERFACE "interface"
#define ACTION_OBJ "action_obj"
#define ACTION "action"
#define LOG "log"

#define GET_NEXT_TOKEN(ptr, del) \
	ptr = strtok(NULL, del); \
	if (!ptr) \
		return CLI_ERROR;

static int is_run = 1;

static node_t *cli_node;
static node_t *cur_node;

static void term_reset(int count);

static char *cmds[NUM_OF_CMD_ENTRIES] = {};

static unsigned int cmd_curr;

static char cli_prompt[128];

static void error(char *msg, int is_nl)
{
	if (is_nl)
		printf("\n");
	printf(COLOR_RED);
	printf("%s\n", msg);
	printf(COLOR_RESET);
}
		
static void notify_info(char *msg)
{
	printf(COLOR_GREEN);
	printf("\n%s\n", msg);
	printf(COLOR_RESET);
}

static void cmd_insert(char *arr[], char *str)
{
	unsigned int i;

	/// Always insert at first
	if (arr[NUM_OF_CMD_ENTRIES - 1])
		free(arr[NUM_OF_CMD_ENTRIES - 1]);
	for (i = NUM_OF_CMD_ENTRIES - 1; i > 0; i--) {
		arr[i] = arr[i - 1];
	}
	arr[0] = strdup(str);
}

static char *cmd_get_first(char *arr[])
{
	cmd_curr = 0;
	return arr[0];
}

static char *cmd_get_next(char *arr[])
{
	if (cmd_curr == NUM_OF_CMD_ENTRIES || !arr[cmd_curr + 1])
		return NULL;
	cmd_curr++;
	return arr[cmd_curr];
}

static char *cmd_get_prev(char *arr[])
{
	if (cmd_curr == 0 || !arr[cmd_curr - 1])
		return NULL;
	cmd_curr--;
	return arr[cmd_curr];
}

static void chop_nl(char *str)
{
	int len = strlen(str);

	if (len > 0 && str[len - 1] == '\n')
		str[len - 1] = '\0';
}

static void print_usage(void)
{
}

static void get_num_param(int *rule_id, int *tuple_id)
{
	char *ptr, *iter;

	ptr = strtok(NULL, " "); 
	if (!ptr)
		return;

	for (iter = ptr; *iter && *iter != '='; iter++);
	if (!iter) 
		return;
	iter++;
	
	if (!memcmp(ptr, "rule", strlen("rule")))
		*rule_id = atoi(iter);
	else if (!memcmp(ptr, "tuple", strlen("tuple")))
		*tuple_id = atoi(iter);
}

static char *get_string_user_input(int is_current, char *def_val, char *prompt, int (*is_valid_cb)(char *data), void (*help_cb)(void))
{
	char buf[512];
	static char input[512];

	sprintf(buf, "%s is %s", is_current ? "current" : "default", def_val ?: "none");
	while (1) { 
		printf(">%s: (%s):", prompt, buf);
		if (!fgets(input, sizeof(input), stdin)) {
			printf("error reading\n");
			continue;
		}
		chop_nl(input);
		if (*input) {
			if (*input == '?') {
				if (help_cb)
					help_cb();
				continue;
			}
			if (is_valid_cb && !is_valid_cb(input)) {
				error("invalid value", 0);
				continue;
			}
			return input;
		}
		if (!def_val) {
			error("enter field value", 0);
			continue;
		}
		return def_val;
	}

	return NULL;
}

static void parse_command(char *cmd)
{
	char *ptr;
	char buf[128];
	node_t *help_node;

	if (!strcmp(cmd, "quit")) {
		is_run = 0;
		return;
	}
	ptr = strtok(cmd, " ");
	if (!ptr)
		return;
	// chekc if the command exists in db
	help_node = node_get_path(&cur_node, ptr);
	if (!help_node) {
		print_usage();
		return;
	}
	ptr = strtok(NULL, " ");
	if (!ptr) {
		// move to next level
		cur_node = help_node;
		cli_prompt[strlen(cli_prompt) - 1] = '/';
		cli_prompt[strlen(cli_prompt)] = 0;
		strcat(cli_prompt, cmd);
		strcat(cli_prompt, ">");
	}

	print_usage();
}

static void term_reset(int count)
{
	struct termios t;

	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag |= ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);

	printf("\033[%dD", count);
	if (system ("/bin/stty cooked")) { 
		printf("error reseting term\n");
		return;
	}
}

char *show_options(char *buf) 
{
	node_t *node_p1;
	int count = 0;
	char *retval = NULL;

	if (!node_get_son(cur_node)) return NULL;

	printf("\n\r");
	for (node_p1 = node_get_son(cur_node); node_p1; node_p1 = node_get_next(node_p1)) {
		char *val = node_get_value(node_p1);
		if (!strlen(buf) || !memcmp(val, buf, strlen(buf))) {
			count++;
			retval = val;
			printf("%s ", val);
		}
	}
	printf("\r");

	return count == 1 ? retval : NULL;
}

static void get_cmd(char *buf, unsigned int size, char *prompt)
{
	char c, *tabstr = NULL;
	char *last_cmd;
	int ind = 0, count = 0, pos, min_pos, in_tab = 0;
	struct termios t;
	int is_up = 0;

	if (system("/bin/stty raw")) {
		printf("error seting the term\n");
		return;
	}
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);

	memset(buf, 0, size);
	pos = min_pos = strlen(prompt);

	while (1) {
		c = getchar();
		switch (c) {
			// Backspace as Control-H
			case 0x8:  //  backword
				if (pos == min_pos)
					break;
				pos--;
				printf("\033[1D");
				printf(CLEAR_RIGHT);
				buf[--ind] = 0;
				break;
			case 0x9: // tab
				if (!in_tab) {
					tabstr = show_options(buf);
					in_tab = 1;
				}
				break;
			case '\033': // Escape
				c = getchar();
				switch (c) { 
					case '[':
						c = getchar();
						switch (c) { 
							case 'A': // up
								last_cmd = is_up ? cmd_get_next(cmds) : cmd_get_first(cmds);
								if (!last_cmd)
									break;
								if (strlen(buf))
									printf("\033[%dD", (int)strlen(buf));
								printf(CLEAR_RIGHT);
								strcpy(buf, last_cmd);
								ind = strlen(buf);
								is_up = 1;
								printf("%s", buf);
								pos = strlen(buf) + strlen(prompt);
								break;
							case 'B': // down
								if (!is_up)
									break;
								last_cmd = cmd_get_prev(cmds);
								if (!last_cmd)
									break;
								if (strlen(buf))
									printf("\033[%dD", (int)strlen(buf));
								printf(CLEAR_RIGHT);
								strcpy(buf, last_cmd);
								ind = strlen(buf);
								is_up = 1;
								printf("%s", buf);
								pos = strlen(buf) + strlen(prompt);
								break;
							case 'D': // left
								if (pos <= min_pos)
									break;
								pos--;
								printf("\033[1D"); // cursor left
								break;
							case 'C': //right
								printf("\033[1C"); // cursor right
								pos++;
								break;
							default:
								printf("XXXXX char:%c \n", c);
								break;
						}
						break;
					default:
						break;
				}
				break;
			case 0xd: // Enter
				if (tabstr && strlen(tabstr)) {
					strcpy(buf, tabstr);
					tabstr = NULL;
				}
				in_tab = 0;
				if (!strlen(buf)) {
					printf("\n");
					printf("\033[%dD", (int)strlen(prompt));
				}
				goto out;
			case 0x3: // Cntrl C
				term_reset(count);
				exit(0);
			case 0x7f:  //  backword
				if (pos == min_pos)
					break;
				pos--;
				printf("\033[1D");
				printf(CLEAR_RIGHT);
				buf[--ind] = 0;
				break;
			default:
				if (isprint(c)) {
					count++;
					printf("%c", c);
					pos++;
					buf[ind++] = c;
				} else {
					printf("%x", c);
				}
				break;
		}
	}

out:
	term_reset(count);
}

int cli_register(char *path, void (*cb)(char *data))
{
}

void test_node(void)
{
	node_t *nodep, *node_p1;

	print_node(cli_node);

	nodep = node_get_path(&cli_node, "show/rule");
	if (nodep)
		printf("node value:%s \n", node_get_value(nodep));
	else
		printf("no node !!!\n");

	printf("Test go back:\n");
	node_p1 = node_get_parent(nodep);
	if (node_p1)
		printf("parent :%s \n", node_get_value(node_p1));
	else
		printf("NO parent \n");

	node_p1 = node_get_parent(node_p1);
	if (node_p1)
		printf("parent :%s \n", node_get_value(node_p1));
	else
		printf("NO parent \n");
	node_p1 = node_get_parent(node_p1);
	if (node_p1)
		printf("parent :%s \n", node_get_value(node_p1));
	else
		printf("NO parent \n");

	printf("Test dril down");

	printf("Fet all sons:\n");
	for (node_p1 = node_get_son(nodep); node_p1; node_p1 = node_get_next(node_p1)) {
		printf("%s\n", node_get_value(node_p1));
	}
}

void cli_run(void)
{
	char cmd[MAX_BUF_SIZE];

	node_t *nodep;

	// init

	cur_node = cli_node = read_node("(cli(show (rule (can)(ip)(file)) (wl (can)(ip)(file))) (update (rule (can)(ip)(file)) (wl (can)(ip)(file))))");

	//test_node();
	
	strcpy(cli_prompt, node_get_value(cur_node));
	strcat(cli_prompt, ">");
	while (is_run) {
		printf("%s", cli_prompt);
		get_cmd(cmd, MAX_BUF_SIZE, cli_prompt);
		if (strlen(cmd))
			cmd_insert(cmds, cmd);
		parse_command(cmd);
		if (strlen(cmd))
			printf("\n");
	}
	printf("\n\033[%dD", (int)strlen(cli_prompt));
}

