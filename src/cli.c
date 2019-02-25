/*
Copyright (c) 2019 Arik Aelxander

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
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

void cli_set_run(int i_is_run)
{
	is_run = i_is_run;
}

static void parse_command(char *cmd)
{
	char *tmp = NULL, *word;
	char *words[100];
	node_t *help_node;
	int count = 0;

	tmp = strdup(cmd);
	for (word = strtok(tmp, " "); word; word = strtok(NULL, " ")) {
                words[count++] = word;
	}

	if (!count) return;

	// chekc if the command exists in db
	help_node = node_get_path(&cur_node, words[0]);
	if (!help_node) {
		print_usage();
		goto out;
	}

out:
	if (tmp)
		free(tmp);
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

static void show_help(node_t *node, char *buf)
{
	node_operations_t *rule_operation = (node_operations_t *)node_get_data(node);
	if (!rule_operation)
		return;
	if (!rule_operation->help_cb)
		return;
	printf("\n\r");
	rule_operation->help_cb();
	printf("\n\r");
	printf("%s%s", cli_prompt, buf);
}

void show_options(char *buf, int *ind, int *pos) 
{
	node_t *node_p, *node_p1;
	int count = 0, i, is_finish_blank, ri;
	char *words[100], *results[100];
	char *tmp = NULL, *word;
	char arikb[1000], outbuf[1000];

#ifdef DEBUG
	printf("\rXXXXX b4 buf:%s: \n", buf);
#endif

	if (!node_get_son(cur_node)) {
		show_help(cur_node, buf);
		return;
	}

	// Run for all the words in the buffer. For each word get form DB
	// If not in DB - return.
	
	is_finish_blank = buf[strlen(buf) - 1] == ' ';

	sprintf(arikb, " is finishb%d\n", is_finish_blank);

	tmp = strdup(buf);
	for (word = strtok(tmp, " "); word; word = strtok(NULL, " ")) {
		words[count++] = word;
	}
	sprintf(arikb, " count%d\n", count);

	node_p = cur_node;
	for (i = 0 ; i < count -1; i++) {
		if (!(node_p1 = node_get_path(&node_p, words[i]))) {
			return;
		}
		node_p = node_p1;
	}

	// If the last word exists try to get the next
	// If not try to complete
	if (!count)
		outbuf[0] = 0;
	else if ((node_p1 = node_get_path(&node_p, words[count - 1]))) {
		node_p = node_p1;
		if (!is_finish_blank) {
			count = 0;
			goto out; // Nothing to do.
		}
		// Show all possible next words
		outbuf[0] = 0;
	} else
		strcpy(outbuf, words[count - 1]);

	ri = 0;
	for (node_p1 = node_get_son(node_p); node_p1; node_p1 = node_get_next(node_p1)) {
		char *val = node_get_value(node_p1);
		if (!strlen(outbuf) || !memcmp(val, outbuf, strlen(outbuf))) {
			results[ri++] = val;
		}
	}

	if (!ri) {
		if (!node_get_son(node_p))
			show_help(node_p, buf);
		goto out;
	}

	if (ri == 1) {
		if (is_finish_blank) {
			printf("%s", results[0]);
			*pos += strlen(results[0]);
			*ind += strlen(results[0]);
		} else {
			printf("%s", results[0] + strlen(words[count - 1]));
			*pos += strlen(results[0]) -strlen(words[count - 1]);
			*ind += strlen(results[0]) -strlen(words[count - 1]);
			strcat(buf, results[0] + strlen(words[count - 1]));
		}
	} else {
		printf("\n\r");
		for (i = 0; i < ri; i++) {
			printf("%s ", results[i]);
		}
		printf("\n\r");
		printf("%s%s", cli_prompt, buf);
	}

out:
	if (tmp)
		free(tmp);
#ifdef DEBUG
	printf("\rXXXXX after buf:%s: \n", buf);
#endif
}

void handle_enter(char *buf)
{
	node_operations_t *rule_operation;
	node_t *nodep, *nodep1;
	char *tmp = NULL, *p, *words[100];
	int count = 0, i;

	if (!memcmp(buf, "..", 2)) {
                nodep = node_get_parent(cur_node);
                if (!nodep)
                        return;
                cur_node = nodep;
                for (i = strlen(cli_prompt) - 1; cli_prompt[i] != '/'; i--)
                        cli_prompt[i] = 0;
                cli_prompt[i] = 0;
                strcat(cli_prompt, ">");
        }

	tmp = strdup(buf);
	nodep = cur_node;
	for (p = strtok(tmp, " "); p; p = strtok(NULL, " ")) {
		for (nodep1 = node_get_son(nodep); nodep1 && strcmp(node_get_value(nodep1), p); nodep1 = node_get_next(nodep1));
		if (!nodep1)
			break;
		nodep = nodep1;
		words[count++] = p;
	}
	
	if (nodep1 && node_get_son(nodep1) &&  count == 1) {
                // move to next level
                cur_node = nodep;
                cli_prompt[strlen(cli_prompt) - 1] = '/';
                cli_prompt[strlen(cli_prompt)] = 0;
                strcat(cli_prompt, words[0]);
                strcat(cli_prompt, ">");
		goto out;
        }

	rule_operation = (node_operations_t *)node_get_data(nodep);
	if (!rule_operation)
		goto out;
	if (!rule_operation->run_cb)
		goto out;
	rule_operation->run_cb(buf);

out:
	if (tmp)
		free(tmp);
}

static void get_cmd(char *buf, unsigned int size, char *prompt)
{
	char c;
	char *last_cmd;
	int ind = 0, count = 0, pos, min_pos;
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
				show_options(buf, &ind, &pos);
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
				if (!strlen(buf)) {
					printf("\n");
					printf("\033[%dD", (int)strlen(prompt));
				}
				term_reset(count);
				handle_enter(buf);
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
				if (ind > 0)
					buf[--ind] = 0;
#ifdef DEBUG
				printf("\rBBBB buf:%s:\n", buf);
#endif
				break;
			default:
				if (isprint(c)) {
					count++;
					printf("%c", c);
					pos++;
					buf[ind++] = c;
#ifdef DEBUG
					printf("\rDDDD buf:%s:\n", buf);
#endif
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
	return 0;
}

int cli_init(char *buf)
{
	cur_node = cli_node = read_node(buf);

	return 0;
}

int cli_register_operatios(char *path, node_operations_t *operation)
{
	return node_path_set_data(&cli_node, path, operation);
}

void cli_run(void)
{
	char cmd[MAX_BUF_SIZE];

	strcpy(cli_prompt, node_get_value(cur_node));
	strcat(cli_prompt, ">");
	while (is_run) {
		printf("%s", cli_prompt);
		get_cmd(cmd, MAX_BUF_SIZE, cli_prompt);
		if (strlen(cmd))
			cmd_insert(cmds, cmd);
		parse_command(cmd);
		if (strlen(cmd) && !is_run)
			printf("\n");
	}
	printf("\n\033[%dD", (int)strlen(cli_prompt));
}

