/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2001 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Sascha Schumann <sascha@schumann.cx>                        |
   +----------------------------------------------------------------------+
 */

/* $Id: ircg.c,v 1.1.1.3 2001/12/14 22:12:29 zarzycki Exp $ */

#include <time.h>

#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_ircg.h"
#include "ext/standard/html.h"

#include "ext/standard/php_smart_str.h"
#include "ext/standard/info.h"
#include "ext/standard/basic_functions.h"

#define IRCG_ERROR_MSG_GC_INTERVAL 50

/* This module is not intended for thread-safe use */

static HashTable h_fd2irconn; /* fd's to Integer IDs */
static HashTable h_irconn; /* Integer IDs to php_irconn_t * */
static HashTable h_fmt_msgs; /* Integer IDs to php_fmt_msgs_t * */ 
static int irconn_id = 1;

static unsigned long irc_connects, irc_set_currents, irc_quit_handlers, 
		exec_fmt_msgs, exec_token_compiler;

/* {{{ Format string numbers */
enum {
	FMT_MSG_CHAN = 0,
	FMT_MSG_PRIV_TO_ME,
	FMT_MSG_PRIV_FROM_ME,
	FMT_MSG_LEAVE,
	FMT_MSG_JOIN,
	FMT_MSG_KICK,
	FMT_MSG_TOPIC,
	FMT_MSG_ERROR,
	FMT_MSG_FATAL_ERROR,
	FMT_MSG_JOIN_LIST_END,
	FMT_MSG_SELF_PART,
	FMT_MSG_NICK,
	FMT_MSG_QUIT,
	FMT_MSG_MASS_JOIN_BEGIN,
	FMT_MSG_MASS_JOIN_ELEMENT,
	FMT_MSG_MASS_JOIN_END,
	FMT_MSG_WHOIS_USER,
	FMT_MSG_WHOIS_SERVER,
	FMT_MSG_WHOIS_IDLE,
	FMT_MSG_WHOIS_CHANNEL,
	FMT_MSG_WHOIS_END,
	FMT_MSG_MODE_VOICE,
	FMT_MSG_MODE_OP,
	FMT_MSG_BANLIST,
	FMT_MSG_BANLIST_END,
	NO_FMTS
};
/* }}} */

/* {{{ ircg_functions[]
 */
function_entry ircg_functions[] = {
	PHP_FE(ircg_pconnect, NULL)
	PHP_FE(ircg_set_current, NULL)
	PHP_FE(ircg_join, NULL)
	PHP_FE(ircg_part, NULL)
	PHP_FE(ircg_msg, NULL)
	PHP_FE(ircg_notice, NULL)
	PHP_FE(ircg_nick, NULL)
	PHP_FE(ircg_topic, NULL)
	PHP_FE(ircg_channel_mode, NULL)	
	PHP_FE(ircg_html_encode, NULL)
	PHP_FE(ircg_whois, NULL)
	PHP_FE(ircg_kick, NULL)
	PHP_FE(ircg_nickname_escape, NULL)
	PHP_FE(ircg_nickname_unescape, NULL)
	PHP_FE(ircg_ignore_add, NULL)
	PHP_FE(ircg_disconnect, NULL)
	PHP_FE(ircg_fetch_error_msg, NULL)
	PHP_FE(ircg_ignore_del, NULL)
	PHP_FE(ircg_is_conn_alive, NULL)
	PHP_FE(ircg_lookup_format_messages, NULL)
	PHP_FE(ircg_register_format_messages, NULL)
	PHP_FE(ircg_get_username, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in ircg_functions[] */
};
/* }}} */

#include "if_irc.h"
#include "irc_write_buffer.h"

#if !defined(IRCG_API_VERSION) || IRCG_API_VERSION < 20010303
# error "Please upgrade to at least IRCG 2.0b1"
#endif

typedef struct {
	unsigned char code;
	union {
		unsigned char v;
		void *ptr;
	} para;
} token_t;

typedef struct {
	token_t *t;
	int ntoken;
} format_msg_t;

typedef struct {
	format_msg_t fmt_msgs[NO_FMTS];
} php_fmt_msgs_t;

typedef struct {
	irconn_t conn;
	smart_str buffer;
	time_t login;
	int buffer_count;
	int fd;
	int irconn_id;
	php_fmt_msgs_t *fmt_msgs;
	irc_write_buf wb;
	HashTable ctcp_msgs;
	char *ident; /* NOT available outside of ircg_pconnect or register_hooks */
	char *password; /* dito */
	char *realname; /* dito */
} php_irconn_t;

enum {
	C_CHANNEL = 1,
	C_FROM,
	C_TO,
	C_MESSAGE,
	C_STRING,
	C_PERCENT
};

enum {
	P_RAW         = 0,
	P_JS          = 1,
	P_NICKNAME    = 2,
	P_NICKNAME_JS = 3,
	P_MIRC        = 4,
	P_MIRC_JS     = 5,
	P_NOAUTO_LINKS = 8, /* Don't automatically convert links */
	P_CONV_BR = 16		/* Convert a special character to <br> */
}; 

static php_fmt_msgs_t fmt_msgs_default_compiled;

static char *fmt_msgs_default[] = {
	"%f@%c: %m<br />",
	"%f: %m<br />",
	"To %t: %m<br />",
	"%f leaves %c<br />",
	"%f joins %c<br />",
	"%t was kicked by %f from %c (%m)<br />",
	"%f changes topic on %c to %m<br />",
	"Error: %m<br />",
	"Fatal Error: %m<br />",
	"",
	"",
	"%f changes nick to %t<br />",
	"%f quits (%m)<br />",
	"Welcome",
	" %f",
	" in this very fine channel",
	"%f: user(%t) host(%c) real name(%m)<br />",
	"%f: server(%c) server info(%m)<br />",
	"%f has been idle for %m seconds<br />",
	"%f is on channel %c<br />",
	"End of whois for %f<br />",
	"%f sets voice flag of %t to %m on %c<br />",
	"%f sets channel operator flag of %t to %m on %c<br />",
	"banned from %c: %m<br />",
	"end of ban list for %c<br />"
};

#define format_msg_cache(fmt, cache, chan, to, from, msg, res) \
	format_msg(fmt, chan, to, from, msg, res)
	
#define MSG(conn, type) \
	(&conn->fmt_msgs->fmt_msgs[type])

#define FORMAT_MSG(conn, type, chan, to, from, msg, res) {			\
	format_msg(MSG(conn, type), chan, \
			to, from, msg, res);									\
}

