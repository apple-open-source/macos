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
#include <Kerberos/kim.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int         quiet = 0;
int         all = 0;
const char *cache_name = NULL;
const char *principal_name = NULL;

/* ------------------------------------------------------------------------ */

static int usage (void)
{
    fprintf (stderr, "Usage: %s [-q] [-[a|A] | -c cache_name | -p principal]\n", 
             getprogname ());
    fprintf (stderr, "\t-q quiet mode\n");
    fprintf (stderr, "\t-[a|A] destroy all caches\n");
    fprintf (stderr, "\t-c specify name of credentials cache\n");
    fprintf (stderr, "\t-p specify name of principal (Kerberos 5 format)\n");
    return 2;
}

/* ------------------------------------------------------------------------ */

static void printiferr (errcode_t err, const char *format, ...)
{
    if (err) {
        va_list pvar;
        
        va_start (pvar, format);
        com_err_va (getprogname (), err, format, pvar);
        va_end (pvar);
    }
}

/* ------------------------------------------------------------------------ */

static void vprinterr (const char *format, va_list args)
{
    if (!quiet) {
        fprintf (stderr, "%s: ", getprogname ());
        vfprintf (stderr, format, args);
    }
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

static int destroy_tickets (void)
{
    krb5_error_code err = 0;
    
    if (cache_name) {
        kim_ccache ccache = NULL;
        
        err = kim_ccache_create_from_display_name (&ccache, cache_name);
        printiferr (err, "while locating credentials cache '%s'", 
                    cache_name);
        
        if (!err) {
            err = kim_ccache_destroy (&ccache);
            printiferr (err, "while destroying credentials cache '%s'", 
                        cache_name);
        }
        
    } else if (all) {
        /* Destroy all ticket caches */
        kim_ccache_iterator iterator = NULL;
        
        err = kim_ccache_iterator_create (&iterator);
        
        while (!err) {
            kim_ccache ccache = NULL;
            
            err = kim_ccache_iterator_next (iterator, &ccache);
            
            if (!err) {
                if (ccache) {
                    err = kim_ccache_destroy (&ccache);
                } else {
                    break; /* no more ccaches */
                }
            }
        }
        
        kim_ccache_iterator_free (&iterator);
        
    } else {
        /* Destroy the tickets by the principal or the default tickets */
        kim_identity identity = KIM_IDENTITY_ANY;
        kim_ccache ccache = NULL;
        
        if (principal_name) {
            err = kim_identity_create_from_string (&identity, principal_name);
            printiferr (err, "while creating identity for '%s'", 
                        principal_name);
        }
        
        if (!err) {
            kim_ccache_create_from_client_identity (&ccache, identity);
            if (identity) {
                printiferr (err, "while getting ccache for '%s'", 
                            principal_name);
            } else {
                printiferr (err, "while getting default ccache");
            }
        }
        
        if (!err) {
            err = kim_ccache_destroy (&ccache);
            printiferr (err, "while destroying tickets");
        }
        
        kim_identity_free (&identity);
    }
    
    return err ? 1 : 0;
}

/* ------------------------------------------------------------------------ */

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
                if (cache_name) {
                    printerr ("Only one -c option allowed\n");
                    return usage ();
                }
                
                cache_name = optarg; /* a pointer into argv */
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
    
    if (cache_name && principal_name) {
        printerr ("Only one of -c or -p allowed\n");
        return usage ();
    }
    
    if (all && (cache_name || principal_name)) {
        printerr ("-a cannot be combined with -c or -p\n");
        return usage ();
    }
    
    return 0;
}

/* ------------------------------------------------------------------------ */

int main (int argc, char * const *argv)
{
    int err = 0;
    
    /* Remember our program name */
    setprogname (argv[0]);
    
    /* Read in our command line options */
    err = options (argc, argv);
    
    if (!err) {
        err = destroy_tickets ();
    }

    return err;
}
 
