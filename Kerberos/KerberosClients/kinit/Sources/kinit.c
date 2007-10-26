/*
 * kinit.c
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
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

const char *program = NULL;

enum {
    passwordMode,
    keytabMode,
    validateMode,
    renewMode
};

/* Options variables: */
int verbose = 0;

int mode = passwordMode;
int seenTicketMode = 0;

char *keytabName = NULL;

KLPrincipal principal = NULL;
KLLoginOptions loginOptions = NULL;

static int options (int argc, char * const * argv);
static int usage (void);
static void printerr (const char *format, ...);
static void vprinterr (const char *format, va_list args);

int main (int argc, char * const * argv)
{
    KLStatus    err = klNoErr;
    char       *ccacheName = NULL;
    KLPrincipal	outPrincipal = NULL;
    
    /* Remember our program name */
    program = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];

    if (!err) {
        /* Read in our command line options */
        err = options (argc, argv);
    }

    if (!err) {
        switch (mode) {
            case keytabMode:
                err = KLAcquireNewInitialTicketsWithKeytab (principal, loginOptions,
                                                              keytabName, &ccacheName);
                break;
            
            case renewMode:
                err = KLRenewInitialTickets (principal, loginOptions, &outPrincipal, &ccacheName);
                if (principal == NULL) {
                    principal = outPrincipal;
                    outPrincipal = NULL;
                }
                break;
                
            case validateMode:
                err = KLValidateInitialTickets (principal, loginOptions, &ccacheName);
                break;
                
            case passwordMode:
            default:
                err = KLAcquireNewInitialTickets (principal, loginOptions, &outPrincipal, &ccacheName);
                if (principal == NULL) {
                    principal = outPrincipal;
                    outPrincipal = NULL;
                }
                break;
        }
    }
    
    if (!err) {
        KLBoolean foundTickets = false;
        KLStatus  ticketsErr = KLCacheHasValidTickets (principal, kerberosVersion_V5, &foundTickets, NULL, NULL);    
        
        // Expired credentials are still credentials, even if they aren't valid.  
        // Just warn the user:
        if (ticketsErr == klCredentialsExpiredErr) {
            printerr ("Warning!  New tickets are expired.  Please check your Date and Time settings.\n");
            foundTickets = 1;
        }
        
        // Credentials with bad addresses are still credentials, even if they aren't valid.  
        // Just warn the user:
        if (ticketsErr == klCredentialsBadAddressErr) {
            printerr ("Warning!  New tickets have invalid IP addresses.  Check your Network settings.\n");
            foundTickets = 1;
        }
    
        if (foundTickets) { 
            if (verbose) {
                fprintf (stderr, "Authenticated via Kerberos v5.  Placing tickets in cache '%s'\n", ccacheName);
            }
        
            /* if we found tickets, set them as the default */
            if (KLSetSystemDefaultCache (principal) != klNoErr) {
                printerr ("Unable to make '%s' the new system default cache\n", ccacheName);
            }
        }
    }

    if (err && err != klUserCanceledErr) {
        printerr ("Error getting initial tickets: %s\n", error_message (err));
    }        
    
    if (principal != NULL)
        KLDisposePrincipal (principal);

    if (ccacheName != NULL)
        KLDisposeString (ccacheName);
        
    if (outPrincipal != NULL)
        KLDisposePrincipal (outPrincipal);
    
    return err ? 1 : 0;
}