static php_irconn_t *lookup_irconn(int id)
{
	php_irconn_t **ret;

	if (zend_hash_index_find(&h_irconn, id, (void **) &ret) == FAILURE)
		return NULL;
	return *ret;
}

static php_fmt_msgs_t *lookup_fmt_msgs(zval **id)
{
	php_fmt_msgs_t *ret;

	if (zend_hash_find(&h_fmt_msgs, Z_STRVAL_PP(id), Z_STRLEN_PP(id), (void **) &ret) == FAILURE)
		return NULL;
	return ret;
}

static void irconn_dtor(void *dummy)
{
	php_irconn_t **conn = dummy;
	
	irc_disconnect(&(*conn)->conn, "Browser connection closed");
}

static void free_fmt_msg(format_msg_t *f)
{
	int i = 0;
	
	while (f->ntoken-- > 0) {
		switch (f->t[i].code) {
		case C_STRING: 
			smart_str_free_ex(f->t[i].para.ptr, 1);
			free(f->t[i].para.ptr); 
			break;
		}
		i++;
	}

	free(f->t);
}

static void fmt_msgs_dtor(void *dummy)
{
	php_fmt_msgs_t *fmt_msgs = dummy;
	int i;

	for (i = 0; i < NO_FMTS; i++) {
		free_fmt_msg(&fmt_msgs->fmt_msgs[i]);
	}
}

static void quit_handler(irconn_t *c, void *dummy)
{
	php_irconn_t *conn = dummy;

	irc_quit_handlers++;
	if (conn->fd > -1) {
		zend_hash_index_del(&h_fd2irconn, conn->fd);
		irc_write_buf_del(&conn->wb);
		shutdown(conn->fd, 2);
	}
	conn->fd = -2;
	zend_hash_index_del(&h_irconn, conn->irconn_id);

	zend_hash_destroy(&conn->ctcp_msgs);
	if (conn->buffer.c) smart_str_free_ex(&conn->buffer, 1);
	free(conn);
}

static void ircg_js_escape(smart_str *input, smart_str *output)
{
	char *p;
	char *end;

	end = input->c + input->len;

	for(p = input->c; p < end; p++) {
		switch (*p) {
		case '"':
		case '\\':
		case '\'':
			smart_str_appendc_ex(output, '\\', 1);
			/* fall-through */
		default:
			smart_str_appendc_ex(output, *p, 1);
		}
	}
}

static const char hextab[] = "0123456789abcdef";

#define NICKNAME_ESC_CHAR '|'

static void ircg_nickname_escape(smart_str *input, smart_str *output)
{
	char *p;
	char *end;
	char c;

	end = input->c + input->len;

	for(p = input->c; p < end; p++) {
		c = *p;
		if ((c >= 'a' && c <= 'z')
				|| (c >= 'A' && c <= 'Z')
				|| (c >= '0' && c <= '9'))
			smart_str_appendc_ex(output, c, 1);
		else {
			smart_str_appendc_ex(output, NICKNAME_ESC_CHAR, 1);
			smart_str_appendc_ex(output, hextab[c >> 4], 1);
			smart_str_appendc_ex(output, hextab[c & 15], 1);
		}
	}
}

#define HEX_VALUE(c) ((c>='a'&&c<='f')?c-'a'+10:(c>='0'&&c<='9')?c-'0':0)

static void ircg_nickname_unescape(smart_str *input, smart_str *output)
{
	char *p;
	char *end;

	end = input->c + input->len;

	for(p = input->c; p < end; p++) {
		switch (p[0]) {
		case NICKNAME_ESC_CHAR:
			if (p + 2 >= end) break;
			smart_str_appendc_ex(output, (HEX_VALUE(p[1]) << 4) + HEX_VALUE(p[2]), 1);
			p += 2;
			break;
		default:
			smart_str_appendc_ex(output, p[0], 1);
		}
	}
}

/* This is an expensive operation in terms of CPU time.  We
   try to spend as little time in it by caching messages which
   are sent to channels (and hence used multiple times). */
void ircg_mirc_color(const char *, smart_str *, size_t, int, int);

#define NR_CACHE_ENTRIES 983

static unsigned long cache_hits, cache_misses, cache_collisions;

struct {
	smart_str src;
	smart_str result;
} static cache_entries[NR_CACHE_ENTRIES];

static inline php_uint32 ghash(smart_str *str)
{
	php_uint32 h;
	const char *data = str->c, *e = str->c + str->len;
	
	for (h = 2166136261U; data < e; ) {
		h *= 16777619;
		h ^= *data++;
	}
	
	return h;
}

/* {{{ ircg_mirc_color_cache
 */
void ircg_mirc_color_cache(smart_str *src, smart_str *result,
		smart_str *channel, int auto_links, int gen_br)
{
	channel = (void *) 1;
	/* We only cache messages in the context of a channel */
	if (channel) {
		int hash;
		
		hash = ghash(src) % NR_CACHE_ENTRIES;

		if (cache_entries[hash].src.len == src->len &&
				memcmp(cache_entries[hash].src.c, src->c, src->len) == 0) {
			cache_hits++;
		} else {
			cache_misses++;
			
			if (cache_entries[hash].src.len != 0) {
				cache_collisions++;
				cache_entries[hash].src.len = 0;
			}
			cache_entries[hash].result.len = 0;
			ircg_mirc_color(src->c, &cache_entries[hash].result, src->len, auto_links, gen_br);
			
			smart_str_append_ex(&cache_entries[hash].src, src, 1);
		}
		smart_str_append_ex(result, &cache_entries[hash].result, 1);
	} else {
		/* No channel message, no caching */
		ircg_mirc_color(src->c, result, src->len, auto_links, gen_br);
	}
}
/* }}} */


#define NEW_TOKEN(a, b) t = realloc(t, sizeof(token_t) * (++n)); t[n-1].code=a; t[n-1].para.b

