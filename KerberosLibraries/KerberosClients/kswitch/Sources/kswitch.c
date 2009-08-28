/*
 * kswitch.c
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
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ------------------------------------------------------------------------ */

static int usage (void)
{
    fprintf (stderr, "Usage: %s [-c cache_name | -p principal]\n", getprogname ());
    fprintf (stderr, "\t-c specify name of credentials cache\n");
    fprintf (stderr, "\t-p specify name of principal (Kerberos 5 format)\n");
    return 2;
}

/* ------------------------------------------------------------------------ */

static void printiferr (errcode_t err, const char *format, ...)
{
    if (err && (err != ccIteratorEnd) && (err != KRB5_CC_END)) {
        va_list pvar;
        
        va_start (pvar, format);
        com_err_va (getprogname (), err, format, pvar);
        va_end (pvar);
    }
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

static int options (int            in_argc, 
                    char * const  *in_argv,
                    const char   **out_ccache_name,
                    const char   **out_principal_name)
{
    int option;
    const char *principal_name = NULL;
    const char *ccache_name = NULL;
    
    /* Get the arguments */
    while ((option = getopt (in_argc, in_argv, "c:p:")) != -1) {
        switch (option) {
            case 'c':
                if (ccache_name) {
                    printerr ("Only one -c option allowed\n");
                    return usage ();
                }
                
                ccache_name = optarg; /* a pointer into argv */
                break;
                
            case 'p':
                if (principal_name) {
                    printerr ("Only one -p option allowed\n");
                    return usage ();
                }
                
                principal_name = optarg;
                break;
                
            default:
                return usage ();
        }
    }
    
    if (ccache_name && principal_name) {
        printerr ("Only one of -c or -p allowed\n");
        return usage ();
    }
    
    if (!ccache_name && !principal_name) {
        printerr ("One of -c or -p must be specified\n");
        return usage ();
    }
    
    /* look for extra arguments */
    if (in_argc - optind > 0) {
        printerr ("Unknown option '%s'\n", in_argv[optind]);
        return usage ();
    }
    
    *out_ccache_name = ccache_name;
    *out_principal_name = principal_name;
    
    return 0;
}

/* ------------------------------------------------------------------------ */

int main (int argc, char * const * argv)
{
    const char *ccache_name = NULL;
    const char *identity_name = NULL;
    kim_ccache ccache = NULL;
    
    int err = 0;
    
    /* Remember our program name */
    setprogname (argv[0]);

    /* Read in our command line options */
    if (!err) { 
        err = options (argc, argv, 
                       &ccache_name, &identity_name);
    }
    
    if (!err && ccache_name) {
        err = kim_ccache_create_from_display_name (&ccache, ccache_name);
        printiferr (err, "Unable to open ccache '%s'", ccache_name);
        
    } else if (!err && identity_name) {
        kim_identity identity = NULL;
        
        err = kim_identity_create_from_string (&identity, identity_name);
        printiferr (err, "Unable to create identity for '%s'", identity_name);
        
        if (!err) {
            err = kim_ccache_create_from_client_identity (&ccache, identity);
            printiferr (err, "Unable to open ccache for '%s'", identity_name);
        }
        
        kim_identity_free (&identity);
    }
    
    if (!err && ccache) {
        err = kim_ccache_set_default (ccache);
        printiferr (err, "Unable to set default ccache");
    }

    kim_ccache_free (&ccache);
    
    return err ? 1 : 0;
}
