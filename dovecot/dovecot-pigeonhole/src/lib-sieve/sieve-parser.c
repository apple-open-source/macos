/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
#include "lib.h"
#include "istream.h"
#include "failures.h"

#include "sieve-common.h"
#include "sieve-limits.h"
#include "sieve-script.h"
#include "sieve-lexer.h"
#include "sieve-parser.h"
#include "sieve-error.h"
#include "sieve-ast.h"

/*
 * Forward declarations
 */
 
inline static void sieve_parser_error
	(struct sieve_parser *parser, const char *fmt, ...) ATTR_FORMAT(2, 3);
inline static void sieve_parser_warning
	(struct sieve_parser *parser, const char *fmt, ...) ATTR_FORMAT(2, 3); 
 
static int sieve_parser_recover
	(struct sieve_parser *parser, enum sieve_token_type end_token);

/*
 * Parser object
 */

struct sieve_parser {
	pool_t pool;
	
	bool valid;
	
	struct sieve_script *script;
		
	struct sieve_error_handler *ehandler;
	
	const struct sieve_lexer *lexer;
	struct sieve_ast *ast;
};

struct sieve_parser *sieve_parser_create
(struct sieve_script *script, struct sieve_error_handler *ehandler,
	enum sieve_error *error_r)
{
	struct sieve_parser *parser;
	const struct sieve_lexer *lexer;
	
	lexer = sieve_lexer_create(script, ehandler, error_r);
  
	if ( lexer != NULL ) {
		pool_t pool = pool_alloconly_create("sieve_parser", 4096);	

		parser = p_new(pool, struct sieve_parser, 1);
		parser->pool = pool;
		parser->valid = TRUE;
		
		parser->ehandler = ehandler;
		sieve_error_handler_ref(ehandler);

		parser->script = script;
		sieve_script_ref(script);
				
		parser->lexer = lexer;
		parser->ast = NULL;
				
		return parser;
	}
	
	return NULL;
}

void sieve_parser_free(struct sieve_parser **parser)
{
	if ((*parser)->ast != NULL)	  
		sieve_ast_unref(&(*parser)->ast);

	sieve_lexer_free(&(*parser)->lexer);
	sieve_script_unref(&(*parser)->script);

	sieve_error_handler_unref(&(*parser)->ehandler);

	pool_unref(&(*parser)->pool);
	
	*parser = NULL;
}

/*
 * Internal error handling
 */

inline static void sieve_parser_error
(struct sieve_parser *parser, const char *fmt, ...)
{ 
	va_list args;
	va_start(args, fmt);

	/* Don't report a parse error if the lexer complained already */ 
	if ( sieve_lexer_token_type(parser->lexer) != STT_ERROR )  
	{
		T_BEGIN {
			sieve_verror(parser->ehandler,
				sieve_error_script_location(parser->script, 
					sieve_lexer_token_line(parser->lexer)),
				fmt, args);
		} T_END; 
	}
	
	parser->valid = FALSE;
	
	va_end(args);
}

inline static void sieve_parser_warning
(struct sieve_parser *parser, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	T_BEGIN	{
		sieve_vwarning(parser->ehandler, 
			sieve_error_script_location(parser->script, 
				sieve_lexer_token_line(parser->lexer)),
			fmt, args);
	} T_END;
		
	va_end(args);
} 

/*
 * Sieve grammar parsing
 */

/* sieve_parse_arguments():
 *
 * Parses both command arguments and sub-tests:
 *   arguments = *argument [test / test-list]
 *   argument = string-list / number / tag
 *   string = quoted-string / multi-line   [[implicitly handled in lexer]]
 *   string-list = "[" string *("," string) "]" / string         ;; if
 *     there is only a single string, the brackets are optional
 *   test-list = "(" test *("," test) ")"
 *   test = identifier arguments
 */
