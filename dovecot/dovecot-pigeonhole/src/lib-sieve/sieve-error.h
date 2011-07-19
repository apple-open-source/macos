/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_ERROR_H
#define __SIEVE_ERROR_H

#include "lib.h"
#include "compat.h"

#include <stdarg.h>

/*
 * Forward declarations
 */

struct var_expand_table;

struct sieve_instance;
struct sieve_script;
struct sieve_error_handler;

/*
 * Types
 */

typedef void (*sieve_error_vfunc_t)
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, va_list args);
typedef void (*sieve_error_func_t)
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, ...) ATTR_FORMAT(3, 4);

/*
 * System errors
 */

void sieve_sys_verror
	(struct sieve_instance *svinst, const char *fmt, va_list args);
void sieve_sys_vwarning
	(struct sieve_instance *svinst, const char *fmt, va_list args);
void sieve_sys_vinfo
	(struct sieve_instance *svinst, const char *fmt, va_list args);
void sieve_sys_vdebug
	(struct sieve_instance *svinst, const char *fmt, va_list args);

void sieve_sys_error
	(struct sieve_instance *svinst, const char *fmt, ...) ATTR_FORMAT(2, 3);
void sieve_sys_warning
	(struct sieve_instance *svinst, const char *fmt, ...) ATTR_FORMAT(2, 3);
void sieve_sys_info
	(struct sieve_instance *svinst, const char *fmt, ...) ATTR_FORMAT(2, 3);
void sieve_sys_debug
	(struct sieve_instance *svinst, const char *fmt, ...) ATTR_FORMAT(2, 3);

void sieve_system_ehandler_set
	(struct sieve_error_handler *ehandler);
struct sieve_error_handler *sieve_system_ehandler_get
	(struct sieve_instance *svinst);

/*
 * Global (user+system) errors
 */

void sieve_global_verror
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *fmt, va_list args);
void sieve_global_vwarning
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *fmt, va_list args);
void sieve_global_vinfo
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *fmt, va_list args);

void sieve_global_error
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *fmt, ...) ATTR_FORMAT(4, 5);
void sieve_global_warning
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *fmt, ...) ATTR_FORMAT(4, 5);
void sieve_global_info
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *fmt, ...) ATTR_FORMAT(4, 5);

/*
 * Main (user) error functions
 */

/* For these functions it is the responsibility of the caller to
 * manage the datastack.
 */

const char *sieve_error_script_location
	(const struct sieve_script *script, unsigned int source_line);

void sieve_verror
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, va_list args);
void sieve_vwarning
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, va_list args); 
void sieve_vinfo
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, va_list args); 
void sieve_vdebug
	(struct sieve_error_handler *ehandler, const char *location,
		const char *fmt, va_list args);
void sieve_vcritical
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *user_prefix, const char *fmt,
		va_list args);

void sieve_error
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, ...) ATTR_FORMAT(3, 4);
void sieve_warning
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, ...) ATTR_FORMAT(3, 4);
void sieve_info
	(struct sieve_error_handler *ehandler, const char *location, 
		const char *fmt, ...) ATTR_FORMAT(3, 4);
void sieve_debug
	(struct sieve_error_handler *ehandler, const char *location,
		const char *fmt, ...) ATTR_FORMAT(3, 4);
void sieve_critical
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		const char *location, const char *user_prefix, const char *fmt, ...)
		ATTR_FORMAT(5, 6);

/*
 * Error handler configuration
 */

void sieve_error_handler_accept_infolog
	(struct sieve_error_handler *ehandler, bool enable);
void sieve_error_handler_accept_debuglog
	(struct sieve_error_handler *ehandler, bool enable);

/*
 * Error handler statistics
 */

unsigned int sieve_get_errors(struct sieve_error_handler *ehandler);
unsigned int sieve_get_warnings(struct sieve_error_handler *ehandler);

bool sieve_errors_more_allowed(struct sieve_error_handler *ehandler);

/*
 * Error handler object
 */

void sieve_error_handler_ref(struct sieve_error_handler *ehandler);
void sieve_error_handler_unref(struct sieve_error_handler **ehandler);

void sieve_error_handler_reset(struct sieve_error_handler *ehandler);

/* 
 * Error handlers 
 */

/* Write errors to dovecot master log */
struct sieve_error_handler *sieve_master_ehandler_create
	(struct sieve_instance *svinst, const char *prefix, unsigned int max_errors);

/* Write errors to stderr */
struct sieve_error_handler *sieve_stderr_ehandler_create
	(struct sieve_instance *svinst, unsigned int max_errors);

/* Write errors into a string buffer */
struct sieve_error_handler *sieve_strbuf_ehandler_create
	(struct sieve_instance *svinst, string_t *strbuf, bool crlf,
		unsigned int max_errors);

/* Write errors to a logfile */
struct sieve_error_handler *sieve_logfile_ehandler_create
	(struct sieve_instance *svinst, const char *logfile, unsigned int max_errors);  

/* Wrapper: prefix all log messages */
struct sieve_error_handler *sieve_prefix_ehandler_create
	(struct sieve_error_handler *parent, const char *location, 
		const char *prefix);

/* Wrapper: make messages part of var expansion */
struct sieve_error_handler *sieve_varexpand_ehandler_create
(struct sieve_error_handler *parent, const char *format,
	const struct var_expand_table *table);

#endif /* __SIEVE_ERROR_H */
