/*
 * com_err_threads.c
 *
 * $Header$
 *
 * Copyright 2003-2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <Kerberos/KerberosDebug.h>

#include "com_err_threads.h"
#include "k5-platform.h"
#include "k5-thread.h"

static k5_mutex_t g_bundle_lookup_mutex = K5_MUTEX_PARTIAL_INITIALIZER;
static k5_mutex_t g_error_tables_mutex = K5_MUTEX_PARTIAL_INITIALIZER;
static k5_mutex_t g_error_hook_mutex = K5_MUTEX_PARTIAL_INITIALIZER;

static struct error_tables g_error_tables = {0, 0, NULL}; 
com_err_handler_t g_com_err_hook = NULL;

MAKE_INIT_FUNCTION(com_err_thread_init);
MAKE_FINI_FUNCTION(com_err_thread_fini);

/* ------------------------------------------------------------------------ */

static int com_err_thread_init (void)
{
    errcode_t err = 0;
    
    if (!err) {
        err = k5_key_register (K5_KEY_COM_ERR, free);
    }
    
    if (!err) {
        err = k5_key_register (K5_KEY_COM_ERR_REENTER, NULL);
    }
    
    if (!err) {
        err = k5_mutex_finish_init(&g_bundle_lookup_mutex);
    }
    
    if (!err) {
        err = k5_mutex_finish_init(&g_error_tables_mutex);
    }
    
    if (!err) {
        err = k5_mutex_finish_init(&g_error_hook_mutex);
    }
    
    return err;
}

/* ------------------------------------------------------------------------ */

static void com_err_thread_fini (void)
{
    if (!INITIALIZER_RAN (com_err_thread_init) || PROGRAM_EXITING ()) {
	return;
    }
    
    k5_key_delete (K5_KEY_COM_ERR);
    k5_key_delete (K5_KEY_COM_ERR_REENTER);
    k5_mutex_destroy(&g_bundle_lookup_mutex);
    k5_mutex_destroy(&g_error_tables_mutex);
    k5_mutex_destroy(&g_error_hook_mutex);
}

#pragma mark -

/* ------------------------------------------------------------------------ */

errcode_t com_err_thread_lock_error_tables (error_table_array_t *error_tables)
{
    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    
    if (!err) {
        err = k5_mutex_lock (&g_error_tables_mutex);
    }
    
    if (!err) {
        *error_tables = &g_error_tables;
    } 
    
    return err;
}

/* ------------------------------------------------------------------------ */

errcode_t com_err_thread_unlock_error_tables (error_table_array_t *error_tables)
{
    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    
    if (!err) {
        err = k5_mutex_unlock (&g_error_tables_mutex);
    }
    
    if (!err) {
        *error_tables = NULL;
    } 
    
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */
errcode_t com_err_thread_get_com_err_hook (com_err_handler_t *handler)
{
    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    
    if (!err) {
        err = k5_mutex_lock (&g_error_hook_mutex);
    }
    
    if (!err) {
        *handler = g_com_err_hook;
        err = k5_mutex_unlock (&g_error_hook_mutex);
    }
    
    return err;
}

/* ------------------------------------------------------------------------ */

errcode_t com_err_thread_set_com_err_hook (com_err_handler_t handler)
{
    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    
    if (!err) {
        err = k5_mutex_lock (&g_error_hook_mutex);
    }
    
    if (!err) {
        g_com_err_hook = handler;
        err = k5_mutex_unlock (&g_error_hook_mutex);
    }
    
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

errcode_t com_err_thread_get_error_message (char **out_string)
{
    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    
    if (!err) {
        *out_string = k5_getspecific (K5_KEY_COM_ERR);
    }
    
    return err;
}

/* ------------------------------------------------------------------------ */

errcode_t com_err_thread_set_error_message (const char *in_string)
{
    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    char *old_string = NULL;
    
    if (!err) {
        err = com_err_thread_get_error_message (&old_string);
    }
    
    if (!err) {
        if (old_string) { free (old_string); }
        err = k5_setspecific (K5_KEY_COM_ERR, (char *) in_string);
    }
    
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

errcode_t com_err_thread_entering_error_message (int *out_reentered)
{
    static int s_reentered = 1;

    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    int *reentered = NULL;
    
    if (!err) {
        reentered = k5_getspecific (K5_KEY_COM_ERR_REENTER);
        
        if (!reentered) {
            err = k5_setspecific (K5_KEY_COM_ERR_REENTER, &s_reentered);
        }
    }
    
    if (!err) {
        /* If the pointer is non-NULL we are reentering error_message */
        *out_reentered = reentered ? 1 : 0;
    }
    
    return err;
}

/* ------------------------------------------------------------------------ */

errcode_t com_err_thread_leaving_error_message (void)
{
    errcode_t err = CALL_INIT_FUNCTION (com_err_thread_init);
    
    if (!err) {
        /* Don't need to free because it is static */
        err = k5_setspecific (K5_KEY_COM_ERR_REENTER, NULL);
    }
    
    return err;
}