static int sieve_parse_arguments
(struct sieve_parser *parser, struct sieve_ast_node *node, unsigned int depth) 
{	
	const struct sieve_lexer *lexer = parser->lexer;
	struct sieve_ast_node *test = NULL;
	bool test_present = TRUE;
	bool arg_present = TRUE;
	int result = TRUE; /* Indicates whether the parser is in a defined, not 
	                       necessarily error-free state */

	/* Parse arguments */
	while ( arg_present && result > 0 ) {
		struct sieve_ast_argument *arg;

		if ( !parser->valid && !sieve_errors_more_allowed(parser->ehandler) ) {
			result = 0;
			break;
		}
		
		switch ( sieve_lexer_token_type(lexer) ) {
		
		/* String list */
		case STT_LSQUARE:
			/* Create stinglist object */
			arg = sieve_ast_argument_stringlist_create
				(node, sieve_lexer_token_line(parser->lexer));

			if ( arg == NULL ) break;
				
			sieve_lexer_skip_token(lexer);			
			
			if ( sieve_lexer_token_type(lexer) == STT_STRING ) {
				bool add_failed = FALSE;

				/* Add the string to the list */
				if ( !sieve_ast_stringlist_add
					(arg, sieve_lexer_token_str(lexer), 
						sieve_lexer_token_line(parser->lexer)) )
					add_failed = TRUE;
				
				sieve_lexer_skip_token(lexer);
				 
				while ( !add_failed && sieve_lexer_token_type(lexer) == STT_COMMA ) {
					sieve_lexer_skip_token(lexer);

					/* Check parser status */
					if ( !parser->valid && !sieve_errors_more_allowed(parser->ehandler) ) {
						result = sieve_parser_recover(parser, STT_RSQUARE);
						break;						
					}
				
					if ( sieve_lexer_token_type(lexer) == STT_STRING ) {
						/* Add the string to the list */
						if ( !sieve_ast_stringlist_add
							(arg, sieve_lexer_token_str(lexer), 
								sieve_lexer_token_line(parser->lexer)) )
							add_failed = TRUE;
							
						sieve_lexer_skip_token(lexer);
					} else {
						sieve_parser_error(parser, 
							"expecting string after ',' in string list, but found %s",
							sieve_lexer_token_description(lexer));
					
						result = sieve_parser_recover(parser, STT_RSQUARE);
						break;
					}
				}
				
				if ( add_failed ) {
					sieve_parser_error(parser, 
						"failed to accept more items in string list");
					return -1;
				}
			} else {
				sieve_parser_error(parser, 
					"expecting string after '[' in string list, but found %s",
					sieve_lexer_token_description(lexer));
			
				result = sieve_parser_recover(parser, STT_RSQUARE);
			}
		
			/* Finish the string list */
			if ( sieve_lexer_token_type(lexer) == STT_RSQUARE ) {
				sieve_lexer_skip_token(lexer);
			} else {
				sieve_parser_error(parser, 
					"expecting ',' or end of string list ']', but found %s",
					sieve_lexer_token_description(lexer));
			
				if ( (result=sieve_parser_recover(parser, STT_RSQUARE)) == TRUE ) 
					sieve_lexer_skip_token(lexer);
			}
	
			break;
			
		/* Single string */
		case STT_STRING: 
			arg = sieve_ast_argument_string_create
				(node, sieve_lexer_token_str(lexer), 
					sieve_lexer_token_line(parser->lexer));

			sieve_lexer_skip_token(lexer);
			break;
		
		/* Number */
		case STT_NUMBER:
			arg = sieve_ast_argument_number_create
				(node, sieve_lexer_token_int(lexer), 
					sieve_lexer_token_line(parser->lexer));
			sieve_lexer_skip_token(lexer);
			break;
			
		/* Tag */
		case STT_TAG:
			arg = sieve_ast_argument_tag_create
				(node, sieve_lexer_token_ident(lexer), 
					sieve_lexer_token_line(parser->lexer));
			sieve_lexer_skip_token(lexer);
			break;
			
		/* End of argument list, continue with tests */
		default:
			arg_present = FALSE;
			break;
		}

		if ( arg_present && arg == NULL ) {
			sieve_parser_error(parser, 
				"failed to accept more arguments for command '%s'", node->identifier);
			return -1;
		}

		if ( sieve_ast_argument_count(node) > SIEVE_MAX_COMMAND_ARGUMENTS ) {
			sieve_parser_error(parser, 
				"too many arguments for command '%s'", node->identifier);
			return FALSE;
		}
	}
	
	if ( result <= 0 ) return result; /* Defer recovery to caller */
	
	/* --> [ test / test-list ] 
 	 * test-list = "(" test *("," test) ")"
	 * test = identifier arguments
	 */
	switch ( sieve_lexer_token_type(lexer) ) {

	/* Single test */
	case STT_IDENTIFIER:
		if ( depth+1 > SIEVE_MAX_TEST_NESTING ) {
			sieve_parser_error(parser, 
				"cannot nest tests deeper than %u levels",
				SIEVE_MAX_TEST_NESTING);
			return FALSE;
		}

		test = sieve_ast_test_create
			(node, sieve_lexer_token_ident(lexer), 
				sieve_lexer_token_line(parser->lexer));
		sieve_lexer_skip_token(lexer);
		
		/* Theoretically, test can be NULL */
		if ( test == NULL ) break;

		/* Parse test arguments, which may include more tests (recurse) */
		if ( !sieve_parse_arguments(parser, test, depth+1) ) {
			return FALSE; /* Defer recovery to caller */
		}
		
		break;
		
	/* Test list */
	case STT_LBRACKET:	
		sieve_lexer_skip_token(lexer);

		if ( depth+1 > SIEVE_MAX_TEST_NESTING ) {
			sieve_parser_error(parser, 
				"cannot nest tests deeper than %u levels",
				SIEVE_MAX_TEST_NESTING);
			result = sieve_parser_recover(parser, STT_RBRACKET);

			if ( result ) sieve_lexer_skip_token(lexer);
			return result;
		}

		node->test_list = TRUE;
		
		/* Test starts with identifier */
		if ( sieve_lexer_token_type(lexer) == STT_IDENTIFIER ) {
			test = sieve_ast_test_create
				(node, sieve_lexer_token_ident(lexer), 
					sieve_lexer_token_line(parser->lexer));
			sieve_lexer_skip_token(lexer);
		
			if ( test == NULL ) break;

			/* Parse test arguments, which may include more tests (recurse) */
			if ( (result=sieve_parse_arguments(parser, test, depth+1)) > 0 ) {
			
				/* More tests ? */
				while ( sieve_lexer_token_type(lexer) == STT_COMMA ) { 
					sieve_lexer_skip_token(lexer);

					/* Check parser status */
					if ( !parser->valid && !sieve_errors_more_allowed(parser->ehandler) ) {
						result = sieve_parser_recover(parser, STT_RBRACKET);
						break;
					}

					/* Test starts with identifier */
					if ( sieve_lexer_token_type(lexer) == STT_IDENTIFIER ) {
						test = sieve_ast_test_create
							(node, sieve_lexer_token_ident(lexer), 
								sieve_lexer_token_line(parser->lexer));
						sieve_lexer_skip_token(lexer);

						if ( test == NULL ) break;
						
						/* Parse test arguments, which may include more tests (recurse) */
						if ( (result=sieve_parse_arguments(parser, test, depth+1)) <= 0 ) {
							if ( result < 0 ) return result;
							result = sieve_parser_recover(parser, STT_RBRACKET);
							break;
						}
					} else {
						sieve_parser_error(parser, 
							"expecting test identifier after ',' in test list, but found %s",
							sieve_lexer_token_description(lexer));
										
						result = sieve_parser_recover(parser, STT_RBRACKET);
						break;
					}
				}

				if ( test == NULL ) break;
			} else { 
				if ( result < 0 ) return result;

				result = sieve_parser_recover(parser, STT_RBRACKET);
			}
		} else {
			sieve_parser_error(parser, 
				"expecting test identifier after '(' in test list, but found %s",
				sieve_lexer_token_description(lexer));
			
			result = sieve_parser_recover(parser, STT_RBRACKET);
		}
		
		/* The next token should be a ')', indicating the end of the test list
		 *   --> previous sieve_parser_recover calls try to restore this situation 
		 *       after parse errors.  
		 */
 		if ( sieve_lexer_token_type(lexer) == STT_RBRACKET ) {
			sieve_lexer_skip_token(lexer);
		} else {
			sieve_parser_error(parser, 
				"expecting ',' or end of test list ')', but found %s",
				sieve_lexer_token_description(lexer));
			
			/* Recover function tries to make next token equal to ')'. If it succeeds 
			 * we need to skip it.
			 */
			if ( (result=sieve_parser_recover(parser, STT_RBRACKET)) == TRUE ) 
				sieve_lexer_skip_token(lexer);
		}
		break;
		
	default:
		/* Not an error: test / test-list is optional
		 *   --> any errors are detected by the caller  
		 */
		test_present = FALSE;
		break;
	}

	if ( test_present && test == NULL ) {
		sieve_parser_error(parser, 
			"failed to accept more tests for command '%s'", node->identifier);
		return -1;
	}			
	
	return result;
}

