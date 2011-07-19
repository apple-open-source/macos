/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_LIMITS_H
#define __SIEVE_LIMITS_H

/*
 * Scripts
 */

#define SIEVE_MAX_SCRIPT_NAME_LEN      256

#define SIEVE_DEFAULT_MAX_SCRIPT_SIZE  (1 << 20)

/*
 * Lexer
 */

#define SIEVE_MAX_STRING_LEN           (1 << 20)
#define SIEVE_MAX_IDENTIFIER_LEN       32

/*
 * AST
 */

#define SIEVE_MAX_COMMAND_ARGUMENTS    32
#define SIEVE_MAX_BLOCK_NESTING        32
#define SIEVE_MAX_TEST_NESTING         32

/*
 * Runtime
 */

#define SIEVE_MAX_MATCH_VALUES         32

/*
 * Actions
 */

#define SIEVE_DEFAULT_MAX_ACTIONS      32
#define SIEVE_DEFAULT_MAX_REDIRECTS    4

#endif /* __SIEVE_LIMITS_H */
