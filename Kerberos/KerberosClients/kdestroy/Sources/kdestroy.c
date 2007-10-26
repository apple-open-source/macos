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

int destroy_tickets (void);
static int options (int argc, char * const *argv);
static int usage (void);
static void printiferr (errcode_t err, const char *format, ...);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const *argv)
{
    int err = 0;
    
    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* Read in our command line options */
    err = options (argc, argv);
    
    if (!err) {
        err = destroy_tickets ();
    }

    return err;
}
   
int destroy_tickets (void)
{
    krb5_error_code err = 0;
    
    if (cacheName) {
        krb5_context kcontext = NULL;
        krb5_ccache  ccache = NULL;

        /* Initialize the Kerberos 5 context */
        err = krb5_init_context (&kcontext);
        printiferr (err, "while initializing Kerberos 5");
        
        if (!err) {
            err = krb5_cc_resolve (kcontext, cacheName, &ccache);
            printiferr (err, "while locating credentials cache '%s'", cacheName);
        }

        if (!err) {
            err = krb5_cc_destroy (kcontext, ccache);
            printiferr (err, "while destroying credentials cache '%s'", cacheName);
        }
        
        if (kcontext != NULL) { krb5_free_context (kcontext); }
        
    } else if (all) {
        /* Destroy all ticket caches */
        while (!err) {
            err = KLDestroyTickets (NULL);
        }
        err = 0;
        
    } else {
        /* Destroy the tickets by the principal or the default tickets */
        KLPrincipal principal = NULL;
        
        if (principalName) {
            err = KLCreatePrincipalFromString (principalName, kerberosVersion_V5, &principal);
            printiferr (err, "while creating principal for '%s'", principalName);
        }
        
        if (!err) {
            err = KLDestroyTickets (principal);
            if ((err == klPrincipalDoesNotExistErr) || 
                (err == klCacheDoesNotExistErr) ||
                (err == klSystemDefaultDoesNotExistErr)) {
                if (principal && principalName) {
                    printerr ("No credentials cache for principal '%s'\n", principalName);
                } else {
                    printerr ("No default credentials cache\n");
                }
            } else {
                printiferr (err, "while destroying tickets");
            }
        }
        
        if (principal != NULL) { KLDisposePrincipal (principal); }
    }
    
    return err ? 1 : 0;
}

static int options (int argc, char * const *argv)
{
    int option;
    
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
                    printerr ("Only one -c option allowed\n");
                    return usage ();
                }
                
                cacheName = optarg; /* a pointer into argv */
                break;
                
            case 'p':
                if (principalName != NULL) {
                    printerr ("Only one -p option allowed\n");
                    return usage ();
                }
                
                principalName = optarg;
                break;
                
            default:
                return usage ();
        }
    }
        
    if (cacheName != NULL && principalName != NULL) {
        printerr ("Only one of -c or -p allowed\n");
        return usage ();
    }
    
    if (all && (cacheName != NULL || principalName != NULL)) {
        printerr ("-a cannot be combined with -c or -p\n");
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
    if (!quiet) {
        fprintf (stderr, "%s: ", program);
        vfprintf (stderr, format, args);
    }
}

