/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#ifndef __SIEVE_TYPES_H
#define __SIEVE_TYPES_H

#include "lib.h"

#include <stdio.h>

/*
 * Forward declarations
 */

struct sieve_instance;
struct sieve_callbacks;

struct sieve_script;
struct sieve_binary;

struct sieve_message_data;
struct sieve_script_env;
struct sieve_exec_status;

/*
 * Callbacks
 */

struct sieve_environment {
	const char *(*get_homedir)(void *context);
	const char *(*get_setting)(void *context, const char *identifier);
};

/*
 * Errors
 */

enum sieve_error {
	SIEVE_ERROR_NONE = 0,

	/* Temporary internal error */
	SIEVE_ERROR_TEMP_FAIL,
	/* It's not possible to do the wanted operation */
	SIEVE_ERROR_NOT_POSSIBLE,
	/* Invalid parameters (eg. script name not valid) */
	SIEVE_ERROR_BAD_PARAMS,
	/* No permission to do the request */
	SIEVE_ERROR_NO_PERM,
	/* Out of disk space */
	SIEVE_ERROR_NO_SPACE,
	/* Out of disk space */
	SIEVE_ERROR_NO_QUOTA,
	/* Item (e.g. script or binary) cannot be found */
	SIEVE_ERROR_NOT_FOUND,
	/* Item (e.g. script or binary) already exists */
	SIEVE_ERROR_EXISTS,
	/* Referenced item (e.g. script or binary) is not valid or currupt */
	SIEVE_ERROR_NOT_VALID,
	/* Not allowed to perform the operation because the item is in active use */
	SIEVE_ERROR_ACTIVE
};

/* 
 * Message data
 *
 * - The mail message + envelope data 
 */

struct sieve_message_data {
	struct mail *mail;
	const char *return_path;
	const char *orig_envelope_to;
	const char *final_envelope_to;
	const char *auth_user;
	const char *id;
};

/*
 * Runtime trace settings
 */

typedef enum {
	SIEVE_TRLVL_NONE,
	SIEVE_TRLVL_ACTIONS,
	SIEVE_TRLVL_COMMANDS,
	SIEVE_TRLVL_TESTS,
	SIEVE_TRLVL_MATCHING
} sieve_trace_level_t;

enum {
	SIEVE_TRFLG_DEBUG = (1 << 0),
	SIEVE_TRFLG_ADDRESSES = (1 << 1)
};

struct sieve_trace_config {
	sieve_trace_level_t level;
	unsigned int flags;
};

/* 
 * Script environment
 *
 * - Environment for currently executing script 
 */

struct sieve_script_env {
	/* Logging related */
	const char *action_log_format;

	/* Mail-related */
	struct mail_user *user;
	const char *default_mailbox;
	bool mailbox_autocreate;
	bool mailbox_autosubscribe;
	
	/* System-related */
	const char *username;
	const char *hostname;
	const char *postmaster_address;
		
	/* External context data */

	void *script_context;

	/* Callbacks */
	
	/* Interface for sending mail */
	void *(*smtp_open)
		(void *script_ctx, const char *destination, 
			const char *return_path, FILE **file_r);
	bool (*smtp_close)(void *script_ctx, void *handle);
	
	/* Interface for marking and checking duplicates */
	int (*duplicate_check)
		(void *script_ctx, const void *id, size_t id_size, const char *user);
	void (*duplicate_mark)
		(void *script_ctx, const void *id, size_t id_size, const char *user, 
			time_t time);

	/* Interface for rejecting mail */
	int (*reject_mail)(void *script_ctx, const char *recipient,
			const char *reason);
	
	/* Execution status record */	
	struct sieve_exec_status *exec_status;
		
	/* Runtime trace*/
	struct ostream *trace_stream;
	struct sieve_trace_config trace_config;
};

#define SIEVE_SCRIPT_DEFAULT_MAILBOX(senv) \
	(senv->default_mailbox == NULL ? "INBOX" : senv->default_mailbox )

/*
 * Script executionstatus
 */	

struct sieve_exec_status {
	bool message_saved;
	bool message_forwarded;
	bool tried_default_save;
	bool keep_original;
	struct mail_storage *last_storage;
};

/*
 * Execution exit codes
 */

enum sieve_execution_exitcode {
	SIEVE_EXEC_OK          = 1,
	SIEVE_EXEC_FAILURE     = 0,
	SIEVE_EXEC_BIN_CORRUPT = -1,
	SIEVE_EXEC_KEEP_FAILED = -2
};

#endif /* __SIEVE_TYPES_H */
