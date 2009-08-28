/*
 * klist.c
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
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/Kerberos.h>
#include <Kerberos/kim.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

enum {
    default_mode,
    ccache_mode,
    keytab_mode
};

time_t now = 0;

krb5_context kcontext = NULL;

int mode = default_mode;
int seen_ticket_mode = 0;

int show_enctypes = 0;
int show_all = 0;
int show_flags = 0;
int set_exit_status_only = 0;
int show_address_list = 0;
int no_reverse_resolve_addresses = 0;
int show_entry_timestamps = 0;
int show_entry_DES_keys = 0;

int print_keytab (const char *in_name);

int print_ccache (kim_ccache in_ccache, int *out_found_valid_tgt);
int print_ccaches (const char *in_name);

int options (int argc, char * const * argv, char **out_argument);
int usage (void);

void vprintmsg (const char *format, va_list args);
void printmsg (const char *format, ...);
void printiferr (errcode_t err, const char *format, ...);
void printerr (const char *format, ...);
void vprinterr (const char *format, va_list args);
void printfiller (char c, int count);
void printtime (time_t t);
void printflags (krb5_flags flags);
void printaddress (krb5_address address);

int get_timestamp_width (void);
char *enctype_to_string (krb5_enctype enctype);

#pragma mark -

/* ------------------------------------------------------------------------ */

int main (int argc, char * const * argv)
{
    int err = 0;
    char *argument = NULL;

    /* Remember our program name */
    setprogname (argv[0]);

    /* Remember the current time */
    now = time (NULL);

    /* Initialize the Kerberos 5 context */
    err = krb5_init_context (&kcontext);
    if (err) {
        com_err (getprogname (), err, "while initializing Kerberos 5");
        return 1;
    }

    
    /* Read in our command line options */
    err = options (argc, argv, &argument);
    if (err) {
        return 1;
    }
    
    switch (mode) {
        case keytab_mode:
            err = print_keytab (argument);
            break;
            
        case ccache_mode:
        case default_mode:
        default:
            err = print_ccaches (argument);
            break;
    }
    
    krb5_free_context (kcontext);
    return err;    
}

#pragma mark -

/* ------------------------------------------------------------------------ */

int print_keytab (const char *in_name)
{
    krb5_error_code err = 0;
    krb5_keytab kt;
    krb5_keytab_entry entry;
    krb5_kt_cursor cursor;
    char keytab_name[BUFSIZ]; /* hopefully large enough for any type */

    if (!err) {
        if (!in_name) {
            err = krb5_kt_default (kcontext, &kt);
            printiferr (err, "while resolving default keytab");
        } else {
            err = krb5_kt_resolve (kcontext, in_name, &kt);
            printiferr (err, "while resolving keytab %s", in_name);
        }
    }

    if (!err) {
        err = krb5_kt_get_name (kcontext, kt, keytab_name, sizeof (keytab_name));
        printiferr (err, "while getting keytab name");
    }

    if (!err) {
        printmsg ("Keytab name: %s\n", keytab_name);
    }

    if (!err) {
        err = krb5_kt_start_seq_get (kcontext, kt, &cursor);
        printiferr (err, "while starting scan of keytab %s", keytab_name);
    }

    if (!err) {
        if (show_entry_timestamps) {
            printmsg ("KVNO Timestamp");
            printfiller (' ', get_timestamp_width () - sizeof ("Timestamp") + 2);
            printmsg ("Principal\n");
            printmsg ("---- ");
            printfiller ('-', get_timestamp_width ());
            printmsg (" ");
            printfiller ('-', 78 - get_timestamp_width () - sizeof ("KVNO"));
            printmsg ("\n");
        } else {
            printmsg("KVNO Principal\n");
            printmsg("---- --------------------------------------------------------------------------\n");
        }
    }

    while (!err) {
        char *principal_name = NULL;

        err = krb5_kt_next_entry (kcontext, kt, &entry, &cursor);
	if (err == KRB5_KT_END) {
	    err = 0;
	    break;
	}
 
        if (!err) {
            err = krb5_unparse_name (kcontext, entry.principal, &principal_name);
            printiferr (err, "while unparsing principal name");
        }
        
        if (!err) {
            printmsg ("%4d ", entry.vno);
            if (show_entry_timestamps) {
                printtime (entry.timestamp);
                printmsg (" ");
            }
            printmsg ("%s", principal_name);
            if (show_enctypes) {
                printmsg (" (%s) ", enctype_to_string (entry.key.enctype));
            }
            if (show_entry_DES_keys) {
                unsigned int i;
                
                printmsg (" (0x");
                for (i = 0; i < entry.key.length; i++) {
                    printmsg ("%02x", entry.key.contents[i]);
                }
                printmsg (")");
            }
            printmsg ("\n");
        }
	printiferr (err, "while scanning keytab %s", keytab_name);

        if (principal_name) { krb5_free_unparsed_name (kcontext, principal_name); }
    }

    if (!err) {
        err = krb5_kt_end_seq_get (kcontext, kt, &cursor);
        printiferr (err, "while ending scan of keytab %s", keytab_name);
    }

    return err ? 1 : 0;
}

