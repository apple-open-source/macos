/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2003 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Sascha Schumann <sascha@schumann.cx>                         |
   +----------------------------------------------------------------------+
 */

/* $Id: ircg.c,v 1.137.2.18 2003/10/22 14:50:19 sas Exp $ */

/* {{{ includes */

#include <time.h>

#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_ircg.h"
#include "ext/standard/html.h"

#include "ext/standard/php_lcg.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/info.h"
#include "ext/standard/basic_functions.h"

#ifdef PHP_WIN32
#include "win32/time.h"
#else
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif


/* }}} */

/* {{{ Definitions */

/* This module is not intended for thread-safe use */

static HashTable h_fd2irconn; /* fd's to Integer IDs */
static HashTable h_irconn; /* Integer IDs to php_irconn_t * */
static HashTable h_fmt_msgs; /* Integer IDs to php_fmt_msgs_t * */ 
static int irconn_id;

static unsigned long irc_connects, irc_set_currents, irc_quit_handlers, 
		exec_fmt_msgs, exec_token_compiler;

static int highest_fd;

#define SEEN_FD(fd) do { if ((fd) > highest_fd) highest_fd = (fd); } while (0)

/* }}} */

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
	FMT_MSG_DISCONNECTED,
	FMT_MSG_LIST,
	FMT_MSG_LISTEND,
	FMT_MSG_WHOREPLY1,
	FMT_MSG_WHOREPLY2,
	FMT_MSG_ENDOFWHO,
	FMT_MSG_INVITE,
	FMT_MSG_NOTICE_CHAN,
	FMT_MSG_NOTICE_TO_ME,
	FMT_MSG_NOTICE_FROM_ME,
	FMT_MSG_LUSERCLIENT,
	FMT_MSG_LUSEROP,
	FMT_MSG_LUSERUNKNOWN,
	FMT_MSG_LUSERCHANNELS,
	FMT_MSG_LUSERME,
	NO_FMTS
};
/* }}} */

/* {{{ ircg_functions[] */
function_entry ircg_functions[] = {
	PHP_FE(ircg_set_on_die, NULL)
	PHP_FE(ircg_pconnect, NULL)
	PHP_FE(ircg_set_current, NULL)
	PHP_FE(ircg_set_file, NULL)
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
	PHP_FE(ircg_eval_ecmascript_params, NULL)
	PHP_FE(ircg_names, NULL)
	PHP_FE(ircg_invite, NULL)
	PHP_FE(ircg_lusers, NULL)
	PHP_FE(ircg_oper, NULL)
	PHP_FE(ircg_who, NULL)
	PHP_FE(ircg_list, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in ircg_functions[] */
};
/* }}} */

/* {{{ Structures, enumerations, definitions */

#include "if_irc.h"
#include "irc_write_buffer.h"

#if IRCG_API_VERSION - 0 < 20010303
# error "Please upgrade to at least IRCG 2.0b1"
#endif

typedef struct {
	unsigned char code;
	union {
		unsigned char v;
		void *ptr;
		smart_str *s;
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
	int fd;
	int irconn_id;
#if 0
	struct sockaddr_in sin; /* address of stream conn */
#endif
	php_fmt_msgs_t *fmt_msgs;
	irc_write_buf wb;
	HashTable ctcp_msgs;
	
#ifdef IRCG_PENDING_URL
	char *od_data; /* On_Die */
	size_t od_len;
	struct in_addr od_ip;
	short od_port;
#endif
	
	int file_fd;
	
	char bailout_on_trivial; /* Whether to handle trivial errors as fatal */
	
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
	C_PERCENT,
	C_TERMINATE_1		/* auth by username */
};

enum {
	P_RAW         = 0,
	P_JS          = 1,
	P_NICKNAME    = 2,
	P_NICKNAME_JS = 3,
	P_HTML        = 4,
	P_HTML_JS     = 5,
	P_NOAUTO_LINKS = 8, /* Don't automatically convert links */
	P_CONV_BR      = 16,	/* Convert a special character to <br> */
	P_COND_STOP    = 32,	/* If argument != username, stop */
}; 

static php_fmt_msgs_t fmt_msgs_default_compiled;

static void format_msg(const format_msg_t *fmt_msg, smart_str *channel, 
		smart_str *to, smart_str *from, smart_str *msg, smart_str *result, 
		const char *username, int username_len, int *status);

static void msg_send(php_irconn_t *conn, smart_str *msg);

static php_irconn_t *flush_data;

/* }}} */

/* {{{ Default format messages */

static char *fmt_msgs_default[] = {
	"%f@%6c: %m<br />",
	"%f: %m<br />",
	"To %t: %m<br />",
	"%f leaves %6c<br />",
	"%f joins %6c<br />",
	"%t was kicked by %f from %6c (%m)<br />",
	"%f changes topic on %6c to %m<br />",
	"Error: %m<br />",
	"Fatal Error: %m<br />",
	"",
	"",
	"%f changes nick to %t<br />",
	"%f quits (%m)<br />",
	"Welcome to channel %6c:",
	" %f",
	" are in the channel %6c<br />",
	"%f: user(%t) host(%c) real name(%m)<br />",
	"%f: server(%c) server info(%m)<br />",
	"%f has been idle for %m seconds<br />",
	"%f is on channel %6c<br />",
	"End of whois for %f<br />",
	"%f sets voice flag of %t to %m on %6c<br />",
	"%f sets channel operator flag of %t to %m on %6c<br />",
	"banned from %6c: %m<br />",
	"end of ban list for %6c<br />",
	"You have been disconnected<br />",
	"Channel %6c has %t users and the topic is '%m'<br />",
	"End of LIST<br />",
	"Nickname %t has ident %f, realname '%m', hostname %c, ",
	"is on server %t, has flag %f, hopcount %m, and channel %c.<br />",
	"End of WHO<br />",
	"%f has invited %t to %6c<br />",
	"[notice %6c] %f: %m<br />",
	"notice from %f: %m<br />",
	"notice to %t: %m<br />",
	"%t users, %f services, %r servers<br />",
	"%r operators<br />",
	"%r unknown connections<br />",
	"%r formed channels<br />",
	"I have %t clients and %r servers<br />",
};

/* }}} */

/* {{{ Format-message accessor macros */

#define format_msg_cache(fmt, cache, chan, to, from, msg, res) \
	format_msg(fmt, chan, to, from, msg, res)
	
#define MSG(conn, type) \
	(&conn->fmt_msgs->fmt_msgs[type])

#define FORMAT_MSG(conn, type, chan, to, from, msg, res, u, ulen) {			\
	format_msg(MSG(conn, type), chan, \
			to, from, msg, res, u, ulen, NULL);								\
}

/* }}} */

/* {{{ Helper-functions */

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

/* }}} */


static int is_my_conn(php_irconn_t *conn)
{
#if 0
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);

	if (getpeername(conn->fd, &sin, &len)) {
		if (errno == EBADF || errno == ENOTSOCK || errno == ENOTCONN)
			return 0;
		else
			/* closing a connection is better than leaking fds */
			return 1;
	}

	if (sin.sin_addr.s_addr == conn->sin.sin_addr.s_addr 
			&& sin.sin_port == conn->sin.sin_port)
#endif
		return 1;
	return 0;
}

