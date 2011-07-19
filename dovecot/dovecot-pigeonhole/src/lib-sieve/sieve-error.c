/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#include "lib.h"
#include "str.h"
#include "array.h"
#include "ostream.h"
#include "var-expand.h"
#include "eacces-error.h"

#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-error-private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/*
 * Definitions
 */

#define CRITICAL_MSG \
	"internal error occurred: refer to server log for more information."
#define CRITICAL_MSG_STAMP CRITICAL_MSG " [%Y-%m-%d %H:%M:%S]"

/* Logfile error handler will rotate log when it exceeds 10k bytes */
#define LOGFILE_MAX_SIZE (10 * 1024)

/*
 * Utility
 */

const char *sieve_error_script_location
(const struct sieve_script *script, unsigned int source_line)
{
	const char *sname;

	sname = ( script == NULL ? NULL : sieve_script_name(script) );

	if ( sname == NULL || *sname == '\0' )
		return t_strdup_printf("line %d", source_line);

	return t_strdup_printf("%s: line %d", sname, source_line);
}

/*
 * Initialization
 */

void sieve_errors_init(struct sieve_instance *svinst)
{
	svinst->system_ehandler = sieve_master_ehandler_create(svinst, NULL, 0);
}
 
void sieve_errors_deinit(struct sieve_instance *svinst)
{
	sieve_error_handler_unref(&svinst->system_ehandler);
}

/*
 * Direct handler calls
 */

void sieve_direct_verror
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler, 
	unsigned int flags, const char *location, const char *fmt, va_list args)
{
	if ( (flags & SIEVE_ERROR_FLAG_GLOBAL) != 0 &&
		(ehandler == NULL || ehandler->parent == NULL) &&
		svinst->system_ehandler != ehandler &&
		svinst->system_ehandler->verror != NULL ) {
		va_list args_copy;
	
		VA_COPY(args_copy, args);

		svinst->system_ehandler->verror
			(svinst->system_ehandler, 0, location, fmt, args_copy);
	}

	if ( ehandler == NULL ) return;

	if ( ehandler->parent != NULL || sieve_errors_more_allowed(ehandler) ) {
		if ( ehandler->verror != NULL )
			ehandler->verror(ehandler, flags, location, fmt, args);
		
		if ( ehandler->pool != NULL )
			ehandler->errors++;
	}
}

void sieve_direct_vwarning
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	unsigned int flags, const char *location, const char *fmt, va_list args)
{
	if ( (flags & SIEVE_ERROR_FLAG_GLOBAL) != 0 &&
		(ehandler == NULL || ehandler->parent == NULL) && 
		svinst->system_ehandler != ehandler &&
		svinst->system_ehandler->vwarning != NULL ) {
		va_list args_copy;
	
		VA_COPY(args_copy, args);

		svinst->system_ehandler->vwarning
			(svinst->system_ehandler, 0, location, fmt, args_copy);
	}

	if ( ehandler == NULL ) return;

	if ( ehandler->vwarning != NULL )	
		ehandler->vwarning(ehandler, flags, location, fmt, args);

	if ( ehandler->pool != NULL )
		ehandler->warnings++;
}

void sieve_direct_vinfo
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	unsigned int flags, const char *location, const char *fmt, va_list args)
{
	if ( (flags & SIEVE_ERROR_FLAG_GLOBAL) != 0 &&
		(ehandler == NULL || ehandler->parent == NULL) &&
		svinst->system_ehandler != ehandler &&
		svinst->system_ehandler->vinfo != NULL ) {
		va_list args_copy;
	
		VA_COPY(args_copy, args);

		svinst->system_ehandler->vinfo
			(svinst->system_ehandler, 0, location, fmt, args_copy);
	}

	if ( ehandler == NULL ) return;

	if ( ehandler->parent != NULL || ehandler->log_info ) {
		if ( ehandler->vinfo != NULL )	
			ehandler->vinfo(ehandler, flags, location, fmt, args);
	}
}

void sieve_direct_vdebug
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	unsigned int flags, const char *location, const char *fmt, va_list args)
{ 
	if ( (flags & SIEVE_ERROR_FLAG_GLOBAL) != 0 &&
		(ehandler == NULL || ehandler->parent == NULL) && 
		svinst->system_ehandler != ehandler &&
		svinst->system_ehandler->vdebug != NULL ) {
		va_list args_copy;
	
		VA_COPY(args_copy, args);

		svinst->system_ehandler->vdebug
			(svinst->system_ehandler, 0, location, fmt, args_copy);
	}

	if ( ehandler == NULL ) return;

	if ( ehandler->parent != NULL || ehandler->log_info ) {
		if ( ehandler->vdebug != NULL )	
			ehandler->vdebug(ehandler, flags, location, fmt, args);
	}
}

