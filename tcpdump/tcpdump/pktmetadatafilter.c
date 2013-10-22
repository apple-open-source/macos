/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */


/*
 * A parser to filter packets based on packet metadata like interface or process.
 *
 * Example 
 *    if=en0 and (not proc=mDNSResponder or svc =! ctl)
 *
 * Grammar  in pseudo BNF notation:
 *  expr: orexpr
 *  orexpr: andexpr { "or" andexpr }
 *  andexpr: notexpr { "and" orexpr }
 *  notexpr: { "not" } parenexp
 *  parenexpr:  "(" expr ")" | termexpr
 *  termexpr: ifexpr | procexpr | svcexpre | direxpr
 *  ifexpr: "if" compexpr
 *  compexpr: equalexpr | notequalexpr
 *  equalexpr: "=" str
 *  str: a-zA-Z0-9
 *  procexpr: "proc" compexpr
 *  svcexpre: "svc" compexpr
 *  direxpr: "dir" compexpr
 * 
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sysexits.h>
#include <err.h>

#include "pktmetadatafilter.h"

enum {
	TOK_NONE,
	
	TOK_OR,
	TOK_AND,
	TOK_NOT,
	TOK_LP,
	TOK_RP,
	TOK_IF,
	TOK_PROC,
	TOK_EPROC,
	TOK_PID,
	TOK_EPID,
	TOK_SVC,
	TOK_DIR,
	TOK_EQ,
	TOK_NEQ,
	TOK_STR
};

struct token {
	int		id;
	char	*label;
	size_t	len;
};

struct token lex_token = { TOK_NONE, NULL, 0 };

struct token tokens[] = {
	{ TOK_OR, "or", 0 },
	{ TOK_AND, "and", 0 },
	{ TOK_NOT, "not", 0 } ,
	{ TOK_LP, "(", 0 },
	{ TOK_RP, ")", 0 },
	{ TOK_IF, "if", 0  },
	{ TOK_PROC, "proc", 0 },
	{ TOK_EPROC, "eproc", 0 },
	{ TOK_PID, "pid", 0 },
	{ TOK_EPID, "epid", 0 },
	{ TOK_SVC, "svc", 0 },
	{ TOK_DIR, "dir", 0 },
	{ TOK_EQ, "=", 0 },
	{ TOK_NEQ, "!=", 0 },

	/* alternate notation */
	{ TOK_OR, "||", 0 },
	{ TOK_AND, "&&", 0 },
	{ TOK_NOT, "!", 0 } ,
    
	{ TOK_NONE, NULL, 0 }
};

struct node {
	int	id;
	char *str;
	int num;
	int op;
	struct node *left_node;
	struct node *right_node;
};

#define MAX_NODES 1000
static int num_nodes = 0;

static struct node * alloc_node(int);
static void free_node(struct node *);
static const char * get_strcharset(void);
static void get_token(const char **);
static struct node * parse_term_expression(const char **);
static struct node * parse_paren_expression(const char **);
static struct node * parse_not_expression(const char **);
static struct node * parse_and_expression(const char **);
static struct node * parse_or_expression(const char **);

#ifdef DEBUG
static int parse_verbose = 0;
#endif /* DEBUG */

static struct node *
alloc_node(int id)
{
	struct node *node = calloc(1, sizeof(struct node));
	
	if (node == NULL)
		err(EX_OSERR, "calloc()");
	
	if (++num_nodes > MAX_NODES)
		errx(EX_USAGE, "Metadata filter expression too long (number of terms limited to %d)", MAX_NODES);
	
	node->id = id;
	
	return (node);
}

static void
free_node(struct node *node)
{
	if (node)
		free(node);
}

static const char *
get_strcharset(void)
{
	static char *strcharset = NULL;
	
	if (strcharset == NULL) {
		int i, n = 0;
		
		strcharset = calloc(1, 256);
		if (strcharset == NULL)
			err(EX_OSERR, "calloc");
		
		for (i = '0'; i <= '9'; i++)
			strcharset[n++] = i;
		for (i = 'A'; i <= 'Z'; i++)
			strcharset[n++] = i;
		for (i = 'a'; i <= 'z'; i++)
			strcharset[n++] = i;
		strcharset[n++] = '-';
		strcharset[n++] = '_';
		strcharset[n++] = '+';
		strcharset[n++] = '.';
		strcharset[n++] = '*';
	}
	
	return strcharset;
}

