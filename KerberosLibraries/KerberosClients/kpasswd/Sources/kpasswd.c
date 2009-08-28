/*
 * kpasswd.c
 *
 * Copyright 2002 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
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
 * 
 *
 * Destroy the contents of your credential cache.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/Kerberos.h>
#include <Kerberos/kim.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>


/* ------------------------------------------------------------------------ */

static int usage (void)
{
    fprintf (stderr, "Usage: %s principal\n", getprogname ());
    return 2;
}

/* ------------------------------------------------------------------------ */

static void vprinterr (const char *format, va_list args)
{
    fprintf (stderr, "%s: ", getprogname ());
    vfprintf (stderr, format, args);
}

/* ------------------------------------------------------------------------ */

static void printerr (const char *format, ...)
{
    va_list pvar;
    
    va_start (pvar, format);
    vprinterr (format, pvar);
    va_end (pvar);
}

/* ------------------------------------------------------------------------ */

static void printiferr (kim_error in_error, const char *in_format, ...)
{
    if (in_error && in_error != KIM_USER_CANCELED_ERR) {
        kim_string message = NULL;
        kim_error err = KIM_NO_ERROR;
        va_list pvar;
        
        va_start (pvar, in_format);
        vprinterr (in_format, pvar);
        va_end (pvar);
        
        err = kim_string_create_for_last_error (&message, in_error);
        if (!err) {
            fprintf (stderr, ": %s\n", message);
        } else {
            fprintf (stderr, ".\n");            
        }
        
        kim_string_free (&message);
    }
}

/* ------------------------------------------------------------------------ */

int main (int argc, char * const * argv)
{
    kim_error err = KIM_NO_ERROR;
    kim_identity identity = NULL;

    /* Remember our program name */
    setprogname (argv[0]);
    
    /* look for extra arguments (other than the principal) */
    if (argc > 2) {
        printerr ("Unknown option '%s'\n", argv[2]);
        return usage ();
    }
    
    /* Get the identity  from the command line */
    if (!err && (argc > 1)) {
        err = kim_identity_create_from_string (&identity, argv[1]);
        printiferr (err, "Unable to create principal for '%s'", argv[1]);
    }
    
    /* If that fails, try the identity of the default ticket cache */
    if (!err && !identity) {
        kim_ccache ccache = NULL;
        
        err = kim_ccache_create_from_default (&ccache);
        printiferr (err, "Unable to open the default ccache");
        
        if (!err) {
            err = kim_ccache_get_client_identity (ccache, &identity);

            if (err) {
                /* No identity in default ccache (ccache collection empty) */
                identity = NULL;
                err = KIM_NO_ERROR;
            }
        }
    }
    
    /* As a last resort, fall back on the passwd database */
    if (!err && !identity) {
        struct passwd *pw = getpwuid (getuid ());
        if (!pw) {
            err = ENOENT;
            printiferr (err, "Unable to get current username in password database");
        }
        
        if (!err) {
            err = kim_identity_create_from_string (&identity, pw->pw_name);
            printiferr (err, "Unable to create principal for '%s'", 
                        pw->pw_name);
        }
    }

    if (!err) {
        err = kim_identity_change_password (identity);
        printiferr (err, "Unable to change password");
    }
    
    kim_identity_free (&identity);
    
    return err ? 1 : 0;    
}
