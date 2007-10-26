/*
 * kvno.c
 *
 * Copyright 2003 by the Massachusetts Institute of Technology.
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
char *ccacheString = NULL;
char *etypeString = NULL;
char *keytabString = NULL;
char * const *serviceNames = NULL;
int serviceNameCount = 0;
int quiet = 0;

static int options (int argc, char * const * argv);
static int usage (void);
static void printiferr (errcode_t err, const char *format, ...);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const * argv)
{
    int err = 0;
    int errCount = 0;
    int i;
    krb5_context context = NULL;
    krb5_principal client = NULL;
    krb5_ccache ccache = NULL;
    krb5_enctype etype = 0;
    krb5_keytab keytab = NULL;
    
    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* Read in our command line options */
    if (!err) {
        err = options (argc, argv);
    }

    if (!err) {
        err = krb5_init_context (&context);
        printiferr (err, "while initializing Kerberos 5");
    }
    
    if (!err && etypeString) {
        err = krb5_string_to_enctype (etypeString, &etype);
        printiferr (err, "while converting etype");
    }
    
    if (!err) {
        if (ccacheString) {
            err = krb5_cc_resolve (context, ccacheString, &ccache);
            printiferr (err, "while opening credentials cache '%s'", 
                        ccacheString);
        } else {
            err = krb5_cc_default (context, &ccache);
            printiferr (err, "while opening credentials cache");
        }
    }
    
    if (!err && keytabString) {
	err = krb5_kt_resolve(context, keytabString, &keytab);
        printiferr (err, "resolving keytab %s", keytabString);
    }

    if (!err) {
        err = krb5_cc_get_principal (context, ccache, &client);
        printiferr (err, "while getting client principal name");
    }
    
    for (i = 0; i < serviceNameCount; i++) {
        krb5_creds inCreds;
        krb5_creds *outCreds = NULL;
        krb5_principal server = NULL;
        krb5_ticket *ticket = NULL;
        char *principalString = NULL;
        
        if (!err) {
            err = krb5_parse_name (context, serviceNames[i], &server);
            printiferr (err, "while parsing principal name '%s'", serviceNames[i]);
        }
        
        if (!err) {
            err = krb5_unparse_name (context, server, &principalString);
            printiferr (err, "while getting string for principal name '%s'", serviceNames[i]);
        }
        
        if (!err) {
            memset (&inCreds, 0, sizeof (inCreds));
            inCreds.client = client;
            inCreds.server = server;
            inCreds.keyblock.enctype = etype;
        }
        
        if (!err) {
            krb5_creds oldCreds;
            int freeOldCreds = 0;
            int removedOldCreds = 0;
            
            /* Remove any existing service ticket in the cache so we get a new one */
            if (krb5_cc_retrieve_cred (context, ccache, 0, &inCreds, &oldCreds) == 0) {
                freeOldCreds = 1;
                if (krb5_cc_remove_cred (context, ccache, 0, &oldCreds) == 0) {
                    removedOldCreds = 1;
                }
            }
            
            err = krb5_get_credentials (context, 0, ccache, &inCreds, &outCreds);
            printiferr (err, "while getting credentials for '%s'", principalString);
            if (err && removedOldCreds) {
                /* Put back old credentials if there were some */
                krb5_cc_store_cred (context, ccache, &oldCreds);
            }
            
            if (freeOldCreds) { krb5_free_cred_contents (context, &oldCreds); }
        }
        
        if (!err) {
            /* we need a native ticket */
            err = krb5_decode_ticket (&outCreds->ticket, &ticket);
            printiferr (err, "while decoding ticket for '%s'", principalString);
        }
        
	if (!err) {
            if (keytab) {
                err = krb5_server_decrypt_ticket_keytab (context, keytab, ticket);
		if (!quiet) { 
                    printf ("%s: kvno = %d, keytab entry %s\n", principalString, 
                            ticket->enc_part.kvno, (err ? "invalid" : "valid")); 
                }
                printiferr (err, "while decrypting ticket for '%s'", principalString);
                
            } else if (!quiet) {
                printf ("%s: kvno = %d\n", principalString, ticket->enc_part.kvno); 
            }
        }
        
        if (err) {
            err = 0;  /* try next principal */
            errCount++;
        }
        
        if (server         ) { krb5_free_principal (context, server); }
        if (ticket         ) { krb5_free_ticket (context, ticket); }
        if (outCreds       ) { krb5_free_creds (context, outCreds); }
        if (principalString) { krb5_free_unparsed_name (context, principalString); }
    }
    
    if (client ) { krb5_free_principal (context, client); }
    if (keytab ) { krb5_kt_close (context, keytab); }
    if (ccache ) { krb5_cc_close (context, ccache); }
    if (context) { krb5_free_context (context); }
    
    return (err || errCount > 0) ? 1 : 0;
}

static int options (int argc, char * const * argv)
{
    int option;
    
    /* Get the arguments */
    while ((option = getopt (argc, argv, "c:e:k:hq4")) != -1) {
        switch (option) {
            case 'c':
                if (ccacheString) {
                    printerr ("Only one -c option allowed\n");
                    return usage ();
                }
                
                ccacheString = optarg; /* a pointer into argv */
                break;
            
            case 'e':
                if (etypeString) {
                    printerr ("Only one -e option allowed\n");
                    return usage ();
                }

                etypeString = optarg; /* a pointer into argv */
                break;

            case 'k':
                if (keytabString) {
                    printerr ("Only one -k option allowed\n");
                    return usage ();
                }
                
                keytabString = optarg; /* a pointer into argv */
                break;
                
            case 'q':
                quiet = 1;
                break;
                
            case '4':
                printerr ("Warning: -4 is no longer supported\n");
                return usage ();
                break;

            case 'h':
                return usage ();
                
            default:
                return usage ();
        }
    }

    /* look for extra arguments (other than the principal) */
    if (argc - optind <= 0) {
        printerr ("Please specify a service name\n");
        return usage ();
    }

    /* Remember where the service names start */
    serviceNameCount = argc - optind;
    serviceNames = &argv[optind];    
    
    return 0;
}


static int usage (void)
{
    fprintf (stderr, "Usage: %s [-h] | [-q] [-c ccache] [-e etype] [-k keytab]]"
             " service1 service2 ...\n", program);
    fprintf (stderr, "\t-h print usage message\n");
    fprintf (stderr, "\t-q quiet mode\n");
    fprintf (stderr, "\t-c <ccache> use credentials cache \"ccache\"\n");
    fprintf (stderr, "\t-e <etype> get tickets with enctype \"etype\"\n");
    fprintf (stderr, "\t-k <keytab> validate service tickets with \"keytab\"\n");
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

