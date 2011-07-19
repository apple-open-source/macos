/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#ifndef __SIEVE_LEXER_H
#define __SIEVE_LEXER_H

#include "lib.h"
#include "str.h"

#include "sieve-common.h"

enum sieve_token_type {
	STT_NONE,
	STT_WHITESPACE,
	STT_EOF,
  
	STT_NUMBER,
	STT_IDENTIFIER,
	STT_TAG,
	STT_STRING,
  
	STT_RBRACKET,
	STT_LBRACKET,
	STT_RCURLY,
	STT_LCURLY,
	STT_RSQUARE,
	STT_LSQUARE,
	STT_SEMICOLON,
	STT_COMMA,
  
	/* These are currently not used in the lexical specification, but a token
	 * is assigned to these to generate proper error messages (these are
	 * technically not garbage and possibly part of mistyped but otherwise
	 * valid tokens).
	 */
	STT_SLASH, 
	STT_COLON, 
  
	/* Error tokens */
	STT_GARBAGE, /* Error reporting deferred to parser */ 
	STT_ERROR    /* Lexer is responsible for error, parser won't report additional 
	                errors */
};

/*
 * Lexer object;
 */

struct sieve_lexical_scanner;

struct sieve_lexer {
	struct sieve_lexical_scanner *scanner;
	
	enum sieve_token_type token_type;
	string_t *token_str_value;
	int token_int_value;

	int token_line;
};

const struct sieve_lexer *sieve_lexer_create
	(struct sieve_script *script, struct sieve_error_handler *ehandler, 
		enum sieve_error *error_r);
void sieve_lexer_free(const struct sieve_lexer **lexer);

/* 
 * Scanning 
 */

bool sieve_lexer_skip_token(const struct sieve_lexer *lexer);

/*
 * Token access
 */ 

static inline enum sieve_token_type sieve_lexer_token_type
(const struct sieve_lexer *lexer) 
{
	return lexer->token_type;
}

static inline const string_t *sieve_lexer_token_str
(const struct sieve_lexer *lexer) 
{
	i_assert(	lexer->token_type == STT_STRING );
		
	return lexer->token_str_value;
}

static inline const char *sieve_lexer_token_ident
(const struct sieve_lexer *lexer) 
{
	i_assert(
		lexer->token_type == STT_TAG ||
		lexer->token_type == STT_IDENTIFIER);
		
	return str_c(lexer->token_str_value);
}

static inline int sieve_lexer_token_int
(const struct sieve_lexer *lexer) 
{
	i_assert(lexer->token_type == STT_NUMBER);
		
	return lexer->token_int_value;
}

static inline bool sieve_lexer_eof
(const struct sieve_lexer *lexer) 
{
	return lexer->token_type == STT_EOF;
}

static inline int sieve_lexer_token_line
(const struct sieve_lexer *lexer) 
{
	return lexer->token_line;
}

const char *sieve_lexer_token_description
	(const struct sieve_lexer *lexer);

void sieve_lexer_token_print
	(const struct sieve_lexer *lexer);

#endif /* __SIEVE_LEXER_H */