static void token_compiler(const char *fmt, format_msg_t *f)
{
	const char *p, *pe;
	const char *q;
	token_t *t = NULL;
	int n = 0;
	char mode;
	unsigned long len;
	char c;
	smart_str *s;

	exec_token_compiler++;
	
	if (fmt[0] == '\0') {
		f->t = NULL;
		f->ntoken = 0;
		return;
	}

	p = fmt;
	pe = fmt + strlen(p);

	do {
		q = p;
		while (*q != '%') 
			if (++q >= pe) {
				s = malloc(sizeof *s);
				s->c = 0;
				smart_str_appendl_ex(s, p, pe - p, 1);
				NEW_TOKEN(C_STRING, ptr) = s;
				goto leave_loop;
			}
		len = q - p;

		if (len > 0) {
			s = malloc(sizeof *s);
			s->c = 0;
			smart_str_appendl_ex(s, p, len, 1);
			NEW_TOKEN(C_STRING, ptr) = s;
		}
		mode = 0;

next:
		c = *++q; /* skip '%' and look at next char */
		switch (c) {
		case '1': mode |= P_JS; goto next;
		case '2': mode |= P_NICKNAME; goto next;
		case '3': mode |= P_NOAUTO_LINKS; goto next;
		case '4': mode |= P_CONV_BR; goto next;

		/* associate mode bits with each command where applicable */
		case 'c': NEW_TOKEN(C_CHANNEL, v) = mode; break;
		case 't': NEW_TOKEN(C_TO, v) = mode; break;
		case 'f': NEW_TOKEN(C_FROM, v) = mode; break;
		case 'r': NEW_TOKEN(C_MESSAGE, v) = mode; break;
		case 'm': NEW_TOKEN(C_MESSAGE, v) = mode | P_MIRC; break;
		case 'j': NEW_TOKEN(C_MESSAGE, v) = mode | P_MIRC | P_JS; break;

		case '%': NEW_TOKEN(C_PERCENT, v) = 0; break;

		default: /* ignore invalid combinations */
		}
		p = q + 1; /* skip last format character */
	} while (p < pe);

leave_loop:
	
	f->ntoken = n;
	f->t = t;
}

/* {{{ format_msg
 */
static void format_msg(const format_msg_t *fmt_msg, smart_str *channel, smart_str *to, smart_str *from, smart_str *msg, smart_str *result)
{
	smart_str encoded_msg = {0};
	int encoded = 0;
	int i = 0;
	const token_t *t = fmt_msg->t;
	int ntoken = fmt_msg->ntoken;

	exec_fmt_msgs++;
	
#define IRCG_APPEND(what) 							\
		switch (t[i].para.v & 7) {					\
		case P_JS: 									\
			if (!what) break;						\
			ircg_js_escape(what, result); 			\
			break; 									\
		case P_NICKNAME_JS: { 						\
			smart_str tmp = {0}; 					\
			if (!what) break;						\
			ircg_nickname_unescape(what, &tmp); 	\
			ircg_js_escape(&tmp, result); 			\
			smart_str_free_ex(&tmp, 1); 			\
			break; 									\
		} 											\
		case P_NICKNAME: 							\
			if (!what) break;						\
			ircg_nickname_unescape(what, result); 	\
			break; 									\
		case P_RAW: 								\
			if (!what) break;						\
			smart_str_append_ex(result, what, 1); 	\
			break; 									\
		case P_MIRC_JS:								\
			if (!what) break;						\
			if (!encoded) {							\
				ircg_mirc_color_cache(msg,			\
						&encoded_msg, channel,		\
						!(t[i].para.v & P_NOAUTO_LINKS), \
						t[i].para.v & P_CONV_BR);	\
				encoded = 1;						\
			}										\
			ircg_js_escape(&encoded_msg, result);	\
			break;									\
		case P_MIRC:								\
			if (!what) break;						\
			if (!encoded) {							\
				ircg_mirc_color_cache(msg, 			\
						&encoded_msg, channel, 		\
						!(t[i].para.v & P_NOAUTO_LINKS), \
						t[i].para.v & P_CONV_BR);	\
				encoded = 1;						\
			}										\
			smart_str_append_ex(result, &encoded_msg, 1); \
			break;									\
		}

	while (ntoken-- > 0) {
		switch (t[i].code) {
		case C_STRING: smart_str_append_ex(result, t[i].para.ptr, 1); break;
		case C_FROM: IRCG_APPEND(from); break;
		case C_TO: IRCG_APPEND(to); break;
		case C_CHANNEL: IRCG_APPEND(channel); break;
		case C_MESSAGE: IRCG_APPEND(msg); break;
		case C_PERCENT: smart_str_appendc_ex(result, '%', 1); break;
		}
		i++;
	}
	
	if (encoded)
		smart_str_free_ex(&encoded_msg, 1);
	
	if (result->c)
		smart_str_0(result);
}
/* }}} */

#include "SAPI.h"

#define ADD_HEADER(a) sapi_add_header(a, sizeof(a) - 1, 1)

static void msg_http_start(php_irconn_t *conn TSRMLS_DC)
{
	ADD_HEADER("Pragma: no-cache");
	sapi_send_headers(TSRMLS_C);
}

static void http_closed_connection(int fd);

static void msg_accum_send(php_irconn_t *conn, smart_str *msg)
{
	if (msg->c == 0) return;

	switch (conn->fd) {
	case -2: /* Connection was finished */
		smart_str_free_ex(msg, 1);
		break;
	case -1: /* No message window yet. Buffer */
		if (conn->buffer_count++ > 10) {
			smart_str_free_ex(msg, 1);
			if ((
#if IRCG_API_VERSION >= 20010601
			ircg_now_time_t
#else
			time(NULL)
#endif
		   	- conn->login) > 30)
				irc_disconnect(&conn->conn, "Timed out waiting for window");
			return;
		}
		smart_str_append_ex(&conn->buffer, msg, 1);
		smart_str_free_ex(msg, 1);
		break;
	default:
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010601
		if (irc_write_buf_append_ex(&conn->wb, msg, 0)) {
			irc_disconnect(&conn->conn, "Write to HTTP client failed");
		}
#elif defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010302
		irc_write_buf_append_ex(&conn->wb, msg, 0);
#else
		irc_write_buf_append(&conn->wb, msg);
		smart_str_free_ex(msg, 1);
#endif
		break;
	}
}

static void msg_send(php_irconn_t *conn, smart_str *msg)
{
	msg_accum_send(conn, msg);
	if (conn->fd != -1)
		irc_write_buf_flush(&conn->wb);
}

static void msg_replay_buffer(php_irconn_t *conn)
{
	msg_accum_send(conn, &conn->buffer);
	conn->buffer.c = NULL;
	conn->buffer.len = conn->buffer.a = 0;
}

