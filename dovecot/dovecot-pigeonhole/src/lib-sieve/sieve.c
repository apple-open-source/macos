/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#include "lib.h"
#include "str.h"
#include "istream.h"
#include "buffer.h"
#include "eacces-error.h"

#include "sieve-limits.h"
#include "sieve-settings.h"
#include "sieve-extensions.h"
#include "sieve-plugins.h"

#include "sieve-script.h"
#include "sieve-ast.h"
#include "sieve-binary.h"
#include "sieve-actions.h"
#include "sieve-result.h"

#include "sieve-parser.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-binary-dumper.h"

#include "sieve.h"
#include "sieve-common.h"
#include "sieve-error-private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>

/* 
 * Main Sieve library interface
 */

struct sieve_instance *sieve_init
(const struct sieve_environment *env, void *context, bool debug)
{
	struct sieve_instance *svinst;
	unsigned long long int uint_setting;
	size_t size_setting;
	pool_t pool;

	/* Create Sieve engine instance */
	pool = pool_alloconly_create("sieve", 8192);
	svinst = p_new(pool, struct sieve_instance, 1);
	svinst->pool = pool;
	svinst->env = env;
	svinst->context = context;
	svinst->debug = debug;

	sieve_errors_init(svinst);

	/* Read limits from configuration */

	svinst->max_script_size = SIEVE_DEFAULT_MAX_SCRIPT_SIZE;
	svinst->max_actions = SIEVE_DEFAULT_MAX_ACTIONS;
	svinst->max_redirects = SIEVE_DEFAULT_MAX_REDIRECTS;

	if ( sieve_setting_get_size_value
		(svinst, "sieve_max_script_size", &size_setting) ) {
		svinst->max_script_size = size_setting;
	}
	
	if ( sieve_setting_get_uint_value
		(svinst, "sieve_max_actions", &uint_setting) ) {
		svinst->max_actions = (unsigned int) uint_setting;
	}

	if ( sieve_setting_get_uint_value
		(svinst, "sieve_max_redirects", &uint_setting) ) {
		svinst->max_redirects = (unsigned int) uint_setting;
	}
	
	/* Initialize extensions */
	if ( !sieve_extensions_init(svinst) ) {
		sieve_deinit(&svinst);
		return NULL;
	}

	sieve_plugins_load(svinst, NULL, NULL);

	return svinst;
}

void sieve_deinit(struct sieve_instance **svinst)
{
	sieve_plugins_unload(*svinst);

	sieve_extensions_deinit(*svinst);

	sieve_errors_deinit(*svinst);

	pool_unref(&(*svinst)->pool);

	*svinst = NULL;
}

void sieve_set_extensions
(struct sieve_instance *svinst, const char *extensions)
{
	sieve_extensions_set_string(svinst, extensions);
}

const char *sieve_get_capabilities
(struct sieve_instance *svinst, const char *name)
{
	if ( name == NULL || *name == '\0' )
		return sieve_extensions_get_string(svinst);

	return sieve_extension_capabilities_get_string(svinst, name);
}

/*
 * Low-level compiler functions 
 */

struct sieve_ast *sieve_parse
(struct sieve_script *script, struct sieve_error_handler *ehandler,
	enum sieve_error *error_r)
{
	struct sieve_parser *parser;
	struct sieve_ast *ast = NULL;
	
	/* Parse */
	if ( (parser = sieve_parser_create(script, ehandler, error_r)) == NULL )
		return NULL;

 	if ( !sieve_parser_run(parser, &ast) ) {
 		ast = NULL;
 	} else 
		sieve_ast_ref(ast);
	
	sieve_parser_free(&parser); 	

	if ( error_r != NULL ) {
		if ( ast == NULL )
			*error_r = SIEVE_ERROR_NOT_VALID;
		else
			*error_r = SIEVE_ERROR_NONE;
	}
	
	return ast;
}

bool sieve_validate
(struct sieve_ast *ast, struct sieve_error_handler *ehandler,
	enum sieve_error *error_r)
{
	bool result = TRUE;
	struct sieve_validator *validator = sieve_validator_create(ast, ehandler);
		
	if ( !sieve_validator_run(validator) ) 
		result = FALSE;
	
	sieve_validator_free(&validator);	

	if ( error_r != NULL ) {
		if ( !result )
			*error_r = SIEVE_ERROR_NOT_VALID;
		else
			*error_r = SIEVE_ERROR_NONE;
	}
		
