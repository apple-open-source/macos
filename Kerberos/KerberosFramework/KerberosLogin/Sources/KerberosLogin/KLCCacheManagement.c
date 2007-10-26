/*
 * KLCCacheManagement.c
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

typedef struct OpaqueKLCCache {
    krb5_context context;
    krb5_ccache  cache;
} CCache;



#pragma mark -

// ---------------------------------------------------------------------------

// The address functions in krb5 don't even use krb5_contexts but take them
// These functions don't need a fresh one (which is expensive to create), 
// just give them any old crappy one, such as this one: 
//
// Note... since we never change the context once we've initialized it,
// only lock the mutex when creating it.

static pthread_mutex_t gUnusedContextMutex = PTHREAD_MUTEX_INITIALIZER;
krb5_context           gUnusedContext = NULL;

static KLStatus __KLGetUnusedKerberos5Context (krb5_context *outContext)
{
    KLStatus err = klNoErr;
   
    if (gUnusedContext == NULL) {
        KLStatus lockErr = err = pthread_mutex_lock (&gUnusedContextMutex);
        
        if (!err) {
            err = krb5_init_context (&gUnusedContext);
        }

        if (!lockErr) { pthread_mutex_unlock (&gUnusedContextMutex); }
    }
    
    if (!err) {
        *outContext = gUnusedContext;
    }
    
    return KLError_ (err);
}

#pragma mark -
#pragma mark -- Local Address Management --
#pragma mark -

// ---------------------------------------------------------------------------

static pthread_mutex_t gAddressesChangeTimeMutex = PTHREAD_MUTEX_INITIALIZER;
krb5_address**         gAddresses = NULL;
KLTime                 gAddressChangeTime = 0;


// ---------------------------------------------------------------------------

static KLStatus __KLSetAddresses (krb5_address **inAddresses,
                                  KLTime        *outNewChangeTime)
{
    KLStatus lockErr = pthread_mutex_lock (&gAddressesChangeTimeMutex);
    KLStatus err = lockErr;
    krb5_context context = NULL;
    
    if (!err) {
        err = __KLGetUnusedKerberos5Context (&context);
    }
    
    if (!err) {
        if (gAddresses != NULL) { krb5_free_addresses (context, gAddresses); }
        
        gAddresses = inAddresses;
        gAddressChangeTime = time (NULL);
        
        if (outNewChangeTime != NULL) { *outNewChangeTime = gAddressChangeTime; }
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gAddressesChangeTimeMutex); }
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLGetAddresses (krb5_address ***outAddresses)
{
    KLStatus lockErr = pthread_mutex_lock (&gAddressesChangeTimeMutex);
    KLStatus err = lockErr;
    
    if (outAddresses == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        *outAddresses = gAddresses;
    }
    
    if (!lockErr) { pthread_mutex_unlock (&gAddressesChangeTimeMutex); }
    return KLError_ (err);
}


// ---------------------------------------------------------------------------

/*
 * Check if an IP address from a ticket is valid for this machine
 *
 * Logic:
 * - If we can't find any IP addresses, we return true. It could be that the machine is temporarily
 * off the net, and so we don't want to throw away the tickets. Noone will be using them while we
 * are off the net anyway. If we come back on the net with a different address later, we will
 * get rid of the tickets then
 * - Otherwise, we return true if any of primary addresses match. This means that we may
 * experience weird behavior on sinle- and multi-link multihomed hosts, but I think that's okay
 * for now. I don't think that there is a behavior that's universallyy correct on multi-homed
 * hosts, but most people don't use multihomed hosts anyway (of course, I write this as I am
 * looking for a 2nd Ethernet card for my 7300...)
 */

