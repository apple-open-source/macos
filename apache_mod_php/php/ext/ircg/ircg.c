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

/* $Id: ircg.c,v 1.1.1.2 2001/07/19 00:19:19 zarzycki Exp $ */

#include <time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
 
#include "php.h"
#include "php_ini.h"
#include "php_ircg.h"
#include "ext/standard/html.h"

#include "ext/standard/php_smart_str.h"
#include "ext/standard/info.h"

/* If you declare any globals in php_ircg.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(ircg)
*/

/* This module is not intended for thread-safe use */

static HashTable h_fd2irconn; /* fd's to Integer IDs */
static HashTable h_irconn; /* Integer IDs to php_irconn_t * */
static HashTable h_fmt_msgs; /* Integer IDs to php_fmt_msgs_t * */ 
static int irconn_id = 1;

static unsigned long irc_connects, irc_set_currents, irc_quit_handlers;

/* Format string numbers */
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

/* Every user visible function must have an entry in ircg_functions[].
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
	PHP_FE(ircg_ignore_del, NULL)
	PHP_FE(ircg_disconnect, NULL)
	PHP_FE(ircg_is_conn_alive, NULL)
	PHP_FE(ircg_lookup_format_messages, NULL)
	PHP_FE(ircg_register_format_messages, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in ircg_functions[] */
};

#include "if_irc.h"
#include "irc_write_buffer.h"

#if !defined(IRCG_API_VERSION) || IRCG_API_VERSION < 20010303
# error "Please upgrade to at least IRCG 2.0b1"
#endif

typedef struct {
	char *fmt_msgs[NO_FMTS];
} php_fmt_msgs_t;

