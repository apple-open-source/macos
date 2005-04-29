/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2004 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <crt_externs.h>
#include <asl.h>
#include <asl_private.h>
#include <regex.h>
#include <notify.h>
#include <mach/mach.h>
#include <pthread.h>

#define _PATH_ASL_IN "/var/run/asl_input"

#define streq(A, B) (strcmp(A, B) == 0)
#define strcaseeq(A, B) (strcasecmp(A, B) == 0)

#define forever for(;;)

#define TOKEN_NULL  0
#define TOKEN_OPEN  1
#define TOKEN_CLOSE 2
#define TOKEN_WORD  3
#define TOKEN_INT   4

/* forward */
time_t asl_parse_time(const char *);
const char *asl_syslog_faciliy_num_to_name(int n);
__private_extern__ asl_client_t *_asl_open_default();

/* notify SPI */
uint32_t notify_get_state(int token, int *state);
uint32_t notify_register_plain(const char *name, int *out_token);

typedef struct
{
	int notify_count;
	int notify_token;
	int master_token;
	char *sender;
	pthread_mutex_t lock;
	asl_client_t *asl;
} _asl_global_t;

#ifndef BUILDING_VARIANT
__private_extern__ _asl_global_t _asl_global = {0, -1, -1, NULL, PTHREAD_MUTEX_INITIALIZER, NULL};

static int
_asl_connect(asl_client_t *asl)
{
	uint32_t len, status;

	if (asl->sock >= 0) return 0;

	asl->sock = socket(AF_UNIX, SOCK_STREAM, 0); 
	if (asl->sock < 0) return -1;

	memset(&(asl->server), 0, sizeof(struct sockaddr_un));
	asl->server.sun_family = AF_UNIX;

	strcpy(asl->server.sun_path, _PATH_ASL_IN);
	len = sizeof(asl->server.sun_len) + sizeof(asl->server.sun_family) + strlen(asl->server.sun_path) + 1;
	asl->server.sun_len = strlen(_PATH_ASL_IN) + 1;

	status = connect(asl->sock, (const struct sockaddr *)&(asl->server), len);

	if (status < 0) return -1;
	return 0;
}

static int
_asl_notify_open(int do_lock)
{
	char *notify_name;
	const char *prefix;
	uint32_t status;

	if (do_lock != 0) pthread_mutex_lock(&_asl_global.lock);

	_asl_global.notify_count++;

	if (_asl_global.notify_token != -1)
	{
		if (do_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
		return 0;
	}

	notify_name = NULL;

	prefix = NOTIFY_PREFIX_USER;
	if (getuid() == 0) prefix = NOTIFY_PREFIX_SYSTEM;

	if (_asl_global.master_token == -1)
	{
		status = notify_register_plain(NOTIFY_SYSTEM_MASTER, &_asl_global.master_token);
		if (status != NOTIFY_STATUS_OK) _asl_global.master_token = -1;
	}

	asprintf(&notify_name, "%s.%d", prefix, getpid());

	if (notify_name != NULL)
	{
		status = notify_register_plain(notify_name, &_asl_global.notify_token);
		free(notify_name);
		if (status != NOTIFY_STATUS_OK) _asl_global.notify_token = -1;
	}

	if (do_lock != 0) pthread_mutex_unlock(&_asl_global.lock);

	if (_asl_global.notify_token == -1) return -1;
	return 0;
}

static void
_asl_notify_close()
{
	pthread_mutex_lock(&_asl_global.lock);

	if (_asl_global.notify_count > 0) _asl_global.notify_count--;

	if (_asl_global.notify_count > 0)
	{
		pthread_mutex_unlock(&_asl_global.lock);
		return;
	}

	if (_asl_global.master_token > 0) notify_cancel(_asl_global.master_token);
	_asl_global.master_token = -1;
	
	if (_asl_global.notify_token > 0) notify_cancel(_asl_global.notify_token);
	_asl_global.notify_token = -1;

	pthread_mutex_unlock(&_asl_global.lock);
}

aslclient
asl_open(const char *ident, const char *facility, uint32_t opts)
{
	char *name, *x;
	asl_client_t *asl;

	asl = (asl_client_t *)calloc(1, sizeof(asl_client_t));
	if (asl == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	asl->options = opts;

	asl->sock = -1;

	if (asl->options & ASL_OPT_NO_DELAY)
	{
		if (_asl_connect(asl) < 0)
		{
			free(asl);
			return NULL;
		}
	}

	asl->pid = getpid();
	asl->uid = getuid();
	asl->gid = getgid();

	asl->filter = ASL_FILTER_MASK_UPTO(ASL_LEVEL_NOTICE);

	if (ident != NULL)
	{
		asl->name = strdup(ident);
	}
	else
	{
		name = *(*_NSGetArgv());
		if (name != NULL)
		{
			x = strrchr(name, '/');
			if (x != NULL) x++;
			else x = name;
			asl->name = strdup(x);
		}
	}

	if (facility != NULL) asl->facility = strdup(facility);
	else asl->facility = strdup(asl_syslog_faciliy_num_to_name(LOG_USER));

	if (!(asl->options & ASL_OPT_NO_REMOTE)) _asl_notify_open(1);

	return (aslclient)asl;
}

void
asl_close(aslclient ac)
{
	asl_client_t *asl;

	asl = (asl_client_t *)ac;
	if (asl == NULL) return;

	if (asl->sock >= 0) close(asl->sock);
	if (asl->name != NULL) free(asl->name);
	if (asl->facility != NULL) free(asl->facility);
	if (!(asl->options & ASL_OPT_NO_REMOTE)) _asl_notify_close();
	if (asl->fd_list != NULL) free(asl->fd_list);

	memset(asl, 0, sizeof(asl_client_t));
	free(asl);
}

__private_extern__ asl_client_t *
_asl_open_default()
{
	pthread_mutex_lock(&_asl_global.lock);
	if (_asl_global.asl != NULL)
	{
		pthread_mutex_unlock(&_asl_global.lock);
		return _asl_global.asl;
	}

	/*
	 * Do a sleight-of-hand with ASL_OPT_NO_REMOTE to avoid a deadlock
	 * since asl_open(xxx, yyy, 0) calls _asl_notify_open(1)
	 * which locks _asl_global.lock.
	 */
	_asl_global.asl = asl_open(NULL, NULL, ASL_OPT_NO_REMOTE);

	/* Reset options to clear ASL_OPT_NO_REMOTE bit */
	if (_asl_global.asl != NULL) _asl_global.asl->options = 0;

	/* Now call _asl_notify_open(0) to finish the work */
	_asl_notify_open(0);

	pthread_mutex_unlock(&_asl_global.lock);
	
	return _asl_global.asl;
}

static uint32_t
_asl_msg_index(asl_msg_t *msg, const char *k)
{
	uint32_t i;

	if (msg == NULL) return (uint32_t)-1;
	if (k == NULL) return (uint32_t)-1;

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] == NULL) continue;
		if (streq(msg->key[i], k)) return i;
	}

	return (uint32_t)-1;
}