static KLStatus __KLAddressIsValid (krb5_context  inContext,
                                    krb5_address *inAddress)
{
    KLStatus       err = klNoErr;
    krb5_address **addresses = NULL;
    krb5_context   context = NULL;
    
    if (inAddress == NULL) { err = KLError_ (klParameterErr); }

    if (!err) {
        err = __KLGetUnusedKerberos5Context (&context);
    }
        
    if (!err) {
        // update the address state
        __KLCheckAddresses ();
    }
    
    if (!err) {
        err = __KLGetAddresses (&addresses);
    }
    
    // TCP/IP has been unloaded so there are no addresses in the list,
    // assume tickets are still valid in case we return
    if (!err) {
        if ((addresses != NULL) && (addresses[0] != NULL)) {
            if (krb5_address_search (context, inAddress, addresses) != TRUE) {
                err = KLError_ (klCredentialsBadAddressErr);
            }
        }
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLTime __KLCheckAddresses ()
{
    // Get the list of addresses from krb5
    KLStatus       err = klNoErr;
    krb5_context   context = NULL;
    krb5_address **oldAddresses = NULL;
    krb5_address **newAddresses = NULL;
    KLIndex        i;
    KLTime         newChangeTime = 0;
    
    if (!err) {
        err = __KLGetUnusedKerberos5Context (&context);
    }
    
    if (!err) {
        err = __KLGetAddresses (&oldAddresses);
    }
    
    if (!err) {
        err = krb5_os_localaddr (context, &newAddresses);
        if (err) { newAddresses = NULL; err = klNoErr; } // no network
    }
    
    if (!err) {
        KLBoolean addressesChanged = false;
        
        if (oldAddresses == NULL || newAddresses == NULL) {
            // If they aren't both NULL, it changed
            addressesChanged = (oldAddresses != newAddresses);
        } else {
            // Compare them: We aren't doing this in a terribly efficient way,
            // but most folks will only have a couple addresses anyway
            
            // Are all the addresses in newAddresses also in oldAddresses?
            for (i = 0; newAddresses[i] != NULL && !addressesChanged; i++) {
                if (krb5_address_search (context, newAddresses[i], oldAddresses) == false) {
                    // Address missing!
                    addressesChanged = true;
                }
            }
            
            // Are all the addresses in oldAddresses also in newAddresses?
            for (i = 0; oldAddresses[i] != NULL && !addressesChanged; i++) {
                if (krb5_address_search (context, oldAddresses[i], newAddresses) == false) {
                    // Address missing!
                    addressesChanged = true;
                }
            }
        }
        
        // save the new address list
        if (addressesChanged) {
            err = __KLSetAddresses (newAddresses, &newChangeTime);
            if (!err) { 
                dprintf ("__KLCheckAddresses(): ADDRESS CHANGED, returning new time %d", newChangeTime);
                newAddresses = NULL;  // Don't free
            }
        }
    }
    
    if (err) {
        dprintf ("__KLCheckAddresses(): got fatal error %d (%s)", err, error_message (err));
    }
    
    if (newAddresses != NULL) { krb5_free_addresses (context, newAddresses); }
    
    return err ? 0xFFFFFFFF : newChangeTime;	
}

// ---------------------------------------------------------------------------

KLStatus __KLFreeAddressList ()
{
    return KLError_ (__KLSetAddresses (NULL, NULL));
}

#pragma mark -
#pragma mark -- Credentials --
#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLV5CredsAreValid (krb5_context inContext, 
                                     krb5_creds *inV5Creds)
{
    KLStatus     err = klNoErr;
    krb5_int32   now = 0;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inV5Creds == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        krb5_int32 usec;
        
        err = krb5_us_timeofday (inContext, &now, &usec);
    }
    
    if (!err) {
        if (inV5Creds->times.endtime <= now) {
            err = KLError_ (klCredentialsExpiredErr); 
        }
    }
    
    if (!err) {
        if ((inV5Creds->ticket_flags & TKT_FLG_POSTDATED) && 
            (inV5Creds->ticket_flags & TKT_FLG_INVALID)) {
            err = KLError_ (klCredentialsNeedValidationErr);
        }
    }
    
    if (!err) {
        if (inV5Creds->addresses != NULL) {  // No addresses is always valid!
            krb5_address **addressPtr = inV5Creds->addresses;
            
            for (; *addressPtr != NULL; *addressPtr++) {
                err = __KLAddressIsValid (inContext, *addressPtr);
                if (!err) { break; }  // If there's one valid address, it's valid
            }
        }
    }
    
    return KLError_ (err);
}

#pragma mark -
#pragma mark -- krb5_ccaches --
#pragma mark -

// ---------------------------------------------------------------------------
// Returns the ccache name for krb5_cc_resolve()

