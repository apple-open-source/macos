/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
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
#include <stdint.h>
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
#include <mach/std_types.h>
#include <mach/mig.h>
#include <mach/mach_types.h>
#include <sys/types.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <asl_ipc.h>

#define ASL_SERVICE_NAME "com.apple.system.logger"

#define streq(A, B) (strcmp(A, B) == 0)
#define strcaseeq(A, B) (strcasecmp(A, B) == 0)

#define forever for(;;)

#define TOKEN_NULL  0
#define TOKEN_OPEN  1
#define TOKEN_CLOSE 2
#define TOKEN_WORD  3
#define TOKEN_INT   4

#define MFMT_RAW 0
#define MFMT_STD 1
#define MFMT_BSD 2
#define MFMT_XML 3
#define MFMT_STR 4
#define MFMT_MSG 5

#define TFMT_SEC 0
#define TFMT_UTC 1
#define TFMT_LCL 2

#define ENCODE_NONE 0
#define ENCODE_VIS 1
#define ENCODE_ASL 2

#define XML_TAG_KEY 0
#define XML_TAG_STRING 1
#define XML_TAG_DATA 2

#define FETCH_BATCH	256

/* forward */
time_t asl_parse_time(const char *);
const char *asl_syslog_faciliy_num_to_name(int n);
__private_extern__ asl_client_t *_asl_open_default();

/* notify SPI */
uint32_t notify_register_plain(const char *name, int *out_token);

/* from asl_util.c */
int _asl_server_socket(int *sock, struct sockaddr_un *server);
int asl_is_utf8(const char *str);
uint8_t *asl_b64_encode(const uint8_t *buf, size_t len);

/* character encoding lengths */
static const uint8_t char_encode_len[128] =
{
	2, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3
};

static const char *cvis_7_13 = "abtnvfr";

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

static mach_port_t asl_server_port = MACH_PORT_NULL;

static int
_asl_connect(asl_client_t *asl)
{
	if (asl->sock >= 0) return 0;

	return _asl_server_socket(&asl->sock, &asl->server);
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
		if (asl->name == NULL)
		{
			close(asl->sock);
			free(asl);
			return NULL;
		}
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
			if (asl->name == NULL)
			{
				close(asl->sock);
				free(asl);
				return NULL;
			}
		}
	}

	asl->facility = NULL;
	if (facility != NULL) asl->facility = strdup(facility);
	else asl->facility = strdup(asl_syslog_faciliy_num_to_name(LOG_USER));
	if (asl->facility == NULL)
	{
		close(asl->sock);
		free(asl);
		return NULL;
	}

	if (!(asl->options & ASL_OPT_NO_REMOTE)) _asl_notify_open(1);

	if (asl->options & ASL_OPT_STDERR) asl_add_output((aslclient)asl, fileno(stderr), ASL_MSG_FMT_STD, ASL_TIME_FMT_LCL);

	return (aslclient)asl;
}

