/*
 * com_err.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosErrors/Sources/com_err.c,v 1.5 2004/10/04 17:48:24 lxs Exp $
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

#include <Kerberos/com_err.h>
#include <Kerberos/KerberosDebug.h>
#include <Kerberos/KerberosLoginPrivate.h>  // for __KLApplicationGetTextEncoding

#include <CoreFoundation/CoreFoundation.h>

#include "com_err_threads.h"

/*
 * Defines:
 */

#define UNKNOWN_ERROR_FORMAT       "Unknown Error Code: %ld"
#define UNKNOWN_ERROR_MESSAGE      "Unknown error"
#define UNKNOWN_ERROR_MANAGER      "Unknown API"
#define UNKNOWN_ERROR_MAX_SIZE     256

#define MESSAGE_KEY_FORMAT         "KEMessage %ld"
#define MANAGER_KEY_FORMAT         "KEManager %ld"
#define MAX_KEY_LENGTH             256
#define MAX_ERROR_MESSAGE_SIZE     BUFSIZ

#define ERROR_TABLE_SIZE_INCREMENT 10

#define ERRCODE_RANGE              8
#define BITS_PER_CHAR              6


/*
 * Prototypes to static functions:
 */


/* ***************** */
/* *** FUNCTIONS *** */
/* ***************** */


#pragma mark -

// ---------------------------------------------------------------------------

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
            
            new_ets = (const struct error_table **) realloc (error_tables->ets, (sizeof (char *)) * new_size);
            if (new_ets == NULL) {
                err = ENOMEM;
            } else {
                error_tables->ets = new_ets;
                error_tables->size = new_size;
            }
        }
    }
    
    if (!err) {
        error_tables->ets[error_tables->count] = et; // add to the end
        error_tables->count++;
    }
    
    if (!lock_err) { com_err_thread_unlock_error_tables (&error_tables); }
    
    return err;
}

// ---------------------------------------------------------------------------

errcode_t remove_error_table (const struct error_table *et)
{
    error_table_array_t error_tables = NULL;
    errcode_t lock_err = com_err_thread_lock_error_tables (&error_tables);
    errcode_t err = lock_err;
    size_t index = 0;

    if (!err) {
        if (et == NULL) { err = EINVAL; }
    }
    
    if (!err) {
        int found = false;
        size_t i;
        
        // find the error table
        for (i = 0; i < error_tables->count; i++) {
            if (error_tables->ets[i] == et) {
                index = i;
                found = true;
            }
        }
        
        if (!found) { err = EINVAL; }
    }
    
    if (!err) {
        // shift the remaining elements over
        memmove (&error_tables->ets[index], &error_tables->ets[index + 1],
                 ((error_tables->count - 1) - index) * sizeof (struct error_table **));
        error_tables->count--;
    }
    
    if (!lock_err) { com_err_thread_unlock_error_tables (&error_tables); }
    
    return err;
}

#pragma mark -

// ---------------------------------------------------------------------------

