/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_NOTIFY_COMMON_H
#define __EXT_NOTIFY_COMMON_H

/*
 * Extension
 */

extern const struct sieve_extension_def notify_extension;

/*
 * Commands
 */

extern const struct sieve_command_def cmd_notify_old;
extern const struct sieve_command_def cmd_denotify;

/*
 * Arguments
 */

void ext_notify_register_importance_tags
	(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg,
		const struct sieve_extension *this_ext, unsigned int id_code);

/*
 * Operations
 */

extern const struct sieve_operation_def notify_old_operation;
extern const struct sieve_operation_def denotify_operation;

enum ext_notify_opcode {
	EXT_NOTIFY_OPERATION_NOTIFY,
	EXT_NOTIFY_OPERATION_DENOTIFY,
};

/* 
 * Actions 
 */

extern const struct sieve_action_def act_notify_old;

struct ext_notify_recipient {
	const char *full;
	const char *normalized;
};

ARRAY_DEFINE_TYPE(recipients, struct ext_notify_recipient);
		
struct ext_notify_action {
	const char *id;
	const char *message;
	sieve_number_t importance;

	ARRAY_TYPE(recipients) recipients;
};

/*
 * Message construct
 */

void ext_notify_construct_message
	(const struct sieve_runtime_env *renv, const char *msg_format, 
		string_t *out_msg);

#endif /* __EXT_NOTIFY_COMMON_H */