typedef struct {
	irconn_t conn;
	zend_llist buffer;
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

#define MSG(conn, type) \
	(conn->fmt_msgs&&conn->fmt_msgs->fmt_msgs[type])?conn->fmt_msgs->fmt_msgs[type]:fmt_msgs_default[type]

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

static void fmt_msgs_dtor(void *dummy)
{
	php_fmt_msgs_t *fmt_msgs = dummy;
	int i;

	for (i = 0; i < NO_FMTS; i++) {
		if (fmt_msgs->fmt_msgs[i]) free(fmt_msgs->fmt_msgs[i]);
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
	zend_llist_destroy(&conn->buffer);
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
void ircg_mirc_color(const char *, smart_str *, size_t);

#define NR_CACHE_ENTRIES 10

static unsigned long cache_hits, cache_misses;

struct {
	smart_str src;
	smart_str result;
	int score;
} static cache_entries[NR_CACHE_ENTRIES];

void ircg_mirc_color_cache(smart_str *src, smart_str *result,
		smart_str *channel)
{
	/* We only cache messages in the context of a channel */
	if (channel) {
		int i;
		int least_used_slot = 0;
		int least_used_score = 50;

		/* Score system.  Initially, each slot gets 100 points.
		 * If a slot result is used, the slot earns three points.
		 * If a slot is not used but looked at, it gets punished.
		 * Slots with less than 50 points are considered for recycling,
		 * when the cache is full.  
		 * This should ensure that new entries are not thrown out too
		 * early and that old entries are expired appropiately.
		 */
		
		for (i = 0; i < NR_CACHE_ENTRIES && cache_entries[i].score; i++) {
			/* case-sensitive comparison */
			if (cache_entries[i].src.len == src->len &&
					memcmp(cache_entries[i].src.c, src->c, src->len) == 0) {
				cache_entries[i].score += 3;
				cache_hits++;
				smart_str_append_ex(result, &cache_entries[i].result, 1);
				return;
			}
			
			/* old entries will "expire" */
			if (cache_entries[i].score > 1)
				cache_entries[i].score--;
			
			/* looks like this is the least used entry up to now */
			if (cache_entries[i].score < least_used_score) {
				least_used_score = cache_entries[i].score;
				least_used_slot = i;
			}
		}

		/* cache is full */
		if (i == NR_CACHE_ENTRIES)
			i = least_used_slot;

		cache_misses++;
		cache_entries[i].score = 100;
		cache_entries[i].src.len = 0;
		cache_entries[i].result.len = 0;
		ircg_mirc_color(src->c, &cache_entries[i].result, src->len);
		
		smart_str_append_ex(&cache_entries[i].src, src, 1);
		smart_str_append_ex(result, &cache_entries[i].result, 1);
	} else {
		/* No channel message, no caching */
		ircg_mirc_color(src->c, result, src->len);
	}
}

static void format_msg(const char *fmt, smart_str *channel, smart_str *to, smart_str *from, smart_str *msg, smart_str *result)
{
	const char *p;
	char c;
	char *q;
	smart_str encoded_msg = {0};
	smart_str js_encoded_msg = {0};
	int encoded = 0;
	int js_encoded = 0;
	unsigned long len;
	int mod_encode;
	int nickname_decode;
	
	if (fmt[0] == '\0') {
		return;
	}

	p = fmt;
	while (q = strchr(p, '%')) {
		len = q - p; 
		if (len > 0)
			smart_str_appendl_ex(result, p, len, 1);
		
		
		nickname_decode = mod_encode = 0;


#define IRCG_APPEND(what) \
		if (mod_encode && nickname_decode) { \
			smart_str tmp = {0}; \
			ircg_nickname_unescape(what, &tmp); \
			ircg_js_escape(&tmp, result); \
			smart_str_free_ex(&tmp, 1); \
		} else if (mod_encode) { \
			ircg_js_escape(what, result); \
		} else if (nickname_decode) { \
			ircg_nickname_unescape(what, result); \
		} else { \
			smart_str_append_ex(result, what, 1); \
		}

again:
		c = q[1];
		
		switch (c) {
		case '1':
			mod_encode = 1;
			q++;
			goto again;
		case '2':
			nickname_decode = 1;
			q++;
			goto again;
		case 'c':
			IRCG_APPEND(channel);
			break;
		case 't':
			IRCG_APPEND(to);
			break;
		case 'f':
			IRCG_APPEND(from);
			break;
		case 'j':
append_js_encoded_msg:
			if (!encoded) {
				ircg_mirc_color_cache(msg, &encoded_msg, channel);
				encoded = 1;
			}
			if (!js_encoded) {
				ircg_js_escape(&encoded_msg, &js_encoded_msg);
				js_encoded = 1;
			}
			smart_str_append_ex(result, &js_encoded_msg, 1);
			break;
		case 'm':
			if (mod_encode) goto append_js_encoded_msg;
			if (!encoded) {
				ircg_mirc_color_cache(msg, &encoded_msg, channel);
				encoded = 1;
			}
			smart_str_append_ex(result, &encoded_msg, 1);
			break;
		case '%':
			smart_str_appendc_ex(result, c, 1);
			break;
		case 'r':
			IRCG_APPEND(msg);
			break;
		case 0:
			goto finish_loop;
		default:
			smart_str_appendc_ex(result, '%', 1);
			smart_str_appendc_ex(result, c, 1);
		}
		p = q + 2;
	}

finish_loop:
	if (*p) {
		smart_str_appends_ex(result, p, 1);
	}

	if (encoded)
		smart_str_free_ex(&encoded_msg, 1);
	if (js_encoded)
		smart_str_free_ex(&js_encoded_msg, 1);

	smart_str_0(result);
}

#include "SAPI.h"

#define ADD_HEADER(a) sapi_add_header(a, sizeof(a) - 1, 1)

static void msg_http_start(php_irconn_t *conn)
{
	ADD_HEADER("Pragma: no-cache");
	sapi_send_headers();
}

static void msg_empty_buffer(php_irconn_t *conn)
{
	zend_llist_clean(&conn->buffer);
}
	
static void http_closed_connection(int fd);

static void msg_accum_send(php_irconn_t *conn, smart_str *msg)
{
	if (msg->c == 0) return;

	if (conn->fd != -1) {
#if defined(IRCG_API_VERSION) && IRCG_API_VERSION >= 20010302
		irc_write_buf_append_ex(&conn->wb, msg, 0);
#else
		irc_write_buf_append(&conn->wb, msg);
		smart_str_free_ex(msg, 1);
#endif
	} else if (conn->fd == -2) {
		smart_str_free_ex(msg, 1);
	} else {
		if (conn->buffer_count++ > 50) {
			time_t now = time(NULL);

			smart_str_free_ex(msg, 1);
			if ((now - conn->login) > 20)
				irc_disconnect(&conn->conn, "Timed out waiting for window");
			return;
		}
		zend_llist_add_element(&conn->buffer, msg);
	}
}

static void msg_send(php_irconn_t *conn, smart_str *msg)
{
	msg_accum_send(conn, msg);
	if (conn->fd != -1)
		irc_write_buf_flush(&conn->wb);
}

static void msg_send_apply(void *data, void *arg)
{
	msg_accum_send(arg, data);
}

static void msg_replay_buffer(php_irconn_t *conn)
{
	zend_llist_apply_with_argument(&conn->buffer, msg_send_apply, conn);
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
			char *fmt_msg;
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
		format_msg(MSG(conn, FMT_MSG_CHAN), chan, &s_username, from, msg, &m);
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
		format_msg(MSG(conn, FMT_MSG_JOIN), channel, NULL, &users[0],
				NULL, &m);
		format_msg(MSG(conn, FMT_MSG_JOIN_LIST_END), channel, NULL, NULL,
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

	irc_set_currents++;
	thttpd_register_on_close(http_closed_connection);
	thttpd_set_dont_close();
	conn->fd = thttpd_get_fd();
	zend_hash_index_update(&h_fd2irconn, conn->fd, &Z_LVAL_PP(p1), sizeof(int), NULL);
	irc_write_buf_add(&conn->wb, conn->fd);
	msg_http_start(conn);
	msg_replay_buffer(conn);
	irc_write_buf_flush(&conn->wb);
	msg_empty_buffer(conn);

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
	zval **p1, **p2;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &p1, &p2) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);
	convert_to_string_ex(p2);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;
	
	irc_join(&conn->conn, Z_STRVAL_PP(p2), NULL,  conn);
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
	irc_ignore_add(&conn->conn, &s, 1);
#endif
}

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

PHP_FUNCTION(ircg_html_encode)
{
	zval **p1;
	smart_str res = {0};
	
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_string_ex(p1);
	
	ircg_mirc_color(Z_STRVAL_PP(p1), &res, Z_STRLEN_PP(p1));

	RETVAL_STRINGL(res.c, res.len, 1);

	smart_str_free_ex(&res, 1);
}

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
		php_error(E_WARNING, "The first parameter should be an array");
		RETURN_FALSE;
	}

	h = HASH_OF(*p2);

	for (i = 0; i < NO_FMTS; i++) {
		if (zend_hash_index_find(h, i, (void **) &arg) == SUCCESS) {
			convert_to_string_ex(arg);
			fmt_msgs.fmt_msgs[i] = strdup(Z_STRVAL_PP(arg));
		} else
			fmt_msgs.fmt_msgs[i] = NULL;
	}
	zend_hash_update(&h_fmt_msgs, Z_STRVAL_PP(p1), Z_STRLEN_PP(p1), 
		&fmt_msgs, sizeof(fmt_msgs), NULL);

	RETVAL_TRUE;	
}

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
		
	irc_register_hook(conn, IRCG_MSG, msg_handler);
	irc_register_hook(conn, IRCG_QUIT, quit_handler);
	irc_register_hook(conn, IRCG_ERROR, error_handler);
	irc_register_hook(conn, IRCG_NICK, nick_handler);

	irc_register_hook(conn, IRCG_PART, part_handler);
	irc_register_hook(conn, IRCG_USER_ADD, user_add);
	irc_register_hook(conn, IRCG_USER_LEAVE, user_leave);
	irc_register_hook(conn, IRCG_USER_KICK, user_kick);
	irc_register_hook(conn, IRCG_USER_QUIT, user_quit);
	irc_register_hook(conn, IRCG_TOPIC, new_topic);

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

