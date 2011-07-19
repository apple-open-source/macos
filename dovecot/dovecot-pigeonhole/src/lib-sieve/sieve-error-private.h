/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_ERROR_PRIVATE_H
#define __SIEVE_ERROR_PRIVATE_H

#include "sieve-error.h"

/*
 * Types
 */

enum sieve_error_flags { 
	SIEVE_ERROR_FLAG_GLOBAL = (1 << 0)
};

/*
 * Initialization
 */

void sieve_errors_init(struct sieve_instance *svinst);
void sieve_errors_deinit(struct sieve_instance *svinst);

/*
 * Error handler object
 */

struct sieve_error_handler {
	pool_t pool;
	int refcount;

	struct sieve_instance *svinst;

	struct sieve_error_handler *parent;

	unsigned int max_errors;

	unsigned int errors;
	unsigned int warnings;

	/* Should the errorhandler handle or discard info/debug log?
	 * (This does not influence the previous setting)
	 */
	bool log_info;
	bool log_debug;

	void (*verror)
		(struct sieve_error_handler *ehandler, unsigned int flags,
			const char *location, const char *fmt, va_list args);
	void (*vwarning)
		(struct sieve_error_handler *ehandler, unsigned int flags,
			const char *location, const char *fmt, va_list args);
	void (*vinfo)
		(struct sieve_error_handler *ehandler, unsigned int flags,
			const char *location, const char *fmt, va_list args);
	void (*vdebug)
		(struct sieve_error_handler *ehandler, unsigned int flags,
			const char *location, const char *fmt, va_list args);

	void (*free)
		(struct sieve_error_handler *ehandler);
};

void sieve_error_handler_init
	(struct sieve_error_handler *ehandler, struct sieve_instance *svinst, 
		pool_t pool, unsigned int max_errors);

void sieve_error_handler_init_from_parent
	(struct sieve_error_handler *ehandler, pool_t pool, 
		struct sieve_error_handler *parent);

/*
 * Direct handler calls
 */

void sieve_direct_verror
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		unsigned int flags, const char *location, const char *fmt, va_list args);
void sieve_direct_vwarning
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		unsigned int flags, const char *location, const char *fmt, va_list args);
void sieve_direct_vinfo
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		unsigned int flags, const char *location, const char *fmt, va_list args);
void sieve_direct_vdebug
	(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
		unsigned int flags, const char *location, const char *fmt, va_list args);

static inline void sieve_direct_error
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	unsigned int flags, const char *location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	sieve_direct_verror(svinst, ehandler, flags, location, fmt, args);
	
	va_end(args);
}

static inline void sieve_direct_warning
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	unsigned int flags, const char *location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	sieve_direct_vwarning(svinst, ehandler, flags, location, fmt, args);
	
	va_end(args);
}

static inline void sieve_direct_info
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	unsigned int flags, const char *location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	sieve_direct_vinfo(svinst, ehandler, flags, location, fmt, args);
	
	va_end(args);
}

static inline void sieve_direct_debug
(struct sieve_instance *svinst, struct sieve_error_handler *ehandler,
	unsigned int flags, const char *location, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	sieve_direct_vdebug(svinst, ehandler, flags, location, fmt, args);

	va_end(args);
}


#endif /* __SIEVE_ERROR_PRIVATE_H */