	return result;
}

static struct sieve_binary *sieve_generate
(struct sieve_ast *ast, struct sieve_error_handler *ehandler, 
	enum sieve_error *error_r)
{
	struct sieve_generator *generator = sieve_generator_create(ast, ehandler);
	struct sieve_binary *sbin = NULL;
		
	sbin = sieve_generator_run(generator, NULL);
	
	sieve_generator_free(&generator);
	
	if ( error_r != NULL ) {
		if ( sbin == NULL )
			*error_r = SIEVE_ERROR_NOT_VALID;
		else
			*error_r = SIEVE_ERROR_NONE;
	}

	return sbin;
}

/*
 * Sieve compilation
 */

struct sieve_binary *sieve_compile_script
(struct sieve_script *script, struct sieve_error_handler *ehandler,
	enum sieve_error *error_r) 
{
	struct sieve_ast *ast;
	struct sieve_binary *sbin;  	

	/* Parse */
	if ( (ast = sieve_parse(script, ehandler, error_r)) == NULL ) {
 		sieve_error(ehandler, sieve_script_name(script), "parse failed");
		return NULL;
	}

	/* Validate */
	if ( !sieve_validate(ast, ehandler, error_r) ) {
		sieve_error(ehandler, sieve_script_name(script), "validation failed");
		
 		sieve_ast_unref(&ast);
 		return NULL;
 	}
 	
	/* Generate */
	if ( (sbin=sieve_generate(ast, ehandler, error_r)) == NULL ) {
		sieve_error(ehandler, sieve_script_name(script), "code generation failed");
		
		sieve_ast_unref(&ast);
		return NULL;
	}

	/* Cleanup */
	sieve_ast_unref(&ast);

	if ( error_r != NULL )
		error_r = SIEVE_ERROR_NONE;

	return sbin;
}

struct sieve_binary *sieve_compile
(struct sieve_instance *svinst, const char *script_path,
	const char *script_name, struct sieve_error_handler *ehandler,
	enum sieve_error *error_r)
{
	struct sieve_script *script;
	struct sieve_binary *sbin;

	if ( (script = sieve_script_create
		(svinst, script_path, script_name, ehandler, error_r)) == NULL )
		return NULL;
	
	sbin = sieve_compile_script(script, ehandler, error_r);
	
	sieve_script_unref(&script);

	if ( svinst->debug && sbin != NULL ) {
		sieve_sys_debug(svinst, "script file %s successfully compiled",
			script_path);
	}
	
	return sbin;
}

/*
 * Sieve runtime
 */

static int sieve_run
(struct sieve_binary *sbin, struct sieve_result **result, 
	const struct sieve_message_data *msgdata, const struct sieve_script_env *senv, 
	struct sieve_error_handler *ehandler)
{
	struct sieve_interpreter *interp;
	int ret = 0;

	/* Create the interpreter */
	if ( (interp=sieve_interpreter_create(sbin, msgdata, senv, ehandler)) 
		== NULL )
		return SIEVE_EXEC_BIN_CORRUPT;

	/* Reset execution status */
	if ( senv->exec_status != NULL )
		memset(senv->exec_status, 0, sizeof(*senv->exec_status));
	
	/* Create result object */
	if ( *result == NULL )
		*result = sieve_result_create
			(sieve_binary_svinst(sbin), msgdata, senv, ehandler);
	else {
		sieve_result_set_error_handler(*result, ehandler);
	}
							
	/* Run the interpreter */
	ret = sieve_interpreter_run(interp, *result);
	
	/* Free the interpreter */
	sieve_interpreter_free(&interp);

	return ret;
}

/*
 * Reading/writing sieve binaries
 */

struct sieve_binary *sieve_load
(struct sieve_instance *svinst, const char *bin_path, enum sieve_error *error_r)
{
	return sieve_binary_open(svinst, bin_path, NULL, error_r);
}

struct sieve_binary *sieve_open
(struct sieve_instance *svinst, const char *script_path, 
	const char *script_name, struct sieve_error_handler *ehandler,
	enum sieve_error *error_r)
{
	struct sieve_script *script;
	struct sieve_binary *sbin;
	const char *bin_path;
	
	/* First open the scriptfile itself */
	script = sieve_script_create
		(svinst, script_path, script_name, ehandler, error_r);

	if ( script == NULL ) {
		/* Failed */
		return NULL;
	}

