/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ostream.h"
#include "str.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-stringlist.h"
#include "sieve-actions.h"
#include "sieve-interpreter.h"
#include "sieve-result.h"

#include "testsuite-common.h"
#include "testsuite-log.h"
#include "testsuite-message.h"

#include "testsuite-result.h"

static struct sieve_result *_testsuite_result;

void testsuite_result_init(void)
{
	struct sieve_instance *svinst = testsuite_sieve_instance;

	_testsuite_result = sieve_result_create
		(svinst, &testsuite_msgdata, testsuite_scriptenv, testsuite_log_ehandler);
}

void testsuite_result_deinit(void)
{
	if ( _testsuite_result != NULL ) {
		sieve_result_unref(&_testsuite_result);
	}
}

void testsuite_result_reset
(const struct sieve_runtime_env *renv)
{
	struct sieve_instance *svinst = testsuite_sieve_instance;

	if ( _testsuite_result != NULL ) {
		sieve_result_unref(&_testsuite_result);
	}

	_testsuite_result = sieve_result_create
		(svinst, &testsuite_msgdata, testsuite_scriptenv, testsuite_log_ehandler);
	sieve_interpreter_set_result(renv->interp, _testsuite_result);
}

struct sieve_result *testsuite_result_get(void)
{
	return _testsuite_result;
}

struct sieve_result_iterate_context *testsuite_result_iterate_init(void)
{
	if ( _testsuite_result == NULL )
		return NULL;

	return sieve_result_iterate_init(_testsuite_result);
}

bool testsuite_result_execute(const struct sieve_runtime_env *renv)
{
	int ret;

	if ( _testsuite_result == NULL ) {
		sieve_runtime_error(renv, NULL,
			"testsuite: trying to execute result, but no result evaluated yet");
		return FALSE;
	}

	testsuite_log_clear_messages();

	/* Execute the result */	
	ret=sieve_result_execute(_testsuite_result, NULL);
	
	return ( ret > 0 );
}

void testsuite_result_print
(const struct sieve_runtime_env *renv)
{
	struct ostream *out;
	
	out = o_stream_create_fd(1, 0, FALSE);	

	o_stream_send_str(out, "\n--");
	sieve_result_print(_testsuite_result, renv->scriptenv, out, NULL);
	o_stream_send_str(out, "--\n\n");

	o_stream_destroy(&out);	
}

/*
 * Result stringlist
 */

/* Forward declarations */

static int testsuite_result_stringlist_next_item
	(struct sieve_stringlist *_strlist, string_t **str_r);
static void testsuite_result_stringlist_reset
	(struct sieve_stringlist *_strlist);

/* Stringlist object */

struct testsuite_result_stringlist {
	struct sieve_stringlist strlist;

	struct sieve_result_iterate_context *result_iter;
	int pos, index;
};

struct sieve_stringlist *testsuite_result_stringlist_create
(const struct sieve_runtime_env *renv, int index)
{
	struct testsuite_result_stringlist *strlist;
	    
	strlist = t_new(struct testsuite_result_stringlist, 1);
	strlist->strlist.runenv = renv;
	strlist->strlist.exec_status = SIEVE_EXEC_OK;
	strlist->strlist.next_item = testsuite_result_stringlist_next_item;
	strlist->strlist.reset = testsuite_result_stringlist_reset;

	strlist->result_iter = testsuite_result_iterate_init();
 	strlist->index = index;
	strlist->pos = 0;
 
	return &strlist->strlist;
}

static int testsuite_result_stringlist_next_item
(struct sieve_stringlist *_strlist, string_t **str_r)
{
	struct testsuite_result_stringlist *strlist = 
		(struct testsuite_result_stringlist *) _strlist;
	const struct sieve_action *action;
	const char *act_name;
	bool keep;

	*str_r = NULL;

	if ( strlist->index > 0 && strlist->pos > 0 )
		return 0;

	do { 
		if ( (action=sieve_result_iterate_next(strlist->result_iter, &keep))
			== NULL )
			return 0;
		
		strlist->pos++;
	} while ( strlist->pos < strlist->index );
	
	if ( keep ) 
		act_name = "keep";
	else
		act_name = ( action == NULL || action->def == NULL ||
			action->def->name == NULL ) ? "" : action->def->name;

	*str_r = t_str_new_const(act_name, strlen(act_name));
	return 1;
}

static void testsuite_result_stringlist_reset
(struct sieve_stringlist *_strlist)
{
	struct testsuite_result_stringlist *strlist = 
		(struct testsuite_result_stringlist *) _strlist;

	strlist->result_iter = testsuite_result_iterate_init();
	strlist->pos = 0;
}