/*
 * System errors
 */

void sieve_sys_verror
(struct sieve_instance *svinst, const char *fmt, va_list args)
{	
	T_BEGIN {
		sieve_direct_verror(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
}

void sieve_sys_vwarning
(struct sieve_instance *svinst, const char *fmt, va_list args)
{	
	T_BEGIN { 
		sieve_direct_vwarning(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
}

void sieve_sys_vinfo
(struct sieve_instance *svinst, const char *fmt, va_list args)
{	
	T_BEGIN { 
		sieve_direct_vinfo(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
}

void sieve_sys_vdebug
(struct sieve_instance *svinst, const char *fmt, va_list args)
{	
	T_BEGIN { 
		sieve_direct_vdebug(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
}

void sieve_sys_error(struct sieve_instance *svinst, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN { 
		sieve_direct_verror(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
	
	va_end(args);
}

void sieve_sys_warning(struct sieve_instance *svinst, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN {
		sieve_direct_vwarning(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
	
	va_end(args);
}

void sieve_sys_info(struct sieve_instance *svinst, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN {
		sieve_direct_vinfo(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
	
	va_end(args);
}

void sieve_sys_debug(struct sieve_instance *svinst, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN {
		sieve_direct_vdebug(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} T_END;
	
	va_end(args);
}

void sieve_system_ehandler_set
(struct sieve_error_handler *ehandler)
{
	struct sieve_instance *svinst = ehandler->svinst;

	sieve_error_handler_unref(&svinst->system_ehandler);
	svinst->system_ehandler = ehandler;
	sieve_error_handler_ref(ehandler);
}

struct sieve_error_handler *sieve_system_ehandler_get
(struct sieve_instance *svinst)
{
	return svinst->system_ehandler; 
}

/*
 * User errors
 */

void sieve_global_verror
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	const char *location, const char *fmt, va_list args)
{
	sieve_direct_verror
		(svinst, ehandler, SIEVE_ERROR_FLAG_GLOBAL, location, fmt, args);
}

void sieve_global_vwarning
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	const char *location, const char *fmt, va_list args)
{
	sieve_direct_vwarning
		(svinst, ehandler, SIEVE_ERROR_FLAG_GLOBAL, location, fmt, args);
}

void sieve_global_vinfo
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	const char *location, const char *fmt, va_list args)
{
	sieve_direct_vinfo
		(svinst, ehandler, SIEVE_ERROR_FLAG_GLOBAL, location, fmt, args);
}

void sieve_global_error
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler, 
	const char *location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN {
		sieve_global_verror(svinst, ehandler, location, fmt, args); 
	} T_END;
	
	va_end(args);
}

void sieve_global_warning
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler, 
	const char *location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN {
		sieve_global_vwarning(svinst, ehandler, location, fmt, args); 
	} T_END;
	
	va_end(args);
}

void sieve_global_info
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler, 
	const char *location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN {
		sieve_global_vinfo(svinst, ehandler, location, fmt, args); 
	} T_END;
	
	va_end(args);
}

/*
 * Default (user) error functions
 */

void sieve_verror
(struct sieve_error_handler *ehandler, const char *location, 
	const char *fmt, va_list args)
{	
	if ( ehandler == NULL ) return;

	sieve_direct_verror(ehandler->svinst, ehandler, 0, location, fmt, args);
}

void sieve_vwarning
(struct sieve_error_handler *ehandler, const char *location, 
	const char *fmt, va_list args)
{
	if ( ehandler == NULL ) return;

	sieve_direct_vwarning(ehandler->svinst, ehandler, 0, location, fmt, args);
}

void sieve_vinfo
(struct sieve_error_handler *ehandler, const char *location, 
	const char *fmt, va_list args)
{
	if ( ehandler == NULL ) return;

	sieve_direct_vinfo(ehandler->svinst, ehandler, 0, location, fmt, args);
}

void sieve_vdebug
(struct sieve_error_handler *ehandler, const char *location,
	const char *fmt, va_list args)
{
	if ( ehandler == NULL ) return;

	sieve_direct_vdebug(ehandler->svinst, ehandler, 0, location, fmt, args);
}

void sieve_vcritical
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	const char *location, const char *user_prefix, const char *fmt, va_list args)
{
	char str[256];
	struct tm *tm; 
		
	if ( location == NULL || *location == '\0' ) {
		sieve_direct_verror
			(svinst, svinst->system_ehandler, 0, NULL, fmt, args);
	} else {
		sieve_direct_verror
			(svinst, svinst->system_ehandler, 0, location, fmt, args);
	}

	if ( ehandler == NULL || ehandler == svinst->system_ehandler ) return;

	tm = localtime(&ioloop_time);
	
	if ( user_prefix == NULL || *user_prefix == '\0' ) {
		sieve_direct_error(svinst, ehandler, 0, location, "%s",  
			( strftime(str, sizeof(str), CRITICAL_MSG_STAMP, tm) > 0 ? 
				str : CRITICAL_MSG ));	
	} else { 
		sieve_direct_error(svinst, ehandler, 0, location, "%s: %s", user_prefix,  
			( strftime(str, sizeof(str), CRITICAL_MSG_STAMP, tm) > 0 ? 
				str : CRITICAL_MSG ));
	}	
}

void sieve_error
(struct sieve_error_handler *ehandler, const char *location, 
	const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN { sieve_verror(ehandler, location, fmt, args); } T_END;
	
	va_end(args);
}

void sieve_warning
(struct sieve_error_handler *ehandler, const char *location, 
	const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN { sieve_vwarning(ehandler, location, fmt, args); } T_END;

	va_end(args);
}

void sieve_info
(struct sieve_error_handler *ehandler, const char *location, 
	const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN { sieve_vinfo(ehandler, location, fmt, args); } T_END;
	
	va_end(args);
}

void sieve_debug
(struct sieve_error_handler *ehandler, const char *location, 
	const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN { sieve_vdebug(ehandler, location, fmt, args); } T_END;
	
	va_end(args);
}

void sieve_critical
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	const char *location, const char *user_prefix, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	T_BEGIN {
		sieve_vcritical(svinst, ehandler, location, user_prefix, fmt, args);
	} T_END;
	
	va_end(args);
}

/*
 * Error statistics
 */

unsigned int sieve_get_errors(struct sieve_error_handler *ehandler) 
{
	if ( ehandler == NULL || ehandler->pool == NULL ) return 0;
	
	return ehandler->errors;
}

unsigned int sieve_get_warnings(struct sieve_error_handler *ehandler) 
{
	if ( ehandler == NULL || ehandler->pool == NULL ) return 0;

	return ehandler->errors;
}

bool sieve_errors_more_allowed(struct sieve_error_handler *ehandler) 
{
	if ( ehandler == NULL || ehandler->pool == NULL ) 
		return TRUE;

	return ehandler->max_errors == 0 || ehandler->errors < ehandler->max_errors;
}

/*
 * Error handler configuration
 */

void sieve_error_handler_accept_infolog
(struct sieve_error_handler *ehandler, bool enable)
{
	while ( ehandler != NULL ) {
		ehandler->log_info = enable;
		ehandler = ehandler->parent;
	}
}

void sieve_error_handler_accept_debuglog
(struct sieve_error_handler *ehandler, bool enable)
{
	while ( ehandler != NULL ) {
		ehandler->log_debug = enable;
		ehandler = ehandler->parent;
	}
}

/*
 * Error handler init
 */

void sieve_error_handler_init
(struct sieve_error_handler *ehandler, struct sieve_instance *svinst,
	pool_t pool, unsigned int max_errors)
{
	ehandler->pool = pool;
	ehandler->svinst = svinst;
	ehandler->refcount = 1;
	ehandler->max_errors = max_errors;
	
	ehandler->errors = 0;
	ehandler->warnings = 0;
}

void sieve_error_handler_init_from_parent
(struct sieve_error_handler *ehandler, pool_t pool, struct sieve_error_handler *parent)
{
	i_assert( parent != NULL );

	sieve_error_handler_init(ehandler, parent->svinst, pool, parent->max_errors);

	ehandler->parent = parent;
	sieve_error_handler_ref(parent);

	ehandler->log_info = parent->log_info;
	ehandler->log_debug = parent->log_debug;
}

void sieve_error_handler_ref(struct sieve_error_handler *ehandler)
{
	if ( ehandler == NULL || ehandler->pool == NULL ) return;

	ehandler->refcount++;
}

void sieve_error_handler_unref(struct sieve_error_handler **ehandler)
{
	if ( *ehandler == NULL || (*ehandler)->pool == NULL ) return;

	i_assert((*ehandler)->refcount > 0);

	if (--(*ehandler)->refcount != 0)
        	return;

	if ( (*ehandler)->parent != NULL ) 
		sieve_error_handler_unref(&(*ehandler)->parent);

	if ( (*ehandler)->free != NULL )
		(*ehandler)->free(*ehandler);

	pool_unref(&((*ehandler)->pool));

	*ehandler = NULL;
}

void sieve_error_handler_reset(struct sieve_error_handler *ehandler)
{
	if ( ehandler == NULL || ehandler->pool == NULL ) return;

	ehandler->errors = 0;
	ehandler->warnings = 0;
}

/* 
 * Master/System error handler
 *
 * - Output errors directly to Dovecot master log
 */

struct sieve_master_ehandler {
	struct sieve_error_handler handler;

	const char *prefix;
};

typedef void (*master_log_func_t)(const char *fmt, ...) ATTR_FORMAT(1, 2);

static void sieve_master_vlog
(struct sieve_error_handler *_ehandler, master_log_func_t log_func,
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_master_ehandler *ehandler =
		(struct sieve_master_ehandler *) _ehandler;
	string_t *str;

	str = t_str_new(256);
	if ( ehandler->prefix != NULL)
		str_printfa(str, "%s: ", ehandler->prefix);

	str_append(str, "sieve: ");

	if ( location != NULL && *location != '\0' )
		str_printfa(str, "%s: ", location);

	str_vprintfa(str, fmt, args);

	log_func("%s", str_c(str));
}

static void sieve_master_verror
(struct sieve_error_handler *ehandler,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args) 
{
	sieve_master_vlog(ehandler, i_error, location, fmt, args);
}

static void sieve_master_vwarning
(struct sieve_error_handler *ehandler ATTR_UNUSED,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args) 
{
	sieve_master_vlog(ehandler, i_warning, location, fmt, args);
}

static void sieve_master_vinfo
(struct sieve_error_handler *ehandler ATTR_UNUSED,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args) 
{
	sieve_master_vlog(ehandler, i_info, location, fmt, args);
}

static void sieve_master_vdebug
(struct sieve_error_handler *ehandler ATTR_UNUSED,
	unsigned int flags ATTR_UNUSED, const char *location, const char *fmt,
	va_list args) 
{
	sieve_master_vlog(ehandler, i_debug, location, fmt, args);
}

struct sieve_error_handler *sieve_master_ehandler_create
(struct sieve_instance *svinst, const char *prefix, unsigned int max_errors) 
{
	pool_t pool;
	struct sieve_master_ehandler *ehandler;

	pool = pool_alloconly_create("master_error_handler", 256);
	ehandler = p_new(pool, struct sieve_master_ehandler, 1);
	sieve_error_handler_init(&ehandler->handler, svinst, pool, max_errors);

	ehandler->handler.verror = sieve_master_verror;
	ehandler->handler.vwarning = sieve_master_vwarning;
	ehandler->handler.vinfo = sieve_master_vinfo;
	ehandler->handler.vdebug = sieve_master_vdebug;

	if ( prefix != NULL )
		ehandler->prefix = p_strdup(pool, prefix);

	return &ehandler->handler;
}

/* 
 * STDERR error handler
 *
 * - Output errors directly to stderror 
 */

static void sieve_stderr_vmessage
(struct sieve_error_handler *ehandler ATTR_UNUSED, const char *prefix,
	const char *location, const char *fmt, va_list args) 
{
	if ( location == NULL || *location == '\0' )
		fprintf(stderr, "%s: %s.\n", prefix, t_strdup_vprintf(fmt, args));
	else
		fprintf(stderr, "%s: %s: %s.\n", location, prefix, t_strdup_vprintf(fmt, args));
}

static void sieve_stderr_verror
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED,
	const char *location, const char *fmt, va_list args) 
{
	sieve_stderr_vmessage(ehandler, "error", location, fmt, args);
}

static void sieve_stderr_vwarning
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args) 
{
	sieve_stderr_vmessage(ehandler, "warning", location, fmt, args);
}

static void sieve_stderr_vinfo
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args) 
{
	sieve_stderr_vmessage(ehandler, "info", location, fmt, args);
}

static void sieve_stderr_vdebug
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args) 
{
	sieve_stderr_vmessage(ehandler, "debug", location, fmt, args);
}

struct sieve_error_handler *sieve_stderr_ehandler_create
(struct sieve_instance *svinst, unsigned int max_errors)
{
	pool_t pool;
	struct sieve_error_handler *ehandler;

	/* Pool is not strictly necessary, but other handler types will need
	 * a pool, so this one will have one too.
	 */
	pool = pool_alloconly_create
		("stderr_error_handler", sizeof(struct sieve_error_handler));
	ehandler = p_new(pool, struct sieve_error_handler, 1);
	sieve_error_handler_init(ehandler, svinst, pool, max_errors);

	ehandler->verror = sieve_stderr_verror;
	ehandler->vwarning = sieve_stderr_vwarning;
	ehandler->vinfo = sieve_stderr_vinfo;
	ehandler->vdebug = sieve_stderr_vdebug;

	return ehandler;
}

/* String buffer error handler
 *
 * - Output errors to a string buffer 
 */

struct sieve_strbuf_ehandler {
	struct sieve_error_handler handler;

	string_t *errors;
	bool crlf;
};

static void sieve_strbuf_vmessage
(struct sieve_error_handler *ehandler, const char *prefix, 
	const char *location, const char *fmt, va_list args)
{
	struct sieve_strbuf_ehandler *handler =
		(struct sieve_strbuf_ehandler *) ehandler;

	if ( location != NULL && *location != '\0' )
		str_printfa(handler->errors, "%s: ", location);
	str_printfa(handler->errors, "%s: ", prefix);
	str_vprintfa(handler->errors, fmt, args);

	if ( !handler->crlf )
		str_append(handler->errors, ".\n");
	else
		str_append(handler->errors, ".\r\n");
}

static void sieve_strbuf_verror
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args)
{
	sieve_strbuf_vmessage(ehandler, "error", location, fmt, args);
}

static void sieve_strbuf_vwarning
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args)
{
	sieve_strbuf_vmessage(ehandler, "warning", location, fmt, args);
}

static void sieve_strbuf_vinfo
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args)
{
	sieve_strbuf_vmessage(ehandler, "info", location, fmt, args);
}

