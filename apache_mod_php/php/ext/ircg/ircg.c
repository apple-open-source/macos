/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
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

#include "php.h"
#include "php_ini.h"
#include "php_ircg.h"

#include "ext/standard/php_smart_str.h"
#include "ext/standard/info.h"

/* If you declare any globals in php_ircg.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(ircg)
*/

/* This module is not intended for thread-safe use */

static HashTable h_fd2irconn;
static HashTable h_irconn;
static int irconn_id;

/* Format string numbers */
enum {
	FMT_MSG_CHAN,
	FMT_MSG_PRIV_TO_ME,
	FMT_MSG_PRIV_FROM_ME,
	FMT_MSG_LEAVE,
	FMT_MSG_JOIN,
	FMT_MSG_KICK,
	FMT_MSG_TOPIC,
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
	PHP_FE(ircg_disconnect, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in ircg_functions[] */
};

#include "if_irc.h"

typedef struct {
	irconn_t conn;
	const char *username;
	const char *channel;
	zend_llist buffer;
	int fd;
	char *fmt_msgs[NO_FMTS];
} php_irconn_t;

static char *fmt_msgs_default[] = {
	"%f@%c: %m<br>",
	"%f: %m<br>",
	"To %t: %m<br>",
	"%f leaves %c<br>",
	"%f joins %c<br>",
	"%t was kicked by %f from %c (%m)<br>",
	"%f changes topic on %c to %m<br>"
};

#define MSG(conn, type) \
	conn->fmt_msgs[type]?conn->fmt_msgs[type]:fmt_msgs_default[type]

php_irconn_t *lookup_irconn(int id)
{
	php_irconn_t **ret = NULL;

	if (zend_hash_index_find(&h_irconn, id, (void **) &ret) == FAILURE)
		return NULL;
	return *ret;
}

static void irconn_dtor(void *dummy)
{
	php_irconn_t **conn = dummy;
	int i;

	for (i = 0; i < NO_FMTS; i++) {
		if ((*conn)->fmt_msgs[i]) free((*conn)->fmt_msgs[i]);
	}
	
	irc_disconnect(&(*conn)->conn, "Browser connection closed");
	free((char *) (*conn)->username);
}

static void buffer_dtor(void *dummy)
{
	free(dummy);
}

static void quit_handler(irconn_t *c, void *dummy)
{
	php_irconn_t *conn = dummy;
	
	zend_llist_destroy(&conn->buffer);
	free(conn);
}

static const char *color_list[] = {
	"white",
	"black",
	"blue",
	"green",
	"red",
	"brown",
	"purple",
	"orange",
	"yellow",
	"lightgreen",
	"teal",
	"lightcyan",
	"lightblue",
	"pink",
	"gray",
	"lightgrey"
};

static void mirc_color(const char *input, smart_str *result)
{
	int mode = 0;
	char *p, c;
	int fg, bg;
	int font_open = 0;

	for (p = input; (c = *p); p++) {
		switch (mode) {
		case 0:
evaluate_normal:
			if (c == '') {
				mode = 1;
				bg = fg = -1;
			} else {
				if (font_open) {
					smart_str_appends_ex(result, "</FONT>", 1);
					font_open = 0;
				}
				if (fg >= 0 && fg <= 15) {
					smart_str_appends_ex(result, "<FONT COLOR=\"", 1);
					smart_str_appends_ex(result, color_list[fg], 1);
					smart_str_appends_ex(result, "\">", 1);
					font_open = 1;
				}
				smart_str_appendc_ex(result, c, 1);
			}
			break;
		case 1:
			if (isdigit(c)) {
				if (fg == -1) fg = 0;
				fg = fg * 10 + c - '0';
				if (fg > 10)
					mode = 2;
			} else {
				mode = 0;
				goto evaluate_normal;
			}
			break;
		case 2:
			if (c == ',') {
				mode = 3;
			} else {
				mode = 0;
				goto evaluate_normal;
			}
			break;
		case 3:
			if (isdigit(c)) {
				if (bg == -1) bg = 0;
				bg = bg * 10 + c - '0';
				if (bg > 10)
					mode = 0;
			} else {
				mode = 0;
				goto evaluate_normal;
			}
			break;
		}
	}
	if (font_open) {
		smart_str_appends_ex(result, "</FONT>", 1);
		font_open = 0;
	}
}

static void format_msg(const char *fmt, const char *channel, const char *to, const char *from, const char *msg, smart_str *result)
{
	const char *p;
	char c;
	int mode = 0;
	smart_str new_msg = {0};

	if (msg) {
		mirc_color(msg, &new_msg);
	}
	
	for (p = fmt; (c = *p); p++) {
		switch (mode) {
		case 0:
			switch (c) {
			case '%':
				mode = 1;
				break;
			default:
				smart_str_appendc_ex(result, c, 1);
			}
			break;
		case 1:
			switch (c) {
			case '%':
				smart_str_appendc_ex(result, c, 1);
				break;
			case 'c':
				smart_str_appends_ex(result, channel, 1);
				break;
			case 't':
				smart_str_appends_ex(result, to, 1);
				break;
			case 'f':
				smart_str_appends_ex(result, from, 1);
				break;
			case 'm':
				smart_str_append_ex(result, &new_msg, 1);
				break;
			default:
				smart_str_appendc_ex(result, '%', 1);
				smart_str_appendc_ex(result, c, 1);
			}
			mode = 0;
			break;
		}
	}

	if (msg) {
		smart_str_free_ex(&new_msg, 1);
	}
	
	smart_str_0(result);
}

#include "SAPI.h"

#define ADD_HEADER(a) sapi_add_header(a, strlen(a), 1)

static void msg_http_start(php_irconn_t *conn)
{
	ADD_HEADER("Pragma: no-cache");
	sapi_flush();
}

static void msg_send(php_irconn_t *conn, smart_str *msg)
{
	/* XXX: Currently we don't buffer data. This is quite bad,
	   but leaves the implementation quite easy for now. */
	/* zend_llist_add_element(&conn->buffer, strdup(msg)); */

	if (conn->fd != -1) {
		send(conn->fd, msg->c, msg->len, 0);
	}
}

static void msg_handler(irconn_t *ircc, const char *chan, const char *from,
		smart_str *msg, void *conn_data, void *chan_data)
{
	php_irconn_t *conn = conn_data;
	smart_str m = {0};

	if (chan) {
		format_msg(MSG(conn, FMT_MSG_CHAN), chan, conn->username, from, msg->c,
				&m);
	} else {
		format_msg(MSG(conn, FMT_MSG_PRIV_TO_ME), NULL, conn->username, from,
				msg->c, &m);
	}

	msg_send(conn, &m);
	smart_str_free_ex(&m, 1);
}

static void user_add(irconn_t *ircc, const char *channel, smart_str *users,
		int nr, void *dummy)
{
	php_irconn_t *conn = dummy;
	int i;
	smart_str m = {0};

	for (i = 0; i < nr; i++) {
		m.len = 0;
		format_msg(MSG(conn, FMT_MSG_JOIN), channel, NULL, users[i].c,
				"", &m);
		msg_send(conn, &m);
	}
	smart_str_free_ex(&m, 1);
}

static void new_topic(irconn_t *ircc, const char *channel, smart_str *who, smart_str *topic, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_TOPIC), channel, NULL, who->c, topic->c, &m);
	msg_send(conn, &m);
	smart_str_free_ex(&m, 1);
}