	T_BEGIN {
		/* Then try to open the matching binary */
		bin_path = sieve_script_binpath(script);	
		sbin = sieve_binary_open(svinst, bin_path, script, error_r);
	
		if (sbin != NULL) {
			/* Ok, it exists; now let's see if it is up to date */
			if ( !sieve_binary_up_to_date(sbin) ) {
				/* Not up to date */
				if ( svinst->debug )
					sieve_sys_debug(svinst, "script binary %s is not up-to-date",
						bin_path);

				sieve_binary_unref(&sbin);
				sbin = NULL;
			} 
		}
		
		/* If the binary does not exist or is not up-to-date, we need
		 * to (re-)compile.
		 */
		if ( sbin != NULL ) {
			if ( svinst->debug )
				sieve_sys_debug(svinst, "script binary %s successfully loaded",
					bin_path);
			
		} else {	
			sbin = sieve_compile_script(script, ehandler, error_r);

			/* Save the binary if compile was successful */
			if ( sbin != NULL ) {
				if ( svinst->debug )
					sieve_sys_debug(svinst, "script %s successfully compiled", 
						script_path);
			}
		}
	} T_END;
	
	/* Drop script reference, if sbin != NULL it holds a reference of its own. 
	 * Otherwise the script object is freed here.
	 */
	sieve_script_unref(&script);

	return sbin;
}

const char *sieve_get_source(struct sieve_binary *sbin)
{
	return sieve_binary_source(sbin);
}

bool sieve_is_loaded(struct sieve_binary *sbin)
{
	return sieve_binary_loaded(sbin);
}

int sieve_save
(struct sieve_binary *sbin, const char *bin_path, bool update,
	enum sieve_error *error_r)
{
	return sieve_binary_save(sbin, bin_path, update, error_r);
}

void sieve_close(struct sieve_binary **sbin)
{
	sieve_binary_unref(sbin);
}

/*
 * Debugging
 */

void sieve_dump
(struct sieve_binary *sbin, struct ostream *stream, bool verbose) 
{
	struct sieve_binary_dumper *dumpr = sieve_binary_dumper_create(sbin);			

	sieve_binary_dumper_run(dumpr, stream, verbose);	
	
	sieve_binary_dumper_free(&dumpr);
}

void sieve_hexdump
(struct sieve_binary *sbin, struct ostream *stream) 
{	
	struct sieve_binary_dumper *dumpr = sieve_binary_dumper_create(sbin);			

	sieve_binary_dumper_hexdump(dumpr, stream);	
	
	sieve_binary_dumper_free(&dumpr);
}

int sieve_test
(struct sieve_binary *sbin, const struct sieve_message_data *msgdata,
	const struct sieve_script_env *senv, struct sieve_error_handler *ehandler,
	struct ostream *stream, bool *keep) 	
{
	struct sieve_result *result = NULL;
	int ret;

	if ( keep != NULL ) *keep = FALSE;
	
	/* Run the script */
	ret = sieve_run(sbin, &result, msgdata, senv, ehandler);
				
	/* Print result if successful */
	if ( ret > 0 ) {
		ret = sieve_result_print(result, senv, stream, keep);
	} else if ( ret == 0 ) {
		if ( keep != NULL ) *keep = TRUE;
	}
	
	/* Cleanup */
	sieve_result_unref(&result);
	
	return ret;
}

/*
 * Script execution
 */

int sieve_execute
(struct sieve_binary *sbin, const struct sieve_message_data *msgdata,
	const struct sieve_script_env *senv, struct sieve_error_handler *ehandler,
	bool *keep)
{
	struct sieve_result *result = NULL;
	int ret;

	if ( keep != NULL ) *keep = FALSE;
	
	/* Run the script */
	ret = sieve_run(sbin, &result, msgdata, senv, ehandler);
		
	/* Evaluate status and execute the result:
	 *   Strange situations, e.g. currupt binaries, must be handled by the caller. 
	 *   In that case no implicit keep is attempted, because the situation may be 
	 *   resolved.
	 */
	if ( ret > 0 ) {
		/* Execute result */
		ret = sieve_result_execute(result, keep);
	} else if ( ret == 0 ) {
		/* Perform implicit keep if script failed with a normal runtime error */
		if ( !sieve_result_implicit_keep(result) ) {
			ret = SIEVE_EXEC_KEEP_FAILED;
		} else {
			if ( keep != NULL ) *keep = TRUE;
		}
	}
	
	/* Cleanup */
	sieve_result_unref(&result);

	return ret;
}

