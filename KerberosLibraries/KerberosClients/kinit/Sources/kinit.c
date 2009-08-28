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
 */

#include <Kerberos/Kerberos.h>
#include <Kerberos/kim.h>
#include <sys/errno.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <err.h>
#include <unistd.h>

enum {
    password_mode,
    keytab_mode,
    validate_mode,
    renew_mode,
    renewBackgroundMode
};

static kim_boolean windows = 0;

/* ------------------------------------------------------------------------ */

static int usage (void)
{
    fprintf (stderr, "Usage: %s [-V] [-l lifetime] [-s start_time] [-r renewable_life]\n", getprogname ());
    fprintf (stderr, "\t[-f | -F] [-p | -P] [-a | -A] [-v] [-R] [-B]\n");
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
    fprintf (stderr, "\t-B renew all tickets\n");
    fprintf (stderr, "\t-W mark as windows credential\n");
    return 2;
}

/* ------------------------------------------------------------------------ */

static void vprinterr (const char *format, va_list args)
{
    fprintf (stderr, "%s: ", getprogname ());
    vfprintf (stderr, format, args);
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

static void printiferr (kim_error   in_error, 
                        const char *in_format, 
                        ...)
{
    if (in_error && in_error != KIM_USER_CANCELED_ERR) {
        kim_string message = NULL;
        kim_error err = KIM_NO_ERROR;
        va_list pvar;
        
        va_start (pvar, in_format);
        vprinterr (in_format, pvar);
        va_end (pvar);

        err = kim_string_create_for_last_error (&message, in_error);
        if (!err) {
            fprintf (stderr, ": %s\n", message);
        } else {
            fprintf (stderr, ".\n");            
        }
        
        kim_string_free (&message);
    }
}

#define WARNING_MODE "Only one of -B, -v, -R and -k allowed\n"


/* ------------------------------------------------------------------------ */

static kim_error parse_options (int            in_argc, 
                                char * const  *in_argv,
                                kim_identity  *out_identity,
                                kim_options   *out_options,
                                int           *out_mode,
                                char         **out_keytab_name,
                                char         **out_password,
                                int           *out_verbose)
{
    kim_error err = KIM_NO_ERROR;
    kim_identity identity = NULL;
    kim_options options = NULL; 
    int mode = password_mode;
    char *keytab_name = NULL;
    char *password = NULL;
    int verbose = 0;
    kim_boolean seen_mode = 0;
    kim_boolean seen_service_name = 0;
    krb5_deltat k5time = 0;
    
    err = kim_options_create (&options);
    
    while (!err) {
        int option = getopt (in_argc, in_argv, "WBVl:T:s:r:fFpPaAvRkt:c:S:");
        
        if (option == -1) { break;  /* out of options */ }
        
        switch (option) {
	    case 'W':
		windows = 1;
		break;
            case 'V':
                verbose = 1;
                break;
                
            case 'l':
                err = krb5_string_to_deltat (optarg, &k5time);
                printiferr (err, "Invalid lifetimetime '%s'", optarg); 
                
                if (!err) {
                    err = kim_options_set_lifetime (options, k5time);
                    printiferr (err, "Unable to set ticket lifetime"); 
                }
                break;
            
            case 'T': { 
                FILE *f = NULL;
                
                if (strcasecmp ("STDIN", optarg) == 0) {
                    f = stdin;
                } else {
                    f = fopen (optarg, "r");
                }
                if (!f) { err = ENOENT; }
                printiferr (err, "Unable to get passwd input file"); 
                
                if (!err) {
                    char pbuffer[1024];
                    
                    char *p = fgets (pbuffer, sizeof (pbuffer), f);
                    if (!p) { err = ENOENT; }
                    printiferr (err, "Unable to read passwd from input file"); 
                    
                    if (!err) {
                        pbuffer[strcspn (pbuffer, "\n")] = '\0';

                        password = strdup (pbuffer);
                        if (!password) { err = ENOMEM; }
                        
                        memset (pbuffer, 0, sizeof (pbuffer));
                    }
                }
                
                if (f && f != stdin) { fclose (f); }
                
                break;                
            }
            
            case 's':                
                err = krb5_string_to_timestamp(optarg, &k5time);
                printiferr (err, "Invalid start time '%s'", optarg); 
        
                if (!err) {
                    k5time -= time(0); /* delta_t is absolute time */
                    
                    err = kim_options_set_start_time (options, k5time);
                    printiferr (err, "Unable to set ticket start time"); 
                }
                break;
                
            case 'r':               
                err = krb5_string_to_deltat (optarg, &k5time);
                printiferr (err, "Invalid renewable lifetime '%s'", optarg);
                
                if (!err) {
                    err = kim_options_set_renewal_lifetime (options, k5time);                
                    printiferr (err, "Unable to set ticket renewal lifetime");
                }
                break;
                
            case 'f':
                err = kim_options_set_forwardable (options, 1);
                printiferr (err, "Unable to set ticket forwardable state"); 
                break;
                
            case 'F':
                err = kim_options_set_forwardable (options, 0);
                printiferr (err, "Unable to set ticket forwardable state"); 
                break;
                
            case 'p':
                err = kim_options_set_proxiable (options, 1);
                printiferr (err, "Unable to set ticket proxiable state"); 
               break;
                
            case 'P':
                err = kim_options_set_proxiable (options, 0);
                printiferr (err, "Unable to set ticket proxiable state"); 
               break;
                
            case 'a':
                err = kim_options_set_addressless (options, 1);
                printiferr (err, "Unable to set ticket addressless state"); 
                break;
                
            case 'A':
                err = kim_options_set_addressless (options, 0);
                printiferr (err, "Unable to set ticket addressless state"); 
                break;
               
            case 'v':
                if (seen_mode) {
                    printerr (WARNING_MODE);
                    err = EINVAL;
                } else {
                    seen_mode = 1;
                    mode = validate_mode;
                }
                break;
                
            case 'R':
                if (seen_mode) {
                    printerr (WARNING_MODE);
                    err = EINVAL;
                } else {
                    seen_mode = 1;
                    mode = renew_mode;
                }
                break;
                
            case 'k':
                if (seen_mode) {
                    printerr (WARNING_MODE);
                    err = EINVAL;
                } else {
                    seen_mode = 1;
                    mode = keytab_mode;
                }
                break;
                
	    case 'B':
		if (seen_mode) {
                    printerr (WARNING_MODE);
                    err = EINVAL;
                } else {
		    seen_mode = 1;
		    mode = renewBackgroundMode;
		}
		break;

            case 't':
                if (keytab_name) {
                    printerr ("Only one -t option allowed\n");
                    err = EINVAL;
                } else if (seen_mode && mode != keytab_mode) {
                    printerr ("-t option requires -k option\n");
                    err = EINVAL;
                }
                keytab_name = optarg;
                break;
                
            case 'S':
                if (seen_service_name) {
                    printerr ("Only one -S option allowed\n");
                    err = EINVAL;
                }
                
                seen_service_name = 1;
                if (!err) {
                    err = kim_options_set_service_name (options, optarg);
                    printiferr (err, "Unable to set ticket service name"); 
                }
                break;
                
            default:
                err = EINVAL;
                break;
        }
    }
    
    if (mode != renewBackgroundMode) {

    /* look for extra arguments (other than the principal) */
    if (!err && in_argc - optind > 1) {
        printerr ("Unknown option '%s'\n", in_argv[optind + 1]);
        err = EINVAL;
    }
    
    /* First, try to get the principal from the argument list */
    if (!err && optind == in_argc - 1) {
        err = kim_identity_create_from_string (&identity, in_argv[optind]);
        if (err) { 
            printerr ("Unable to create principal for '%s': %s\n", 
                      in_argv[optind], error_message (err)); 
        }
    }
    
    /* Second, try the principal of the default ccache */
    if (!err && !identity) {
        kim_ccache ccache = NULL;
        
        /* No principal specified, try to find it in the default ccache */
        err = kim_ccache_create_from_default (&ccache);
        printiferr (err, "Unable to open default ccache"); 
        
        if (!err) {
            err = kim_ccache_get_client_identity (ccache, &identity);
            if (err != KRB5_FCC_NOFILE) {
                printiferr (err, "Unable to create principal for default ccache"); 
            } else {
                err = KIM_NO_ERROR; /* no ccaches in cache collection */
            }
        }
    }
    
    /* If cache collection is empty, fall back on the passwd database */
    if (!err && !identity) {
        struct passwd *pw = getpwuid (getuid ());
        if (!pw) {
            err = ENOENT;
            printiferr (err, "Unable to get current username in password database");
        }
        
        if (!err) {
            err = kim_identity_create_from_string (&identity, pw->pw_name);
            printiferr (err, "Unable to create principal for '%s'", 
                        pw->pw_name);
        }
    }
    }    
    
    if (!err) {
        *out_identity = identity;
        *out_options = options;
        *out_mode = mode;
        *out_keytab_name = keytab_name;
        *out_password = password;
        *out_verbose = verbose;
    }
    
    return !err ? 0 : usage ();    
}

static int
bgRenew(kim_options options)
{
    cc_ccache_iterator_t iterator;
    kim_time timenow = time(NULL);
    cc_context_t cc_context;
    krb5_error_code ret;
    krb5_context kcontext;
    int found = 0;
    
    ret = krb5_init_context(&kcontext);
    if (ret)
	errx(1, "krb5_init_context");
    
    /* Initialize the CCAPI */
    ret = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    if (ret)
	errx(1, "cc_initialize failed: %d", ret);
    
    ret = cc_context_new_ccache_iterator (cc_context, &iterator);
    if (ret)
	errx(1, "cc_context_new_ccache_iterator failed: %d", ret);
    
    while (1) {
	cc_ccache_t cc_ccache;
	kim_ccache kcache = NULL;
	kim_string ccacheName;
	cc_string_t principalName = NULL;
	krb5_ccache ccache;
	kim_identity identity = NULL;
	kim_time expiration, starttime;
	
	ret = cc_ccache_iterator_next (iterator, &cc_ccache);
	if (ret == ccIteratorEnd)
	    break;
	else if (ret)
	    errx(1, "cc_ccache_iterator_next");
	
	ret = cc_ccache_get_principal (cc_ccache, cc_credentials_v5, &principalName);
	if (ret)
	    errx(1, "cc_ccache_get_principal");
	
	ret = kim_identity_create_from_string (&identity, principalName->data);
	if (ret)
	    errx(1, "kim_identity_create_from_string");

	ret = kim_ccache_create_from_client_identity (&kcache, identity);
	if (ret)
	    errx(1, "kim_ccache_create_from_client_identity");

	
	ret = kim_ccache_get_expiration_time(kcache, &expiration);
	if (ret)
	    errx(1, "kim_ccache_get_expiration_time");

	ret = kim_ccache_get_start_time(kcache, &starttime);
	if (ret)
	    errx(1, "kim_ccache_get_expiration_time");

	/* if expired, skip */
	if (expiration < timenow) {
	    goto next;
	}	
	
	/* is the lifetime left more then half left, mark that we found an entry and try next */
	if ((expiration - starttime) / 2 < (expiration - timenow)) {
	    found++;
	    goto next;
	}
	
	/* figure out last time renew was done */
	ret = kim_ccache_get_display_name (kcache, &ccacheName);
	if (ret)
	    errx(1, "while getting credentials cache name");
	
	ret = krb5_cc_resolve (kcontext, ccacheName, &ccache);
	kim_string_free(&ccacheName);
	if (ret)
	    errx(1, "resolve credentials cache");
	
#define LASTRENEW "last-renew"
#define RENEWLIMIT 600 /* don't refetch any faster then this */
	{
	    uint32_t last_renew, now = timenow;
	    krb5_data data;
	    
	    ret = krb5_cc_get_config(kcontext, ccache, NULL, LASTRENEW, &data);
	    if (ret == 0) {
		if (data.length != sizeof(last_renew)) {
		    krb5_cc_close(kcontext, ccache);
		    krb5_free_data_contents(kcontext, &data);
		    goto next;
		}
		memcpy(&last_renew, data.data, sizeof(last_renew));
		krb5_free_data_contents(kcontext, &data);
		if (now < last_renew + RENEWLIMIT) {
		    krb5_cc_close(kcontext, ccache);
		    goto next;
		}
	    }
	    
	    data.data = (void *)&now;
	    data.length = sizeof(now);
	    
	    ret = krb5_cc_set_config(kcontext, ccache, NULL, LASTRENEW, &data);
	    printf("stored config %d\n", ret);
	}
	krb5_cc_close(kcontext, ccache);
	
        
	ret = kim_ccache_renew (kcache, options);

	if (ret == 0) {
	    found++;
	} else
	    warnx("KLRenewInitialTickets: %s: %d", principalName->data, ret);
	
    next:
	if (kcache)
	    kim_ccache_free (&kcache);
	if (identity)
	    kim_identity_free (&identity);
    }
    
    cc_ccache_iterator_release (iterator);
    cc_context_release (cc_context);
    krb5_free_context(kcontext);
    
    /* no ticket, or no successful renewals, fail */
    return (found == 0) ? -1 : 0;
}


/* ------------------------------------------------------------------------ */

int main (int argc, char * const * argv)
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    kim_identity identity = NULL;
    kim_string id_string = NULL;
    kim_options options = NULL; 
    int mode = password_mode;
    char *keytab_name = NULL;
    char *password = NULL;
    int verbose = 0;
    
    
    /* Remember our program name */
    setprogname (argv[0]);
    
    /* Read in our command line options */
    err = parse_options (argc, argv, 
                         &identity, &options, &mode,
                         &keytab_name, &password, &verbose);

    if (mode == renewBackgroundMode) {
	    bgRenew(options);
    } else {


    if (!err) {
        err = kim_identity_get_display_string (identity, &id_string);
        printiferr (err, "Unable to create display string for principal");
    }
    
    if (!err) {
        if (mode == keytab_mode) {
            err = kim_ccache_create_from_keytab (&ccache, 
                                                 identity, 
                                                 options, 
                                                 keytab_name);
            printiferr (err, "Unable to create tickets using keytab '%s'", 
                        keytab_name);
            
        } else if (mode == renew_mode) {
            err = kim_ccache_create_from_client_identity (&ccache, identity);
            
            printiferr (err, "Could not find ticket cache for '%s'", id_string);
            if (!err) {
                err = kim_ccache_renew (ccache, options);
                printiferr (err, "Unable to renew tickets for '%s'",
			    id_string);
            }
            
        } else if (mode == validate_mode) {
            err = kim_ccache_create_from_client_identity (&ccache, identity);
            printiferr (err, "Could not find ticket cache for '%s'", id_string);
            
            if (!err) {
                err = kim_ccache_validate (ccache, options);
                printiferr (err, "Unable to validate tickets for '%s'",
                            id_string);
            }
            
        } else if (password) {
            err = kim_ccache_create_new_with_password (&ccache, identity, 
                                                       options, password);
            printiferr (err, "Unable to acquire tickets for '%s'",
                        id_string);
            
        } else {
            err = kim_ccache_create_new (&ccache, identity, options);
            printiferr (err, "Unable to acquire credentials for '%s'",
                        id_string);
            
        }
    }
    }
    
    if (!err) {
	krb5_context context;
	krb5_ccache id;
	
	err = krb5_init_context(&context);
	
	if (!err) {
	    err = kim_ccache_get_krb5_ccache (ccache, context, &id);
	    if (!err) {
		krb5_data data;
		data.length = 0;
		data.data = NULL;
		/* mark up as sso credential */
		krb5_cc_set_config(context, id, NULL, "apple-sso", &data);
		if (windows)
		    krb5_cc_set_config(context, id, NULL, "windows", &data);
		krb5_cc_close(context, id);
	    }
	    krb5_free_context(context);
	}
    }

    if (!err) {
        kim_credential_state state;
        
        err = kim_ccache_get_state (ccache, &state);
        
        if (!err) {
            if (state == kim_credentials_state_expired) {
                printerr ("Warning!  New tickets are expired.  Please check your Date and Time settings.\n");
                
            } else if (state == kim_credentials_state_address_mismatch) {
                printerr ("Warning!  New tickets have invalid IP addresses.  Check your Network settings.\n");
            }
            
            if (verbose) {
                kim_string name = NULL;
                
                err = kim_ccache_get_display_name (ccache, &name);
                
                if (!err) {
                    fprintf (stderr, "Authenticated via Kerberos v5.  "
                             "Placing tickets for '%s' in cache '%s'\n", 
                             id_string, name);
                }
                
                kim_string_free (&name);
            }
        }
    }
    
    if (!err) {
        err = kim_ccache_set_default (ccache);
    }
    
    if (password) {
        memset (password, 0, strlen (password));
        free (password);
    }
    
    kim_string_free (&id_string);
    kim_ccache_free (&ccache);
    kim_identity_free (&identity);
    kim_options_free (&options);
    
    return err ? 1 : 0;
}