static int options (int argc, char * const * argv)
{
    int option;
    int err = 0;
    
    krb5_deltat lifetime = 0;
    int seenLifetime = 0;
    krb5_deltat startTime = 0;
    int seenStartTime = 0;
    krb5_deltat renewableLife = 0;
    int seenRenewableLife = 0;
    
    int forwardable = 0;
    int seenForwardable = 0;
    
    int proxiable = 0;
    int seenProxiable = 0;
    
    int addressless = 1;
    int seenAddressless = 0;

    char *serviceName = NULL;

    /* Get the arguments */
    while ((option = getopt (argc, argv, "Vl:s:r:fFpPaAvRkt:c:S:")) != -1) {
        switch (option) {
            case 'V':
                verbose = 1;
                break;
                
            case 'l':
                /* Lifetime */
                err = krb5_string_to_deltat(optarg, &lifetime);
                if (err) {
                    printerr ("Invalid lifetime '%s'\n", optarg);
                    return usage ();
                }
                seenLifetime = 1;
                break;
                
            case 's':
                /* Start Time */
                err = krb5_string_to_timestamp(optarg, &startTime);
                if (err) {
                    printerr ("Invalid start time '%s'\n", optarg);
                    return usage ();
                } else {
                    /* Got an absolute time; create the offset: */
                    startTime -= time(0);
                }
                seenStartTime = 1;
                break;
            
            case 'r':
                /* Renewable Lifetime */
                err = krb5_string_to_deltat(optarg, &renewableLife);
                if (err)  {
                    printerr ("Invalid renewable lifetime '%s'\n", optarg);
                    return usage ();
                }
                seenRenewableLife = 1;
                break;
            
            case 'f':
                if (seenForwardable) {
                    printerr ("Only one of -f or -F allowed\n");
                    return usage ();
                }
                forwardable = 1;
                seenForwardable = 1;
                break;
                
            case 'F':
                if (seenForwardable) {
                    printerr ("Only one of -f or -F allowed\n");
                    return usage ();
                }
                forwardable = 0;
                seenForwardable = 1;
                break;
                
            case 'p':
                if (seenProxiable) {
                    printerr ("Only one of -p or -P allowed\n");
                    return usage ();
                }
                proxiable = 1;
                seenProxiable = 1;
                break;
                
            case 'P':
                if (seenProxiable) {
                    printerr ("Only one of -p or -P allowed\n");
                    return usage ();
                }
                proxiable = 0;
                seenProxiable = 1;
                break;
            
            case 'a':
                if (seenAddressless) {
                    printerr ("Only one of -a or -A allowed\n");
                    return usage ();
                }
                addressless = 0;
                seenAddressless = 1;
                break;

            case 'A':
                if (seenAddressless) {
                    printerr ("Only one of -a or -A allowed\n");
                    return usage ();
                }
                addressless = 1;
                seenAddressless = 1;
                break;
                
            case 'v':
                if (seenTicketMode) {
                    printerr ("Only one of -v, -R and -k allowed\n");
                    return usage ();
                }
                mode = validateMode;
                break;
                
            case 'R':
                if (seenTicketMode) {
                    printerr ("Only one of -v, -R and -k allowed\n");
                    return usage ();
                }
                mode = renewMode;
                break;
            
            case 'k':
                if (seenTicketMode) {
                    printerr ("Only one of -v, -R and -k allowed\n");
                    return usage ();
                }
                mode = keytabMode;
                break;
            
            case 't':
                if (keytabName != NULL) {
                    printerr ("Only one -t option allowed\n");
                	return usage ();
                }
                keytabName = optarg;
                break;
            
            case 'S':
                if (serviceName != NULL) {
                    printerr ("Only one -S option allowed\n");
                	return usage ();
                }
                serviceName = optarg;
                break;
                            
            default:
                return usage ();
        }
    }
    
    /* look for extra arguments (other than the principal) */
    if (argc - optind > 1) {
        printerr ("Unknown option '%s'\n", argv[optind + 1]);
        return usage ();
    }
    
    /* First, try to get the principal from the argument list */
    if (optind == argc - 1) {
        err = KLCreatePrincipalFromString (argv[optind], kerberosVersion_V5, &principal);
        if (err != klNoErr) {
            printerr ("Unable to create principal for '%s': %s\n", 
                      argv[optind], error_message (err));
            return 1;
        }        
    }
    
    /* Second, try the principal of the default ccache */
    if (principal == NULL) {
        cc_context_t context = NULL;
        cc_ccache_t  ccache = NULL;
        cc_string_t  principalName = NULL;
        
        /* Initialize the ccache */
        err = cc_initialize (&context, ccapi_version_4, NULL, NULL);
        if (err != ccNoError) {
            printerr ("Unable to initialize credentials cache (error = %d)\n", err);
            return 1;
        }

        if (err == ccNoError) {
            err = cc_context_open_default_ccache (context, &ccache);
        }

        if (err == ccNoError) {
            err = cc_ccache_get_principal (ccache, cc_credentials_v5, &principalName);
        }
        
        if (err == ccNoError) {
            err = KLCreatePrincipalFromString (principalName->data, kerberosVersion_V5, &principal);
            if (err != klNoErr) {
                printerr ("Unable to create principal for '%s': %s\n", 
                        argv[optind], error_message (err));
                return 1;
            }        
        }
    }

    /* If not, fall back on the passwd database */
    if (principal == NULL) {
        struct passwd *pw;
        
        /* Try to get the principal name from the password database */
        if ((pw = getpwuid (getuid ())) != NULL) {
            char *username = malloc (strlen (pw->pw_name) + 1); /* copy because pw not stable */
            if (username == NULL) {
                printerr ("Out of memory\n");
                return 1;
            }
            strcpy (username, pw->pw_name);
            err = KLCreatePrincipalFromString (username, kerberosVersion_V5, &principal);
            free (username);
            
            if (err != klNoErr) {
                printerr ("Unable to create principal for current user: %s\n", 
                          error_message (err));
                return 1;
            }        
        }
    }
    
    /* Login Options */
    err = KLCreateLoginOptions (&loginOptions);
    if (err != klNoErr) {
        printerr ("Unable to initialize kerberos login options: %s\n", error_message (err));
        return 1;
    }
    
    if (seenLifetime) {
        err = KLLoginOptionsSetTicketLifetime (loginOptions, lifetime);
        if (err != klNoErr) {
            printerr ("Unable to set ticket lifetime: %s\n", error_message (err));
            return 1;
        }
    }

    if (seenStartTime) {
        err = KLLoginOptionsSetTicketStartTime (loginOptions, startTime);
        if (err != klNoErr) {
            printerr ("Unable to set ticket start time: %s\n", error_message (err));
            return 1;
        }
    }
    
    if (seenRenewableLife) {
        err = KLLoginOptionsSetRenewableLifetime (loginOptions, renewableLife);
        if (err != klNoErr) {
            printerr ("Unable to set ticket renewable lifetime: %s\n", error_message (err));
            return 1;
        }
    }

    if (seenForwardable) {
        err = KLLoginOptionsSetForwardable (loginOptions, forwardable);
        if (err != klNoErr) {
            printerr ("Unable to set ticket %s forwardable: %s\n", 
                    forwardable ? "" : "not ", error_message (err));
            return 1;
        }
    }

    if (seenProxiable) {
        err = KLLoginOptionsSetProxiable (loginOptions, proxiable);
        if (err != klNoErr) {
            printerr ("Unable to set ticket %s proxiable: %s\n", 
                    proxiable ? "" : "not ", error_message (err));
            return 1;
        }
    }

    if (seenAddressless) {
        err = KLLoginOptionsSetAddressless (loginOptions, addressless);
        if (err != klNoErr) {
            printerr ("Unable to set ticket %s: %s\n", 
                    addressless ? "addressless" : "with addresses", error_message (err));
            return 1;
        }
    }
    
    if (serviceName != NULL) {
        err = KLLoginOptionsSetServiceName (loginOptions, serviceName);
        if (err != klNoErr) {
            printerr ("Unable to set service name to '%s': %s\n", serviceName, error_message (err));
            return 1;
        }
    }

    return 0;
}