static void sieve_strbuf_vdebug
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args)
{
	sieve_strbuf_vmessage(ehandler, "debug", location, fmt, args);
}

struct sieve_error_handler *sieve_strbuf_ehandler_create
(struct sieve_instance *svinst, string_t *strbuf, bool crlf,
	unsigned int max_errors)
{
	pool_t pool;
	struct sieve_strbuf_ehandler *ehandler;

	pool = pool_alloconly_create("strbuf_error_handler", 256);
	ehandler = p_new(pool, struct sieve_strbuf_ehandler, 1);
	ehandler->errors = strbuf;

	sieve_error_handler_init(&ehandler->handler, svinst, pool, max_errors);

	ehandler->handler.verror = sieve_strbuf_verror;
	ehandler->handler.vwarning = sieve_strbuf_vwarning;
	ehandler->handler.vinfo = sieve_strbuf_vinfo;
	ehandler->handler.vdebug = sieve_strbuf_vdebug;

	ehandler->crlf = crlf;

	return &(ehandler->handler);
}

/* 
 * Logfile error handler
 * 
 * - Output errors to a log file 
 */

struct sieve_logfile_ehandler {
	struct sieve_error_handler handler;
	
	const char *logfile;
	bool started;
	int fd;
	struct ostream *stream;
};

