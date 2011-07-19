/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "str-sanitize.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-ast.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-dump.h"

#include "testsuite-common.h"
#include "testsuite-substitutions.h"
#include "testsuite-arguments.h"

#include <ctype.h>

/* 
 * Testsuite string argument 
 */

static bool arg_testsuite_string_validate
	(struct sieve_validator *validator, struct sieve_ast_argument **arg, 
		struct sieve_command *context);

const struct sieve_argument_def testsuite_string_argument = { 
	"@testsuite-string", 
	NULL, 
	arg_testsuite_string_validate, 
	NULL, NULL,
	sieve_arg_catenated_string_generate,
};

static bool arg_testsuite_string_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	enum { ST_NONE, ST_OPEN, ST_SUBSTITUTION, ST_PARAM, ST_CLOSE } state = 
		ST_NONE;
	pool_t pool = sieve_ast_pool((*arg)->ast);
	struct sieve_arg_catenated_string *catstr = NULL;
	string_t *str = sieve_ast_argument_str(*arg);
	const char *p, *strstart, *substart = NULL;
	const char *strval = (const char *) str_data(str);
	const char *strend = strval + str_len(str);
	bool result = TRUE;
	string_t *subs_name = t_str_new(256);
	string_t *subs_param = t_str_new(256);
	
	T_BEGIN {
		/* Initialize substitution structure */
	
		p = strval;
		strstart = p;
		while ( result && p < strend ) {
			switch ( state ) {

			/* Nothing found yet */
			case ST_NONE:
				if ( *p == '%' ) {
					substart = p;
					state = ST_OPEN;
					str_truncate(subs_name, 0);
					str_truncate(subs_param, 0);
				}
				p++;
				break;

			/* Got '%' */
			case ST_OPEN:
				if ( *p == '{' ) {
					state = ST_SUBSTITUTION;
					p++;
				} else 
					state = ST_NONE;
				break;

			/* Got '%{' */ 
			case ST_SUBSTITUTION:
				state = ST_PARAM;	

				while ( *p != '}' && *p != ':' ) {
					if ( !i_isalnum(*p) ) {
						state = ST_NONE;
						break;
					}	
					str_append_c(subs_name, *p);
					p++;
				}
				break;
				
			/* Got '%{name' */
			case ST_PARAM:
				if ( *p == ':' ) {
					p++;
					while ( *p != '}' ) {
						str_append_c(subs_param, *p);
						p++;
					}
				}
				state = ST_CLOSE;
				break;

			/* Finished parsing param, expecting '}' */
			case ST_CLOSE:
				if ( *p == '}' ) {				
					struct sieve_ast_argument *strarg;
				
					/* We now know that the substitution is valid */	
					
					if ( catstr == NULL ) {
						catstr = sieve_arg_catenated_string_create(*arg);
					}
				
					/* Add the substring that is before the substitution to the 
					 * variable-string AST.
					 */
					if ( substart > strstart ) {
						string_t *newstr = str_new(pool, substart - strstart);
						str_append_n(newstr, strstart, substart - strstart); 
						
						strarg = sieve_ast_argument_string_create_raw
							((*arg)->ast, newstr, (*arg)->source_line);
						sieve_arg_catenated_string_add_element(catstr, strarg);
					
						/* Give other substitution extensions a chance to do their work */
						if ( !sieve_validator_argument_activate_super
							(valdtr, cmd, strarg, FALSE) ) {
							result = FALSE;
							break;
						}
					}
				
					strarg = testsuite_substitution_argument_create
						(valdtr, (*arg)->ast, (*arg)->source_line, str_c(subs_name), 
							str_c(subs_param));
					
					if ( strarg != NULL )
						sieve_arg_catenated_string_add_element(catstr, strarg);
					else {
						sieve_argument_validate_error(valdtr, *arg, 
							"unknown testsuite substitution type '%s'", str_c(subs_name));
					}

					strstart = p + 1;
					substart = strstart;

					p++;	
				}
		
				/* Finished, reset for the next substitution */	
				state = ST_NONE;
			}
		}
	} T_END;

	/* Bail out early if substitution is invalid */
	if ( !result ) return FALSE;
	
	/* Check whether any substitutions were found */
	if ( catstr == NULL ) {
		/* No substitutions in this string, pass it on to any other substution
		 * extension.
		 */
		return sieve_validator_argument_activate_super(valdtr, cmd, *arg, TRUE);
	}
	
	/* Add the final substring that comes after the last substitution to the 
	 * variable-string AST.
	 */
	if ( strend > strstart ) {
		struct sieve_ast_argument *strarg;
		string_t *newstr = str_new(pool, strend - strstart);
		str_append_n(newstr, strstart, strend - strstart); 

		strarg = sieve_ast_argument_string_create_raw
			((*arg)->ast, newstr, (*arg)->source_line);
		sieve_arg_catenated_string_add_element(catstr, strarg);
			
		/* Give other substitution extensions a chance to do their work */	
		if ( !sieve_validator_argument_activate_super
			(valdtr, cmd, strarg, FALSE) )
			return FALSE;
	}	
	
	return TRUE;
}
