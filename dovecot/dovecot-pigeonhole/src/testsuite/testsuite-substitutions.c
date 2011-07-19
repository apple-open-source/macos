/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve.h"
#include "sieve-code.h"
#include "sieve-commands.h"
#include "sieve-binary.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "testsuite-common.h"
#include "testsuite-substitutions.h"

/*
 * Forward declarations
 */
 
void testsuite_opr_substitution_emit
	(struct sieve_binary_block *sblock, const struct testsuite_substitution *tsub,
		const char *param);
			
/*
 * Testsuite substitutions
 */
 
/* FIXME: make this extendible */

enum {
	TESTSUITE_SUBSTITUTION_FILE,
};

static const struct testsuite_substitution_def testsuite_file_substitution;

static const struct testsuite_substitution_def *substitutions[] = {
	&testsuite_file_substitution,
};

static const unsigned int substitutions_count = N_ELEMENTS(substitutions);
 
static inline const struct testsuite_substitution_def *
testsuite_substitution_get
(unsigned int code)
{
	if ( code > substitutions_count )
		return NULL;
	
	return substitutions[code];
}

static const struct testsuite_substitution *testsuite_substitution_create
(struct sieve_ast *ast, const char *identifier)
{
	unsigned int i; 
	
	for ( i = 0; i < substitutions_count; i++ ) {
		if ( strcasecmp(substitutions[i]->obj_def.identifier, identifier) == 0 ) {
			const struct testsuite_substitution_def *tsub_def = substitutions[i];
			struct testsuite_substitution *tsub;

			tsub = p_new(sieve_ast_pool(ast), struct testsuite_substitution, 1);
			tsub->object.def = &tsub_def->obj_def;
			tsub->object.ext = testsuite_ext;
			tsub->def = tsub_def; 
		
			return tsub;
		}
	}
	
	return NULL;
}

/*
 * Substitution argument
 */
 
static bool arg_testsuite_substitution_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *context);

struct _testsuite_substitution_context {
	const struct testsuite_substitution *tsub;
	const char *param;
};

const struct sieve_argument_def testsuite_substitution_argument = { 
	"@testsuite-substitution", 
	NULL, NULL, NULL, NULL,
	arg_testsuite_substitution_generate 
};

struct sieve_ast_argument *testsuite_substitution_argument_create
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_ast *ast, 
	unsigned int source_line, const char *substitution, const char *param)
{
	const struct testsuite_substitution *tsub;
	struct _testsuite_substitution_context *tsctx;
	struct sieve_ast_argument *arg;
	pool_t pool;
	
	tsub = testsuite_substitution_create(ast, substitution);
	if ( tsub == NULL ) 
		return NULL;
	
	arg = sieve_ast_argument_create(ast, source_line);
	arg->type = SAAT_STRING;

	pool = sieve_ast_pool(ast);
	tsctx = p_new(pool, struct _testsuite_substitution_context, 1);
	tsctx->tsub = tsub;
	tsctx->param = p_strdup(pool, param);

	arg->argument = sieve_argument_create
		(ast, &testsuite_substitution_argument, testsuite_ext, 0);
	arg->argument->data = (void *) tsctx;

	return arg;
}

static bool arg_testsuite_substitution_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *context ATTR_UNUSED)
{
	struct _testsuite_substitution_context *tsctx =  
		(struct _testsuite_substitution_context *) arg->argument->data;
	
	testsuite_opr_substitution_emit(cgenv->sblock, tsctx->tsub, tsctx->param);

	return TRUE;
}

/*
 * Substitution operand
 */

static bool opr_substitution_dump
	(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
		sieve_size_t *address);
static int opr_substitution_read_value
	(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd, 
		sieve_size_t *address, string_t **str);
	
const struct sieve_opr_string_interface testsuite_substitution_interface = { 
	opr_substitution_dump,
	opr_substitution_read_value
};
		
const struct sieve_operand_def testsuite_substitution_operand = { 
	"test-substitution", 
	&testsuite_extension, 
	TESTSUITE_OPERAND_SUBSTITUTION,
	&string_class,
	&testsuite_substitution_interface
};

void testsuite_opr_substitution_emit
(struct sieve_binary_block *sblock, const struct testsuite_substitution *tsub,
	const char *param) 
{
	/* Default variable storage */
	(void) sieve_operand_emit
		(sblock, testsuite_ext, &testsuite_substitution_operand);
	(void) sieve_binary_emit_unsigned(sblock, tsub->object.def->code);
	(void) sieve_binary_emit_cstring(sblock, param);
}

static bool opr_substitution_dump
(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd, 
	sieve_size_t *address) 
{
	unsigned int code = 0;
	const struct testsuite_substitution_def *tsub;
	string_t *param; 

	if ( !sieve_binary_read_unsigned(denv->sblock, address, &code) )
		return FALSE;
		
	tsub = testsuite_substitution_get(code);
	if ( tsub == NULL )
		return FALSE;	
			
	if ( !sieve_binary_read_string(denv->sblock, address, &param) )
		return FALSE;
	
	if ( oprnd->field_name != NULL ) 
		sieve_code_dumpf(denv, "%s: TEST_SUBS %%{%s:%s}", 
			oprnd->field_name, tsub->obj_def.identifier, str_c(param));
	else
		sieve_code_dumpf(denv, "TEST_SUBS %%{%s:%s}", 
			tsub->obj_def.identifier, str_c(param));
	return TRUE;
}

static int opr_substitution_read_value
(const struct sieve_runtime_env *renv, 
	const struct sieve_operand *oprnd ATTR_UNUSED, sieve_size_t *address, 
	string_t **str_r)
{ 
	const struct testsuite_substitution_def *tsub;
	unsigned int code = 0;
	string_t *param;
	
	if ( !sieve_binary_read_unsigned(renv->sblock, address, &code) )
		return SIEVE_EXEC_BIN_CORRUPT;
		
	tsub = testsuite_substitution_get(code);
	if ( tsub == NULL )
		return SIEVE_EXEC_FAILURE;	

	/* Parameter str can be NULL if we are requested to only skip and not 
	 * actually read the argument.
	 */	
	if ( str_r == NULL ) {
		if ( !sieve_binary_read_string(renv->sblock, address, NULL) )
			return SIEVE_EXEC_BIN_CORRUPT;
		
		return SIEVE_EXEC_OK;
	}
	
	if ( !sieve_binary_read_string(renv->sblock, address, &param) )
		return SIEVE_EXEC_BIN_CORRUPT;
				
	if ( !tsub->get_value(str_c(param), str_r) )
		return SIEVE_EXEC_FAILURE;
	
	return SIEVE_EXEC_OK;
}

/*
 * Testsuite substitution definitions
 */
 
static bool testsuite_file_substitution_get_value
	(const char *param, string_t **result); 
 
static const struct testsuite_substitution_def testsuite_file_substitution = {
	SIEVE_OBJECT(
		"file", 
		&testsuite_substitution_operand, 
		TESTSUITE_SUBSTITUTION_FILE
	),
	testsuite_file_substitution_get_value
};

static bool testsuite_file_substitution_get_value
	(const char *param, string_t **result)
{
	*result = t_str_new(256);

	str_printfa(*result, "[FILE: %s]", param);
	return TRUE;
}

