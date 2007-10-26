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
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

const char *program = NULL;
const char *cacheName = NULL;
const char *principalName = NULL;

static int options (int argc, char * const * argv);
static int usage (void);
static void printiferr (errcode_t err, const char *format, ...);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const * argv)
{
    int err = 0;
    
    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* Read in our command line options */
    if (!err) { 
        err = options (argc, argv);
    }
    
    /* Set the system default */
    if (cacheName) {
        cc_context_t context = NULL;
        cc_ccache_t  cache = NULL;

        /* Use CCAPI version 4 to set the default ccache */
        if (!err) { 
            err = cc_initialize (&context, ccapi_version_4, nil, nil);
            printiferr (err, "while initializing credentials cache");
        }

        if (!err) {
            err = cc_context_open_ccache (context, cacheName, &cache);
            printiferr(err, "while opening credentials cache '%s'\n", cacheName);
        }
        
        if (!err) {
            err = cc_ccache_set_default (cache);
            printiferr (err, "while setting ccache '%s' to the system default", cacheName);
        }

        if (cache  ) { cc_ccache_release (cache); }
        if (context) { cc_context_release (context); } 
    } else if (principalName) {
        KLPrincipal principal = NULL;

        if (!err) {
            err = KLCreatePrincipalFromString (principalName, kerberosVersion_V5, &principal);
            printiferr (err, "while creating KLPrincipal for '%s'", principalName);
        }

        /* Use the provided principal to set the system default */
        if (!err) {
            err = KLSetSystemDefaultCache (principal);
            printiferr (err, "while setting the cache for principal '%s' to the system default", 
                        principalName);
         }

        if (principal) { KLDisposePrincipal (principal); }
    }
    
    /* Free any allocated resources */
    
    return err ? 1 : 0;
}

static int options (int argc, char * const * argv)
{
    int option;
    
    /* Get the arguments */
    while ((option = getopt (argc, argv, "c:p:")) != -1) {
        switch (option) {
            case 'c':
                if (cacheName) {
                    printerr ("Only one -c option allowed\n");
                    return usage ();
                }
                
                cacheName = optarg; /* a pointer into argv */
                
                /* remove the API: if necessary */
                if (strncmp (cacheName, "API:", 4) == 0) {
                    if (strlen (cacheName) > 4) {
                        cacheName += 4;
                    } else {
                        printerr ("Invalid cache name '%s'\n", cacheName);
                        return usage ();
                    }
                }
                break;
                
            case 'p':
                if (principalName) {
                    printerr ("Only one -p option allowed\n");
                    return usage ();
                }
                
                principalName = optarg;
                break;
                
            default:
                return usage ();
        }
    }
        
    if (cacheName && principalName) {
        printerr ("Only one of -c or -p allowed\n");
        return usage ();
    }
    
    if (!cacheName && !principalName) {
        printerr ("One of -c or -p must be specified\n");
        return usage ();
    }
    
    /* look for extra arguments */
    if (argc - optind > 0) {
        printerr ("Unknown option '%s'\n", argv[optind]);
        return usage ();
    }

    return 0;
}


static int usage (void)
{
    fprintf (stderr, "Usage: %s [-c cache_name | -p principal]\n", program);
    fprintf (stderr, "\t-c specify name of credentials cache\n");
    fprintf (stderr, "\t-p specify name of principal (Kerberos 5 format)\n");
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

