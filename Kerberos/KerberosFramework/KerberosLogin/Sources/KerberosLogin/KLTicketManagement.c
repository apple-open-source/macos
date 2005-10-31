/*
 * KLTicketManagement.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLTicketManagement.c,v 1.28 2005/05/16 22:02:38 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
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


#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLAcquireKerberos4CredentialsFromKerberos5Credentials (krb5_context inContext, 
                                                                         krb5_creds  *inV5Credentials, 
                                                                         CREDENTIALS *outV4Credentials)
{
    KLStatus    err = klNoErr;
    krb5_ccache memoryCache = NULL;
    krb5_creds  *v524Credentials = NULL;

    if (inContext        == NULL) { err = KLError_ (klParameterErr); }
    if (inV5Credentials  == NULL) { err = KLError_ (klParameterErr); }
    if (outV4Credentials == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = krb5_cc_resolve (inContext, "MEMORY:__KLAcquireKerberos5CredentialsForKerberos524", &memoryCache);
    }
    
    if (err == klNoErr) {
        err = krb5_cc_initialize (inContext, memoryCache, inV5Credentials->client);
    }
    
    if (err == klNoErr) {
        err = krb5_cc_store_cred (inContext, memoryCache, inV5Credentials);
    }
    
    if (err == klNoErr) {
        krb5_creds in;
        
        memset((char *) &in, 0, sizeof(in));
        in.client = inV5Credentials->client;
        in.server = inV5Credentials->server;
        in.times.endtime = 0;
        in.keyblock.enctype = ENCTYPE_DES_CBC_CRC;

        err = krb5_get_credentials (inContext, 0, memoryCache, &in, &v524Credentials);
        dprintf ("krb5_get_credentials returned %d (%s)\n", err, error_message (err));
    }
    
    if (err == klNoErr) {
        err = krb5_524_convert_creds (inContext, v524Credentials, outV4Credentials);
        dprintf ("krb5_524_convert_creds returned %d (%s)\n", err, error_message (err));
    }
    
    if (err == klNoErr) {
        err = __KLVerifyKDCOffsetsForKerberos4 (inContext);
    }

    if (v524Credentials != NULL) { krb5_free_creds (inContext, v524Credentials); }
    if (memoryCache     != NULL) { krb5_cc_destroy (inContext, memoryCache); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLAcquireInitialTicketsForCache (const char          *inCacheName,
                                            KLKerberosVersion    inKerberosVersion,
                                            KLLoginOptions       inLoginOptions,
                                            KLPrincipal         *outPrincipal,
                                            char               **outCacheName)
{
    KLStatus  lockErr = __KLLockCCache (kWriteLock);
    KLStatus  err = lockErr;

    KLCCache    ccache = NULL;
    KLPrincipal ccachePrincipal = NULL; 

    KLPrincipal gotPrincipal = NULL;
    char       *gotCCacheName = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (inCacheName  == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetCCacheByName (inCacheName, &ccache);

        if (err == klNoErr) {            
            err = __KLGetPrincipalForCCache (ccache, &ccachePrincipal);
        }

        if (err == klNoErr) {
            err = __KLCacheHasValidTickets (ccache, inKerberosVersion);
            
            if (err == klNoErr) { // Tickets exist!
                err = __KLGetPrincipalForCCache (ccache, &gotPrincipal);
                
                if (!err) {
                    err = __KLGetNameForCCacheVersion (ccache, inKerberosVersion, &gotCCacheName);
                }
            } else {
                if (err == klCredentialsExpiredErr) {
                    err = KLRenewInitialTickets (ccachePrincipal, inLoginOptions, &gotPrincipal, &gotCCacheName);
                } else if (err == klCredentialsBadAddressErr) {
                    err = __KLAcquireNewKerberos4TicketsFromKerberos5Tickets (ccachePrincipal, &gotPrincipal, &gotCCacheName);
                } else if (err == klCredentialsNeedValidationErr) {
                    err = __KLValidateInitialTickets (ccachePrincipal, inLoginOptions, &gotPrincipal, &gotCCacheName);
                }
            }
        }

        if ((err != klNoErr) && __KLAllowAutomaticPrompting ()) {
            // If searching the credentials cache failed, try popping the dialog if we are allowed to
            // If the cache had a principal but just didn't have valid tickets, use that principal
            err = KLAcquireNewInitialTickets (ccachePrincipal, inLoginOptions, &gotPrincipal, NULL);
            
            // This code here because we need to request a ccache name for a specific version
            if (!err) {
                KLCCache ccache = NULL;
                
                err = __KLGetFirstCCacheForPrincipal (gotPrincipal, &ccache);
                
                if (!err) {
                    err = __KLGetNameForCCacheVersion (ccache, inKerberosVersion, &gotCCacheName);                    
                }
                
                if (ccache != NULL) { __KLCloseCCache (ccache); }
            }
        } 
    }

    if (err == klNoErr) {
        if (outPrincipal != NULL) {
            *outPrincipal = gotPrincipal;
            gotPrincipal = NULL;
        }
        if (outCacheName != NULL) {
            *outCacheName = gotCCacheName;
            gotCCacheName = NULL;
        }
    }
    
    if (gotPrincipal    != NULL) { KLDisposePrincipal (gotPrincipal); }
    if (gotCCacheName   != NULL) { KLDisposeString (gotCCacheName); }
    if (ccache          != NULL) { __KLCloseCCache (ccache); }
    if (ccachePrincipal != NULL) { KLDisposePrincipal (ccachePrincipal); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLAcquireNewKerberos4TicketsFromKerberos5Tickets (KLPrincipal    inPrincipal,
                                                             KLPrincipal   *outPrincipal,
                                                             char         **outCredCacheName)
{
    KLStatus         lockErr = __KLLockCCache (kWriteLock);
    KLStatus         err = lockErr;
    KLCCache         ccache = NULL;
    KLCCache         newCCache = NULL;
    char            *newCCacheName = NULL;
    KLPrincipal      ccachePrincipal = NULL;
    char            *ccacheName = NULL;
    krb5_context     context = NULL;
    krb5_creds       *v5Creds = NULL;
    CREDENTIALS      v4Creds;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLGetPrincipalAndNameForCCache (ccache, &ccachePrincipal, &ccacheName);
    }
    
    if (err == klNoErr) {
        if (!__KLPrincipalHasKerberos4 (ccachePrincipal)) { err = KLError_ (klBadPrincipalErr); }
    }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }

    if (err == klNoErr) {
        err = __KLGetValidV5TgtForCCache (ccache, &v5Creds);
    }
    
    if (err == klNoErr) {
        err = __KLAcquireKerberos4CredentialsFromKerberos5Credentials (context, v5Creds, &v4Creds);
    }

    if (err == klNoErr) {
        err = __KLCreateNewCCacheWithCredentials (ccachePrincipal, v5Creds, &v4Creds, &newCCache);
    }

    if (err == klNoErr) {
        err = __KLGetNameForCCache (newCCache, &newCCacheName);
    }

    if (err == klNoErr) {
        err = __KLCallLoginPlugin (kKLN_PasswordLogin, newCCacheName);
    }

    if (err == klNoErr) {
        err = __KLMoveCCache (newCCache, ccache);
        if (err == klNoErr) { newCCache = NULL; }  // cc_ccache_move invalidates source ccache
    }

    if (err == klNoErr) {
        if (outPrincipal != NULL) {
            *outPrincipal = ccachePrincipal;
            ccachePrincipal = NULL;
        }
        if (outCredCacheName != NULL) {
            *outCredCacheName = ccacheName;
            ccacheName = NULL;
        }
    }

    if (ccachePrincipal != NULL) { KLDisposePrincipal (ccachePrincipal); }
    if (ccacheName      != NULL) { KLDisposeString (ccacheName); }
    if (newCCacheName   != NULL) { KLDisposeString (newCCacheName); }
    if (newCCache       != NULL) { __KLDestroyCCache (newCCache); }
    if (ccache          != NULL) { __KLCloseCCache (ccache); }
    if (v5Creds         != NULL) { krb5_free_creds (context, v5Creds); }
    if (context         != NULL) { krb5_free_context (context); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLAcquireTickets (KLPrincipal   inPrincipal,
                           KLPrincipal  *outPrincipal,
                           char        **outCredCacheName)
{
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    return KLAcquireInitialTickets (inPrincipal, NULL, outPrincipal, outCredCacheName);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewTickets (KLPrincipal   inPrincipal,
                              KLPrincipal  *outPrincipal,
                              char        **outCredCacheName)
{
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    return KLAcquireNewInitialTickets (inPrincipal, NULL, outPrincipal, outCredCacheName);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireTicketsWithPassword (KLPrincipal      inPrincipal,
                                       KLLoginOptions   inLoginOptions,
                                       const char      *inPassword,
                                       char           **outCredCacheName)
{
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    return KLAcquireInitialTicketsWithPassword (inPrincipal, inLoginOptions, inPassword, outCredCacheName);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewTicketsWithPassword (KLPrincipal     inPrincipal,
                                          KLLoginOptions  inLoginOptions,
                                          const char     *inPassword,
                                          char          **outCredCacheName)
{
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    return KLAcquireNewInitialTicketsWithPassword (inPrincipal, inLoginOptions, inPassword, outCredCacheName);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLAcquireInitialTickets (KLPrincipal      inPrincipal,
                                  KLLoginOptions   inLoginOptions,
                                  KLPrincipal     *outPrincipal,
                                  char           **outCredCacheName)
{
    KLStatus  lockErr = __KLLockCCache (kWriteLock);
    KLStatus  err = lockErr;
    KLCCache  ccache = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (ccache, kerberosVersion_All);
        if (err == klNoErr) {
            err = __KLGetPrincipalAndNameForCCache (ccache, outPrincipal, outCredCacheName);
        } else {
            if (err == klCredentialsExpiredErr) {  // Try renewing the tickets:
                err = KLRenewInitialTickets (inPrincipal, inLoginOptions, outPrincipal, outCredCacheName);
            } else if (err == klCredentialsBadAddressErr) { // Try getting new krb4 tickets
                err = __KLAcquireNewKerberos4TicketsFromKerberos5Tickets (inPrincipal, outPrincipal, outCredCacheName);
            }
        }
    }

    if (err != klNoErr) {
        // If inPrincipal == nil, the user may not get tickets for the same principal, 
        // but we don't want to force them to because this is the case where an 
        // application is starting up and another user left tickets on the machine.
        // After the application starts up we will be storing the principal so we 
        // can pass that in... (ie: in KClient sessions and krb5 contexts)
        err = KLAcquireNewInitialTickets (inPrincipal, inLoginOptions, outPrincipal, outCredCacheName);
    }
    
    if (ccache != NULL) { __KLCloseCCache (ccache); }
    
    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireInitialTicketsWithPassword (KLPrincipal     inPrincipal,
                                              KLLoginOptions  inLoginOptions,
                                              const char     *inPassword,
                                              char          **outCredCacheName)
{
    KLStatus lockErr = __KLLockCCache (kWriteLock);
    KLStatus err = lockErr;
    KLCCache ccache = NULL;
	
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());

    if (err == klNoErr) {
        if (inPrincipal == NULL)                             { err = KLError_ (klBadPrincipalErr); }
        if ((inPassword == NULL) || (inPassword[0] == '\0')) { err = KLError_ (klBadPasswordErr); }
    }
    
    if (err == klNoErr) {
        err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
    }
    
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (ccache, kerberosVersion_All);
        if (err == klNoErr) {
            if (outCredCacheName != NULL) {
                err = __KLGetNameForCCache (ccache, outCredCacheName);
            }
        } else {
            if (err == klCredentialsExpiredErr) {  // Try renewing the tickets:
                err = KLRenewInitialTickets (inPrincipal, inLoginOptions, NULL, outCredCacheName);
            } else if (err == klCredentialsBadAddressErr) { // Try getting new krb4 tickets
                err = __KLAcquireNewKerberos4TicketsFromKerberos5Tickets (inPrincipal, NULL, outCredCacheName);
            }
        }
    }
		
    // Either tickets have expired or there are none: get tickets
    if (err != klNoErr) {
        err = KLAcquireNewInitialTicketsWithPassword (inPrincipal, inLoginOptions, inPassword, outCredCacheName); 
    }

    if (ccache != NULL) { __KLCloseCCache (ccache); }
    
    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewInitialTicketsWithPassword (KLPrincipal     inPrincipal,
                                                 KLLoginOptions  inLoginOptions,
                                                 const char     *inPassword,
                                                 char          **outCredCacheName)
{
    KLStatus            lockErr = __KLLockCCache (kWriteLock);
    KLStatus            err = lockErr;
    krb5_context        context = NULL;
    KLBoolean           gotKrb5 = false;
    krb5_creds          v5Creds;
    KLBoolean           gotKrb4 = false;
    CREDENTIALS         v4Creds;
    
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL)                             { err = KLError_ (klBadPrincipalErr); }
        if ((inPassword == NULL) || (inPassword[0] == '\0')) { err = KLError_ (klBadPasswordErr); }
    }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }

    if (err == klNoErr) {
        err = KLAcquireNewInitialTicketCredentialsWithPassword (inPrincipal, inLoginOptions, inPassword, context,
                                                                &gotKrb4, &gotKrb5, &v4Creds, &v5Creds);
    }

    if (err == klNoErr) {
        err = KLStoreNewInitialTicketCredentials (inPrincipal, context,
                                                  gotKrb4 ? &v4Creds : NULL,
                                                  gotKrb5 ? &v5Creds : NULL,
                                                  outCredCacheName);
    }

    if (gotKrb5        ) { krb5_free_cred_contents (context, &v5Creds); }
    if (context != NULL) { krb5_free_context (context); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewInitialTicketCredentialsWithPassword (KLPrincipal         inPrincipal,
                                                           KLLoginOptions      inLoginOptions,
                                                           const char         *inPassword,
                                                           krb5_context        inV5Context,
                                                           KLBoolean          *outGotV4Credentials,
                                                           KLBoolean          *outGotV5Credentials,
                                                           CREDENTIALS        *outV4Credentials,
                                                           krb5_creds 	      *outV5Credentials)
{
    KLStatus            err = klNoErr;
    KLLoginOptions      options = NULL;
    krb5_context        context = NULL;

    KLBoolean		getKrb4 = __KLPrincipalHasKerberos4 (inPrincipal);
    KLBoolean		useKrb524 = __KLPrincipalShouldUseKerberos524Protocol (inPrincipal);
    KLBoolean		gotKrb4 = false;

    KLBoolean		getKrb5 = __KLPrincipalHasKerberos5 (inPrincipal);
    KLBoolean		gotKrb5 = false;

    CREDENTIALS		v4Credentials;
    krb5_creds		v5Credentials;
    KLBoolean		freeV5Creds = false;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    // Parameter check
    if (inPrincipal  == NULL)                                { err = KLError_ (klBadPrincipalErr); }
    if ((inPassword  == NULL) || (inPassword[0] == '\0'))    { err = KLError_ (klBadPasswordErr); }
    if ((inV5Context == NULL) && (outV5Credentials != NULL)) { err = KLError_ (klBadV5ContextErr); }
    if (!getKrb4 && !getKrb5)                                { err = KLError_ (klRealmDoesNotExistErr); }

    // Create the login options if the caller didn't pass one in
    if (err == klNoErr) {
        if (inLoginOptions == NULL) {
            err = KLCreateLoginOptions (&options);
        } else {
            options = inLoginOptions;
        }
    }

    // Create the krb5 context if the caller didn't pass one in
    if (err == klNoErr) {
        if (inV5Context == NULL) {
            err = krb5_init_context (&context);
        } else {
            context = inV5Context;
        }
    }

    if (err == klNoErr) {
        if (getKrb5) {
            err = krb5_get_init_creds_password (context,  // Use caller's context if returning creds
                                                &v5Credentials,
                                                __KLGetKerberos5PrincipalFromPrincipal (inPrincipal),
                                                (char *) inPassword, __KLPrompter, NULL,
                                                __KLLoginOptionsGetStartTime (options),
                                                __KLLoginOptionsGetServiceName (options) /* NULL == krbtgt */,
                                                __KLLoginOptionsGetKerberos5Options (options));
            dprintf ("krb5_get_init_creds_password returned %d (%s)\n", err, error_message (err));
            if (err == klNoErr) {
                gotKrb5 = freeV5Creds = true;
            }
        }
    }

    if (gotKrb5 && getKrb4 && useKrb524) {
        err = __KLAcquireKerberos4CredentialsFromKerberos5Credentials (context, &v5Credentials, &v4Credentials);
        if (err == klNoErr) {
            gotKrb4 = true;
        } else {
            err = klNoErr;  // Don't abort here... the site might not have a krb524d
        }
    }
    
    if (getKrb4 && !gotKrb4) {
        char *name = NULL;
        char *instance = NULL;
        char *realm = NULL;
    
        if (err == klNoErr) {
            err = __KLGetTripletFromPrincipal (inPrincipal, kerberosVersion_V4, &name, &instance, &realm);
        }

        if (err == klNoErr) {
            char *v4ServiceName = __KLLoginOptionsGetServiceName (options) ? 
                                  __KLLoginOptionsGetServiceName (options) : (char *) KRB_TICKET_GRANTING_TICKET;
            krb5_get_init_creds_opt *opt = __KLLoginOptionsGetKerberos5Options (options);
                        
            err = krb_get_pw_in_tkt_creds(name, instance, realm,
                                          v4ServiceName, realm,
                                          krb_time_to_life (0, opt->tkt_life),
                                          (char *) inPassword,
                                          &v4Credentials);
            err = __KLRemapKerberos4Error (err);
            dprintf ("krb_get_pw_in_tkt_creds returned %d (%s)\n", err, error_message (err));
            if (err == klNoErr) { gotKrb4 = true; }
        }

        // free the v4 principal info
        if (name != NULL)     { KLDisposeString (name); }
        if (instance != NULL) { KLDisposeString (instance); }
        if (realm != NULL)    { KLDisposeString (realm); }
    }
    
    // Tell the caller if we got credentials for them.
    if (err == klNoErr) {
        if (outGotV4Credentials != NULL) {
            *outGotV4Credentials = gotKrb4;
        }

        if (outGotV5Credentials != NULL) {
            *outGotV5Credentials = gotKrb5;
        }

        if (gotKrb4 && (outV4Credentials != NULL)) {
            *outV4Credentials = v4Credentials;
        }

        if (gotKrb5 && (outV5Credentials != NULL)) {
            *outV5Credentials = v5Credentials;
            freeV5Creds = false;
        } 
    }

    if (freeV5Creds                                  ) { krb5_free_cred_contents (context, &v5Credentials); }
    if ((inV5Context    == NULL) && (context != NULL)) { krb5_free_context (context); }
    if ((inLoginOptions == NULL) && (options != NULL)) { KLDisposeLoginOptions (options); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLStoreNewInitialTicketCredentials (KLPrincipal     inPrincipal,
                                             krb5_context    inV5Context,
                                             CREDENTIALS    *inV4Credentials,
                                             krb5_creds     *inV5Credentials,
                                             char          **outCredCacheName)
{
    KLStatus  lockErr = __KLLockCCache (kWriteLock);
    KLStatus  err = lockErr;
    KLCCache  principalCCache = NULL;
    KLCCache  newCCache = NULL;
    char     *newCCacheName = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if ((inV5Context     == NULL) && (inV5Credentials != NULL)) { err = KLError_ (klBadV5ContextErr); }
        if ((inV4Credentials == NULL) && (inV5Credentials == NULL)) { err = KLError_ (klParameterErr); }
    }

    if (err == klNoErr) {
        // check to see if there is already a ccache for this principal
        __KLGetFirstCCacheForPrincipal (inPrincipal, &principalCCache);
    }

    if (err == klNoErr) {
        err = __KLCreateNewCCacheWithCredentials (inPrincipal, inV5Credentials, inV4Credentials, &newCCache);
    }

    if (err == klNoErr) {
        err = __KLGetNameForCCache (newCCache, &newCCacheName);
    }

    if (err == klNoErr) {
        err = __KLCallLoginPlugin (kKLN_PasswordLogin, newCCacheName);
    }

    if (err == klNoErr) {
        if (principalCCache != NULL) {
            err = __KLMoveCCache (newCCache, principalCCache);  // Already a cache for that principal

            if (err == klNoErr) {
                newCCache = NULL;  // cc_ccache_move invalidates source ccache -- don't double release

                if (newCCacheName != NULL) {
                    KLDisposeString (newCCacheName);
                    newCCacheName = NULL;
                }
                err = __KLGetNameForCCache (principalCCache, &newCCacheName);  // remember the new name
            }
        }
    }

    if (err == klNoErr) {
        if (outCredCacheName != NULL) {
            *outCredCacheName = newCCacheName;
            newCCacheName = NULL;  // Don't free it if we are returning it.
        }
        if (newCCache != NULL) { __KLCloseCCache (newCCache); }
    } else {
        if (newCCache != NULL) { __KLDestroyCCache (newCCache); }
    }

    if (newCCacheName   != NULL) { KLDisposeString (newCCacheName); }
    if (principalCCache != NULL) { __KLCloseCCache (principalCCache); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }

    return KLError_ (err);    
}


