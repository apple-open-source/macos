/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "mempool.h"

#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-binary.h"

#include "sieve-generator.h"

/* 
 * Jump list 
 */

struct sieve_jumplist *sieve_jumplist_create
(pool_t pool, struct sieve_binary_block *sblock)
{
	struct sieve_jumplist *jlist;
	
	jlist = p_new(pool, struct sieve_jumplist, 1);
	jlist->block = sblock;
	p_array_init(&jlist->jumps, pool, 4);
	
	return jlist;
}

void sieve_jumplist_init_temp
(struct sieve_jumplist *jlist, struct sieve_binary_block *sblock)
{
	jlist->block = sblock;
	t_array_init(&jlist->jumps, 4);
}

void sieve_jumplist_reset
	(struct sieve_jumplist *jlist)
{
	array_clear(&jlist->jumps);
}

void sieve_jumplist_add(struct sieve_jumplist *jlist, sieve_size_t jump) 
{
	array_append(&jlist->jumps, &jump, 1);
}

void sieve_jumplist_resolve(struct sieve_jumplist *jlist) 
{
	unsigned int i;
	
	for ( i = 0; i < array_count(&jlist->jumps); i++ ) {
		const sieve_size_t *jump = array_idx(&jlist->jumps, i);
	
		sieve_binary_resolve_offset(jlist->block, *jump);
	}
}

/* 
 * Code Generator 
 */

struct sieve_generator {
	pool_t pool;

	struct sieve_instance *instance;
	
	struct sieve_error_handler *ehandler;

	struct sieve_codegen_env genenv;
	struct sieve_binary_debug_writer *dwriter;
	
	ARRAY_DEFINE(ext_contexts, void *);
};

struct sieve_generator *sieve_generator_create
(struct sieve_ast *ast, struct sieve_error_handler *ehandler) 
{
	pool_t pool;
	struct sieve_generator *gentr;
	struct sieve_script *script;
	struct sieve_instance *svinst;
	
	pool = pool_alloconly_create("sieve_generator", 4096);	
	gentr = p_new(pool, struct sieve_generator, 1);
	gentr->pool = pool;

	gentr->ehandler = ehandler;
	sieve_error_handler_ref(ehandler);
	
	gentr->genenv.gentr = gentr;
	gentr->genenv.ast = ast;	
	sieve_ast_ref(ast);

	script = sieve_ast_script(ast);
	svinst = sieve_script_svinst(script);

	gentr->genenv.script = script;
	gentr->genenv.svinst = svinst;

	/* Setup storage for extension contexts */		
	p_array_init(&gentr->ext_contexts, pool, sieve_extensions_get_count(svinst));
		
	return gentr;
}

void sieve_generator_free(struct sieve_generator **gentr) 
{
	sieve_ast_unref(&(*gentr)->genenv.ast);
		
	sieve_error_handler_unref(&(*gentr)->ehandler);
	sieve_binary_debug_writer_deinit(&(*gentr)->dwriter);

	if ( (*gentr)->genenv.sbin != NULL )
		sieve_binary_unref(&(*gentr)->genenv.sbin);

	pool_unref(&((*gentr)->pool));
	
	*gentr = NULL;
}

/* 
 * Accessors 
 */

struct sieve_error_handler *sieve_generator_error_handler
(struct sieve_generator *gentr)
{
	return gentr->ehandler;
}

pool_t sieve_generator_pool(struct sieve_generator *gentr)
{
	return gentr->pool;
}

struct sieve_script *sieve_generator_script
(struct sieve_generator *gentr)
{
	return gentr->genenv.script;
}

struct sieve_binary *sieve_generator_get_binary
(struct sieve_generator *gentr)
{
	return gentr->genenv.sbin;
}

struct sieve_binary_block *sieve_generator_get_block
(struct sieve_generator *gentr)
{
	return gentr->genenv.sblock;
}

/* 
 * Error handling 
 */

void sieve_generator_warning
(struct sieve_generator *gentr, unsigned int source_line, 
	const char *fmt, ...) 
{ 
	va_list args;
	
	va_start(args, fmt);
	sieve_vwarning(gentr->ehandler,
        sieve_error_script_location(gentr->genenv.script, source_line),
        fmt, args);
	va_end(args);
}
 
void sieve_generator_error
(struct sieve_generator *gentr, unsigned int source_line, 
	const char *fmt, ...) 
{
	va_list args;
	
	va_start(args, fmt);
	sieve_verror(gentr->ehandler,
        sieve_error_script_location(gentr->genenv.script, source_line),
        fmt, args);
	va_end(args);
}

void sieve_generator_critical
(struct sieve_generator *gentr, unsigned int source_line, 
	const char *fmt, ...) 
{
	va_list args;
	
	va_start(args, fmt);
	sieve_vwarning(gentr->ehandler,
        sieve_error_script_location(gentr->genenv.script, source_line),
        fmt, args);
	va_end(args);
}

/* 
 * Extension support 
 */