static void sieve_logfile_vprintf
(struct sieve_logfile_ehandler *ehandler, const char *location, 
	const char *prefix, const char *fmt, va_list args) 
{
	string_t *outbuf;
	ssize_t ret = 0, remain;
	const char *data;
	
	if ( ehandler->stream == NULL ) return;
	
	T_BEGIN {
		outbuf = t_str_new(256);
		if ( location != NULL && *location != '\0' )
			str_printfa(outbuf, "%s: ", location);
		str_printfa(outbuf, "%s: ", prefix);	
		str_vprintfa(outbuf, fmt, args);
		str_append(outbuf, ".\n");
	
		remain = str_len(outbuf);
		data = (const char *) str_data(outbuf);

		while ( remain > 0 ) { 
			if ( (ret=o_stream_send(ehandler->stream, data, remain)) < 0 )
				break;

			remain -= ret;
			data += ret;
		}
	} T_END;

	if ( ret < 0 ) {
		sieve_sys_error(ehandler->handler.svinst,
			"o_stream_send() failed on logfile %s: %m", ehandler->logfile);		
	}
}

inline static void sieve_logfile_printf
(struct sieve_logfile_ehandler *ehandler, const char *location,
	const char *prefix, const char *fmt, ...) 
{
	va_list args;
	va_start(args, fmt);
	
	sieve_logfile_vprintf(ehandler, location, prefix, fmt, args);
	
	va_end(args);
}