/* commands = *command
 * command = identifier arguments ( ";" / block )
 * block = "{" commands "}"
 */
static int sieve_parse_commands
(struct sieve_parser *parser, struct sieve_ast_node *block, unsigned int depth) 
{ 
	const struct sieve_lexer *lexer = parser->lexer;
	int result = TRUE;

	while ( result > 0 && 
		sieve_lexer_token_type(lexer) == STT_IDENTIFIER ) {
		struct sieve_ast_node *command;

		/* Check parser status */
		if ( !parser->valid && !sieve_errors_more_allowed(parser->ehandler) ) {
			result = sieve_parser_recover(parser, STT_SEMICOLON);
			break;
		}

		/* Create command node */
		command = sieve_ast_command_create
			(block, sieve_lexer_token_ident(lexer), 
				sieve_lexer_token_line(parser->lexer));
		sieve_lexer_skip_token(lexer);
	
		if ( command == NULL ) {
			sieve_parser_error(parser, 
				"failed to accept more commands inside the block of command '%s'", 
				block->identifier);
			return -1;
		}

		result = sieve_parse_arguments(parser, command, 1);

		/* Check whether the command is properly terminated 
		 * (i.e. with ; or a new block) 
		 */
		if ( result > 0 &&
			sieve_lexer_token_type(lexer) != STT_SEMICOLON &&
			sieve_lexer_token_type(lexer) != STT_LCURLY ) {
			
			sieve_parser_error(parser, 
				"expected end of command ';' or the beginning of a compound block '{', "
				"but found %s",
				sieve_lexer_token_description(lexer));	
			result = FALSE;
		}
		
		/* Try to recover from parse errors to reacquire a defined state */
		if ( result == 0 ) {
			result = sieve_parser_recover(parser, STT_SEMICOLON);
		}

		/* Don't bother to continue if we are not in a defined state */
		if ( result <= 0 ) {
			return result;
		}
			
		switch ( sieve_lexer_token_type(lexer) ) {
		
		/* End of the command */
		case STT_SEMICOLON:
			sieve_lexer_skip_token(lexer);
			break;

		/* Command has a block {...} */		
		case STT_LCURLY:
			sieve_lexer_skip_token(lexer);
			
			/* Check current depth first */
			if ( depth+1 > SIEVE_MAX_BLOCK_NESTING ) {
				sieve_parser_error(parser, 
					"cannot nest command blocks deeper than %u levels",
					SIEVE_MAX_BLOCK_NESTING);
				result = sieve_parser_recover(parser, STT_RCURLY);

				if ( result > 0 )
					sieve_lexer_skip_token(lexer);
				break;
			}

			command->block = TRUE;
			
			if ( (result=sieve_parse_commands(parser, command, depth+1)) > 0 ) {
			
				if ( sieve_lexer_token_type(lexer) != STT_RCURLY ) {
					sieve_parser_error(parser, 
						"expected end of compound block '}', but found %s",
						sieve_lexer_token_description(lexer));
					result = sieve_parser_recover(parser, STT_RCURLY);				
				} else 
					sieve_lexer_skip_token(lexer);
			} else {
				if ( result < 0 ) return result;

				if ( (result=sieve_parser_recover(parser, STT_RCURLY)) > 0 ) 
					sieve_lexer_skip_token(lexer);
			}

			break;
			
		default:
			/* Recovered previously, so this cannot happen */
			i_unreached();
		}
	}

	return result;
}