static void handle_ctcp(php_irconn_t *conn, smart_str *chan, smart_str *from,
		smart_str *msg, smart_str *result)
{
	char *token_end;
	char *real_msg;
	char *real_msg_end;

	for (token_end = msg->c + 1; *token_end; token_end++)
		if (!isalpha(*token_end)) break;

	if (*token_end != '\001') {
		real_msg = token_end + 1;

		real_msg_end = strchr(real_msg, '\001');
		if (real_msg_end) {
			format_msg_t *fmt_msg;
			smart_str tmp, tmp2;
			
			*real_msg_end = '\0';
			*token_end = '\0';
			
			if (zend_hash_find(&conn->ctcp_msgs, msg->c + 1, token_end - msg->c - 1, (void **) &fmt_msg) != SUCCESS) {
				return;
			}

			smart_str_setl(&tmp, real_msg, real_msg_end - real_msg);
			smart_str_setl(&tmp2, conn->conn.username, conn->conn.username_len);
			format_msg(fmt_msg, chan, &tmp2, from, &tmp, result);
		}
	}
}
	

static void msg_handler(irconn_t *ircc, smart_str *chan, smart_str *from,
		smart_str *msg, void *conn_data, void *chan_data)
{
	php_irconn_t *conn = conn_data;
	smart_str m = {0};
	smart_str s_username;

	smart_str_setl(&s_username, ircc->username, ircc->username_len);

	if (msg->c[0] == '\001') {
		handle_ctcp(conn, chan, from, msg, &m);
	} else if (chan) {
		FORMAT_MSG(conn, FMT_MSG_CHAN, chan, &s_username, from, msg, &m);
	} else {
		format_msg(MSG(conn, FMT_MSG_PRIV_TO_ME), NULL, &s_username, from,
				msg, &m);
	}

	msg_send(conn, &m);
}

static void nick_handler(irconn_t *c, smart_str *oldnick, smart_str *newnick,
		void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_NICK), NULL, newnick, oldnick, NULL,
			&m);
	msg_send(conn, &m);
}

static void whois_user_handler(irconn_t *c, smart_str *nick, smart_str *user,
		smart_str *host, smart_str *real_name, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_WHOIS_USER), host, user, nick,
			real_name, &m);
	msg_send(conn, &m);
}


static void whois_server_handler(irconn_t *c, smart_str *nick, 
		smart_str *server, smart_str *server_info, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_WHOIS_SERVER), server, NULL, nick,
			server_info, &m);
	msg_send(conn, &m);
}


static void whois_idle_handler(irconn_t *c, smart_str *nick, 
		smart_str *idletime, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_WHOIS_IDLE), NULL, NULL, nick,
			idletime, &m);
	msg_send(conn, &m);
}

static void end_of_whois_handler(irconn_t *c, smart_str *nick, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_WHOIS_END), NULL, NULL, nick, NULL, &m);
	msg_send(conn, &m);
}


static void whois_channels_handler(irconn_t *c, smart_str *nick, 
		smart_str *channels, int nr, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};
	int i;
	
	for (i = 0; i < nr; i++) {
		format_msg(MSG(conn, FMT_MSG_WHOIS_CHANNEL), &channels[i], NULL, 
				nick, NULL, &m);
	}
	msg_send(conn, &m);
}

/*
 * This is an internal API which serves the purpose to store the reason
 * for terminating a connection.  The termination will cause the
 * connection id to become invalid.  A script can then use a
 * function to retrieve the last error message which was received
 * from the IRC server, and will usually present a nicely formatted
 * error message to the end-user.
 *
 * We automatically garbage-collect every n-th login, so there is
 * no need for a separate gc thread.
 */

struct errormsg {
	smart_str msg;
	int msgid;
	int id;
	struct errormsg *next;
};

static struct errormsg *errormsgs;

static void error_msg_dtor(struct errormsg *m)
{
	smart_str_free_ex(&m->msg, 1);
	free(m);
}

static void error_msg_gc(void)
{
	struct errormsg *m, *prev = NULL, *next;
	int lim;

	lim = irconn_id - IRCG_ERROR_MSG_GC_INTERVAL;
	
	for (m = errormsgs; m; prev = m, m = m->next) {
		if (m->id < lim) {
			struct errormsg *to;
			/* Check whether we have subsequent outdated records */
			
			for (to = m->next; to; to = next) {
				next = to->next;
				if (m->id >= lim) break;
				error_msg_dtor(to);
			}

			error_msg_dtor(m);
			
			if (prev)
				prev->next = to;
			else
				errormsgs = to;
			
			if (!to) break;
			m = to;
		}
	}
}

static void add_error_msg(smart_str *msg, int msgid, php_irconn_t *conn)
{
	struct errormsg *m;

	for (m = errormsgs; m; m = m->next) {
		if (m->id == conn->irconn_id) break;
	}

	if (!m) {
		m = malloc(sizeof(*m));
		m->msg.c = 0;
		m->id = conn->irconn_id;
	}

	m->msg.len = 0;
	smart_str_append_ex(&m->msg, msg, 1);
	m->msgid = msgid;
	m->next = errormsgs;
	errormsgs = m;
}

static struct errormsg *lookup_and_remove_error_msg(int id)
{
	struct errormsg *m, *prev = NULL;

	for (m = errormsgs; m; prev = m, m = m->next) {
		if (m->id == id) {
			if (prev)
				prev->next = m->next;
			else
				errormsgs = m->next;

			return m;
		}
	}
	return NULL;
}

static void error_handler(irconn_t *ircc, int id, int fatal, smart_str *msg, void *conn_data)
{
	php_irconn_t *conn = conn_data;
	smart_str m = {0};
	smart_str s_username;
	smart_str tmp;
	
	smart_str_setl(&tmp, "IRC SERVER", sizeof("IRC SERVER") - 1);
	smart_str_setl(&s_username, ircc->username, ircc->username_len);
	format_msg(MSG(conn, fatal ? FMT_MSG_FATAL_ERROR : FMT_MSG_ERROR), NULL, 
			&s_username, &tmp, msg, &m);

	if (fatal) {
		add_error_msg(msg, id, conn);
	}
	
	msg_send(conn, &m);
}

static void banlist_handler(irconn_t *ircc, smart_str *channel, smart_str *mask, void *conn_data)
{
	php_irconn_t *conn = conn_data;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_BANLIST), channel, NULL, NULL, mask, &m);
	msg_send(conn, &m);
}

static void end_of_banlist_handler(irconn_t *ircc, smart_str *channel, void *conn_data)
{
	php_irconn_t *conn = conn_data;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_BANLIST_END), channel, NULL, NULL, NULL, &m);
	msg_send(conn, &m);
}

