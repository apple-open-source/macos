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

#include "ext-variables-common.h"
#include "ext-variables-limits.h"
#include "ext-variables-name.h"
#include "ext-variables-operands.h"
#include "ext-variables-namespaces.h"
#include "ext-variables-arguments.h"

/* 
 * Variable argument implementation
 */

static bool arg_variable_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *context);

const struct sieve_argument_def variable_argument = { 
	"@variable", 
	NULL, NULL, NULL, NULL,
	arg_variable_generate 
};

static bool ext_variables_variable_argument_activate
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr, 
	struct sieve_ast_argument *arg, const char *variable)
{
	struct sieve_ast *ast = arg->ast;
	struct sieve_variable *var;
	
	var = ext_variables_validator_get_variable(this_ext, valdtr, variable, TRUE);

	if ( var == NULL ) {
		sieve_argument_validate_error(valdtr, arg, 
			"(implicit) declaration of new variable '%s' exceeds the limit "
			"(max variables: %u)", variable, 
			EXT_VARIABLES_MAX_SCOPE_SIZE);
		return FALSE;
	}
	
	arg->argument = sieve_argument_create(ast, &variable_argument, this_ext, 0);
	arg->argument->data = (void *) var;
	return TRUE;
}

static struct sieve_ast_argument *ext_variables_variable_argument_create
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr, 
	struct sieve_ast_argument *parent_arg, const char *variable)
{
	struct sieve_ast *ast = parent_arg->ast;
	struct sieve_ast_argument *new_arg;
		
	new_arg = sieve_ast_argument_create(ast, sieve_ast_argument_line(parent_arg));
	new_arg->type = SAAT_STRING;

	if ( !ext_variables_variable_argument_activate
		(this_ext, valdtr, new_arg, variable) )
		return NULL;
	
	return new_arg;
}

static bool arg_variable_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *context ATTR_UNUSED)
{
	struct sieve_argument *argument = arg->argument;
	struct sieve_variable *var = (struct sieve_variable *) argument->data;
	
	sieve_variables_opr_variable_emit(cgenv->sblock, argument->ext, var);

	return TRUE;
}

/* 
 * Match value argument implementation
 */

static bool arg_match_value_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *context ATTR_UNUSED);

const struct sieve_argument_def match_value_argument = { 
	"@match_value", 
	NULL, NULL, NULL, NULL,
	arg_match_value_generate 
};

static bool ext_variables_match_value_argument_activate
(const struct sieve_extension *this_ext, 
	struct sieve_validator *valdtr, struct sieve_ast_argument *arg, 
	unsigned int index, bool assignment)
{
	struct sieve_ast *ast = arg->ast;

	if ( assignment ) {
		sieve_argument_validate_error(valdtr, arg, 
			"cannot assign to match variable");
		return FALSE;
	}

	if ( index > EXT_VARIABLES_MAX_MATCH_INDEX ) {
		sieve_argument_validate_error(valdtr, arg, 
			"match value index %u out of range (max: %u)", index, 
			EXT_VARIABLES_MAX_MATCH_INDEX);
		return FALSE;
	} 

	arg->argument = sieve_argument_create
		(ast, &match_value_argument, this_ext, 0);
	arg->argument->data = (void *) POINTER_CAST(index);
	return TRUE;
}

static struct sieve_ast_argument *ext_variables_match_value_argument_create
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr, 
	struct sieve_ast_argument *parent_arg, unsigned int index)
{
	struct sieve_ast *ast = parent_arg->ast;
	struct sieve_ast_argument *new_arg;
	
	new_arg = sieve_ast_argument_create(ast, sieve_ast_argument_line(parent_arg));
	new_arg->type = SAAT_STRING;

	if ( !ext_variables_match_value_argument_activate
		(this_ext, valdtr, new_arg, index, FALSE) ) {
		return NULL;
  }
	
	return new_arg;
}

static bool arg_match_value_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *context ATTR_UNUSED)
{
	struct sieve_argument *argument = arg->argument;
	unsigned int index = POINTER_CAST_TO(argument->data, unsigned int);
	
	sieve_variables_opr_match_value_emit(cgenv->sblock, argument->ext, index);

	return TRUE;
}

/* 
 * Variable string argument implementation
 */

static bool arg_variable_string_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);

const struct sieve_argument_def variable_string_argument = { 
	"@variable-string", 
	NULL,
	arg_variable_string_validate, 
	NULL, NULL, 
	sieve_arg_catenated_string_generate,
};