static void
_asl_append_string(char **m, uint32_t *x, char *s, uint32_t encode, uint32_t escspace)
{
	uint32_t i, n;

	if (m == NULL) return;
	if (x == NULL) return;
	if (s == NULL) return;

	n = 0;
	if (encode == 0) n = strlen(s);
	else
	{
		for (i = 0; s[i] != '\0'; i++) 
		{
			if (s[i] == '\\') n++;
			else if (s[i] == ']') n++;
			else if ((escspace != 0) && (s[i] == ' ')) n++;
			n++;
		}
	}

	if (n == 0) return;

	if (*m == NULL)
	{
		*m = malloc(n + 1);
		*x = 1;
	}
	else
	{
		*m = realloc(*m, n + (*x));
	}

	if (encode == 0)
	{
		memcpy((*m) + (*x) - 1, s, n + 1);
		*x += n;
		return;
	}

	n = *x - 1;
	for (i = 0; s[i] != '\0'; i++) 
	{
		if ((s[i] == '\\') || (s[i] == ']') || ((escspace != 0) && (s[i] == ' ')))
		{
			(*m)[n++] = '\\';
			(*m)[n++] = s[i];
		}
		else if (s[i] == '\n') (*m)[n++] = ';';
		else (*m)[n++] = s[i];
	}

	(*m)[n++] = '\0';

	*x = n;

	return;
}

static void
_asl_append_op(char **m, uint32_t *x, uint32_t op)
{
	char opstr[8];
	uint32_t i;

	if (m == NULL) return;
	if (x == NULL) return;

	if (op == ASL_QUERY_OP_NULL) return _asl_append_string(m, x, ".", 0, 0);

	i = 0;
	if (op & ASL_QUERY_OP_CASEFOLD) opstr[i++] = 'C';

	if (op & ASL_QUERY_OP_CASEFOLD) opstr[i++] = 'R';

	if (op & ASL_QUERY_OP_NUMERIC) opstr[i++] = 'N';

	if (op & ASL_QUERY_OP_PREFIX)
	{
		if (op & ASL_QUERY_OP_SUFFIX) opstr[i++] = 'S';
		else opstr[i++] = 'A';
	}
	if (op & ASL_QUERY_OP_SUFFIX) opstr[i++] = 'Z';

	switch (op & ASL_QUERY_OP_TRUE)
	{
		case ASL_QUERY_OP_EQUAL:
			opstr[i++] = '=';
			break;
		case ASL_QUERY_OP_GREATER:
			opstr[i++] = '>';
			break;
		case ASL_QUERY_OP_GREATER_EQUAL:
			opstr[i++] = '>';
			opstr[i++] = '=';
			break;
		case ASL_QUERY_OP_LESS:
			opstr[i++] = '<';
			break;
		case ASL_QUERY_OP_LESS_EQUAL:
			opstr[i++] = '<';
			opstr[i++] = '=';
			break;
		case ASL_QUERY_OP_NOT_EQUAL:
			opstr[i++] = '!';
			break;
		case ASL_QUERY_OP_TRUE:
			opstr[i++] = 'T';
			break;
		default:
			break;
	}

	if (i == 0) return _asl_append_string(m, x, ".", 0, 0);

	opstr[i++] = '\0';
	return _asl_append_string(m, x, opstr, 0, 0);
}

char *
asl_msg_to_string(asl_msg_t *msg, uint32_t *len)
{
	uint32_t i, outlen;
	char *out, *s;

	*len = 0;

	if (msg == NULL) return NULL;

	s = NULL;
	out = NULL;
	outlen = 0;

	if (msg->type == ASL_TYPE_QUERY)
	{
		_asl_append_string(&out, &outlen, "Q ", 0, 0);
		if (out == NULL) return NULL;
	}

	if (msg->count == 0)
	{
		if (out == NULL) return NULL;
		*len = outlen;
		return out;
	}

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] == NULL) continue;

		if (i > 0) _asl_append_string(&out, &outlen, " [", 0, 0);
		else _asl_append_string(&out, &outlen, "[", 0, 0);

		if (msg->type == ASL_TYPE_QUERY)
		{
			_asl_append_op(&out, &outlen, msg->op[i]);
			_asl_append_string(&out, &outlen, " ", 0, 0);
		}

		_asl_append_string(&out, &outlen, msg->key[i], 1, 1);

		if (msg->val[i] != NULL)
		{
			_asl_append_string(&out, &outlen, " ", 0, 0);
			_asl_append_string(&out, &outlen, msg->val[i], 1, 0);
		}

		_asl_append_string(&out, &outlen, "]", 0, 0);
	}

	*len = outlen;
	return out;
}

static uint32_t
_asl_msg_op_from_string(char *o)
{
	uint32_t op, i;

	op = ASL_QUERY_OP_NULL;

	if (o == NULL) return op;

	for (i = 0; o[i] != '\0'; i++)
	{
		if (o[i] == '.') return ASL_QUERY_OP_NULL;
		if (o[i] == 'C') op |= ASL_QUERY_OP_CASEFOLD;
		if (o[i] == 'R') op |= ASL_QUERY_OP_CASEFOLD;
		if (o[i] == 'N') op |= ASL_QUERY_OP_NUMERIC;
		if (o[i] == 'S') op |= ASL_QUERY_OP_SUBSTRING;
		if (o[i] == 'A') op |= ASL_QUERY_OP_PREFIX;
		if (o[i] == 'Z') op |= ASL_QUERY_OP_SUFFIX;
		if (o[i] == '<') op |= ASL_QUERY_OP_LESS;
		if (o[i] == '>') op |= ASL_QUERY_OP_GREATER;
		if (o[i] == '=') op |= ASL_QUERY_OP_EQUAL;
		if (o[i] == '!') op |= ASL_QUERY_OP_NOT_EQUAL;
		if (o[i] == 'T') op |= ASL_QUERY_OP_TRUE;
	}

	return op;
}

static char *
_asl_msg_get_next_word(char **p, uint32_t *tt, uint32_t spacedel)
{
	char *start, *out;
	uint32_t i, esc, len, n;

	*tt = TOKEN_NULL;

	if (p == NULL) return NULL;
	if (*p == NULL) return NULL;
	if (**p == '\0') return NULL;

	/* skip one space if it's there (word separator) */
	if (**p == ' ') (*p)++;

	/* skip leading white space */
	if (spacedel != 0)
	{
		while ((**p == ' ') || (**p == '\t')) (*p)++;
	}

	if (**p == '\0') return NULL;
	if (**p == '\n') return NULL;

	/* opening [ */
	if (**p == '[')
	{
		*tt = TOKEN_OPEN;

		(*p)++;
		out = malloc(2);
		out[0] = '[';
		out[1] = '\0';
		return out;
	}

	start = *p;
	len = 0;

	forever
	{
		/* stop scanning when we hit a delimiter */
		if (((spacedel != 0) && (**p == ' ')) || (**p == ']') || (**p == '\0')) break;

		esc = 0;
		if (**p == '\\') esc = 1;
		(*p)++;

		/* skip over escaped chars so len is correct */
		if ((esc == 1) && ((**p == ' ') || (**p == ']') || (**p == '\\'))) (*p)++;
		len++;
	}

	if ((len == 0) && (**p == ']'))
	{
		*tt = TOKEN_CLOSE;
		(*p)++;
		out = malloc(2);
		out[0] = ']';
		out[1] = '\0';
		return out;
	}

	*tt = TOKEN_INT;

	out = malloc(len + 1);

	for (n = 0, i = 0; n < len; i++)
	{
		if ((start[i] == '\\') && ((start[i+1] == ' ') || (start[i+1] == ']') || (start[i+1] == '\\')))
		{
			*tt = TOKEN_WORD;
			i++;
		}

		if ((start[i] < '0') || (start[i] > '9')) *tt = TOKEN_WORD;
		out[n++] = start[i];
	}

	out[n] = '\0';

	return out;
}