static void user_add(irconn_t *ircc, smart_str *channel, smart_str *users,
		int nr, void *dummy)
{
	php_irconn_t *conn = dummy;
	int i;
	smart_str m = {0};

	if (nr > 1) {
		format_msg(MSG(conn, FMT_MSG_MASS_JOIN_BEGIN), channel, NULL, NULL,
				NULL, &m);
		for (i = 0; i < nr; i++) {
			format_msg(MSG(conn, FMT_MSG_MASS_JOIN_ELEMENT), channel, NULL,
					&users[i], NULL, &m);
		}
	
		format_msg(MSG(conn, FMT_MSG_MASS_JOIN_END), channel, NULL, NULL,
				NULL, &m);
	} else {
		FORMAT_MSG(conn, FMT_MSG_JOIN, channel, NULL, &users[0],
				NULL, &m);
		FORMAT_MSG(conn, FMT_MSG_JOIN_LIST_END, channel, NULL, NULL,
			NULL, &m);
	}
	msg_send(conn, &m);
}

static void new_topic(irconn_t *ircc, smart_str *channel, smart_str *who, smart_str *topic, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_TOPIC), channel, NULL, who, topic, &m);
	msg_send(conn, &m);
}

static void part_handler(irconn_t *ircc, smart_str *channel, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};
	smart_str s_username;

	smart_str_setl(&s_username, ircc->username, ircc->username_len);

	format_msg(MSG(conn, FMT_MSG_SELF_PART), channel, NULL, &s_username, 
			NULL, &m);
	msg_send(conn, &m);
}

static void user_leave(irconn_t *ircc, smart_str *channel, smart_str *user, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_LEAVE), channel, NULL, user, NULL, &m);
	msg_send(conn, &m);
}

static void user_quit(irconn_t *ircc, smart_str *user, smart_str *msg, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_QUIT), NULL, NULL, user, msg, &m);
	msg_send(conn, &m);
}

static void idle_recv_queue(irconn_t *ircc, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	smart_str_appendl_ex(&m, "<!---->", 7, 1);
	msg_send(conn, &m);
}

static void user_kick(irconn_t *ircc, smart_str *channel, smart_str *who, smart_str *kicked_by, smart_str *reason, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_KICK), channel, who, kicked_by, reason, &m);
	msg_send(conn, &m);
}

#if IRCG_API_VERSION >= 20010307
static void mode_channel_handler(irconn_t *ircc, smart_str *nick,
		smart_str *channel, smart_str *who, int mode, int give, void *dummy,
		void *dummy2)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};
	smart_str what;

	if (give)
		smart_str_setl(&what, "1", 1);
	else
		smart_str_setl(&what, "0", 1);
	
	if (mode & IRCG_MODE_VOICE) {
		format_msg(MSG(conn, FMT_MSG_MODE_VOICE), channel, nick, who, &what, 
				&m);
		msg_send(conn, &m);
	}
	if (mode & IRCG_MODE_OP) {
		format_msg(MSG(conn, FMT_MSG_MODE_OP), channel, nick, who, &what, &m);
		msg_send(conn, &m);
	}
}
#endif

static void http_closed_connection(int fd)
{
	int *id, stored_id;

	if (zend_hash_index_find(&h_fd2irconn, fd, (void **) &id) == FAILURE)
		return;

	stored_id = *id;

	zend_hash_index_del(&h_fd2irconn, fd);
	zend_hash_index_del(&h_irconn, stored_id);
}

PHP_FUNCTION(ircg_get_username)
{
	zval **p1;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;

	RETVAL_STRINGL(conn->conn.username, conn->conn.username_len, 1);
}

PHP_FUNCTION(ircg_set_current)
{
#ifdef IRCG_WITH_THTTPD
	zval **p1;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;

	if (conn->fd >= 0) {
		/* Migrate IRC session to another HTTP connection */
		zend_hash_index_del(&h_fd2irconn, conn->fd);
		irc_write_buf_del(&conn->wb);
		shutdown(conn->fd, 2);
	}
	irc_set_currents++;
	thttpd_register_on_close(http_closed_connection);
	thttpd_set_dont_close();
	conn->fd = thttpd_get_fd();
	if (fcntl(conn->fd, F_GETFL) == -1) {
		zend_hash_index_del(&h_irconn, Z_LVAL_PP(p1));
		RETURN_FALSE;
	}
	zend_hash_index_update(&h_fd2irconn, conn->fd, &Z_LVAL_PP(p1), sizeof(int), NULL);
	irc_write_buf_add(&conn->wb, conn->fd);
	msg_http_start(conn TSRMLS_CC);
	msg_replay_buffer(conn);
	irc_write_buf_flush(&conn->wb);

	RETURN_TRUE;
#endif
}

PHP_FUNCTION(ircg_nickname_escape)
{
	zval **p1;
	smart_str in;
	smart_str out = {0};

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string_ex(p1);

	smart_str_setl(&in, Z_STRVAL_PP(p1), Z_STRLEN_PP(p1));

	ircg_nickname_escape(&in, &out);
	smart_str_0(&out);
	
	RETVAL_STRINGL(out.c, out.len, 1);

	smart_str_free_ex(&out, 1);
}

PHP_FUNCTION(ircg_nickname_unescape)
{
	zval **p1;
	smart_str in;
	smart_str out = {0};

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string_ex(p1);

	smart_str_setl(&in, Z_STRVAL_PP(p1), Z_STRLEN_PP(p1));

	ircg_nickname_unescape(&in, &out);
	smart_str_0(&out);
	
	RETVAL_STRINGL(out.c, out.len, 1);

	smart_str_free_ex(&out, 1);
}

PHP_FUNCTION(ircg_new_window)
{
	zval **p1;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string_ex(p1);

	RETVAL_STRING(Z_STRVAL_PP(p1), 1);
}

PHP_FUNCTION(ircg_join)
{
	zval **p1, **p2, **p3;
	php_irconn_t *conn;
	char *key = NULL;

	if (ZEND_NUM_ARGS() < 2 || ZEND_NUM_ARGS() > 3
			|| zend_get_parameters_ex(ZEND_NUM_ARGS(), &p1, &p2, &p3) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);

	if (ZEND_NUM_ARGS() > 2) {
		convert_to_string_ex(p3);
		key = Z_STRVAL_PP(p3);
	}
	
	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_join(&conn->conn, Z_STRVAL_PP(p2), key,  conn);

	RETVAL_TRUE;
}

PHP_FUNCTION(ircg_whois)
{
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010227
	zval **p1, **p2;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &p1, &p2) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_handle_command(&conn->conn, "WHOIS", 1, Z_STRVAL_PP(p2));
	RETVAL_TRUE;
#endif
}

/* {{{ PHP_FUNCTION(ircg_ignore_add)
 */