/*
 * Multiscript support
 */
 
struct sieve_multiscript {
	struct sieve_instance *svinst;
	struct sieve_result *result;
	const struct sieve_message_data *msgdata;
	const struct sieve_script_env *scriptenv;

	int status;
	bool active;
	bool keep;

	struct ostream *teststream;
};
 
struct sieve_multiscript *sieve_multiscript_start_execute
(struct sieve_instance *svinst,	const struct sieve_message_data *msgdata, 
	const struct sieve_script_env *senv)
{
	pool_t pool;
	struct sieve_result *result;
	struct sieve_multiscript *mscript;
	
	result = sieve_result_create(svinst, msgdata, senv, NULL);
	pool = sieve_result_pool(result);
	
	sieve_result_set_keep_action(result, NULL, NULL);
	
	mscript = p_new(pool, struct sieve_multiscript, 1);
	mscript->svinst = svinst;
	mscript->result = result;
	mscript->msgdata = msgdata;
	mscript->scriptenv = senv;
	mscript->status = SIEVE_EXEC_OK;
	mscript->active = TRUE;
	mscript->keep = TRUE;
	
	return mscript;
}

struct sieve_multiscript *sieve_multiscript_start_test
(struct sieve_instance *svinst, const struct sieve_message_data *msgdata,
	const struct sieve_script_env *senv, struct ostream *stream)
{
	struct sieve_multiscript *mscript = 
		sieve_multiscript_start_execute(svinst, msgdata, senv);
	
	mscript->teststream = stream;

	return mscript;
}

static void sieve_multiscript_test
(struct sieve_multiscript *mscript, struct sieve_error_handler *ehandler,
	bool *keep)
{						
	sieve_result_set_error_handler(mscript->result, ehandler);

	if ( mscript->status > 0 ) {
		mscript->status = sieve_result_print
			(mscript->result, mscript->scriptenv, mscript->teststream, keep);
	} else {
		if ( keep != NULL ) *keep = TRUE;
	}

	sieve_result_mark_executed(mscript->result);
}

static void sieve_multiscript_execute
(struct sieve_multiscript *mscript, struct sieve_error_handler *ehandler,
	bool *keep)
{
	sieve_result_set_error_handler(mscript->result, ehandler);

	if ( mscript->status > 0 ) {
		mscript->status = sieve_result_execute(mscript->result, keep);
	} else {
		if ( !sieve_result_implicit_keep(mscript->result) )
			mscript->status = SIEVE_EXEC_KEEP_FAILED;
		else
			if ( keep != NULL ) *keep = TRUE;			
	}
}

bool sieve_multiscript_run
(struct sieve_multiscript *mscript, struct sieve_binary *sbin,
	struct sieve_error_handler *ehandler, bool final)
{
	if ( !mscript->active ) return FALSE;
	
	if ( final )
		sieve_result_set_keep_action(mscript->result, NULL, &act_store);
	
	/* Run the script */
	mscript->status = sieve_run(sbin, &mscript->result, mscript->msgdata, 
		mscript->scriptenv, ehandler);

	if ( mscript->status >= 0 ) {
		mscript->keep = FALSE;

		if ( mscript->teststream != NULL ) 
			sieve_multiscript_test(mscript, ehandler, &mscript->keep);
		else
			sieve_multiscript_execute(mscript, ehandler, &mscript->keep);

		mscript->active =
			( mscript->active && mscript->keep && !final && mscript->status > 0 );
	}	

	if ( mscript->status <= 0 )
		return FALSE;

	return mscript->active;
}

int sieve_multiscript_status(struct sieve_multiscript *mscript)
{
	return mscript->status;
}

int sieve_multiscript_finish(struct sieve_multiscript **mscript, 
	struct sieve_error_handler *ehandler, bool *keep)
{
	struct sieve_result *result = (*mscript)->result;
	int ret = (*mscript)->status;

	if ( ehandler != NULL )
		sieve_result_set_error_handler((*mscript)->result, ehandler);	

	if ( (*mscript)->active ) {
		ret = SIEVE_EXEC_FAILURE;

		if ( (*mscript)->teststream ) {
			(*mscript)->keep = TRUE;
		} else {
			if ( !sieve_result_implicit_keep((*mscript)->result) )
				ret = SIEVE_EXEC_KEEP_FAILED;
			else
				(*mscript)->keep = TRUE;
		}
	}