/* ------------------------------------------------------------------------ */

int print_ccache (kim_ccache in_ccache, int *out_found_valid_tgt)
{
    kim_error err = 0;
    kim_credential_iterator iterator = NULL;
    kim_identity ccache_identity = NULL;
    kim_string type = NULL;
    kim_string name = NULL;
    kim_string ccache_identity_string = NULL;
    int found_tickets = 0;

    *out_found_valid_tgt = 0;
    
    if (!err) {
        err = kim_ccache_get_type (in_ccache, &type);
        printiferr (err, "while getting the ccache type");
    }
    
    if (!err) {
        err = kim_ccache_get_name (in_ccache, &name);
        printiferr (err, "while getting the ccache name");
    }
    
    if (!err) {
        err = kim_ccache_get_client_identity (in_ccache, &ccache_identity);
        printiferr (err, "while getting the ccache principal");
   }
    
    if (!err) {
        err = kim_identity_get_display_string (ccache_identity, 
                                               &ccache_identity_string);
        printiferr (err, "while unparsing the ccache principal name");
    }
    
    if (!err) {
        printmsg ("Kerberos 5 ticket cache: '%s:%s'\nDefault principal: %s\n\n",
                  type, name, ccache_identity_string);
        printmsg ("Valid Starting");
        printfiller (' ', get_timestamp_width () - sizeof ("Valid Starting") + 3);
        printmsg ("Expires");
        printfiller (' ', get_timestamp_width () - sizeof ("Expires") + 3);
        printmsg ("Service Principal\n");
        
        err = kim_credential_iterator_create (&iterator, in_ccache);
    }
    
    while (!err) {
        kim_credential credential = NULL;
        kim_identity client = NULL;
        kim_identity service = NULL;
        kim_string client_string = NULL;
        kim_string service_string = NULL;
        krb5_creds *creds = NULL;
        int extra_field = 0;
        
        err = kim_credential_iterator_next (iterator, &credential);
        if (!err && !credential) { break; }
        
        if (!err) {
            err = kim_credential_get_client_identity (credential, &client);
            printiferr (err, "while getting the client principal");
        }

        if (!err) {
            err = kim_identity_get_display_string (client, &client_string);
            printiferr (err, "while unparsing the client principal name");
        }
        
        if (!err) {
            err = kim_credential_get_service_identity (credential, &service);
            printiferr (err, "while getting the service principal");
        }
        
        if (!err) {
            err = kim_identity_get_display_string (service, &service_string);
            printiferr (err, "while unparsing the service principal name");
        }
        
        if (!err) {
            err = kim_credential_get_krb5_creds (credential, kcontext, &creds);
            printiferr (err, "while getting krb5 creds");
        }
                
	if (!err && krb5_is_config_principal(kcontext, creds->server))
	    goto next;

        if (!err) {
            found_tickets = 1;
            
            printtime (creds->times.starttime ? creds->times.starttime : creds->times.authtime);
            printmsg ("  ");
            printtime (creds->times.endtime);
            printmsg ("  ");
            printmsg ("%s\n", service_string);
            
            if (strcmp (ccache_identity_string, client_string)) {
                if (!extra_field) {
                    printmsg ("\t");
                }
                printmsg ("for client %s", client_string);
                extra_field++;
            }
            
            if (creds->ticket_flags & TKT_FLG_RENEWABLE) {
                printmsg (extra_field ? ", " : "\t");
                printmsg ("renew until ");
                printtime (creds->times.renew_till);
                extra_field += 2;
            }
            
            if (extra_field > 2) {
                printmsg ("\n");
                extra_field = 0;
            }
            
            if (show_flags) {
                printmsg (extra_field ? ", " : "\t");
                printflags (creds->ticket_flags);
                extra_field++;
            }
            
            if (extra_field > 2) {
                printmsg ("\n");
                extra_field = 0;
            }
            
            if (show_enctypes) {
                krb5_ticket *ticket_rep;
                
                if (krb5_decode_ticket (&creds->ticket, &ticket_rep) == 0) {
                    if (!extra_field) {
                        printmsg ("\t");
                    } else {
                        printmsg (", ");
                    }
                    printmsg ("Etype (skey, tkt): %s, ", 
                              enctype_to_string (creds->keyblock.enctype));
                    printmsg ("%s ", 
                              enctype_to_string (ticket_rep->enc_part.enctype));
                    extra_field++;
                    
                    krb5_free_ticket (kcontext, ticket_rep);
                }
            }
            
            if (extra_field) {
                printmsg ("\n");
            }
            
            if (show_address_list) {
                printmsg ("\tAddresses: ");
                if (!creds->addresses || !creds->addresses[0]) {
                    printmsg ("(none)\n");
                } else {
                    int i;
                    
                    for (i = 0; creds->addresses[i]; i++) {
                        if (i > 0) {
                            printmsg (", ");
                        }
                        printaddress (*creds->addresses[i]);
                    }
                    printmsg ("\n");
                }
            }
            if (extra_field) {
                printmsg ("\n");
            }
            
        }
        
        if (!err) {
            kim_boolean is_tgt = 0;
            kim_credential_state state;
            
            err = kim_credential_is_tgt (credential, &is_tgt);
            printiferr (err, "while checking if creds are valid");
           
            if (!err) {
                err = kim_credential_get_state (credential, &state);
            }
            
            if (!err && is_tgt && state == kim_credentials_state_valid) { 
                *out_found_valid_tgt = 1;
            }
        }
        
    next:
        if (creds) { krb5_free_creds (kcontext, creds); }
        kim_string_free (&client_string);
        kim_string_free (&service_string);
        kim_identity_free (&client);
        kim_identity_free (&service);
        kim_credential_free (&credential);
    }
        
    kim_string_free (&type);
    kim_string_free (&name);
    kim_string_free (&ccache_identity_string);
    kim_identity_free (&ccache_identity);
    kim_credential_iterator_free (&iterator);
    
    if (!err) {
        if (!found_tickets) {
            printerr ("No Kerberos 5 tickets in credentials cache\n");
        } else {
            printmsg ("\n");
        }
    }

    return err;
}