static void sieve_logfile_start(struct sieve_logfile_ehandler *ehandler)
{
	struct sieve_instance *svinst = ehandler->handler.svinst;
	struct ostream *ostream = NULL;
	struct stat st;
	struct tm *tm;
	char buf[256];
	time_t now;
	int fd;

	/* Open the logfile */

	fd = open(ehandler->logfile, O_CREAT | O_APPEND | O_WRONLY, 0600);
	if (fd == -1) {
		if ( errno == EACCES ) {
			sieve_sys_error(svinst, "failed to open logfile (LOGGING TO STDERR): %s",
				eacces_error_get_creating("open", ehandler->logfile));
		} else {
			sieve_sys_error(svinst, "failed to open logfile (LOGGING TO STDERR): "
				"open(%s) failed: %m", ehandler->logfile);
		}
		fd = STDERR_FILENO;
	} else {
		/* fd_close_on_exec(fd, TRUE); Necessary? */

		/* Stat the log file to obtain size information */
		if ( fstat(fd, &st) != 0 ) {
			sieve_sys_error(svinst, "failed to stat logfile (logging to STDERR): "
				"fstat(fd=%s) failed: %m", ehandler->logfile);
			
			if ( close(fd) < 0 ) {
				sieve_sys_error(svinst, "failed to close logfile after error: "
					"close(fd=%s) failed: %m", ehandler->logfile);
			}

			fd = STDERR_FILENO;
		}
		
		/* Rotate log when it has grown too large */
		if ( st.st_size >= LOGFILE_MAX_SIZE ) {
			const char *rotated;
			
			/* Close open file */
			if ( close(fd) < 0 ) {
				sieve_sys_error(svinst, 
					"failed to close logfile: close(fd=%s) failed: %m", ehandler->logfile);
			}
			
			/* Rotate logfile */
			rotated = t_strconcat(ehandler->logfile, ".0", NULL);
			if ( rename(ehandler->logfile, rotated) < 0 ) {
				sieve_sys_error(svinst,
					"failed to rotate logfile: rename(%s, %s) failed: %m", 
					ehandler->logfile, rotated);
			}
			
			/* Open clean logfile (overwrites existing if rename() failed earlier) */
			fd = open(ehandler->logfile, O_CREAT | O_WRONLY | O_TRUNC, 0600);
			if (fd == -1) {
				if ( errno == EACCES ) {
					sieve_sys_error(svinst,
						"failed to open logfile (LOGGING TO STDERR): %s",
						eacces_error_get_creating("open", ehandler->logfile));
				} else {
					sieve_sys_error(svinst,
						"failed to open logfile (LOGGING TO STDERR): open(%s) failed: %m", 
						ehandler->logfile);
				}
				fd = STDERR_FILENO;
			}
		}
	}

	ostream = o_stream_create_fd(fd, 0, FALSE);
	if ( ostream == NULL ) {
		/* Can't we do anything else in this most awkward situation? */
		sieve_sys_error(svinst, "failed to open log stream on open file: "
			"o_stream_create_fd(fd=%s) failed "
			"(non-critical messages are not logged!)", ehandler->logfile);
	} 

	ehandler->fd = fd;
	ehandler->stream = ostream;
	ehandler->started = TRUE;
	
	if ( ostream != NULL ) {
		now = time(NULL);	
		tm = localtime(&now);

		if (strftime(buf, sizeof(buf), "%b %d %H:%M:%S", tm) > 0) {
			sieve_logfile_printf(ehandler, "sieve", "info",
				"started log at %s", buf);
		}
	}
}