void
asl_close(aslclient ac)
{
	asl_client_t *asl;
	uint32_t i;

	asl = (asl_client_t *)ac;
	if (asl == NULL) return;

	if (asl->sock >= 0) close(asl->sock);
	if (asl->name != NULL) free(asl->name);
	if (asl->facility != NULL) free(asl->facility);
	if (!(asl->options & ASL_OPT_NO_REMOTE)) _asl_notify_close();
	if (asl->fd_list != NULL) free(asl->fd_list);

	if (asl->fd_mfmt != NULL)
	{
		for (i = 0; i < asl->fd_count; i++) if (asl->fd_mfmt[i] != NULL) free(asl->fd_mfmt[i]);
		free(asl->fd_mfmt);
	}

	if (asl->fd_tfmt != NULL)
	{
		for (i = 0; i < asl->fd_count; i++) if (asl->fd_tfmt[i] != NULL) free(asl->fd_tfmt[i]);
		free(asl->fd_tfmt);
	}

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
_asl_encode_char(char **m, uint32_t *x, uint32_t c, uint32_t encode, uint32_t encode_space)
{
	char *p;
	int meta;

	meta = 0;

	p = *m + *x - 1;

	/* NUL is not allowed */
	if (c == 0) return;

	/* Meta chars get \M prefix */
	if (c >= 128)
	{
		/* except meta-space, which is \240 */
		if (c == 160)
		{
			*p++ = '\\';
			*p++ = '2';
			*p++ = '4';
			*p++ = '0';
			*p = '\0';
			*x = *x + 4;
			return;
		}

		*p++ = '\\';
		*p++ = 'M';
		*p = '\0';
		*x = *x + 2;
		c &= 0x7f;
		meta = 1;
	}

	/* space is either ' ' or \s */
	if (c == 32)
	{
		if (encode_space == 0)
		{
			*p++ = ' ';
			*p = '\0';
			*x = *x + 1;
			return;
		}

		*p++ = '\\';
		*p++ = 's';
		*p = '\0';
		*x = *x + 2;
		return;
	}

	/* \ is escaped */
	if ((meta == 0) && (c == 92))
	{
		*p++ = '\\';
		*p++ = c;
		*p = '\0';
		*x = *x + 2;
		return;
	}

	/* [ and ] ane escaped in ASL encoding */
	if ((encode == ENCODE_ASL) && (meta == 0) && ((c == 91) || (c == 93)))
	{
		*p++ = '\\';
		*p++ = c;
		*p = '\0';
		*x = *x + 2;
		return;
	}

	/* DEL is \^? */
	if (c == 127)
	{
		if (meta == 0)
		{
			*p++ = '\\';
			*x = *x + 1;
		}

		*p++ = '^';
		*p++ = '?';
		*p = '\0';
		*x = *x + 2;
		return;
	}

	/* 33-126 are printable (add a '-' prefix for meta) */
	if ((c >= 33) && (c <= 126))
	{
		if (meta == 1)
		{
			*p++ = '-';
			*x = *x + 1;
		}

		*p++ = c;
		*p = '\0';
		*x = *x + 1;
		return;
	}

	/* non-meta BEL, BS, HT, NL, VT, NP, CR (7-13) are \a, \b, \t, \n, \v, \f, and \r */
	if ((meta == 0) && (c >= 7) && (c <= 13))
	{
		*p++ = '\\';
		*p++ = cvis_7_13[c - 7];
		*p = '\0';
		*x = *x + 2;
		return;
	}

	/* 0 - 31 are ^@ - ^_ (non-meta get a leading \) */
	if ((c >= 0) && (c <= 31))
	{
		if (meta == 0)
		{
			*p++ = '\\';
			*x = *x + 1;
		}

		*p++ = '^';
		*p++ = 64 + c;
		*p = '\0';
		*x = *x + 2;
		return;
	}

	return;
}

static void
_asl_append_string(char **m, uint32_t *x, const char *s, uint32_t encode, uint32_t escspace)
{
	uint32_t i, n, spextra;
	uint8_t c;

	if (m == NULL) return;
	if (x == NULL) return;
	if (s == NULL) return;

	if (encode == ENCODE_NONE)
	{
		n = strlen(s);
		if (n == 0) return;

		if (*m == NULL)
		{
			*m = malloc(n + 1);
			*x = 1;
		}
		else
		{
			*m = reallocf(*m, n + (*x));
		}

		if (*m == NULL) return;

		memcpy((*m) + (*x) - 1, s, n + 1);
		*x += n;

		return;
	}

	spextra = 0;

	if (escspace != 0) spextra = 1;

	n = 0;
	for (i = 0; s[i] != '\0'; i++)
	{
		c = s[i];

		if (c >= 128)
		{
			n += 4;
		}
		else if ((c == 91) || (c == 93))
		{
			if (encode == ENCODE_ASL) n += 2;
			else n += 1;
		}
		else
		{
			n += char_encode_len[c];
			if (c == 32) n += spextra;
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
		*m = reallocf(*m, n + (*x));
	}

	if (*m == NULL) return;

	for (i = 0; s[i] != '\0'; i++)
	{
		c = s[i];
		_asl_encode_char(m, x, c, encode, escspace);
	}

	return;
}

static void
_asl_append_xml_string(char **m, uint32_t *x, char *s)
{
	uint32_t i, n;
	uint8_t c;
	char tmp[8], *p;

	if (m == NULL) return;
	if (x == NULL) return;
	if (s == NULL) return;

	n = 0;
	for (i = 0; s[i] != '\0'; i++)
	{
		c = s[i];

		/*
		 * XML wants &amp; &lt; &gt; &quot; and &apos;
		 * We use &#xnn; for control chars.
		 * Everything else just gets printed "as is" (we know the input is UTF8)
		 */
		if (c == '&') n += 5;
		else if (c == '<') n += 4;
		else if (c == '>') n += 4;
		else if (c == '"') n += 6;
		else if (c == '\'') n += 6;
		else if (iscntrl(c)) n += 6;
		else n += 1;
	}

	if (n == 0) return;

	if (*m == NULL)
	{
		*m = malloc(n + 1);
		*x = 1;
	}
	else
	{
		*m = reallocf(*m, n + (*x));
	}

	if (*m == NULL) return;

	for (i = 0; s[i] != '\0'; i++)
	{
		c = s[i];

		if (c == '&')
		{
			p = *m + *x - 1;
			memcpy(p, "&amp;", 5);
			p += 5;
			*p = '\0';
			*x = *x + 5;
		}
		else if (c == '<')
		{
			p = *m + *x - 1;
			memcpy(p, "&lt;", 4);
			p += 4;
			*p = '\0';
			*x = *x + 4;
		}
		else if (c == '>')
		{
			p = *m + *x - 1;
			memcpy(p, "&gt;", 4);
			p += 4;
			*p = '\0';
			*x = *x + 4;
		}
		else if (c == '"')
		{
			p = *m + *x - 1;
			memcpy(p, "&quot;", 6);
			p += 6;
			*p = '\0';
			*x = *x + 6;
		}
		else if (c == '\'')
		{
			p = *m + *x - 1;
			memcpy(p, "&apos;", 6);
			p += 6;
			*p = '\0';
			*x = *x + 6;
		}
		else if (iscntrl(c))
		{
			snprintf(tmp, sizeof(tmp), "&#x%02hhu;", c);
			p = *m + *x - 1;
			memcpy(p, tmp, 6);
			p += 6;
			*p = '\0';
			*x = *x + 6;
		}
		else
		{
			p = *m + *x - 1;
			*p++ = c;
			*p = '\0';
			*x = *x + 1;
		}
	}

	return;
}

static void
_asl_append_xml_tag(char **m, uint32_t *x, int tag, char *s)
{
	char *b64;

	if (m == NULL) return;
	if (x == NULL) return;

	if (tag == XML_TAG_KEY)
	{
		_asl_append_string(m, x, "\t\t<key>", ENCODE_NONE, 0);
		_asl_append_xml_string(m, x, s);
		_asl_append_string(m, x, "</key>\n", ENCODE_NONE, 0);
		return;
	}

	if (tag == XML_TAG_STRING)
	{
		_asl_append_string(m, x, "\t\t<string>", ENCODE_NONE, 0);
		_asl_append_xml_string(m, x, s);
		_asl_append_string(m, x, "</string>\n", ENCODE_NONE, 0);
		return;
	}

	if (tag == XML_TAG_DATA)
	{
		_asl_append_string(m, x, "\t\t<data>", ENCODE_NONE, 0);
		b64 = (char *)asl_b64_encode((uint8_t *)s, strlen(s));
		if (b64 != NULL)
		{
			_asl_append_string(m, x, b64, ENCODE_NONE, 0);
			free(b64);
		}
		_asl_append_string(m, x, "</data>\n", ENCODE_NONE, 0);
		return;
	}
}

static void
_asl_append_op(char **m, uint32_t *x, uint32_t op)
{
	char opstr[8];
	uint32_t i;

	if (m == NULL) return;
	if (x == NULL) return;

	if (op == ASL_QUERY_OP_NULL) return _asl_append_string(m, x, ".", ENCODE_NONE, 0);

	i = 0;
	if (op & ASL_QUERY_OP_CASEFOLD) opstr[i++] = 'C';

	if (op & ASL_QUERY_OP_REGEX) opstr[i++] = 'R';

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

	if (i == 0) return _asl_append_string(m, x, ".", ENCODE_NONE, 0);

	opstr[i++] = '\0';
	return _asl_append_string(m, x, opstr, ENCODE_NONE, 0);
}

static char *
_asl_time_string(int fmt, const char *str)
{
	time_t tick;
	struct tm *stm;
	char *ltime;
	char *out;
	char ltbuf[32];
	out = NULL;

	tick = 0;
	if (str != NULL) tick = asl_parse_time(str);

	if (fmt == TFMT_SEC)
	{
		asprintf(&out, "%lu", tick);
		return out;
	}

	if (fmt == TFMT_UTC)
	{
		stm = gmtime(&tick);
		asprintf(&out, "%d.%02d.%02d %02d:%02d:%02d UTC", stm->tm_year + 1900, stm->tm_mon + 1, stm->tm_mday, stm->tm_hour, stm->tm_min, stm->tm_sec);
		return out;
	}

	if (fmt == TFMT_LCL)
	{
		ltime = ctime_r(&tick, ltbuf);
		if (ltime == NULL) return NULL;
		ltime[19] = '\0';
		asprintf(&out, "%s", ltime);
		return out;
	}

	return NULL;
}

static char *
_asl_msg_to_string_time_fmt(asl_msg_t *msg, uint32_t *len, int tf)
{
	uint32_t i, outlen;
	char *out, *s;

	*len = 0;

	if (msg == NULL) return NULL;

	s = NULL;
	out = NULL;
	outlen = 0;

	if (msg->count == 0)
	{
		if (out == NULL) return NULL;
		*len = outlen;
		return out;
	}

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] == NULL) continue;
		if (i > 0) _asl_append_string(&out, &outlen, " [", ENCODE_NONE, 0);
		else _asl_append_string(&out, &outlen, "[", ENCODE_NONE, 0);

		_asl_append_string(&out, &outlen, msg->key[i], ENCODE_ASL, 1);

		if ((tf != TFMT_SEC) && (!strcmp(msg->key[i], ASL_KEY_TIME)))
		{
			s = _asl_time_string(tf, msg->val[i]);
			if (s != NULL)
			{
				_asl_append_string(&out, &outlen, " ", ENCODE_NONE, 0);
				_asl_append_string(&out, &outlen, s, ENCODE_ASL, 0);
			}
		}
		else if (msg->val[i] != NULL)
		{
			_asl_append_string(&out, &outlen, " ", ENCODE_NONE, 0);
			_asl_append_string(&out, &outlen, msg->val[i], ENCODE_ASL, 0);
		}

		_asl_append_string(&out, &outlen, "]", ENCODE_NONE, 0);
	}

	_asl_append_string(&out, &outlen, "\n", ENCODE_NONE, 0);

	*len = outlen;
	return out;
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
		_asl_append_string(&out, &outlen, "Q ", ENCODE_NONE, 0);
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

		if (i > 0) _asl_append_string(&out, &outlen, " [", ENCODE_NONE, 0);
		else _asl_append_string(&out, &outlen, "[", ENCODE_NONE, 0);

		if (msg->type == ASL_TYPE_QUERY)
		{
			_asl_append_op(&out, &outlen, msg->op[i]);
			_asl_append_string(&out, &outlen, " ", ENCODE_NONE, 0);
		}

		_asl_append_string(&out, &outlen, msg->key[i], ENCODE_ASL, 1);

		if (msg->val[i] != NULL)
		{
			_asl_append_string(&out, &outlen, " ", ENCODE_NONE, 0);
			_asl_append_string(&out, &outlen, msg->val[i], ENCODE_ASL, 0);
		}

		_asl_append_string(&out, &outlen, "]", ENCODE_NONE, 0);
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
		if (o[i] == 'R') op |= ASL_QUERY_OP_REGEX;
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
	char *str, *out, c, oval;
	uint32_t i, len, n, outlen;

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

	str = *p;

	/* opening [ */
	if (**p == '[')
	{
		*tt = TOKEN_OPEN;

		(*p)++;
		out = malloc(2);
		if (out == NULL) return NULL;

		out[0] = '[';
		out[1] = '\0';
		return out;
	}

	/* scan for token and calulate it's length (input and decoded output len) */
	len = 0;
	outlen = 0;

	forever
	{
		c = str[len];

		/* stop scanning when we hit a delimiter */
		if (((spacedel != 0) && (c == ' ')) || (c == ']') || (c == '\0')) break;

		if (c == '\\')
		{
			len++;
			c = str[len];
			if ((c == 'a') || (c == 'b') || (c == 't') || (c == 'n') || (c == 'v') || (c == 'f') || (c == 'r') || (c == 's') || (c == '[') || (c == '\\') || (c == ']'))
			{
			}
			else if (c == '^')
			{
				if (str[++len] == '\0') return NULL;
			}
			else if (c == 'M')
			{
				if (str[++len] == '\0') return NULL;
				if (str[++len] == '\0') return NULL;
			}
			else if ((c >= '0') && (c <= '3'))
			{
				if (str[++len] == '\0') return NULL;
				if (str[++len] == '\0') return NULL;
			}
			else
			{
				return NULL;
			}
		}

		len++;
		outlen++;
	}

	(*p) += len;

	if ((len == 0) && (**p == ']'))
	{
		*tt = TOKEN_CLOSE;
		(*p)++;
		out = malloc(2);
		if (out == NULL) return NULL;

		out[0] = ']';
		out[1] = '\0';
		return out;
	}

	*tt = TOKEN_INT;

	out = malloc(outlen + 1);
	if (out == NULL) return NULL;

	n = 0;
	for (i = 0; i < len; i++)
	{
		c = str[i];

		if (c == '\\')
		{
			*tt = TOKEN_WORD;

			i++;
			c = str[i];
			if (c == 'a')
			{
				out[n++] = '\a';
			}
			else if (c == 'b')
			{
				out[n++] = '\b';
			}
			else if (c == 't')
			{
				out[n++] = '\t';
			}
			else if (c == 'n')
			{
				out[n++] = '\n';
			}
			else if (c == 'v')
			{
				out[n++] = '\v';
			}
			else if (c == 'f')
			{
				out[n++] = '\f';
			}
			else if (c == 'r')
			{
				out[n++] = '\r';
			}
			else if (c == 's')
			{
				out[n++] = ' ';
			}
			else if (c == '[')
			{
				out[n++] = '[';
			}
			else if (c == '\\')
			{
				out[n++] = '\\';
			}
			else if (c == ']')
			{
				out[n++] = ']';
			}
			else if (c == '^')
			{
				i++;
				if (str[i] == '?') out[n++] = 127;
				else out[n++] = str[i] - 64;
			}
			else if (c == 'M')
			{
				i++;
				c = str[i];
				if (c == '^')
				{
					i++;
					if (str[i] == '?') out[n++] = 255;
					else out[n++] = str[i] + 64;
				}
				else if (c == '-')
				{
					i++;
					out[n++] = str[i] + 128;
				}
				else
				{
					*tt = TOKEN_NULL;
					free(out);
					return NULL;
				}

			}
			else if ((c >= '0') && (c <= '3'))
			{
				oval = (c - '0') * 64;

				i++;
				c = str[i];
				if ((c < '0') || (c > '7'))
				{
					*tt = TOKEN_NULL;
					free(out);
					return NULL;
				}

				oval += ((c - '0') * 8);

				i++;
				c = str[i];
				if ((c < '0') || (c > '7'))
				{
					*tt = TOKEN_NULL;
					free(out);
					return NULL;
				}

				oval += (c - '0');

				out[n++] = oval;
			}
			else
			{
				*tt = TOKEN_NULL;
				free(out);
				return NULL;
			}
		}
		else
		{

			if ((c < '0') || (c > '9')) *tt = TOKEN_WORD;
			out[n++] = c;
		}
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

char *
asl_list_to_string(asl_search_result_t *list, uint32_t *outlen)
{
	uint32_t i, len, newlen;
	char *msgbuf, *out;

	if (list == NULL) return NULL;
	if (list->count == 0) return NULL;
	if (list->msg == NULL) return NULL;

	out = NULL;
	asprintf(&out, "%u\n", list->count);
	if (out == NULL) return NULL;
	*outlen = strlen(out) + 1;

	for (i = 0; i < list->count; i++)
	{
		len = 0;
		msgbuf = asl_msg_to_string(list->msg[i], &len);
		if (msgbuf == NULL)
		{
			free(out);
			*outlen = 0;
			return NULL;
		}

		newlen = *outlen + len;
		out = reallocf(out, newlen);
		if (out == NULL)
		{
			*outlen = 0;
			return NULL;
		}

		memmove((out + *outlen - 1), msgbuf, len);
		out[newlen - 2] = '\n';
		out[newlen - 1] = '\0';
		*outlen = newlen;

		free(msgbuf);
	}

	return out;
}

asl_search_result_t *
asl_list_from_string(const char *buf)
{
	uint32_t i, n;
	const char *p;
	asl_search_result_t *out;
	asl_msg_t *m;

	if (buf == NULL) return NULL;
	p = buf;

	n = atoi(buf);
	if (n == 0) return NULL;

	out = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));
	if (out == NULL) return NULL;

	out->msg = (asl_msg_t **)calloc(n, sizeof(asl_msg_t *));
	if (out->msg == NULL)
	{
		free(out);
		return NULL;
	}

	for (i = 0; i < n; i++)
	{
		p = strchr(p, '\n');
		if (p == NULL)
		{
			aslresponse_free((aslresponse)out);
			return NULL;
		}

		p++;

		m = asl_msg_from_string(p);
		if (m == NULL)
		{
			aslresponse_free((aslresponse)out);
			return NULL;
		}

		out->msg[i] = m;
		out->count += 1;
	}

	return out;
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
		cmp = regexec(&rex, m, 0, NULL, 0);
		regfree(&rex);
		return (cmp == 0);
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
	for (i = 0; i <= d; i++)
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
_asl_msg_test_time_op(uint32_t op, char *q, char *m)
{
	time_t tq, tm;
	char *vq, *vm;
	struct tm gtime;
	uint32_t t, do_numeric;
	int cmp;

	do_numeric = 1;

	if ((op & ASL_QUERY_OP_PREFIX) || (op & ASL_QUERY_OP_SUFFIX) || (op & ASL_QUERY_OP_REGEX) || (op & ASL_QUERY_OP_CASEFOLD)) do_numeric = 0;

	tq = asl_parse_time(q);
	if (tq < 0) return _asl_msg_test_op(op, q, m);

	tm = asl_parse_time(m);
	if (tm < 0) return _asl_msg_test_op(op, q, m);

	if (do_numeric == 1)
	{
		t = op & ASL_QUERY_OP_TRUE;
		switch (t)
		{
			case ASL_QUERY_OP_EQUAL:
				if (tm == tq) return 1;
				return 0;
			case ASL_QUERY_OP_GREATER:
				if (tm > tq) return 1;
				return 0;
			case ASL_QUERY_OP_GREATER_EQUAL:
				if (tm >= tq) return 1;
				return 0;
			case ASL_QUERY_OP_LESS:
				if (tm < tq) return 1;
				return 0;
			case ASL_QUERY_OP_LESS_EQUAL:
				if (tm <= tq) return 1;
				return 0;
			case ASL_QUERY_OP_NOT_EQUAL:
				if (tm != tq) return 1;
				return 0;
			default:
				return 0;
		}

		return 0;
	}

	memset(&gtime, 0, sizeof(struct tm));
	gmtime_r(&tq, &gtime);

	/* Canonical form: YYYY.MM.DD hh:mm:ss UTC */
	vq = NULL;
	asprintf(&vq, "%d.%02d.%02d %02d:%02d:%02d UTC", gtime.tm_year + 1900, gtime.tm_mon + 1, gtime.tm_mday, gtime.tm_hour, gtime.tm_min, gtime.tm_sec);
	if (vq == NULL) return 0;

	memset(&gtime, 0, sizeof(struct tm));
	gmtime_r(&tm, &gtime);

	/* Canonical form: YYYY.MM.DD hh:mm:ss UTC */
	vm = NULL;
	asprintf(&vm, "%d.%02d.%02d %02d:%02d:%02d UTC", gtime.tm_year + 1900, gtime.tm_mon + 1, gtime.tm_mday, gtime.tm_hour, gtime.tm_min, gtime.tm_sec);
	if (vm == NULL) return 0;

	cmp = _asl_msg_test_op(op, q, m);

	free(vq);
	free(vm);

	return cmp;
}

