/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"
#include "mail-storage.h"
#include "mail-namespace.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "ext-mailbox-common.h"

/* 
 * Mailboxexists command 
 *
 * Syntax:
 *    mailboxexists <mailbox-names: string-list>
 */

static bool tst_mailboxexists_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_mailboxexists_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

const struct sieve_command_def mailboxexists_test = { 
	"mailboxexists", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	tst_mailboxexists_validate, 
	tst_mailboxexists_generate, 
	NULL 
};

/* 
 * Mailboxexists operation
 */

static bool tst_mailboxexists_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_mailboxexists_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def mailboxexists_operation = { 
	"MAILBOXEXISTS",
	&mailbox_extension, 
	0, 
	tst_mailboxexists_operation_dump, 
	tst_mailboxexists_operation_execute 
};

/* 
 * Test validation 
 */

static bool tst_mailboxexists_validate
(struct sieve_validator *valdtr, struct sieve_command *tst) 
{ 		
	struct sieve_ast_argument *arg = tst->first_positional;
	
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "mailbox-names", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, tst, arg, FALSE);
}

/* 
 * Test generation 
 */

static bool tst_mailboxexists_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst) 
{
	sieve_operation_emit(cgenv->sblock, tst->ext, &mailboxexists_operation);

 	/* Generate arguments */
	return sieve_generate_arguments(cgenv, tst, NULL);
}

/* 
 * Code dump 
 */

static bool tst_mailboxexists_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "MAILBOXEXISTS");
	sieve_code_descend(denv);
		
	return
		sieve_opr_stringlist_dump(denv, address, "mailbox-names");
}

/* 
 * Code execution 
 */

static int tst_mailboxexists_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_stringlist *mailbox_names;
	string_t *mailbox_item;
	bool trace = FALSE; 
	bool all_exist = TRUE;
	int ret;

	/*
	 * Read operands 
	 */
	
	/* Read notify uris */
	if ( (ret=sieve_opr_stringlist_read
		(renv, address, "mailbox-names", &mailbox_names)) <= 0 )
		return ret;
	
	/*
	 * Perform operation
	 */

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_TESTS) ) {
		sieve_runtime_trace(renv, 0, "mailboxexists test");
		sieve_runtime_trace_descend(renv);

		trace = sieve_runtime_trace_active(renv, SIEVE_TRLVL_MATCHING);
	}

	if ( renv->scriptenv->user != NULL ) {
		int ret;

		mailbox_item = NULL;
		while ( (ret=sieve_stringlist_next_item(mailbox_names, &mailbox_item)) > 0 ) 
			{
			struct mail_namespace *ns;
			const char *mailbox = str_c(mailbox_item);
			struct mailbox *box;

			/* Find the namespace */	
			ns = mail_namespace_find(renv->scriptenv->user->namespaces, &mailbox);
			if ( ns == NULL) {
				if ( trace ) {
					sieve_runtime_trace(renv, 0, "mailbox `%s' not found", 
						str_sanitize(mailbox, 80));
				}

				all_exist = FALSE;
				break;
			}

			/* Open the box */
			box = mailbox_alloc(ns->list, mailbox, 0);
			if ( mailbox_open(box) < 0 ) {
				if ( trace ) {
					sieve_runtime_trace(renv, 0, "mailbox `%s' cannot be opened", 
						str_sanitize(mailbox, 80));
				}

				all_exist = FALSE;
				mailbox_free(&box);
				break;
			}

			/* Also fail when it is readonly */
			if ( mailbox_is_readonly(box) ) {
				if ( trace ) {
					sieve_runtime_trace(renv, 0, "mailbox `%s' is read-only", 
						str_sanitize(mailbox, 80));
				}

				all_exist = FALSE;
				mailbox_free(&box);
				break;
			}

			/* FIXME: check acl for 'p' or 'i' ACL permissions as required by RFC */

			if ( trace ) {
				sieve_runtime_trace(renv, 0, "mailbox `%s' exists", 
					str_sanitize(mailbox, 80));
			}

			/* Close mailbox */
			mailbox_free(&box);
		}
	
		if ( ret < 0 ) {
			sieve_runtime_trace_error(renv, "invalid mailbox name item");
			return SIEVE_EXEC_BIN_CORRUPT;
		}
	}

	if ( trace ) {
		if ( all_exist )
			sieve_runtime_trace(renv, 0, "all mailboxes are available");
		else
			sieve_runtime_trace(renv, 0, "some mailboxes are unavailable");
	}
	
	sieve_interpreter_set_test_result(renv->interp, all_exist);
	return SIEVE_EXEC_OK;
}