bool sieve_parser_run
(struct sieve_parser *parser, struct sieve_ast **ast) 
{
	if ( parser->ast != NULL )
		sieve_ast_unref(&parser->ast);
	
	/* Create AST object if none is provided */
	if ( *ast == NULL )
		*ast = sieve_ast_create(parser->script);
	else 
		sieve_ast_ref(*ast);
		
	parser->ast = *ast;

	/* Scan first token */
	sieve_lexer_skip_token(parser->lexer);

	/* Parse */
	if ( sieve_parse_commands(parser, sieve_ast_root(parser->ast), 1) > 0 && 
		parser->valid ) {
		 
		/* Parsed right to EOF ? */
		if ( sieve_lexer_token_type(parser->lexer) != STT_EOF ) { 
			sieve_parser_error(parser, 
				"unexpected %s found at (the presumed) end of file",
				sieve_lexer_token_description(parser->lexer));
			parser->valid = FALSE;
		}
	} else parser->valid = FALSE;
	
	/* Clean up AST if parse failed */
	if ( !parser->valid ) {
		parser->ast = NULL;
		sieve_ast_unref(ast);
	}
	
	return parser->valid;
}	

/* Error recovery:
 *   To continue parsing after an error it is important to find the next 
 *   parsible item in the stream. The recover function skips over the remaining 
 *   garbage after an error. It tries  to find the end of the failed syntax 
 *   structure and takes nesting of structures into account. 
 */