/* ------------------------------------------------------------------------ */

int print_ccaches (const char *in_name)
{
    kim_error err = 0;
    kim_ccache primary_cache = NULL;
    int found_tgt = 0;
    
    /* Start by printing out the main ccache... either the default or inName */
    if (!err) {
        if (in_name) {
            err = kim_ccache_create_from_display_name (&primary_cache, in_name);
            printiferr (err, "while locating credentials cache '%s'", in_name);
        } else {
            err = kim_ccache_create_from_default (&primary_cache);
            printiferr (err, "while locating the default credentials cache");
        }
    }
    
    /* print the v5 tickets in mainCCache */
    if (!err) {
        int found = 0;
        err = print_ccache (primary_cache, &found);
        if (!err) { found_tgt += found; }
    }
        
    if (show_all) {
        kim_ccache_iterator iterator = NULL;

        if (!err) {
            err = kim_ccache_iterator_create (&iterator);
        }
        
        while (!err) {
            kim_ccache ccache = NULL;
            kim_comparison comparison;
            
            err = kim_ccache_iterator_next (iterator, &ccache);
            if (!err && !ccache) { break; }
            
            if (!err) {
                err = kim_ccache_compare (ccache, primary_cache, &comparison);
            }
                        
            /* if we haven't printed this ccache already, print it */
            if (!err && !kim_comparison_is_equal_to (comparison)) {
                int found;
                
                /* Print spacing between caches in the list */
                printfiller ('-', 79);
                printmsg ("\n");

                err = print_ccache (ccache, &found);
                if (!err) { found_tgt += found; }
            }

            kim_ccache_free (&ccache);
        }
        
        kim_ccache_iterator_free (&iterator);
    }

    kim_ccache_free (&primary_cache);
    
    return (err || !found_tgt) ? 1 : 0;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

int options (int argc, char * const * argv, char **out_argument)
{
    int option;
 
    /* Get the arguments */
    while ((option = getopt (argc, argv, "eck54AfsantK")) != -1) {
        switch (option) {
            case 'e':
                show_enctypes = 1;
                break;
            
            case 'c':
                if (mode != default_mode) {
                    printerr ("Only one -c or -k allowed\n");
                    return usage ();
                }
                mode = ccache_mode;
                break;

            case 'k':
                if (mode != default_mode) {
                    printerr ("Only one -c or -k allowed\n");
                    return usage ();
                }
                mode = keytab_mode;
                break;
            
            case '5':
                // This is always on now
                break;

            case '4':
                printerr ("Warning: -4 option is no longer supported\n");
                break;

            case 'A':
                show_all = 1;
                break;

            case 'f':
                show_flags = 1;
                break;

            case 's':
                set_exit_status_only = 1;
                break;

            case 'a':
                show_address_list = 1;
                break;

             case 'n':
                no_reverse_resolve_addresses = 1;
                break;

            case 't':
                show_entry_timestamps = 1;
                break;

            case 'K':
                show_entry_DES_keys = 1;
                break;

            default:
                return usage ();
        }
    }
    
    if (no_reverse_resolve_addresses && !show_address_list) {
        printerr ("-n option requires -a option\n");
        return usage ();
    }
    
    switch (mode) {
        case keytab_mode:
            if (show_all || show_flags || set_exit_status_only || show_address_list) {
                printerr ("-A, -f, -s, -a, and -n options cannot be used with -k\n");
                return usage ();
            }
            break;
        
        case default_mode:
        case ccache_mode:
        default:
             if (show_entry_timestamps || show_entry_DES_keys) {
                printerr ("-t and -K options require -k\n");
                return usage ();
            }
            break;           
    }
    
    /* look for extra arguments (other than the principal) */
    if (argc - optind > 1) {
        printerr ("Unknown option '%s'\n", argv[optind + 1]);
        return usage ();
    }
    
    /* try to get the cache name from the argument list */
    *out_argument = (optind == argc - 1) ? argv[optind] : NULL;
    
    return 0;
}

/* ------------------------------------------------------------------------ */

int usage (void)
{
    fprintf (stderr, "Usage: %s [-e] [[-c] [-A] [-f] [-s] [-a [-n]]] [-k [-t] [-K]] [name]\n", 
        getprogname ());

    fprintf (stderr, "\t-e show the encryption type of the ticket or entry\n");
    fprintf (stderr, "\t-c show tickets in credentials cache (default)\n");
    fprintf (stderr, "\tOptions for credentials caches:\n");
    fprintf (stderr, "\t\t-A show all ticket caches\n");
    fprintf (stderr, "\t\t-f show ticket flags\n");
    fprintf (stderr, "\t\t-s set exit status based on valid TGT existence\n");
    fprintf (stderr, "\t\t-a display address lists for tickets\n");
    fprintf (stderr, "\t\t\t-n do not reverse-resolve addresses\n");
    
    fprintf (stderr, "\t-k show principals in keytab\n");
    fprintf (stderr, "\tOptions for keytabs:\n");
    fprintf (stderr, "\t\t-t show keytab entry timestamps\n");
    fprintf (stderr, "\t\t-K show keytab entry DES keys\n");
    return 2;
}

/* ------------------------------------------------------------------------ */

void printiferr (errcode_t err, const char *format, ...)
{
    if (err && err != KRB5_CC_END) {
        va_list pvar;

        va_start (pvar, format);
        com_err_va (getprogname (), err, format, pvar);
        va_end (pvar);
    }
}

/* ------------------------------------------------------------------------ */

void printerr (const char *format, ...)
{
    va_list pvar;

    va_start (pvar, format);
    vprinterr (format, pvar);
    va_end (pvar);
}

/* ------------------------------------------------------------------------ */

void vprinterr (const char *format, va_list args)
{
    if (!set_exit_status_only) {
        fprintf (stderr, "%s: ", getprogname ());
        vfprintf (stderr, format, args);
    }
}

/* ------------------------------------------------------------------------ */

void printmsg (const char *format, ...)
{
    va_list pvar;

    va_start (pvar, format);
    vprintmsg (format, pvar);
    va_end (pvar);
}

/* ------------------------------------------------------------------------ */

void vprintmsg (const char *format, va_list args)
{
    if (!set_exit_status_only) {
        vfprintf (stdout, format, args);
    }
}

/* ------------------------------------------------------------------------ */

void printfiller (char c, int count)
{
    int i;

    for (i = 0; i < count; i++)
        printmsg("%c", c);
}

/* ------------------------------------------------------------------------ */

void printtime (time_t t)
{
    char string[BUFSIZ];
    char filler = ' ';
    
    if (!krb5_timestamp_to_sfstring((krb5_timestamp) t, string, 
                                    get_timestamp_width() + 1, &filler)) {
        printmsg ("%s", string);
    }
}

/* ------------------------------------------------------------------------ */

void printflags (krb5_flags flags) 
{
    char flagsString[32];
    int i = 0;
	
    if (flags & TKT_FLG_FORWARDABLE)            flagsString[i++] = 'F';
    if (flags & TKT_FLG_FORWARDED)              flagsString[i++] = 'f';
    if (flags & TKT_FLG_PROXIABLE)              flagsString[i++] = 'P';
    if (flags & TKT_FLG_PROXY)                  flagsString[i++] = 'p';
    if (flags & TKT_FLG_MAY_POSTDATE)           flagsString[i++] = 'D';
    if (flags & TKT_FLG_POSTDATED)              flagsString[i++] = 'd';
    if (flags & TKT_FLG_INVALID)                flagsString[i++] = 'i';
    if (flags & TKT_FLG_RENEWABLE)              flagsString[i++] = 'R';
    if (flags & TKT_FLG_INITIAL)                flagsString[i++] = 'I';
    if (flags & TKT_FLG_HW_AUTH)                flagsString[i++] = 'H';
    if (flags & TKT_FLG_PRE_AUTH)               flagsString[i++] = 'A';
    if (flags & TKT_FLG_TRANSIT_POLICY_CHECKED) flagsString[i++] = 'T';
    if (flags & TKT_FLG_OK_AS_DELEGATE)         flagsString[i++] = 'O'; /* D/d taken. */
    if (flags & TKT_FLG_ANONYMOUS)              flagsString[i++] = 'a';
    flagsString[i] = '\0';
    printmsg ("%s", flagsString);
}

/* ------------------------------------------------------------------------ */

void printaddress (krb5_address address)
{
    int             af;
    char            buf[46];
    const char     *addr_string = NULL;
    
    
    switch (address.addrtype) {
        case ADDRTYPE_INET:
            af = AF_INET;
            break;
            
        case ADDRTYPE_INET6:
            af = AF_INET6;
            break;
            
        default:
             printmsg ("unknown address type %d", address.addrtype);
             return;
    }
    
    if (!no_reverse_resolve_addresses) {
        struct hostent *h = NULL;
        int err; 
        
        h = getipnodebyaddr (address.contents, address.length, af, &err);
        if (h != NULL) {
            printmsg ("%s", h->h_name);
            freehostent (h);
            return;
        }
    }
    
    /* either we aren't resolving addresses or we failed to do so */
    addr_string = inet_ntop(af, address.contents, buf, sizeof(buf));
    if (addr_string != NULL) {
        printmsg ("%s", addr_string);
    }
}

#pragma mark -

/* ------------------------------------------------------------------------ */

int get_timestamp_width (void)
{
    static int width = 0;
    
    if (width == 0) {
        char time_string[BUFSIZ];
        
        if (!krb5_timestamp_to_sfstring (now, time_string, 20, NULL) ||
            !krb5_timestamp_to_sfstring (now, time_string, sizeof (time_string), NULL)) {
            width = strlen (time_string);
        } else {
            width = 15;
        }
    }
    
    return width;
}

/* ------------------------------------------------------------------------ */

char *enctype_to_string (krb5_enctype enctype)
{
    static char enctypeString[100];

    if (krb5_enctype_to_string (enctype, enctypeString, sizeof(enctypeString)) != 0) {
        sprintf (enctypeString, "etype %d", enctype); /* default if error */
    }

    return enctypeString;
}