asl_msg_t *
asl_msg_from_string(const char *buf)
{
	uint32_t tt, type, op;
	char *k, *v, *o, *p;
	asl_msg_t *msg;
	
	if (buf == NULL) return NULL;

	type = ASL_TYPE_MSG;
	p = (char *)buf;

	k = _asl_msg_get_next_word(&p, &tt, 1);
	if (k == NULL) return NULL;

	if (streq(k, "Q"))
	{
		type = ASL_TYPE_QUERY;
		free(k);

		k = _asl_msg_get_next_word(&p, &tt, 1);
	}
	else if (tt == TOKEN_INT)
	{
		/* Leading integer is a string length - skip it */
		free(k);
		k = _asl_msg_get_next_word(&p, &tt, 1);
		if (k == NULL) return NULL;
	}

	msg = calloc(1, sizeof(asl_msg_t));
	if (msg == NULL) return NULL;
	msg->type = type;
	
	/* OPEN WORD [WORD [WORD]] CLOSE */
	while (k != NULL)
	{
		op = ASL_QUERY_OP_NULL;

		if (tt != TOKEN_OPEN)
		{
			asl_free(msg);
			return NULL;
		}

		free(k);

		/* get op for query type */
		if (type == ASL_TYPE_QUERY)
		{
			o = _asl_msg_get_next_word(&p, &tt, 1);
			if ((o == NULL) || (tt != TOKEN_WORD))
			{
				if (o != NULL) free(o);
				asl_free(msg);
				return NULL;
			}

			op = _asl_msg_op_from_string(o);
			free(o);
		}

		k = _asl_msg_get_next_word(&p, &tt, 1);
		if (tt == TOKEN_INT) tt = TOKEN_WORD;
		if ((k == NULL) || (tt != TOKEN_WORD))
		{
			if (k != NULL) free(k);
			asl_free(msg);
			return NULL;
		}

		v = _asl_msg_get_next_word(&p, &tt, 0);
		if (tt == TOKEN_INT) tt = TOKEN_WORD;
		if (v == NULL) 
		{
			asl_set_query(msg, k, NULL, op);
			break;
		}

		if (tt == TOKEN_CLOSE)
		{
			asl_set_query(msg, k, NULL, op);
		}
		else if (tt == TOKEN_WORD)
		{
			asl_set_query(msg, k, v, op);
		}
		else
		{
			if (k != NULL) free(k);
			if (v != NULL) free(v);
			asl_free(msg);
			return NULL;
		}

		if (k != NULL) free(k);
		if (v != NULL) free(v);

		if (tt != TOKEN_CLOSE)
		{
			k = _asl_msg_get_next_word(&p, &tt, 1);
			if (k == NULL) break;

			if (tt != TOKEN_CLOSE)
			{
				asl_free(msg);
				return NULL;
			}

			free(k);
		}

		k = _asl_msg_get_next_word(&p, &tt, 1);
		if (k == NULL) break;
	}

	return msg;
}

static int
_asl_msg_equal(asl_msg_t *a, asl_msg_t *b)
{
	uint32_t i, j;

	if (a->count != b->count) return 0;

	for (i = 0; i < a->count; i++)
	{
		j = _asl_msg_index(b, a->key[i]);
		if (j == (uint32_t)-1) return 0;

		if (a->val[i] == NULL)
		{
			if (b->val[j] != NULL) return 0;
		}
		else
		{
			if (b->val[j] == NULL) return 0;
			if (strcmp(a->val[i], b->val[j])) return 0;
		}

		if (a->type == ASL_TYPE_QUERY)
		{
			if (a->op[i] != b->op[j]) return 0;
		}
	}

	return 1;
}

static int
_asl_isanumber(char *s)
{
	int i;

	if (s == NULL) return 0;

	i = 0;
	if ((s[0] == '-') || (s[0] == '+')) i = 1;

	if (s[i] == '\0') return 0;

	for (; s[i] != '\0'; i++)
	{
		if (!isdigit(s[i])) return 0;
	}

	return 1;
}

static int
_asl_msg_op_test(uint32_t op, char *q, char *m, uint32_t n)
{
	int cmp;
	uint32_t t;
	int nq, nm, rflags;
	regex_t rex;

	t = op & ASL_QUERY_OP_TRUE;

	if (op & ASL_QUERY_OP_REGEX)
	{
		memset(&rex, 0, sizeof(regex_t));

		rflags = REG_EXTENDED | REG_NOSUB;
		if (op & ASL_QUERY_OP_CASEFOLD) rflags |= REG_ICASE;

		if (regcomp(&rex, q, rflags) != 0) return 0;
		return (regexec(&rex, m, 0, NULL, 0) == 0);
	}

	if (op & ASL_QUERY_OP_NUMERIC)
	{
		/* We assume the query contains a numeric string */
		if (_asl_isanumber(m) == 0) return 0;

		nq = atoi(q);
		nm = atoi(m);

		switch (t)
		{
			case ASL_QUERY_OP_EQUAL: return (nm == nq);
			case ASL_QUERY_OP_GREATER: return (nm > nq);
			case ASL_QUERY_OP_GREATER_EQUAL: return (nm >= nq);
			case ASL_QUERY_OP_LESS: return (nm < nq);
			case ASL_QUERY_OP_LESS_EQUAL: return (nm <= nq);
			case ASL_QUERY_OP_NOT_EQUAL: return (nm != nq);
			default: return 0;
		}
	}

	cmp = 0;
	if (op & ASL_QUERY_OP_CASEFOLD)
	{
		if (n == 0) cmp = strcasecmp(m, q);
		else cmp = strncasecmp(m, q, n);
	}
	else 
	{
		if (n == 0) cmp = strcmp(m, q);
		else cmp = strncmp(m, q, n);
	}

	switch (t)
	{
		case ASL_QUERY_OP_EQUAL: return (cmp == 0);
		case ASL_QUERY_OP_GREATER: return (cmp > 0);
		case ASL_QUERY_OP_GREATER_EQUAL: return (cmp >= 0);
		case ASL_QUERY_OP_LESS: return (cmp < 0);
		case ASL_QUERY_OP_LESS_EQUAL: return (cmp <= 0);
		case ASL_QUERY_OP_NOT_EQUAL: return (cmp != 0);
		default: return 0;
	}

	return 0;
}

static int
_asl_msg_test_op_substr(uint32_t op, char *q, char *m)
{
	uint32_t i, d, lm, lq;

	lm = strlen(m);
	lq = strlen(q);

	if (lq > lm) return 0;

	d = lm - lq;
	for (i = 0; i < d; i++)
	{
		if (_asl_msg_op_test(op, q, m + i, lq) != 0) return 1;
	}

	return 0;
}

static int
_asl_msg_test_op_prefix(uint32_t op, char *q, char *m)
{
	uint32_t lm, lq;

	lm = strlen(m);
	lq = strlen(q);

	if (lq > lm) return 0;

	return _asl_msg_op_test(op, q, m, lq);
}

static int
_asl_msg_test_op_suffix(uint32_t op, char *q, char *m)
{
	uint32_t lm, lq, d;

	lm = strlen(m);
	lq = strlen(q);

	if (lq > lm) return 0;

	d = lm - lq;
	return _asl_msg_op_test(op, q, m + d, lq);
}