PHP_FUNCTION(ircg_ignore_add)
{
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010402
	zval **args[2];
	php_irconn_t *conn;
	smart_str s;

	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_array_ex(2, args) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(args[0]);
	convert_to_string_ex(args[1]);

	conn = lookup_irconn(Z_LVAL_PP(args[0]));
	if (!conn) RETURN_FALSE;

	smart_str_setl(&s, Z_STRVAL_PP(args[1]), Z_STRLEN_PP(args[1]));
	if (irc_ignore_check(&conn->conn, &s) == 0)
		irc_ignore_add(&conn->conn, &s, 1);
#endif
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_fetch_error_msg)
 */
PHP_FUNCTION(ircg_fetch_error_msg)
{
	zval **args[2];
	struct errormsg *m;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_array_ex(1, args) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(args[0]);

	m = lookup_and_remove_error_msg(Z_LVAL_PP(args[0]));

	if (!m) RETURN_FALSE;

	array_init(return_value);
	add_index_long(return_value, 0, m->msgid);
	add_index_stringl(return_value, 1, m->msg.c, m->msg.len, 1);
	error_msg_dtor(m);
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_ignore_del)
 */
PHP_FUNCTION(ircg_ignore_del)
{
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010402
	zval **args[2];
	php_irconn_t *conn;
	smart_str s;

	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_array_ex(2, args) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(args[0]);
	convert_to_string_ex(args[1]);

	conn = lookup_irconn(Z_LVAL_PP(args[0]));
	if (!conn) RETURN_FALSE;

	smart_str_setl(&s, Z_STRVAL_PP(args[1]), Z_STRLEN_PP(args[1]));
	if (irc_ignore_del(&conn->conn, &s))
		RETURN_FALSE;
	RETURN_TRUE;
#endif
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_channel_mode)
 */
PHP_FUNCTION(ircg_channel_mode)
{
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010227
	zval **args[4];
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 4 || zend_get_parameters_array_ex(4, args) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(args[0]);
	convert_to_string_ex(args[1]);
	convert_to_string_ex(args[2]);
	convert_to_string_ex(args[3]);

	conn = lookup_irconn(Z_LVAL_PP(args[0]));
	if (!conn) RETURN_FALSE;
	
	irc_handle_command(&conn->conn, "MODE", 3, Z_STRVAL_PP(args[1]),
			Z_STRVAL_PP(args[2]), Z_STRVAL_PP(args[3]));
	RETVAL_TRUE;
#endif
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_topic)
 */
PHP_FUNCTION(ircg_topic)
{
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010226
	zval **p1, **p2, **p3;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &p1, &p2, &p3) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);
	convert_to_string_ex(p3);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_handle_command(&conn->conn, "TOPIC", 2, Z_STRVAL_PP(p2), Z_STRVAL_PP(p3));
	RETVAL_TRUE;
#endif
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_html_encode)
 */
PHP_FUNCTION(ircg_html_encode)
{
	zval **p1, **p2, **p3;
	smart_str res = {0};
	int auto_links = 1;
	int conv_br = 0;
	
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 3
		   	|| zend_get_parameters_ex(ZEND_NUM_ARGS(), &p1, &p2, &p3) == FAILURE)
		WRONG_PARAM_COUNT;

	switch (ZEND_NUM_ARGS()) {
		case 3:
			convert_to_boolean_ex(p3);
			conv_br = Z_LVAL_PP(p3);
		case 2:
			convert_to_boolean_ex(p2);
			auto_links = Z_LVAL_PP(p2);
		case 1:
			convert_to_string_ex(p1);
	}
	
	ircg_mirc_color(Z_STRVAL_PP(p1), &res, Z_STRLEN_PP(p1), auto_links, conv_br);

	RETVAL_STRINGL(res.c, res.len, 1);

	smart_str_free_ex(&res, 1);
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_kick)
 */
PHP_FUNCTION(ircg_kick)
{
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010226
	zval **p1, **p2, **p3, **p4;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 4 || zend_get_parameters_ex(4, &p1, &p2, &p3, &p4) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);
	convert_to_string_ex(p3);
	convert_to_string_ex(p4);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_handle_command(&conn->conn, "KICK", 3, Z_STRVAL_PP(p2), Z_STRVAL_PP(p3), Z_STRVAL_PP(p4));
	RETVAL_TRUE;
#endif
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_part)
 */
PHP_FUNCTION(ircg_part)
{
	zval **p1, **p2;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &p1, &p2) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_part(&conn->conn, Z_STRVAL_PP(p2));
	RETVAL_TRUE;
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_is_conn_alive)
 */
PHP_FUNCTION(ircg_is_conn_alive)
{
	zval **p1;

	if (ZEND_NUM_ARGS() != 1
			|| zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);

	if (lookup_irconn(Z_LVAL_PP(p1)))
		RETURN_TRUE;
	RETURN_FALSE;
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_lookup_format_messages)
 */
