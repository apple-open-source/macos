/*
 * com_err.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosErrors/Sources/com_err.c,v 1.4 2003/08/22 16:20:57 lxs Exp $
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

/*
 * Defines:
 */

#define UNKNOWN_ERROR_FORMAT       "Unknown Error Code: %ld"
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
 * Types:
 */

typedef struct error_tables {
    size_t count;
    size_t size;
    const struct error_table **ets;
} error_tables;

/*
 * Prototypes to static functions:
 */

static void default_com_err_proc (const char *progname, errcode_t code, const char *format, va_list args);
static const char *bundle_error_string_for_format (errcode_t code, const char *format);
static const char *unknown_error_message (errcode_t code);
static const char *error_table_name (int32_t base);

/* 
 * Globals:
 */

static com_err_handler_t g_com_err_hook = default_com_err_proc;
error_tables g_error_tables = {0, 0, NULL}; 



#pragma mark -

/* ***************** */
/* *** FUNCTIONS *** */
/* ***************** */

errcode_t 
add_error_table (const struct error_table *et)
{
    errcode_t err = 0;

    if (et == NULL) { err = EINVAL; }

    if (!err) {
        if ((g_error_tables.count + 1) > g_error_tables.size) {
            const struct error_table **new_ets = NULL;
            size_t new_size = g_error_tables.size + ERROR_TABLE_SIZE_INCREMENT;

            new_ets = (const struct error_table **) realloc (g_error_tables.ets, (sizeof (char *)) * new_size);
            if (new_ets == NULL) {
                err = ENOMEM;
            } else {
                g_error_tables.ets = new_ets;
                g_error_tables.size = new_size;
            }
        }
    }

    if (!err) {
        g_error_tables.ets[g_error_tables.count] = et; // add to the end
        g_error_tables.count++;
    }
    
    return err;
}

errcode_t 
remove_error_table (const struct error_table *et)
{
    errcode_t err = 0;
    size_t index = 0;

    if (et == NULL) { err = EINVAL; }

    if (!err) {
        int found = false;
        size_t i;
        
        // find the error table
        for (i = 0; i < g_error_tables.count; i++) {
            if (g_error_tables.ets[i] == et) {
                index = i;
                found = true;
            }
        }
        
        if (!found) { err = EINVAL; }
    }

    if (!err) {
        // shift the remaining elements over
        memmove (&g_error_tables.ets[index], &g_error_tables.ets[index + 1],
                 ((g_error_tables.count - 1) - index) * sizeof (struct error_table **));
        g_error_tables.count--;
    }

    return err;
}

#pragma mark -

const char *
error_message (errcode_t code)
{
    const char *message = bundle_error_string_for_format (code, MESSAGE_KEY_FORMAT);

    if (message == NULL) {
        size_t i;
        
        // Try to get it from the tables
        for (i = 0; i < g_error_tables.count; i++) {
            const struct error_table *et = g_error_tables.ets[i];

            // Check to see if the error is inside the range:
            int32_t offset = code - et->base;
            int32_t max = et->base + et->count - 1;

            if (((code >= et->base) && (code < max)) &&   // error in range
                (et->messages[offset] != NULL) &&         // message is non-NULL
                (strlen (et->messages[offset]) > 0)) {    // message is non-empty
                
                message = et->messages[offset];	// Found the error table containing the error
                break;
            }
        }
    }

    if (message == NULL) {
        if ((code > 0) && (code < ELAST)) {
            message = strerror (code); // strerror mangles non-errno error codes
        }
    }

    if (message == NULL) {
        message = unknown_error_message (code);
    }
    
    return message;
}

const char *
error_manager (errcode_t code)
{
    const char *manager = bundle_error_string_for_format (code, MANAGER_KEY_FORMAT);

    if (manager == NULL) {
        size_t i;

        // Try to get it from the tables
        for (i = 0; i < g_error_tables.count; i++) {
            const struct error_table *et = g_error_tables.ets[i];
            
            // Check to see if the error is inside the range:
            int32_t offset = code - et->base;
            int32_t max = et->base + et->count - 1;

            if (((code >= et->base) && (code < max)) &&   // error in range
                (et->messages[offset] != NULL) &&         // message is non-NULL
                (strlen (et->messages[offset]) > 0)) {    // message is non-empty

                if (et->messages[et->count] != 0) {
                    // Our new compile_et places the manager name after the messages:
                    manager = et->messages[et->count];
                } else {
                    // Fall back on the old way of calculating it:
                    int32_t tableBase = code - (code & ((1 << ERRCODE_RANGE) - 1));
                    manager = error_table_name (tableBase);
                }
                break;
            }
        }
    }

    if (manager == NULL) {
        manager = UNKNOWN_ERROR_MANAGER;
    }

    return manager;
}

