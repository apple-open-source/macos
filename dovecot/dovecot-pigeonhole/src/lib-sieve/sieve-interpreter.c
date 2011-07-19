/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ostream.h"
#include "mempool.h"
#include "array.h"
#include "hash.h"
#include "mail-storage.h"

#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-error.h"
#include "sieve-extensions.h"
#include "sieve-message.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-actions.h"
#include "sieve-generator.h"
#include "sieve-binary.h"
#include "sieve-result.h"
#include "sieve-comparators.h"
#include "sieve-runtime-trace.h"

#include "sieve-interpreter.h"

#include <string.h>

/* 
 * Interpreter extension 
 */

struct sieve_interpreter_extension_reg {
	const struct sieve_interpreter_extension *intext;
	const struct sieve_extension *ext;

	void *context;
};

/* 
 * Interpreter 
 */

struct sieve_interpreter {
	pool_t pool;
			
	struct sieve_error_handler *ehandler;

	/* Runtime data for extensions */
	ARRAY_DEFINE(extensions, struct sieve_interpreter_extension_reg); 
	
	sieve_size_t reset_vector;	
		
	/* Execution status */
	
	sieve_size_t pc;          /* Program counter */
	bool interrupted;         /* Interpreter interrupt requested */
	bool test_result;         /* Result of previous test command */
	
	/* Runtime environment */
	struct sieve_runtime_env runenv;
	struct sieve_runtime_trace trace;

	/* Current operation */
	struct sieve_operation oprtn; 

	/* Location information */
	struct sieve_binary_debug_reader *dreader;
	unsigned int command_line;
};

static struct sieve_interpreter *_sieve_interpreter_create
(struct sieve_binary *sbin, struct sieve_binary_block *sblock, 
	struct sieve_script *script, const struct sieve_message_data *msgdata,
	const struct sieve_script_env *senv, struct sieve_error_handler *ehandler) 
{
	unsigned int i, ext_count;
	struct sieve_interpreter *interp;
	pool_t pool;
	struct sieve_instance *svinst;
	const struct sieve_extension *const *ext_preloaded;
	unsigned int debug_block_id;
	sieve_size_t *address;
	bool success = TRUE;
	
	pool = pool_alloconly_create("sieve_interpreter", 4096);	
	interp = p_new(pool, struct sieve_interpreter, 1);
	interp->pool = pool;

	interp->ehandler = ehandler;
	sieve_error_handler_ref(ehandler);

	interp->runenv.interp = interp;
	interp->runenv.oprtn = &interp->oprtn;
	interp->runenv.sbin = sbin;
	interp->runenv.sblock = sblock;
	sieve_binary_ref(sbin);

	svinst = sieve_binary_svinst(sbin);

	interp->runenv.svinst = svinst;
	interp->runenv.msgdata = msgdata;
	interp->runenv.scriptenv = senv;

	if ( senv->trace_stream != NULL ) {
		interp->trace.stream = senv->trace_stream;
		interp->trace.config = senv->trace_config;
		interp->trace.indent = 0;
		interp->runenv.trace = &interp->trace;
	}

	if ( senv->exec_status == NULL ) 
		interp->runenv.exec_status = p_new(interp->pool, struct sieve_exec_status, 1);
	else
		interp->runenv.exec_status = senv->exec_status;

	if ( script == NULL )	
		interp->runenv.script = sieve_binary_script(sbin);
	else
		interp->runenv.script = script;
	
	interp->runenv.pc = 0;
	address = &(interp->runenv.pc);

	sieve_runtime_trace_begin(&(interp->runenv));

	p_array_init(&interp->extensions, pool, sieve_extensions_get_count(svinst));

	/* Pre-load core language features implemented as 'extensions' */
	ext_preloaded = sieve_extensions_get_preloaded(svinst, &ext_count); 
	for ( i = 0; i < ext_count; i++ ) {
		const struct sieve_extension_def *ext_def = ext_preloaded[i]->def;

		if ( ext_def != NULL && ext_def->interpreter_load != NULL )
			(void)ext_def->interpreter_load
				(ext_preloaded[i], &interp->runenv, address);		
	}

	/* Load debug block */
	if ( sieve_binary_read_unsigned(sblock, address, &debug_block_id) ) {
		struct sieve_binary_block *debug_block =
			sieve_binary_block_get(sbin, debug_block_id);

		if ( debug_block == NULL ) {
			sieve_runtime_trace_error(&interp->runenv, "invalid id for debug block");
			success = FALSE;
		} else {
			/* Initialize debug reader */
			interp->dreader = sieve_binary_debug_reader_init(debug_block);
		}
	}

	/* Load other extensions listed in code */
	if ( success && 
		sieve_binary_read_unsigned(sblock, address, &ext_count) ) {

		for ( i = 0; i < ext_count; i++ ) {
			unsigned int code = 0;
			const struct sieve_extension *ext;
			
			if ( !sieve_binary_read_extension(sblock, address, &code, &ext) ) {
				success = FALSE;
				break;
			}
 
			if ( ext->def != NULL && ext->def->interpreter_load != NULL && 
				!ext->def->interpreter_load(ext, &interp->runenv, address) ) {
				success = FALSE;
				break;
			}
		}
	}	else
		success = FALSE;
	
	if ( !success ) {
		sieve_interpreter_free(&interp);
		interp = NULL;
	} else {
		interp->reset_vector = *address;
	}
	
	return interp;
}