PHP_FUNCTION(ircg_lookup_format_messages)
{
	zval **p1;

	if (ZEND_NUM_ARGS() != 1 
			|| zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string_ex(p1);

	if (lookup_fmt_msgs(p1))
		RETURN_TRUE;
	RETVAL_FALSE;
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_register_format_messages)
 */
PHP_FUNCTION(ircg_register_format_messages)
{
	zval **p1, **p2;
	HashTable *h;
	int i;
	php_fmt_msgs_t fmt_msgs;
	zval **arg;

	if (ZEND_NUM_ARGS() != 2 
			|| zend_get_parameters_ex(2, &p1, &p2) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string_ex(p1);	

	if (Z_TYPE_PP(p2) != IS_ARRAY) {
		php_error(E_WARNING, "The second parameter should be an array");
		RETURN_FALSE;
	}

	h = HASH_OF(*p2);

	for (i = 0; i < NO_FMTS; i++) {
		if (zend_hash_index_find(h, i, (void **) &arg) == SUCCESS) {
			convert_to_string_ex(arg);
			token_compiler(Z_STRVAL_PP(arg), &fmt_msgs.fmt_msgs[i]);
		} else
			token_compiler("", &fmt_msgs.fmt_msgs[i]);
	}

	
	zend_hash_update(&h_fmt_msgs, Z_STRVAL_PP(p1), Z_STRLEN_PP(p1), 
		&fmt_msgs, sizeof(fmt_msgs), NULL);

	RETVAL_TRUE;	
}
/* }}} */

/* {{{ register_hooks
 */
static void register_hooks(irconn_t *conn, void *dummy)
{
	php_irconn_t *irconn = dummy;
	
	if (irconn->ident) {
		smart_str m;

		smart_str_sets(&m, irconn->ident);
		irc_set_ident(conn, &m);
	}

	if (irconn->password) {
		smart_str m;

		smart_str_sets(&m, irconn->password);
		irc_set_password(conn, &m);
	}

#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010225
	if (irconn->realname) {
		smart_str m;
		smart_str_sets(&m, irconn->realname);
		irc_set_realname(conn, &m);
	}
#endif
	
#define IFMSG(n, p, q) if (MSG(irconn, n)->ntoken != 0) irc_register_hook(conn, p, q)
	
	irc_register_hook(conn, IRCG_MSG, msg_handler);
	irc_register_hook(conn, IRCG_QUIT, quit_handler);
	irc_register_hook(conn, IRCG_ERROR, error_handler);
	IFMSG(FMT_MSG_NICK, IRCG_NICK, nick_handler);

	IFMSG(FMT_MSG_SELF_PART, IRCG_PART, part_handler);
	IFMSG(FMT_MSG_MASS_JOIN_ELEMENT, IRCG_USER_ADD, user_add);
	IFMSG(FMT_MSG_LEAVE, IRCG_USER_LEAVE, user_leave);
	IFMSG(FMT_MSG_KICK, IRCG_USER_KICK, user_kick);
	IFMSG(FMT_MSG_QUIT, IRCG_USER_QUIT, user_quit);
	IFMSG(FMT_MSG_TOPIC, IRCG_TOPIC, new_topic);

#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010227
	irc_register_hook(conn, IRCG_WHOISUSER, whois_user_handler);
	irc_register_hook(conn, IRCG_WHOISSERVER, whois_server_handler);
	irc_register_hook(conn, IRCG_WHOISIDLE, whois_idle_handler);
	irc_register_hook(conn, IRCG_WHOISCHANNELS, whois_channels_handler);
	irc_register_hook(conn, IRCG_ENDOFWHOIS, end_of_whois_handler);
#endif

#if IRCG_API_VERSION >= 20010307
	irc_register_hook(conn, IRCG_MODE_CHANNEL, mode_channel_handler);
#endif

#if IRCG_API_VERSION >= 20010310
	irc_register_hook(conn, IRCG_IDLE_RECV_QUEUE, idle_recv_queue);
#endif

#if IRCG_API_VERSION >= 20010416
	irc_register_hook(conn, IRCG_BANLIST, banlist_handler);
	irc_register_hook(conn, IRCG_ENDOFBANLIST, end_of_banlist_handler);
#endif
}
/* }}} */

static void ctcp_msgs_dtor(format_msg_t *fmt)
{
	free_fmt_msg(fmt);
}

/* {{{ ircg_copy_ctcp_msgs
 */
static int ircg_copy_ctcp_msgs(zval **array, php_irconn_t *conn)
{
	zval **val;
	char *str;
	ulong num;
	uint str_len;
	HashPosition pos;
	format_msg_t fmt;

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(array), &pos);

	while (zend_hash_get_current_key_ex(Z_ARRVAL_PP(array), &str, &str_len, &num, 0, &pos) == HASH_KEY_IS_STRING) {
		zend_hash_get_current_data_ex(Z_ARRVAL_PP(array), (void **) &val, &pos);
		convert_to_string_ex(val);
		token_compiler(Z_STRVAL_PP(val), &fmt);
		zend_hash_add(&conn->ctcp_msgs, str, str_len - 1, &fmt,
				sizeof(fmt), NULL);
		
		zend_hash_move_forward_ex(Z_ARRVAL_PP(array), &pos);
	}
	
	return 0;
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_pconnect)
 */
PHP_FUNCTION(ircg_pconnect)
{
	/* This should become an array very soon */
	zval **p1, **p2, **p3, **p4 = NULL, **p5 = NULL, **p6;
	const char *username = 0;
	const char *server = "0";
	int port = 6667;
	php_fmt_msgs_t *fmt_msgs = NULL;	
	php_irconn_t *conn;
	
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 6
			|| zend_get_parameters_ex(ZEND_NUM_ARGS(), &p1, &p2, &p3, &p4, &p5, &p6) == FAILURE)
		WRONG_PARAM_COUNT;

	switch (ZEND_NUM_ARGS()) {
	case 6:
	case 5:
	case 4:
		convert_to_string_ex(p4);
		fmt_msgs = lookup_fmt_msgs(p4);
	case 3:
		convert_to_long_ex(p3);
		port = Z_LVAL_PP(p3);
	case 2:
		convert_to_string_ex(p2);
		server = Z_STRVAL_PP(p2);
	case 1:
		convert_to_string_ex(p1);
		username = Z_STRVAL_PP(p1);
	}

	if (!fmt_msgs)
		fmt_msgs = &fmt_msgs_default_compiled;

	/*
	 * conn must be able to live longer than the hash entry in h_irconn,
	 * so we have to allocate it ourselves.
	 */
	conn = malloc(sizeof(*conn));
	conn->fd = -1;
	conn->ident = conn->password = conn->realname = NULL;
	if (ZEND_NUM_ARGS() > 5 && Z_TYPE_PP(p6) == IS_ARRAY) {
		zval **val;

		if (zend_hash_find(Z_ARRVAL_PP(p6), "ident", sizeof("ident"),
					(void **) &val) == SUCCESS) {
			convert_to_string_ex(val);
			conn->ident = Z_STRVAL_PP(val);
		}

		if (zend_hash_find(Z_ARRVAL_PP(p6), "password", sizeof("password"),
					(void **) &val) == SUCCESS) {
			convert_to_string_ex(val);
			conn->password = Z_STRVAL_PP(val);
		}

		if (zend_hash_find(Z_ARRVAL_PP(p6), "realname", sizeof("realname"),
					(void **) &val) == SUCCESS) {
			convert_to_string_ex(val);
			conn->realname = Z_STRVAL_PP(val);
		}
	}

	conn->fmt_msgs = fmt_msgs;	
	if (irc_connect(username, register_hooks, 
			conn, server, port, &conn->conn)) {
		free(conn);
		RETURN_FALSE;
	}
	irc_connects++;
	zend_hash_init(&conn->ctcp_msgs, 10, NULL, ctcp_msgs_dtor, 1);
	if (p5 && Z_TYPE_PP(p5) == IS_ARRAY) {
		ircg_copy_ctcp_msgs(p5, conn);
	}
	conn->password = conn->ident = NULL;
	irconn_id++;
	if ((irconn_id % IRCG_ERROR_MSG_GC_INTERVAL) == 0)
		error_msg_gc();
	conn->irconn_id = irconn_id;
	zend_hash_index_update(&h_irconn, irconn_id, &conn, sizeof(conn), NULL);
	conn->buffer.c = NULL;
	conn->buffer_count = 0;
#if IRCG_API_VERSION >= 20010601
	conn->login = ircg_now_time_t;
#else
	time(&conn->login);
#endif

	RETVAL_LONG(irconn_id);
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_disconnect)
 */