static void user_leave(irconn_t *ircc, const char *channel, smart_str *user, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_LEAVE), channel, NULL, user->c, "", &m);
	msg_send(conn, &m);
	smart_str_free_ex(&m, 1);
}

static void user_kick(irconn_t *ircc, const char *channel, smart_str *who, smart_str *kicked_by, smart_str *reason, void *dummy)
{
	php_irconn_t *conn = dummy;
	smart_str m = {0};

	format_msg(MSG(conn, FMT_MSG_KICK), channel, who->c, kicked_by->c, reason?reason->c:"", &m);
	msg_send(conn, &m);
	smart_str_free_ex(&m, 1);
}

static void http_closed_connection(int fd)
{
	int *id;
	
	if (zend_hash_index_find(&h_fd2irconn, fd, (void **) &id) == FAILURE)
		return;

	zend_hash_index_del(&h_fd2irconn, fd);
	zend_hash_index_del(&h_irconn, *id);
}

PHP_FUNCTION(ircg_set_current)
{
	zval **p1;
	php_irconn_t *conn;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &p1) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(p1);

	conn = lookup_irconn(Z_LVAL_PP(p1));

	if (!conn) RETURN_FALSE;

	thttpd_register_on_close(http_closed_connection);
	thttpd_set_dont_close();
	conn->fd = thttpd_get_fd();
	zend_hash_index_update(&h_fd2irconn, conn->fd, &Z_LVAL_PP(p1), sizeof(int), NULL);
	msg_http_start(conn);

	RETURN_TRUE;
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
	
	irc_join(&conn->conn, Z_STRVAL_PP(p2), NULL, NULL, user_add, user_leave, user_kick, new_topic, conn);
	RETVAL_TRUE;
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