static int
_asl_msg_test_op(uint32_t op, char *q, char *m)
{
	uint32_t t;

	t = op & ASL_QUERY_OP_TRUE;
	if (t == ASL_QUERY_OP_TRUE) return 1;

	if (op & ASL_QUERY_OP_PREFIX)
	{
		if (op & ASL_QUERY_OP_SUFFIX) return _asl_msg_test_op_substr(op, q, m);
		return _asl_msg_test_op_prefix(op, q, m);
	}
	if (op & ASL_QUERY_OP_SUFFIX) return _asl_msg_test_op_suffix(op, q, m);

	return _asl_msg_op_test(op, q, m, 0);
}

static int
_asl_msg_test(asl_msg_t *q, asl_msg_t *m)
{
	uint32_t i, j;
	int cmp, freeval;
	char *val;
	struct tm gtime;
	time_t tick;
	
	for (i = 0; i < q->count; i++)
	{
		j = _asl_msg_index(m, q->key[i]);
		if (j == (uint32_t)-1) return 0;

		if (q->val[i] == NULL) continue;
		if ((q->op[i] & ASL_QUERY_OP_TRUE) == ASL_QUERY_OP_TRUE) continue;

		if (m->val[j] == NULL) return 0;

		val = q->val[i];
		freeval = 0;
		
		if (streq(q->key[i], ASL_KEY_TIME))
		{
			tick = asl_parse_time(val);
			if (tick != -1) 
			{
				memset(&gtime, 0, sizeof(struct tm));
				gmtime_r(&tick, &gtime);
					
				/* Canonical form: YYYY.MM.DD hh:mm:ss UTC */
				val = NULL;
				asprintf(&val, "%d.%02d.%02d %02d:%02d:%02d UTC", gtime.tm_year + 1900, gtime.tm_mon + 1, gtime.tm_mday, gtime.tm_hour, gtime.tm_min, gtime.tm_sec);
				freeval = 1;			}
		}

		cmp = _asl_msg_test_op(q->op[i], val, m->val[j]);
		if ((freeval == 1) && (val != NULL)) free(val);

		if (cmp == 0) return 0;
	}

	return 1;
}

int
asl_msg_cmp(asl_msg_t *a, asl_msg_t *b)
{
	if (a == NULL) return 0;
	if (b == NULL) return 0;

	if (a->type == b->type) return _asl_msg_equal(a, b);
	if (a->type == ASL_TYPE_QUERY) return _asl_msg_test(a, b);
	return _asl_msg_test(b, a);
}

static char *
_get_line_from_file(FILE *f)
{
	char *s, *out;
	size_t len;

	out = fgetln(f, &len);
	if (out == NULL) return NULL;
	if (len == 0) return NULL;

	if (out[len] != '\n') len++;

	s = malloc(len);
	memcpy(s, out, len - 1);

	s[len] = '\0';
	return s;
}

/*
 * asl_add_file: write log messages to the given file descriptor
 * Log messages will be written to this file as well as to the server.
 */
int
asl_add_log_file(aslclient ac, int fd)
{
	uint32_t i;
	int use_global_lock;
	asl_client_t *asl;

	use_global_lock = 0;
	asl = (asl_client_t *)ac;
	if (asl == NULL)
	{
		asl = _asl_open_default();
		if (asl == NULL) return -1;
		pthread_mutex_lock(&_asl_global.lock);
		use_global_lock = 1;
	}

	for (i = 0; i < asl->fd_count; i++) 
	{
		if (asl->fd_list[i] == fd)
		{
			if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
			return 0;
		}
	}

	if (asl->fd_count == 0)
	{
		asl->fd_list = (int *)calloc(1, sizeof(int));
	}
	else
	{
		asl->fd_list = (int *)realloc(asl->fd_list, (1 + asl->fd_count) * sizeof(int));
	}

	if (asl->fd_list == NULL)
	{
		if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
		return -1;
	}

	asl->fd_list[asl->fd_count] = fd;
	asl->fd_count++;

	if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
	return 0;
}

/*
 * asl_remove_file: stop writing log messages to the given file descriptor
 */
int
asl_remove_log_file(aslclient ac, int fd)
{
	uint32_t i;
	int x, use_global_lock;
	asl_client_t *asl;

	use_global_lock = 0;
	asl = (asl_client_t *)ac;
	if (asl == NULL)
	{
		asl = _asl_open_default();
		if (asl == NULL) return -1;
		pthread_mutex_lock(&_asl_global.lock);
		use_global_lock = 1;
	}
	
	if (asl->fd_count == 0)
	{
		if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
		return 0;
	}

	x = -1;
	for (i = 0; i < asl->fd_count; i++) 
	{
		if (asl->fd_list[i] == fd)
		{
			x = i;
			break;
		}
	}

	if (x == -1)
	{
		if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
		return 0;
	}

	for (i = x + 1; i < asl->fd_count; i++, x++) asl->fd_list[x] = asl->fd_list[i];
	asl->fd_count--;

	if (asl->fd_count == 0)
	{
		free(asl->fd_list);
		asl->fd_list = NULL;
	}
	else
	{
		asl->fd_list = (int *)realloc(asl->fd_list, asl->fd_count * sizeof(int));
		if (asl->fd_list == NULL) 
		{
			asl->fd_count = 0;
			if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
			return -1;
		}
	}

	if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
	return 0;
}

int
asl_set_filter(aslclient ac, int f)
{
	int last, use_global_lock;
	asl_client_t *asl;
	
	use_global_lock = 0;
	asl = (asl_client_t *)ac;
	if (asl == NULL)
	{
		asl = _asl_open_default();
		if (asl == NULL) return -1;
		pthread_mutex_lock(&_asl_global.lock);
		use_global_lock = 1;
	}
	
	last = asl->filter;
	asl->filter = f;

	if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
	return last;
}

/*
 * asl_key: examine attribute keys
 * returns the key of the nth attribute in a message (beginning at zero)
 * returns NULL if the message has fewer attributes
 */
const char *
asl_key(aslmsg a, uint32_t n)
{
	asl_msg_t *msg;

	msg = (asl_msg_t *)a;
	if (msg == NULL) return NULL;

	if (n >= msg->count) return NULL;
	return msg->key[n];
}

/*
 * asl_new: create a new log message.
 */
aslmsg
asl_new(uint32_t type)
{
	uint32_t i;
	asl_msg_t *msg;
	char *name, *x;

	msg = calloc(1, sizeof(asl_msg_t));
	if (msg == NULL) return NULL;

	msg->type = type;
	if (type == ASL_TYPE_QUERY) return (aslmsg)msg;

	/*
	 * Defaut attributes are:
	 * 0 Time
	 * 1 Host
	 * 2 Sender
	 * 3 PID
	 * 4 UID
	 * 5 GID
	 * 6 Level
	 * 7 Message
	 */
	msg->count = 8;

	msg->key = calloc(msg->count, sizeof(char *));
	if (msg->key == NULL)
	{
		free(msg);
		return NULL;
	}

	msg->val = calloc(msg->count, sizeof(char *));
	if (msg->val == NULL)
	{
		free(msg->key);
		free(msg);
		return NULL;
	}

	i = 0;
	msg->key[i] = strdup(ASL_KEY_TIME);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}

	i++;
	msg->key[i] = strdup(ASL_KEY_HOST);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}
	
	i++;
	msg->key[i] = strdup(ASL_KEY_SENDER);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}
	
	/* Get the value for ASL_KEY_SENDER from cache */
	if (_asl_global.sender == NULL)
	{		
		name = *(*_NSGetArgv());
		if (name != NULL)
		{
			x = strrchr(name, '/');
			if (x != NULL) x++;
			else x = name;

			pthread_mutex_lock(&_asl_global.lock);
			if (_asl_global.sender == NULL) _asl_global.sender = strdup(x);
			pthread_mutex_unlock(&_asl_global.lock);
		}
	}

	if (_asl_global.sender == NULL)
	{
		msg->val[i] = strdup("Unknown");
		if (msg->val[i] == NULL)
		{
			asl_free(msg);
			return NULL;
		}
	}
	else
	{
		msg->val[i] = strdup(_asl_global.sender);
		if (msg->val[i] == NULL)
		{
			asl_free(msg);
			return NULL;
		}
	}
	
	i++;
	msg->key[i] = strdup(ASL_KEY_PID);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}
	
	i++;
	msg->key[i] = strdup(ASL_KEY_UID);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}
	
	i++;
	msg->key[i] = strdup(ASL_KEY_GID);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}
	
	i++;
	msg->key[i] = strdup(ASL_KEY_LEVEL);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}
	
	i++;
	msg->key[i] = strdup(ASL_KEY_MSG);
	if (msg->key[i] == NULL) 
	{
		asl_free(msg);
		return NULL;
	}
	
	return (aslmsg)msg;
}

