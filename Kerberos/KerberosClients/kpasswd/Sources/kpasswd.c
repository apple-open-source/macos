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
    if (argc > 1) {
        klErr = KLCreatePrincipalFromString (argv[1], kerberosVersion_V5, &principal);
        if (klErr != klNoErr) {
            printerr ("Unable to create principal for '%s': %s\n", 
                      argv[1], error_message (klErr));
            err = 1;
            goto done;
        }        
    }
    
    /* If that fails, try the principal of the current ticket cache */
    if (principal == NULL) {
        KLBoolean found = false;
        KLPrincipal foundPrincipal;
        
        klErr = KLCacheHasValidTickets (NULL, kerberosVersion_Any, &found, &foundPrincipal, NULL);
        if (klErr == klNoErr && found) {
            principal = foundPrincipal;
        }
    }
    
    /* As a last resort, fall back on the passwd database */
    if (principal == NULL) {
        struct passwd *pw;
        
        /* Try to get the principal name from the password database */
        if ((pw = getpwuid (getuid ())) != NULL) {
            char *username = malloc (strlen (pw->pw_name) + 1); /* copy because pw not stable */
            if (username == NULL) {
                printerr ("Out of memory\n");
                err = 1;
                goto done;
            }
            strcpy (username, pw->pw_name);
            klErr = KLCreatePrincipalFromString (username, kerberosVersion_V5, &principal);
            free (username);
            
            if (klErr != klNoErr) {
                printerr ("Unable to create principal for current user: %s\n", 
                          error_message (klErr));
                err = 1;
                goto done;
            }        
        }
    }

    klErr = KLChangePassword (principal);
    if (klErr != klNoErr) {
        printerr ("Error changing password: %s\n", error_message (klErr));
        goto done;
    }
    
    return 0;
    
done:
    if (principal != NULL)
        KLDisposePrincipal (principal);
                
    if (klErr != klNoErr)
        err = 1;
    
    return err;    
}

static int usage (void)
{
    fprintf (stderr, "Usage: %s principal\n", program);
    return 2;
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


