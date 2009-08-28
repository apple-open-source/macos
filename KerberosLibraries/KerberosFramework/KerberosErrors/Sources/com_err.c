/*
 * com_err.c
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

#include <Kerberos/com_err.h>
#include <Kerberos/KerberosDebug.h>
#include "kim_string_private.h"

#include <CoreFoundation/CoreFoundation.h>

#include "com_err_threads.h"

/*
 * Defines:
 */

#define UNKNOWN_ERROR_FORMAT       "Unknown error code: %ld"
#define UNKNOWN_ERROR_MAX_SIZE     256

#define UNKNOWN_ERROR_MESSAGE      "Unknown error"
#define REENTER_ERROR_MESSAGE      "Cannot generate error string.  Re-entered error_message."

#define MESSAGE_KEY_FORMAT         "KEMessage %ld"
#define MAX_KEY_LENGTH             256
#define MAX_ERROR_MESSAGE_SIZE     BUFSIZ

#define ERROR_TABLE_SIZE_INCREMENT 10

#define ERRCODE_RANGE              8
#define BITS_PER_CHAR              6



/* ***************** */
/* *** FUNCTIONS *** */
/* ***************** */


#pragma mark -

/* ------------------------------------------------------------------------ */

errcode_t add_error_table (const struct error_table *et)
{
    error_table_array_t error_tables = NULL;
    errcode_t lock_err = com_err_thread_lock_error_tables (&error_tables);
    errcode_t err = lock_err;
    
    if (!err) {
        if (et == NULL) { err = EINVAL; }
    }
    
    if (!err) {
        if ((error_tables->count + 1) > error_tables->size) {
            const struct error_table **new_ets = NULL;
            size_t new_size = error_tables->size + ERROR_TABLE_SIZE_INCREMENT;
            
            new_ets = realloc (error_tables->ets, (sizeof (char *)) * new_size);
            if (new_ets == NULL) {
                err = ENOMEM;
            } else {
                error_tables->ets = new_ets;
                error_tables->size = new_size;
            }
        }
    }
    
    if (!err) {
        error_tables->ets[error_tables->count] = et; /* add to the end */
        error_tables->count++;
    }
    
    if (!lock_err) { com_err_thread_unlock_error_tables (&error_tables); }
    
    return err;
}

/* ------------------------------------------------------------------------ */

errcode_t remove_error_table (const struct error_table *et)
{
    error_table_array_t error_tables = NULL;
    errcode_t lock_err = com_err_thread_lock_error_tables (&error_tables);
    errcode_t err = lock_err;
    size_t etindex = 0;

    if (!err) {
        if (et == NULL) { err = EINVAL; }
    }
    
    if (!err) {
        int found = false;
        size_t i;
        
        /* find the error table */
        for (i = 0; i < error_tables->count; i++) {
            if (error_tables->ets[i] == et) {
                etindex = i;
                found = true;
            }
        }
        
        if (!found) { err = EINVAL; }
    }
    
    if (!err) {
        /* shift the remaining elements over */
        memmove (&error_tables->ets[etindex], &error_tables->ets[etindex + 1],
                 ((error_tables->count - 1) - etindex) * sizeof (struct error_table **));
        error_tables->count--;
    }
    
    if (!lock_err) { com_err_thread_unlock_error_tables (&error_tables); }
    
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

static errcode_t create_cstring (const char *in_string, char **out_string)
{
    errcode_t err = 0;
    
    if (in_string  == NULL) { err = EINVAL; }
    if (out_string == NULL) { err = EINVAL; }
    
    if (!err) {
        *out_string = malloc (strlen (in_string) + 1);
        if (*out_string == NULL) { err = ENOMEM; }
    }
    
    if (!err) {
        strcpy (*out_string, in_string);
    }
    
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning string '%s' for string '%s' (err = %d)", 
             __FUNCTION__, err ? "(null)" : *out_string, in_string, err);
#endif
    
    return err;
}

/* ------------------------------------------------------------------------ */

static errcode_t create_unknown_error_message (errcode_t   in_code, 
                                               char      **out_string)
{
    errcode_t err = 0;
    
    if (!err) {
        char unknownError[UNKNOWN_ERROR_MAX_SIZE];
        
        snprintf (unknownError, UNKNOWN_ERROR_MAX_SIZE, 
                  UNKNOWN_ERROR_FORMAT, in_code);
        unknownError[UNKNOWN_ERROR_MAX_SIZE - 1] = '\0';
        
        err = create_cstring (unknownError, out_string);
    }
        
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning string '%s' for code %ld (err = %d)", 
             __FUNCTION__, err ? "(null)" : *out_string, in_code, err);
