/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
#ifndef __SIEVE_PARSER_H
#define __SIEVE_PARSER_H

#include "lib.h"

#include "sieve-common.h"

struct sieve_parser;

struct sieve_parser *sieve_parser_create
	(struct sieve_script *script, struct sieve_error_handler *ehandler,
		enum sieve_error *error_r);
void sieve_parser_free(struct sieve_parser **parser);
bool sieve_parser_run(struct sieve_parser *parser, struct sieve_ast **ast);

#endif /* __SIEVE_PARSER_H */