struct sieve_interpreter *sieve_interpreter_create
(struct sieve_binary *sbin, const struct sieve_message_data *msgdata,
	const struct sieve_script_env *senv, struct sieve_error_handler *ehandler) 
{
	struct sieve_binary_block *sblock;

	if ( (sblock=sieve_binary_block_get(sbin, SBIN_SYSBLOCK_MAIN_PROGRAM)) 
		== NULL )
		return NULL;

 	return _sieve_interpreter_create(sbin, sblock, NULL, msgdata, senv, ehandler);
}

struct sieve_interpreter *sieve_interpreter_create_for_block
(struct sieve_binary_block *sblock, struct sieve_script *script,
	const struct sieve_message_data *msgdata, const struct sieve_script_env *senv, 
	struct sieve_error_handler *ehandler) 
{
	if ( sblock == NULL ) return NULL;

 	return _sieve_interpreter_create
		(sieve_binary_block_get_binary(sblock), sblock, script, msgdata, senv,
			ehandler);
}

void sieve_interpreter_free(struct sieve_interpreter **interp) 
{
	const struct sieve_interpreter_extension_reg *eregs;
	unsigned int ext_count, i;

	sieve_runtime_trace_end(&((*interp)->runenv));

	/* Signal registered extensions that the interpreter is being destroyed */
	eregs = array_get(&(*interp)->extensions, &ext_count);
	for ( i = 0; i < ext_count; i++ ) {
		if ( eregs[i].intext != NULL && eregs[i].intext->free != NULL )
			eregs[i].intext->free(eregs[i].ext, *interp, eregs[i].context);
	}

	sieve_binary_debug_reader_deinit(&(*interp)->dreader);
	sieve_binary_unref(&(*interp)->runenv.sbin);
	sieve_error_handler_unref(&(*interp)->ehandler);

	pool_unref(&((*interp)->pool));	
	*interp = NULL;
}

/*
 * Accessors
 */

pool_t sieve_interpreter_pool(struct sieve_interpreter *interp)
{
	return interp->pool;
}

struct sieve_script *sieve_interpreter_script
(struct sieve_interpreter *interp)
{
	return interp->runenv.script;
}

struct sieve_error_handler *sieve_interpreter_get_error_handler
(struct sieve_interpreter *interp)
{
	return interp->ehandler;
}

struct sieve_instance *sieve_interpreter_svinst
(struct sieve_interpreter *interp)
{
	return interp->runenv.svinst;
}

/* Do not use this function for normal sieve extensions. This is intended for
 * the testsuite only.
 */
void sieve_interpreter_set_result
(struct sieve_interpreter *interp, struct sieve_result *result)
{
	sieve_result_unref(&interp->runenv.result);
	interp->runenv.result = result;
	sieve_result_ref(result);
}

/*
 * Error handling
 */

static inline void sieve_runtime_vmsg
(const struct sieve_runtime_env *renv, sieve_error_vfunc_t msg_func,
	const char *location, const char *fmt, va_list args)
{
	T_BEGIN {
		if ( location == NULL )
			location = sieve_runtime_get_full_command_location(renv);

		msg_func(renv->interp->ehandler, location, fmt, args); 
	} T_END;
}

void sieve_runtime_error
(const struct sieve_runtime_env *renv, const char *location,
	const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	sieve_runtime_vmsg(renv, sieve_verror, location, fmt, args);
	va_end(args);
}