void sieve_generator_extension_set_context
(struct sieve_generator *gentr, const struct sieve_extension *ext, void *context)
{
	if ( ext->id < 0 ) return;
	
	array_idx_set(&gentr->ext_contexts, (unsigned int) ext->id, &context);	
}

const void *sieve_generator_extension_get_context
(struct sieve_generator *gentr, const struct sieve_extension *ext) 
{
	void * const *ctx;

	if  ( ext->id < 0 || ext->id >= (int) array_count(&gentr->ext_contexts) )
		return NULL;
	
	ctx = array_idx(&gentr->ext_contexts, (unsigned int) ext->id);		

	return *ctx;
}

/* 
 * Code generation API
 */

static void sieve_generate_debug_from_ast_node
(const struct sieve_codegen_env *cgenv, struct sieve_ast_node *ast_node)
{
	sieve_size_t address = sieve_binary_block_get_size(cgenv->sblock);
	unsigned int line = sieve_ast_node_line(ast_node);

	sieve_binary_debug_emit(cgenv->gentr->dwriter, address, line, 0);
}

static void sieve_generate_debug_from_ast_argument
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *ast_arg)
{
	sieve_size_t address = sieve_binary_block_get_size(cgenv->sblock);
	unsigned int line = sieve_ast_argument_line(ast_arg);

	sieve_binary_debug_emit(cgenv->gentr->dwriter, address, line, 0);
}

bool sieve_generate_argument
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *cmd)
{
	const struct sieve_argument_def *arg_def;
	
	if ( arg->argument == NULL || arg->argument->def == NULL ) return FALSE;

	arg_def = arg->argument->def;
	
	if ( arg_def->generate == NULL )
		return TRUE;

	sieve_generate_debug_from_ast_argument(cgenv, arg);

	return arg_def->generate(cgenv, arg, cmd);
}

bool sieve_generate_arguments
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd, 
	struct sieve_ast_argument **last_arg_r)
{
	enum { ARG_START, ARG_OPTIONAL, ARG_POSITIONAL } state = ARG_START;
	struct sieve_ast_argument *arg = sieve_ast_argument_first(cmd->ast_node);
	
	/* Generate all arguments with assigned generator function */
	
	while ( arg != NULL ) {
		const struct sieve_argument *argument;
		const struct sieve_argument_def *arg_def;
		
		if ( arg->argument == NULL || arg->argument->def == NULL )
			return FALSE;

		argument = arg->argument;
		arg_def = argument->def;

		switch ( state ) {
		case ARG_START: 
			if ( argument->id_code == 0 )
				state = ARG_POSITIONAL;
			else {
				/* Mark start of optional operands with 0 operand identifier */
				sieve_binary_emit_byte(cgenv->sblock, SIEVE_OPERAND_OPTIONAL);
								
				/* Emit argument id for optional operand */
				sieve_binary_emit_byte
					(cgenv->sblock, (unsigned char) argument->id_code);

				state = ARG_OPTIONAL;
			}
			break;
		case ARG_OPTIONAL: 
			if ( argument->id_code == 0 )
				state = ARG_POSITIONAL;
			
			/* Emit argument id for optional operand (0 marks the end of the optionals) */
			sieve_binary_emit_byte
				(cgenv->sblock, (unsigned char) argument->id_code);

			break;
		case ARG_POSITIONAL:
			if ( argument->id_code != 0 )
				return FALSE;
			break;
		}
		
		/* Call the generation function for the argument */ 
		if ( arg_def->generate != NULL ) {
			sieve_generate_debug_from_ast_argument(cgenv, arg);
 
			if ( !arg_def->generate(cgenv, arg, cmd) ) 
				return FALSE;
		} else if ( state == ARG_POSITIONAL ) break;

		arg = sieve_ast_argument_next(arg);
	}

	/* Mark end of optional list if it is still open */
	if ( state == ARG_OPTIONAL )
		sieve_binary_emit_byte(cgenv->sblock, 0);
	
	if ( last_arg_r != NULL )
		*last_arg_r = arg;
	
	return TRUE;
}

bool sieve_generate_argument_parameters
(const struct sieve_codegen_env *cgenv, 
	struct sieve_command *cmd, struct sieve_ast_argument *arg)
{
	struct sieve_ast_argument *param = arg->parameters;
	
	/* Generate all parameters with assigned generator function */
	
	while ( param != NULL ) {
		if ( param->argument != NULL && param->argument->def != NULL ) {
			const struct sieve_argument_def *parameter = param->argument->def;
				
			/* Call the generation function for the parameter */ 
			if ( parameter->generate != NULL ) {
				sieve_generate_debug_from_ast_argument(cgenv, param);
			 
				if ( !parameter->generate(cgenv, param, cmd) ) 
					return FALSE;
			}
		}

		param = sieve_ast_argument_next(param);
	}
		
	return TRUE;
}