#pragma mark -

void 
com_err_va (const char *progname, errcode_t code, const char *format, va_list args)
{
    (*g_com_err_hook) (progname, code, format, args);
}

void 
com_err (const char *progname, errcode_t code, const char *format, ...)
{
    va_list pvar;

    va_start (pvar, format);
    com_err_va (progname, code, format, pvar);
    va_end (pvar);
}

com_err_handler_t 
set_com_err_hook (com_err_handler_t new_proc)
{
    com_err_handler_t handler = g_com_err_hook;

    g_com_err_hook = (new_proc) ? new_proc : default_com_err_proc;
    return handler;
}

com_err_handler_t 
reset_com_err_hook (void)
{
    com_err_handler_t handler = g_com_err_hook;

    g_com_err_hook = default_com_err_proc;
    return handler;
}

static void 
default_com_err_proc (const char *progname, errcode_t code, const char *format, va_list args)
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

#pragma mark -

static const char *
bundle_error_string_for_format (errcode_t code, const char *format)
{
    static char s_string[MAX_ERROR_MESSAGE_SIZE];

    errcode_t err = 0;
    CFStringRef key = NULL;
    CFStringRef value = NULL;
    char keyString [MAX_KEY_LENGTH];
    CFStringEncoding encoding = __KLApplicationGetTextEncoding ();

    if (err == 0) {
        snprintf (keyString, sizeof (keyString), format, code);
        keyString [sizeof (keyString) - 1] = '\0';
    }

    if (err == 0) {
        key = CFStringCreateWithCString (kCFAllocatorDefault, keyString, kCFStringEncodingASCII);
        if (key == NULL) { err = ENOMEM; }
    }

    // Try to find the key, first searching in the framework, then in the main bundle
    if (err == 0) {
        CFBundleRef frameworkBundle = CFBundleGetBundleWithIdentifier (CFSTR ("edu.mit.Kerberos"));
        if (frameworkBundle != NULL) {
            value = (CFStringRef) CFBundleGetValueForInfoDictionaryKey (frameworkBundle, key);
        }

        if (value == NULL) {
            CFBundleRef mainBundle = CFBundleGetMainBundle ();
            if (mainBundle != NULL) {
                value = (CFStringRef) CFBundleGetValueForInfoDictionaryKey (mainBundle, key);
            }
        }
    }

    // If we got a key, try to pull it out
    if (err == 0) {
        if ((value == NULL) || (CFGetTypeID (value) != CFStringGetTypeID ())) {
            err = EINVAL;
        }
    }

    if (err == 0) {
        CFIndex length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (value), encoding) + 1;
        if (length > MAX_ERROR_MESSAGE_SIZE) {
            dprintf ("bundle_error_string_for_format: Error string of length %ld is too long\n", length);
            err = EINVAL;
        }
    }

    if (err == 0) {
        if (CFStringGetCString (value, s_string, MAX_ERROR_MESSAGE_SIZE, encoding) != true) {
            err = ENOMEM;
        }
    }

    if (key != NULL) { CFRelease (key); }

    if (err == 0) {
        return s_string;
    } else {
        return NULL;
    }
}

static const char *
unknown_error_message (errcode_t code)
{
    static char unknownError[UNKNOWN_ERROR_MAX_SIZE];

    snprintf (unknownError, UNKNOWN_ERROR_MAX_SIZE, UNKNOWN_ERROR_FORMAT, code);
    unknownError[UNKNOWN_ERROR_MAX_SIZE - 1] = '\0';

    return unknownError;    
}


static const char *
error_table_name (int32_t base)
{
    static char table_name[6];

    int ch;
    int i;
    char *p;
    const char char_set[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";

    /* num = aa aaa abb bbb bcc ccc cdd ddd d?? ??? ??? */
    p = table_name;
    base >>= ERRCODE_RANGE;
    /* num = ?? ??? ??? aaa aaa bbb bbb ccc ccc ddd ddd */
    base &= 077777777;
    /* num = 00 000 000 aaa aaa bbb bbb ccc ccc ddd ddd */
    for (i = 4; i >= 0; i--) {
        ch = (base >> BITS_PER_CHAR * i) & ((1 << BITS_PER_CHAR) - 1);
        if (ch != 0)
            *p++ = char_set[ch-1];
    }
    *p = '\0';

    return table_name;
}