	if ( keep != NULL ) *keep = (*mscript)->keep;
	
	/* Cleanup */
	sieve_result_unref(&result);
	*mscript = NULL;
	
	return ret;
}

/*
 * Configured Limits
 */

unsigned int sieve_max_redirects(struct sieve_instance *svinst)
{
	return svinst->max_redirects;
}

unsigned int sieve_max_actions(struct sieve_instance *svinst)
{
	return svinst->max_actions;
}

size_t sieve_max_script_size(struct sieve_instance *svinst)
{
	return svinst->max_script_size;
}

/*
 * Script directory
 */

struct sieve_directory {
		struct sieve_instance *svinst;
		DIR *dirp;

		const char *path;
};

struct sieve_directory *sieve_directory_open
(struct sieve_instance *svinst, const char *path, enum sieve_error *error_r)
{ 
	struct sieve_directory *sdir = NULL;
	DIR *dirp;
	struct stat st;

	if ( error_r != NULL )
		*error_r = SIEVE_ERROR_NONE;

	/* Specified path can either be a regular file or a directory */
	if ( stat(path, &st) != 0 ) {
		switch ( errno ) {
		case ENOENT:
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NOT_FOUND;
			break;
		case EACCES:
			sieve_sys_error(svinst, "failed to open sieve dir: %s",
				eacces_error_get("stat", path));
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_NO_PERM;
			break;
		default:
			sieve_sys_error(svinst, "failed to open sieve dir: stat(%s) failed: %m",
				path);
			if ( error_r != NULL )
				*error_r = SIEVE_ERROR_TEMP_FAIL;
			break;
		}
		return NULL;
	}

	if ( S_ISDIR(st.st_mode) ) {
	 	
		/* Open the directory */
		if ( (dirp = opendir(path)) == NULL ) {
			switch ( errno ) {
			case ENOENT:
				if ( error_r != NULL )
					*error_r = SIEVE_ERROR_NOT_FOUND;
				break;
			case EACCES:
				sieve_sys_error(svinst, "failed to open sieve dir: %s",
					eacces_error_get("opendir", path));
				if ( error_r != NULL )
					*error_r = SIEVE_ERROR_NO_PERM;
				break;
			default:
				sieve_sys_error(svinst, "failed to open sieve dir: opendir(%s) failed: "
					"%m", path);
				if ( error_r != NULL )
					*error_r = SIEVE_ERROR_TEMP_FAIL;
				break;
			}
			return NULL;		
		}
	
		/* Create object */
		sdir = t_new(struct sieve_directory, 1);
		sdir->path = path;
		sdir->dirp = dirp;
	} else {
		sdir = t_new(struct sieve_directory, 1);
		sdir->path = path;
		sdir->dirp = NULL;
	}

	sdir->svinst = svinst;

	return sdir;
}

const char *sieve_directory_get_scriptfile(struct sieve_directory *sdir)
{
	const char *script = NULL;
	struct dirent *dp;
	
	if ( sdir->dirp != NULL ) {
		while ( script == NULL ) {
			const char *file;
			struct stat st;

			errno = 0;
			if ( (dp = readdir(sdir->dirp)) == NULL ) {
				if ( errno != 0 ) {
					sieve_sys_error(sdir->svinst, "failed to read sieve dir: "
						"readdir(%s) failed: %m", sdir->path);
				}

				return NULL;
			}

			if ( !sieve_script_file_has_extension(dp->d_name) )
				continue;

			if ( sdir->path[strlen(sdir->path)-1] == '/' )
				file = t_strconcat(sdir->path, dp->d_name, NULL);
			else
				file = t_strconcat(sdir->path, "/", dp->d_name, NULL);

			if ( stat(file, &st) != 0 || !S_ISREG(st.st_mode) )
				continue;

			script = file;
		}
	} else {
		script = sdir->path;
		sdir->path = NULL;		
	}
							
	return script;
}

void sieve_directory_close(struct sieve_directory **sdir)
{
	/* Close the directory */
	if ( (*sdir)->dirp != NULL && closedir((*sdir)->dirp) < 0 ) 
		sieve_sys_error((*sdir)->svinst, "failed to close sieve dir: "
			"closedir(%s) failed: %m", (*sdir)->path);
		
	*sdir = NULL;
}



