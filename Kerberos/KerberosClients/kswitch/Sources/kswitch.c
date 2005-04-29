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
KLPrincipal principal = NULL;

static int options (int argc, char * const * argv);
static int usage (void);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const * argv)
{
    int      err = 0;
    KLStatus klErr = klNoErr;
    cc_int32 ccErr = ccNoError;
    
    cc_context_t context = NULL;
    cc_ccache_t  cache = NULL;

    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* Read in our command line options */
    err = options (argc, argv);
    if (err) 
        goto done;

    /* Set the system default */
    if (cacheName != NULL) {
        /* Use CCAPI version 4 to set the default ccache */
        ccErr = cc_initialize (&context, ccapi_version_4, nil, nil);
        if (ccErr != ccNoError) {
            printerr ("Unable to initialize credentials cache (error = %d)\n", ccErr);
            goto done;
        }
        
        ccErr = cc_context_open_ccache (context, cacheName, &cache);
        if (ccErr == ccErrCCacheNotFound) {
            printerr("No credentials cache named '%s'\n", cacheName);
            goto done;                   
        }
        if (ccErr != ccNoError) {
            printerr("Unable to open credentials cache '%s' (error = %d)\n", cacheName, ccErr);
            goto done;
        }
        
        ccErr = cc_ccache_set_default (cache);
        if (ccErr != ccNoError) {
            printerr ("Unable to set ccache '%s' to the system default (error = %d)\n", 
                    cacheName, ccErr);
            goto done;
        }
    } else {
        /* Use the provided principal to set the system default */
        klErr = KLSetSystemDefaultCache (principal);
        if (klErr != klNoErr) {
            if (principal != NULL && principalName != NULL) {
                printerr ("No credentials cache for principal '%s'\n", principalName);
            } else {
                printerr ("Unable to set system default: %s\n", error_message (klErr));
            }
            goto done;
        }
    }
    
done:
    /* Free any allocated resources */
    if (principal != NULL)
        KLDisposePrincipal (principal);
    
    if (cache != NULL)
        cc_ccache_release (cache);
        
    if (context != NULL)
        cc_context_release (context);
    
    if (klErr != klNoErr || ccErr != ccNoError)
        err = 1;
    
    return err;
}

static int options (int argc, char * const * argv)
{
    int option;
    KLStatus err = klNoErr;
    
    /* Get the arguments */
    while ((option = getopt (argc, argv, "c:p:")) != -1) {
        switch (option) {
            case 'c':
                if (cacheName != NULL) {
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
                if (principal != NULL) {
                    printerr ("Only one -p option allowed\n");
                    return usage ();
                }
                
                principalName = optarg;
                err = KLCreatePrincipalFromString (principalName, kerberosVersion_V5, &principal);
                if (err != klNoErr) {
                    printerr ("Unable to create principal for '%s': %s\n", 
                                principalName, error_message (err));
                    return 1;
                }
                break;
                
            default:
                return usage ();
        }
    }
        
    if (cacheName != NULL && principal != NULL) {
        printerr ("Only one of -c or -p allowed\n");
        return usage ();
    }
    
    if (cacheName == NULL && principal == NULL) {
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