static void sieve_logfile_verror
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_logfile_ehandler *handler = 
		(struct sieve_logfile_ehandler *) ehandler;

	if ( !handler->started ) sieve_logfile_start(handler);	

	sieve_logfile_vprintf(handler, location, "error", fmt, args);
}

static void sieve_logfile_vwarning
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_logfile_ehandler *handler = 
		(struct sieve_logfile_ehandler *) ehandler;

	if ( !handler->started ) sieve_logfile_start(handler);	

	sieve_logfile_vprintf(handler, location, "warning", fmt, args);
}

static void sieve_logfile_vinfo
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_logfile_ehandler *handler = 
		(struct sieve_logfile_ehandler *) ehandler;

	if ( !handler->started ) sieve_logfile_start(handler);	

	sieve_logfile_vprintf(handler, location, "info", fmt, args);
}

static void sieve_logfile_vdebug
(struct sieve_error_handler *ehandler, unsigned int flags ATTR_UNUSED, 
	const char *location, const char *fmt, va_list args)
{
	struct sieve_logfile_ehandler *handler =
		(struct sieve_logfile_ehandler *) ehandler;

	if ( !handler->started ) sieve_logfile_start(handler);

	sieve_logfile_vprintf(handler, location, "debug", fmt, args);
}

static void sieve_logfile_free
(struct sieve_error_handler *ehandler)
{
	struct sieve_logfile_ehandler *handler = 
		(struct sieve_logfile_ehandler *) ehandler;
		
	if ( handler->stream != NULL ) {
		o_stream_destroy(&(handler->stream));
		if ( handler->fd != STDERR_FILENO ){
			if ( close(handler->fd) < 0 ) {
				sieve_sys_error(ehandler->svinst, "failed to close logfile: "
					"close(fd=%s) failed: %m", handler->logfile);
			}
		}
	}
}

