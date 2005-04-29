/*
 * com_err_threads.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosErrors/Sources/com_err_threads.c,v 1.1 2004/10/04 17:48:25 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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

// If only pthread_once propagated an error from the initializer
static errcode_t g_com_err_thread_init_error = 0;
static pthread_once_t g_com_err_thread_init_once = PTHREAD_ONCE_INIT;

// Statically initialized mutexes
static pthread_mutex_t g_bundle_lookup_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t g_error_tables_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct error_tables g_error_tables = {0, 0, NULL}; 


static pthread_key_t g_com_err_hook_key;
static pthread_key_t g_error_message_key;
static pthread_key_t g_error_manager_key;


// ---------------------------------------------------------------------------

static void com_err_thread_init_hook ()
{
    errcode_t err = 0;
    
    if (!err) {
        err = pthread_key_create (&g_com_err_hook_key, NULL);
    }
    
    if (!err) {
        err = pthread_key_create (&g_error_message_key, free);
    }
    
    if (!err) {
        err = pthread_key_create (&g_error_manager_key, free);
    }
    
    if (err) {
        g_com_err_thread_init_error = err;  // Never write to this variable from anywhere else!
        dprintf ("%s: Warning!  Pthread initialization failed with error %d (%s)", 
                 __FUNCTION__, err, strerror (err)); 
    } else {
        dprintf ("%s: successfully initialized pthread keys", __FUNCTION__);
    }
}

// ---------------------------------------------------------------------------

static errcode_t com_err_thread_init ()
{
    errcode_t err = 0;
    
    if (!err) {
        err = pthread_once (&g_com_err_thread_init_once, com_err_thread_init_hook);
    }
    
    // If only pthread_once propagated an error from the initializer
    return err ? err : g_com_err_thread_init_error;
}

// ---------------------------------------------------------------------------

#warning com_err pthread destructor not called
static void com_err_thread_destroy ()
{
    errcode_t err = 0;
    
    if (!err) {
        err = pthread_key_delete (g_com_err_hook_key);
    }
    
    if (!err) {
        err = pthread_key_delete (g_error_message_key);
    }
    
    if (!err) {
        err = pthread_key_delete (g_error_manager_key);
    }
    
    if (err) {
        dprintf ("%s: Warning!  Pthread termination failed with error %d (%s)", 
                 __FUNCTION__, err, strerror (err)); 
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

errcode_t com_err_thread_lock_error_tables (error_table_array_t *error_tables)
{
    errcode_t err = 0;
    
    if (!err) {
        err = pthread_mutex_lock (&g_error_tables_mutex);
    }
    
    if (!err) {
        *error_tables = &g_error_tables;
    } 
    
    return err;
}

// ---------------------------------------------------------------------------

errcode_t com_err_thread_unlock_error_tables (error_table_array_t *error_tables)
{
    errcode_t err = 0;
    
    if (!err) {
        err = pthread_mutex_unlock (&g_error_tables_mutex);
    }
    
    if (!err) {
        *error_tables = NULL;
    } 
    
    return err;
}

#pragma mark -

// ---------------------------------------------------------------------------

errcode_t com_err_thread_lock_for_bundle_lookup ()
{
    return pthread_mutex_lock (&g_bundle_lookup_mutex);
}

// ---------------------------------------------------------------------------

errcode_t com_err_thread_unlock_for_bundle_lookup ()
{
    return pthread_mutex_unlock (&g_bundle_lookup_mutex);;
}

#pragma mark -

// ---------------------------------------------------------------------------

errcode_t com_err_thread_get_com_err_hook (com_err_handler_t *handler)
{
    errcode_t err = com_err_thread_init ();
    
    if (!err) {
        *handler = pthread_getspecific (g_com_err_hook_key);
    }
    
    return err;
}

// ---------------------------------------------------------------------------

errcode_t com_err_thread_set_com_err_hook (com_err_handler_t handler)
{
    errcode_t err = com_err_thread_init ();
    
    if (!err) {
        err = pthread_setspecific (g_com_err_hook_key, handler);
    }
    
    return err;
}

#pragma mark -

// ---------------------------------------------------------------------------

errcode_t com_err_thread_get_error_message (char **out_string)
{
    errcode_t err = com_err_thread_init ();
    
    if (!err) {
        *out_string = pthread_getspecific (g_error_message_key);
    }
    
    return err;
}

// ---------------------------------------------------------------------------

errcode_t com_err_thread_set_error_message (const char *in_string)
{
    errcode_t err = com_err_thread_init ();
    char *old_string = NULL;
    
    if (!err) {
        err = com_err_thread_get_error_message (&old_string);
    }
    
    if (!err) {
        if (old_string != NULL) { free (old_string); }
        err = pthread_setspecific (g_error_message_key, in_string);
    }
    
    return err;
}

#pragma mark -

// ---------------------------------------------------------------------------

errcode_t com_err_thread_get_error_manager (char **out_string)
{
    errcode_t err = com_err_thread_init ();
    
    if (!err) {
        *out_string = pthread_getspecific (g_error_manager_key);
    }
    
    return err;
}

// ---------------------------------------------------------------------------

errcode_t com_err_thread_set_error_manager (const char *in_string)
{
    errcode_t err = com_err_thread_init ();
    char *old_string = NULL;
    
    if (!err) {
        err = com_err_thread_get_error_manager (&old_string);
    }
    
    if (!err) {
        if (old_string != NULL) { free (old_string); }
        err = pthread_setspecific (g_error_manager_key, in_string);
    }
    
    return err;
}

