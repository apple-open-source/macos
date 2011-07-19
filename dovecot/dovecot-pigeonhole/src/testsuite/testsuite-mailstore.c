/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "mempool.h"
#include "imem.h"
#include "array.h"
#include "strfuncs.h"
#include "unlink-directory.h"
#include "env-util.h"
#include "mail-namespace.h"
#include "mail-storage.h"

#include "sieve-common.h" 
#include "sieve-error.h"
#include "sieve-interpreter.h"
 
#include "testsuite-message.h"
#include "testsuite-common.h"
#include "testsuite-smtp.h"

#include "testsuite-mailstore.h"

#include <sys/stat.h>
#include <sys/types.h>

/*
 * Forward declarations
 */

static void testsuite_mailstore_close(void);

/*
 * State
 */

static char *testsuite_mailstore_tmp = NULL;

static char *testsuite_mailstore_folder = NULL;
static struct mailbox *testsuite_mailstore_box = NULL;
static struct mailbox_transaction_context *testsuite_mailstore_trans = NULL;
static struct mail *testsuite_mailstore_mail = NULL;

/*
 * Initialization
 */

void testsuite_mailstore_init(void)
{	
	testsuite_mailstore_tmp = i_strconcat
		(testsuite_tmp_dir_get(), "/mailstore", NULL);

	if ( mkdir(testsuite_mailstore_tmp, 0700) < 0 ) {
		i_fatal("failed to create temporary directory '%s': %m.", 
			testsuite_mailstore_tmp);		
	}

	sieve_tool_init_mail_user
		(sieve_tool, t_strconcat("maildir:", testsuite_mailstore_tmp, NULL));
}

void testsuite_mailstore_deinit(void)
{
	testsuite_mailstore_close();

	if ( unlink_directory(testsuite_mailstore_tmp, TRUE) < 0 ) {
		i_warning("failed to remove temporary directory '%s': %m.",
			testsuite_mailstore_tmp);
	}
	
	i_free(testsuite_mailstore_tmp);		
}

void testsuite_mailstore_reset(void)
{
}

/*
 * Mailbox Access
 */

bool testsuite_mailstore_mailbox_create
(const struct sieve_runtime_env *renv ATTR_UNUSED, const char *folder)
{
	struct mail_user *mail_user = sieve_tool_get_mail_user(sieve_tool);
	struct mail_namespace *ns = mail_user->namespaces;
	struct mailbox *box;

	box = mailbox_alloc(ns->list, folder, 0);

	if ( mailbox_create(box, NULL, FALSE) < 0 ) {
		mailbox_free(&box);
		return FALSE;
	}

	mailbox_free(&box);

	return TRUE;
}

static void testsuite_mailstore_close(void)
{
	if ( testsuite_mailstore_mail != NULL )
		mail_free(&testsuite_mailstore_mail);

	if ( testsuite_mailstore_trans != NULL )
		mailbox_transaction_rollback(&testsuite_mailstore_trans);
		
	if ( testsuite_mailstore_box != NULL )
		mailbox_free(&testsuite_mailstore_box);

	if ( testsuite_mailstore_folder != NULL )
		i_free(testsuite_mailstore_folder);
}

static struct mail *testsuite_mailstore_open(const char *folder)
{
	enum mailbox_flags flags =
		MAILBOX_FLAG_KEEP_RECENT | MAILBOX_FLAG_SAVEONLY |
		MAILBOX_FLAG_POST_SESSION;
	struct mail_user *mail_user = sieve_tool_get_mail_user(sieve_tool);
	struct mail_namespace *ns = mail_user->namespaces;
	struct mailbox *box;
	struct mailbox_transaction_context *t;

	if ( testsuite_mailstore_mail == NULL ) {
		testsuite_mailstore_close();
	} else if ( testsuite_mailstore_folder != NULL 
		&& strcmp(folder, testsuite_mailstore_folder) != 0  ) {
		testsuite_mailstore_close();	
	} else {
		return testsuite_mailstore_mail;
	}

	box = mailbox_alloc(ns->list, folder, flags);
	if ( mailbox_open(box) < 0 ) {
		sieve_sys_error(testsuite_sieve_instance,
			"testsuite: failed to open mailbox '%s'", folder);
		mailbox_free(&box);
		return NULL;	
	}
	
	/* Sync mailbox */

	if ( mailbox_sync(box, MAILBOX_SYNC_FLAG_FULL_READ) < 0 ) {
		sieve_sys_error(testsuite_sieve_instance,
			"testsuite: failed to sync mailbox '%s'", folder);
		mailbox_free(&box);
		return NULL;
	}

	/* Start transaction */

	t = mailbox_transaction_begin(box, 0);

	testsuite_mailstore_folder = i_strdup(folder);
	testsuite_mailstore_box = box;
	testsuite_mailstore_trans = t;
	testsuite_mailstore_mail = mail_alloc(t, 0, NULL);

	return testsuite_mailstore_mail;
}

bool testsuite_mailstore_mail_index
(const struct sieve_runtime_env *renv, const char *folder, unsigned int index)
{
	struct mail *mail = testsuite_mailstore_open(folder);

	if ( mail == NULL )
		return FALSE;

	mail_set_seq(mail, index+1);
	testsuite_message_set_mail(renv, mail);

	return TRUE;
}
