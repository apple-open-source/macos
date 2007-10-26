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
 *
 * Destroy the contents of your credential cache.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/Kerberos.h>
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

const char *program = NULL;

enum {
    defaultMode,
    ccacheMode,
    keytabMode
};

time_t now = 0;

krb5_context kcontext = NULL;

int mode = defaultMode;
int seenTicketMode = 0;

int useCCAPI = 1;

const char *name = NULL;

int showEnctypes = 0;
int seenShowEnctypes = 0;

int showAll = 0;
int seenShowAll = 0;

int showFlags = 0;
int seenShowFlags = 0;

int setExitStatusOnly = 0;
int seenSetExitStatusOnly = 0;

int showAddressList = 0;
int seenShowAddressList = 0;

int noReverseResolveAddresses = 0;
int seenNoReverseResolveAddresses = 0;

int showEntryTimestamps = 0;
int seenShowEntryTimestamps = 0;

int showEntryDESKeys = 0;
int seenShowEntryDESKeys = 0;

static int printkeytab (void);

static int printv5cache (krb5_ccache inCCache, int *outFoundTickets);
static int printccache (const char *inName);

static int options (int argc, char * const * argv);
static int usage (void);

static void vprintmsg (const char *format, va_list args);
static void printmsg (const char *format, ...);
static void printiferr (errcode_t err, const char *format, ...);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);
static void printfiller (char c, int count);
static void printtime (time_t t);
static void printflags (krb5_flags flags);
static void printaddress (krb5_address address);

static int get_timestamp_width (void);
static char *enctype_to_string (krb5_enctype enctype);

#pragma mark -

int main (int argc, char * const * argv)
{
    int err = 0;

    /* Initialize the Kerberos 5 context */
    err = krb5_init_context (&kcontext);
    if (err) {
        com_err (program, err, "while initializing Kerberos 5");
        return 1;
    }
    
    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    /* Remember the current time */
    now = time (NULL);
    
    /* Read in our command line options */
    err = options (argc, argv);
    if (err) {
        return 1;
    }
    
    switch (mode) {
        case keytabMode:
            err = printkeytab ();
            break;
            
        case ccacheMode:
        case defaultMode:
        default:
            err = printccache (name);
            break;
    }
    
    krb5_free_context (kcontext);
    return err;    
}

#pragma mark -

