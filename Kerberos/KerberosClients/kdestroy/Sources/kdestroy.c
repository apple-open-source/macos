/*
 * kdestroy.c
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
int         quiet = 0;
int         all = 0;
const char *cacheName = NULL;
const char *principalName = NULL;
KLPrincipal principal = NULL;

static int options (int argc, char * const *argv);
static int usage (void);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const *argv)
{
    int      err = 0;
    KLStatus klErr = klNoErr;
    cc_int32 ccErr = ccNoError;
    
    cc_context_t context = NULL;
    cc_ccache_t  cache = NULL;
    cc_string_t  principalString = NULL;

    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* Read in our command line options */
    err = options (argc, argv);
    if (err) 
        goto done;
 
    /* Destroy the tickets */
    if (cacheName != NULL) {
        /* Use CCAPI version 4 to get the principal for the cache */
        ccErr = cc_initialize (&context, ccapi_version_4, nil, nil);
        if (ccErr != ccNoError) {
            printerr ("Unable to initialize credentials cache (error = %d)\n", ccErr);
            goto done;
        }
        
        ccErr = cc_context_open_ccache (context, cacheName, &cache);
        if (ccErr == ccErrCCacheNotFound) {
            printerr("No such credentials cache '%s'\n", cacheName);
            goto done;                   
        }
        if (ccErr != ccNoError) {
            printerr("Unable to open credentials cache '%s' (error = %d)\n", cacheName, ccErr);
            goto done;
        }
        
        ccErr = cc_ccache_get_principal (cache, cc_credentials_v5, &principalString);
        if (ccErr == ccNoError) {
            principalName = principalString->data;
            
            err = KLCreatePrincipalFromString (principalName, kerberosVersion_V5, &principal);
            if (err != klNoErr) {
                printerr ("Invalid principal name '%s' in credentials cache '%s': %s\n", 
                            principalName, cacheName, error_message (klErr));
                goto done;
            }
        } else {
            ccErr = cc_ccache_get_principal (cache, cc_credentials_v4, &principalString);
            if (ccErr != ccNoError) {
                printerr ("Unable to get principal for ticket cache '%s' (error = %d)\n", 
                        cacheName, ccErr);
                goto done;
            }
            principalName = principalString->data;
            
            err = KLCreatePrincipalFromString (principalName, kerberosVersion_V4, &principal);
            if (err != klNoErr) {
                printerr ("Invalid principal name '%s' in credentials cache '%s': %s\n", 
                            principalName, cacheName, error_message (klErr));
                goto done;
            }
        }
    }
    
    /* Destroy the cache for the given principal (NULL is the default cache) */
    klErr = KLDestroyTickets (principal);
    if (klErr == klPrincipalDoesNotExistErr) {
        if (principal != NULL && principalName != NULL) {
            printerr ("No credentials cache for principal '%s'\n", principalName);
        } else {
            printerr ("No default credentials cache\n");
        }
        goto done;
    }
    if (klErr != klNoErr) {
        printerr ("Unable to destroy tickets: %s\n", error_message (klErr));
        goto done;
    }
    
    if (all) {
        /* Destroy any additional ticket caches */
        while (klErr == klNoErr) {
            klErr = KLDestroyTickets (NULL);
        }
        klErr = klNoErr;
    }
    
done:
    /* Free any allocated resources */
    if (principal != NULL)
        KLDisposePrincipal (principal);
    
    if (principalString != NULL)
        cc_string_release (principalString);
    
    if (cache != NULL)
        cc_ccache_release (cache);
        
    if (context != NULL)
        cc_context_release (context);
    
    if (klErr != klNoErr || ccErr != ccNoError)
        err = 1;
    
    return err;
}

static int options (int argc, char * const *argv)
{
    int         option;
    KLStatus	err = klNoErr;
    
    /* Get the arguments */
    while ((option = getopt (argc, argv, "qaAc:p:")) != -1) {
        switch (option) {
            case 'q':
                quiet = 1;
                break;
            
            case 'a':
            case 'A':
                all = 1;
                break;
            
            case 'c':
                if (cacheName != NULL) {
                    fprintf (stderr, "Only one -c option allowed\n");
                    return usage ();
                }
                
                cacheName = optarg; /* a pointer into argv */
                
                /* remove the API: if necessary */
                if (strncmp (cacheName, "API:", 4) == 0) {
                    if (strlen (cacheName) > 4) {
                        cacheName += 4;
                    } else {
                        fprintf (stderr, "Invalid cache name '%s'\n", cacheName);
                        return usage ();
                    }
                }
                break;
                
            case 'p':
                if (principal != NULL) {
                    fprintf (stderr, "Only one -p option allowed\n");
                    return usage ();
                }
                
                principalName = optarg;
                err = KLCreatePrincipalFromString (principalName, kerberosVersion_V5, &principal);
                if (err != klNoErr) {
                    fprintf (stderr, "Unable to create principal for '%s': %s\n", 
                            principalName, error_message (err));
                    return 1;
                }
                break;
                
            default:
                return usage ();
        }
    }
        
    if (cacheName != NULL && principal != NULL) {
        fprintf (stderr, "Only one of -c or -p allowed\n");
        return usage ();
    }
    
    if (all && (cacheName != NULL || principal != NULL)) {
        fprintf (stderr, "-a cannot be combined with -c or -p\n");
        return usage ();
    }
    
    return 0;
}


static int usage (void)
{
    fprintf (stderr, "Usage: %s [-q] [-[a|A] | -c cache_name | -p principal]\n", program);
    fprintf (stderr, "\t-q quiet mode\n");
    fprintf (stderr, "\t-[a|A] destroy all caches\n");
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
    if (!quiet) {
        fprintf (stderr, "%s: ", program);
        vfprintf (stderr, format, args);
    }
}

