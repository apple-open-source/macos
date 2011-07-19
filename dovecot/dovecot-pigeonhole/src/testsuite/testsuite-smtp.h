/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#ifndef __TESTSUITE_SMTP_H
#define __TESTSUITE_SMTP_H
 
void testsuite_smtp_init(void);
void testsuite_smtp_deinit(void);
void testsuite_smtp_reset(void);

/*
 * Simulated SMTP out
 */
 
void *testsuite_smtp_open
	(void *script_ctx, const char *destination, const char *return_path, 
		FILE **file_r);
bool testsuite_smtp_close
	(void *script_ctx, void *handle);

/*
 * Access
 */

bool testsuite_smtp_get
	(const struct sieve_runtime_env *renv, unsigned int index);

#endif /* __TESTSUITE_SMTP_H */