PHP_FUNCTION(ircg_pconnect)
{
	zval **p1, **p2, **p3, **p4 = NULL;
	const char *username;
	const char *server = "0";
	int port = 6667;
	php_irconn_t *conn;
	
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 4
			|| zend_get_parameters_ex(ZEND_NUM_ARGS(), &p1, &p2, &p3, &p4) == FAILURE)
		WRONG_PARAM_COUNT;

	switch (ZEND_NUM_ARGS()) {
	case 4:
		if (Z_TYPE_PP(p4) != IS_ARRAY) {
			php_error(E_WARNING, "Fourth parameter to ircg_pconnect() shall be an array");
			RETURN_FALSE;
		}
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

	conn = malloc(sizeof(*conn));
	conn->username = strdup(username);
	conn->channel = NULL,
	conn->fd = -1;
	if (p4) {
		HashTable *h;
		int i;
		zval **arg;

		h = HASH_OF(*p4);
		for (i = 0; i < NO_FMTS; i++) {
			if (zend_hash_index_find(h, i, (void **) &arg) == SUCCESS
					&& Z_STRLEN_PP(arg)) {
				conn->fmt_msgs[i] = strdup(Z_STRVAL_PP(arg));
			} else
				conn->fmt_msgs[i] = NULL;
		}
	} else {
		memset(conn->fmt_msgs, 0, sizeof(conn->fmt_msgs));
	}
	irconn_id++;
	zend_hash_index_update(&h_irconn, irconn_id, &conn, sizeof(conn), NULL);
	zend_llist_init(&conn->buffer, 0, buffer_dtor, 1);
	irc_connect(username, NULL, msg_handler, quit_handler, conn, server, port,
			&conn->conn);

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

PHP_FUNCTION(ircg_msg)
{
	zval **id, **recipient, **msg;
	php_irconn_t *conn;
	smart_str l = {0};
	smart_str m = {0};
	
	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &id, &recipient, &msg) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long_ex(id);
	convert_to_string_ex(recipient);
	convert_to_string_ex(msg);

	conn = lookup_irconn(Z_LVAL_PP(id));

	if (!conn) RETURN_FALSE;

	irc_msg(&conn->conn, Z_STRVAL_PP(recipient), Z_STRVAL_PP(msg));
	smart_str_setl(&l, Z_STRVAL_PP(msg), Z_STRLEN_PP(msg));
	
	switch (Z_STRVAL_PP(recipient)[0]) {
	case '#':
	case '&':
		format_msg(MSG(conn, FMT_MSG_CHAN), Z_STRVAL_PP(recipient),
				NULL, conn->username, l.c, &m);
		break;
	default:
		format_msg(MSG(conn, FMT_MSG_PRIV_FROM_ME), NULL,
				Z_STRVAL_PP(recipient), conn->username, l.c, &m);
	}

	msg_send(conn, &m);
	smart_str_free_ex(&m, 1);

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

	zend_hash_init(&h_fd2irconn, 0, NULL, NULL, 1);
	zend_hash_init(&h_irconn, 0, NULL, irconn_dtor, 1);
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(ircg)
{
/* Remove comments if you have entries in php.ini
	UNREGISTER_INI_ENTRIES();
*/
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
	php_info_print_table_start();
	php_info_print_table_header(2, "ircg support", "enabled");
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