static int
_asl_msg_test(asl_msg_t *q, asl_msg_t *m)
{
	uint32_t i, j;
	int cmp;
	char *val;

	for (i = 0; i < q->count; i++)
	{
		j = _asl_msg_index(m, q->key[i]);
		if (j == (uint32_t)-1) return 0;

		if (q->val[i] == NULL) continue;
		if (q->op == NULL) continue;

		if ((q->op[i] & ASL_QUERY_OP_TRUE) == ASL_QUERY_OP_TRUE) continue;

		if (m->val[j] == NULL) return 0;

		val = q->val[i];

		cmp = 1;
		if (streq(q->key[i], ASL_KEY_TIME))
		{
			cmp = _asl_msg_test_time_op(q->op[i], q->val[i], m->val[j]);
		}
		else
		{
			cmp = _asl_msg_test_op(q->op[i], val, m->val[j]);
		}

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

/*
 * asl_add_file: write log messages to the given file descriptor
 * Log messages will be written to this file as well as to the server.
 */
int
asl_add_output(aslclient ac, int fd, const char *mfmt, const char *tfmt)
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
			/* update message format and time format */
			if (asl->fd_mfmt[i] != NULL) free(asl->fd_mfmt[i]);
			asl->fd_mfmt[i] = NULL;
			if (mfmt != NULL) asl->fd_mfmt[i] = strdup(mfmt);

			if (asl->fd_tfmt[i] != NULL) free(asl->fd_tfmt[i]);
			asl->fd_tfmt[i] = NULL;
			if (tfmt != NULL) asl->fd_tfmt[i] = strdup(tfmt);

			if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
			return 0;
		}
	}

	if (asl->fd_count == 0)
	{
		asl->fd_list = (int *)calloc(1, sizeof(int));
		asl->fd_mfmt = (char **)calloc(1, sizeof(char *));
		asl->fd_tfmt = (char **)calloc(1, sizeof(char *));
	}
	else
	{
		asl->fd_list = (int *)reallocf(asl->fd_list, (1 + asl->fd_count) * sizeof(int));
		asl->fd_mfmt = (char **)reallocf(asl->fd_mfmt, (1 + asl->fd_count) * sizeof(char *));
		asl->fd_tfmt = (char **)reallocf(asl->fd_tfmt, (1 + asl->fd_count) * sizeof(char *));
	}

	if ((asl->fd_list == NULL) || (asl->fd_mfmt == NULL) || (asl->fd_tfmt == NULL))
	{
		if (asl->fd_list != NULL) free(asl->fd_list);
		if (asl->fd_mfmt != NULL) free(asl->fd_mfmt);
		if (asl->fd_tfmt != NULL) free(asl->fd_tfmt);

		if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
		return -1;
	}

	asl->fd_list[asl->fd_count] = fd;
	if (mfmt != NULL) asl->fd_mfmt[asl->fd_count] = strdup(mfmt);
	if (tfmt != NULL) asl->fd_tfmt[asl->fd_count] = strdup(tfmt);

	asl->fd_count++;

	if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
	return 0;
}

