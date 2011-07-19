/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "istream.h"
#include "mail-storage.h"
#include "master-service.h"

#include "sieve-common.h"
#include "sieve-message.h"
#include "sieve-interpreter.h"

#include "sieve-tool.h"

#include "testsuite-common.h"
#include "testsuite-message.h"

/* 
 * Testsuite message environment 
 */
 
struct sieve_message_data testsuite_msgdata;

static struct mail *testsuite_mail;

static const char *_default_message_data = 
"From: stephan@rename-it.nl\n"
"To: sirius@drunksnipers.com\n"
"Subject: Frop!\n"
"\n"
"Friep!\n";

static string_t *envelope_from;
static string_t *envelope_to;
static string_t *envelope_auth;

pool_t message_pool;

static void testsuite_message_set_data(struct mail *mail)
{
	const char *recipient = NULL, *sender = NULL;
	
	/* 
	 * Collect necessary message data 
	 */
	 
	/* Get recipient address */ 
	(void)mail_get_first_header(mail, "Envelope-To", &recipient);
	if ( recipient == NULL )
		(void)mail_get_first_header(mail, "To", &recipient);
	if ( recipient == NULL ) 
		recipient = "recipient@example.com";
	
	/* Get sender address */
	(void)mail_get_first_header(mail, "Return-path", &sender);
	if ( sender == NULL ) 
		(void)mail_get_first_header(mail, "Sender", &sender);
	if ( sender == NULL ) 
		(void)mail_get_first_header(mail, "From", &sender);
	if ( sender == NULL ) 
		sender = "sender@example.com";

	memset(&testsuite_msgdata, 0, sizeof(testsuite_msgdata));	
	testsuite_msgdata.mail = mail;
	testsuite_msgdata.auth_user = sieve_tool_get_username(sieve_tool);
	testsuite_msgdata.return_path = sender;
	testsuite_msgdata.orig_envelope_to = recipient;
	testsuite_msgdata.final_envelope_to = recipient;

	(void)mail_get_first_header(mail, "Message-ID", &testsuite_msgdata.id);
}

void testsuite_message_init(void)
{		
	message_pool = pool_alloconly_create("testsuite_message", 6096);

	string_t *default_message = str_new(message_pool, 1024);
	str_append(default_message, _default_message_data);

	testsuite_mail = sieve_tool_open_data_as_mail(sieve_tool, default_message);
	testsuite_message_set_data(testsuite_mail);

	envelope_to = str_new(message_pool, 256);
	envelope_from = str_new(message_pool, 256);
	envelope_auth = str_new(message_pool, 256);
}

void testsuite_message_set_string
(const struct sieve_runtime_env *renv, string_t *message)
{
	testsuite_mail = sieve_tool_open_data_as_mail(sieve_tool, message);
	testsuite_message_set_data(testsuite_mail);

	sieve_message_context_flush(renv->msgctx);
}

void testsuite_message_set_file
(const struct sieve_runtime_env *renv, const char *file_path)
{
	testsuite_mail = sieve_tool_open_file_as_mail(sieve_tool, file_path);
	testsuite_message_set_data(testsuite_mail);

	sieve_message_context_flush(renv->msgctx);
}

void testsuite_message_set_mail
(const struct sieve_runtime_env *renv, struct mail *mail)
{
	testsuite_message_set_data(mail);

	sieve_message_context_flush(renv->msgctx);
}
	
void testsuite_message_deinit(void)
{
	pool_unref(&message_pool);
}

void testsuite_envelope_set_sender
(const struct sieve_runtime_env *renv, const char *value)
{
	str_truncate(envelope_from, 0);

	if ( value != NULL )
		str_append(envelope_from, value);

	testsuite_msgdata.return_path = str_c(envelope_from);

	sieve_message_context_flush(renv->msgctx);
}

void testsuite_envelope_set_recipient
(const struct sieve_runtime_env *renv, const char *value)
{
	str_truncate(envelope_to, 0);

	if ( value != NULL )
		str_append(envelope_to, value);

	testsuite_msgdata.orig_envelope_to = str_c(envelope_to);
	testsuite_msgdata.final_envelope_to = str_c(envelope_to);

	sieve_message_context_flush(renv->msgctx);
}

void testsuite_envelope_set_auth_user
(const struct sieve_runtime_env *renv, const char *value)
{
	str_truncate(envelope_auth, 0);

	if ( value != NULL )
		str_append(envelope_auth, value);

	testsuite_msgdata.auth_user = str_c(envelope_auth);

	sieve_message_context_flush(renv->msgctx);
} 
 