static errcode_t create_cstring (const char *in_string, char **out_string)
{
    errcode_t err = 0;
    
    if (in_string  == NULL) { err = EINVAL; }
    if (out_string == NULL) { err = EINVAL; }
    
    if (!err) {
        *out_string = (char *) malloc (strlen (in_string) + 1);
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

// ---------------------------------------------------------------------------

static errcode_t create_cstring_from_cfstring (CFStringRef in_cfstring, char **out_string)
{
    errcode_t err = klNoErr;
    char *string = NULL;
    CFIndex length = 0;
    CFStringEncoding encoding = __KLApplicationGetTextEncoding ();
    
    if (in_cfstring == NULL) { err = EINVAL; }
    if (out_string  == NULL) { err = EINVAL; }
    
    if (!err) {
        length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (in_cfstring), encoding) + 1;
    }
    
    if (!err) {
        string = (char *) calloc (length, sizeof (char));
        if (string == NULL) { err = ENOMEM; }
    }
    
    if (!err) {
        if (CFStringGetCString (in_cfstring, string, length, encoding) != true) {
            err = ENOMEM;
        }
    }
    
    if (!err) {
        *out_string = string;
    } else {
        free (string);
    }
    
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning string '%s' (err = %d)", __FUNCTION__, err ? "(null)" : *out_string, err);
#endif
    
    return err;
}
// ---------------------------------------------------------------------------

static errcode_t create_bundle_error_string_for_format (errcode_t in_code, const char *in_format, char **out_string)
{
    errcode_t lock_err = com_err_thread_lock_for_bundle_lookup ();
    errcode_t err = lock_err;
    char keyString [MAX_KEY_LENGTH];
    CFStringRef cfstring = NULL;
    
    if (!err) {
        snprintf (keyString, sizeof (keyString), in_format, in_code);
        keyString [sizeof (keyString) - 1] = '\0';
    }
    
    if (!err) {
        cfstring = __KLGetCFStringForInfoDictionaryKey (keyString);
        if (cfstring == NULL) { 
            *out_string = NULL;
        } else {
            err = create_cstring_from_cfstring (cfstring, out_string);
        }
    }
    
    if (!lock_err) { com_err_thread_unlock_for_bundle_lookup (); }

#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning string '%s' for code %ld (err = %d)", 
             __FUNCTION__, err ? "(null)" : *out_string, in_code, err);
#endif
    
    return err;
}

// ---------------------------------------------------------------------------

static errcode_t create_unknown_error_message (errcode_t in_code, char **out_string)
{
    errcode_t err = 0;
    
    if (!err) {
        char unknownError[UNKNOWN_ERROR_MAX_SIZE];
        
        snprintf (unknownError, UNKNOWN_ERROR_MAX_SIZE, UNKNOWN_ERROR_FORMAT, in_code);
        unknownError[UNKNOWN_ERROR_MAX_SIZE - 1] = '\0';
        
        err = create_cstring (unknownError, out_string);
    }
        
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning string '%s' for code %ld (err = %d)", 
             __FUNCTION__, err ? "(null)" : *out_string, in_code, err);
#endif
    
    return err;    
}

// ---------------------------------------------------------------------------