static int printkeytab (void)
{
    krb5_error_code err = 0;
    krb5_keytab kt;
    krb5_keytab_entry entry;
    krb5_kt_cursor cursor;
    char keytabName[BUFSIZ]; /* hopefully large enough for any type */

    if (!err) {
        if (name == NULL) {
            err = krb5_kt_default (kcontext, &kt);
            printiferr (err, "while resolving default keytab");
        } else {
            err = krb5_kt_resolve (kcontext, name, &kt);
            printiferr (err, "while resolving keytab %s", name);
        }
    }

    if (!err) {
        err = krb5_kt_get_name (kcontext, kt, keytabName, sizeof (keytabName));
        printiferr (err, "while getting keytab name");
    }

    if (!err) {
        printmsg ("Keytab name: %s\n", keytabName);
    }

    if (!err) {
        err = krb5_kt_start_seq_get (kcontext, kt, &cursor);
        printiferr (err, "while starting scan of keytab %s", name);
    }

    if (!err) {
        if (showEntryTimestamps) {
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

    if (!err) {
        while ((err = krb5_kt_next_entry (kcontext, kt, &entry, &cursor)) == 0) {
            char *principalName = NULL;

            if (!err) {
                err = krb5_unparse_name (kcontext, entry.principal, &principalName);
                printiferr (err, "while unparsing principal name");
            }

            if (!err) {
                printmsg ("%4d ", entry.vno);
                if (showEntryTimestamps) {
                    printtime (entry.timestamp);
                    printmsg (" ");
                }
                printmsg ("%s", principalName);
                if (showEnctypes) {
                    printmsg (" (%s) ", enctype_to_string (entry.key.enctype));
                }
                if (showEntryDESKeys) {
                    unsigned int i;

                    printmsg (" (0x");
                    for (i = 0; i < entry.key.length; i++) {
                        printmsg ("%02x", entry.key.contents[i]);
                    }
                    printmsg (")");
                }
                printmsg ("\n");
            }
            if (principalName != NULL) { krb5_free_unparsed_name (kcontext, principalName); }
        }
        if (err == KRB5_KT_END) { err = 0; }
        printiferr (err, "while scanning keytab %s", name);
    }

    if (!err) {
        err = krb5_kt_end_seq_get (kcontext, kt, &cursor);
        printiferr (err, "while ending scan of keytab %s", name);
    }

    return err ? 1 : 0;
}

static int printv5cache (krb5_ccache inCCache, int *outFoundTickets)
{
    krb5_error_code  err = 0;
    krb5_cc_cursor   cursor = NULL;
    krb5_principal   principal = NULL;
    char            *principalName = NULL;
    krb5_flags       flags = 0;
    int              resetFlags = 0;
    int              foundV5Tickets = 0;

    *outFoundTickets = 0;
    
    if (!err) {
        err = krb5_cc_set_flags (kcontext, inCCache, flags); /* turn off OPENCLOSE */
        if (!err) { resetFlags = 1; }
        if (err == KRB5_FCC_NOFILE) { err = 0; }  /*  */
        printiferr (err, "while setting cache flags (ticket cache %s:%s)",
                    krb5_cc_get_type (kcontext, inCCache),
                    krb5_cc_get_name (kcontext, inCCache));
    }

    if (!err && (krb5_cc_get_principal (kcontext, inCCache, &principal) == 0)) {
        err = krb5_unparse_name (kcontext, principal, &principalName);
        printiferr (err, "while unparsing principal name");

        if (!err) {
            printmsg ("Kerberos 5 ticket cache: '%s:%s'\nDefault principal: %s\n\n",
                      krb5_cc_get_type (kcontext, inCCache),
                      krb5_cc_get_name (kcontext, inCCache), principalName);
            printmsg ("Valid Starting");
            printfiller (' ', get_timestamp_width () - sizeof ("Valid Starting") + 3);
            printmsg ("Expires");
            printfiller (' ', get_timestamp_width () - sizeof ("Expires") + 3);
            printmsg ("Service Principal\n");
        }
        
        if (!err) {
            err = krb5_cc_start_seq_get (kcontext, inCCache, &cursor);
            printiferr (err, "while starting to retrieve tickets");
        }
        
        while (!err) {
            krb5_creds  creds;
            int         freeCreds = 0;
            char       *clientName = NULL;
            char       *serverName = NULL;
            int         extraField = 0;
            
            err = krb5_cc_next_cred (kcontext, inCCache, &cursor, &creds);
            if (!err) { freeCreds = 1; }
            printiferr (err, "while retrieving a ticket");
            
            if (!err) {
                err = krb5_unparse_name (kcontext, creds.client, &clientName);
                printiferr (err, "while unparsing client name");
            }
            
            if (!err) {
                err = krb5_unparse_name (kcontext, creds.server, &serverName);
                printiferr (err, "while unparsing server name");
            }
            
            if (!err) {
                foundV5Tickets = 1;
                
                printtime (creds.times.starttime ? creds.times.starttime : creds.times.authtime);
                printmsg ("  ");
                printtime (creds.times.endtime);
                printmsg ("  ");
                printmsg ("%s\n", serverName);
                
                if (strcmp (principalName, clientName) != 0) {
                    if (!extraField) {
                        printmsg ("\t");
                    }
                    printmsg ("for client %s", clientName);
                    extraField++;
                }
                
                if (creds.ticket_flags & TKT_FLG_RENEWABLE) {
                    if (!extraField) {
                        printmsg ("\t");
                    } else {
                        printmsg (", ");
                    }
                    printmsg ("renew until ");
                    printtime (creds.times.renew_till);
                    extraField += 2;
                }
                
                if (extraField > 2) {
                    printmsg ("\n");
                    extraField = 0;
                }
                
                if (showFlags) {
                    if (!extraField) {
                        printmsg ("\t");
                    } else {
                        printmsg (", ");
                    }
                    printflags (creds.ticket_flags);
                    extraField++;
                }
                
                if (extraField > 2) {
                    printmsg ("\n");
                    extraField = 0;
                }
                
                if (showEnctypes) {
                    krb5_ticket *ticket_rep;
                    
                    if (krb5_decode_ticket (&creds.ticket, &ticket_rep) == 0) {
                        if (!extraField) {
                            printmsg ("\t");
                        } else {
                            printmsg (", ");
                        }
                        printmsg ("Etype (skey, tkt): %s, ", enctype_to_string (creds.keyblock.enctype));
                        printmsg ("%s ", enctype_to_string (ticket_rep->enc_part.enctype));
                        extraField++;
                        
                        krb5_free_ticket (kcontext, ticket_rep);
                    }
                }
                
                if (extraField) {
                    printmsg ("\n");
                }
                
                if (showAddressList) {
                    printmsg ("\tAddresses: ");
                    if (creds.addresses == NULL || creds.addresses[0] == NULL) {
                        printmsg ("(none)\n");
                    } else {
                        int i;
                        
                        for (i = 0; creds.addresses[i]; i++) {
                            if (i > 0) {
                                printmsg (", ");
                            }
                            printaddress (*creds.addresses[i]);
                        }
                        printmsg ("\n");
                    }
                }
            }
            
            if (!err) {
                krb5_data *pname = krb5_princ_name (kcontext, creds.server);
                
                if ((krb5_princ_size (kcontext, creds.server) == 2) &&   // one name and one instance
                    (strlen (KRB5_TGS_NAME) == pname->length) &&         // name is "krbtgt"
                    (strncmp (pname->data, KRB5_TGS_NAME, pname->length) == 0) && 
                    creds.times.endtime > now) {                         // ticket is valid
                    *outFoundTickets = 1; /* valid tgt */
                }
            }
            
            if (clientName != NULL) { krb5_free_unparsed_name (kcontext, clientName); }
            if (serverName != NULL) { krb5_free_unparsed_name (kcontext, serverName); }
            if (freeCreds         ) { krb5_free_cred_contents (kcontext, &creds); }
        }
        if (err == KRB5_CC_END) { err = 0; }
        
        if (!err) {
            err = krb5_cc_end_seq_get (kcontext, inCCache, &cursor);
            printiferr (err, "while finishing ticket retrieval");
        }
    }
    
    if (resetFlags) {
        flags = KRB5_TC_OPENCLOSE; /* restore OPENCLOSE mode */
        err = krb5_cc_set_flags (kcontext, inCCache, flags);
        printiferr (err, "while finishing ticket retrieval");
    }

    if (!err) {
        if (!foundV5Tickets) {
            printerr ("No Kerberos 5 tickets in credentials cache\n");
        } else {
            printmsg ("\n");
        }
    }

    return err;
}

static int printccache (const char *inName)
{
    krb5_error_code      err = 0;
    krb5_ccache          mainCCache = NULL;
    int                  foundTGT = 0;
    
    /* Start by printing out the main ccache... either the default or inName */
    if (!err) {
        if (inName != NULL) {
            err = krb5_cc_resolve (kcontext, inName, &mainCCache);
            printiferr (err, "while locating credentials cache '%s'", name);
        } else {
            err = krb5_cc_default (kcontext, &mainCCache);
            printiferr (err, "while locating the default credentials cache");
        }
    }
    
    /* print the v5 tickets in mainCCache */
    if (!err) {
        int found = 0;
        err = printv5cache (mainCCache, &found);
        if (!err) { foundTGT += found; }
    }
        
    if (showAll) {
        cc_context_t         cc_context = NULL;
        cc_ccache_iterator_t iterator = NULL;
        
        /* Initialize the CCAPI */
        if (!err) {
            err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
            printiferr (err, "while initializing credentials cache");
        }
        
        if (!err) {
            err = cc_context_new_ccache_iterator (cc_context, &iterator);
            printiferr (err, "while starting to iterate over credentials caches");
        }
        
        while (!err) {
            cc_ccache_t   cc_ccache = NULL;
            cc_string_t   ccacheName = NULL;
            int           found = 0;
            
            err = cc_ccache_iterator_next (iterator, &cc_ccache);
            printiferr (err, "while iterating over credentials caches");
            
            if (!err) {
                err = cc_ccache_get_name (cc_ccache, &ccacheName);
                printiferr (err, "while getting credentials cache name");
            }
            
            /* if we haven't printed this ccache already, print it */
            if (!err && 
                (strcmp (krb5_cc_get_type (kcontext, mainCCache), "API") ||
                 strcmp (krb5_cc_get_name (kcontext, mainCCache), ccacheName->data))) {
                /* Print spacing between caches in the list */
                printfiller ('-', 79);
                printmsg ("\n");
            
                if (!err) {
                    krb5_ccache ccache = NULL;

                    err = krb5_cc_resolve (kcontext, ccacheName->data, &ccache);
                    
                    if (!err) {
                        err = printv5cache (ccache, &found);
                        if (!err) { foundTGT += found; }
                    }
                    
                    if (ccache != NULL) { krb5_cc_close (kcontext, ccache); }                
                }
            }

            if (ccacheName != NULL) { cc_string_release (ccacheName); }
            if (cc_ccache  != NULL) { cc_ccache_release (cc_ccache); }
        }
        if (err == ccIteratorEnd) { err = ccNoError; }
        
        if (iterator   != NULL) { cc_ccache_iterator_release (iterator); }
        if (cc_context != NULL) { cc_context_release (cc_context); }
    }

    if (mainCCache != NULL) { krb5_cc_close (kcontext, mainCCache); }
    
    return (err || !foundTGT) ? 1 : 0;
}

#pragma mark -

static int options (int argc, char * const * argv)
{
    int option;
 
    /* Get the arguments */
    while ((option = getopt (argc, argv, "eck54AfsantK")) != -1) {
        switch (option) {
            case 'e':
                 if (seenShowEnctypes) {
                    printerr ("Only one -e option allowed\n");
                    return usage ();
                }
                showEnctypes = 1;
                seenShowEnctypes = 1;
                break;
            
            case 'c':
                if (mode != defaultMode) {
                    printerr ("Only one -c or -k allowed\n");
                    return usage ();
                }
                mode = ccacheMode;
                break;

            case 'k':
                if (mode != defaultMode) {
                    printerr ("Only one -c or -k allowed\n");
                    return usage ();
                }
                mode = keytabMode;
                break;
            
            case '5':
                // This is always on now
                break;

            case '4':
                printerr ("Warning: -4 option is no longer supported\n");
                break;

            case 'A':
                if (seenShowAll) {
                    printerr ("Only one -A option allowed\n");
                    return usage ();
                }
                showAll = 1;
                seenShowAll = 1;
                break;

            case 'f':
                if (seenShowFlags) {
                    printerr ("Only one -f option allowed\n");
                    return usage ();
                }
                showFlags = 1;
                seenShowFlags = 1;
                break;

            case 's':
                if (seenSetExitStatusOnly) {
                    printerr ("Only one -s option allowed\n");
                    return usage ();
                }
                setExitStatusOnly = 1;
                seenSetExitStatusOnly = 1;
                break;

            case 'a':
                if (seenShowAddressList) {
                    printerr ("Only one -a option allowed\n");
                    return usage ();
                }
                showAddressList = 1;
                seenShowAddressList = 1;
                break;

             case 'n':
                if (seenNoReverseResolveAddresses) {
                    printerr ("Only one -n option allowed\n");
                    return usage ();
                }
                noReverseResolveAddresses = 1;
                seenNoReverseResolveAddresses = 1;
                break;

            case 't':
                if (seenShowEntryTimestamps) {
                    printerr ("Only one -t option allowed\n");
                    return usage ();
                }
                showEntryTimestamps = 1;
                seenShowEntryTimestamps = 1;
                break;

            case 'K':
                if (seenShowEntryDESKeys) {
                    printerr ("Only one -K option allowed\n");
                    return usage ();
                }
                showEntryDESKeys = 1;
                seenShowEntryDESKeys = 1;
                break;

            default:
                return usage ();
        }
    }
    
    if (seenNoReverseResolveAddresses && !seenShowAddressList) {
        printerr ("-n option requires -a option\n");
        return usage ();
    }
    
    switch (mode) {
        case keytabMode:
            if (seenShowAll || seenShowFlags || seenSetExitStatusOnly || seenShowAddressList) {
                printerr ("-A, -f, -s, -a, and -n options cannot be used with -k\n");
                return usage ();
            }
            break;
        
        case defaultMode:
        case ccacheMode:
        default:
             if (seenShowEntryTimestamps || seenShowEntryDESKeys) {
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
    
    /* try to get the principal from the argument list */
    name = (optind == argc - 1) ? argv[optind] : NULL;
    
    return 0;
}

static int usage (void)
{
    fprintf (stderr, "Usage: %s [-e] [[-c] [-A] [-f] [-s] [-a [-n]]] [-k [-t] [-K]] [name]\n", 
        program);

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
    if (!setExitStatusOnly) {
        fprintf (stderr, "%s: ", program);
        vfprintf (stderr, format, args);
    }
}

static void printmsg (const char *format, ...)
{
    va_list pvar;

    va_start (pvar, format);
    vprintmsg (format, pvar);
    va_end (pvar);
}

static void vprintmsg (const char *format, va_list args)
{
    if (!setExitStatusOnly) {
        vfprintf (stdout, format, args);
    }
}

static void printfiller (char c, int count)
{
    int i;

    for (i = 0; i < count; i++)
        printmsg("%c", c);
}

static void printtime (time_t t)
{
    char string[BUFSIZ];
    char filler = ' ';
    
    if (!krb5_timestamp_to_sfstring((krb5_timestamp) t, string, 
                                    get_timestamp_width() + 1, &filler)) {
        printmsg ("%s", string);
    }
}

static void printflags (krb5_flags flags) 
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

static void printaddress (krb5_address address)
{
    int             af;
    char            buf[46];
    const char     *addrString = NULL;
    
    
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
    
    if (!noReverseResolveAddresses) {
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
    addrString = inet_ntop(af, address.contents, buf, sizeof(buf));
    if (addrString != NULL) {
        printmsg ("%s", addrString);
    }
}

#pragma mark -

static int get_timestamp_width (void)
{
    static int width = 0;
    
    if (width == 0) {
        char timeString[BUFSIZ];
        
        if (!krb5_timestamp_to_sfstring(now, timeString, 20, NULL) ||
            !krb5_timestamp_to_sfstring(now, timeString, sizeof (timeString), NULL)) {
            width = strlen(timeString);
        } else {
            width = 15;
        }
    }
    
    return width;
}

static char *enctype_to_string (krb5_enctype enctype)
{
    static char enctypeString[100];

    if (krb5_enctype_to_string (enctype, enctypeString, sizeof(enctypeString)) != 0) {
        sprintf (enctypeString, "etype %d", enctype); /* default if error */
    }

    return enctypeString;
}