struct sieve_error_handler *sieve_logfile_ehandler_create
(struct sieve_instance *svinst, const char *logfile, unsigned int max_errors)
{
	pool_t pool;
	struct sieve_logfile_ehandler *ehandler;

	pool = pool_alloconly_create("logfile_error_handler", 512);
	ehandler = p_new(pool, struct sieve_logfile_ehandler, 1);
	sieve_error_handler_init(&ehandler->handler, svinst, pool, max_errors);

	ehandler->handler.verror = sieve_logfile_verror;
	ehandler->handler.vwarning = sieve_logfile_vwarning;
	ehandler->handler.vinfo = sieve_logfile_vinfo;
	ehandler->handler.vdebug = sieve_logfile_vdebug;
	ehandler->handler.free = sieve_logfile_free;

	/* Don't open logfile until something is actually logged.
	 * Let's not pullute the sieve directory with useless logfiles.
	 */
	ehandler->logfile = p_strdup(pool, logfile);
	ehandler->started = FALSE;
	ehandler->stream = NULL;
	ehandler->fd = -1;

	return &(ehandler->handler);
}

/*
 * Prefix error handler 
 *
 *   Encapsulates an existing error handler and prefixes all messages with
 *   the given prefix.
 */

struct sieve_prefix_ehandler {
	struct sieve_error_handler handler;

	struct sieve_error_handler *parent;

	const char *location;
	const char *prefix;
};

static const char *_prefix_message
(struct sieve_prefix_ehandler *ehandler,
	const char *location, const char *fmt, va_list args) 
{
	string_t *str = t_str_new(256);

	if ( ehandler->prefix != NULL)
		str_printfa(str, "%s: ", ehandler->prefix);
	if ( location != NULL)
		str_printfa(str, "%s: ", location);
	str_vprintfa(str, fmt, args);

	return str_c(str);
}

static void sieve_prefix_verror
(struct sieve_error_handler *_ehandler, unsigned int flags,
	const char *location, const char *fmt, va_list args) 
{	
	struct sieve_prefix_ehandler *ehandler =
		(struct sieve_prefix_ehandler *) _ehandler;

	sieve_direct_error(_ehandler->svinst, _ehandler->parent, flags,
		ehandler->location, "%s", _prefix_message(ehandler, location, fmt, args)); 
}

static void sieve_prefix_vwarning
(struct sieve_error_handler *_ehandler, unsigned int flags, 
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_prefix_ehandler *ehandler =
		(struct sieve_prefix_ehandler *) _ehandler;

	sieve_direct_warning(_ehandler->svinst, _ehandler->parent, flags,
		ehandler->location, "%s", _prefix_message(ehandler, location, fmt, args)); 
}

static void sieve_prefix_vinfo
(struct sieve_error_handler *_ehandler, unsigned int flags,
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_prefix_ehandler *ehandler =
		(struct sieve_prefix_ehandler *) _ehandler;

	sieve_direct_info(_ehandler->svinst, _ehandler->parent, flags,
		ehandler->location, "%s", _prefix_message(ehandler, location, fmt, args)); 
}

static void sieve_prefix_vdebug
(struct sieve_error_handler *_ehandler, unsigned int flags, 
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_prefix_ehandler *ehandler =
		(struct sieve_prefix_ehandler *) _ehandler;

	sieve_direct_debug(_ehandler->svinst, _ehandler->parent, flags, 
		ehandler->location, "%s", _prefix_message(ehandler, location, fmt, args)); 
}