#endif
    
    return err;    
}

/* ------------------------------------------------------------------------ */

const char *error_message (errcode_t code)
{
    errcode_t err = 0;
    errcode_t enter_err = 0;
    char *message = NULL;
    int reentered = 0; /* some of the functions that we call may call us. */
    
    kim_library_init();

    if (!err) {
        enter_err = com_err_thread_entering_error_message (&reentered);
        
        if (!enter_err && reentered) {
            message = REENTER_ERROR_MESSAGE;
        } else {
            err = enter_err;
        }
    }
    
    if (!err && !message) {
        /* string not found */
        error_table_array_t error_tables = NULL;
        errcode_t lock_err = com_err_thread_lock_error_tables (&error_tables);
        
        if (!lock_err) {
            size_t i;
            
            /* Try to get it from the tables */
            for (i = 0; i < error_tables->count; i++) {
                const struct error_table *et = error_tables->ets[i];
                
                /* Check to see if the error is inside the range: */
                int32_t offset = code - et->base;
                int32_t max = et->base + et->count - 1;
                
                if (((code >= et->base) && (code <= max)) && /* error in range */
                    (et->messages[offset] != NULL) &&        /* message is non-NULL */
                    (strlen (et->messages[offset]) > 0)) {   /* message is non-empty */
                    
                    /* Found the error table containing the error */
                    err = kim_os_string_create_localized ((kim_string *) &message, 
                                                          et->messages[offset]);
#ifdef KERBEROSCOMERR_DEBUG
                    dprintf ("%s: returning string '%s' for code %ld (err = %d)", 
                             __FUNCTION__, 
                             err ? "(null)" : message, in_code, err);
#endif
                    break;
                }
            }
        }
        
        if (!lock_err) { com_err_thread_unlock_error_tables (&error_tables); }
    }
    
    if (!err && !message && (code > 0) && (code < ELAST)) {
        /* strerror mangles non-errno error codes */
        err = create_cstring (strerror (code), &message);
    }

    if (!err && !message) {
        err = create_unknown_error_message (code, &message);
    }
    
    if (!err) {
        err = com_err_thread_set_error_message (message);
    }
    
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning message '%s'", __FUNCTION__, 
             err ? UNKNOWN_ERROR_MESSAGE : message);
#endif
    
    if (!enter_err && !reentered) { com_err_thread_leaving_error_message (); }
    
    return err ? UNKNOWN_ERROR_MESSAGE : message ;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

static void default_com_err_proc (const char *progname, 
                                  errcode_t   code, 
                                  const char *format, 
                                  va_list     args)
{
    if (progname) {
        fputs (progname, stderr);
        fputs (": ", stderr);
    }
    
    if (code) {
        fputs (error_message (code), stderr);
        fputs (" ", stderr);
    }
    
    if (format) {
        vfprintf(stderr, format, args);
    }
    
    /* possibly should do the \r only on a tty in raw mode */
    putc ('\r', stderr);
    putc ('\n', stderr);
    fflush (stderr);
}

/* ------------------------------------------------------------------------ */

void com_err_va (const char *progname, errcode_t code, const char *format, va_list args)
{
    errcode_t err = 0;
    com_err_handler_t handler = NULL;
    
    if (!err) {
        err = com_err_thread_get_com_err_hook (&handler);
    }
    
    if (!err) {
        if (handler == NULL) { handler = default_com_err_proc; }
        (*handler) (progname, code, format, args);
    }
}

/* ------------------------------------------------------------------------ */

void com_err (const char *progname, 
              errcode_t   code, 
              const char *format, ...)
{
    va_list pvar;

    va_start (pvar, format);
    com_err_va (progname, code, format, pvar);
    va_end (pvar);
}

/* ------------------------------------------------------------------------ */

com_err_handler_t set_com_err_hook (com_err_handler_t new_proc)
{
    errcode_t err = 0;
    com_err_handler_t handler = NULL;
    
    if (!err) {
        err = com_err_thread_get_com_err_hook (&handler);
    }
    
    if (!err) {
        err = com_err_thread_set_com_err_hook (new_proc);
    }
    
    return err ? NULL : handler;
}

/* ------------------------------------------------------------------------ */

com_err_handler_t reset_com_err_hook (void)
{
    return set_com_err_hook (NULL);
}