static errcode_t create_error_table_name (int32_t in_base, char **out_string)
{
    errcode_t err = 0;
    
    if (!err) {
        char table_name[6];
        
        int ch;
        int i;
        char *p;
        const char char_set[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
        
        /* num = aa aaa abb bbb bcc ccc cdd ddd d?? ??? ??? */
        p = table_name;
        in_base >>= ERRCODE_RANGE;
        /* num = ?? ??? ??? aaa aaa bbb bbb ccc ccc ddd ddd */
        in_base &= 077777777;
        /* num = 00 000 000 aaa aaa bbb bbb ccc ccc ddd ddd */
        for (i = 4; i >= 0; i--) {
            ch = (in_base >> BITS_PER_CHAR * i) & ((1 << BITS_PER_CHAR) - 1);
            if (ch != 0)
                *p++ = char_set[ch-1];
        }
        *p = '\0';
        
        err = create_cstring (table_name, out_string);
    }
    
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning string '%s' (err = %d)", __FUNCTION__, err ? "(null)" : *out_string, err);
#endif
    
    return err;
}

// ---------------------------------------------------------------------------

const char *error_message (errcode_t code)
{
    errcode_t err = 0;
    char *message = NULL;
    
    if (!err) {
        err = create_bundle_error_string_for_format (code, MESSAGE_KEY_FORMAT, &message);
    }
    
    if (!err) {
        if (message == NULL) {
            // string not found
            error_table_array_t error_tables = NULL;
            errcode_t lock_err = com_err_thread_lock_error_tables (&error_tables);
            
            if (!lock_err) {
                size_t i;
                
                // Try to get it from the tables
                for (i = 0; i < error_tables->count; i++) {
                    const struct error_table *et = error_tables->ets[i];
                    
                    // Check to see if the error is inside the range:
                    int32_t offset = code - et->base;
                    int32_t max = et->base + et->count - 1;
                    
                    if (((code >= et->base) && (code <= max)) &&   // error in range
                        (et->messages[offset] != NULL) &&         // message is non-NULL
                        (strlen (et->messages[offset]) > 0)) {    // message is non-empty
                        
                        // Found the error table containing the error
                        err = create_cstring (et->messages[offset], &message);
                        break;
                    }
                }
            }
            
            if (!lock_err) { com_err_thread_unlock_error_tables (&error_tables); }
        }
    }
    
    if (!err) {
        if ((message == NULL) && (code > 0) && (code < ELAST)) {
            // strerror mangles non-errno error codes
            err = create_cstring (strerror (code), &message);
        }
    }

    if (!err) {
        if (message == NULL) {
            err = create_unknown_error_message (code, &message);
        }
    }
    
    if (!err) {
        err = com_err_thread_set_error_message (message);
    }
    
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning message '%s'", __FUNCTION__, err ? UNKNOWN_ERROR_MESSAGE : message);
#endif
    
    return err ? UNKNOWN_ERROR_MESSAGE : message ;
}

// ---------------------------------------------------------------------------

const char *error_manager (errcode_t code)
{
    errcode_t err = 0;
    char *manager = NULL;
    
    if (!err) {
        err = create_bundle_error_string_for_format (code, MANAGER_KEY_FORMAT, &manager);
    }
    
    if (!err) {
        if (manager == NULL) {
            error_table_array_t error_tables = NULL;
            errcode_t lock_err = com_err_thread_lock_error_tables (&error_tables);
            
            if (!lock_err) {
                size_t i;
                
                // Try to get it from the tables
                for (i = 0; i < error_tables->count; i++) {
                    const struct error_table *et = error_tables->ets[i];
                    
                    // Check to see if the error is inside the range:
                    int32_t offset = code - et->base;
                    int32_t max = et->base + et->count - 1;
                    
                    if (((code >= et->base) && (code < max)) &&   // error in range
                        (et->messages[offset] != NULL) &&         // message is non-NULL
                        (strlen (et->messages[offset]) > 0)) {    // message is non-empty
                        
                        if (et->messages[et->count] != 0) {
                            // Our new compile_et places the manager name after the messages:
                            err = create_cstring (et->messages[et->count], &manager);
                        } else {
                            // Fall back on the old way of calculating it:
                            int32_t tableBase = code - (code & ((1 << ERRCODE_RANGE) - 1));
                            err = create_error_table_name (tableBase, &manager);
                        }
                        break;
                    }
                }
            }
            
            if (!lock_err) { com_err_thread_unlock_error_tables (&error_tables); }
        }
    }

    if (!err) {
        if (manager == NULL) {
            err = create_cstring (UNKNOWN_ERROR_MANAGER, &manager);
        }
    }
    
    if (!err) {
        err = com_err_thread_set_error_manager (manager);
    }
        
#ifdef KERBEROSCOMERR_DEBUG
    dprintf ("%s: returning manager '%s'", __FUNCTION__, err ? UNKNOWN_ERROR_MANAGER : manager);
#endif
    
    return err ? UNKNOWN_ERROR_MANAGER : manager;
}

#pragma mark -

// ---------------------------------------------------------------------------

static void default_com_err_proc (const char *progname, errcode_t code, const char *format, va_list args)
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

// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------

void com_err (const char *progname, errcode_t code, const char *format, ...)
{
    va_list pvar;

    va_start (pvar, format);
    com_err_va (progname, code, format, pvar);
    va_end (pvar);
}

// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------

com_err_handler_t reset_com_err_hook (void)
{
    return set_com_err_hook (NULL);
}

