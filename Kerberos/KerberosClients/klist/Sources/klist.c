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

krb5_context kcontext = NULL;

int mode = defaultMode;
int seenTicketMode = 0;

int useCCAPI = 1;

const char *name = NULL;

int showKerberos5 = 0;
int seenShowKerberos5 = 0;

int showKerberos4 = 0;
int seenShowKerberos4 = 0;

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
static int printkrb5ccache (void);
static int printccache (void);
static int printcache (cc_context_t context, cc_ccache_t ccache, int *foundTGT);

static int options (int argc, char * const * argv);
static int usage (void);

static void vprintmsg (const char *format, va_list args);
static void printmsg (const char *format, ...);
static void printiferr (errcode_t err, const char *format, ...);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);
static void printfiller (char c, int count);
static void printtime (time_t time);
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
            if ((name != NULL) && (strchr (name, ':') != NULL) && (strncmp (name, "API:", 4) != 0)) {
                err = printkrb5ccache ();
            } else {
                if ((name != NULL) && (strncmp (name, "API:", 4) == 0)) {
                    name += 4;
                }
                err = printccache ();
            }
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
                    int i;

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

static int printkrb5ccache (void)
{
    krb5_error_code err = 0;
    krb5_ccache ccache = NULL;
    krb5_cc_cursor cursor = NULL;
    krb5_creds creds;
    krb5_principal principal = NULL;
    char *principalName = NULL;
    krb5_flags flags = 0;

    if (!err) {
        if (name != NULL) {
            err = krb5_cc_resolve (kcontext, name, &ccache);
            printiferr (err, "while locating credentials cache '%s'", name);
        } else {
            err = krb5_cc_default (kcontext, &ccache);
            printiferr (err, "while locating the default credentials cache");
        }
    }

    if (!err) {
        err = krb5_cc_set_flags (kcontext, ccache, flags); /* turn off OPENCLOSE */
        if (err == KRB5_FCC_NOFILE) {
            printiferr (err, "(ticket cache %s:%s)",
                        krb5_cc_get_type (kcontext, ccache),
                        krb5_cc_get_name (kcontext, ccache));
        } else {
            printiferr (err, "while setting cache flags (ticket cache %s:%s)",
                        krb5_cc_get_type (kcontext, ccache),
                        krb5_cc_get_name (kcontext, ccache));
        }
    }

    if (!err) {
        err = krb5_cc_get_principal (kcontext, ccache, &principal);
        printiferr (err, "while retrieving principal name");
    }

    if (!err) {
        err = krb5_unparse_name (kcontext, principal, &principalName);
        printiferr (err, "while unparsing principal name");
    }

    if (!err) {
        printmsg ("Kerberos 5 ticket cache: '%s:%s'\nDefault principal: %s\n\n",
                  krb5_cc_get_type (kcontext, ccache),
                  krb5_cc_get_name (kcontext, ccache), principalName);
        printmsg ("Valid Starting");
        printfiller (' ', get_timestamp_width () - sizeof ("Valid Starting") + 3);
        printmsg ("Expires");
        printfiller (' ', get_timestamp_width () - sizeof ("Expires") + 3);
        printmsg ("Service Principal\n");
    }

    if (!err) {
        err = krb5_cc_start_seq_get (kcontext, ccache, &cursor);
        printiferr (err, "while starting to retrieve tickets");
    }

    if (!err) {
        while ((err = krb5_cc_next_cred (kcontext, ccache, &cursor, &creds)) == 0) {
            char *clientName = NULL;
            char *serverName = NULL;
            int extraField = 0;

            if (!err) {
                err = krb5_unparse_name (kcontext, creds.client, &clientName);
                printiferr (err, "while unparsing client name");
            }

            if (!err) {
                err = krb5_unparse_name (kcontext, creds.server, &serverName);
                printiferr (err, "while unparsing server name");
            }

            if (!err) {
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
            }

            if (!err) {
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
            }

            if (!err) {
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

            if (clientName != NULL) { krb5_free_unparsed_name (kcontext, clientName); }
            if (serverName != NULL) { krb5_free_unparsed_name (kcontext, serverName); }
            
            krb5_free_cred_contents (kcontext, &creds);
        }
        if (err == KRB5_CC_END) { err = 0; }
        printiferr (err, "while retrieving a ticket");

    }

    if (!err) {
        err = krb5_cc_end_seq_get (kcontext, ccache, &cursor);
        printiferr (err, "while finishing ticket retrieval");
    }

    if (!err) {
        flags = KRB5_TC_OPENCLOSE; /* restore OPENCLOSE mode */
        err = krb5_cc_set_flags (kcontext, ccache, flags);
        printiferr (err, "while finishing ticket retrieval");
    }

    return err ? 1 : 0;
}

static int printccache (void)
{
    cc_context_t         context = NULL;
    cc_ccache_iterator_t ccacheIterator = NULL;
    cc_int32             err = ccNoError;
    int                  exit = 0;
    int                  foundTGT = 0;
    
    /* Initialize the ccache */
    if (err == ccNoError) {
        err = cc_initialize (&context, ccapi_version_4, NULL, NULL);
        printiferr (err, "while initializing credentials cache");
    }

    if (showAll) {
        cc_ccache_t ccache = NULL;
        int         firstCCache = 1;

        if (err == ccNoError) {
            err = cc_context_new_ccache_iterator (context, &ccacheIterator);
            printiferr (err, "while starting to iterate over credentials caches");
        }

        if (err == ccNoError) {
            while ((err = cc_ccache_iterator_next (ccacheIterator, &ccache)) == ccNoError) {
                int found = 0;
    
                if (err == ccNoError) {
                    /* Print spacing between caches in the list */
                    if (!firstCCache) {
                        printfiller ('-', 79);
                        printmsg ("\n");
                    }
                    firstCCache = 0;
                }
    
                if (err == ccNoError) {
                    exit = printcache (context, ccache, &found);
                }
                
                if (err == ccNoError) {
                    if (found) { foundTGT++; }
                }

                if (ccache != NULL) { cc_ccache_release (ccache); ccache = NULL; }
            }
    
            if (err == ccIteratorEnd) { err = ccNoError; }
            printiferr (err, "while iterating over credentials caches");
        }
    } else {
        cc_ccache_t ccache = NULL;
        cc_string_t ccacheName = NULL;
        const char *namePtr = NULL;

        if (name == NULL) {
            if (err == ccNoError) {
                /* get the ccache name if it was not specified */
                err = cc_context_get_default_ccache_name (context, &ccacheName);
                printiferr (err, "while getting default ccache name");
            }
        }

        if (err == ccNoError) {
            namePtr = (name != NULL) ? name : ccacheName->data;
        }
        
        if (err == ccNoError) {
            err = cc_context_open_ccache (context, namePtr, &ccache);
            if (err == ccErrCCacheNotFound) {
                printerr ("No credentials cache found (ticket cache API:%s)\n", namePtr);
            } else {
                printiferr (err, "while opening credentials cache '%s'", namePtr);
            }
        }

        if (err == ccNoError) {
            exit = printcache (context, ccache, &foundTGT);
        }

        if (ccache     != NULL) { cc_ccache_release (ccache); }
        if (ccacheName != NULL) { cc_string_release (ccacheName); }
    }

    if (ccacheIterator != NULL) { cc_ccache_iterator_release (ccacheIterator); }
    if (context        != NULL) { cc_context_release (context); }

    if (exit != 0) {
        return exit;
    } else {
        return foundTGT ? 0 : 1;
    }
}

static int printcache (cc_context_t context, cc_ccache_t ccache, int *foundTGT)
{
    cc_credentials_iterator_t credsIterator = NULL;
    cc_credentials_t          creds = NULL;
    cc_string_t               ccacheName = NULL;
    cc_string_t               v4PrincipalName = NULL;
    cc_string_t               v5PrincipalName = NULL;
    cc_int32                  err = ccNoError;
    
    krb5_principal            client = NULL;
    krb5_principal            service = NULL;

    int                       foundV4Tickets = 0;
    int                       foundV5Tickets = 0;
    int                       exit = 0;
    int                       firstTickets;
    
    *foundTGT = 0;
    
    err = cc_ccache_get_name (ccache, &ccacheName);
    if (err != ccNoError) {
        printerr ("Unable to get name of credentials cache (error = %d)\n", err);
        exit = 1;
        goto done;
    }
    
    err = cc_ccache_get_principal (ccache, cc_credentials_v4, &v4PrincipalName);
    if (err != ccNoError) {
        v4PrincipalName = NULL;
    }
    
    err = cc_ccache_get_principal (ccache, cc_credentials_v5, &v5PrincipalName);
    if (err != ccNoError) {
        v5PrincipalName = NULL;
    }
    
    /* 
     * Kerberos 5 Credentials 
     */
    if (showKerberos5 && v5PrincipalName != NULL) {
        printmsg ("Kerberos 5 ticket cache: 'API:%s'\n", ccacheName->data);
        printmsg ("Default Principal: %s\n", v5PrincipalName->data);

        err = cc_ccache_new_credentials_iterator (ccache, &credsIterator);
        if (err != ccNoError) {
            printerr ("Unable to iterate over credentials in ccache '%s' (error = %d)\n", 
                        ccacheName->data, err);
            exit = 1;
            goto done;
        }
        
        firstTickets = 1;
        for (;;) {
            err = cc_credentials_iterator_next (credsIterator, &creds);
            if ((err != ccNoError) && (err != ccIteratorEnd)) {
                printerr ("Error iterating over credentials in ccache '%s' (error = %d)\n", 
                            ccacheName->data, err);
                exit = 1;
                goto done;
            } else if (err == ccIteratorEnd) {
                err = ccNoError; // No more credentials
                break;
            }
            
            /* print out any v5 credentials */
            if (creds->data->version == cc_credentials_v5) {
                int 				extraField = 0;
                cc_credentials_v5_t *creds5 = creds->data->credentials.credentials_v5;

                foundV5Tickets = 1;
                
                if (firstTickets) {
                    printmsg ("Valid Starting");
                    printfiller (' ', get_timestamp_width () - sizeof ("Valid Starting") + 3);
                    printmsg ("Expires");
                    printfiller (' ', get_timestamp_width () - sizeof ("Expires") + 3);
                    printmsg ("Service Principal\n");
                    firstTickets = 0;
                }

               /* Get the client principal */
                err = krb5_parse_name (kcontext, creds5->client, &client);
                if (err) {
                    printerr ("Error parsing client principal '%s' (error = %d)\n", creds5->client, err);
                    exit = 1;
                    goto done;                    
                }
                
               /* Get the service principal */
                err = krb5_parse_name (kcontext, creds5->server, &service);
                if (err) {
                    printerr ("Error parsing service principal '%s' (error = %d)\n", creds5->server, err);
                    exit = 1;
                    goto done;                    
                }
                
                printtime (creds5->starttime ? creds5->starttime : creds5->authtime);
                printmsg ("  ");
                printtime (creds5->endtime);
                printmsg ("  ");
                printmsg ("%s\n", creds5->server);
                
                if (strcmp (v5PrincipalName->data, creds5->client) != 0) {
                    if (!extraField) {
                        printmsg ("\t");
                    }
                    printmsg ("for client %s", creds5->client);
                    extraField++;
                }
                
                if (creds5->ticket_flags & TKT_FLG_RENEWABLE) {
                    if (!extraField) {
                        printmsg ("\t");
                    } else {
                        printmsg (", ");
                    }
                    printmsg ("renew until ");
                    printtime (creds5->renew_till);
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
                    printflags (creds5->ticket_flags);
                    extraField++;
                }
                
                if (extraField > 2) {
                    printmsg ("\n");
                    extraField = 0;
                }
                
                if (showEnctypes) {
                    krb5_data ticket;
                    krb5_ticket *ticket_rep;
                    
                    ticket.length = creds5->ticket.length;
                    ticket.data = creds5->ticket.data;
                    
                    if (krb5_decode_ticket (&ticket, &ticket_rep) == 0) {
                        if (!extraField) {
                            printmsg ("\t");
                        } else {
                            printmsg (", ");
                        }
                        printmsg ("Etype (skey, tkt): %s, ", enctype_to_string (creds5->keyblock.type));
                        printmsg ("%s ", enctype_to_string (ticket_rep->enc_part.enctype));
                        extraField++;
                        
                        krb5_free_ticket(kcontext, ticket_rep);
                    }
                }
                
                if (extraField) {
                    printmsg ("\n");
                }
                
                if (showAddressList) {
                    printmsg ("\tAddresses: ");
                    if (creds5->addresses == NULL || creds5->addresses[0] == NULL) {
                        printmsg ("(none)\n");
                    } else {
                        krb5_address address;
                        int i;
                        
                        for (i = 0; creds5->addresses[i]; i++) {
                            address.addrtype = creds5->addresses[i]->type;
                            address.magic = KV5M_ADDRESS;
                            address.length = creds5->addresses[i]->length;
                            address.contents = creds5->addresses[i]->data;  /* just copy pointer */
                            
                            if (i > 0) {
                                printmsg (", ");
                            }
                            printaddress (address);
                        }
                        printmsg ("\n");
                    }
                }
                
                /* Is the ticket a valid krb5 tgt?  should be "krbtgt/realm@realm" */
                if (service->length == 2 &&
                    strcmp (service->data[0].data, KRB5_TGS_NAME) == 0 &&
                    strcmp (service->data[1].data, client->realm.data) == 0 &&
                    strcmp (service->realm.data, client->realm.data) == 0 &&
                    creds5->endtime > time (0)) {
                    *foundTGT = 1; /* valid tgt */
                }
                
                krb5_free_principal (kcontext, client);
                client = NULL;
                krb5_free_principal (kcontext, service);
                service = NULL;
            }
            
            cc_credentials_release (creds);
            creds = NULL;
        }
        
        cc_credentials_iterator_release (credsIterator);
        credsIterator = NULL;
        
        if (!foundV5Tickets) {
            printerr ("No Kerberos 5 tickets in credentials cache\n");
        }
        printmsg ("\n");
    }

    /* 
     * Kerberos 4 Credentials 
     */
    if (showKerberos4 && v4PrincipalName != NULL) {
        printmsg ("Kerberos 4 ticket cache: '%s'\n", ccacheName->data);
        printmsg ("Default Principal: %s\n", v4PrincipalName->data);
        
        err = cc_ccache_new_credentials_iterator (ccache, &credsIterator);
        if (err != ccNoError) {
            printerr ("Unable to iterate over credentials in ccache '%s' (error = %d)\n", 
                        ccacheName->data, err);
            exit = 1;
            goto done;
        }
        
        firstTickets = 1;
        for (;;) {
            err = cc_credentials_iterator_next (credsIterator, &creds);
            if ((err != ccNoError) && (err != ccIteratorEnd)) {
                printerr ("Error iterating over credentials in ccache '%s' (error = %d)\n", 
                            ccacheName->data, err);
                exit = 1;
                goto done;
            } else if (err == ccIteratorEnd) {
                err = ccNoError; // No more credentials
                break;
            }
            
            /* print out any v4 credentials */
            if (creds->data->version == cc_credentials_v4) {
                cc_credentials_v4_t *creds4 = creds->data->credentials.credentials_v4;
                foundV4Tickets = 1;
                
                if (firstTickets) {
                    printmsg ("Issued");
                    printfiller (' ', get_timestamp_width () - sizeof ("Issued") + 3);
                    printmsg ("Expires");
                    printfiller (' ', get_timestamp_width () - sizeof ("Expires") + 3);
                    printmsg ("Service Principal\n");
                    firstTickets = 0;
                }
                printtime (creds4->issue_date);
                printmsg ("  ");
                printtime (creds4->issue_date + creds4->lifetime);
                printmsg ("  ");
                printmsg("%s%s%s%s%s\n", creds4->service, creds4->service_instance[0] ? "." : "",
                        creds4->service_instance, creds4->realm[0] ? "@" : "", creds4->realm);

                if (showAddressList) {
                    krb5_address address;
                    
                    address.addrtype = ADDRTYPE_INET;
                    address.magic = KV5M_ADDRESS;
                    address.length = 4;
                    address.contents = (krb5_octet *) &creds4->address;
                        
                    printmsg ("\tAddress: ");
                    if (creds4->address == 0) {
                        printmsg ("(none)");
                    } else {
                        printaddress (address);
                    }
                    printmsg ("\n");
                }

                /* Is the ticket a valid krb4 tgt?  should be "krbtgt.realm@realm" */
                if (strcmp (creds4->service, KRB_TICKET_GRANTING_TICKET) == 0 &&
                    strcmp (creds4->service_instance, creds4->realm) == 0 &&
                    (creds4->issue_date + creds4->lifetime) > time (0)) {
                    *foundTGT = 1; /* valid tgt */
                }
            }
            
            cc_credentials_release (creds);
            creds = NULL;
        }
        
        cc_credentials_iterator_release (credsIterator);
        credsIterator = NULL;
        
        if (!foundV4Tickets) {
            printerr ("No Kerberos 4 tickets in credentials cache\n");
        }
        printmsg ("\n");
    }
    
done:     
    if (client != NULL)
        krb5_free_principal (kcontext, client);
        
    if (service != NULL)
        krb5_free_principal (kcontext, service);
    
    if (creds != NULL)
        cc_credentials_release (creds);
    
    if (credsIterator != NULL)
        cc_credentials_iterator_release (credsIterator);

    if (v4PrincipalName != NULL)
        cc_string_release (v4PrincipalName);

    if (v5PrincipalName != NULL)
        cc_string_release (v5PrincipalName);

    if (ccacheName != NULL) {
        cc_string_release (ccacheName);
        name = NULL;
    }
    
    return exit;
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
                if (seenShowKerberos5) {
                    printerr ("Only one -5 option allowed\n");
                    return usage ();
                }
                showKerberos5 = 1;
                seenShowKerberos5 = 1;
                break;

            case '4':
                if (seenShowKerberos5) {
                    printerr ("Only one -4 option allowed\n");
                    return usage ();
                }
                showKerberos4 = 1;
                seenShowKerberos4 = 1;
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
    
    if (!seenShowKerberos5 && !seenShowKerberos4) {
        /* Default to both */
        showKerberos5 = 1;
        showKerberos4 = 1;
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
    
    /* First, try to get the principal from the argument list */
    name = (optind == argc - 1) ? argv[optind] : NULL;

    if (name == NULL) {
        name = getenv ("KRB5CCNAME");
    }
    
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
    if (err) {
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

static void printtime (time_t time)
{
    char string[BUFSIZ];
    char filler = ' ';
    
    if (!krb5_timestamp_to_sfstring((krb5_timestamp) time, string, 
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
        time_t now = time(0);
        
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
