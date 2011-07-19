/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "array.h"
#include "unlink-directory.h"

#include "sieve-common.h" 
#include "sieve-error.h"
#include "sieve-interpreter.h"
 
#include "testsuite-message.h"
#include "testsuite-common.h"
#include "testsuite-smtp.h"

#include <sys/stat.h>
#include <sys/types.h>

struct testsuite_smtp_message {
	const char *envelope_from;
	const char *envelope_to;
	const char *file;
};

static pool_t testsuite_smtp_pool;
static const char *testsuite_smtp_tmp;
static ARRAY_DEFINE(testsuite_smtp_messages, struct testsuite_smtp_message);

/*
 * Initialize
 */

void testsuite_smtp_init(void)
{
	pool_t pool;
	
	testsuite_smtp_pool = pool = pool_alloconly_create("testsuite_smtp", 8192);	
	
	testsuite_smtp_tmp = p_strconcat
		(pool, testsuite_tmp_dir_get(), "/smtp", NULL);

	if ( mkdir(testsuite_smtp_tmp, 0700) < 0 ) {
		i_fatal("failed to create temporary directory '%s': %m.", 
			testsuite_smtp_tmp);		
	}
	
	p_array_init(&testsuite_smtp_messages, pool, 16);
}

void testsuite_smtp_deinit(void)
{
	if ( unlink_directory(testsuite_smtp_tmp, TRUE) < 0 )
		i_warning("failed to remove temporary directory '%s': %m.",
			testsuite_smtp_tmp);
	
	pool_unref(&testsuite_smtp_pool);		
}

void testsuite_smtp_reset(void)
{
	testsuite_smtp_deinit();
	testsuite_smtp_init();
}

/*
 * Simulated SMTP out
 */
 
struct testsuite_smtp {
	const char *tmp_path;
	FILE *mfile;
};
 
void *testsuite_smtp_open
(void *script_ctx ATTR_UNUSED, const char *destination, 
	const char *return_path, FILE **file_r)
{	
	struct testsuite_smtp_message smtp_msg;
	struct testsuite_smtp *smtp;
	unsigned int smtp_count = array_count(&testsuite_smtp_messages);
	
	smtp_msg.file = p_strdup_printf(testsuite_smtp_pool, 
		"%s/%d.eml", testsuite_smtp_tmp, smtp_count);
	smtp_msg.envelope_from = 
		( return_path != NULL ? p_strdup(testsuite_smtp_pool, return_path) : NULL );
	smtp_msg.envelope_to = p_strdup(testsuite_smtp_pool, destination);
	 
	array_append(&testsuite_smtp_messages, &smtp_msg, 1);
	
	smtp = t_new(struct testsuite_smtp, 1);
	smtp->tmp_path = smtp_msg.file;
	smtp->mfile = fopen(smtp->tmp_path, "w");

	if ( smtp->mfile == NULL )
		i_fatal("failed to open tmp file for SMTP simulation.");

	*file_r = smtp->mfile;
	
	return (void *) smtp;	
}

bool testsuite_smtp_close
(void *script_ctx ATTR_UNUSED, void *handle)
{
	struct testsuite_smtp *smtp = (struct testsuite_smtp *) handle;

	fclose(smtp->mfile);
	
	return TRUE;
}

/*
 * Access
 */

bool testsuite_smtp_get
(const struct sieve_runtime_env *renv, unsigned int index)
{
	const struct testsuite_smtp_message *smtp_msg;

	if ( index >= array_count(&testsuite_smtp_messages) )
		return FALSE;

	smtp_msg = array_idx(&testsuite_smtp_messages, index);

	testsuite_message_set_file(renv, smtp_msg->file);
	testsuite_envelope_set_sender(renv, smtp_msg->envelope_from);
	testsuite_envelope_set_recipient(renv, smtp_msg->envelope_to);

	return TRUE;
}