static void
get_token(const char **ptr)
{
	size_t len;
	const char *charset;
	struct token *tok;
   
#ifdef DEBUG
	if (parse_verbose)
		printf("%s\n", __func__);
#endif /* DEBUG */
	
	/* Skip white spaces */
	while (isspace(**ptr)) {
		(*ptr)++;
	}
	
	/* Are we at the end of the expression */
	if (**ptr == 0) {
		lex_token.id = TOK_NONE;
		return;
	}
	
	for (tok = &tokens[0]; tok->id != TOK_NONE; tok++) {
		if (tok->len == 0)
			tok->len = strlen(tok->label);
		
		if (strncmp(*ptr, tok->label, tok->len) == 0) {
#ifdef DEBUG
			if (parse_verbose)
				printf("tok id: %d label: %s\n", tok->id, tok->label);
#endif /* DEBUG */

			lex_token.id = tok->id;
			lex_token.label = strdup(tok->label);
			lex_token.len = tok->len;
			*ptr += lex_token.len;
			
			return;
		}
	}
	
	lex_token.id = TOK_STR;

	if (strncmp(*ptr, "''", 2) == 0 || strncmp(*ptr, "\"\"", 2) == 0) {
		lex_token.label = malloc(1);
		*lex_token.label = 0; // empty string
		lex_token.len = 0;
		*ptr += 2;
	} else {
		charset = get_strcharset();
		
		len = strspn(*ptr, charset);

		lex_token.label = realloc(lex_token.label, len + 1);
		strlcpy(lex_token.label, *ptr, len + 1);
		lex_token.len = len;
		*ptr += lex_token.len;
	}
	
#ifdef DEBUG
	if (parse_verbose) {
		char fmt[50];

		bzero(fmt, sizeof(fmt));
		snprintf(fmt, sizeof(fmt), "tok id: %%d len: %%lu str: %%.%lus\n", len);
		printf(fmt, lex_token.id , lex_token.len, lex_token.label);
	}
#endif /* DEBUG */
	return;
}

static struct node *
parse_term_expression(const char **ptr)
{
	struct node *term_node = NULL;

#ifdef DEBUG
	if (parse_verbose)
		printf("%s\n", __func__);
#endif /* DEBUG */

	switch (lex_token.id) {
		case TOK_IF:
		case TOK_PROC:
		case TOK_EPROC:
		case TOK_PID:
		case TOK_EPID:
		case TOK_SVC:
		case TOK_DIR:
			term_node = alloc_node(lex_token.id);
			get_token(ptr);
            
			if (lex_token.id == TOK_EQ || lex_token.id == TOK_NEQ)
				term_node->op = lex_token.id;
			else {
				warnx("cannot parse operator at: %s", *ptr);
				term_node = NULL;
				goto fail;
            }
			get_token(ptr);
			if (lex_token.id != TOK_STR) {
				warnx("missig comparison string at: %s", *ptr);
				goto fail;
            }
			/*
			 * TBD
			 * For TOK_SVC and TOK_DIR restrict to meaningful values
			 */
			
			term_node->str = strdup(lex_token.label);

			if (term_node->id == TOK_PID || term_node->id == TOK_EPID)
				term_node->num = atoi(term_node->str);
			break;
		
		default:
			warnx("cannot parse term at: %s", *ptr);
			break;
	}
	return (term_node);
fail:
	free_node(term_node);
	return (NULL);
}

static struct node *
parse_paren_expression(const char **ptr)
{
#ifdef DEBUG
	if (parse_verbose)
		printf("%s\n", __func__);
#endif /* DEBUG */

	if (lex_token.id == TOK_LP) {
		struct node *or_node;
		
		get_token(ptr);
		or_node = parse_or_expression(ptr);
		if (or_node == NULL)
			return (NULL);
		
		if (lex_token.id != TOK_RP) {
			warnx("missing right parenthesis at %s", *ptr);
			free_node(or_node);

			return (NULL);
		}
		
		return (or_node);
	} else {
		return parse_term_expression(ptr);
	}
}

static struct node *
parse_not_expression(const char **ptr)
{
#ifdef DEBUG
	if (parse_verbose)
		printf("%s\n", __func__);
#endif /* DEBUG */

	if (lex_token.id == TOK_NOT) {
		struct node *other_node;

		get_token(ptr);
		other_node = parse_not_expression(ptr);

		if (other_node == NULL) {
			return (NULL);
		} else {
			struct node *not_node = alloc_node(TOK_NOT);
			
			not_node->left_node = other_node;
			
			return (not_node);
		}
	} else {
		struct node *paren_node = parse_paren_expression(ptr);
        
		return (paren_node);
	}
}

static struct node *
parse_and_expression(const char **ptr)
{
	struct node *not_node = NULL;

#ifdef DEBUG
	if (parse_verbose)
		printf("%s\n", __func__);
#endif /* DEBUG */

	not_node = parse_not_expression(ptr);
	if (not_node == NULL)
		return (NULL);

	get_token(ptr);
	if (lex_token.id == TOK_AND) {
		struct node *other_node;
		
		get_token(ptr);
		other_node = parse_or_expression(ptr);

		if (other_node == NULL) {
			free_node(not_node);
			
			return (NULL);
		} else {
			struct node *and_node = alloc_node(TOK_AND);
			
			and_node->left_node = not_node;
			and_node->right_node = other_node;
			
			return (and_node);
		}
	} else {
		return (not_node);
	}
}

