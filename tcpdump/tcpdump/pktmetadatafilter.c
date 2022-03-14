/*
 * Copyright (c) 2012-2021 Apple Inc. All rights reserved.
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
 *  notequalexpr: "!=" str
 *  str: a-zA-Z0-9
 *  procexpr: "proc" compexpr
 *  svcexpre: "svc" compexpr
 *  direxpr: "dir" compexpr
` */

#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "pktmetadatafilter.h"

#define TOKEN_ID_LIST	\
	X(TOK_NONE)		\
	X(TOK_OR)		\
	X(TOK_AND)		\
	X(TOK_NOT)		\
	X(TOK_LP)		\
	X(TOK_RP)		\
	X(TOK_IF)		\
	X(TOK_PROC)		\
	X(TOK_EPROC)	\
	X(TOK_PID)		\
	X(TOK_EPID)		\
	X(TOK_SVC)		\
	X(TOK_DIR)		\
	X(TOK_FLOWID)	\
	X(TOK_EQ)		\
	X(TOK_NEQ)		\
	X(TOK_STR)		\

enum token_id_type {
#define X(name, ...) name,
	TOKEN_ID_LIST
#undef X
};

struct token {
	int		tok_id;
	char	*tok_label;
	size_t	tok_len;
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
	{ TOK_FLOWID, "flowid", 0 },
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
	uint32_t num;
	int op;
	struct node *left_node;
	struct node *right_node;
};

#define MAX_NODES 1000
static int num_nodes = 0;

static struct node * alloc_node(int);
static void free_node(struct node *);
static const char * get_strcharset(bool);
static void get_token(const char **);
static struct node * parse_term_expression(const char **);
static struct node * parse_paren_expression(const char **);
static struct node * parse_not_expression(const char **);
static struct node * parse_and_expression(const char **);
static struct node * parse_or_expression(const char **);

#ifdef DEBUG
static int parse_verbose = 0;

void
set_parse_verbose(int val)
{
    parse_verbose = val;
}

const char *
get_token_id_str(int tok_id)
{
#define X(name, ...) case name: return #name;
	switch (tok_id) {
		TOKEN_ID_LIST

		default:
			break;
	}
	return "invalid token";
#undef X
}
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
get_strcharset(bool quoted)
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
		if (quoted) {
			strcharset[n++] = ' ';
		}
	}
	
	return strcharset;
}

static void
get_token(const char **ptr)
{
	size_t len = 0;
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
		lex_token.tok_id = TOK_NONE;
		return;
	}
	
	for (tok = &tokens[0]; tok->tok_id != TOK_NONE; tok++) {
		if (tok->tok_len == 0)
			tok->tok_len = strlen(tok->tok_label);
		
		if (strncmp(*ptr, tok->tok_label, tok->tok_len) == 0) {
#ifdef DEBUG
			if (parse_verbose)
				printf("tok id: %s label: %s\n", get_token_id_str(tok->tok_id), tok->tok_label);
#endif /* DEBUG */

			lex_token.tok_id = tok->tok_id;
			lex_token.tok_label = strdup(tok->tok_label);
			lex_token.tok_len = tok->tok_len;
			*ptr += lex_token.tok_len;
			
			return;
		}
	}
	
	lex_token.tok_id = TOK_STR;

	if (strncmp(*ptr, "''", 2) == 0 || strncmp(*ptr, "\"\"", 2) == 0) {
		lex_token.tok_label = malloc(1);
		*lex_token.tok_label = 0; // empty string
		lex_token.tok_len = 0;
		*ptr += 2;
	} else {
		bool single_quoted = false;
		bool double_quoted = false;

		if (**ptr == '\'') {
			single_quoted = 1;
			*ptr += 1;
		} else if (**ptr == '"') {
			double_quoted = 1;
			*ptr += 1;
		}
		charset = get_strcharset(single_quoted || double_quoted);
		
		len = strspn(*ptr, charset);

		lex_token.tok_label = realloc(lex_token.tok_label, len + 1);
		strlcpy(lex_token.tok_label, *ptr, len + 1);
		lex_token.tok_len = len;
		*ptr += lex_token.tok_len;

		if (single_quoted) {
			if (**ptr != '\'') {
				lex_token.tok_id = TOK_NONE;
			} else {
				*ptr += 1;
			}
		} else if (double_quoted) {
				if (**ptr != '"') {
					lex_token.tok_id = TOK_NONE;
				} else {
					*ptr += 1;
				}
		}
	}
	
#ifdef DEBUG
	if (parse_verbose) {
		char fmt[50];

		bzero(fmt, sizeof(fmt));
		snprintf(fmt, sizeof(fmt), "tok id: %%s len: %%lu str: %%.%lus *ptr: %%s\n", len);
		printf(fmt, get_token_id_str(lex_token.tok_id) , lex_token.tok_len, lex_token.tok_label, *ptr);
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

	switch (lex_token.tok_id) {
		case TOK_IF:
		case TOK_PROC:
		case TOK_EPROC:
		case TOK_PID:
		case TOK_EPID:
		case TOK_SVC:
		case TOK_DIR:
		case TOK_FLOWID:
			term_node = alloc_node(lex_token.tok_id);
			get_token(ptr);
			
			if (lex_token.tok_id == TOK_EQ || lex_token.tok_id == TOK_NEQ)
				term_node->op = lex_token.tok_id;
			else {
				warnx("cannot parse operator at: %s", *ptr);
				goto fail;
			}
			get_token(ptr);
			if (lex_token.tok_id != TOK_STR) {
				warnx("missing comparison string at: %s", *ptr);
				goto fail;
			}
			/*
			 * TBD
			 * For TOK_SVC and TOK_DIR restrict to meaningful values
			 */
			
			term_node->str = strdup(lex_token.tok_label);
			
			if (term_node->id == TOK_PID || term_node->id == TOK_EPID || term_node->id == TOK_FLOWID) {
				term_node->num = (uint32_t)strtoul(term_node->str, NULL, 0);
			}
			break;
			
		default:
			warnx("cannot parse term at: %s", *ptr);
			break;
	}
	return (term_node);
fail:
	if (term_node != NULL)
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

	if (lex_token.tok_id == TOK_LP) {
		struct node *or_node;
		
		get_token(ptr);
		or_node = parse_or_expression(ptr);
		if (or_node == NULL)
			return (NULL);
		
		if (lex_token.tok_id != TOK_RP) {
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

	if (lex_token.tok_id == TOK_NOT) {
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
	if (lex_token.tok_id == TOK_AND) {
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
	if (lex_token.tok_id == TOK_OR) {
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
		case TOK_FLOWID:
			match = (p->flowid == expression->num);
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
    if (expression == NULL) {
        return;
    }
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
		case TOK_FLOWID:
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
				case TOK_FLOWID:
					printf("flowid");
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