struct sieve_error_handler *sieve_prefix_ehandler_create
(struct sieve_error_handler *parent, const char *location, const char *prefix)
{
	pool_t pool;
	struct sieve_prefix_ehandler *ehandler;

	if ( parent == NULL )
		return NULL;

	pool = pool_alloconly_create("sieve_prefix_error_handler", 256);	
	ehandler = p_new(pool, struct sieve_prefix_ehandler, 1);
	sieve_error_handler_init_from_parent(&ehandler->handler, pool, parent);

	ehandler->location = p_strdup(pool, location);
	ehandler->prefix = p_strdup(pool, prefix);

	ehandler->handler.verror = sieve_prefix_verror;
	ehandler->handler.vwarning = sieve_prefix_vwarning;
	ehandler->handler.vinfo = sieve_prefix_vinfo;
	ehandler->handler.vdebug = sieve_prefix_vdebug;

	return &(ehandler->handler);
}

/*
 * Varexpand error handler 
 *
 *   Encapsulates an existing error handler and formats all messages using the
 *   provided format string and variables;
 */

struct sieve_varexpand_ehandler {
	struct sieve_error_handler handler;

	const char *format;
	ARRAY_DEFINE(table, struct var_expand_table);
};

static const char *_expand_message
(struct sieve_error_handler *_ehandler,
	const char *location, const char *fmt, va_list args) 
{
	struct sieve_varexpand_ehandler *ehandler =
		(struct sieve_varexpand_ehandler *) _ehandler;
	unsigned int count;
	struct var_expand_table *table =
		array_get_modifiable(&ehandler->table, &count);
	string_t *str = t_str_new(256);

	/* Fill in substitution items */
	table[0].value = t_strdup_vprintf(fmt, args);
	table[1].value = location;

	/* Expand variables */
	var_expand(str, ehandler->format, table);

	return str_c(str);
}

static void sieve_varexpand_verror
(struct sieve_error_handler *ehandler, unsigned int flags,
	const char *location, const char *fmt, va_list args) 
{	
	sieve_direct_error(ehandler->svinst, ehandler->parent, flags, location,
		"%s", _expand_message(ehandler, location, fmt, args)); 
}

static void sieve_varexpand_vwarning
(struct sieve_error_handler *ehandler, unsigned int flags,
	const char *location, const char *fmt, va_list args) 
{
	sieve_direct_warning(ehandler->svinst, ehandler->parent, flags, location, 
		"%s", _expand_message(ehandler, location, fmt, args)); 
}

static void sieve_varexpand_vinfo
(struct sieve_error_handler *ehandler, unsigned int flags,
	const char *location, const char *fmt, va_list args) 
{
	sieve_direct_info(ehandler->svinst, ehandler->parent, flags, location,
		"%s", _expand_message(ehandler, location, fmt, args)); 
}

static void sieve_varexpand_vdebug
(struct sieve_error_handler *ehandler, unsigned int flags,
	const char *location, const char *fmt, va_list args) 
{
	sieve_direct_debug(ehandler->svinst, ehandler->parent, flags, location,
		"%s", _expand_message(ehandler, location, fmt, args)); 
}

struct sieve_error_handler *sieve_varexpand_ehandler_create
(struct sieve_error_handler *parent, const char *format,
	const struct var_expand_table *table)
{
	pool_t pool;
	struct sieve_varexpand_ehandler *ehandler;
	struct var_expand_table *entry;
	int i;

	if ( parent == NULL )
		return NULL;

	if ( format == NULL ) {
		sieve_error_handler_ref(parent);
		return parent;
	}

	pool = pool_alloconly_create("sieve_varexpand_error_handler", 1024);
	ehandler = p_new(pool, struct sieve_varexpand_ehandler, 1);
	sieve_error_handler_init_from_parent(&ehandler->handler, pool, parent);

	ehandler->format = format;
	p_array_init(&ehandler->table, pool, 10);

	entry = array_append_space(&ehandler->table);
	entry->key = '$';
	entry = array_append_space(&ehandler->table);
	entry->key = 'l';
	entry->long_key = "location";

	for (i = 0; table[i].key != '\0'; i++) {
		entry = array_append_space(&ehandler->table);

		/* Sanitize substitution items */
		entry->key = table[i].key;

		if ( table[i].value != NULL )
			entry->value = p_strdup(pool, table[i].value);
		if ( table[i].long_key != NULL )
			entry->long_key = p_strdup(pool, table[i].long_key);
	}

	(void)array_append_space(&ehandler->table);

	ehandler->handler.verror = sieve_varexpand_verror;
	ehandler->handler.vwarning = sieve_varexpand_vwarning;
	ehandler->handler.vinfo = sieve_varexpand_vinfo;
	ehandler->handler.vdebug = sieve_varexpand_vdebug;

	return &(ehandler->handler);
}
