/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-stringlist.h"
#include "sieve-error-private.h"

#include "testsuite-common.h"
#include "testsuite-log.h"

/*
 * Configuration
 */

bool _testsuite_log_stdout = FALSE;

/* 
 * Testsuite log error handlers
 */

struct sieve_error_handler *testsuite_log_ehandler = NULL;
struct sieve_error_handler *testsuite_log_main_ehandler = NULL;

struct _testsuite_log_message {
	const char *location;
	const char *message;
};

static pool_t _testsuite_logmsg_pool = NULL;
ARRAY_DEFINE(_testsuite_log_errors, struct _testsuite_log_message);
ARRAY_DEFINE(_testsuite_log_warnings, struct _testsuite_log_message);
ARRAY_DEFINE(_testsuite_log_messages, struct _testsuite_log_message);

static void _testsuite_log_verror
(struct sieve_error_handler *ehandler ATTR_UNUSED,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args)
{
	pool_t pool = _testsuite_logmsg_pool;
	struct _testsuite_log_message msg;

	if ( _testsuite_log_stdout ) {
		va_list args_copy;
		VA_COPY(args_copy, args);

		if ( location == NULL || *location == '\0' )
			fprintf(stdout,
				"LOG: error: %s\n", t_strdup_vprintf(fmt, args_copy));
		else
			fprintf(stdout,
				"LOG: error: %s: %s\n", location, t_strdup_vprintf(fmt, args_copy));
	}

	msg.location = p_strdup(pool, location);
	msg.message = p_strdup_vprintf(pool, fmt, args);

	array_append(&_testsuite_log_errors, &msg, 1);	
}

static void _testsuite_log_main_verror
(struct sieve_error_handler *ehandler ATTR_UNUSED,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args)
{
	if ( location == NULL || *location == '\0' )
		fprintf(stderr, 
			"error: %s\n", t_strdup_vprintf(fmt, args));
	else
		fprintf(stderr, 
			"%s: error: %s\n", location, t_strdup_vprintf(fmt, args));
}

static void _testsuite_log_vwarning
(struct sieve_error_handler *ehandler ATTR_UNUSED,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args)
{
	pool_t pool = _testsuite_logmsg_pool;
	struct _testsuite_log_message msg;

	if ( _testsuite_log_stdout ) {
		va_list args_copy;
		VA_COPY(args_copy, args);

		if ( location == NULL || *location == '\0' )
			fprintf(stdout,
				"LOG: warning: %s\n", t_strdup_vprintf(fmt, args_copy));
		else
			fprintf(stdout,
				"LOG: warning: %s: %s\n", location, t_strdup_vprintf(fmt, args_copy));
	}

	msg.location = p_strdup(pool, location);
	msg.message = p_strdup_vprintf(pool, fmt, args);

	array_append(&_testsuite_log_warnings, &msg, 1);
}

static void _testsuite_log_vinfo
(struct sieve_error_handler *ehandler ATTR_UNUSED,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args)
{
	pool_t pool = _testsuite_logmsg_pool;
	struct _testsuite_log_message msg;

	if ( _testsuite_log_stdout ) {
		va_list args_copy;
		VA_COPY(args_copy, args);

		if ( location == NULL || *location == '\0' )
			fprintf(stdout,
				"LOG: info: %s\n", t_strdup_vprintf(fmt, args_copy));
		else
			fprintf(stdout,
				"LOG: info: %s: %s\n", location, t_strdup_vprintf(fmt, args_copy));
	}

	msg.location = p_strdup(pool, location);
	msg.message = p_strdup_vprintf(pool, fmt, args);

	array_append(&_testsuite_log_messages, &msg, 1);
}

static struct sieve_error_handler *_testsuite_log_ehandler_create(void)
{
	pool_t pool;
	struct sieve_error_handler *ehandler;

	pool = pool_alloconly_create
		("testsuite_log_ehandler", sizeof(struct sieve_error_handler));
	ehandler = p_new(pool, struct sieve_error_handler, 1);
	sieve_error_handler_init(ehandler, testsuite_sieve_instance, pool, 0);

	ehandler->verror = _testsuite_log_verror;
	ehandler->vwarning = _testsuite_log_vwarning;
	ehandler->vinfo = _testsuite_log_vinfo;

	return ehandler;
}