/* {{{ quit_handler */

static void quit_handler(irconn_t *c, void *dummy)
{
	php_irconn_t *conn = dummy;

	irc_quit_handlers++;
	if (conn->fd > -1) {
		zend_hash_index_del(&h_fd2irconn, conn->fd);
		if (conn->file_fd == -1)
			irc_write_buf_del(&conn->wb);
		if (is_my_conn(conn))
			shutdown(conn->fd, 2);
	}
	if (conn->file_fd != -1) {
		smart_str m = {0};

		FORMAT_MSG(conn, FMT_MSG_DISCONNECTED, NULL, NULL, NULL, NULL,
			&m, conn->conn.username, conn->conn.username_len);
		msg_send(conn, &m);

		close(conn->file_fd);
		conn->file_fd = -1;
	}
		
	conn->fd = -2;
	zend_hash_index_del(&h_irconn, conn->irconn_id);

	zend_hash_destroy(&conn->ctcp_msgs);
	smart_str_free_ex(&conn->buffer, 1);
	
#ifdef IRCG_PENDING_URL
	if (conn->od_port) {
		irc_add_pending_url(conn->od_ip, conn->od_port, conn->od_data, conn->od_len);
		free(conn->od_data);
	}
#endif
	
	free(conn);
}
/* }}} */

/* {{{ Escape functions */
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
	unsigned char *p;
	unsigned char *end;
	unsigned char c;

	p = (unsigned char *) input->c;
	end = p + input->len;

	while (p < end) {
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
		p++;
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
/* }}} */

/* {{{ cache-related stuff */

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
/* }}} */

/* {{{ ircg_mirc_color_cache */
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

/* {{{ token_compiler */

#define NEW_TOKEN(a, b) t = realloc(t, sizeof(token_t) * (++n)); t[n-1].code=a; t[n-1].para.b

static void token_compiler(const char *fmt, size_t fmtlen, format_msg_t *f)
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
	pe = fmt + fmtlen;

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
		case '5': mode |= P_COND_STOP; goto next;
		case '6': mode |= P_HTML; goto next;

		/* associate mode bits with each command where applicable */
		case 'c': NEW_TOKEN(C_CHANNEL, v) = mode; break;
		case 'd': NEW_TOKEN(C_TERMINATE_1, v) = mode; break;
		case 't': NEW_TOKEN(C_TO, v) = mode; break;
		case 'f': NEW_TOKEN(C_FROM, v) = mode; break;
		case 'r': NEW_TOKEN(C_MESSAGE, v) = mode; break;
		case 'm': NEW_TOKEN(C_MESSAGE, v) = mode | P_HTML; break;
		case 'j': NEW_TOKEN(C_MESSAGE, v) = mode | P_HTML | P_JS; break;

		case '%': NEW_TOKEN(C_PERCENT, v) = 0; break;

		default: /* ignore invalid combinations */
				  break;
		}
		p = q + 1; /* skip last format character */
	} while (p < pe);

leave_loop:
	
	f->ntoken = n;
	f->t = t;
}
/* }}} */

/* {{{ format_msg */
static void format_msg(const format_msg_t *fmt_msg, smart_str *channel, 
		smart_str *to, smart_str *from, smart_str *msg, smart_str *result, 
		const char *username, int username_len, int *status)
{
	int i = 0;
	const token_t *t = fmt_msg->t;
	int ntoken = fmt_msg->ntoken;
	smart_str tmp = {0};
	
	exec_fmt_msgs++;
	
#define IRCG_APPEND(what, use_cache) 							\
		if (t[i].para.v & P_COND_STOP) {			\
			if (username_len != what->len || memcmp(what->c, username, username_len) != 0) \
				goto stop;							\
			continue;								\
		}											\
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
		case P_HTML_JS:								\
			if (!what) break;						\
			if (use_cache) {						\
				ircg_mirc_color_cache(msg,			\
						&tmp, channel,				\
						!(t[i].para.v & P_NOAUTO_LINKS), \
						t[i].para.v & P_CONV_BR);	\
			} else {								\
				ircg_mirc_color(what->c, &tmp,		\
						what->len,					\
						!(t[i].para.v & P_NOAUTO_LINKS), \
						t[i].para.v & P_CONV_BR);	\
			}										\
			ircg_js_escape(&tmp, result);			\
			smart_str_free_ex(&tmp, 1);				\
			break;									\
		case P_HTML:								\
			if (!what) break;						\
			if (use_cache) {						\
				ircg_mirc_color_cache(msg, 			\
						result, channel,			\
						!(t[i].para.v & P_NOAUTO_LINKS), \
						t[i].para.v & P_CONV_BR);	\
			} else {								\
				ircg_mirc_color(what->c, result,	\
						what->len,					\
						!(t[i].para.v & P_NOAUTO_LINKS), \
						t[i].para.v & P_CONV_BR);	\
			}										\
			break;									\
		}

	for (; ntoken-- > 0; i++) {
		switch (t[i].code) {
		case C_STRING: smart_str_append_ex(result, t[i].para.ptr, 1); break;
		case C_FROM: IRCG_APPEND(from, 0); break;
		case C_TO: IRCG_APPEND(to, 0); break;
		case C_CHANNEL: IRCG_APPEND(channel, 0); break;
		case C_MESSAGE: IRCG_APPEND(msg, 1); break;
		case C_PERCENT: smart_str_appendc_ex(result, '%', 1); break;
		case C_TERMINATE_1: /* auth by username */
			if (ntoken > 0 && t[i+1].code == C_STRING) {
				if (t[i+1].para.s->len == from->len
						&& strncasecmp(t[i+1].para.s->c, from->c, from->len) == 0)
					*status = 1;
			} else
				*status = 1;
		}
	}

stop:	
	
	if (result->c)
		smart_str_0(result);
}
/* }}} */