static struct node *
parse_or_expression(const char **ptr)
{
	struct node *and_node = NULL;

#ifdef DEBUG
	if (parse_verbose)
		printf("%s\n", __func__);
#endif /* DEBUG */

	and_node = parse_and_expression(ptr);
	if (and_node == NULL)
		return (NULL);
	
	/*
	 * Note that  parse_and_expression() returns
	 * with the current token
	 */
	if (lex_token.id == TOK_OR) {
		struct node *other_node;
		
		get_token(ptr);
		other_node = parse_or_expression(ptr);
		
		if (other_node == NULL) {
			free_node(and_node);
			
			return (NULL);
		} else {
			struct node *or_node = alloc_node(TOK_OR);
			
			or_node->left_node = and_node;
			or_node->right_node = other_node;
			
			return (or_node);
		}
	} else {
		return (and_node);
	}
}

node_t *
parse_expression(const char *ptr)
{
	struct node *expression = NULL;

#ifdef DEBUG
	if (parse_verbose)
		printf("%s\n", __func__);
#endif /* DEBUG */

	get_token(&ptr);
	expression = parse_or_expression(&ptr);
	
#ifdef DEBUG
	if (parse_verbose) {
		printf("%s got: ", __func__);
		print_expression(expression);
		printf("\n");
	}
#endif /* DEBUG */
	return (expression);
}

int
evaluate_expression(node_t *expression, struct pkt_meta_data *p)
{
	int match = 0;
    
	switch (expression->id) {
		case TOK_AND:
			match = evaluate_expression(expression->left_node, p) &&
			evaluate_expression(expression->right_node, p);
			break;
		case TOK_OR:
			match = evaluate_expression(expression->left_node, p) ||
			evaluate_expression(expression->right_node, p);
			break;
		case TOK_NOT:
			match = !evaluate_expression(expression->left_node, p);
			break;
            
		case TOK_IF:
			match = !strcmp(p->itf, expression->str);
			if (expression->op == TOK_NEQ)
				match = !match;
			break;
		case TOK_PROC:
			match = !strcmp(p->proc, expression->str);
			if (expression->op == TOK_NEQ)
				match = !match;
			break;
		case TOK_EPROC:
			match = !strcmp(p->eproc, expression->str);
			if (expression->op == TOK_NEQ)
				match = !match;
			break;
		case TOK_PID:
			match = (p->pid == expression->num);
			if (expression->op == TOK_NEQ)
				match = !match;
			break;
		case TOK_EPID:
			match = (p->epid == expression->num);
			if (expression->op == TOK_NEQ)
				match = !match;
			break;
		case TOK_SVC:
			match = !strcasecmp(p->svc, expression->str);
			if (expression->op == TOK_NEQ)
				match = !match;
			break;
		case TOK_DIR:
			match = !strcasecmp(p->dir, expression->str);
			if (expression->op == TOK_NEQ)
				match = !match;
			break;
		default:
			break;
	}
	return (match);
}

void
print_expression(node_t *expression)
{
	switch (expression->id) {
		case TOK_AND:
			printf("(");
			print_expression(expression->left_node);
			printf(" and ");
			print_expression(expression->right_node);
			printf(")");
			break;
			
		case TOK_OR:
			printf("(");
			print_expression(expression->left_node);
			printf(" or ");
			print_expression(expression->right_node);
			printf(")");
			break;
			
		case TOK_NOT:
			printf("not ");
			print_expression(expression->left_node);
			break;
            
		case TOK_IF:
		case TOK_PROC:
		case TOK_EPROC:
		case TOK_PID:
		case TOK_EPID:
		case TOK_SVC:
		case TOK_DIR:
			switch (expression->id) {
				case TOK_IF:
					printf("if");
					break;
				case TOK_PID:
					printf("pid");
					break;
				case TOK_EPID:
					printf("epid");
					break;
				case TOK_PROC:
					printf("proc");
					break;
				case TOK_EPROC:
					printf("eproc");
					break;
				case TOK_SVC:
					printf("svc");
					break;
				case TOK_DIR:
					printf("dir");
					break;
			}
			switch (expression->op) {
				case TOK_EQ:
					printf("=");
					break;
				case TOK_NEQ:
					printf("!=");
					break;
			}
			printf("%s", *expression->str ? expression->str : "\"\"");
		default:
			break;
	}
}

void
free_expression(node_t *expression)
{
    if (expression->left_node != NULL)
        free_expression(expression->left_node);
    if (expression->right_node != NULL)
        free_expression(expression->right_node);
    free(expression);
}