/*
 * asl_get: get attribute values from a message 
 * msg:  an aslmsg
 * key:  attribute key 
 * returns the attribute value
 * returns NULL if the message does not contain the key
 */
const char *
asl_get(aslmsg a, const char *key)
{
	asl_msg_t *msg;
	uint32_t i;

	msg = (asl_msg_t *)a;

	if (msg == NULL) return NULL;

	i = _asl_msg_index(msg, key);
	if (i == (uint32_t)-1) return NULL;
	return msg->val[i];
}

#endif /* BUILDING_VARIANT */

/*
 * asl_vlog: Similar to asl_log, but taking a va_list instead of a list of
 * arguments.
 * msg:  an aslmsg
 * level: the log level of the associated message
 * format: A formating string followed by a list of arguments, like vprintf()
 * returns 0 for success, non-zero for failure
 */
int
asl_vlog(aslclient ac, aslmsg a, int level, const char *format, va_list ap)
{
	int status, saved_errno;
	asl_msg_t *msg;
	char *str, *fmt, *estr;
	uint32_t i, len, elen, expand, my_msg;
	asl_client_t *asl;

	asl = (asl_client_t *)ac;
	if (asl == NULL)
	{
		/*
		 * Initialize _asl_global so that asl_new will have global data.
		 * Not strictly necessary, but helps performance.
		 */
		asl = _asl_open_default();
		if (asl == NULL) return -1;
	}
	
	saved_errno = errno;

	if (format == NULL) return -1;

	msg = (asl_msg_t *)a;

	my_msg = 0;
	if (msg == NULL) 
	{
		my_msg = 1;
		msg = asl_new(ASL_TYPE_MSG);
		if (msg == NULL) return -1;
	}

	if (msg->type != ASL_TYPE_MSG) return -1;

	if (level < ASL_LEVEL_EMERG) level = ASL_LEVEL_EMERG;
	if (level > ASL_LEVEL_DEBUG) level = ASL_LEVEL_DEBUG;

	str = NULL;
	asprintf(&str, "%d", level);
	if (str == NULL)
	{
		if ((msg != NULL) && (my_msg != 0)) asl_free(msg);
		return -1;
	}

	asl_set(msg, ASL_KEY_LEVEL, str);
	free(str);

	/* insert strerror for %m */
	len = 0;
	elen = 0;
	estr = strdup(strerror(saved_errno));
	expand = 0;

	if (estr != NULL)
	{
		elen = strlen(estr);

		for (i = 0; format[i] != '\0'; i++)
		{
			if (format[i] == '%')
			{
				if (format[i+1] == '\0') len++;
				else if (format[i+1] == 'm')
				{
					expand = 1;
					len += elen;
					i++;
				}
				else
				{
					len += 2;
					i++;
				}
			}
			else len++;
		}
	}

	fmt = (char *)format;

	if (expand != 0)
	{
		fmt = malloc(len + 1);
		len = 0;

		for (i = 0; format[i] != '\0'; i++)
		{
			if (format[i] == '%')
			{
				if (format[i+1] == '\0')
				{
				}
				else if (format[i+1] == 'm')
				{
					memcpy(fmt+len, estr, elen);
					len += elen;
					i++;
				}
				else
				{
					fmt[len++] = format[i++];
					fmt[len++] = format[i];
				}
			}
			else fmt[len++] = format[i];
		}

		fmt[len] = '\0';
	}

	if (estr != NULL) free(estr);

	vasprintf(&str, fmt, ap);
	if (expand != 0) free(fmt);

	if (str == NULL)
	{
		if ((msg != NULL) && (my_msg != 0)) asl_free(msg);
		return -1;
	}

	asl_set(msg, ASL_KEY_MSG, str);
	free(str);

	status = asl_send(ac, (aslmsg)msg);

	if ((msg != NULL) && (my_msg != 0)) asl_free(msg);
	return status;
}

/*
 * asl_log: log a message with a particular log level 
 * msg:  an aslmsg
 * level: the log level
 * format: A formating string followed by a list of arguments, like printf()
 * returns 0 for success, non-zero for failure
 */
int
asl_log(aslclient ac, aslmsg a, int level, const char *format, ...)
{
	va_list ap;
	int status;

	if (format == NULL) return -1;

	va_start(ap, format);
	status = asl_vlog(ac, a, level, format, ap);
	va_end(ap);

	return status;
}

#ifndef BUILDING_VARIANT

/*
 * asl_send: send a message 
 * This routine may be used instead of asl_log() or asl_vlog() if asl_set() 
 * has been used to set all of a message's attributes.
 * msg:  an aslmsg
 * returns 0 for success, non-zero for failure
 */
int
asl_send(aslclient ac, aslmsg msg)
{
	char *str, *out;
	uint32_t i, len, level, lmask, outstatus, filter;
	const char *val;
	time_t tick;
	int status, rc_filter;
	asl_client_t *asl;
	int use_global_lock;

	use_global_lock = 0;
	asl = (asl_client_t *)ac;
	if (asl == NULL)
	{
		asl = _asl_open_default();
		if (asl == NULL) return -1;
		use_global_lock = 1;
	}

	if (msg == NULL) return 0;

	level = ASL_LEVEL_DEBUG;

	val = asl_get(msg, ASL_KEY_LEVEL);
	if (val != NULL) level = atoi(val);

	lmask = ASL_FILTER_MASK(level);
	
	filter = asl->filter;
	rc_filter = 0;

	if (!(asl->options & ASL_OPT_NO_REMOTE))
	{
		pthread_mutex_lock(&_asl_global.lock);

		if (_asl_global.notify_token >= 0)
		{
			status = notify_get_state(_asl_global.notify_token, &i);
			if ((status == NOTIFY_STATUS_OK) && (i != 0))
			{
				filter = i;
				rc_filter = 1;
			}
		}

		if ((rc_filter == 0) && (_asl_global.master_token >= 0))
		{
			status = notify_get_state(_asl_global.master_token, &i);
			if ((status == NOTIFY_STATUS_OK) && (i != 0))
			{
				filter = i;
			}
		}

		pthread_mutex_unlock(&_asl_global.lock);
	}

	/* 
	 * Time, PID, UID, and GID values get set here
	 */
	str = NULL;
	tick = time(NULL);
	asprintf(&str, "%u", tick);
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_TIME, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%u", getpid());
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_PID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%d", getuid());
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_UID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%u", getgid());
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_GID, str);
		free(str);
	}

	len = 0;
	str = asl_msg_to_string((asl_msg_t *)msg, &len);
	if (str == NULL) return -1;

	asprintf(&out, "%10u %s\n", len+1, str);
	free(str);
	if (out == NULL) return -1;

	outstatus = 0;

	if (use_global_lock != 0) pthread_mutex_lock(&_asl_global.lock);

	if ((filter != 0) && ((filter & lmask) != 0))
	{
		if (asl->sock == -1) _asl_connect(asl);

		status = write(asl->sock, out, len + 12);
		if (status < 0)
		{
			/* Write failed - try resetting */
			asl->sock = -1;
			_asl_connect(asl);
			status = write(asl->sock, out, len + 12);
			if (status < 0) outstatus = -1;
		}
	}

	if (asl->options & ASL_OPT_STDERR) fprintf(stderr, "%s", out);

	for (i = 0; i < asl->fd_count; i++)
	{
		if (asl->fd_list[i] < 0) continue;
		status = write(asl->fd_list[i], out, len + 12);
		if (status < 0)
		{
			asl->fd_list[i] = -1;
			outstatus = -1;
		}
	}

	if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);

	free(out);

	return outstatus;
}