bool sieve_generate_test
(const struct sieve_codegen_env *cgenv, struct sieve_ast_node *tst_node,
	struct sieve_jumplist *jlist, bool jump_true) 
{
	struct sieve_command *test;
	const struct sieve_command_def *tst_def;

	i_assert( tst_node->command != NULL && tst_node->command->def != NULL );

	test = tst_node->command;
	tst_def = test->def;

	if ( tst_def->control_generate != NULL ) {
		sieve_generate_debug_from_ast_node(cgenv, tst_node);
		
		if ( tst_def->control_generate(cgenv, test, jlist, jump_true) ) 
			return TRUE;
		
		return FALSE;
	}
	
	if ( tst_def->generate != NULL ) {
		sieve_generate_debug_from_ast_node(cgenv, tst_node);

		if ( tst_def->generate(cgenv, test) ) {
			
			if ( jump_true ) 
				sieve_operation_emit(cgenv->sblock, NULL, &sieve_jmptrue_operation);
			else
				sieve_operation_emit(cgenv->sblock, NULL, &sieve_jmpfalse_operation);
			sieve_jumplist_add(jlist, sieve_binary_emit_offset(cgenv->sblock, 0));
						
			return TRUE;
		}	
		
		return FALSE;
	}
	
	return TRUE;
}

static bool sieve_generate_command
(const struct sieve_codegen_env *cgenv, struct sieve_ast_node *cmd_node) 
{
	struct sieve_command *command;
	const struct sieve_command_def *cmd_def;

	i_assert( cmd_node->command != NULL && cmd_node->command->def != NULL );

	command = cmd_node->command;
	cmd_def = command->def;

	if ( cmd_def->generate != NULL ) {
		sieve_generate_debug_from_ast_node(cgenv, cmd_node);

		return cmd_def->generate(cgenv, command);
	}
	
	return TRUE;		
}

bool sieve_generate_block
(const struct sieve_codegen_env *cgenv, struct sieve_ast_node *block) 
{
	bool result = TRUE;
	struct sieve_ast_node *cmd_node;

	T_BEGIN {	
		cmd_node = sieve_ast_command_first(block);
		while ( result && cmd_node != NULL ) {	
			result = sieve_generate_command(cgenv, cmd_node);	
			cmd_node = sieve_ast_command_next(cmd_node);
		}		
	} T_END;
	
	return result;
}

struct sieve_binary *sieve_generator_run
(struct sieve_generator *gentr, struct sieve_binary_block **sblock_r) 
{
	bool topmost = ( sblock_r == NULL || *sblock_r == NULL );
	struct sieve_binary *sbin;
	struct sieve_binary_block *sblock, *debug_block;
	const struct sieve_extension *const *extensions;
	unsigned int i, ext_count;
	bool result = TRUE;
	
	/* Initialize */
	
	if ( topmost ) {
		sbin = sieve_binary_create_new(sieve_ast_script(gentr->genenv.ast));
		sblock = sieve_binary_block_get(sbin, SBIN_SYSBLOCK_MAIN_PROGRAM);
	} else {
		sblock = *sblock_r;
		sbin = sieve_binary_block_get_binary(sblock);
	}
	
	sieve_binary_ref(sbin);
	gentr->genenv.sbin = sbin;
	gentr->genenv.sblock = sblock;

	/* Create debug block */
	debug_block = sieve_binary_block_create(sbin);
	gentr->dwriter = sieve_binary_debug_writer_init(debug_block);
	(void)sieve_binary_emit_unsigned
		(sblock, sieve_binary_block_get_id(debug_block));
		
	/* Load extensions linked to the AST and emit a list in code */
	extensions = sieve_ast_extensions_get(gentr->genenv.ast, &ext_count);
	(void) sieve_binary_emit_unsigned(sblock, ext_count);
	for ( i = 0; i < ext_count && sbin != NULL; i++ ) {
		const struct sieve_extension *ext = extensions[i];

		/* Link to binary */
		(void)sieve_binary_extension_link(sbin, ext);
	
		/* Emit */
		sieve_binary_emit_extension(sblock, ext, 0);
	
		/* Load */
		if ( ext->def != NULL && ext->def->generator_load != NULL &&
			!ext->def->generator_load(ext, &gentr->genenv) )
			result = FALSE;
	}

	/* Generate code */
	
	if ( result ) {
		if ( !sieve_generate_block
			(&gentr->genenv, sieve_ast_root(gentr->genenv.ast))) 
			result = FALSE;
		else if ( topmost ) 
			sieve_binary_activate(sbin);
	}

	/* Cleanup */
		
	gentr->genenv.sbin = NULL;
	gentr->genenv.sblock = NULL;
	sieve_binary_unref(&sbin);

	if ( !result ) {
		if ( topmost ) {
			sieve_binary_unref(&sbin);
			if ( sblock_r != NULL )
				*sblock_r = NULL;
		}
		sbin = NULL;
	} else {
		if ( sblock_r != NULL )
			*sblock_r = sblock;
	}
	
	return sbin;
}