static int usage (void)
{
    fprintf (stderr, "Usage: %s [-V] [-l lifetime] [-s start_time] [-r renewable_life]\n", program);
    fprintf (stderr, "\t[-f | -F] [-p | -P] [-a | -A] [-v] [-R]\n");
    fprintf (stderr, "\t[-k [-t keytab_file]] [-S service] [principal]\n\n");

    fprintf (stderr, "\t-V verbose mode\n");
    fprintf (stderr, "\t-l <lifetime> get ticket with lifetime \"lifetime\"\n");
    fprintf (stderr, "\t-s <start_time> get tickets which become valid at \"start_time\"\n");
    fprintf (stderr, "\t-r <renewable_life> get tickets which are renewable for \"renewable_life\"\n");
    fprintf (stderr, "\t-f get forwardable tickets\n");
    fprintf (stderr, "\t-F get tickets which are not forwardable\n");
    fprintf (stderr, "\t-p get proxiable tickets\n");
    fprintf (stderr, "\t-P get tickets which are not proxiable\n");
    fprintf (stderr, "\t-a get tickets with the host's local addresses\n");
    fprintf (stderr, "\t-A get tickets without addresses\n");
    fprintf (stderr, "\t-v validate tickets\n");
    fprintf (stderr, "\t-R renew tickets\n");
    fprintf (stderr, "\t-k get tickets from a keytab\n");
    fprintf (stderr, "\t-t <keytab_file> use keytab \"keytab_file\"\n");
    fprintf (stderr, "\t-S <service> get tickets with service principal name \"service\"\n");
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