char *
asl_msg_string(aslmsg a)
{
	uint32_t len;

	return asl_msg_to_string((asl_msg_t *)a, &len);
}

/*
 * asl_free: free a message 
 * msg:  an aslmsg to free
 */
void
asl_free(aslmsg a)
{
	uint32_t i;
	asl_msg_t *msg;

	msg = (asl_msg_t *)a;

	if (msg == NULL) return;

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] != NULL) free(msg->key[i]);
		if (msg->val[i] != NULL) free(msg->val[i]);
	}

	if (msg->count > 0) 
	{
		if (msg->key != NULL) free(msg->key);
		if (msg->val != NULL) free(msg->val);
		if (msg->op != NULL) free(msg->op);
	}

	free(msg);
}

/*
 * asl_set_query: set arbitrary parameters of a query
 * Similar to als_set, but allows richer query operations.
 * See ASL_QUERY_OP_* above.
 * msg:  an aslmsg
 * key:  attribute key 
 * value:  attribute value
 * op:  an operation from the set above.
 * returns 0 for success, non-zero for failure
 */
int asl_set_query(aslmsg a, const char *key, const char *val, uint32_t op)
{
	uint32_t i;
	char *dk, *dv;
	asl_msg_t *msg;

	msg = (asl_msg_t *)a;

	if (msg == NULL) return 0;

	if (key == NULL) return -1;

	dv = NULL;

	if (streq(key, ASL_KEY_LEVEL))
	{
		if (val == NULL) return -1;
		if (val[0] == '\0') return -1;
		if ((val[0] >= '0') && (val[0] <= '9')) 
		{
			i = atoi(val);
			asprintf(&dv, "%d", i);
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_EMERG)) dv = strdup("0");
		else if (!strcasecmp(val, ASL_STRING_ALERT)) dv = strdup("1");
		else if (!strcasecmp(val, ASL_STRING_CRIT)) dv = strdup("2");
		else if (!strcasecmp(val, ASL_STRING_ERR)) dv = strdup("3");
		else if (!strcasecmp(val, ASL_STRING_WARNING)) dv = strdup("4");
		else if (!strcasecmp(val, ASL_STRING_NOTICE)) dv = strdup("5");
		else if (!strcasecmp(val, ASL_STRING_INFO)) dv = strdup("6");
		else if (!strcasecmp(val, ASL_STRING_DEBUG)) dv = strdup("7");
		else return -1;
	}

	if ((dv == NULL) && (val != NULL))
	{
		dv = strdup(val);
		if (dv == NULL) return -1;
	}

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] == NULL) continue;

		if ((msg->type != ASL_TYPE_QUERY) && (streq(msg->key[i], key)))
		{
			if (msg->val[i] != NULL) free(msg->val[i]);
			msg->val[i] = NULL;
			if (val != NULL) msg->val[i] = dv;
			if (msg->op != NULL) msg->op[i] = op;
			return 0;
		}
	}

	if (msg->count == 0)
	{
		msg->key = (char **)calloc(1, sizeof(char *));
		if (msg->key == NULL)
		{
			asl_free(msg);
			return -1;
		}

		msg->val = (char **)calloc(1, sizeof(char *));
		if (msg->val == NULL)
		{
			asl_free(msg);
			return -1;
		}

		if (msg->type == ASL_TYPE_QUERY)
		{
			msg->op = (uint32_t *)calloc(1, sizeof(uint32_t));
			if (msg->op == NULL)
			{
				asl_free(msg);
				return -1;
			}
		}
	}
	else
	{
		msg->key = (char **)realloc(msg->key, (msg->count + 1) * sizeof(char *));
		if (msg->key == NULL)
		{
			asl_free(msg);
			return -1;
		}

		msg->val = (char **)realloc(msg->val, (msg->count + 1) * sizeof(char *));
		if (msg->val == NULL)
		{
			asl_free(msg);
			return -1;
		}

		if (msg->type == ASL_TYPE_QUERY)
		{
			msg->op = (uint32_t *)realloc(msg->op, (msg->count + 1) * sizeof(uint32_t));
			if (msg->op == NULL)
			{
				asl_free(msg);
				return -1;
			}
		}
	}

	dk = strdup(key);
	if (dk == NULL) return -1;
	
	msg->key[msg->count] = dk;
	msg->val[msg->count] = dv;
	if (msg->op != NULL) msg->op[msg->count] = op;
	msg->count++;

	return 0;
}

/*
 * asl_set: set attributes of a message 
 * msg:  an aslmsg
 * key:  attribute key 
 * value:  attribute value
 * returns 0 for success, non-zero for failure
 */
int
asl_set(aslmsg msg, const char *key, const char *val)
{
	return asl_set_query(msg, key, val, 0);
}

/*
 * asl_unset: remove attributes of a message 
 * msg:  an aslmsg
 * key:  attribute key 
 * returns 0 for success, non-zero for failure
 */
int
asl_unset(aslmsg a, const char *key)
{
	uint32_t i, j;
	asl_msg_t *msg;

	msg = (asl_msg_t *)a;

	if (msg == NULL) return 0;
	if (key == NULL) return 0;

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] == NULL) continue;

		if (streq(msg->key[i], key))
		{
			free(msg->key[i]);
			if (msg->val[i] != NULL) free(msg->val[i]);

			for (j = i + 1; j < msg->count; j++, i++)
			{
				msg->key[i] = msg->key[j];
				msg->val[i] = msg->val[j];
				if (msg->op != NULL) msg->op[i] = msg->op[j];
			}

			msg->count--;

			if (msg->count == 0)
			{
				free(msg->key);
				msg->key = NULL;

				free(msg->val);
				msg->val = NULL;

				if (msg->op != NULL) free(msg->op);
				msg->op = NULL;
			}
			else
			{
				msg->key = (char **)realloc(msg->key, msg->count * sizeof(char *));
				if (msg->key == NULL) return -1;

				msg->val = (char **)realloc(msg->val, msg->count * sizeof(char *));
				if (msg->val == NULL) return -1;

				if (msg->op != NULL)
				{
					msg->op = (uint32_t *)realloc(msg->op, msg->count * sizeof(uint32_t));
					if (msg->op == NULL) return -1;
				}
			}

			return 0;
		}
	}

	return 0;
}

