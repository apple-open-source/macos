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
char *etypeString = NULL;
char * const *serviceNames = NULL;
int serviceNameCount = 0;
int quiet = 0;
int getKrb4 = false;

static int options (int argc, char * const * argv);
static int usage (void);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const * argv)
{
    int err = 0;
    int errCount = 0;
    int i;
    
    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* Read in our command line options */
    if (err == 0) {
        err = options (argc, argv);
    }

    if (getKrb4) {
        for (i = 0; i < serviceNameCount; i++) {
            char name[ANAME_SZ];
            char instance[INST_SZ];
            char realm[REALM_SZ];
            KTEXT_ST request;
            CREDENTIALS creds;

            if (err == 0) {
                // kname_parse doesn't always fill in the realm
                err = krb_get_lrealm (realm, 1);
                if (err) { printerr ("error looking up local realm: %s\n", krb_get_err_text (err)); }
            }

            if (err == 0) {
                err = kname_parse (name, instance, realm, serviceNames[i]);
                if (err) { printerr ("error parsing name '%s': %s\n",
                                     serviceNames[i], krb_get_err_text (err)); }
            }

            if (err == 0) {
                err = krb_mk_req (&request, name, instance, realm, 0);
                if (err) { printerr ("krb_mk_req error: %s\n", krb_get_err_text (err)); }
            }

            if (err == 0) {
                err = krb_get_cred (name, instance, realm, &creds);
                if (err) { printerr ("krb_get_cred error: %s\n", krb_get_err_text (err)); }
            }

            if (err == 0) {
                if (!quiet) {
                    printf ("%s: kvno = %d\n", serviceNames[i], creds.kvno);
                }
            } else {
                // try next principal
                err = 0;
                errCount++;
            }
        }
    } else {
        krb5_context context = NULL;
        krb5_principal client = NULL;
        krb5_ccache ccache = NULL;
        krb5_enctype etype = 0;

        if (err == 0) {
            err = krb5_init_context (&context);
            if (err) { com_err (program, err, "while initializing Kerberos 5"); }
        }
    
        if (etype != NULL) {
            if (err == 0) {
                err = krb5_string_to_enctype (etypeString, &etype);
                if (err) { com_err (program, err, "while converting etype"); }
            }
        }

        if (err == 0) {
            err = krb5_cc_default (context, &ccache);
            if (err) { com_err (program, err, "while opening credentials cache"); }
        }

        if (err == 0) {
            err = krb5_cc_get_principal (context, ccache, &client);
            if (err) { com_err (program, err, "while getting client principal name"); }
        }

        for (i = 0; i < serviceNameCount; i++) {
            krb5_creds in_creds;
            krb5_creds *out_creds = NULL;
            krb5_principal server = NULL;
            krb5_ticket *ticket = NULL;
            char *principalString = NULL;

            if (err == 0) {
                err = krb5_parse_name (context, serviceNames[i], &server);
                if (err) { printerr ("%s: %s while parsing principal name\n",
                                     serviceNames[i], error_message (err)); }
            }

            if (err == 0) {
                err = krb5_unparse_name (context, server, &principalString);
                if (err) { printerr ("%s: %s while printing principal name\n",
                                     serviceNames[i], error_message (err)); }
            }

            if (err == 0) {
                memset (&in_creds, 0, sizeof (in_creds));
                in_creds.client = client;
                in_creds.server = server;
                in_creds.keyblock.enctype = etype;
            }

            if (err == 0) {
                err = krb5_get_credentials (context, 0, ccache, &in_creds, &out_creds);
                if (err) { printerr ("%s: %s while getting credentials\n",
                                     principalString, error_message (err)); }
            }

            if (err == 0) {
                /* we need a native ticket */
                err = krb5_decode_ticket (&out_creds->ticket, &ticket);
                if (err) { printerr ("%s: %s while decoding ticket\n",
                                     principalString, error_message (err)); }
            }
            
            if (err == 0) {
                if (!quiet) {
                    printf ("%s: kvno = %d\n", principalString, ticket->enc_part.kvno);
                }
            } else {
                // try next principal
                err = 0;
                errCount++;
            }

            if (server          != NULL) { krb5_free_principal (context, server); }
            if (ticket          != NULL) { krb5_free_ticket (context, ticket); }
            if (out_creds       != NULL) { krb5_free_creds (context, out_creds); }
            if (principalString != NULL) { krb5_free_unparsed_name (context, principalString); }
            
        }

        if (client  != NULL) { krb5_free_principal (context, client); }
        if (ccache  != NULL) { krb5_cc_close (context, ccache); }
        if (context != NULL) { krb5_free_context (context); }
    }

    if (err != 0) { errCount++; }

    return (errCount > 0) ? 1 : 0;
}

static int options (int argc, char * const * argv)
{
    int option;
    
    /* Get the arguments */
    while ((option = getopt (argc, argv, "e:hq4")) != -1) {
        switch (option) {
            case 'e':
                if (getKrb4) {
                    fprintf (stderr, "Only one of -e and -4 allowed\n");
                    return usage ();
                }

                if (etypeString != NULL) {
                    fprintf (stderr, "Only one -e option allowed\n");
                    return usage ();
                }

                etypeString = optarg; /* a pointer into argv */
                break;

            case 'q':
                quiet = 1;
                break;
                
            case '4':
                if (etypeString != NULL) {
                    fprintf (stderr, "Only one of -e and -4 allowed\n");
                    return usage ();
                }

                getKrb4 = true;
                break;

            case 'h':
                return usage ();
                
            default:
                return usage ();
        }
    }

    /* look for extra arguments (other than the principal) */
    if (argc - optind <= 0) {
        fprintf (stderr, "Please specify a service name\n");
        return usage ();
    }

    /* Remember where the service names start */
    serviceNameCount = argc - optind;
    serviceNames = &argv[optind];    
    
    return 0;
}


static int usage (void)
{
    fprintf (stderr, "Usage: %s [-4 | -e etype] service1 service2 ...\n", program);
    fprintf (stderr, "\t-4 get Kerberos 4 tickets\n");
    fprintf (stderr, "\t-e <etype> get tickets with enctype \"etype\"\n");
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