static KLStatus __KLGetCCacheResolveName (char  *inCCacheType,
                                          char  *inCCacheName,
                                          char **outResolveName)
{
    KLStatus  err = klNoErr;
    char     *resolveName = NULL;
    
    if (inCCacheType   == NULL) { err = KLError_ (klParameterErr); }
    if (inCCacheName   == NULL) { err = KLError_ (klParameterErr); }
    if (outResolveName == NULL) { err = KLError_ (klParameterErr); }

    if (!err) {
        err = __KLCreateString (inCCacheName, &resolveName);
        
        if (!err) {
            err = __KLAddPrefixToString (":", &resolveName);
        }
        
        if (!err) {
            err = __KLAddPrefixToString (inCCacheType, &resolveName);
        }
    }
    
    if (!err) {
        *outResolveName = resolveName;
        resolveName = NULL;
    }
    
    if (resolveName != NULL) { KLDisposeString (resolveName); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------
// Returns the ccache type string and name
// Caller should pass in NULL for any strings it does not need.

static KLStatus __KLGetCCacheTypeAndName (krb5_context   inContext, 
                                          krb5_ccache    inCCache,
                                          char         **outCCacheType,
                                          char         **outCCacheName)
{
    KLStatus  err = klNoErr;
    char     *type = NULL;
    char     *name = NULL;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        const char *v5Type = NULL;
        
        v5Type = krb5_cc_get_type (inContext, inCCache);
        if (v5Type == NULL) { err = KLError_ (klCacheDoesNotExistErr); }
        
        if (!err) {
            err = __KLCreateString (v5Type, &type);
        }
    }
    
    if (!err) {
        const char *v5Name = NULL;
        
        v5Name = krb5_cc_get_name (inContext, inCCache);
        if (v5Name == NULL) { err = KLError_ (klCacheDoesNotExistErr); }
        
        if (!err) {
            err = __KLCreateString (v5Name, &name);
        }
    }
    
    if (!err) {
        if (outCCacheType != NULL) { 
            *outCCacheType = type;
            type = NULL; 
        }
        if (outCCacheName != NULL) { 
            *outCCacheName = name;
            name = NULL; 
        }
    }
    
    if (type != NULL) { KLDisposeString (type); }
    if (name != NULL) { KLDisposeString (name); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLBoolean __KLKrb5CCacheIsCCAPICCache (krb5_context  inContext, 
                                              krb5_ccache   inCCache)
{
    KLStatus  err = klNoErr;
    char *type = NULL;
    KLBoolean isAPICCache = FALSE;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLGetCCacheTypeAndName (inContext, inCCache, &type, NULL);
    }
    
    if (!err) {
        if (strcmp (type, "API") == 0) { 
            isAPICCache = TRUE;
        }
    }
    
    if (type != NULL) { KLDisposeString (type); }
    
    return isAPICCache;
}

// ---------------------------------------------------------------------------

static KLStatus __KLGetCCAPICCacheForKrb5CCache (krb5_context  inContext, 
                                                 krb5_ccache   inCCache,
                                                 cc_ccache_t  *outCCache)
{
    KLStatus      err = klNoErr;
    char         *name = NULL;
    cc_context_t  cc_context = NULL;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        if (!__KLKrb5CCacheIsCCAPICCache (inContext, inCCache)) { err = KLError_ (klParameterErr); }
    }
    
    if (!err) {
        err = __KLGetCCacheTypeAndName (inContext, inCCache, NULL, &name);
    }
    
    if (!err) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (!err) {
        err = cc_context_open_ccache (cc_context, name, outCCache);
        if (err == ccErrCCacheNotFound || err == ccErrBadName) {
            err = KLError_ (klCacheDoesNotExistErr);
        }
    }
    
    if (name       != NULL) { KLDisposeString (name); }
    if (cc_context != NULL) { cc_context_release (cc_context); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLGetKrb5CCacheForCCAPICCache (krb5_context  inContext, 
                                                 cc_ccache_t   inCCache,
                                                 krb5_ccache  *outCCache)
{
    KLStatus      err = klNoErr;
    cc_string_t   name = NULL;
    char         *resolveName = NULL;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = cc_ccache_get_name (inCCache, &name);
    }
    
    if (!err) {
        err = __KLGetCCacheResolveName ("API", (char *) name->data, &resolveName);
    }
    
    if (!err) {
        err = krb5_cc_resolve (inContext, resolveName, outCCache);
    }
    
    if (name        != NULL) { cc_string_release (name); }
    if (resolveName != NULL) { KLDisposeString (resolveName); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLCloseKrb5CCache (krb5_context inContext, krb5_ccache inCCache)
{
    KLStatus err = klNoErr;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = krb5_cc_close (inContext, inCCache);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLDestroyKrb5CCache (krb5_context inContext, krb5_ccache inCCache)
{
    KLStatus err = klNoErr;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = krb5_cc_destroy (inContext, inCCache);
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------
// krb5_ccaches can point at ccaches which have not actually been created.
// This function checks to see if they really exist.

static KLBoolean __KLKrb5CCachesAreEqual (krb5_context inContextA, 
                                          krb5_ccache  inCCacheA, 
                                          krb5_context inContextB, 
                                          krb5_ccache  inCCacheB)
{
    KLStatus  err = klNoErr;
    KLBoolean equal = FALSE;
    char     *typeA = NULL;
    char     *nameA = NULL;
    char     *typeB = NULL;
    char     *nameB = NULL;
    
    if (inContextA == NULL) { err = KLError_ (klParameterErr); }
    if (inCCacheA  == NULL) { err = KLError_ (klParameterErr); }
    if (inContextB == NULL) { err = KLError_ (klParameterErr); }
    if (inCCacheB  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLGetCCacheTypeAndName (inContextA, inCCacheA, &typeA, &nameA);
    }
    
    if (!err) {
        err = __KLGetCCacheTypeAndName (inContextB, inCCacheB, &typeB, &nameB);
    }
    
    if (!err) {
        if ((strcmp (typeA, typeB) == 0) && (strcmp (nameA, nameB) == 0)) {
            equal = TRUE;
        }
    }
    
    if (typeA != NULL) { KLDisposeString (typeA); }
    if (nameA != NULL) { KLDisposeString (nameA); }
    if (typeB != NULL) { KLDisposeString (typeB); }
    if (nameB != NULL) { KLDisposeString (nameB); }
    
    return equal;
}

// ---------------------------------------------------------------------------
// krb5_ccaches can point at ccaches which have not actually been created.
// This function checks to see if they really exist.

static KLBoolean __KLKrb5CCacheExists (krb5_context inContext, krb5_ccache inCCache)
{
    KLStatus       err = klNoErr;
    krb5_principal principal = NULL;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = krb5_cc_get_principal (inContext, inCCache, &principal);
    }
    
    if (principal != NULL) { krb5_free_principal (inContext, principal); }
    
    return !KLError_ (err);
}

#pragma mark -
#pragma mark -- KLCCache Creation/Destruction --
#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLStoreKerberos5CredentialsInCCache (KLPrincipal        inPrincipal,
                                                       krb5_creds        *inV5Creds, 
                                                       KLCCache           inCCache)
{	
    KLStatus       err = klNoErr;
    krb5_principal principal = NULL;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (inV5Creds   == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache    == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        principal = __KLPrincipalGetKerberos5Principal (inPrincipal);
        if (principal == NULL) { err = KLError_ (klParameterErr); }
    }
    
    if (!err) {
        err = krb5_cc_initialize (inCCache->context, inCCache->cache, principal);
    }
    
    if (!err) {
        err = krb5_cc_store_cred (inCCache->context, inCCache->cache, inV5Creds);
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLCreateNewCCAPICCache (KLPrincipal        inPrincipal, 
                                          krb5_context       inV5Context,
                                          KLCCache          *outCCache)
{
    KLStatus err = klNoErr;
    KLCCache ccache = NULL;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        ccache = (KLCCache) calloc (1, sizeof (CCache));
        if (ccache == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (!err) {
        err = krb5_copy_context (inV5Context, &ccache->context);
    }
    
    if (!err) {
        err = krb5_cc_new_unique (ccache->context, "API", NULL, &ccache->cache);
    }
    
    if (!err) {
        err = krb5_cc_initialize (ccache->context, ccache->cache, 
                                  __KLPrincipalGetKerberos5Principal (inPrincipal));
    }
    
    if (!err) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { __KLDestroyCCache (ccache); }
    }
    
    return KLError_ (err);    
}

// ---------------------------------------------------------------------------

KLStatus __KLCreateNewCCacheWithCredentials (KLPrincipal    inPrincipal, 
                                             krb5_context   inV5Context,
                                             krb5_creds    *inV5Creds, 
                                             KLCCache      *outCCache)
{
    KLStatus err = klNoErr;
    KLCCache ccache = NULL;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    if (inV5Creds   == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLCreateNewCCAPICCache (inPrincipal, inV5Context, &ccache);
    }
    
    if (!err && (inV5Creds != NULL)) {
        err = __KLStoreKerberos5CredentialsInCCache (inPrincipal, inV5Creds, ccache);
    }
    
    if (!err) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { __KLDestroyCCache (ccache); }
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheByName (const char *inCacheName, 
                              KLCCache   *outCCache)
{
    KLStatus err = klNoErr;
    KLCCache ccache = NULL;
    
    if (inCacheName == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    
    
    if (!err) {
        ccache = (KLCCache) calloc (1, sizeof (CCache));
        if (ccache == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (!err) {
        err = krb5_init_context (&ccache->context);
    }
    
    if (!err) {
        err = krb5_cc_resolve (ccache->context, inCacheName, &ccache->cache);
    }
    
    if (!err) {
        *outCCache = ccache;
        ccache = NULL;
    }
    
    if (ccache != NULL) { __KLCloseCCache (ccache); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetSystemDefaultCCache (KLCCache *outCCache)
{
    KLStatus err = klNoErr;
    KLCCache ccache = NULL;
    
    if (outCCache == NULL)  { err = KLError_ (klParameterErr); }
    
    if (!err) {
        ccache = (KLCCache) calloc (1, sizeof (CCache));
        if (ccache == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (!err) {
        err = krb5_init_context (&ccache->context);
    }
    
    if (!err) {
        err = krb5_cc_default (ccache->context, &ccache->cache);
    }
        
    if (!err) {
        err = __KLCCacheExists (ccache);  // make sure the ccache isn't a placeholder ccache
        if (err) { err = KLError_ (klSystemDefaultDoesNotExistErr); }
    }

    if (!err) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { __KLCloseCCache (ccache); }
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLGetCCAPICCacheForPrincipal (KLPrincipal inPrincipal, krb5_context inContext, krb5_ccache *outCCache)
{
    KLStatus             err = klNoErr;
    KLBoolean            found = FALSE;
    char                *principalV5String = NULL;
    cc_context_t         cc_context = NULL;
    cc_ccache_iterator_t iterator = NULL;
    krb5_ccache          ccache = NULL;    
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (inContext   == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (!err) {
        err = cc_context_new_ccache_iterator (cc_context, &iterator);
    }
    
    while (!err && !found) {
        cc_ccache_t cc_ccache = NULL;
        krb5_principal ccachePrincipal = NULL;
        
        err = cc_ccache_iterator_next (iterator, &cc_ccache);
        
        if (!err) {
            err = __KLGetKrb5CCacheForCCAPICCache (inContext, cc_ccache, &ccache);
        }
        
        if (!err && (krb5_cc_get_principal (inContext, ccache, &ccachePrincipal) == 0)) {
            found = krb5_principal_compare (inContext, 
                                            __KLPrincipalGetKerberos5Principal (inPrincipal),
                                            ccachePrincipal);
        }
        
        if (ccachePrincipal != NULL) { krb5_free_principal (inContext, ccachePrincipal); }
        if (cc_ccache       != NULL) { cc_ccache_release (cc_ccache); }
    }
    
    if ((err == ccIteratorEnd) || (err == ccErrCCacheNotFound) || (err == ccErrBadName)) { 
        err = KLError_ (klPrincipalDoesNotExistErr); 
    }
    
    if (!err) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { __KLCloseKrb5CCache (inContext, ccache); }
    }
    
    if (principalV5String != NULL) { KLDisposeString (principalV5String); }
    if (iterator          != NULL) { cc_ccache_iterator_release (iterator); }
    if (cc_context        != NULL) { cc_context_release (cc_context); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetFirstCCacheForPrincipal (KLPrincipal  inPrincipal, 
                                         KLCCache    *outCCache)
{
    KLStatus             err = klNoErr;
    KLCCache             ccache = NULL;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        ccache = (KLCCache) calloc (1, sizeof (CCache));
        if (ccache == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (!err) {
        err = krb5_init_context (&ccache->context);
    }
    
    // First check to see if the v5 default is not a CCAPI ccache and contains our principal
    if (!err) {
        char *environmentName = getenv ("KRB5CCNAME");
        krb5_ccache defaultCCache = NULL;
        
        if ((environmentName != NULL) && (krb5_cc_default (ccache->context, &defaultCCache) == klNoErr)) {
            krb5_principal defaultPrincipal = NULL;
            
            if (krb5_cc_get_principal (ccache->context, defaultCCache, &defaultPrincipal) == klNoErr) {
                if (krb5_principal_compare (ccache->context, defaultPrincipal, 
                                            __KLPrincipalGetKerberos5Principal (inPrincipal))) {
                    ccache->cache = defaultCCache;
                    defaultCCache = NULL;  // don't close
                }
            }
            
            if (defaultPrincipal != NULL) { krb5_free_principal (ccache->context, defaultPrincipal); }
        }

        if (defaultCCache != NULL) { __KLCloseKrb5CCache (ccache->context, defaultCCache); }
    }
    
    // If we didn't find it in the default, search the cache collection
    if (!err && (ccache->cache == NULL)) {
        err = __KLGetCCAPICCacheForPrincipal (inPrincipal, ccache->context, &ccache->cache);
    }
    
    if (!err) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { __KLCloseCCache (ccache); }
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLCloseCCache (KLCCache inCCache)
{
    KLStatus err = klNoErr;
    
    if (inCCache == NULL)  { err = KLError_ (klParameterErr); }

    if (!err) {
        if (inCCache->cache   != NULL) { __KLCloseKrb5CCache (inCCache->context, inCCache->cache); }
        if (inCCache->context != NULL) { krb5_free_context (inCCache->context); }
        free (inCCache);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLDestroyCCache (KLCCache inCCache)
{
    KLStatus err = klNoErr;
    
    if (inCCache == NULL)  { err = KLError_ (klParameterErr); }
    
    if (!err) {
        if (inCCache->cache   != NULL) { __KLDestroyKrb5CCache (inCCache->context, inCCache->cache); }
        if (inCCache->context != NULL) { krb5_free_context (inCCache->context); }
        free (inCCache);
    }
    
    return KLError_ (err);
}

#pragma mark -
#pragma mark -- Operations on KLCCaches --
#pragma mark -

// ---------------------------------------------------------------------------
// Overwrites the v5 tickets in inDestinationCCache with those in inSourceCCache
// Does nothing if inSourceCCache == inDestinationCCache

static KLStatus __KLOverwriteV5Credentials (krb5_context inSourceContext, 
                                            krb5_ccache  inSourceCCache, 
                                            krb5_context inDestinationContext, 
                                            krb5_ccache  inDestinationCCache)
{
    KLStatus err = klNoErr;
    
    if (inSourceContext      == NULL)  { err = KLError_ (klParameterErr); }
    if (inSourceCCache       == NULL)  { err = KLError_ (klParameterErr); }
    if (inDestinationContext == NULL)  { err = KLError_ (klParameterErr); }
    if (inDestinationCCache  == NULL)  { err = KLError_ (klParameterErr); }
    
    if (!err && !__KLKrb5CCachesAreEqual (inSourceContext, inSourceCCache, inDestinationContext, inDestinationCCache)) {
        // Caches are different.  clear out any v5 credentials in inDestination CCache
        krb5_principal v5Principal = NULL;
        
        err = krb5_cc_get_principal (inSourceContext, inSourceCCache, &v5Principal);
        
        if (!err) {
            err = krb5_cc_initialize (inDestinationContext, inDestinationCCache, v5Principal);
        }
        
        if (!err) {
            err = krb5_cc_copy_creds (inSourceContext, inSourceCCache, inDestinationCCache);
        }
        
        if (v5Principal != NULL) { krb5_free_principal (inSourceContext, v5Principal); }            
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLMoveCCache (KLCCache inSourceCCache, KLCCache inDestinationCCache)
{
    KLStatus    err = klNoErr;
    KLBoolean   sourceIsCCAPI = FALSE;        
    KLBoolean   destinationIsCCAPI = FALSE;
    
    if (inSourceCCache      == NULL)  { err = KLError_ (klParameterErr); }
    if (inDestinationCCache == NULL)  { err = KLError_ (klParameterErr); }
    
    if (!err) {
        sourceIsCCAPI      = __KLKrb5CCacheIsCCAPICCache (inSourceCCache->context,      inSourceCCache->cache);        
        destinationIsCCAPI = __KLKrb5CCacheIsCCAPICCache (inDestinationCCache->context, inDestinationCCache->cache);
 
        // Sanity check to prevent destroying inDestinationCCache on bad input
        if (!__KLKrb5CCacheExists (inSourceCCache->context, inSourceCCache->cache)) { 
            err = KLError_ (klCacheDoesNotExistErr); 
        } 
    }
    
    if (!err) {
        if (destinationIsCCAPI) {
            KLCCache    temporaryCCache = NULL;
            cc_ccache_t sourceCCache = NULL;
            cc_ccache_t destinationCCache = NULL;
            
            if (sourceIsCCAPI) {
                // Both main ccaches are ccapi, just get the source directly
                err = __KLGetCCAPICCacheForKrb5CCache (inSourceCCache->context, inSourceCCache->cache, &sourceCCache);
            } else {
                // Source is not ccapi, copy source creds into a temporary ccapi ccache:
                KLPrincipal principal = NULL;
                
                err = __KLGetPrincipalForCCache (inSourceCCache, &principal);
                
                if (!err) {
                    err = __KLCreateNewCCAPICCache (principal, inSourceCCache->context, &temporaryCCache);
                }
                
                if (!err) {
                    err = __KLOverwriteV5Credentials (inSourceCCache->context, inSourceCCache->cache, 
                                                      temporaryCCache->context, temporaryCCache->cache);
                }

                if (!err) {
                    err = __KLGetCCAPICCacheForKrb5CCache (temporaryCCache->context, temporaryCCache->cache, &sourceCCache);                       
                }
                
                if (principal != NULL) { KLDisposePrincipal (principal); }
            }
               
            if (!err) {
                err = __KLGetCCAPICCacheForKrb5CCache (inDestinationCCache->context, inDestinationCCache->cache, &destinationCCache);                
            }
            
            if (!err) {
                err = cc_ccache_move (sourceCCache, destinationCCache);
                if (!err) { sourceCCache = NULL; }
            }
            
            if (sourceCCache      != NULL) { cc_ccache_release (sourceCCache); }
            if (destinationCCache != NULL) { cc_ccache_release (destinationCCache); }
            if (temporaryCCache   != NULL) { __KLDestroyCCache (temporaryCCache); }
            
        } else {
            // Destination is not CCAPI so we don't have the move operator: 
            
            if (!err) {
                err = __KLOverwriteV5Credentials (inSourceCCache->context, inSourceCCache->cache, 
                                                  inDestinationCCache->context, inDestinationCCache->cache);
            }
        }
        
        if (!err) {
            // Only destroy the source credentials if we copied to a *different* ccache
            if (!__KLKrb5CCachesAreEqual (inSourceCCache->context, inSourceCCache->cache, 
                                          inDestinationCCache->context, inDestinationCCache->cache)) {
                __KLDestroyKrb5CCache (inSourceCCache->context, inSourceCCache->cache);
                inSourceCCache->cache = NULL;
            }
            
            // free remaining memory associated with the source ccache
            err = __KLCloseCCache (inSourceCCache);
        }
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLSetKrb5CCacheToCCAPIDefault (krb5_context inContext,
                                                 krb5_ccache  inV5CCache)
{
    KLStatus err = klNoErr;
    cc_ccache_t cc_ccache = NULL;
    
    if (inContext  == NULL) { err = KLError_ (klParameterErr); }
    if (inV5CCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLGetCCAPICCacheForKrb5CCache (inContext, inV5CCache, &cc_ccache); 
    }
    
    if (!err) {
        err = cc_ccache_set_default (cc_ccache);
    }
    
    if (cc_ccache != NULL) { cc_ccache_release (cc_ccache); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLSetDefaultCCache (KLCCache *ioCCache)
{
    KLStatus err = klNoErr;
    
    if ((ioCCache == NULL) || (*ioCCache == NULL)) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        char *environmentName = getenv ("KRB5CCNAME");

        if (environmentName != NULL) {
            // Move the tickets into this ccache:
            KLCCache environmentCCache = NULL;
            
            err = __KLGetCCacheByName (environmentName, &environmentCCache);
            
            if (!err) {
                err = __KLMoveCCache (*ioCCache, environmentCCache);
            }
            
            if (!err) {
                // ioCCache got invalidated by __KLMoveCCache so copy it over
                *ioCCache = environmentCCache;
                environmentCCache = NULL;
            }
            
            if (environmentCCache != NULL) { __KLCloseCCache (environmentCCache); }
            
        } else {
            // KRB5CCNAME is not set so we will store tickets in the CCAPI default ccache

            if (__KLKrb5CCacheIsCCAPICCache ((*ioCCache)->context, (*ioCCache)->cache)) {
                // inCCache is a CCAPI ccache:
                err = __KLSetKrb5CCacheToCCAPIDefault ((*ioCCache)->context, (*ioCCache)->cache);
            
            } else { 
                // inCCache isn't a CCAPI ccache and there are no v4 tickets... copy into a new ccapi ccache.
                KLCCache destinationCCache = NULL;
                KLPrincipal principal = NULL;
                
                err = __KLGetPrincipalForCCache (*ioCCache, &principal);
                
                if (!err) {
                    // __KLGetFirstCCacheForPrincipal will only return API ccaches because KRB5CCNAME is not set
                    err = __KLGetFirstCCacheForPrincipal (principal, &destinationCCache);
                    if (err) {
                        // No cache for this principal
                        err = __KLCreateNewCCAPICCache (principal, (*ioCCache)->context, &destinationCCache);
                    }
                }

                if (!err) {
                    err = __KLMoveCCache (*ioCCache, destinationCCache);
                }
                
                if (!err) {
                    err = __KLSetKrb5CCacheToCCAPIDefault (destinationCCache->context, destinationCCache->cache);
                }
                
                if (!err) {
                    err = __KLDestroyCCache (*ioCCache);
                }
                
                if (!err) {
                    *ioCCache = destinationCCache;
                    destinationCCache = NULL;
                }
                
                if (principal         != NULL) { KLDisposePrincipal (principal); }
                if (destinationCCache != NULL) { __KLCloseCCache (destinationCCache); }
            }
        }
    }
    
    return KLError_ (err);
}

#pragma mark -
#pragma mark -- KLCCache Validity --
#pragma mark -

// ---------------------------------------------------------------------------
// krb5_ccaches can point at ccaches which have not actually been created.
// This function checks to see if they really exist.

KLStatus __KLCCacheExists (KLCCache inCCache)
{
    KLStatus err = klNoErr;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        if (!__KLKrb5CCacheExists (inCCache->context, inCCache->cache)) {
            err = KLError_ (klCacheDoesNotExistErr);
        }
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------
// Prefers tgts over service tickets:

KLStatus __KLGetValidV5TicketForCCache (KLCCache     inCCache, 
                                        krb5_creds **outV5Creds,
                                        KLBoolean   *outV5CredsAreTGT)
{
    KLStatus err = klNoErr;
    krb5_cc_cursor cursor = NULL;
    krb5_creds *validCreds = NULL;
    KLBoolean foundValidTGT = FALSE;
    KLBoolean foundInvalidTGT = FALSE;
    KLStatus  invalidTGTErr = klNoErr;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    // outV5Creds and outV5CredsAreTGT may be NULL if the caller only wants to get success/error
    
    if (!err) {
        err = krb5_cc_start_seq_get (inCCache->context, inCCache->cache, &cursor);
    }
    
    while (!err && !foundValidTGT) {
        krb5_creds  creds;
        KLBoolean   freeCreds = FALSE;
        KLPrincipal servicePrincipal = NULL;
        
        err = krb5_cc_next_cred (inCCache->context, inCCache->cache, &cursor, &creds);
        if (!err) { freeCreds = TRUE; }
        
        if (!err) {
            err = __KLCreatePrincipalFromKerberos5Principal (creds.server, &servicePrincipal);
        }
        
        if (!err) {
            KLBoolean credsAreTGT = __KLPrincipalIsTicketGrantingService (servicePrincipal);
            KLStatus  credsErr = __KLV5CredsAreValid (inCCache->context, &creds);

            if (credsAreTGT && credsErr) {
                foundInvalidTGT = TRUE;
                invalidTGTErr = credsErr;
                
            } else if (!credsErr && ((validCreds == NULL) || (!foundValidTGT && credsAreTGT))) {
                if (validCreds != NULL) { krb5_free_creds (inCCache->context, validCreds); }
                
                err = krb5_copy_creds (inCCache->context, &creds, &validCreds);
                if (!err) { foundValidTGT = credsAreTGT; }
            }
        }
        
        if (servicePrincipal != NULL) { KLDisposePrincipal (servicePrincipal); }
        if (freeCreds               ) { krb5_free_cred_contents (inCCache->context, &creds); }
    }
    if (!err || (err == KRB5_CC_END)) {
        // Invalid TGT invalidates the entire cache
        if (foundInvalidTGT && !foundValidTGT) { 
            err = KLError_ (invalidTGTErr); 
        } else {
            if (validCreds == NULL) { err = KLError_ (klNoCredentialsErr); } else { err = klNoErr; }
        }
    }
    
    if (!err) {
        if (outV5Creds != NULL) {
            *outV5Creds = validCreds;
            validCreds = NULL;  // don't free
        }
        if (outV5CredsAreTGT != NULL) {
            *outV5CredsAreTGT = foundValidTGT;
        }
    }
    
    if (validCreds != NULL) { krb5_free_creds (inCCache->context, validCreds); }
    if (cursor     != NULL) { krb5_cc_end_seq_get (inCCache->context, inCCache->cache, &cursor); }
    
    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus __KLGetValidV5TgtForCCache (KLCCache     inCCache, 
                                     krb5_creds **outV5Creds)
{
    KLStatus err = klNoErr;
    krb5_creds *creds = NULL;
    KLBoolean credsAreTGT = FALSE;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    // outV5Creds maybe NULL if the caller only wants to get success/error
    
    if (!err) {
        err = __KLGetValidV5TicketForCCache (inCCache, &creds, &credsAreTGT);
    }
    
    if (!err) {
        if (credsAreTGT) {
            if (outV5Creds != NULL) {
                *outV5Creds = creds;
                creds = NULL;  // don't free
            }            
        } else {
            err = KLError_ (klNoCredentialsErr);
        }
    }
    
    if (creds != NULL) { krb5_free_creds (inCCache->context, creds); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLCacheHasValidTickets (KLCCache inCCache)
{
    KLStatus err = __KLGetValidV5TicketForCCache (inCCache, NULL, NULL);
    
    return KLError_ (err);
}	

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheExpirationTime (KLCCache  inCCache, 
                                      KLTime   *outExpirationTime)
{
    KLStatus err = klNoErr;
    krb5_creds *v5Creds = NULL;
    
    if (inCCache          == NULL) { err = KLError_ (klParameterErr); }
    if (outExpirationTime == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLCacheHasValidTickets (inCCache);
    }
    
    if (!err) {        
        err = __KLGetValidV5TicketForCCache (inCCache, &v5Creds, NULL);
    }
    
    if (!err) {
        *outExpirationTime = v5Creds->times.endtime;
    }
    
    // This is usually, but not always a bug
    if (!err && (*outExpirationTime == 0 || *outExpirationTime == 0xFFFFFFFF)) {
        dprintf ("__KLGetCCacheExpirationTime returning suspicious expiration time %d", *outExpirationTime);
    }
    
    if (v5Creds != NULL) { krb5_free_creds (inCCache->context, v5Creds); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheStartTime (KLCCache  inCCache, 
                                 KLTime   *outStartTime)
{
    KLStatus err = klNoErr;
    krb5_creds *v5Creds = NULL;
    
    if (inCCache     == NULL)            { err = KLError_ (klParameterErr); }
    if (outStartTime == NULL)            { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLCacheHasValidTickets (inCCache);
    }
    
    if (!err) {        
        err = __KLGetValidV5TicketForCCache (inCCache, &v5Creds, NULL);
    }
        
    if (!err) {
        *outStartTime = v5Creds->times.starttime;
    }
    
    // This is usually, but not always a bug
    if (!err && (*outStartTime == 0 || *outStartTime == 0xFFFFFFFF)) {
        dprintf ("__KLGetCCacheStartTime returning suspicious start time %d", *outStartTime);
    }
    
    return KLError_ (err);
}

#pragma mark -
#pragma mark -- Other Information About KLCCaches --
#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetKrb5CCacheAndContextForCCache (KLCCache      inCCache, 
                                               krb5_ccache  *outCCache, 
                                               krb5_context *outContext)
{
    KLStatus err = klNoErr;
    
    if (inCCache   == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache  == NULL) { err = KLError_ (klParameterErr); }
    if (outContext == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        *outCCache = inCCache->cache;
        *outContext = inCCache->context;
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetPrincipalForCCache (KLCCache     inCCache, 
                                    KLPrincipal *outPrincipal)
{
    KLStatus     err = klNoErr;
    KLPrincipal  principal = NULL;
    krb5_principal v5Principal = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = krb5_cc_get_principal (inCCache->context, inCCache->cache, &v5Principal);
    }
    
    if (!err) {
        err = __KLCreatePrincipalFromKerberos5Principal (v5Principal, &principal);
    }
    
    if (!err) {
        if (outPrincipal != NULL) {
            *outPrincipal = principal;
            principal = NULL;
        }
    }
    
    if (v5Principal != NULL) { krb5_free_principal (inCCache->context, v5Principal); }
    if (principal != NULL) { KLDisposePrincipal (principal); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetNameForCCache (KLCCache   inCCache,
                               char     **outName)
{
    KLStatus     err = klNoErr;
    char        *name = NULL;
    char        *type = NULL;
    char        *resolveName = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLGetCCacheTypeAndName (inCCache->context, inCCache->cache, &type, &name);
    }
    
    if (!err) {
        err = __KLGetCCacheResolveName (type, name, &resolveName);
    }
    
    if (!err) {
        if (outName != NULL) {
            *outName = resolveName;
            resolveName = NULL;
        }
    }
    
    if (resolveName != NULL) { KLDisposeString (resolveName); }
    if (type        != NULL) { KLDisposeString (type); }
    if (name        != NULL) { KLDisposeString (name); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetPrincipalAndNameForCCache (KLCCache      inCCache, 
                                           KLPrincipal  *outPrincipal, 
                                           char        **outName)
{
    KLStatus err = klNoErr;
    KLPrincipal principal = NULL;
    char *name = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLGetPrincipalForCCache (inCCache, &principal);
    }
    
    if (!err) {
        err = __KLGetNameForCCache (inCCache, &name);
    }

    if (!err) {
        if (outPrincipal != NULL) {
            *outPrincipal = principal;
            principal = NULL;
        }
        if (outName != NULL) {
            *outName = name;
            name = NULL;
        }
    }

    if (principal != NULL) { KLDisposePrincipal (principal); }
    if (name      != NULL) { KLDisposeString (name); }

    return KLError_ (err);
}