/* Assign useful names to priorities for readability */ 
enum sieve_grammatical_prio {
	SGP_BLOCK = 3,
	SGP_COMMAND = 2,
	SGP_TEST_LIST = 1,
	SGP_STRING_LIST = 0,
  
	SGP_OTHER = -1
};

static inline enum sieve_grammatical_prio __get_token_priority
(enum sieve_token_type token) 
{
	switch ( token ) {
	case STT_LCURLY:
	case STT_RCURLY: 
		return SGP_BLOCK;
	case STT_SEMICOLON: 
		return SGP_COMMAND;
	case STT_LBRACKET:
	case STT_RBRACKET: 
		return SGP_TEST_LIST;
	case STT_LSQUARE:
	case STT_RSQUARE: 
		return SGP_STRING_LIST;
	default:
		break;
	}
	
	return SGP_OTHER;
}

static int sieve_parser_recover
(struct sieve_parser *parser, enum sieve_token_type end_token) 
{
	/* The tokens that begin/end a specific block/command/list in order 
 	 * of ascending grammatical priority.
 	 */ 
 	static const enum sieve_token_type begin_tokens[4] = 
 		{ STT_LSQUARE, STT_LBRACKET, STT_NONE, STT_LCURLY };
	static const enum sieve_token_type end_tokens[4] = 
		{ STT_RSQUARE, STT_RBRACKET, STT_SEMICOLON, STT_RCURLY};

	const struct sieve_lexer *lexer = parser->lexer;
	int nesting = 1;
	enum sieve_grammatical_prio end_priority = __get_token_priority(end_token);
			
	i_assert( end_priority != SGP_OTHER );
			
	while ( sieve_lexer_token_type(lexer) != STT_EOF && 
		__get_token_priority(sieve_lexer_token_type(lexer)) <= end_priority ) {
			
		if ( sieve_lexer_token_type(lexer) == begin_tokens[end_priority] ) {
			nesting++;
			sieve_lexer_skip_token(lexer);
			continue;
		}
		
		if ( sieve_lexer_token_type(lexer) == end_tokens[end_priority] ) {
			nesting--;

			if ( nesting == 0 ) {
				/* Next character is the end */
				return TRUE; 
			}
		}
		
		sieve_lexer_skip_token(lexer);
	}
	
	/* Special case: COMMAND */
	if (end_token == STT_SEMICOLON && 
		sieve_lexer_token_type(lexer) == STT_LCURLY) {
		return TRUE;
	}
	
	/* End not found before eof or end of surrounding grammatical structure 
	 */
	return FALSE; 
}



