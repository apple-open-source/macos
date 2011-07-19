/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_SAVE_H
#define __SIEVE_SAVE_H

#include "sieve-storage.h"

struct sieve_save_context;

struct sieve_save_context *
sieve_storage_save_init(struct sieve_storage *storage,
	const char *scriptname, struct istream *input);

int sieve_storage_save_continue(struct sieve_save_context *ctx);

int sieve_storage_save_finish(struct sieve_save_context *ctx);

struct sieve_script *sieve_storage_save_get_tempscript
  (struct sieve_save_context *ctx);

void sieve_storage_save_cancel(struct sieve_save_context **ctx);

int sieve_storage_save_commit(struct sieve_save_context **ctx);

#endif