int
asl_add_log_file(aslclient ac, int fd)
{
	return asl_add_output(ac, fd, ASL_MSG_FMT_STD, ASL_TIME_FMT_LCL);
}

/*
 * asl_remove_output: stop writing log messages to the given file descriptor
 */
int
asl_remove_output(aslclient ac, int fd)
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

	if (asl->fd_mfmt[x] != NULL) free(asl->fd_mfmt[x]);
	if (asl->fd_tfmt[x] != NULL) free(asl->fd_tfmt[x]);

	for (i = x + 1; i < asl->fd_count; i++, x++)
	{
		asl->fd_list[x] = asl->fd_list[i];
		asl->fd_mfmt[x] = asl->fd_mfmt[i];
		asl->fd_tfmt[x] = asl->fd_tfmt[i];
	}

	asl->fd_count--;

	if (asl->fd_count == 0)
	{
		free(asl->fd_list);
		asl->fd_list = NULL;
		asl->fd_mfmt = NULL;
		asl->fd_tfmt = NULL;
	}
	else
	{
		asl->fd_list = (int *)reallocf(asl->fd_list, asl->fd_count * sizeof(int));
		asl->fd_mfmt = (char **)reallocf(asl->fd_mfmt, asl->fd_count * sizeof(char *));
		asl->fd_tfmt = (char **)reallocf(asl->fd_tfmt, asl->fd_count * sizeof(char *));

		if ((asl->fd_list == NULL) || (asl->fd_mfmt == NULL) || (asl->fd_tfmt == NULL))
		{
			if (asl->fd_list != NULL)
			{
				free(asl->fd_list);
				asl->fd_list = NULL;
			}

			if (asl->fd_mfmt != NULL)
			{
				for (i = 0; i < asl->fd_count; i++) if (asl->fd_mfmt[i] != NULL) free(asl->fd_mfmt[i]);
				free(asl->fd_mfmt);
				asl->fd_mfmt = NULL;
			}

			if (asl->fd_tfmt != NULL)
			{
				for (i = 0; i < asl->fd_count; i++) if (asl->fd_tfmt[i] != NULL) free(asl->fd_tfmt[i]);
				free(asl->fd_tfmt);
				asl->fd_tfmt = NULL;
			}

			asl->fd_count = 0;
			if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
			return -1;
		}
	}

	if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);
	return 0;
}