void sieve_runtime_warning
(const struct sieve_runtime_env *renv, const char *location,
	const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	sieve_runtime_vmsg(renv, sieve_vwarning, location, fmt, args);
	va_end(args);
}

void sieve_runtime_log
(const struct sieve_runtime_env *renv, const char *location,
	const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	sieve_runtime_vmsg(renv, sieve_vinfo, location, fmt, args);
	va_end(args);
}

void sieve_runtime_critical
(const struct sieve_runtime_env *renv, const char *location,
	const char *user_prefix, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	T_BEGIN {
		if ( location == NULL )
			location = sieve_runtime_get_full_command_location(renv);

		sieve_vcritical
			(renv->svinst, renv->interp->ehandler, location, user_prefix, fmt, args); 
	} T_END;

	va_end(args);
}

/*
 * Source location
 */

unsigned int sieve_runtime_get_source_location
(const struct sieve_runtime_env *renv, sieve_size_t code_address)
{
	struct sieve_interpreter *interp = renv->interp;

	if ( interp->dreader == NULL )
		return 0;

	if ( interp->command_line == 0 ) {
		interp->command_line = sieve_binary_debug_read_line
			(interp->dreader, renv->oprtn->address);
	}

	return sieve_binary_debug_read_line(interp->dreader, code_address);
}

unsigned int sieve_runtime_get_command_location
(const struct sieve_runtime_env *renv)
{
	struct sieve_interpreter *interp = renv->interp;

	if ( interp->dreader == NULL )
		return 0;

	if ( interp->command_line == 0 )
		interp->command_line = sieve_binary_debug_read_line
			(interp->dreader, renv->oprtn->address);

	return interp->command_line;
}

const char *sieve_runtime_get_full_command_location
(const struct sieve_runtime_env *renv)
{
	return sieve_error_script_location
		(renv->script, sieve_runtime_get_command_location(renv));
}

/*
 * Extension support
 */

void sieve_interpreter_extension_register
(struct sieve_interpreter *interp, const struct sieve_extension *ext,
	const struct sieve_interpreter_extension *intext, void *context)
{
	struct sieve_interpreter_extension_reg *reg;

	if ( ext->id < 0 ) return;

	reg = array_idx_modifiable(&interp->extensions, (unsigned int) ext->id);	
	reg->intext = intext;
	reg->ext = ext;
	reg->context = context;
}

void sieve_interpreter_extension_set_context
(struct sieve_interpreter *interp, const struct sieve_extension *ext, 
	void *context)
{
	struct sieve_interpreter_extension_reg *reg;

	if ( ext->id < 0 ) return;
	
	reg = array_idx_modifiable(&interp->extensions, (unsigned int) ext->id);	
	reg->context = context;
}

void *sieve_interpreter_extension_get_context
(struct sieve_interpreter *interp, const struct sieve_extension *ext) 
{
	const struct sieve_interpreter_extension_reg *reg;

	if  ( ext->id < 0 || ext->id >= (int) array_count(&interp->extensions) )
		return NULL;
	
	reg = array_idx(&interp->extensions, (unsigned int) ext->id);		

	return reg->context;
}

/* 
 * Program flow 
 */

void sieve_interpreter_reset(struct sieve_interpreter *interp) 
{
	interp->runenv.pc = interp->reset_vector;
	interp->interrupted = FALSE;
	interp->test_result = FALSE;
	interp->runenv.result = NULL;
}

void sieve_interpreter_interrupt(struct sieve_interpreter *interp)
{
	interp->interrupted = TRUE;
}

sieve_size_t sieve_interpreter_program_counter(struct sieve_interpreter *interp)
{
	return interp->runenv.pc;
}