#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLVerifyInitialTickets (KLPrincipal   inPrincipal,
                                 KLBoolean     inFailIfNoHostKey,
                                 char        **outCredCacheName)
{
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    return __KLVerifyInitialTickets (inPrincipal, inFailIfNoHostKey, NULL, outCredCacheName);
}

KLStatus __KLVerifyInitialTickets (KLPrincipal   inPrincipal,
                                   KLBoolean     inFailIfNoHostKey,
                                   KLPrincipal  *outPrincipal,
                                   char        **outCredCacheName)
{
    KLStatus            lockErr = __KLLockCCache (kReadLock);
    KLStatus            err = lockErr;
    CREDENTIALS        	v4Credentials;
    CREDENTIALS        *v4CredentialsPtr = NULL;
    krb5_creds 	       *v5CredentialsPtr = NULL;
    KLCCache            ccache = NULL;
    krb5_context        context = NULL;
    KLPrincipal         ccachePrincipal = NULL;
    char               *ccacheName = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }

    if (err == klNoErr) {
        err = __KLGetPrincipalAndNameForCCache (ccache, &ccachePrincipal, &ccacheName);
    }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }

    // Check for v5 credentials
    if (err == klNoErr) {
        err = __KLGetValidV5TgtForCCache (ccache, &v5CredentialsPtr);
        if (err) { err = klNoErr; }  // OK if no v5 creds
    }
    
    // Check for v4 credentials
    if (err == klNoErr) {
        if (__KLGetValidV4TgtForCCache (ccache, &v4Credentials) == klNoErr) {
            v4CredentialsPtr = &v4Credentials;
        } 
    }

    if (err == klNoErr) {
        err = KLVerifyInitialTicketCredentials (v4CredentialsPtr, v5CredentialsPtr, inFailIfNoHostKey);
    }

    if (err == klNoErr) {
        if (outCredCacheName != NULL) {
            *outCredCacheName = ccacheName;
            ccacheName = NULL;
        }
        if (outPrincipal != NULL) {
            *outPrincipal = ccachePrincipal;
            ccachePrincipal = NULL;
        }        
    }

    if (ccachePrincipal  != NULL) { KLDisposePrincipal (ccachePrincipal); }
    if (ccacheName       != NULL) { KLDisposeString (ccacheName); }
    if (v5CredentialsPtr != NULL) { krb5_free_creds (context, v5CredentialsPtr); }
    if (context          != NULL) { krb5_free_context (context); }
    if (ccache           != NULL) { __KLCloseCCache (ccache); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLVerifyInitialTicketCredentials (
        CREDENTIALS        *inV4Credentials,
        krb5_creds         *inV5Credentials,
        KLBoolean           inFailIfNoHostKey)
{
    KLStatus lockErr = __KLLockCCache (kReadLock);
    KLStatus err = lockErr;
    krb5_context context = NULL;
  
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inV4Credentials == NULL && inV5Credentials == NULL) { err = KLError_ (klParameterErr); }
    }

    if (err == klNoErr) {
        err = krb5_init_secure_context (&context);
    }

    if (inV5Credentials != NULL) {
        krb5_verify_init_creds_opt options;
        
        if (err == klNoErr) {
            // That's "no key == fail" not "no fail" ;-)
            krb5_verify_init_creds_opt_init (&options);
            krb5_verify_init_creds_opt_set_ap_req_nofail (&options, inFailIfNoHostKey);
        }
        
        if (err == klNoErr) {
            err = krb5_verify_init_creds (context,  inV5Credentials, 
                                          NULL /* default server principal */,
                                          NULL /* default keytab location */,
                                          NULL /* don't store creds in ccache */,
                                          &options);
            dprintf ("krb5_verify_init_creds('host/<hostname>') returned error = %d\n", err);

            if (err != klNoErr) {
                krb5_keytab       keytab = NULL;
                krb5_kt_cursor    cursor = NULL;
                krb5_keytab_entry entry;
                KLBoolean         freeKTEntry = false;
    
                err = klNoErr;  // Try first key in the keytab if there is one
    
                if (err == klNoErr) {
                    err = krb5_kt_default (context, &keytab);
                }
    
                if (err == klNoErr) {
                    err = krb5_kt_start_seq_get (context, keytab, &cursor);
                }
    
                if (err == klNoErr) {
                    err = krb5_kt_next_entry (context, keytab, &entry, &cursor);  // get 1st entry
                    freeKTEntry = (err == klNoErr); // remember to free later
                }
    
                if (err == klNoErr) {
                    err = krb5_verify_init_creds (context, inV5Credentials,
                                                  entry.principal /* get principal for the 1st entry */, 
                                                  NULL /* default keytab location */,
                                                  NULL /* don't store creds in ccache */,
                                                  &options);
                    dprintf ("krb5_verify_init_creds('<first keytab entry>') returned error = %d\n", err);
                } else {
#warning "Some of this logic should be in krb5_verify_init_creds()"
                    if (!inFailIfNoHostKey) {
                        err = klNoErr;  /* Don't fail if no keytab */
                    }
                }
    
                if (cursor != NULL) { krb5_kt_end_seq_get (context, keytab, &cursor); }
                if (freeKTEntry   ) { krb5_free_keytab_entry_contents (context, &entry); }
                if (keytab != NULL) { krb5_kt_close (context, keytab); }
            }
        }
    } else if (inV4Credentials != NULL) {
        KLBoolean       haveSrvtab = false;
        char            hostname [MAXHOSTNAMELEN];
        char            phost [MAXHOSTNAMELEN];
        char            lrealm [REALM_SZ];
        struct in_addr  addr;
        KLCCache     tempCCache = NULL;
        char           *tempCCacheName = NULL;
        KLPrincipal     v4Principal = NULL;
        char           *saveTktString = NULL;
        KTEXT_ST        ticket;
        AUTH_DAT        authdata;
        
        if (err == klNoErr) {
            err = krb_get_lrealm (lrealm, 1);
            err = __KLRemapKerberos4Error (err);
        }
        
        if (err == klNoErr) {
            int fd = -1;

            // Check to see if we can read the srvtab
            fd = open (KEYFILE, O_RDONLY, 0);
            if ((fd < 0) && inFailIfNoHostKey) {
                err = KLError_ (errno);
            } else {
                haveSrvtab = true;
                close (fd);
            }
        }
        
        if (err == klNoErr) {
            // Get the local hostname (which we need for the rcmd key)
            err = gethostname (hostname, sizeof (hostname));
            if (err) { err = KLError_ (klNoHostnameErr); }
        }
        
        if (err == klNoErr) {
            // Get address information for the local host
            struct hostent *hp = gethostbyname (hostname);
            if (hp == NULL) { 
                err = KLError_ (klNoHostnameErr); 
            } else {
                memcpy ((char *) &addr, (char *) hp->h_addr, sizeof (addr));
            }
        }
        
        if (err == klNoErr) {
            // Get the host principal
            char *krb_phost = krb_get_phost (hostname);
            if (krb_phost == NULL) { 
                err = KLError_ (klMemFullErr);
            } else {
                strncpy (phost, krb_phost, sizeof (phost));
                phost[sizeof (phost) - 1] = 0;
            }
        }
        
        if (err == klNoErr) {
            err = __KLCreatePrincipalFromTriplet (inV4Credentials->pname,
                                                  inV4Credentials->pinst,
                                                  inV4Credentials->realm,
                                                  kerberosVersion_V4,
                                                  &v4Principal);
        }

        if (err == klNoErr) {
            err = __KLCreateNewCCacheWithCredentials (v4Principal, NULL, inV4Credentials, &tempCCache);
        }
    
        if (err == klNoErr) {
            err = __KLGetNameForCCache (tempCCache, &tempCCacheName);
        }
        
        if (err == klNoErr) {
            err = __KLCreateString (tkt_string (), &saveTktString);
        }
        
        if (err == klNoErr) {
            krb_set_tkt_string (tempCCacheName);
        }
        
        if (err == klNoErr) {
            err = krb_mk_req (&ticket, KRB_REMOTE_COMMAND_TICKET, phost, lrealm, 0);
            err = __KLRemapKerberos4Error (err);
            dprintf ("krb_mk_req returned %d (%s)\n", err, error_message (err));
            
            if (err == KRBET_KDC_PR_UNKNOWN) {
                // Unknown host key...
                if (inFailIfNoHostKey || haveSrvtab) {
                    // Fail here if the caller asked us to fail in this case or
                    // if there is a srvtab so the user is probably faking us out
                    err = KLError_ (err);
                } else {
                    // Otherwise no srvtab is okay...
                    err = klNoErr;
                }
            } else if (err == klNoErr) {
                // krb_mk_req succeeded... Now that we have a host ticket, try to use it
                err = krb_rd_req (&ticket, KRB_REMOTE_COMMAND_TICKET, phost, addr.s_addr, &authdata, "");
                err = __KLRemapKerberos4Error (err);
                dprintf ("krb_rd_req returned %d (%s)\n", err, error_message (err));
                
                if ((err == KRBET_RD_AP_UNDEC) && !haveSrvtab && !inFailIfNoHostKey) {
                    // No host key is okay
                    err = klNoErr;
                }
            }
        }
        
        memset (&ticket, 0, sizeof (ticket));     // Don't leave in memory!
        memset (&authdata, 0, sizeof (authdata)); // Don't leave in memory!
        
        if (v4Principal    != NULL) { KLDisposePrincipal (v4Principal); }
        if (saveTktString  != NULL) { krb_set_tkt_string (saveTktString); 
                                      KLDisposeString (saveTktString); }
        if (tempCCache     != NULL) { __KLDestroyCCache (tempCCache); }
        if (tempCCacheName != NULL) { KLDisposeString (tempCCacheName); }
    }

    if (context != NULL) { krb5_free_context (context); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}