static int ircg_copy_ctcp_msgs(zval **array, php_irconn_t *conn)
{
	zval **val;
	char *str;
	ulong num;
	ulong str_len;
	HashPosition pos;

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(array), &pos);

	while (zend_hash_get_current_key_ex(Z_ARRVAL_PP(array), &str, &str_len, &num, 0, &pos) == HASH_KEY_IS_STRING) {
		zend_hash_get_current_data_ex(Z_ARRVAL_PP(array), (void **) &val, &pos);
		convert_to_string_ex(val);
		zend_hash_add(&conn->ctcp_msgs, str, str_len - 1, Z_STRVAL_PP(val),
				Z_STRLEN_PP(val) + 1, NULL);
		
		zend_hash_move_forward_ex(Z_ARRVAL_PP(array), &pos);
	}
	
	return 0;
}

PHP_FUNCTION(ircg_pconnect)
{
	/* This should become an array very soon */
	zval **p1, **p2, **p3, **p4 = NULL, **p5 = NULL, **p6;
	const char *username;
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

	zend_hash_init(&conn->ctcp_msgs, 10, NULL, NULL, 1);
	if (irc_connect(username, register_hooks, 
			conn, server, port, &conn->conn)) {
		free(conn);
		RETURN_FALSE;
	}
	irc_connects++;
	if (p5 && Z_TYPE_PP(p5) == IS_ARRAY) {
		ircg_copy_ctcp_msgs(p5, conn);
	}
	conn->password = conn->ident = NULL;
	conn->fmt_msgs = fmt_msgs;	
	irconn_id++;
	conn->irconn_id = irconn_id;
	zend_hash_index_update(&h_irconn, irconn_id, &conn, sizeof(conn), NULL);
	zend_llist_init(&conn->buffer, sizeof(smart_str), NULL, 1);
	conn->buffer_count = 0;
	time(&conn->login);

	RETVAL_LONG(irconn_id);
}


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