/*
 * asl_search: Search for messages matching the criteria described
 * by the aslmsg .  The caller should set the attributes to match using
 * asl_set_query() or asl_set().  The operatoin ASL_QUERY_OP_EQUAL is
 * used for attributes set with asl_set().
 * a:  an aslmsg
 * returns: a set of messages that can be iterated over using aslresp_next(),
 * and the values can be retrieved using aslresp_get.
 */
aslresponse
asl_search(aslclient ac, aslmsg a)
{
	FILE *log;
	asl_msg_t *q, *m;
	asl_search_result_t *res;
	char *str;

	q = (asl_msg_t *)a;

	if (q == NULL) return 0;

	log = fopen(_PATH_ASL_OUT, "r");
	if (log == NULL) return NULL;

	res = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));

	while (NULL != (str = _get_line_from_file(log)))
	{
		m = asl_msg_from_string(str);
		if (m == NULL) continue;
		if (asl_msg_cmp(q, m) == 0)
		{
			asl_free(m);
			continue;
		}

		if (res->count == 0)
		{
			res->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
		}
		else
		{
			res->msg = (asl_msg_t **)realloc(res->msg, (res->count + 1) * sizeof(asl_msg_t *));
		}

		res->msg[res->count] = m;
		res->count++;
	}

	fclose(log);
	return res;
}

/*
 * aslresponse_next: Iterate over responses returned from asl_search()
 * a: a response returned from asl_search();
 * returns: The next log message (an aslmsg) or NULL on failure
 */
aslmsg
aslresponse_next(aslresponse r)
{
	asl_search_result_t *res;
	aslmsg m;

	res = (asl_search_result_t *)r;
	if (res == NULL) return NULL;

	if (res->curr >= res->count) return NULL;
	m = res->msg[res->curr];
	res->curr++;

	return m;
}

/*
 * aslresponse_free: Free a response returned from asl_search() 
 * a: a response returned from asl_search()
 */
void
aslresponse_free(aslresponse r)
{
	asl_search_result_t *res;
	uint32_t i;

	res = (asl_search_result_t *)r;
	if (res == NULL) return;

	for (i = 0; i < res->count; i++) free(res->msg[i]);
	free(res->msg);
	free(res);
}

int
asl_syslog_faciliy_name_to_num(const char *name)
{
	if (name == NULL) return -1;

	if (strcaseeq(name, "auth")) return LOG_AUTH;
	if (strcaseeq(name, "authpriv")) return LOG_AUTHPRIV;
	if (strcaseeq(name, "cron")) return LOG_CRON;
	if (strcaseeq(name, "daemon")) return LOG_DAEMON;
	if (strcaseeq(name, "ftp")) return LOG_FTP;
	if (strcaseeq(name, "install")) return LOG_INSTALL;
	if (strcaseeq(name, "kern")) return LOG_KERN;
	if (strcaseeq(name, "lpr")) return LOG_LPR;
	if (strcaseeq(name, "mail")) return LOG_MAIL;
	if (strcaseeq(name, "netinfo")) return LOG_NETINFO;
	if (strcaseeq(name, "remoteauth")) return LOG_REMOTEAUTH;
	if (strcaseeq(name, "news")) return LOG_NEWS;
	if (strcaseeq(name, "security")) return LOG_AUTH;
	if (strcaseeq(name, "syslog")) return LOG_SYSLOG;
	if (strcaseeq(name, "user")) return LOG_USER;
	if (strcaseeq(name, "uucp")) return LOG_UUCP;
	if (strcaseeq(name, "local0")) return LOG_LOCAL0;
	if (strcaseeq(name, "local1")) return LOG_LOCAL1;
	if (strcaseeq(name, "local2")) return LOG_LOCAL2;
	if (strcaseeq(name, "local3")) return LOG_LOCAL3;
	if (strcaseeq(name, "local4")) return LOG_LOCAL4;
	if (strcaseeq(name, "local5")) return LOG_LOCAL5;
	if (strcaseeq(name, "local6")) return LOG_LOCAL6;
	if (strcaseeq(name, "local7")) return LOG_LOCAL7;
	if (strcaseeq(name, "launchd")) return LOG_LAUNCHD;

	return -1;
}

const char *
asl_syslog_faciliy_num_to_name(int n)
{
	if (n < 0) return NULL;

	if (n == LOG_AUTH) return "auth";
	if (n == LOG_AUTHPRIV) return "authpriv";
	if (n == LOG_CRON) return "cron";
	if (n == LOG_DAEMON) return "daemon";
	if (n == LOG_FTP) return "ftp";
	if (n == LOG_INSTALL) return "install";
	if (n == LOG_KERN) return "kern";
	if (n == LOG_LPR) return "lpr";
	if (n == LOG_MAIL) return "mail";
	if (n == LOG_NETINFO) return "netinfo";
	if (n == LOG_REMOTEAUTH) return "remoteauth";
	if (n == LOG_NEWS) return "news";
	if (n == LOG_AUTH) return "security";
	if (n == LOG_SYSLOG) return "syslog";
	if (n == LOG_USER) return "user";
	if (n == LOG_UUCP) return "uucp";
	if (n == LOG_LOCAL0) return "local0";
	if (n == LOG_LOCAL1) return "local1";
	if (n == LOG_LOCAL2) return "local2";
	if (n == LOG_LOCAL3) return "local3";
	if (n == LOG_LOCAL4) return "local4";
	if (n == LOG_LOCAL5) return "local5";
	if (n == LOG_LOCAL6) return "local6";
	if (n == LOG_LOCAL7) return "local7";
	if (n == LOG_LAUNCHD) return "launchd";

	return NULL;
}

/*
 * utility for converting a time string into a time_t
 * we only deal with the following formats:
 * Canonical form YYYY.MM.DD hh:mm:ss UTC
 * ctime() form Mth dd hh:mm:ss (e.g. Aug 25 09:54:37)
 * absolute form - # seconds since the epoch (e.g. 1095789191)
 * relative time - seconds before or after now (e.g. -300, +43200)
 * relative time - days/hours/minutes/seconds before or after now (e.g. -1d, +6h, +30m, -10s)
 */

#define CANONICAL_TIME_REX "^[0-9][0-9][0-9][0-9].[01]?[0-9].[0-3]?[0-9][ ]+[0-2]?[0-9]:[0-5][0-9]:[0-5][0-9][ ]+UTC$"
#define CTIME_REX "^[adfjmnos][aceopu][bcglnprtvy][ ]+[0-3]?[0-9][ ]+[0-2]?[0-9]:[0-5][0-9]:[0-5][0-9]$"
#define ABSOLUTE_TIME_REX "^[0-9]+[s]?$"
#define RELATIVE_TIME_REX "^[\\+-\\][0-9]+[smhdw]?$"

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400
#define SECONDS_PER_WEEK 604800

/*
 * We use the last letter in the month name to determine
 * the month number (0-11).  There are two collisions:
 * Jan and Jun both end in n
 * Mar and Apr both end in r
 * In these cases we check the second letter.
 *
 * The MTH_LAST array maps the last letter to a number.
 */
static const int8_t MTH_LAST[] = {-1, 1, 11, -1, -1, -1, 7, -1, -1, -1, -1, 6, -1, 5, -1, 8, -1, 3, -1, 9, -1, 10, -1, -1, 4, -1};

