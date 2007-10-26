/*
 * KLTicketManagement.c
 *
 * $Header$
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    if (inCacheName       == NULL)               { err = KLError_ (klParameterErr); }
    if (inKerberosVersion == kerberosVersion_V4) { err = KLError_ (klInvalidVersionErr); }

    if (err == klNoErr) {
        err = __KLGetCCacheByName (inCacheName, &ccache);

        if (err == klNoErr) {            
            err = __KLGetPrincipalForCCache (ccache, &ccachePrincipal);
        }

        if (err == klNoErr) {
            err = __KLCacheHasValidTickets (ccache);
            
            if (err == klNoErr) { // Tickets exist!
                err = __KLGetPrincipalForCCache (ccache, &gotPrincipal);
                
                if (!err) {
                    err = __KLGetNameForCCache (ccache, &gotCCacheName);
                }
            } else {
                if (err == klCredentialsExpiredErr) {
                    err = KLRenewInitialTickets (ccachePrincipal, inLoginOptions, &gotPrincipal, &gotCCacheName);
                } else if (err == klCredentialsNeedValidationErr) {
                    err = __KLValidateInitialTickets (ccachePrincipal, inLoginOptions, &gotPrincipal, &gotCCacheName);
                }
            }
        }

        if ((err != klNoErr) && __KLAllowAutomaticPrompting ()) {
            // If searching the credentials cache failed, try popping the dialog if we are allowed to
            // If the cache had a principal but just didn't have valid tickets, use that principal
            err = KLAcquireNewInitialTickets (ccachePrincipal, inLoginOptions, &gotPrincipal, &gotCCacheName);
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

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus KLAcquireTickets (KLPrincipal   inPrincipal,
                           KLPrincipal  *outPrincipal,
                           char        **outCredCacheName)
{
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    return KLAcquireInitialTickets (inPrincipal, NULL, outPrincipal, outCredCacheName);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewTickets (KLPrincipal   inPrincipal,
                              KLPrincipal  *outPrincipal,
                              char        **outCredCacheName)
{
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    return KLAcquireNewInitialTickets (inPrincipal, NULL, outPrincipal, outCredCacheName);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireTicketsWithPassword (KLPrincipal      inPrincipal,
                                       KLLoginOptions   inLoginOptions,
                                       const char      *inPassword,
                                       char           **outCredCacheName)
{
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    return KLAcquireInitialTicketsWithPassword (inPrincipal, inLoginOptions, inPassword, outCredCacheName);
}

// ---------------------------------------------------------------------------

KLStatus KLAcquireNewTicketsWithPassword (KLPrincipal     inPrincipal,
                                          KLLoginOptions  inLoginOptions,
                                          const char     *inPassword,
                                          char          **outCredCacheName)
{
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (ccache);
        if (err == klNoErr) {
            err = __KLGetPrincipalAndNameForCCache (ccache, outPrincipal, outCredCacheName);
        } else {
            if (err == klCredentialsExpiredErr) {  // Try renewing the tickets:
                err = KLRenewInitialTickets (inPrincipal, inLoginOptions, outPrincipal, outCredCacheName);
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
	
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();

    if (err == klNoErr) {
        if (inPrincipal == NULL)                             { err = KLError_ (klBadPrincipalErr); }
        if ((inPassword == NULL) || (inPassword[0] == '\0')) { err = KLError_ (klBadPasswordErr); }
    }
    
    if (err == klNoErr) {
        err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
    }
    
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (ccache);
        if (err == klNoErr) {
            if (outCredCacheName != NULL) {
                err = __KLGetNameForCCache (ccache, outCredCacheName);
            }
        } else {
            if (err == klCredentialsExpiredErr) {  // Try renewing the tickets:
                err = KLRenewInitialTickets (inPrincipal, inLoginOptions, NULL, outCredCacheName);
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
    
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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

    KLBoolean		gotKrb5 = false;
    krb5_creds		v5Credentials;
    KLBoolean		freeV5Creds = false;

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    // Parameter check
    if (inPrincipal  == NULL)                                { err = KLError_ (klBadPrincipalErr); }
    if ((inPassword  == NULL) || (inPassword[0] == '\0'))    { err = KLError_ (klBadPasswordErr); }
    if ((inV5Context == NULL) && (outV5Credentials != NULL)) { err = KLError_ (klBadV5ContextErr); }

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
        err = krb5_get_init_creds_password (context,  // Use caller's context if returning creds
                                            &v5Credentials,
                                            __KLPrincipalGetKerberos5Principal (inPrincipal),
                                            (char *) inPassword, __KLPrompter, NULL,
                                            __KLLoginOptionsGetStartTime (options),
                                            __KLLoginOptionsGetServiceName (options) /* NULL == krbtgt */,
                                            __KLLoginOptionsGetKerberos5Options (options));
        dprintf ("krb5_get_init_creds_password returned %d (%s)\n", err, error_message (err));
        if (err == klNoErr) {
            gotKrb5 = freeV5Creds = true;
        }
    }
    
    // Tell the caller if we got credentials for them.
    if (err == klNoErr) {
        if (outGotV4Credentials != NULL) {
            *outGotV4Credentials = false;
        }

        if (outGotV5Credentials != NULL) {
            *outGotV5Credentials = gotKrb5;
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    if (err == klNoErr) {
        if (inV5Credentials == NULL) { err = KLError_ (klParameterErr); }
        if (inV5Context     == NULL) { err = KLError_ (klBadV5ContextErr); }
    }

    if (err == klNoErr) {
        // check to see if there is already a ccache for this principal
        __KLGetFirstCCacheForPrincipal (inPrincipal, &principalCCache);
    }

    if (err == klNoErr) {
        err = __KLCreateNewCCacheWithCredentials (inPrincipal, inV5Context, inV5Credentials, &newCCache);
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
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    return __KLVerifyInitialTickets (inPrincipal, inFailIfNoHostKey, NULL, outCredCacheName);
}

KLStatus __KLVerifyInitialTickets (KLPrincipal   inPrincipal,
                                   KLBoolean     inFailIfNoHostKey,
                                   KLPrincipal  *outPrincipal,
                                   char        **outCredCacheName)
{
    KLStatus            lockErr = __KLLockCCache (kReadLock);
    KLStatus            err = lockErr;
    krb5_creds 	       *v5CredentialsPtr = NULL;
    KLCCache            ccache = NULL;
    krb5_context        context = NULL;
    KLPrincipal         ccachePrincipal = NULL;
    char               *ccacheName = NULL;

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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
    
    if (err == klNoErr) {
        err = KLVerifyInitialTicketCredentials (NULL, v5CredentialsPtr, inFailIfNoHostKey);
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
    krb5_verify_init_creds_opt options;
  
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    if (err == klNoErr) {
        if (inV5Credentials == NULL) { err = KLError_ (klParameterErr); }
    }

    if (err == klNoErr) {
        err = krb5_init_secure_context (&context);
    }

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
    }
    
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
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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
    krb5_creds        v5Creds;
    KLBoolean         freeV5Creds = false;

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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
            principal = __KLPrincipalGetKerberos5Principal (inPrincipal);
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
        err = KLStoreNewInitialTicketCredentials (keytabPrincipal, context,
                                                  NULL, &v5Creds, outCredCacheName);
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
    krb5_creds     v5Creds;
    KLBoolean      freeV5Creds = false;
    
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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
                                      __KLPrincipalGetKerberos5Principal (ccachePrincipal), 
                                      v5CCache, __KLLoginOptionsGetServiceName (options));
        dprintf ("krb5_get_renewed_creds returned %d (%s)\n", err, error_message (err));
        freeV5Creds = (err == klNoErr); // remember we need to free the contents of the creds
    }
    
    if (err == klNoErr) {
        err = __KLCreateNewCCacheWithCredentials (ccachePrincipal, v5Context, &v5Creds, &newCCache);
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
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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
    krb5_creds     v5Creds;
    KLBoolean      freeV5Creds = false;
    
    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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
        err = krb5_get_validated_creds (v5Context, &v5Creds, __KLPrincipalGetKerberos5Principal (ccachePrincipal), 
                                        v5CCache, __KLLoginOptionsGetServiceName (options));
        dprintf ("krb5_get_validated_creds returned %d (%s)\n", err, error_message (err));
        freeV5Creds = (err == klNoErr); // remember we need to free the contents of the creds
    }
    
    if (err == klNoErr) {
        err = __KLCreateNewCCacheWithCredentials (ccachePrincipal, v5Context, &v5Creds, &newCCache);
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    if (err == klNoErr) {
        if (outFoundValidTickets == NULL)               { err = KLError_ (klParameterErr); }
        if (inKerberosVersion    == kerberosVersion_V4) { err = KLError_ (klInvalidVersionErr); }
    }
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (ccache);
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    if (err == klNoErr) {
        if (outStartTime     == NULL)                { err = KLError_ (klParameterErr); }
        if (inKerberosVersion == kerberosVersion_V4) { err = KLError_ (klInvalidVersionErr); }
    }
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLGetCCacheStartTime (ccache, outStartTime);
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
    if (err == klNoErr) {
        if (outExpirationTime == NULL)               { err = KLError_ (klParameterErr); }
        if (inKerberosVersion == kerberosVersion_V4) { err = KLError_ (klInvalidVersionErr); }
    }
    
    if (err == klNoErr) {
        if (inPrincipal == NULL) {
            err = __KLGetSystemDefaultCCache (&ccache);
        } else {
            err = __KLGetFirstCCacheForPrincipal (inPrincipal, &ccache);
        }
    }
    
    if (err == klNoErr) {
        err = __KLGetCCacheExpirationTime (ccache, outExpirationTime);
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

    dprintf ("Entering %s(): session is:", __FUNCTION__);
    dprintsession ();
    
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


