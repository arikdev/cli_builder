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
#include <string.h>
#include <cli_node.h>
#include <ctype.h>

#define DEL "/"

struct node {
	char *value;
	struct node *next;
	struct node *son;
	struct node *parent;
	void *data;
};

static char *get_value(char *buf, unsigned int *len)
{
	unsigned int i;
	char *value;

	for (i = 0; buf[i] && buf[i] != ')' && buf[i] != '(' && !isspace(buf[i]); i++);
	if (!i) {
		*len = 0;
		return strdup("");
	}

	if (!(value = malloc(i + 1)))
		return NULL;

	memcpy(value, buf, i);
	value[i] = 0;

	for (; isspace(buf[i]); i++);

	*len = i;

	return value;
}

static node_t *node_alloc(char *value)
{
	node_t *node;
   
	if (!(node = malloc(sizeof(node_t))))
		return NULL;
	memset(node, 0, sizeof(node_t)); 
 
	if (!(node->value = strdup(value)))
		return NULL;
   
	return node;
}

static void print_node_recursive(FILE *file, node_t *node, int level, int is_print)
{
	unsigned int i;
 
	for (node = node->son; node; node = node->next) {
		if (is_print)
			for (i = 0;i < level; i++) fprintf(file, "  ");
		fprintf(file, "(%s", node->value);
		if (node->son) {
			if (is_print)
				fprintf(file, "\n");
			print_node_recursive(file, node, level + 1, is_print);
			if (is_print)
				for (i = 0;i < level; i++) fprintf(file, "  ");
		}
		fprintf(file, ")");
		if (is_print)
			fprintf(file, "\n");
  	}
}

void write_node(FILE *file, node_t *node,int is_print)
{
	fprintf(file, "(%s", node->value);
	if (node->son) {
		if (is_print)
        		fprintf(file, "\n");
		print_node_recursive(file, node, 1, is_print); 
	}
	fprintf(file, ")");
	if (is_print)
		fprintf(file, "\n");
}

void print_node(node_t *node)
{
	write_node(stdout, node, 1);
}

static char *read_node_recursive(node_t **node, char *buf, node_t *parent)
{
	char *tmp;
	unsigned int len = 0;

	while (1) {
		switch (*buf) {
			case '(':
				buf++;  
				tmp = get_value(buf, &len);
				buf += len;   
				if (!(*node = node_alloc(tmp)))  {
					// XXX indicate no memory error.
					return NULL;
				}
				(*node)->parent = parent;
				free(tmp);
				buf = read_node_recursive(&((*node)->son), buf, *node);
				node = &((*node)->next);
				break;
			case ')':   
             			buf++;
			// Fall-through until return.
			case ' ':
			case '\n':
			case '\r':
			case '\t':
				for (; isspace(*buf); buf++);
				case 0:
			default:
				*node = NULL;
				return buf;
		}
  	}
}

node_t *read_node(char *buf)
{
	node_t *node;

	read_node_recursive(&node, buf, NULL);

	return node;
}

static node_t **node_get_or_set(int is_get, node_t **node, char *path)
{
	node_t **ret_node = NULL, *parent;
	char *tmp = NULL, *token;  

	if (!node)
		return NULL;
	if (!*node) {
		if (is_get)
			return NULL;
		if (!(tmp = strdup(path)))
			return NULL;
		token = strtok(tmp, DEL);
		if(!token)
			return NULL;
		if (!(*node = node_alloc(token)))
			return NULL;
		free(tmp);
		
	}

	parent = *node;

	if (!(tmp = strdup(path)))
		return NULL;

	for (token = strtok(tmp, DEL); token; token = strtok(NULL, DEL)) {
        for (node = &((*node)->son); *node && strcmp((*node)->value, token); node = &((*node)->next));
		if (!*node) {
			if (is_get) {
				ret_node = NULL;
				goto out;
			}
			break;
		}
		ret_node = node;
		parent = *node;
	}
 
	if (is_get) 
		goto out;

	for (; token; token = strtok(NULL, DEL)) {
		if (!(*node = node_alloc(token))) {
			// Indicate memory error.
			ret_node = NULL;
			goto out;
		}
		(*node)->parent = parent;
		ret_node = node; 
		parent = *node;
		node = &((*node)->son);
	}

out: 
	if (tmp)
		free(tmp);
	return ret_node;
}

node_t **node_get(node_t **node, char *path)
{
	return node_get_or_set(1, node, path);
}

char *node_get_value(node_t *node)
{
	if (!node)
		return NULL;
  
	return node->value;
}

void *node_get_data(node_t *node)
{
	if (!node)
		return NULL;
  
	return node->data;
}

#define RET_VAL(is_null, val) (is_null || val ? val : strdup(""))

static char *_node_get_path_str(int is_null, node_t **node, char *path)
{
	if (!(node = node_get(node, path)) || !(*node)->son)
		return RET_VAL(is_null, NULL);

	return RET_VAL(is_null, (*node)->son->value); 
}

char *node_get_path_str(node_t **node, char *path)
{
	return _node_get_path_str(1, node, path);
}

node_t *node_get_path(node_t **node, char *path)
{
	node_t **nodep;

	nodep = node_get(node, path);
	if (!nodep) return NULL;
	return *nodep;
}

char *node_get_path_strz(node_t **node, char *path)
{
	return _node_get_path_str(0, node, path);
}

node_t **set_node(node_t **node, char *path)
{
	return node_get_or_set(0, node, path);
}

static void free_sons(node_t *node)
{
	node_t *tmp; 

	if (!node)
		return;

	for (node = node->son; node; ) {
		free(node->value);
		free_sons(node);
		tmp = node;
		node = node->next;
		free(tmp);
	}
}

node_t **set_node_path_str(node_t **node, char *path, char *value)
{
	if (!(node = set_node(node, path)))
		return NULL;

	free_sons(*node);
	(*node)->son = node_alloc(value);
	(*node)->son->parent = *node;

	return node;
}

int node_path_set_data(node_t **node, char *path, void *data)
{
	node_t **hnode;

	if (!(hnode = node_get(node, path)))
		return -1;

	(*hnode)->data = data;

	return 0;
}

node_t **node_lookfor_value(node_t **node, char *path, char *value)
{
	if (!node || !*node) 
		return NULL;

	for (node = &((*node)->son); *node; node = &((*node)->next)) {
		if (!strcmp(node_get_path_strz(node, path), value))
			return node;
	}

	return NULL;
}

node_t *node_get_son(node_t *node) 
{
	if (!node)
		return NULL;
	return node->son;
}

node_t *node_get_next(node_t *node) 
{
	if (!node)
		return NULL;
	return node->next;
}

node_t *node_get_parent(node_t *node) 
{
	if (!node)
		return NULL;
	return node->parent;
}

int node_get_nsons(node_t *node)
{
	int nsons = 0;
	node_t *iter;

	for (iter = node_get_son(node); iter;
		iter = node_get_next(iter)) {
		nsons++;
	}
	return nsons;
}

static void node_free_one(node_t *node)
{
	if (node->value)
		free(node->value);
	free(node);
}

static int node_free_recursive(node_t **node)
{
	node_t **s, *d;

	s = &(*node)->son;
	while (*s) {
		if (!node_free_recursive(s))
			s = &(*s)->next;
	}

	d = *node;
	*node = (*node)->next;
 	node_free_one(d);
	return 1;
}

int node_free(node_t **node)
{
	if (!node || !*node)
		return 0;

	return node_free_recursive(node);
}