static int
_month_num(char *s)
{
	int i;
	int8_t v8;

	v8 = -1;
	if (s[2] > 90) v8 = s[2] - 'a';
	else v8 = s[2] - 'A';

	if ((v8 < 0) || (v8 > 25)) return -1;

	v8 = MTH_LAST[v8];
	if (v8 < 0) return -1;

	i = v8;
	if ((i == 5) && ((s[1] == 'a') || (s[1] == 'A'))) return 0;
	if ((i == 3) && ((s[1] == 'a') || (s[1] == 'A'))) return 2;
	return i;
}

time_t
asl_parse_time(const char *in)
{
	int len, y, status, rflags, factor;
	struct tm t;
	time_t tick, delta;
	char *str, *p, *x;
	static regex_t rex_canon, rex_ctime, rex_abs, rex_rel;
	static int init_canon = 0;
	static int init_ctime = 0;
	static int init_abs = 0;
	static int init_rel = 0;

	if (in == NULL) return -1;

	rflags = REG_EXTENDED | REG_NOSUB | REG_ICASE;

	if (init_canon == 0)
	{
		memset(&rex_canon, 0, sizeof(regex_t));
		status = regcomp(&rex_canon, CANONICAL_TIME_REX, rflags);
		if (status != 0) return -1;
		init_canon = 1;
	}

	if (init_ctime == 0)
	{
		memset(&rex_ctime, 0, sizeof(regex_t));
		status = regcomp(&rex_ctime, CTIME_REX, rflags);
		if (status != 0) return -1;
		init_ctime = 1;
	}
	
	if (init_abs == 0)
	{
		memset(&rex_abs, 0, sizeof(regex_t));
		status = regcomp(&rex_abs, ABSOLUTE_TIME_REX, rflags);
		if (status != 0) return -1;
		init_abs = 1;
	}
	
	if (init_rel == 0)
	{
		memset(&rex_rel, 0, sizeof(regex_t));
		status = regcomp(&rex_rel, RELATIVE_TIME_REX, rflags);
		if (status != 0) return -1;
		init_rel = 1;
	}

	len = strlen(in) + 1;

	if (regexec(&rex_abs, in, 0, NULL, 0) == 0)
	{
		/*
		 * Absolute time (number of seconds since the epoch)
		 */
		str = strdup(in);
		if ((str[len-2] == 's') || (str[len-2] == 'S')) str[len-2] = '\0';

		tick = atoi(str);
		free(str);

		return tick;
	}
	else if (regexec(&rex_rel, in, 0, NULL, 0) == 0)
	{
		/*
		 * Reletive time (number of seconds before or after right now)
		 */
		str = strdup(in);
	
		factor = 1;
	
		if ((str[len-2] == 's') || (str[len-2] == 'S'))
		{
			str[len-2] = '\0';
		}
		else if ((str[len-2] == 'm') || (str[len-2] == 'M'))
		{
			str[len-2] = '\0';
			factor = SECONDS_PER_MINUTE;
		}
		else if ((str[len-2] == 'h') || (str[len-2] == 'H'))
		{
			str[len-2] = '\0';
			factor = SECONDS_PER_HOUR;
		}
		else if ((str[len-2] == 'd') || (str[len-2] == 'D'))
		{
			str[len-2] = '\0';
			factor = SECONDS_PER_DAY;
		}
		else if ((str[len-2] == 'w') || (str[len-2] == 'W'))
		{
			str[len-2] = '\0';
			factor = SECONDS_PER_WEEK;
		}
		
		tick = time(NULL);
		delta = factor * atoi(str);
		tick += delta;

		free(str);

		return tick;
	}
	else if (regexec(&rex_canon, in, 0, NULL, 0) == 0)
	{
		memset(&t, 0, sizeof(struct tm));
		str = strdup(in);

		/* Get year */
		x = str;
		p = strchr(x, '.');
		*p = '\0';
		t.tm_year = atoi(x) - 1900;

		/* Get month */
		x = p + 1;
		p = strchr(x, '.');
		*p = '\0';
		t.tm_mon = atoi(x) - 1;

		/* Get day */
		x = p + 1;
		p = strchr(x, ' ');		
		*p = '\0';
		t.tm_mday = atoi(x);

		/* Get hour */
		for (x = p + 1; *x == ' '; x++);
		p = strchr(x, ':');		
		*p = '\0';
		t.tm_hour = atoi(x);

		/* Get minutes */
		x = p + 1;
		p = strchr(x, ':');		
		*p = '\0';
		t.tm_min = atoi(x);

		/* Get seconds */
		x = p + 1;
		p = strchr(x, ' ');		
		*p = '\0';
		t.tm_sec = atoi(x);

		free(str);
		return timegm(&t);
	}
	else if (regexec(&rex_ctime, in, 0, NULL, 0) == 0)
	{
		/* We assume it's in the current year */
		memset(&t, 0, sizeof(struct tm));
		tick = time(NULL);
		gmtime_r(&tick, &t);
		y = t.tm_year;

		memset(&t, 0, sizeof(struct tm));
		str = strdup(in);

		t.tm_year = y;
		t.tm_mon = _month_num(str);
		if (t.tm_mon < 0) return -1;

		for (x = strchr(str, ' '); *x == ' '; x++);
		p = strchr(x, ' ');
		*p = '\0';
		t.tm_mday = atoi(x);

		/* Get hour */
		for (x = p + 1; *x == ' '; x++);
		p = strchr(x, ':');		
		*p = '\0';
		t.tm_hour = atoi(x);
		
		/* Get minutes */
		x = p + 1;
		p = strchr(x, ':');		
		*p = '\0';
		t.tm_min = atoi(x);
		
		/* Get seconds */
		x = p + 1;
		t.tm_sec = atoi(x);
		
		t.tm_isdst = -1;

		free(str);
		return mktime(&t);
	}

	return -1;
}

#ifdef ASL_SYSLOG_COMPAT

__private_extern__ void
asl_syslog_syslog(int pri, const char *fmt, ...)
{
	va_list ap;
	asl_msg_t *m;
	
	if (fmt == NULL) return;
	
	m = asl_new(ASL_TYPE_MSG);
	
	va_start(ap, fmt);
	asl_vlog(NULL, m, pri, fmt, ap);
	va_end(ap);
	
	asl_free(m);
}

__private_extern__ void
asl_syslog_vsyslog(int pri, const char *fmt, va_list ap)
{
	asl_msg_t *m;
	
	m = asl_new(ASL_TYPE_MSG);
	asl_vlog(NULL, m, pri, fmt, ap);
	asl_free(m);
}

__private_extern__ void
asl_syslog_openlog(const char *ident, int flags, int facility)
{
	const char *fname;
	uint32_t opts;

	opts = 0;

	if (flags & LOG_NDELAY) opts |= ASL_OPT_NO_DELAY;
	if (flags & LOG_PERROR) opts |= ASL_OPT_STDERR;
	
	fname = asl_syslog_faciliy_num_to_name(facility);
	if (fname == NULL) fname = "user";

	asl_global_client = asl_open(ident, fname, opts);
}

__private_extern__ void
asl_syslog_closelog()
{
	asl_close();
}

__private_extern__ int
asl_syslog_setlogmask(int p)
{
	return asl_set_filter(p);
}

#endif ASL_SYSLOG_COMPAT

#endif /* BUILDING_VARIANT */