int sieve_interpreter_program_jump
(struct sieve_interpreter *interp, bool jump)
{
	const struct sieve_runtime_env *renv = &interp->runenv;
	sieve_size_t *address = &(interp->runenv.pc);
	sieve_size_t jmp_start = *address;
	sieve_offset_t jmp_offset;
	
	if ( !sieve_binary_read_offset(renv->sblock, address, &jmp_offset) )
	{
		sieve_runtime_trace_error(renv, "invalid jump offset"); 
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	if ( jmp_start + jmp_offset <= sieve_binary_block_get_size(renv->sblock) && 
		jmp_start + jmp_offset > 0 ) 
	{	
		if ( jump ) {
			sieve_size_t jmp_addr = jmp_start + jmp_offset;

			if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
				unsigned int jmp_line = 
					sieve_runtime_get_source_location(renv, jmp_addr);

				if ( sieve_runtime_trace_hasflag(renv, SIEVE_TRFLG_ADDRESSES) ) {
					sieve_runtime_trace(renv, 0, "jumping to line %d [%08llx]", 
						jmp_line, (long long unsigned int) jmp_addr);
				} else {
					sieve_runtime_trace(renv, 0, "jumping to line %d", jmp_line);
				}
			}

			*address = jmp_addr;
		} else {
			sieve_runtime_trace(renv, 0, "not jumping");	
		}
		
		return SIEVE_EXEC_OK;
	}
	
	sieve_runtime_trace_error(renv, "jump offset out of range");
	return SIEVE_EXEC_BIN_CORRUPT;
}

/*
 * Test results
 */

void sieve_interpreter_set_test_result
(struct sieve_interpreter *interp, bool result)
{
	interp->test_result = result;
}

bool sieve_interpreter_get_test_result
(struct sieve_interpreter *interp)
{
	return interp->test_result;
}

/* 
 * Code execute 
 */

static int sieve_interpreter_operation_execute
(struct sieve_interpreter *interp) 
{
	struct sieve_operation *oprtn = &(interp->oprtn);
	sieve_size_t *address = &(interp->runenv.pc);

	sieve_runtime_trace_toplevel(&interp->runenv);

	/* Read the operation */
	if ( sieve_operation_read(interp->runenv.sblock, address, oprtn) ) {
		const struct sieve_operation_def *op = oprtn->def;
		int result = SIEVE_EXEC_OK;

		/* Reset cached command location */
		interp->command_line = 0;

		/* Execute the operation */
		if ( op->execute != NULL ) { /* Noop ? */
			T_BEGIN {
				result = op->execute(&(interp->runenv), address);
			} T_END;
		} else {
			sieve_runtime_trace
				(&interp->runenv, SIEVE_TRLVL_COMMANDS, "OP: %s (NOOP)", 
					sieve_operation_mnemonic(oprtn));
		}

		return result;
	}
	
	/* Binary corrupt */
	sieve_runtime_trace_error(&interp->runenv, "Encountered invalid operation");	
	return SIEVE_EXEC_BIN_CORRUPT;
}		

int sieve_interpreter_continue
(struct sieve_interpreter *interp, bool *interrupted) 
{
	sieve_size_t *address = &(interp->runenv.pc);
	int ret = SIEVE_EXEC_OK;
	
	sieve_result_ref(interp->runenv.result);
	interp->interrupted = FALSE;
	
	if ( interrupted != NULL )
		*interrupted = FALSE;
	
	while ( ret == SIEVE_EXEC_OK && !interp->interrupted && 
		*address < sieve_binary_block_get_size(interp->runenv.sblock) ) {
		
		ret = sieve_interpreter_operation_execute(interp);

		if ( ret != SIEVE_EXEC_OK ) {
			sieve_runtime_trace(&interp->runenv, SIEVE_TRLVL_NONE, 
				"[[EXECUTION ABORTED]]");
		}
	}
	
	if ( interrupted != NULL )
		*interrupted = interp->interrupted;
			
	sieve_result_unref(&interp->runenv.result);
	return ret;
}

int sieve_interpreter_start
(struct sieve_interpreter *interp, struct sieve_result *result, bool *interrupted) 
{
	const struct sieve_interpreter_extension_reg *eregs;
	unsigned int ext_count, i;
	
	interp->runenv.result = result;
	interp->runenv.msgctx = sieve_result_get_message_context(result);		
	
	/* Signal registered extensions that the interpreter is being run */
	eregs = array_get(&interp->extensions, &ext_count);
	for ( i = 0; i < ext_count; i++ ) {
		if ( eregs[i].intext != NULL && eregs[i].intext->run != NULL )
			eregs[i].intext->run(eregs[i].ext, &interp->runenv, eregs[i].context);
	}

	return sieve_interpreter_continue(interp, interrupted); 
}

int sieve_interpreter_run
(struct sieve_interpreter *interp, struct sieve_result *result)
{
	int ret = 0;
	
	sieve_interpreter_reset(interp);
	sieve_result_ref(result);
	
	ret = sieve_interpreter_start(interp, result, NULL);
	
	sieve_result_unref(&result);
	
	return ret;
}


