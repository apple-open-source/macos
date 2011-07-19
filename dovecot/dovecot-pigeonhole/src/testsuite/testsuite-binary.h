/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __TESTSUITE_BINARY_H
#define __TESTSUITE_BINARY_H

#include "sieve-common.h" 

void testsuite_binary_init(void);
void testsuite_binary_deinit(void);
void testsuite_binary_reset(void);

/*
 * Binary Access
 */

bool testsuite_binary_save(struct sieve_binary *sbin, const char *name);
struct sieve_binary *testsuite_binary_load(const char *name);

#endif /* __TESTSUITE_BINARY_H */
