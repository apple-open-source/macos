/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve.h"
#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-binary.h"
#include "sieve-interpreter.h"
#include "sieve-runtime-trace.h"
#include "sieve-result.h"

#include "testsuite-common.h"
#include "testsuite-settings.h"
#include "testsuite-log.h"
#include "testsuite-result.h"

#include "testsuite-script.h"

/*
 * Tested script environment
 */ 

struct sieve_binary *_testsuite_compiled_script;

void testsuite_script_init(void)
{
	_testsuite_compiled_script = NULL;
}

void testsuite_script_deinit(void)
{
	if ( _testsuite_compiled_script != NULL ) {
		sieve_binary_unref(&_testsuite_compiled_script);
	}
}

static struct sieve_binary *_testsuite_script_compile
(const struct sieve_runtime_env *renv, const char *script)
{
	struct sieve_instance *svinst = testsuite_sieve_instance;
	struct sieve_binary *sbin;
	const char *script_path;

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "compile script `%s'", script);

	script_path = sieve_script_dirpath(renv->script);
	if ( script_path == NULL ) 
		return SIEVE_EXEC_FAILURE;

	script_path = t_strconcat(script_path, "/", script, NULL);

	if ( (sbin = sieve_compile(svinst, script_path, NULL, testsuite_log_ehandler,
		NULL)) == NULL )
		return NULL;

	return sbin;
}

bool testsuite_script_compile
(const struct sieve_runtime_env *renv, const char *script)
{
	struct sieve_binary *sbin;

	testsuite_log_clear_messages();

	if ( (sbin=_testsuite_script_compile(renv, script)) == NULL )
		return FALSE;

	if ( _testsuite_compiled_script != NULL ) {
		sieve_binary_unref(&_testsuite_compiled_script);
	}

	_testsuite_compiled_script = sbin;

	return TRUE;
}

bool testsuite_script_run(const struct sieve_runtime_env *renv)
{
	struct sieve_script_env scriptenv;
	struct sieve_result *result;
	struct sieve_interpreter *interp;
	int ret;

	if ( _testsuite_compiled_script == NULL ) {
		sieve_runtime_error(renv, NULL,
			"testsuite: trying to run script, but no script compiled yet");
		return FALSE;
	}

	testsuite_log_clear_messages();

	/* Compose script execution environment */
	memset(&scriptenv, 0, sizeof(scriptenv));
	scriptenv.default_mailbox = "INBOX";
	scriptenv.username = "user";
	scriptenv.hostname = "host.example.com";
	scriptenv.postmaster_address = "postmaster@example.com";
	scriptenv.smtp_open = NULL;
	scriptenv.smtp_close = NULL;
	scriptenv.duplicate_mark = NULL;
	scriptenv.duplicate_check = NULL;
	scriptenv.user = renv->scriptenv->user;
	scriptenv.trace_stream = renv->scriptenv->trace_stream;
	scriptenv.trace_config = renv->scriptenv->trace_config;
	
	result = testsuite_result_get();

	/* Execute the script */
	interp=sieve_interpreter_create(_testsuite_compiled_script, renv->msgdata, 
		&scriptenv, testsuite_log_ehandler);
	
	if ( interp == NULL )
		return SIEVE_EXEC_BIN_CORRUPT;
		
	ret = sieve_interpreter_run(interp, result);

	sieve_interpreter_free(&interp);

	return ( ret > 0 );
}

struct sieve_binary *testsuite_script_get_binary(void)
{
	return _testsuite_compiled_script;
}

void testsuite_script_set_binary(struct sieve_binary *sbin)
{
	if ( _testsuite_compiled_script != NULL ) {
		sieve_binary_unref(&_testsuite_compiled_script);
	}

	_testsuite_compiled_script = sbin;
	sieve_binary_ref(sbin);
}

/*
 * Multiscript
 */

bool testsuite_script_multiscript
(const struct sieve_runtime_env *renv, ARRAY_TYPE (const_string) *scriptfiles)
{
	struct sieve_instance *svinst = testsuite_sieve_instance;
	struct sieve_script_env scriptenv;
	struct sieve_multiscript *mscript;
	const char *const *scripts;
	unsigned int count, i;
	bool more = TRUE;
	bool result = TRUE;

	testsuite_log_clear_messages();

	/* Compose script execution environment */
	memset(&scriptenv, 0, sizeof(scriptenv));
	scriptenv.default_mailbox = "INBOX";
	scriptenv.username = "user";
	scriptenv.hostname = "host.example.com";
	scriptenv.postmaster_address = "postmaster@example.com";
	scriptenv.smtp_open = NULL;
	scriptenv.smtp_close = NULL;
	scriptenv.duplicate_mark = NULL;
	scriptenv.duplicate_check = NULL;
	scriptenv.user = renv->scriptenv->user;
	scriptenv.trace_stream = renv->scriptenv->trace_stream;	
	scriptenv.trace_config = renv->scriptenv->trace_config;

	/* Start execution */

	mscript = sieve_multiscript_start_execute(svinst, renv->msgdata, &scriptenv);

	/* Execute scripts before main script */

	scripts = array_get(scriptfiles, &count);

	for ( i = 0; i < count && more; i++ ) {
		struct sieve_binary *sbin = NULL;
		const char *script = scripts[i];
		bool final = ( i == count - 1 );

		/* Open */
	
		if ( (sbin=_testsuite_script_compile(renv, script)) == NULL ) {
			result = FALSE;
			break;
		}

		/* Execute */

		sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "run script `%s'", script);

		more = sieve_multiscript_run(mscript, sbin, testsuite_log_ehandler, final);

		sieve_close(&sbin);
	}

	return ( sieve_multiscript_finish(&mscript, testsuite_log_ehandler, NULL) > 0
		&& result );
}
