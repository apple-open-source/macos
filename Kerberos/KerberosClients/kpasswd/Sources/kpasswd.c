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
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

const char *program = NULL;

static int usage (void);
static void printiferr (errcode_t err, const char *format, ...);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const * argv)
{
    int         err = 0;
    KLStatus    klErr = klNoErr;
    KLPrincipal principal = NULL;

    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* look for extra arguments (other than the principal) */
    if (argc > 2) {
        printerr ("Unknown option '%s'\n", argv[2]);
        return usage ();
    }
    
    /* Get the principal name from the command line */
    if (!err && (argc > 1)) {
        err = KLCreatePrincipalFromString (argv[1], kerberosVersion_V5, &principal);
        printiferr (err, "while creating principal '%s'", argv[1]);
    }
    
    /* If that fails, try the principal of the current ticket cache */
    if (!err && (principal == NULL)) {
        KLBoolean   found = false;
        KLPrincipal foundPrincipal;
        KLStatus    terr = KLCacheHasValidTickets (NULL, kerberosVersion_Any, &found, &foundPrincipal, NULL);
        if (!terr && found) { principal = foundPrincipal; }
    }
    
    /* As a last resort, fall back on the passwd database */
    if (!err && (principal == NULL)) {
        struct passwd *pw;
        
        /* Try to get the principal name from the password database */
        if ((pw = getpwuid (getuid ())) != NULL) {
            char *username = malloc (strlen (pw->pw_name) + 1); /* copy because pw not stable */
            if (username == NULL) { err = ENOMEM; }

            if (!err) {
                strcpy (username, pw->pw_name);
                err = KLCreatePrincipalFromString (username, kerberosVersion_V5, &principal);
            }
            
            printiferr (err, "while creating principal for current user");

            if (username != NULL) { free (username); }
        }
    }

    if (!err) {
        err = KLChangePassword (principal);
        printiferr (err, "while changing password");
    }
    
    if (principal != NULL) { KLDisposePrincipal (principal); }
    
    return err ? 1 : 0;    
}

static int usage (void)
{
    fprintf (stderr, "Usage: %s principal\n", program);
    return 2;
}

static void printiferr (errcode_t err, const char *format, ...)
{
    if (err && (err != ccIteratorEnd) && (err != KRB5_CC_END)) {
        va_list pvar;
        
        va_start (pvar, format);
        com_err_va (program, err, format, pvar);
        va_end (pvar);
    }
}

static void printerr (const char *format, ...)
{
    va_list pvar;

    va_start (pvar, format);
    vprinterr (format, pvar);
    va_end (pvar);
}

static void vprinterr (const char *format, va_list args)
{
    fprintf (stderr, "%s: ", program);
    vfprintf (stderr, format, args);
}