PHP_FUNCTION(ircg_notice)
{
	zval **id, **recipient, **msg;
	php_irconn_t *conn;
	smart_str l = {0};
	smart_str m = {0};
	smart_str tmp, tmp2;
	
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
					format_msg(MSG(conn, FMT_MSG_CHAN), &tmp, NULL, &tmp2, &l, &m);
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

zend_module_entry ircg_module_entry = {
	"ircg",
	ircg_functions,
	PHP_MINIT(ircg),
	PHP_MSHUTDOWN(ircg),
	PHP_RINIT(ircg),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(ircg),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(ircg),
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_IRCG
ZEND_GET_MODULE(ircg)
#endif

PHP_MINIT_FUNCTION(ircg)
{
/* Remove comments if you have entries in php.ini
	REGISTER_INI_ENTRIES();
*/

#if IRCG_API_VERSION >= 20010307
	if (irc_sizeof_irconn() != sizeof(irconn_t)) {
		printf("FATAL: The size of the irconn_t structure has grown "
				"since you compiled PHP.  Please rebuild PHP against "
				"the correct IRCG header files.\n");
		exit(1);
	}
#endif

	zend_hash_init(&h_fmt_msgs, 0, NULL, fmt_msgs_dtor, 1);	
	zend_hash_init(&h_fd2irconn, 0, NULL, NULL, 1);
	zend_hash_init(&h_irconn, 0, NULL, irconn_dtor, 1);
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(ircg)
{
/* Remove comments if you have entries in php.ini
	UNREGISTER_INI_ENTRIES();
*/
	zend_hash_destroy(&h_fmt_msgs);	
	zend_hash_destroy(&h_irconn);
	zend_hash_destroy(&h_fd2irconn);
	return SUCCESS;
}

/* Remove if there's nothing to do at request start */
PHP_RINIT_FUNCTION(ircg)
{
	return SUCCESS;
}

/* Remove if there's nothing to do at request end */
PHP_RSHUTDOWN_FUNCTION(ircg)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(ircg)
{
	char buf[100];
	php_info_print_table_start();
	php_info_print_table_header(2, "ircg support", "enabled");
	sprintf(buf, "%lu", cache_hits);
	php_info_print_table_row(2, "scanner result cache hits", buf);
	sprintf(buf, "%lu", cache_misses);
	php_info_print_table_row(2, "scanner result cache misses", buf);
	sprintf(buf, "%lu", irc_connects);
	php_info_print_table_row(2, "irc_connects", buf);
	sprintf(buf, "%lu", irc_set_currents);
	php_info_print_table_row(2, "irc_set_currents", buf);
	sprintf(buf, "%lu", irc_quit_handlers);
	php_info_print_table_row(2, "irc_quit_handlers", buf);
	php_info_print_table_end();
}

/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_ircg_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_ircg_compiled)
{
	zval **arg;
	int len;
	char string[256];

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &arg) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg);

	len = sprintf(string, "Congratulations, you have successfully modified ext/ircg/config.m4, module %s is compiled into PHP", Z_STRVAL_PP(arg));
	RETURN_STRINGL(string, len, 1);
}
/* }}} */
/* The previous line is meant for emacs, so it can correctly fold and unfold
   functions in source code. See the corresponding marks just before function
   definition, where the functions purpose is also documented. Please follow
   this convention for the convenience of others editing your code.
*/



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