int
asl_remove_log_file(aslclient ac, int fd)
{
	return asl_remove_output(ac, fd);
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
	if (estr == NULL) 
	{
		if ((msg != NULL) && (my_msg != 0)) asl_free(msg);
		return -1;
	}

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
		if (fmt == NULL)
		{
			if (estr != NULL) free(estr);
			return -1;
		}

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

static const char *
_asl_level_string(int level)
{
	if (level == ASL_LEVEL_EMERG) return ASL_STRING_EMERG;
	if (level == ASL_LEVEL_ALERT) return ASL_STRING_ALERT;
	if (level == ASL_LEVEL_CRIT) return ASL_STRING_CRIT;
	if (level == ASL_LEVEL_ERR) return ASL_STRING_ERR;
	if (level == ASL_LEVEL_WARNING) return ASL_STRING_WARNING;
	if (level == ASL_LEVEL_NOTICE) return ASL_STRING_NOTICE;
	if (level == ASL_LEVEL_INFO) return ASL_STRING_INFO;
	if (level == ASL_LEVEL_DEBUG) return ASL_STRING_DEBUG;
	return "Unknown";
}

/*
 * format a message for printing
 * out parameter len returns string length including trailing NUL
 */
char *
asl_format_message(aslmsg msg, const char *mfmt, const char *tfmt, uint32_t *len)
{
	char *out, *tstr, *k, c[2];
	const char *hstr, *sstr, *pstr, *mstr, *lstr, *rprc, *rpid, *v;
	int i, j, l, mf, tf, paren, oval, level;

	out = NULL;
	*len = 0;

	if (msg == NULL) return NULL;

	mf = MFMT_RAW;
	tf = TFMT_SEC;

	if (mfmt == NULL) mf = MFMT_RAW;
	else if (!strcmp(mfmt, ASL_MSG_FMT_RAW)) mf = MFMT_RAW;
	else if (!strcmp(mfmt, ASL_MSG_FMT_STD)) mf = MFMT_STD;
	else if (!strcmp(mfmt, ASL_MSG_FMT_BSD)) mf = MFMT_BSD;
	else if (!strcmp(mfmt, ASL_MSG_FMT_XML)) mf = MFMT_XML;
	else if (!strcmp(mfmt, ASL_MSG_FMT_MSG)) mf = MFMT_MSG;
	else mf = MFMT_STR;

	if (tfmt == NULL) tf = TFMT_SEC;
	else if (!strcmp(tfmt, ASL_TIME_FMT_SEC)) tf = TFMT_SEC;
	else if (!strcmp(tfmt, ASL_TIME_FMT_UTC)) tf = TFMT_UTC;
	else if (!strcmp(tfmt, ASL_TIME_FMT_LCL)) tf = TFMT_LCL;

	if (mf == MFMT_RAW)
	{
		out = _asl_msg_to_string_time_fmt((asl_msg_t *)msg, len, tf);
		return out;
	}

	if (mf == MFMT_MSG)
	{
		mstr = asl_get(msg, ASL_KEY_MSG);
		if (mstr == NULL) return NULL;

		_asl_append_string(&out, len, mstr, ENCODE_VIS, 0);
		_asl_append_string(&out, len, "\n", ENCODE_NONE, 0);

		return out;
	}

	if ((mf == MFMT_STD) || (mf == MFMT_BSD))
	{
		/* BSD:  Mth dd hh:mm:ss host sender[pid]: message */
		/* BSD:  Mth dd hh:mm:ss host sender[pid] (refproc[refpid]): message */
		/* STD:  Mth dd hh:mm:ss host sender[pid] <Level>: message */
		/* STD:  Mth dd hh:mm:ss host sender[pid] (refproc[refpid]) <Level>: message */

		v = asl_get(msg, ASL_KEY_TIME);
		tstr = _asl_time_string(tf, v);

		hstr = asl_get(msg, ASL_KEY_HOST);
		sstr = asl_get(msg, ASL_KEY_SENDER);
		pstr = asl_get(msg, ASL_KEY_PID);
		mstr = asl_get(msg, ASL_KEY_MSG);

		rprc = asl_get(msg, ASL_KEY_REF_PROC);
		rpid = asl_get(msg, ASL_KEY_REF_PID);

		level = -1;

		if (mf == MFMT_STD)
		{
			lstr = asl_get(msg, ASL_KEY_LEVEL);
			if (lstr != NULL) level = atoi(lstr);
		}

		if (tstr == NULL)
		{
			_asl_append_string(&out, len, "0", ENCODE_NONE, 0);
		}
		else
		{
			_asl_append_string(&out, len, tstr, ENCODE_NONE, 0);
			free(tstr);
		}

		_asl_append_string(&out, len, " ", ENCODE_NONE, 0);

		if (hstr == NULL) _asl_append_string(&out, len, "unknown", ENCODE_NONE, 0);
		else _asl_append_string(&out, len, hstr, ENCODE_VIS, 0);

		_asl_append_string(&out, len, " ", ENCODE_NONE, 0);

		if (sstr == NULL) _asl_append_string(&out, len, "unknown", ENCODE_NONE, 0);
		else _asl_append_string(&out, len, sstr, ENCODE_VIS, 0);

		if ((pstr != NULL) && (strcmp(pstr, "-1")))
		{
			_asl_append_string(&out, len, "[", ENCODE_NONE, 0);
			_asl_append_string(&out, len, pstr, ENCODE_NONE, 0);
			_asl_append_string(&out, len, "]", ENCODE_NONE, 0);
		}

		if ((rprc != NULL) || (rpid != NULL)) _asl_append_string(&out, len, " (", ENCODE_NONE, 0);

		if (rprc != NULL) _asl_append_string(&out, len, rprc, ENCODE_VIS, 0);
		if (rpid != NULL)
		{
			_asl_append_string(&out, len, "[", ENCODE_NONE, 0);
			_asl_append_string(&out, len, rpid, ENCODE_NONE, 0);
			_asl_append_string(&out, len, "]", ENCODE_NONE, 0);
		}

		if ((rprc != NULL) || (rpid != NULL)) _asl_append_string(&out, len, ")", ENCODE_NONE, 0);

		if (mf == MFMT_STD)
		{
			_asl_append_string(&out, len, " <", ENCODE_NONE, 0);
			_asl_append_string(&out, len, _asl_level_string(level), ENCODE_NONE, 0);
			_asl_append_string(&out, len, ">", ENCODE_NONE, 0);
		}

		_asl_append_string(&out, len, ": ", ENCODE_NONE, 0);

		if (mstr != NULL) _asl_append_string(&out, len, mstr, ENCODE_VIS, 0);

		_asl_append_string(&out, len, "\n", ENCODE_NONE, 0);
		return out;
	}

	if (mf == MFMT_XML)
	{
		_asl_append_string(&out, len, "\t<dict>\n", ENCODE_NONE, 0);

		for (i = 0; i < msg->count; i++)
		{
			if (asl_is_utf8(msg->key[i]) == 1)
			{
				_asl_append_xml_tag(&out, len, XML_TAG_KEY, msg->key[i]);
				if (!strcmp(msg->key[i], ASL_KEY_TIME))
				{
					tstr = _asl_time_string(tf, msg->val[i]);
					_asl_append_xml_tag(&out, len, XML_TAG_STRING, tstr);
					if (tstr != NULL) free(tstr);
				}
				else 
				{
					if (asl_is_utf8(msg->val[i]) == 1) _asl_append_xml_tag(&out, len, XML_TAG_STRING, msg->val[i]);
					else _asl_append_xml_tag(&out, len, XML_TAG_DATA, msg->val[i]);
				}
			}
		}

		_asl_append_string(&out, len, "\t</dict>\n", ENCODE_NONE, 0);

		return out;
	}

	c[1] = '\0';

	for (i = 0; mfmt[i] != '\0'; i++)
	{
		if (mfmt[i] == '$')
		{
			i++;
			paren = 0;

			if (mfmt[i] == '(')
			{
				paren = 1;
				i++;
			}

			k = calloc(1, 1);
			if (k == NULL)
			{
				if (out != NULL) free(out);
				return NULL;
			}

			l = 0;

			for (j = i; mfmt[j] != '\0'; j++)
			{
				c[0] = '\0';
				if (mfmt[j] == '\\') c[0] = mfmt[++j];
				else if ((paren == 1) && (mfmt[j] ==')')) break;
				else if (mfmt[j] != ' ') c[0] = mfmt[j];

				if (c[0] == '\0') break;

				k = reallocf(k, l + 1);
				if (k == NULL)
				{
					if (out != NULL) free(out);
					return NULL;
				}

				k[l] = c[0];
				k[l + 1] = '\0';
				l++;
			}

			if (paren == 1) j++;
			i = j;
			if (l > 0)
			{
				v = asl_get(msg, k);
				if (v != NULL)
				{
					if (!strcmp(k, ASL_KEY_TIME))
					{
						tstr = _asl_time_string(tf, v);
						_asl_append_string(&out, len, tstr, ENCODE_NONE, 0);
						if (tstr != NULL) free(tstr);
					}
					else
					{
						_asl_append_string(&out, len, (char *)v, ENCODE_NONE, 0);
					}
				}
			}
			free(k);
		}

		if (mfmt[i] == '\\')
		{
			i++;
			if (mfmt[i] == '$') _asl_append_string(&out, len, "$", ENCODE_NONE, 0);
			else if (mfmt[i] == 'e') _asl_append_string(&out, len, "\e", ENCODE_NONE, 0);
			else if (mfmt[i] == 's') _asl_append_string(&out, len, " ", ENCODE_NONE, 0);
			else if (mfmt[i] == 'a') _asl_append_string(&out, len, "\a", ENCODE_NONE, 0);
			else if (mfmt[i] == 'b') _asl_append_string(&out, len, "\b", ENCODE_NONE, 0);
			else if (mfmt[i] == 'f') _asl_append_string(&out, len, "\f", ENCODE_NONE, 0);
			else if (mfmt[i] == 'n') _asl_append_string(&out, len, "\n", ENCODE_NONE, 0);
			else if (mfmt[i] == 'r') _asl_append_string(&out, len, "\r", ENCODE_NONE, 0);
			else if (mfmt[i] == 't') _asl_append_string(&out, len, "\t", ENCODE_NONE, 0);
			else if (mfmt[i] == 'v') _asl_append_string(&out, len, "\v", ENCODE_NONE, 0);
			else if (mfmt[i] == '\'') _asl_append_string(&out, len, "\'", ENCODE_NONE, 0);
			else if (mfmt[i] == '\\') _asl_append_string(&out, len, "\\", ENCODE_NONE, 0);
			else if (isdigit(mfmt[i]))
			{
				oval = mfmt[i] - '0';
				if (isdigit(mfmt[i+1]))
				{
					i++;
					oval = (oval * 8) + (mfmt[i] - '0');
					if (isdigit(mfmt[i+1]))
					{
						i++;
						oval = (oval * 8) + (mfmt[i] - '0');
					}
				}
				c[0] = oval;
				_asl_append_string(&out, len, c, ENCODE_NONE, 0);
			}
			continue;
		}

		if (mfmt[i] == '\0') break;
		c[0] = mfmt[i];
		_asl_append_string(&out, len, c, ENCODE_NONE, 0);
	}

	_asl_append_string(&out, len, "\n", ENCODE_NONE, 0);

	return out;
}

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
	char *str, *out_raw, *out;
	uint32_t i, len, level, lmask, outstatus, filter, senderx, facilityx;
	uint64_t v64;
	const char *val;
	char *name, *x;
	time_t tick;
	int status, rc_filter;
	asl_client_t *asl;
	int use_global_lock;
	asl_msg_t *mt;
	char hname[_POSIX_HOST_NAME_MAX];

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
			v64 = 0;

			status = notify_get_state(_asl_global.notify_token, &v64);
			if ((status == NOTIFY_STATUS_OK) && (v64 != 0))
			{
				filter = v64;
				rc_filter = 1;
			}
		}

		if ((rc_filter == 0) && (_asl_global.master_token >= 0))
		{
			v64 = 0;

			status = notify_get_state(_asl_global.master_token, &v64);
			if ((status == NOTIFY_STATUS_OK) && (v64 != 0))
			{
				filter = v64;
			}
		}

		pthread_mutex_unlock(&_asl_global.lock);
	}

	/* 
	 * Time, Host, PID, UID, and GID values get set here
	 */
	str = NULL;
	tick = time(NULL);
	asprintf(&str, "%lu", tick);
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_TIME, str);
		free(str);
	}

	memset(&hname, 0, _POSIX_HOST_NAME_MAX);
	if (gethostname(hname, _POSIX_HOST_NAME_MAX) == 0)
	{
		asl_set(msg, ASL_KEY_HOST, hname);
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

	senderx = (uint32_t)-1;
	facilityx = (uint32_t)-1;
	mt = (asl_msg_t *)msg;

	for (i = 0; (i < mt->count) && ((senderx == (uint32_t)-1) || (facilityx == (uint32_t)-1)); i++)
	{
		if (mt->key[i] == NULL) continue;
		if (streq(mt->key[i], ASL_KEY_SENDER)) senderx = i;
		else if (streq(mt->key[i], ASL_KEY_FACILITY)) facilityx = i;
	}

	/*
	 * Set Sender if needed
	 */
	if ((senderx == (uint32_t)-1) || (mt->val[senderx] == NULL))
	{
		if ((ac != NULL) && (ac->name != NULL))
		{
			/* Use the Sender name from the client handle */
			asl_set(msg, ASL_KEY_SENDER, ac->name);
		}
		else
		{
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

			if (_asl_global.sender != NULL) asl_set(msg, ASL_KEY_SENDER, _asl_global.sender);
			else asl_set(msg, ASL_KEY_SENDER, "Unknown");
		}
	}

	/*
	 * Set Facility
	 */
	if ((facilityx == (uint32_t)-1) || (mt->val[facilityx] == NULL))
	{
		if ((ac != NULL) && (ac->facility != NULL))
		{
			/* Use the Facility name from the client handle */
			asl_set(msg, ASL_KEY_FACILITY, ac->facility);
		}
	}

	outstatus = 0;

	if (use_global_lock != 0) pthread_mutex_lock(&_asl_global.lock);

	if ((filter != 0) && ((filter & lmask) != 0))
	{
		len = 0;
		out_raw = asl_msg_to_string((asl_msg_t *)msg, &len);

		if ((out_raw != NULL) && (len != 0))
		{
			asprintf(&out, "%10u %s\n", len + 1, out_raw);
			if (out != NULL)
			{
				if (asl->sock == -1) _asl_connect(asl);

				if (asl->sock >= 0)
				{
					status = write(asl->sock, out, len + 12);
					if (status < 0)
					{
						/* Write failed - try resetting */
						close(asl->sock);
						asl->sock = -1;
						_asl_connect(asl);
						if (asl->sock >= 0) status = write(asl->sock, out, len + 12);
						if (status < 0) outstatus = -1;
					}
				}
				else outstatus = -1;

				free(out);
			}

			free(out_raw);
		}
	}

	for (i = 0; i < asl->fd_count; i++)
	{
		if (asl->fd_list[i] < 0) continue;

		len = 0;
		out = asl_format_message(msg, asl->fd_mfmt[i], asl->fd_tfmt[i], &len);
		if (out == NULL) continue;

		status = write(asl->fd_list[i], out, len - 1);
		if (status < 0)
		{
			asl->fd_list[i] = -1;
			outstatus = -1;
		}

		free(out);
	}

	if (use_global_lock != 0) pthread_mutex_unlock(&_asl_global.lock);

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
 * Called if there's a malloc error while manipulating a message in asl_set_query.
 * Cleans up the key, kap, and op fields, sets count to zero.
 */
static void
_asl_clear_msg(asl_msg_t *msg)
{
	uint32_t i;

	if (msg == NULL) return;

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key != NULL && msg->key[i] != NULL) free(msg->key[i]);
		if (msg->val != NULL && msg->val[i] != NULL) free(msg->val[i]);
	}

	if (msg->key != NULL) free(msg->key);
	if (msg->val != NULL) free(msg->val);
	if (msg->op != NULL) free(msg->op);

	msg->key = NULL;
	msg->val = NULL;
	msg->op = NULL;

	msg->count = 0;
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
int
asl_set_query(aslmsg a, const char *key, const char *val, uint32_t op)
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
		else if (!strcasecmp(val, ASL_STRING_EMERG))
		{
			dv = strdup("0");
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_ALERT))
		{
			dv = strdup("1");
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_CRIT))
		{
			dv = strdup("2");
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_ERR))
		{
			dv = strdup("3");
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_WARNING))
		{
			dv = strdup("4");
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_NOTICE))
		{
			dv = strdup("5");
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_INFO))
		{
			dv = strdup("6");
			if (dv == NULL) return -1;
		}
		else if (!strcasecmp(val, ASL_STRING_DEBUG))
		{
			dv = strdup("7");
			if (dv == NULL) return -1;
		}
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
			_asl_clear_msg(msg);
			return -1;
		}

		msg->val = (char **)calloc(1, sizeof(char *));
		if (msg->val == NULL)
		{
			_asl_clear_msg(msg);
			return -1;
		}

		if (msg->type == ASL_TYPE_QUERY)
		{
			msg->op = (uint32_t *)calloc(1, sizeof(uint32_t));
			if (msg->op == NULL)
			{
				_asl_clear_msg(msg);
				return -1;
			}
		}
	}
	else
	{
		msg->key = (char **)reallocf(msg->key, (msg->count + 1) * sizeof(char *));
		if (msg->key == NULL)
		{
			_asl_clear_msg(msg);
			return -1;
		}

		msg->val = (char **)reallocf(msg->val, (msg->count + 1) * sizeof(char *));
		if (msg->val == NULL)
		{
			_asl_clear_msg(msg);
			return -1;
		}

		if (msg->type == ASL_TYPE_QUERY)
		{
			msg->op = (uint32_t *)reallocf(msg->op, (msg->count + 1) * sizeof(uint32_t));
			if (msg->op == NULL)
			{
				_asl_clear_msg(msg);
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
				msg->key = (char **)reallocf(msg->key, msg->count * sizeof(char *));
				if (msg->key == NULL) return -1;

				msg->val = (char **)reallocf(msg->val, msg->count * sizeof(char *));
				if (msg->val == NULL) return -1;

				if (msg->op != NULL)
				{
					msg->op = (uint32_t *)reallocf(msg->op, msg->count * sizeof(uint32_t));
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
 * by the aslmsg.  The caller should set the attributes to match using
 * asl_set_query() or asl_set().  The operatoin ASL_QUERY_OP_EQUAL is
 * used for attributes set with asl_set().
 * a:  an aslmsg
 * returns: a set of messages that can be iterated over using aslresp_next(),
 * and the values can be retrieved using aslresp_get.
 */
aslresponse
asl_search(aslclient ac, aslmsg a)
{
	asl_search_result_t *batch, *out;
	char *qstr, *str, *res;
	uint32_t i, j, len, reslen, status;
	uint64_t cmax, qmin;
	kern_return_t kstatus;
	security_token_t sec;
	caddr_t vmstr;

	if (a == NULL) return 0;

	len = 0;
	qstr = asl_msg_to_string((asl_msg_t *)a, &len);

	str = NULL;
	if (qstr == NULL)
	{
		asprintf(&str, "0\n");
		len = 3;
	}
	else
	{
		asprintf(&str, "1\n%s\n", qstr);
		len += 4;
		free(qstr);
	}

	if (str == NULL) return NULL;

	if (asl_server_port == MACH_PORT_NULL)
	{
		kstatus = bootstrap_look_up(bootstrap_port, ASL_SERVICE_NAME, &asl_server_port);
		if (kstatus != KERN_SUCCESS) return NULL;
	}

	/*
	 * Fetch a batch of results each time through the loop.
	 * Fetching many small batches rebuces the load on syslogd.
	 */
	out = NULL;
	qmin = 0;
	cmax = 0;

	forever
	{
		res = NULL;
		reslen = 0;
		sec.val[0] = -1;
		sec.val[1] = -1;
		status = 0;

		kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&vmstr, len, TRUE);
		if (kstatus != KERN_SUCCESS) return NULL;

		memmove(vmstr, str, len);

		kstatus = _asl_server_query(asl_server_port, vmstr, len, qmin, FETCH_BATCH, 0, (caddr_t *)&res, &reslen, &cmax, (int *)&status, &sec);
		if (kstatus != KERN_SUCCESS) break;
		if (res == NULL) break;

		batch = asl_list_from_string(res);
		vm_deallocate(mach_task_self(), (vm_address_t)res, reslen);

		if (batch == NULL) break;
		if (batch->count == 0)
		{
			aslresponse_free(batch);
			break;
		}

		if (out == NULL) out = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));
		if (out == NULL)
		{
			aslresponse_free(batch);
			break;
		}

		if (out->count == 0) out->msg = (asl_msg_t **)calloc(batch->count, sizeof(asl_msg_t *));
		else out->msg = (asl_msg_t **)reallocf(out->msg, (out->count + batch->count) * sizeof(asl_msg_t *));
		if (out->msg == NULL)
		{
			aslresponse_free(batch);
			free(out);
			free(str);
			return NULL;
		}

		for (i = 0, j = out->count; i < batch->count; i++, j++) out->msg[j] = batch->msg[i];

		out->count += batch->count;
		free(batch->msg);
		free(batch);

		if (i < FETCH_BATCH) break;

		if (cmax > qmin) qmin = cmax;
	}

	free(str);
	return out;
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

	for (i = 0; i < res->count; i++) asl_free(res->msg[i]);
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
	int len, y, status, rflags;
	struct tm t;
	time_t tick, delta, factor;
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
		if (str == NULL) return -1;

		if ((str[len-2] == 's') || (str[len-2] == 'S')) str[len-2] = '\0';

		tick = atol(str);
		free(str);

		return tick;
	}
	else if (regexec(&rex_rel, in, 0, NULL, 0) == 0)
	{
		/*
		 * Reletive time (number of seconds before or after right now)
		 */
		str = strdup(in);
		if (str == NULL) return -1;

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
		delta = factor * atol(str);
		tick += delta;

		free(str);

		return tick;
	}
	else if (regexec(&rex_canon, in, 0, NULL, 0) == 0)
	{
		memset(&t, 0, sizeof(struct tm));
		str = strdup(in);
		if (str == NULL) return -1;

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
		if (str == NULL) return -1;

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