static bool arg_variable_string_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	const struct sieve_extension *this_ext = (*arg)->argument->ext;
	enum { ST_NONE, ST_OPEN, ST_VARIABLE, ST_CLOSE } state = ST_NONE;
	pool_t pool = sieve_ast_pool((*arg)->ast);
	struct sieve_arg_catenated_string *catstr = NULL;
	string_t *str = sieve_ast_argument_str(*arg);
	const char *p, *strstart, *substart = NULL;
	const char *strval = (const char *) str_data(str);
	const char *strend = strval + str_len(str);
	bool result = TRUE;
	ARRAY_TYPE(sieve_variable_name) substitution;	
	int nelements = 0;
	
	T_BEGIN {
		/* Initialize substitution structure */
		t_array_init(&substitution, 2);		
	
		p = strval;
		strstart = p;
		while ( result && p < strend ) {
			switch ( state ) {

			/* Nothing found yet */
			case ST_NONE:
				if ( *p == '$' ) {
					substart = p;
					state = ST_OPEN;
				}
				p++;
				break;

			/* Got '$' */
			case ST_OPEN:
				if ( *p == '{' ) {
					state = ST_VARIABLE;
					p++;
				} else 
					state = ST_NONE;
				break;

			/* Got '${' */ 
			case ST_VARIABLE:
				nelements = ext_variable_name_parse(&substitution, &p, strend);
			
				if ( nelements < 0 )
					state = ST_NONE;
				else 
					state = ST_CLOSE;
			
				break;

			/* Finished parsing name, expecting '}' */
			case ST_CLOSE:
				if ( *p == '}' ) {				
					struct sieve_ast_argument *strarg;
				
					/* We now know that the substitution is valid */	
					
					if ( catstr == NULL ) {
						catstr = sieve_arg_catenated_string_create(*arg);
					}
				
					/* Add the substring that is before the substitution to the 
					 * variable-string AST.
					 *
					 * FIXME: For efficiency, if the variable is not found we should 
					 * coalesce this substring with the one after the substitution.
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
				
					/* Find the variable */
					if ( nelements == 1 ) {
						const struct sieve_variable_name *cur_element = 
							array_idx(&substitution, 0);
						
						if ( cur_element->num_variable == -1 ) {
							/* Add variable argument '${identifier}' */

							strarg = ext_variables_variable_argument_create
								(this_ext, valdtr, *arg, str_c(cur_element->identifier));

						} else {
							/* Add match value argument '${000}' */
				
							strarg = ext_variables_match_value_argument_create
								(this_ext, valdtr, *arg, cur_element->num_variable);
						}
					} else {
						strarg = ext_variables_namespace_argument_create
							(this_ext, valdtr, *arg, cmd, &substitution);				
					}

					if ( strarg != NULL ) {
						sieve_arg_catenated_string_add_element(catstr, strarg);
					} else {
						result = FALSE;
						break;
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

/*
 * Variable argument interface
 */

static bool _sieve_variable_argument_activate
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr, 
	struct sieve_command *cmd, struct sieve_ast_argument *arg, bool assignment)
{
	bool result = FALSE;
	string_t *variable;
	const char *varstr, *varend;
	ARRAY_TYPE(sieve_variable_name) vname;
	int nelements = 0;

	T_BEGIN {
		t_array_init(&vname, 2);

		variable = sieve_ast_argument_str(arg);
		varstr = str_c(variable);
		varend = PTR_OFFSET(varstr, str_len(variable));
		nelements = ext_variable_name_parse(&vname, &varstr, varend);

		/* Check whether name parsing succeeded */
		if ( nelements <= 0 || varstr != varend ) {
			/* Parse failed */
			sieve_argument_validate_error(valdtr, arg, 
				"invalid variable name '%s'", str_sanitize(str_c(variable),80));
		} else if ( nelements == 1 ) {
			/* Normal (match) variable */

			const struct sieve_variable_name *cur_element = 
				array_idx(&vname, 0);

			if ( cur_element->num_variable < 0 ) {
				/* Variable */
				result = ext_variables_variable_argument_activate
					(this_ext, valdtr, arg, str_c(cur_element->identifier));

			} else {
				/* Match value */	
				result = ext_variables_match_value_argument_activate
					(this_ext, valdtr, arg, cur_element->num_variable, assignment);
			}

		} else {
			/* Namespace variable */
			result = ext_variables_namespace_argument_activate
				(this_ext, valdtr, arg, cmd, &vname, assignment);
		}
	} T_END;

	return result;
}

bool sieve_variable_argument_activate
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr, 
	struct sieve_command *cmd, struct sieve_ast_argument *arg, 
	bool assignment)
{
	if ( sieve_ast_argument_type(arg) == SAAT_STRING ) {
		/* Single string */
		return _sieve_variable_argument_activate
			(this_ext, valdtr, cmd, arg, assignment);
		
	} else if ( sieve_ast_argument_type(arg) == SAAT_STRING_LIST ) {
		/* String list */
		struct sieve_ast_argument *stritem;
		
		i_assert ( !assignment );
		
		stritem = sieve_ast_strlist_first(arg);
		while ( stritem != NULL ) {
			if ( !_sieve_variable_argument_activate
				(this_ext, valdtr, cmd, stritem, assignment) )
				return FALSE;
			
			stritem = sieve_ast_strlist_next(stritem);
		}
		
		arg->argument = sieve_argument_create
			(arg->ast, &string_list_argument, NULL, 0);
		
		return TRUE;
	} 
	
	return FALSE;
}