#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewInitialTicketsWithKeytab (KLPrincipal             inPrincipal,
                                               KLLoginOptions          inLoginOptions,
                                               const char             *inKeytabName,
                                               char                  **outCredCacheName)
{
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    return __KLAcquireNewInitialTicketsWithKeytab (inPrincipal, inLoginOptions, inKeytabName, NULL, outCredCacheName);
}

KLStatus __KLAcquireNewInitialTicketsWithKeytab (KLPrincipal             inPrincipal,
                                                 KLLoginOptions          inLoginOptions,
                                                 const char             *inKeytabName,
                                                 KLPrincipal            *outPrincipal,
                                                 char                  **outCredCacheName)
{
    KLStatus          lockErr = __KLLockCCache (kWriteLock);
    KLStatus          err = lockErr;
    KLLoginOptions    options = NULL;
    krb5_principal    principal = NULL;
    KLPrincipal       keytabPrincipal = NULL;
    krb5_context      context = NULL;
    krb5_keytab       keytab = NULL;
    krb5_keytab_entry entry;
    KLBoolean         freeKTEntry = false;
    KLBoolean         useKrb524 = false;
    KLBoolean         gotKrb4 = false;
    krb5_creds        v5Creds;
    KLBoolean         freeV5Creds = false;
    CREDENTIALS       v4Creds;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inLoginOptions == NULL) {
            err = KLCreateLoginOptions (&options);
        } else {
            options = inLoginOptions;
        }
    }

    if (err == klNoErr) {
        err = krb5_init_context (&context);
    }

    if (err == klNoErr) {
        if (inKeytabName != NULL) {
            err = krb5_kt_resolve (context, inKeytabName, &keytab);
        } else {
            err = krb5_kt_default (context, &keytab);
        }
    }

    if (inPrincipal == NULL) {
        krb5_kt_cursor cursor = NULL;

        if (err == klNoErr) {
            err = krb5_kt_start_seq_get (context, keytab, &cursor);
        }
        
        if (err == klNoErr) {
            err = krb5_kt_next_entry (context, keytab, &entry, &cursor);  // get 1st entry
            freeKTEntry = (err == klNoErr); // remember to free later
        }

        if (err == klNoErr) {
            principal = entry.principal;  // get principal for the 1st entry
        }

        if (cursor != NULL) { krb5_kt_end_seq_get (context, keytab, &cursor); }
    } else {
        if (err == klNoErr) {
            principal = __KLGetKerberos5PrincipalFromPrincipal (inPrincipal);
        }
    }
    
    if (err == klNoErr) {
        err = krb5_get_init_creds_keytab (context, &v5Creds,
                                          principal,
                                          keytab,  // NULL == default keytab location
                                          __KLLoginOptionsGetStartTime (options), 
                                          __KLLoginOptionsGetServiceName (options),
                                          __KLLoginOptionsGetKerberos5Options (options));
        dprintf ("krb5_get_init_creds_keytab returned %d (%s)\n", err, error_message (err));
        freeV5Creds = (err == klNoErr); // remember we need to free the contents of the creds
    }

    if (err == klNoErr) {
        err = __KLCreatePrincipalFromKerberos5Principal (principal, &keytabPrincipal);
    }
    
    if (err == klNoErr) {
        useKrb524 = __KLPrincipalShouldUseKerberos524Protocol (keytabPrincipal);
    }

    if (useKrb524) {
        if (err == klNoErr) {
            err = __KLAcquireKerberos4CredentialsFromKerberos5Credentials (context, &v5Creds, &v4Creds);
            if (err == klNoErr) {
                gotKrb4 = true;
            } else {
                err = klNoErr;  // Don't abort here... the site might not have a krb524d
            }
        }
    }

    if (err == klNoErr) {
        err = KLStoreNewInitialTicketCredentials (keytabPrincipal, context,
                                                  gotKrb4 ? &v4Creds : NULL,
                                                  &v5Creds, outCredCacheName);
    }

    if (err == klNoErr) {
        if (outPrincipal != NULL) {
            *outPrincipal = keytabPrincipal;
            keytabPrincipal = NULL;
        }
    }
    
    if (keytab          != NULL) { krb5_kt_close (context, keytab); }
    if (freeKTEntry            ) { krb5_free_keytab_entry_contents (context, &entry); }
    if (freeV5Creds            ) { krb5_free_cred_contents (context, &v5Creds); }
    if (context         != NULL) { krb5_free_context (context); }
    if (keytabPrincipal != NULL) { KLDisposePrincipal (keytabPrincipal); }
    if ((inLoginOptions == NULL) && (options != NULL)) { KLDisposeLoginOptions (options); }
    
    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLRenewInitialTickets (KLPrincipal     inPrincipal,
                                KLLoginOptions  inLoginOptions,
                                KLPrincipal    *outPrincipal,
                                char          **outCredCacheName)
{
    KLStatus       lockErr = __KLLockCCache (kWriteLock);
    KLStatus       err = lockErr;
    KLLoginOptions options = NULL;
    KLCCache       ccache = NULL;
    KLPrincipal    ccachePrincipal = NULL;
    char          *ccacheName = NULL;
    KLCCache       newCCache = NULL;
    char          *newCCacheName = NULL;
    krb5_context   v5Context = NULL;
    krb5_ccache    v5CCache = NULL;
    KLBoolean      useKrb524 = false;
    KLBoolean      gotKrb4 = false;
    krb5_creds     v5Creds;
    KLBoolean      freeV5Creds = false;
    CREDENTIALS    v4Creds;
    
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        if (inLoginOptions == NULL) {
            err = KLCreateLoginOptions (&options);
        } else {
            options = inLoginOptions;
        }
    }

    if (err == klNoErr) {
        err = __KLGetPrincipalAndNameForCCache (ccache, &ccachePrincipal, &ccacheName);
    }
    
    if (err == klNoErr) {
        err = __KLGetKrb5CCacheAndContextForCCache (ccache, &v5CCache, &v5Context);
    }
    
    if (err == klNoErr) {
        err = krb5_get_renewed_creds (v5Context, &v5Creds,
                                      __KLGetKerberos5PrincipalFromPrincipal (ccachePrincipal), 
                                      v5CCache, __KLLoginOptionsGetServiceName (options));
        dprintf ("krb5_get_renewed_creds returned %d (%s)\n", err, error_message (err));
        freeV5Creds = (err == klNoErr); // remember we need to free the contents of the creds
    }
    
    if (err == klNoErr) {
        useKrb524 = __KLPrincipalShouldUseKerberos524Protocol (ccachePrincipal);
    }

    if (useKrb524) {
        if (err == klNoErr) {
            err = __KLAcquireKerberos4CredentialsFromKerberos5Credentials (v5Context, &v5Creds, &v4Creds);
            if (err == klNoErr) {
                gotKrb4 = true;
            } else {
                err = klNoErr;  // Don't abort here... the site might not have a krb524d
            }
        }
    }
    
    if (err == klNoErr) {
        err = __KLCreateNewCCacheWithCredentials (ccachePrincipal, &v5Creds, gotKrb4 ? &v4Creds : NULL, &newCCache);
    }

    if (err == klNoErr) {
        err = __KLGetNameForCCache (newCCache, &newCCacheName);
    }

    if (err == klNoErr) {
        err = __KLCallLoginPlugin (kKLN_PasswordLogin, newCCacheName);
    }

    if (err == klNoErr) {
        err = __KLMoveCCache (newCCache, ccache);
        if (err == klNoErr) { newCCache = NULL; }  // cc_ccache_move invalidates source ccache
    }

    if (err == klNoErr) {
        if (outPrincipal != NULL) {
            *outPrincipal = ccachePrincipal;
            ccachePrincipal = NULL;
        }
        if (outCredCacheName != NULL) {
            *outCredCacheName = ccacheName;
            ccacheName = NULL;
        }
    }
    
    if (freeV5Creds            ) { krb5_free_cred_contents (v5Context, &v5Creds); }
    if (ccachePrincipal != NULL) { KLDisposePrincipal (ccachePrincipal); }
    if (ccacheName      != NULL) { KLDisposeString (ccacheName); }
    if (newCCacheName   != NULL) { KLDisposeString (newCCacheName); }
    if (newCCache       != NULL) { __KLDestroyCCache (newCCache); }  // since we always replace, this only happens on error
    if (ccache          != NULL) { __KLCloseCCache (ccache); }
    if ((inLoginOptions == NULL) && (options != NULL)) { KLDisposeLoginOptions (options); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLValidateInitialTickets (KLPrincipal      inPrincipal,
                                   KLLoginOptions   inLoginOptions,
                                   char           **outCredCacheName)
{
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    return __KLValidateInitialTickets (inPrincipal, inLoginOptions, NULL, outCredCacheName);
}

KLStatus __KLValidateInitialTickets (KLPrincipal      inPrincipal,
                                     KLLoginOptions   inLoginOptions,
                                     KLPrincipal     *outPrincipal,
                                     char           **outCredCacheName)
{
    KLStatus       lockErr = __KLLockCCache (kWriteLock);
    KLStatus       err = lockErr;
    KLLoginOptions options = NULL;
    KLCCache       ccache = NULL;
    KLPrincipal    ccachePrincipal = NULL;
    char          *ccacheName = NULL;
    KLCCache       newCCache = NULL;
    char          *newCCacheName = NULL;
    krb5_context   v5Context = NULL;
    krb5_ccache    v5CCache = NULL;
    KLBoolean      useKrb524 = false;
    KLBoolean      gotKrb4 = false;
    krb5_creds     v5Creds;
    KLBoolean      freeV5Creds = false;
    CREDENTIALS    v4Creds;
    
    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }

    if (err == klNoErr) {
        if (inLoginOptions == NULL) {
            err = KLCreateLoginOptions (&options);
        } else {
            options = inLoginOptions;
        }
    }

    if (err == klNoErr) {
        err = __KLGetPrincipalAndNameForCCache (ccache, &ccachePrincipal, &ccacheName);
    }
    
    if (err == klNoErr) {
        err = __KLGetKrb5CCacheAndContextForCCache (ccache, &v5CCache, &v5Context);
    }

    if (err == klNoErr) {
        err = krb5_get_validated_creds (v5Context, &v5Creds, __KLGetKerberos5PrincipalFromPrincipal (ccachePrincipal), 
                                        v5CCache, __KLLoginOptionsGetServiceName (options));
        dprintf ("krb5_get_validated_creds returned %d (%s)\n", err, error_message (err));
        freeV5Creds = (err == klNoErr); // remember we need to free the contents of the creds
    }
    
    if (err == klNoErr) {
        useKrb524 = __KLPrincipalShouldUseKerberos524Protocol (ccachePrincipal);
    }

    if (useKrb524) {
        if (err == klNoErr) {
            err = __KLAcquireKerberos4CredentialsFromKerberos5Credentials (v5Context, &v5Creds, &v4Creds);
            if (err == klNoErr) {
                gotKrb4 = true;
            } else {
                err = klNoErr;  // Don't abort here... the site might not have a krb524d
            }
        }
    }
    
    if (err == klNoErr) {
        err = __KLCreateNewCCacheWithCredentials (ccachePrincipal, &v5Creds, gotKrb4 ? &v4Creds : NULL, &newCCache);
    }

    if (err == klNoErr) {
        err = __KLGetNameForCCache (newCCache, &newCCacheName);
    }

    if (err == klNoErr) {
        err = __KLCallLoginPlugin (kKLN_PasswordLogin, newCCacheName);
    }

    if (err == klNoErr) {
        err = __KLMoveCCache (newCCache, ccache);
        if (err == klNoErr) { newCCache = NULL; }  // cc_ccache_move invalidates source ccache
    }

    if (err == klNoErr) {
        if (outPrincipal != NULL) {
            *outPrincipal = ccachePrincipal;
            ccachePrincipal = NULL;
        }
        if (outCredCacheName != NULL) {
            *outCredCacheName = ccacheName;
            ccacheName = NULL;
        }
    }
    
    if (freeV5Creds            ) { krb5_free_cred_contents (v5Context, &v5Creds); }
    if (ccache          != NULL) { __KLCloseCCache (ccache); }
    if (ccachePrincipal != NULL) { KLDisposePrincipal (ccachePrincipal); }
    if (ccacheName      != NULL) { KLDisposeString (ccacheName); }
    if (newCCacheName   != NULL) { KLDisposeString (newCCacheName); }
    if (newCCache       != NULL) { __KLDestroyCCache (newCCache); }  // Since we always replace, this only happens on error
    if ((inLoginOptions == NULL) && (options != NULL)) { KLDisposeLoginOptions (options); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLDestroyTickets (KLPrincipal inPrincipal)
{
    KLStatus  lockErr = __KLLockCCache (kWriteLock);
    KLStatus  err = lockErr;
    KLCCache ccache = NULL;
    char *ccacheName = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLGetNameForCCache (ccache, &ccacheName);
    }
    
    if (err == klNoErr) {
        __KLCallLogoutPlugin (ccacheName);
    }

    if (err == klNoErr) {
        err = __KLDestroyCCache (ccache);
        if (err == klNoErr) { ccache = NULL; }  // destroy invalidates pointer
    }
    
    if (ccacheName != NULL) { KLDisposeString (ccacheName); }
    if (ccache     != NULL) { __KLCloseCCache (ccache); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

#pragma mark -

static pthread_mutex_t gLastChangeTimeMutex = PTHREAD_MUTEX_INITIALIZER;
static KLTime          gKLLastChangeTime = 0;

// ---------------------------------------------------------------------------
// Do not lock the CCAPI around this function.  Anyone calling 
// this function is going to call it repeatedly so they will 
// just get the in-progress changes next time they call it.
//
// Locking here would just cause slowdowns when an app is
// calling this repeatedly

KLStatus KLLastChangedTime (KLTime *outLastChangedTime)
{
    KLStatus     err = klNoErr;
    KLTime       addressChangeTime = 0; 
    cc_context_t context = NULL;
    cc_time_t    ccChangeTime = 0;
    
    if (outLastChangedTime == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = cc_initialize (&context, ccapi_version_4, NULL, NULL);
    }

    if (!err) {
        err = cc_context_get_change_time (context, &ccChangeTime);
    }
    
    if (!err) {
        KLStatus lockErr = err = pthread_mutex_lock (&gLastChangeTimeMutex);

        if (!err) {
            addressChangeTime = __KLCheckAddresses ();
        
            if (addressChangeTime > gKLLastChangeTime) { gKLLastChangeTime = addressChangeTime; }
            if (ccChangeTime      > gKLLastChangeTime) { gKLLastChangeTime = ccChangeTime; }
            
            *outLastChangedTime = gKLLastChangeTime;
        }
        
        if (!lockErr) { pthread_mutex_unlock (&gLastChangeTimeMutex); }
    }
    
    if (context != NULL) { cc_context_release (context); }

    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus KLCacheHasValidTickets (KLPrincipal        inPrincipal,
                                 KLKerberosVersion  inKerberosVersion,
                                 KLBoolean         *outFoundValidTickets,
                                 KLPrincipal       *outPrincipal,
                                 char             **outCredCacheName)
{
    KLStatus  lockErr = __KLLockCCache (kReadLock);
    KLStatus  err = lockErr;
    KLCCache ccache = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (outFoundValidTickets == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (ccache, inKerberosVersion);
    }
    
    if (err == klNoErr) {
        err = __KLGetPrincipalAndNameForCCache (ccache, outPrincipal, outCredCacheName);
    }

    // We check for it being NULL because that might be the error
    if (outFoundValidTickets != NULL) {
        *outFoundValidTickets = (err == klNoErr);
    }

    if (ccache != NULL) { __KLCloseCCache (ccache); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus KLTicketStartTime (KLPrincipal        inPrincipal,
                            KLKerberosVersion  inKerberosVersion,
                            KLTime            *outStartTime)
{
    KLStatus  lockErr = __KLLockCCache (kReadLock);
    KLStatus  err = lockErr;
    KLCCache ccache = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (outStartTime == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLGetCCacheStartTime (ccache, inKerberosVersion, outStartTime);
    }
    
    if (ccache != NULL) { __KLCloseCCache (ccache); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}
     

// ---------------------------------------------------------------------------

KLStatus KLTicketExpirationTime (KLPrincipal        inPrincipal,
                                 KLKerberosVersion  inKerberosVersion,
                                 KLTime            *outExpirationTime)
{
    KLStatus  lockErr = __KLLockCCache (kReadLock);
    KLStatus  err = lockErr;
    KLCCache ccache = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (outExpirationTime == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLGetCCacheExpirationTime (ccache, inKerberosVersion, outExpirationTime);
    }
    
    if (ccache != NULL) { __KLCloseCCache (ccache); }

    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

#pragma mark -

KLStatus KLSetSystemDefaultCache (KLPrincipal inPrincipal)
{
    KLStatus  lockErr = __KLLockCCache (kWriteLock);
    KLStatus  err = lockErr;
    KLCCache ccache = NULL;

    dprintf ("Entering %s(): session is %s", __FUNCTION__, LoginSessionGetSecuritySessionName ());
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (err == klNoErr) {
        err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
    }
    
    if (err == klNoErr) {
        err = __KLSetDefaultCCache (&ccache);
    }

    if (ccache != NULL) { __KLCloseCCache (ccache); }
    
    if (lockErr == klNoErr) { __KLUnlockCCache (); }
    
    return KLError_ (err);
}


