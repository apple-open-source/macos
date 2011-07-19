/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#ifndef __SIEVE_ACTIONS_H
#define __SIEVE_ACTIONS_H

#include "lib.h"
#include "mail-storage.h"

#include "sieve-common.h"
#include "sieve-objects.h"
#include "sieve-extensions.h"

/*
 * Action execution environment
 */

struct sieve_action_exec_env { 
	struct sieve_instance *svinst;

	struct sieve_result *result;
	struct sieve_error_handler *ehandler;

	const struct sieve_message_data *msgdata;
	struct sieve_message_context *msgctx;
	const struct sieve_script_env *scriptenv;
	struct sieve_exec_status *exec_status;
};

const char *sieve_action_get_location(const struct sieve_action_exec_env *aenv);

/*
 * Action flags
 */

enum sieve_action_flags {
	SIEVE_ACTFLAG_TRIES_DELIVER = (1 << 0),
	SIEVE_ACTFLAG_SENDS_RESPONSE = (1 << 1)
};
 
/* 
 * Action definition
 */

struct sieve_action_def {
	const char *name;
	unsigned int flags;
	
	bool (*equals)
		(const struct sieve_script_env *senv, const struct sieve_action *act1, 
			const struct sieve_action *act2);

	/* Result verification */
	
	int (*check_duplicate)	
		(const struct sieve_runtime_env *renv,
			const struct sieve_action *act, 
			const struct sieve_action *act_other);	
	int (*check_conflict)
		(const struct sieve_runtime_env *renv, 
			const struct sieve_action *act, 
			const struct sieve_action *act_other);	

	/* Result printing */
	
	void (*print)
		(const struct sieve_action *action, 
			const struct sieve_result_print_env *penv, bool *keep);	
		
	/* Result execution */	
		
	bool (*start)
		(const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void **tr_context);		
	bool (*execute)
		(const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void *tr_context);
	bool (*commit)
		(const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void *tr_context, bool *keep);
	void (*rollback)
		(const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void *tr_context, bool success);
};

/*
 * Action instance
 */

struct sieve_action {
	const struct sieve_action_def *def;
	const struct sieve_extension *ext;

	const char *location;
	void *context;
	bool executed;
};

#define sieve_action_is(act, definition) \
	( (act)->def == &(definition) )

/* 
 * Action side effects 
 */

/* Side effect object */

struct sieve_side_effect_def {
	struct sieve_object_def obj_def;
	
	/* The action it is supposed to link to */
	
	const struct sieve_action_def *to_action;
		
	/* Context coding */
	
	bool (*dump_context)
		(const struct sieve_side_effect *seffect, 
			const struct sieve_dumptime_env *renv, sieve_size_t *address);
	int (*read_context)
		(const struct sieve_side_effect *seffect, 
			const struct sieve_runtime_env *renv, sieve_size_t *address,
			void **se_context);
		
	/* Result verification */
	
	int (*merge)
		(const struct sieve_runtime_env *renv, const struct sieve_action *action, 
			const struct sieve_side_effect *old_seffect,
			const struct sieve_side_effect *new_seffect, void **old_context);

	/* Result printing */	
			
	void (*print)
		(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
			const struct sieve_result_print_env *penv, bool *keep);

	/* Result execution */

	bool (*pre_execute)
		(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void **context, 
			void *tr_context);
	bool (*post_execute)
		(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void *tr_context);
	void (*post_commit)
		(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void *tr_context, bool *keep);
	void (*rollback)
		(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
			const struct sieve_action_exec_env *aenv, void *tr_context, bool success);
};

struct sieve_side_effect {
	struct sieve_object object;

	const struct sieve_side_effect_def *def;

	void *context;
};

/*
 * Side effect operand
 */
 
#define SIEVE_EXT_DEFINE_SIDE_EFFECT(SEF) SIEVE_EXT_DEFINE_OBJECT(SEF)
#define SIEVE_EXT_DEFINE_SIDE_EFFECTS(SEFS) SIEVE_EXT_DEFINE_OBJECTS(SEFS)

#define SIEVE_OPT_SIDE_EFFECT (-1)

extern const struct sieve_operand_class sieve_side_effect_operand_class;

static inline void sieve_opr_side_effect_emit
(struct sieve_binary_block *sblock, const struct sieve_extension *ext,
	const struct sieve_side_effect_def *seff)
{ 
	sieve_opr_object_emit(sblock, ext, &seff->obj_def);
}

bool sieve_opr_side_effect_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address,
		struct sieve_side_effect *seffect);

bool sieve_opr_side_effect_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);

/*
 * Optional operands
 */

int sieve_action_opr_optional_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address,
		signed int *opt_code);

int sieve_action_opr_optional_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address,
		signed int *opt_code, int *exec_status,
		struct sieve_side_effects_list **list);

/* 
 * Core actions 
 */

extern const struct sieve_action_def act_redirect;
extern const struct sieve_action_def act_store;
extern const struct sieve_action_def act_discard;

/* 
 * Store action
 */

struct act_store_context {
	/* Folder name represented in utf-8 */
	const char *mailbox; 
};

struct act_store_transaction {
	struct act_store_context *context;
	struct mailbox *box;
	struct mailbox_transaction_context *mail_trans;
	struct mail *dest_mail;
	
	const char *error;
	enum mail_error error_code;

	enum mail_flags flags;
	ARRAY_TYPE(const_string) keywords;

	unsigned int flags_altered:1;
	unsigned int disabled:1;
	unsigned int redundant:1;
};

int sieve_act_store_add_to_result
	(const struct sieve_runtime_env *renv, 
		struct sieve_side_effects_list *seffects, const char *folder);

void sieve_act_store_add_flags
	(const struct sieve_action_exec_env *aenv, void *tr_context,
		const char *const *keywords, enum mail_flags flags);

void sieve_act_store_get_storage_error
	(const struct sieve_action_exec_env *aenv, struct act_store_transaction *trans);

/*		
 * Action utility functions
 */

/* Checking for duplicates */

bool sieve_action_duplicate_check_available
	(const struct sieve_script_env *senv);
int sieve_action_duplicate_check
	(const struct sieve_script_env *senv, const void *id, size_t id_size);
void sieve_action_duplicate_mark
	(const struct sieve_script_env *senv, const void *id, size_t id_size,
		time_t time);

/* Rejecting mail */

bool sieve_action_reject_mail
(const struct sieve_action_exec_env *aenv,
	const char *sender, const char *recipient, const char *reason);

#endif /* __SIEVE_ACTIONS_H */