/* {{{ HTTP-related */

#include "SAPI.h"

#define ADD_HEADER(a) sapi_add_header(a, sizeof(a) - 1, 1)

static void http_closed_connection(int fd)
{
	int *id, stored_id;

	if (zend_hash_index_find(&h_fd2irconn, fd, (void **) &id) == FAILURE)
		return;

	stored_id = *id;

	zend_hash_index_del(&h_fd2irconn, fd);
	zend_hash_index_del(&h_irconn, stored_id);
}

/* }}} */

static time_t ircg_now(void)
{
	struct timeval now;

#if IRCG_API_VERSION >= 20010601
	if (ircg_now_time_t != (time_t) 0) 
		return ircg_now_time_t;
	else
#endif
	gettimeofday(&now, NULL);

	return now.tv_sec;
}

#define GC_INTVL 60
#define WINDOW_TIMEOUT (3 * 60)

static const char timeout_message[] = "Timed out waiting for streaming window";

/* {{{ Message-delivery */
static void msg_accum_send(php_irconn_t *conn, smart_str *msg)
{
	int n;
	
	if (msg->c == 0) return;

	if (conn->file_fd != -1) {
		write(conn->file_fd, msg->c, msg->len);
		goto done;
	}
	
	switch (conn->fd) {
	case -2: /* Connection was finished */
		goto done;
	case -1: /* No message window yet. Buffer */
		if ((ircg_now() - conn->login) > WINDOW_TIMEOUT) {
			irc_disconnect(&conn->conn, timeout_message);
			goto done;
		}
		smart_str_append_ex(&conn->buffer, msg, 1);
		goto done;
	default:
#if IRCG_API_VERSION - 0 >= 20010601
		if ((n = irc_write_buf_append_ex(&conn->wb, msg, 0))) {
			const char *reason;
		
#if IRCG_API_VERSION - 0 >= 20020308	
			switch (n) {
			case D_OVERFLOW:
				reason = "Client is too slow, client-specific queue full";
				break;
			case D_POLL_ERROR:
				reason = "Poll failed. The connection is bad.";
				break;
			case D_CORRUPT_QUEUE:
				reason = "Internal failure: Corrupt queue";
				break;
			default:
				if (n < 0)
					reason = strerror(-n);
				else
#else
			{
#endif
					reason = "Write to HTTP client failed for no reason";
			}

			irc_disconnect(&conn->conn, reason);
		}
		return;
#elif IRCG_API_VERSION - 0 >= 20010302
		irc_write_buf_append_ex(&conn->wb, msg, 0); /* no copy */
		return;
#else
		irc_write_buf_append(&conn->wb, msg);
		goto done;
#endif
		break;
	}


done:
	smart_str_free_ex(msg, 1);
}

static void msg_send(php_irconn_t *conn, smart_str *msg)
{
	msg_accum_send(conn, msg);
	if (conn->fd != -1 && conn->file_fd == -1)
		irc_write_buf_flush(&conn->wb);
}

static void msg_replay_buffer(php_irconn_t *conn)
{
	msg_accum_send(conn, &conn->buffer);
	conn->buffer.c = NULL;
	conn->buffer.len = conn->buffer.a = 0;
}
/* }}} */

/* {{{ IRCG-handlers */
static void handle_ctcp(php_irconn_t *conn, smart_str *chan, smart_str *from,
		smart_str *msg, smart_str *result, smart_str *recipient)
{
	char *token;
	char *token_end;
	char *ctcp_arg;
	char *ctcp_arg_end;
	format_msg_t *fmt_msg;
	smart_str tmp = {0};
	int status = 0;

	token = msg->c + 1;
	token_end = strchr(token, 1);

	if (!token_end) return;

	*token_end = 0;
	
	ctcp_arg = strchr(token, ' ');
	
	if (ctcp_arg) {
		ctcp_arg_end = token_end;
		token_end = ctcp_arg;
		*token_end = 0;
		ctcp_arg++;
		smart_str_setl(&tmp, ctcp_arg, ctcp_arg_end - ctcp_arg);
	}
	
	if (zend_hash_find(&conn->ctcp_msgs, token, token_end - token, 
				(void **) &fmt_msg) != SUCCESS) {
			return;
	}
		
	format_msg(fmt_msg, chan, recipient, from, &tmp, result, 
			conn->conn.username, conn->conn.username_len, &status);

	if (status == 1) {
		irc_disconnect(&conn->conn, "Connection terminated by "
				"authenticated CTCP message");
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
		handle_ctcp(conn, chan, from, msg, &m, chan?chan:&s_username);
	} else if (chan) {
		FORMAT_MSG(conn, FMT_MSG_CHAN, chan, &s_username, from, msg, &m, conn->conn.username, conn->conn.username_len);
	} else {
		FORMAT_MSG(conn, FMT_MSG_PRIV_TO_ME, NULL, &s_username, from,
				msg, &m, conn->conn.username, conn->conn.username_len);
	}

	msg_send(conn, &m);
}

static void notice_handler(irconn_t *ircc, smart_str *chan, smart_str *from,
        smart_str *msg, void *conn_data, void *chan_data)
{
    php_irconn_t *conn = conn_data;
    smart_str m = {0};
    smart_str s_username;

    smart_str_setl(&s_username, ircc->username, ircc->username_len);

    if (msg->c[0] == '\001') {
        handle_ctcp(conn, chan, from, chan?chan:&s_username, msg, &m);
    } else if (chan) {
        FORMAT_MSG(conn, FMT_MSG_NOTICE_CHAN, chan, &s_username, from, msg, &m, conn->conn.username, conn->conn.username_len);
    } else {
        FORMAT_MSG(conn, FMT_MSG_NOTICE_TO_ME, NULL, &s_username, from,
                msg, &m, conn->conn.username, conn->conn.username_len);
    }

    msg_send(conn, &m);
}

static void nick_handler(irconn_t *c, smart_str *oldnick, smart_str *newnick,
		void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_NICK, NULL, newnick, oldnick, NULL,
			&m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void whois_user_handler(irconn_t *c, smart_str *nick, smart_str *user,
		smart_str *host, smart_str *real_name, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_WHOIS_USER, host, user, nick,
			real_name, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}


static void whois_server_handler(irconn_t *c, smart_str *nick, 
		smart_str *server, smart_str *server_info, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_WHOIS_SERVER, server, NULL, nick,
			server_info, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}


static void whois_idle_handler(irconn_t *c, smart_str *nick, 
		smart_str *idletime, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_WHOIS_IDLE, NULL, NULL, nick,
			idletime, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void end_of_whois_handler(irconn_t *c, smart_str *nick, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_WHOIS_END, NULL, NULL, nick, NULL, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}


static void whois_channels_handler(irconn_t *c, smart_str *nick, 
		smart_str *channels, int nr, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};
	int i;
	
	for (i = 0; i < nr; i++) {
		FORMAT_MSG(conn, FMT_MSG_WHOIS_CHANNEL, &channels[i], NULL, 
				nick, NULL, &m, conn->conn.username, conn->conn.username_len);
	}
	msg_send(conn, &m);
}

static void list_handler(irconn_t *c, smart_str *channel, smart_str *visible,
		smart_str *topic, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LIST, channel, visible, NULL, topic, &m, 
			conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void listend_handler(irconn_t *c, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LISTEND, NULL, NULL, NULL, NULL, &m,
			conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void whoreply_handler(irconn_t *c, smart_str *chan, smart_str *user,
        smart_str *host, smart_str *server, smart_str *nick, smart_str *flag,
        smart_str *hopcount, smart_str *realname, void *dummy,
        void *chan_data)
{
    php_irconn_t *conn = dummy;
    smart_str m = {0};

    FORMAT_MSG(conn, FMT_MSG_WHOREPLY1, host, nick, user, realname, &m,
            conn->conn.username, conn->conn.username_len);

    FORMAT_MSG(conn, FMT_MSG_WHOREPLY2, chan, server, flag, hopcount, &m,
            conn->conn.username, conn->conn.username_len);
    msg_send(conn, &m);
}

static void endofwho_handler(irconn_t *c, void *dummy)
{
    php_irconn_t *conn = dummy;
    smart_str m = {0};

    FORMAT_MSG(conn, FMT_MSG_ENDOFWHO, NULL, NULL, NULL, NULL, &m,
            conn->conn.username, conn->conn.username_len);

    msg_send(conn, &m);
}

static void invite_handler(irconn_t *c, smart_str *nick, smart_str *chan, int mode, void *dummy)
{
    php_irconn_t *conn = dummy;
    smart_str m = {0};
    smart_str *from, *to, tmp = {0};

    smart_str_setl(&tmp, conn->conn.username, conn->conn.username_len);
    if (mode == 1) {
        from = &tmp;
        to = nick;
    } else {
        from = nick;
        to = &tmp;
    }

    FORMAT_MSG(conn, FMT_MSG_INVITE, chan, to, from, NULL, &m,
            conn->conn.username, conn->conn.username_len);
    msg_send(conn, &m);
}


static void luserclient_handler(irconn_t *c, smart_str *users, smart_str *services, smart_str *servers, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LUSERCLIENT, NULL, users, services, servers, &m,
			conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void luserme_handler(irconn_t *c, smart_str *users, smart_str *servers, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LUSERME, NULL, users, NULL, servers, &m,
			conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void luserop_handler(irconn_t *c, smart_str *str, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LUSEROP, NULL, NULL, NULL, str, &m,
			conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void luserunknown_handler(irconn_t *c, smart_str *str, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LUSERUNKNOWN, NULL, NULL, NULL, str, &m,
			conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void luserchannels_handler(irconn_t *c, smart_str *str, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LUSERCHANNELS, NULL, NULL, NULL, str, &m,
			conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

/* }}} */

/* {{{ Post-connection error-storage */

/*
 * This is an internal API which serves the purpose to store the reason
 * for terminating a connection.  The termination will cause the
 * connection id to become invalid.  A script can then use a
 * function to retrieve the last error message associated with that id
 * and will usually present a nicely formatted
 * error message to the end-user.
 *
 * We automatically garbage-collect every GC_INTVL seconds, so there is
 * no need for a separate gc thread.
 */

struct errormsg {
	smart_str msg;
	int msgid;
	int id;
	time_t when;
	struct errormsg *next;
};

static time_t next_gc;
static struct errormsg *errormsgs;

static void error_msg_dtor(struct errormsg *m)
{
	smart_str_free_ex(&m->msg, 1);
	free(m);
}

static void error_msg_gc(time_t now)
{
	struct errormsg *m, *prev = NULL, *next;
	time_t lim;

	lim = now - GC_INTVL;
	next_gc = now + GC_INTVL;
	
	for (m = errormsgs; m; prev = m, m = m->next) {
		if (m->when < lim) {
			struct errormsg *to;
			/* Check whether we have subsequent outdated records */
			
			for (to = m->next; to; to = next) {
				next = to->next;
				if (m->when >= lim) break;
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

	m->when = ircg_now();
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
	int disconn = 0;

	if (conn->bailout_on_trivial) {
		if (id == 474 || id == 475 || id == 471 || id == 473) {
			fatal = disconn = 1;
		}
	}
	
	smart_str_setl(&tmp, "IRC SERVER", sizeof("IRC SERVER") - 1);
	smart_str_setl(&s_username, ircc->username, ircc->username_len);
	FORMAT_MSG(conn, fatal ? FMT_MSG_FATAL_ERROR : FMT_MSG_ERROR, NULL, 
			&s_username, &tmp, msg, &m, conn->conn.username, conn->conn.username_len);

	if (fatal) {
		add_error_msg(msg, id, conn);
	}
	
	msg_send(conn, &m);

	/* Fatal messages from the IRCG layer automatically call irc_disconnect; 
	   if we simulate a fatal error, we need to do that manually */
	if (disconn)
		irc_disconnect(ircc, "A fatal error occured");
}
/* }}} */

/* {{{ IRCG-handlers */
static void banlist_handler(irconn_t *ircc, smart_str *channel, smart_str *mask, void *conn_data)
{
	php_irconn_t *conn = conn_data;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_BANLIST, channel, NULL, NULL, mask, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void end_of_banlist_handler(irconn_t *ircc, smart_str *channel, void *conn_data)
{
	php_irconn_t *conn = conn_data;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_BANLIST_END, channel, NULL, NULL, NULL, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void user_add_single(php_irconn_t *conn, smart_str *channel, smart_str *users)
{
	smart_str m = {0};
	FORMAT_MSG(conn, FMT_MSG_JOIN, channel, NULL, &users[0],
			NULL, &m, conn->conn.username, conn->conn.username_len);
	FORMAT_MSG(conn, FMT_MSG_JOIN_LIST_END, channel, NULL, NULL,
		NULL, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void user_add_multiple(php_irconn_t *conn, smart_str *channel, smart_str *users, int nr)
{
	int i;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_MASS_JOIN_BEGIN, channel, NULL, NULL,
			NULL, &m, conn->conn.username, conn->conn.username_len);
	for (i = 0; i < nr; i++) {
		FORMAT_MSG(conn, FMT_MSG_MASS_JOIN_ELEMENT, channel, NULL,
				&users[i], NULL, &m, conn->conn.username, conn->conn.username_len);
	}

	FORMAT_MSG(conn, FMT_MSG_MASS_JOIN_END, channel, NULL, NULL,
			NULL, &m, conn->conn.username, conn->conn.username_len);


	msg_send(conn, &m);
}

#if IRCG_API_VERSION >= 20021109

static void user_add_ex(irconn_t *ircc, smart_str *channel, smart_str *users,
		int nr, int namelist, void *dummy)
{
	if (namelist) {
		user_add_multiple(dummy, channel, users, nr);
	} else {
		user_add_single(dummy, channel, users);
	}
}

#else

static void user_add(irconn_t *ircc, smart_str *channel, smart_str *users,
		int nr, void *dummy)
{
	if (nr > 1) {
		user_add_multiple(dummy, channel, users, nr);
	} else {
		user_add_single(dummy, channel, users);
	}
}

#endif

static void new_topic(irconn_t *ircc, smart_str *channel, smart_str *who, smart_str *topic, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_TOPIC, channel, NULL, who, topic, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void part_handler(irconn_t *ircc, smart_str *channel, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};
	smart_str s_username;

	smart_str_setl(&s_username, ircc->username, ircc->username_len);

	FORMAT_MSG(conn, FMT_MSG_SELF_PART, channel, NULL, &s_username, 
			NULL, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void user_leave(irconn_t *ircc, smart_str *channel, smart_str *user, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_LEAVE, channel, NULL, user, NULL, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void user_quit(irconn_t *ircc, smart_str *user, smart_str *msg, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	FORMAT_MSG(conn, FMT_MSG_QUIT, NULL, NULL, user, msg, &m, conn->conn.username, conn->conn.username_len);
	msg_send(conn, &m);
}

static void idle_recv_queue(irconn_t *ircc, void *dummy)
{
	php_irconn_t *conn = dummy;

	if (conn->fd >= 0)
		send(conn->fd, "  ", 2, 0);
	else if (conn->file_fd < 0 && (ircg_now() - conn->login) > WINDOW_TIMEOUT) {
		char buf[1024];

		sprintf(buf, "timeout after %ld seconds (%ld, %ld)", ircg_now()-conn->login,
				ircg_now(), conn->login);
		irc_disconnect(ircc, buf);
	}
}

static void user_kick(irconn_t *ircc, smart_str *channel, smart_str *who, smart_str *kicked_by, smart_str *reason, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	if (conn->bailout_on_trivial && who->len == ircc->username_len && memcmp(who->c, ircc->username, who->len) == 0) {
		irc_disconnect(ircc, "Bailout on trivial: KICK");
	}
	FORMAT_MSG(conn, FMT_MSG_KICK, channel, who, kicked_by, reason, &m, conn->conn.username, conn->conn.username_len);
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
		FORMAT_MSG(conn, FMT_MSG_MODE_VOICE, channel, nick, who, &what, 
				&m, conn->conn.username, conn->conn.username_len);
		msg_send(conn, &m);
	}
	if (mode & IRCG_MODE_OP) {
		FORMAT_MSG(conn, FMT_MSG_MODE_OP, channel, nick, who, &what, &m, conn->conn.username, conn->conn.username_len);
		msg_send(conn, &m);
	}
}
#endif
/* }}} */

/* {{{ proto bool ircg_set_on_die(int connection, string host, int port, string data) 
   Sets hostaction to be executed when connection dies */
PHP_FUNCTION(ircg_set_on_die)
{
#ifdef IRCG_PENDING_URL
	zval **p1, **p2, **p3, **p4;
	php_irconn_t *conn;
	
	if (ZEND_NUM_ARGS() != 4 || zend_get_parameters_ex(4, &p1, &p2, &p3, &p4) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	convert_to_string_ex(p2);
	convert_to_long_ex(p3);
	convert_to_string_ex(p4);

	if (inet_aton(Z_STRVAL_PP(p2), &conn->od_ip) == 0)
		RETURN_FALSE;

	conn->od_port = Z_LVAL_PP(p3);
	conn->od_data = malloc(Z_STRLEN_PP(p4));
	memcpy(conn->od_data, Z_STRVAL_PP(p4), Z_STRLEN_PP(p4));
	conn->od_len = Z_STRLEN_PP(p4);

	RETURN_TRUE;
#endif
}
/* }}} */

/* {{{ proto string ircg_get_username(int connection)
   Gets username for connection */
PHP_FUNCTION(ircg_get_username)
{
	zval **p1;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;
	
	convert_to_long_ex(p1);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;

	RETVAL_STRINGL((char *) conn->conn.username, conn->conn.username_len, 1);
}
/* }}} */

/* {{{ proto bool ircg_set_file(int connection, string path)
   Sets logfile for connection */
PHP_FUNCTION(ircg_set_file)
{
	zval **p1, **p2;
	php_irconn_t *conn;
	
	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &p1, &p2) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;

	if (conn->fd != -1) {
		php_error(E_WARNING, "%s(): Called after a call to ircg_set_current(). You must set the output filename before opening the persistent HTTP connection which is kept alive.", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	if (conn->file_fd != -1) 
		close(conn->file_fd);
	
	conn->file_fd = open(Z_STRVAL_PP(p2), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
	SEEN_FD(conn->file_fd);
	if (conn->file_fd == -1) {
		RETURN_FALSE;
	}
	
#ifdef F_SETFD
	if (fcntl(conn->file_fd, F_SETFD, 1)) {
		close(conn->file_fd);
		conn->file_fd = -1;
		RETURN_FALSE;
	}
#endif
	
	msg_replay_buffer(conn);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool ircg_set_current(int connection)
   Sets current connection for output */
PHP_FUNCTION(ircg_set_current)
{
#ifdef IRCG_WITH_THTTPD
	zval **p1;
	php_irconn_t *conn;
#if 0
	socklen_t len = sizeof(struct sockaddr_in);
#endif

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;

	if (conn->fd >= 0) {
		/* There is a HTTP connection alive, kill it */
		zend_hash_index_del(&h_fd2irconn, conn->fd);
		if (conn->file_fd == -1)
			irc_write_buf_del(&conn->wb);
		if (is_my_conn(conn))
			shutdown(conn->fd, 2);
	}

#if 0
	/* store peer information for safe shutdown */
	if (getpeername(conn->fd, &conn->sin, &len) == -1) {
		php_error(E_WARNING, "getpeername failed: %d (%s)", errno, strerror(errno));
	}
#endif

	irc_set_currents++;
	thttpd_register_on_close(http_closed_connection);
	thttpd_set_dont_close();
	conn->fd = thttpd_get_fd();
	SEEN_FD(conn->fd);
	if (fcntl(conn->fd, F_GETFL) == -1) {
		zend_hash_index_del(&h_irconn, Z_LVAL_PP(p1));
		php_error(E_WARNING, "current fd is not valid");
		RETURN_FALSE;
	}
	zend_hash_index_update(&h_fd2irconn, conn->fd, &Z_LVAL_PP(p1), sizeof(int), NULL);
	if (conn->file_fd == -1) {
		flush_data = conn;
		irc_write_buf_add(&conn->wb, conn->fd);
	}

	RETURN_TRUE;
#endif
}
/* }}} */

/* {{{ proto string ircg_nickname_escape(string nick) 
   Escapes special characters in nickname to be IRC-compliant */
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
/* }}} */

/* {{{ proto string ircg_nickname_unescape(string nick)
   Decodes encoded nickname */
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
/* }}} */

/* {{{ proto bool ircg_join(int connection, string channel [, string chan-key])
   Joins a channel on a connected server */
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
/* }}} */

/* {{{ proto bool ircg_oper(int connection, string name, string password)
   Elevates privileges to IRC OPER */
PHP_FUNCTION(ircg_oper)
{
	zval **p1, **p2, **p3;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 3
			|| zend_get_parameters_ex(ZEND_NUM_ARGS(), &p1, &p2, &p3) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);
	convert_to_string_ex(p3);
	
	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;

	irc_handle_command(&conn->conn, "OPER", 2, Z_STRVAL_PP(p2), Z_STRVAL_PP(p3));	

	RETVAL_TRUE;
}
/* }}} */

/* {{{ proto bool ircg_whois(int connection, string nick)
   Queries user information for nick on server */
PHP_FUNCTION(ircg_whois)
{
#if IRCG_API_VERSION - 0 >= 20010227
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
/* }}} */

/* {{{ proto bool ircg_who(int connection, string mask [, bool ops_only])
   Queries server for WHO information */
#if IRCG_API_VERSION >= 20021115
PHP_FUNCTION(ircg_who)
{
    int ops = 0;
    zval **p1, **p2, **p3;
    php_irconn_t *conn;

    if (ZEND_NUM_ARGS() < 2 || ZEND_NUM_ARGS() > 3
            || zend_get_parameters_ex(ZEND_NUM_ARGS(), &p1, &p2, &p3) == FAILURE)
        WRONG_PARAM_COUNT;

    convert_to_long_ex(p1);
    convert_to_string_ex(p2);

    if (ZEND_NUM_ARGS() > 2) {
        convert_to_boolean_ex(p3);
        ops = Z_BVAL_PP(p3);
    }

    conn = lookup_irconn(Z_LVAL_PP(p1));

    if (!conn) RETURN_FALSE;

    irc_handle_command(&conn->conn, "WHO", ops ? 2 : 1, Z_STRVAL_PP(p2), "o");

    RETVAL_TRUE;
}
#endif
/* }}} */

/* {{{ proto bool ircg_invite(int connection, string channel, string nickname)
   INVITEs nickname to channel */
#if IRCG_API_VERSION >= 20021117
PHP_FUNCTION(ircg_invite)
{
    zval **p1, **p2, **p3;
    php_irconn_t *conn;

    if (ZEND_NUM_ARGS() != 3
            || zend_get_parameters_ex(3, &p1, &p2, &p3) == FAILURE)
        WRONG_PARAM_COUNT;

    convert_to_long_ex(p1);
    convert_to_string_ex(p2);
    convert_to_string_ex(p3);

    conn = lookup_irconn(Z_LVAL_PP(p1));

    if (!conn) RETURN_FALSE;

    irc_handle_command(&conn->conn, "INVITE", 2, Z_STRVAL_PP(p3),
            Z_STRVAL_PP(p2));

    RETVAL_TRUE;
}
#endif
/* }}} */


/* {{{ proto bool ircg_names( int connection, string channel [, string target])
   Queries visible usernames */
PHP_FUNCTION(ircg_names)
{
	zval **p1, **p2, **p3;
	php_irconn_t *conn;
	int ac = ZEND_NUM_ARGS();
 
	if (ac < 2 || ac > 3 || zend_get_parameters_ex(ac, &p1, &p2, &p3) == FAILURE)

		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);
	
    if (ac > 2) {
        convert_to_string_ex(p3);
    }

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_handle_command(&conn->conn, "NAMES", ac > 2 ? 2 : 1, Z_STRVAL_PP(p2), ac > 2 ? Z_STRVAL_PP(p3) : NULL);
	RETVAL_TRUE;
}
/* }}} */

/* {{{ proto bool ircg_ignore_add(int connection, string nick)
   Adds a user to your ignore list on a server */
PHP_FUNCTION(ircg_ignore_add)
{
#if IRCG_API_VERSION - 0 >= 20010402
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

/* {{{ proto array ircg_fetch_error_msg(int connection)
   Returns the error from previous ircg operation */
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

/* {{{ proto bool ircg_ignore_del(int connection, string nick)
   Removes a user from your ignore list */
PHP_FUNCTION(ircg_ignore_del)
{
#if IRCG_API_VERSION - 0 >= 20010402
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

/* {{{ proto bool ircg_channel_mode(int connection, string channel, string mode_spec, string nick)
   Sets channel mode flags for user */
PHP_FUNCTION(ircg_channel_mode)
{
#if IRCG_API_VERSION - 0 >= 20010227
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
	
	irc_handle_command(&conn->conn, "MODE", Z_STRLEN_PP(args[3]) > 0 ? 3 : 2, 
			Z_STRVAL_PP(args[1]),
			Z_STRVAL_PP(args[2]), Z_STRVAL_PP(args[3]));
	RETVAL_TRUE;
#endif
}
/* }}} */

/* {{{ proto bool ircg_topic(int connection, string channel, string topic)
   Sets topic for channel */
PHP_FUNCTION(ircg_topic)
{
#if IRCG_API_VERSION - 0 >= 20010226
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

/* {{{ proto string ircg_html_encode(string html_text)
   Encodes HTML preserving output */
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

/* {{{ proto bool ircg_kick(int connection, string channel, string nick, string reason)
   Kicks user from channel */
PHP_FUNCTION(ircg_kick)
{
#if IRCG_API_VERSION - 0 >= 20010226
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

/* {{{ proto bool ircg_lusers(int connection)
   IRC network statistics */
PHP_FUNCTION(ircg_lusers)
{
	zval **p1;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_handle_command(&conn->conn, "LUSERS", 0);
	RETVAL_TRUE;
}
/* }}} */


/* {{{ proto bool ircg_part(int connection, string channel)
   Leaves a channel */
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

/* {{{ proto bool ircg_is_conn_alive(int connection)
   Checks connection status */
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

/* {{{ proto bool ircg_lookup_format_messages(string name)
   Selects a set of format strings for display of IRC messages */
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

/* {{{ proto bool ircg_register_format_messages(string name, array messages)
   Registers a set of format strings for display of IRC messages */
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
		php_error(E_WARNING, "%s(): The second parameter should be an array", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	h = HASH_OF(*p2);

	for (i = 0; i < NO_FMTS; i++) {
		if (zend_hash_index_find(h, i, (void **) &arg) == SUCCESS) {
			convert_to_string_ex(arg);
			token_compiler(Z_STRVAL_PP(arg), Z_STRLEN_PP(arg), &fmt_msgs.fmt_msgs[i]);
		} else
			token_compiler("", 0, &fmt_msgs.fmt_msgs[i]);
	}

	
	zend_hash_update(&h_fmt_msgs, Z_STRVAL_PP(p1), Z_STRLEN_PP(p1), 
		&fmt_msgs, sizeof(fmt_msgs), NULL);

	RETVAL_TRUE;	
}
/* }}} */

/* {{{ register_hooks */
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

#if IRCG_API_VERSION - 0 >= 20010225
	if (irconn->realname) {
		smart_str m;
		smart_str_sets(&m, irconn->realname);
		irc_set_realname(conn, &m);
	}
#endif

#define MSG_NOT_EMPTY(n) (MSG(irconn,n) && MSG(irconn,n)->ntoken != 0)

#define IFMSG(n, p, q) if (MSG_NOT_EMPTY(n)) irc_register_hook(conn, p, q)
	
	irc_register_hook(conn, IRCG_MSG, msg_handler);
	irc_register_hook(conn, IRCG_QUIT, quit_handler);
	irc_register_hook(conn, IRCG_ERROR, error_handler);
	IFMSG(FMT_MSG_NICK, IRCG_NICK, nick_handler);

	IFMSG(FMT_MSG_SELF_PART, IRCG_PART, part_handler);
	IFMSG(FMT_MSG_LEAVE, IRCG_USER_LEAVE, user_leave);
	IFMSG(FMT_MSG_KICK, IRCG_USER_KICK, user_kick);
	IFMSG(FMT_MSG_QUIT, IRCG_USER_QUIT, user_quit);
	IFMSG(FMT_MSG_TOPIC, IRCG_TOPIC, new_topic);

#if IRCG_API_VERSION - 0 >= 20010227
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

#if IRCG_API_VERSION >= 20020922
	/* RPL_LIST/RPL_LISTEND */
	irc_register_hook(conn, IRCG_LIST, list_handler);
	irc_register_hook(conn, IRCG_LISTEND, listend_handler);
#endif

#if IRCG_API_VERSION >= 20021109
	IFMSG(FMT_MSG_MASS_JOIN_ELEMENT, IRCG_USER_ADD_EX, user_add_ex);
#else	
	IFMSG(FMT_MSG_MASS_JOIN_ELEMENT, IRCG_USER_ADD, user_add);
#endif

#if IRCG_API_VERSION >= 20021115
	if (MSG_NOT_EMPTY(FMT_MSG_WHOREPLY1)
			|| MSG_NOT_EMPTY(FMT_MSG_WHOREPLY2)) {
		irc_register_hook(conn, IRCG_WHOREPLY, whoreply_handler);
	}
	IFMSG(FMT_MSG_ENDOFWHO, IRCG_ENDOFWHO, endofwho_handler);
#endif

#if IRCG_API_VERSION >= 20021117
	IFMSG(FMT_MSG_INVITE, IRCG_INVITE, invite_handler);
#endif

	irc_register_hook(conn, IRCG_NOTICE, notice_handler);

	IFMSG(FMT_MSG_LUSERCLIENT, IRCG_LUSERCLIENT, luserclient_handler);
	IFMSG(FMT_MSG_LUSERME, IRCG_LUSERME, luserme_handler);
	IFMSG(FMT_MSG_LUSEROP, IRCG_LUSEROP, luserop_handler);
	IFMSG(FMT_MSG_LUSERUNKNOWN, IRCG_LUSERUNKNOWN, luserunknown_handler);
	IFMSG(FMT_MSG_LUSERCHANNELS, IRCG_LUSERCHANNELS, luserchannels_handler);
}
/* }}} */

static void ctcp_msgs_dtor(format_msg_t *fmt)
{
	free_fmt_msg(fmt);
}

/* {{{ ircg_copy_ctcp_msgs */
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
		token_compiler(Z_STRVAL_PP(val), Z_STRLEN_PP(val), &fmt);
		zend_hash_add(&conn->ctcp_msgs, str, str_len - 1, &fmt,
				sizeof(fmt), NULL);
		
		zend_hash_move_forward_ex(Z_ARRVAL_PP(array), &pos);
	}
	
	return 0;
}
/* }}} */

/* {{{ proto int ircg_pconnect(string username [, string server [, int port [, string format-msg-set-name [, array ctcp-set [, array user-details [, bool bailout-on-trivial]]]]]])
   Create a persistent IRC connection */
PHP_FUNCTION(ircg_pconnect)
{
	/* This should become an array very soon */
	zval **p1, **p2, **p3, **p4 = NULL, **p5 = NULL, **p6, **p7;
	const char *username = 0;
	const char *server = "0";
	int port = 6667;
	php_fmt_msgs_t *fmt_msgs = NULL;	
	php_irconn_t *conn;
	int bailout_on_trivial = 1;
	
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 7 
			|| zend_get_parameters_ex(ZEND_NUM_ARGS(), &p1, &p2, &p3, &p4, &p5, &p6, &p7) == FAILURE)
		WRONG_PARAM_COUNT;

	switch (ZEND_NUM_ARGS()) {
	case 7:
		convert_to_long_ex(p7);
		bailout_on_trivial = Z_LVAL_PP(p7);
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
	conn->fd = conn->file_fd = -1;
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

	conn->bailout_on_trivial = bailout_on_trivial;
#ifdef IRCG_PENDING_URL
	conn->od_port = 0;
#endif
	conn->fmt_msgs = fmt_msgs;	
	if (irc_connect(username, register_hooks, 
			conn, server, port, &conn->conn)) {
		free(conn);
		php_error(E_WARNING, "%s(): irc_connect() failed prematurely", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}
	irc_connects++;
	zend_hash_init(&conn->ctcp_msgs, 10, NULL, (dtor_func_t) ctcp_msgs_dtor, 1);
	if (p5 && Z_TYPE_PP(p5) == IS_ARRAY) {
		ircg_copy_ctcp_msgs(p5, conn);
	}
	conn->password = conn->ident = NULL;
	if (irconn_id == 0)
		irconn_id = 10000.0 * php_combined_lcg(TSRMLS_C);
	else
		irconn_id += 20.0 * (1.0 + php_combined_lcg(TSRMLS_C));
	
	
	while (zend_hash_index_exists(&h_irconn, irconn_id)) {
		irconn_id++;
	}
	
	conn->irconn_id = irconn_id;
	zend_hash_index_update(&h_irconn, irconn_id, &conn, sizeof(conn), NULL);
	conn->buffer.c = NULL;
	conn->login = ircg_now();
		
	if (conn->login >= next_gc)
		error_msg_gc(conn->login);

	RETVAL_LONG(irconn_id);
}
/* }}} */

/* {{{ proto bool ircg_disconnect(int connection, string reason)
   Terminate IRC connection */
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

/* {{{ proto bool ircg_nick(int connection, string newnick)
   Changes the nickname */
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

/* {{{ proto bool ircg_list(int connection, string channel)
   List topic/user count of channel(s) */
PHP_FUNCTION(ircg_list)
{
	zval **id, **p2;
	php_irconn_t *conn;
	int ac = ZEND_NUM_ARGS();
	
	if (ac != 2 || zend_get_parameters_ex(ac, &id, &p2) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(id);
	convert_to_string_ex(p2);

	conn = lookup_irconn(Z_LVAL_PP(id));

	if (!conn) RETURN_FALSE;
	
	irc_handle_command(&conn->conn, "LIST", 1, Z_STRVAL_PP(p2));
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool ircg_notice(int connection, string recipient, string message)
   Sends a one-way communication NOTICE to a target */
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

/* {{{ proto array ircg_eval_ecmascript_params(string params)
   Decodes a list of JS-encoded parameters into a native array */

#define ADD_PARA() do { \
				if (para.len) smart_str_0(&para); \
				add_next_index_stringl(return_value, \
						para.len == 0 ? empty_string : para.c, \
						para.len, 0); \
				para.len = 0; \
				para.c = 0; \
} while (0)

PHP_FUNCTION(ircg_eval_ecmascript_params)
{
	zval **str;
	int s;
	unsigned char *ptr, *ptre;
	unsigned char c;
	smart_str para = {0};

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &str) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string_ex(str);

	array_init(return_value);

	ptr = Z_STRVAL_PP(str);
	ptre = ptr + Z_STRLEN_PP(str);

	s = 0;
	
	for (; ptr < ptre; ptr++) {
		c = *ptr;
		switch (s) {

		/*
		 * State 0: Looking for ' or digit
		 * State 1: Assembling parameter inside '..'
		 * State 2: After escape sign: Copies single char verbatim, go to 1
		 * State 3: Assembling numeric para, no quotation
		 * State 4: Looking for ",", skipping whitespace
		 */

		case 0:
			switch (c) {
			case '\'':
				s = 1;
				para.len = 0;
				break;

			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				s = 3;
				smart_str_appendc(&para, c);
				break;

			default: /* erroneous */
				return;
			}
			break;

		case 1:
			switch (c) {
			case '\\':
				s = 2;
				break;

			case '\'':
				s = 4;
				ADD_PARA();
				break;

			default:
				smart_str_appendc(&para, c);
				break;
			}
			break;

		case 2:
			smart_str_appendc(&para, c);
			s = 1;
			break;

		case 3:
			switch (c) {
			case ',':
				s = 0;
				ADD_PARA();
				break;

			default:
				smart_str_appendc(&para, c);
				break;
			}
			break;

		case 4:
			switch (c) {
			case ',':
				s = 0;
				break;
			}
			break;
		}
	}

	if (para.len != 0) {
		if (s == 3)
			ADD_PARA();
		else
			smart_str_free(&para);
	}
}
/* }}} */

/* {{{ proto bool ircg_msg(int connection, string recipient, string message [,bool loop-suppress])
   Delivers a message to the IRC network */
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
					handle_ctcp(conn, &tmp, &tmp2, &l, &m, &tmp);
				} else {
					FORMAT_MSG(conn, FMT_MSG_CHAN, &tmp, NULL, &tmp2, &l, &m, conn->conn.username, conn->conn.username_len);
				}
				break;
			default:
				if (l.c[0] == 1) {
					handle_ctcp(conn, NULL, &tmp2, &l, &m, &tmp);
				} else {
					FORMAT_MSG(conn, FMT_MSG_PRIV_FROM_ME, NULL,
							&tmp, &tmp2, &l, &m, conn->conn.username, conn->conn.username_len);
				}
		}

		msg_send(conn, &m);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN
 */
PHP_RSHUTDOWN_FUNCTION(ircg)
{
	if (flush_data) {
		php_irconn_t *conn = flush_data;

		msg_replay_buffer(conn);
		irc_write_buf_flush(&conn->wb);
		
		flush_data = NULL;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ ircg_module_entry */
zend_module_entry ircg_module_entry = {
	STANDARD_MODULE_HEADER,
	"ircg",
	ircg_functions,
	PHP_MINIT(ircg),
	PHP_MSHUTDOWN(ircg),
	NULL,
	PHP_RSHUTDOWN(ircg),
	PHP_MINFO(ircg),
	NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_IRCG
ZEND_GET_MODULE(ircg)
#endif
/* }}} */

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
		token_compiler(fmt_msgs_default[i], strlen(fmt_msgs_default[i]), &fmt_msgs_default_compiled.fmt_msgs[i]);
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
	
	sprintf(buf, "%d", getdtablesize());
	php_info_print_table_row(2, "Maximum number of open fds (system limit)", buf);
	sprintf(buf, "%d", highest_fd);
	php_info_print_table_row(2, "Highest encountered fd", buf);

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
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
