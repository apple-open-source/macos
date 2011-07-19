/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_RESULT_H
#define __SIEVE_RESULT_H

#include "sieve-common.h"
#include "sieve-error.h"

/*
 * Types
 */
 
struct sieve_side_effects_list;

/*
 * Result object
 */

struct sieve_result;

struct sieve_result *sieve_result_create
	(struct sieve_instance *svinst, const struct sieve_message_data *msgdata, 
		const struct sieve_script_env *senv,
		struct sieve_error_handler *ehandler);

void sieve_result_ref(struct sieve_result *result); 

void sieve_result_unref(struct sieve_result **result); 

pool_t sieve_result_pool(struct sieve_result *result);

/*
 * Getters/Setters
 */

struct sieve_error_handler *sieve_result_get_error_handler
	(struct sieve_result *result);
void sieve_result_set_error_handler
	(struct sieve_result *result, struct sieve_error_handler *ehandler);

const struct sieve_script_env *sieve_result_get_script_env
	(struct sieve_result *result);
const struct sieve_message_data *sieve_result_get_message_data
	(struct sieve_result *result);
struct sieve_message_context *sieve_result_get_message_context
	(struct sieve_result *result);

/*
 * Extension support
 */

void sieve_result_extension_set_context
	(struct sieve_result *result, const struct sieve_extension *ext,
		void *context);
const void *sieve_result_extension_get_context
	(struct sieve_result *result, const struct sieve_extension *ext); 

/* 
 * Result printing 
 */

struct sieve_result_print_env {
	struct sieve_result *result;
	const struct sieve_script_env *scriptenv;
	struct ostream *stream;
};

void sieve_result_vprintf
	(const struct sieve_result_print_env *penv, const char *fmt, va_list args);
void sieve_result_printf
	(const struct sieve_result_print_env *penv, const char *fmt, ...)
		ATTR_FORMAT(2, 3);
void sieve_result_action_printf
	(const struct sieve_result_print_env *penv, const char *fmt, ...)
		ATTR_FORMAT(2, 3);
void sieve_result_seffect_printf
	(const struct sieve_result_print_env *penv, const char *fmt, ...)
		ATTR_FORMAT(2, 3);

bool sieve_result_print
	(struct sieve_result *result, const struct sieve_script_env *senv, 
		struct ostream *stream, bool *keep);

/* 
 * Error handling 
 */

void sieve_result_error
(const struct sieve_action_exec_env *aenv, const char *fmt, ...)
	ATTR_FORMAT(2, 3);
void sieve_result_global_error
(const struct sieve_action_exec_env *aenv, const char *fmt, ...)
	ATTR_FORMAT(2, 3);
void sieve_result_warning
(const struct sieve_action_exec_env *aenv, const char *fmt, ...)
	ATTR_FORMAT(2, 3);
void sieve_result_global_warning
(const struct sieve_action_exec_env *aenv, const char *fmt, ...)
	ATTR_FORMAT(2, 3);
void sieve_result_log
(const struct sieve_action_exec_env *aenv, const char *fmt, ...)
	ATTR_FORMAT(2, 3);
void sieve_result_global_log
(const struct sieve_action_exec_env *aenv, const char *fmt, ...)
	ATTR_FORMAT(2, 3);

/*
 * Result composition
 */

void sieve_result_add_implicit_side_effect
(struct sieve_result *result, const struct sieve_action_def *to_action,
	bool to_keep, const struct sieve_extension *ext,
	const struct sieve_side_effect_def *seffect, void *context);

int sieve_result_add_action
	(const struct sieve_runtime_env *renv, const struct sieve_extension *ext,
		const struct sieve_action_def *act_def,
		struct sieve_side_effects_list *seffects,
		void *context, unsigned int instance_limit);
int sieve_result_add_keep
	(const struct sieve_runtime_env *renv,
		struct sieve_side_effects_list *seffects);

void sieve_result_set_keep_action
	(struct sieve_result *result, const struct sieve_extension *ext,
		const struct sieve_action_def *act_def);
void sieve_result_set_failure_action
	(struct sieve_result *result, const struct sieve_extension *ext,
		const struct sieve_action_def *act_def);

/*
 * Result execution
 */
 
bool sieve_result_implicit_keep(struct sieve_result *result);

void sieve_result_mark_executed(struct sieve_result *result);

int sieve_result_execute(struct sieve_result *result, bool *keep);

/*
 * Result evaluation
 */

struct sieve_result_iterate_context;

struct sieve_result_iterate_context *sieve_result_iterate_init
	(struct sieve_result *result);
const struct sieve_action *sieve_result_iterate_next
	(struct sieve_result_iterate_context *rictx, bool *keep);
void sieve_result_iterate_delete
	(struct sieve_result_iterate_context *rictx);
	
/*
 * Side effects list
 */
 
struct sieve_side_effects_list *sieve_side_effects_list_create
	(struct sieve_result *result);
void sieve_side_effects_list_add
	(struct sieve_side_effects_list *list, 
		const struct sieve_side_effect *seffect);

#endif
