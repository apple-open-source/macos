/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __TESTSUITE_MESSAGE_H
#define __TESTSUITE_MESSAGE_H

#include "lib.h"
#include "master-service.h"

#include "sieve-common.h"
#include "sieve-tool.h"

extern struct sieve_message_data testsuite_msgdata;

void testsuite_message_init(void);
void testsuite_message_deinit(void);

void testsuite_message_set_string
	(const struct sieve_runtime_env *renv, string_t *message);
void testsuite_message_set_file
	(const struct sieve_runtime_env *renv, const char *file_path);
void testsuite_message_set_mail
	(const struct sieve_runtime_env *renv, struct mail *mail);

void testsuite_envelope_set_sender
	(const struct sieve_runtime_env *renv, const char *value);
void testsuite_envelope_set_recipient
	(const struct sieve_runtime_env *renv, const char *value);
void testsuite_envelope_set_orig_recipient
	(const struct sieve_runtime_env *renv, const char *value);
void testsuite_envelope_set_auth_user
	(const struct sieve_runtime_env *renv, const char *value);

#endif /* __TESTSUITE_MESSAGE_H */