PHP_FUNCTION(ircg_disconnect)
{
	zval **id, **reason;
	
	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &id, &reason) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(id);
	convert_to_string_ex(reason);

	zend_hash_index_del(&h_irconn, Z_LVAL_PP(id));

	RETURN_TRUE;
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_nick)
 */
PHP_FUNCTION(ircg_nick)
{
	zval **id, **newnick;
	php_irconn_t *conn;
	
	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &id, &newnick) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(id);
	convert_to_string_ex(newnick);

	conn = lookup_irconn(Z_LVAL_PP(id));

	if (!conn) RETURN_FALSE;
	
	irc_nick(&conn->conn, Z_STRVAL_PP(newnick));
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_notice)
 */
PHP_FUNCTION(ircg_notice)
{
	zval **id, **recipient, **msg;
	php_irconn_t *conn;
	
	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &id, &recipient, &msg) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(id);
	convert_to_string_ex(recipient);
	convert_to_string_ex(msg);

	conn = lookup_irconn(Z_LVAL_PP(id));

	if (!conn) RETURN_FALSE;

	irc_handle_command(&conn->conn, "NOTICE", 2, Z_STRVAL_PP(recipient), Z_STRVAL_PP(msg));
	RETURN_TRUE;
}
/* }}} */

/* {{{ PHP_FUNCTION(ircg_msg)
 */
PHP_FUNCTION(ircg_msg)
{
	zval **id, **recipient, **msg, **suppress;
	php_irconn_t *conn;
	smart_str l = {0};
	smart_str m = {0};
	smart_str tmp, tmp2;
	int o_suppress = 0;
	
	if (ZEND_NUM_ARGS() < 3 || ZEND_NUM_ARGS() > 4 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &id, &recipient, &msg, &suppress) == FAILURE)
		WRONG_PARAM_COUNT;

	switch (ZEND_NUM_ARGS()) {
	case 4:
		convert_to_long_ex(suppress);
		o_suppress = Z_LVAL_PP(suppress);
	case 3:
		convert_to_long_ex(id);
		convert_to_string_ex(recipient);
		convert_to_string_ex(msg);
	}

	conn = lookup_irconn(Z_LVAL_PP(id));

	if (!conn) RETURN_FALSE;

	irc_msg(&conn->conn, Z_STRVAL_PP(recipient), Z_STRVAL_PP(msg));
	
	if (!o_suppress) {
		smart_str_setl(&l, Z_STRVAL_PP(msg), Z_STRLEN_PP(msg));

		smart_str_setl(&tmp, Z_STRVAL_PP(recipient), Z_STRLEN_PP(recipient));
		smart_str_setl(&tmp2, conn->conn.username, conn->conn.username_len);


		switch (Z_STRVAL_PP(recipient)[0]) {
			case '#':
			case '&':
				if (l.c[0] == 1) {
					handle_ctcp(conn, &tmp, &tmp2, &l, &m);
				} else {
					FORMAT_MSG(conn, FMT_MSG_CHAN, &tmp, NULL, &tmp2, &l, &m);
				}
				break;
			default:
				if (l.c[0] == 1) {
					handle_ctcp(conn, NULL, &tmp2, &l, &m);
				} else {
					format_msg(MSG(conn, FMT_MSG_PRIV_FROM_ME), NULL,
							&tmp, &tmp2, &l, &m);
				}
		}

		msg_send(conn, &m);
	}

	RETURN_TRUE;
}
/* }}} */

zend_module_entry ircg_module_entry = {
	STANDARD_MODULE_HEADER,
	"ircg",
	ircg_functions,
	PHP_MINIT(ircg),
	PHP_MSHUTDOWN(ircg),
	NULL,
	NULL,
	PHP_MINFO(ircg),
	NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_IRCG
ZEND_GET_MODULE(ircg)
#endif

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(ircg)
{
	int i;

#if IRCG_API_VERSION >= 20010307
	if (irc_sizeof_irconn() != sizeof(irconn_t)) {
		printf("FATAL: The size of the irconn_t structure has changed "
				"since you compiled PHP.  Please rebuild PHP against "
				"the correct IRCG header files.\n");
		exit(1);
	}

	if (irc_sizeof_write_buf() != sizeof(irc_write_buf)) {
		printf("FATAL: The size of the irc_write_buf structure has changed "
				"since you compiled PHP.  Please rebuild PHP against "
				"the correct IRCG header files.\n");
		exit(1);
	}
#endif
	
	for (i = 0; i < NO_FMTS; i++) {
		token_compiler(fmt_msgs_default[i], &fmt_msgs_default_compiled.fmt_msgs[i]);
	}

	zend_hash_init(&h_fmt_msgs, 0, NULL, fmt_msgs_dtor, 1);	
	zend_hash_init(&h_fd2irconn, 0, NULL, NULL, 1);
	zend_hash_init(&h_irconn, 0, NULL, irconn_dtor, 1);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(ircg)
{
	zend_hash_destroy(&h_fmt_msgs);	
	zend_hash_destroy(&h_irconn);
	zend_hash_destroy(&h_fd2irconn);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(ircg)
{
	char buf[100];

	php_info_print_table_start();
	php_info_print_table_header(2, "ircg support", "enabled");
	sprintf(buf, "%lu", cache_hits);
	php_info_print_table_row(2, "scanner result cache hits", buf);
	sprintf(buf, "%lu", cache_misses);
	php_info_print_table_row(2, "scanner result cache misses", buf);
	sprintf(buf, "%lu", cache_collisions);
	php_info_print_table_row(2, "scanner result cache collisions", buf);
	sprintf(buf, "%lu", exec_fmt_msgs);
	php_info_print_table_row(2, "executed format messages", buf);
	sprintf(buf, "%lu", exec_token_compiler);
	php_info_print_table_row(2, "number of tokenizer invocations", buf);
	sprintf(buf, "%lu", irc_connects);
	php_info_print_table_row(2, "irc_connects", buf);
	sprintf(buf, "%lu", irc_set_currents);
	php_info_print_table_row(2, "irc_set_currents", buf);
	sprintf(buf, "%lu", irc_quit_handlers);
	php_info_print_table_row(2, "irc_quit_handlers", buf);
	php_info_print_table_end();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 tw=78 fdm=marker
 * vim<600: sw=4 ts=4 tw=78
 */