static struct sieve_error_handler *_testsuite_log_main_ehandler_create(void)
{
	pool_t pool;
	struct sieve_error_handler *ehandler;

	pool = pool_alloconly_create
		("testsuite_log_main_ehandler", sizeof(struct sieve_error_handler));
	ehandler = p_new(pool, struct sieve_error_handler, 1);
	sieve_error_handler_init(ehandler, testsuite_sieve_instance, pool, 0);

	ehandler->verror = _testsuite_log_main_verror;
	ehandler->vwarning = _testsuite_log_vwarning;
	ehandler->vinfo = _testsuite_log_vinfo;

	return ehandler;
}

/*
 *
 */

void testsuite_log_clear_messages(void)
{
	if ( _testsuite_logmsg_pool != NULL ) {
		if ( array_count(&_testsuite_log_errors) == 0 )
			return;
		pool_unref(&_testsuite_logmsg_pool);
	}

	_testsuite_logmsg_pool = pool_alloconly_create
		("testsuite_log_messages", 8192);

	p_array_init(&_testsuite_log_errors, _testsuite_logmsg_pool, 128);
	p_array_init(&_testsuite_log_warnings, _testsuite_logmsg_pool, 128);
	p_array_init(&_testsuite_log_messages, _testsuite_logmsg_pool, 128);

	sieve_error_handler_reset(testsuite_log_ehandler);
}

/*
 *
 */

void testsuite_log_init(bool log_stdout)
{
	_testsuite_log_stdout = log_stdout;

	testsuite_log_ehandler = _testsuite_log_ehandler_create(); 
	sieve_error_handler_accept_infolog(testsuite_log_ehandler, TRUE);

	testsuite_log_main_ehandler = _testsuite_log_main_ehandler_create(); 
	sieve_error_handler_accept_infolog(testsuite_log_main_ehandler, TRUE);

	sieve_system_ehandler_set(testsuite_log_ehandler);

	testsuite_log_clear_messages();
}

void testsuite_log_deinit(void)
{
	sieve_error_handler_unref(&testsuite_log_ehandler);
	sieve_error_handler_unref(&testsuite_log_main_ehandler);

	pool_unref(&_testsuite_logmsg_pool);
}

/*
 * Log stringlist
 */

/* Forward declarations */

static int testsuite_log_stringlist_next_item
	(struct sieve_stringlist *_strlist, string_t **str_r);
static void testsuite_log_stringlist_reset
	(struct sieve_stringlist *_strlist);

/* Stringlist object */

struct testsuite_log_stringlist {
	struct sieve_stringlist strlist;

	int pos, index;
};

struct sieve_stringlist *testsuite_log_stringlist_create
(const struct sieve_runtime_env *renv, int index)
{
	struct testsuite_log_stringlist *strlist;
	    
	strlist = t_new(struct testsuite_log_stringlist, 1);
	strlist->strlist.runenv = renv;
	strlist->strlist.exec_status = SIEVE_EXEC_OK;
	strlist->strlist.next_item = testsuite_log_stringlist_next_item;
	strlist->strlist.reset = testsuite_log_stringlist_reset;

 	strlist->index = index;
	strlist->pos = 0;
 
	return &strlist->strlist;
}

static int testsuite_log_stringlist_next_item
(struct sieve_stringlist *_strlist, string_t **str_r)
{
	struct testsuite_log_stringlist *strlist = 
		(struct testsuite_log_stringlist *) _strlist;
	const struct _testsuite_log_message *msg;
	int pos;

	*str_r = NULL;

	if ( strlist->pos < 0 )
		return 0;

	if ( strlist->index > 0 ) {
		pos = strlist->index - 1;
		strlist->pos = -1;
	} else { 
		pos = strlist->pos++;
	}

	if ( pos >= (int) array_count(&_testsuite_log_errors) ) {
		strlist->pos = -1;
		return 0;
	}

	msg = array_idx(&_testsuite_log_errors, (unsigned int) pos);

	*str_r = t_str_new_const(msg->message, strlen(msg->message));
	return 1;
}

static void testsuite_log_stringlist_reset
(struct sieve_stringlist *_strlist)
{
	struct testsuite_log_stringlist *strlist = 
		(struct testsuite_log_stringlist *) _strlist;

	strlist->pos = 0;
}




