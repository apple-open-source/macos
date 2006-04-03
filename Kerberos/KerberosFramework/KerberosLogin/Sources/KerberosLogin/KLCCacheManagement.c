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
    krb5_ccache  alternateV4Cache;
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
        if (inV5Creds->times.endtime <= (cc_uint32) now) {
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

// ---------------------------------------------------------------------------

static KLStatus __KLV4CredsAreValid (krb5_context inContext, 
                                     CREDENTIALS *inV4Creds)
{
    KLStatus err = klNoErr;
    KLTime   now = 0;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inV4Creds == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        struct timeval timeStruct;
        
        err = gettimeofday (&timeStruct, NULL);
        if (!err) { 
            now = timeStruct.tv_sec;
        } else {
            err = KLError_ (errno); 
        }
    }
    
    if (!err) {
        // Account for long lifetime support when doing comparison:
        if (krb_life_to_time (inV4Creds->issue_date, inV4Creds->lifetime) <= now) {
            err = KLError_ (klCredentialsExpiredErr); 
        }
    }
    
    if (!err) {
        if (inV4Creds->address != 0L) {
            krb5_address address;
            
            // Eeeeeeeew!  Build a fake krb5_address.
            address.magic = KV5M_ADDRESS;
            address.addrtype = AF_INET;
            address.length = NS_INADDRSZ;
            address.contents = (unsigned char *) &inV4Creds->address;
            
            err = __KLAddressIsValid (inContext, &address);
        }
    }
    
    return KLError_ (err);
}

#pragma mark -
#pragma mark -- krb5_ccaches --
#pragma mark -

// ---------------------------------------------------------------------------
// Returns the ccache name for krb5_cc_resolve()
// Caller should pass in NULL for any strings it does not need.

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
        if (__KLKrb5CCacheIsCCAPICCache (inContext, inCCache)) {
            cc_ccache_t    cc_ccache = NULL;
            
            err = __KLGetCCAPICCacheForKrb5CCache (inContext, inCCache, &cc_ccache);
            
            if (!err) {
                err = cc_ccache_destroy (cc_ccache);
            } else if (err == klCacheDoesNotExistErr) {
                err = klNoErr; // ccache already destroyed by someone else
            }
            
            if (!err) {
                err = krb5_cc_close (inContext, inCCache); 
            }
        } else {
            err = krb5_cc_destroy (inContext, inCCache);
        }
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLGetCCAPICCacheForPrincipal (KLPrincipal inPrincipal, KLKerberosVersion inVersion, 
                                                krb5_context inContext, krb5_ccache *outCCache)
{
    KLStatus             err = klNoErr;
    KLBoolean            found = FALSE;
    char                *principalV4String = NULL;
    char                *principalV5String = NULL;
    cc_context_t         cc_context = NULL;
    cc_ccache_iterator_t iterator = NULL;
    krb5_ccache          ccache = NULL;    
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (inContext   == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        if (KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V5, &principalV5String) != klNoErr) {
            principalV5String = NULL;  // If we can't convert to this version, set to NULL
        }
        if (KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V4, &principalV4String) != klNoErr) {
            principalV4String = NULL;  // If we can't convert to this version, set to NULL
        }
        if ((principalV5String == NULL) && (principalV4String == NULL)) {
            err = KLError_ (klBadPrincipalErr);
        }
    }
    
    if (!err) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (!err) {
        err = cc_context_new_ccache_iterator (cc_context, &iterator);
    }
    
    while (!err && !found) {
        cc_ccache_t cc_ccache = NULL;
        cc_uint32   cc_version;
        
        err = cc_ccache_iterator_next (iterator, &cc_ccache);
        
        if (!err) {
            err = cc_ccache_get_credentials_version (cc_ccache, &cc_version);
        }
        
        if (!err) {
            // Check the cache version
            if ((inVersion == kerberosVersion_Any) || 
                ((inVersion == kerberosVersion_All) && (cc_version == cc_credentials_v4_v5)) ||
                ((inVersion & kerberosVersion_V5) && ((cc_version == cc_credentials_v4_v5) || (cc_version == cc_credentials_v5))) ||
                ((inVersion & kerberosVersion_V4) && ((cc_version == cc_credentials_v4_v5) || (cc_version == cc_credentials_v4)))) {
                
                cc_string_t cachePrincipal = NULL;
                cc_int32 requestVersion = (cc_version == cc_credentials_v4_v5) ? cc_credentials_v5 : cc_version;  // favor v5
                
                if (inVersion == kerberosVersion_V4) {
                    requestVersion = cc_credentials_v4;  // unless we are just looking for v4 caches
                }
                
                if (!err) {
                    err = cc_ccache_get_principal (cc_ccache, requestVersion, &cachePrincipal);
                }
                
                if (!err) {
                    char *principalString = (requestVersion == cc_credentials_v5) ? principalV5String : principalV4String;
                    if ((principalString != NULL) && (strcmp (cachePrincipal->data, principalString) == 0)) {
                        err = __KLGetKrb5CCacheForCCAPICCache (inContext, cc_ccache, &ccache);
                        if (!err) { found = true; }   // force it to break out of the loop gracefully
                    }
                }
                
                if (cachePrincipal != NULL) { cc_string_release (cachePrincipal); }
            }
        }
        
        if (cc_ccache != NULL) { cc_ccache_release (cc_ccache); }
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
    if (principalV4String != NULL) { KLDisposeString (principalV4String); }
    if (iterator          != NULL) { cc_ccache_iterator_release (iterator); }
    if (cc_context        != NULL) { cc_context_release (cc_context); }
    
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
    cc_ccache_t    cc_ccache = NULL;
    krb5_principal principal = NULL;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        if (__KLKrb5CCacheIsCCAPICCache (inContext, inCCache)) {
            err = __KLGetCCAPICCacheForKrb5CCache (inContext, inCCache, &cc_ccache);
        } else {
            err = krb5_cc_get_principal (inContext, inCCache, &principal);
        }
    }
    
    if (principal != NULL) { krb5_free_principal (inContext, principal); }
    if (cc_ccache != NULL) { cc_ccache_release (cc_ccache); }
    
    return !KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------
// Determines whether a v5 principal is set for the ccache

static KLBoolean __KLKrb5CacheHasV5Credentials (krb5_context inContext, krb5_ccache inCCache)
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

// ---------------------------------------------------------------------------
// Determines whether a v4 principal is set for the cccache

static KLBoolean __KLKrb5CacheHasV4Credentials (krb5_context inContext, krb5_ccache inCCache)
{
    KLStatus       err = klNoErr;
    cc_ccache_t    cc_ccache = NULL;
    cc_string_t    principalString = NULL;
    
    if (inContext == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLGetCCAPICCacheForKrb5CCache (inContext, inCCache, &cc_ccache);
    }
    
    if (!err) {
        err = cc_ccache_get_principal (cc_ccache, cc_credentials_v4, &principalString);
    }
    
    if (principalString != NULL) { cc_string_release (principalString); }
    if (cc_ccache != NULL) { cc_context_release (cc_ccache); }
    
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
        principal = __KLGetKerberos5PrincipalFromPrincipal (inPrincipal);
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

// ---------------------------------------------------------------------------

static KLStatus __KLStoreKerberos4CredentialsInCCache (KLPrincipal       inPrincipal,
                                                       CREDENTIALS      *inV4Creds, 
                                                       KLCCache          inCCache)
{
    KLStatus             err = klNoErr;
    cc_ccache_t          cc_ccache = NULL;
    cc_credentials_union creds;
    cc_credentials_v4_t  ccV4Creds;
    char                *principalV4String = NULL;
    
    if (inV4Creds == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V4, &principalV4String);
    }
    
    if (!err) {
        krb5_ccache v4CCache = (inCCache->alternateV4Cache != NULL) ? inCCache->alternateV4Cache : inCCache->cache;
        
        err = __KLGetCCAPICCacheForKrb5CCache (inCCache->context, v4CCache, &cc_ccache);
    }
    
    if (!err) {
        err = cc_ccache_set_principal (cc_ccache, cc_credentials_v4, principalV4String);
    }
    
    if (!err) {
        creds.version                    = cc_credentials_v4;
        creds.credentials.credentials_v4 = &ccV4Creds;
        
        strcpy (ccV4Creds.principal,           inV4Creds->pname);
        strcpy (ccV4Creds.principal_instance,  inV4Creds->pinst);
        strcpy (ccV4Creds.service,             inV4Creds->service);
        strcpy (ccV4Creds.service_instance,    inV4Creds->instance);
        strcpy (ccV4Creds.realm,               inV4Creds->realm);
        memmove (ccV4Creds.session_key,        inV4Creds->session, sizeof (des_cblock));
        ccV4Creds.kvno =                       inV4Creds->kvno;
        ccV4Creds.string_to_key_type =         cc_v4_stk_unknown;
        ccV4Creds.issue_date =                 inV4Creds->issue_date;
        ccV4Creds.address =                    inV4Creds->address;
        // Fix the lifetime to something CCAPI v4 likes:
        ccV4Creds.lifetime =                   (int) (krb_life_to_time (inV4Creds->issue_date, inV4Creds->lifetime) 
                                                      - inV4Creds->issue_date);
        ccV4Creds.ticket_size =                inV4Creds->ticket_st.length;
        memmove (ccV4Creds.ticket,             inV4Creds->ticket_st.dat, inV4Creds->ticket_st.length);
    }
    
    if (!err) {
        err = cc_ccache_store_credentials (cc_ccache, &creds);
    }
    
    if (principalV4String != NULL) { KLDisposeString (principalV4String); }
    if (cc_ccache         != NULL) { cc_ccache_release (cc_ccache); }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

static KLStatus __KLCreateNewCCAPICCache (KLPrincipal        inPrincipal, 
                                          KLKerberosVersion  inVersion,
                                          KLCCache          *outCCache)
{
    KLStatus          err = klNoErr;
    KLCCache          ccache = NULL;
    cc_context_t      cc_context = NULL;
    cc_ccache_t       cc_ccache = NULL;
    char             *principalString = NULL;
    KLBoolean         hasV5 = (inVersion & kerberosVersion_V5);
    KLBoolean         hasV4 = (inVersion & kerberosVersion_V4);
    KLKerberosVersion klVersion = hasV5 ? kerberosVersion_V5 : kerberosVersion_V4;
    cc_int32          version   = hasV5 ? cc_credentials_v5  : cc_credentials_v4;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    if (!hasV5 && !hasV4   ) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = KLGetStringFromPrincipal (inPrincipal, klVersion, &principalString);
    }
    
    if (!err) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (!err) {
        err = cc_context_create_new_ccache (cc_context, version, principalString, &cc_ccache);
    }
    
    if (!err) {
        ccache = (KLCCache) calloc (1, sizeof (CCache));
        if (ccache == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    if (!err) {
        err = krb5_init_context (&ccache->context);
    }
    
    if (!err) {
        err = __KLGetKrb5CCacheForCCAPICCache (ccache->context, cc_ccache, &ccache->cache);
    }
    
    if (!err) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { __KLDestroyCCache (ccache); }
    }
    
    if (principalString != NULL) { KLDisposeString (principalString); }   
    if (cc_ccache       != NULL) { cc_ccache_release (cc_ccache); }    
    if (cc_context      != NULL) { cc_context_release (cc_context); }    
        
    return KLError_ (err);    
}

// ---------------------------------------------------------------------------

KLStatus __KLCreateNewCCacheWithCredentials (KLPrincipal    inPrincipal, 
                                             krb5_creds    *inV5Creds, 
                                             CREDENTIALS   *inV4Creds,
                                             KLCCache      *outCCache)
{
    KLStatus err = klNoErr;
    KLCCache ccache = NULL;
    
    if (inPrincipal                       == NULL)  { err = KLError_ (klParameterErr); }
    if (outCCache                         == NULL)  { err = KLError_ (klParameterErr); }
    if ((inV5Creds == NULL) && (inV4Creds == NULL)) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        KLKerberosVersion klVersion = 0;
        if (inV5Creds != NULL) { klVersion |= kerberosVersion_V5; }
        if (inV4Creds != NULL) { klVersion |= kerberosVersion_V4; }
        
        err = __KLCreateNewCCAPICCache (inPrincipal, klVersion, &ccache);
    }
    
    if (!err && (inV5Creds != NULL)) {
        err = __KLStoreKerberos5CredentialsInCCache (inPrincipal, inV5Creds, ccache);
    }
    
    
    if (!err && (inV4Creds != NULL)) {
        err = __KLStoreKerberos4CredentialsInCCache (inPrincipal, inV4Creds, ccache);
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
        // If the cache is v5-only, look for matching v4 tickets in the CCAPI
        if (!__KLKrb5CCacheIsCCAPICCache (ccache->context, ccache->cache)) {
            krb5_principal principal = NULL;
            
            if (krb5_cc_get_principal (ccache->context, ccache->cache, &principal) == klNoErr) {
                KLPrincipal klPrincipal = NULL;
                
                err = __KLCreatePrincipalFromKerberos5Principal (principal, &klPrincipal);
                
                if (!err) {
                    // Ignore error because it doesn't matter if we don't find anything
                    __KLGetCCAPICCacheForPrincipal (klPrincipal, kerberosVersion_V4, ccache->context, &ccache->alternateV4Cache);
                }
                
                if (klPrincipal != NULL) { KLDisposePrincipal (klPrincipal); }
            }
            
            if (principal != NULL) { krb5_free_principal (ccache->context, principal); }
        }
    }

    if (!err) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { __KLCloseCCache (ccache); }
    }

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
    
    // If the v5 cache is not a CCAPI ccache, check for a matching principal.
    // If there isn't one, get the CCAPI default ccache for v4:
    if (!err) {
        if (!__KLKrb5CCacheIsCCAPICCache (ccache->context, ccache->cache)) { 
            KLStatus terr = klNoErr;
            krb5_principal defaultPrincipal = NULL;
            
            if (krb5_cc_get_principal (ccache->context, ccache->cache, &defaultPrincipal) == klNoErr) {
                KLPrincipal klPrincipal = NULL;
                
                terr = __KLCreatePrincipalFromKerberos5Principal (defaultPrincipal, &klPrincipal);
                
                if (!terr) {
                    terr = __KLGetCCAPICCacheForPrincipal (klPrincipal, kerberosVersion_V4, 
                                                           ccache->context, &ccache->alternateV4Cache);
                }
                
                if (klPrincipal != NULL) { KLDisposePrincipal (klPrincipal); }
            } else {
                cc_ccache_t  defaultCCache = NULL;
                cc_context_t cc_context = NULL;
                
                terr = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
                
                if (!terr) {
                    terr = cc_context_open_default_ccache (cc_context, &defaultCCache);
                }
                
                if (!terr) {
                    terr = __KLGetKrb5CCacheForCCAPICCache (ccache->context, defaultCCache, &ccache->alternateV4Cache);
                }
                
                if (defaultCCache != NULL) { cc_ccache_release (defaultCCache); }
                if (cc_context    != NULL) { cc_context_release (cc_context); }
            }

            if (defaultPrincipal != NULL) { krb5_free_principal (ccache->context, defaultPrincipal); }
        }
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
        krb5_ccache defaultCCache = NULL;
        
        if (krb5_cc_default (ccache->context, &defaultCCache) == klNoErr) {
            if (!__KLKrb5CCacheIsCCAPICCache (ccache->context, defaultCCache)) {
                krb5_principal defaultPrincipal = NULL;
                
                if (krb5_cc_get_principal (ccache->context, defaultCCache, &defaultPrincipal) == klNoErr) {
                    if (krb5_principal_compare (ccache->context, defaultPrincipal, 
                                                __KLGetKerberos5PrincipalFromPrincipal (inPrincipal))) {
                        ccache->cache = defaultCCache;
                        defaultCCache = NULL;  // don't close
                    }
                }
                
                if (defaultPrincipal != NULL) { krb5_free_principal (ccache->context, defaultPrincipal); }
            }
        }

        if (defaultCCache != NULL) { __KLCloseKrb5CCache (ccache->context, defaultCCache); }
    }
    
    // Now walk the CCAPI ccaches looking for either a v4 equivalent cache 
    // or the main v5 one if we didn't already find that
    if (!err) {
        err = __KLGetCCAPICCacheForPrincipal (inPrincipal,     ((ccache->cache == NULL) ? kerberosVersion_Any : kerberosVersion_V4), 
                                              ccache->context, ((ccache->cache == NULL) ? &ccache->cache      : &ccache->alternateV4Cache));
        if (ccache->cache == NULL) { 
            err = KLError_ (klPrincipalDoesNotExistErr); 
        } else {
            err = klNoErr;  // we got a ccache from the v5 default (presumably a FILE one)
        }
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
        if (inCCache->cache            != NULL) { __KLCloseKrb5CCache (inCCache->context, inCCache->cache); }
        if (inCCache->alternateV4Cache != NULL) { __KLCloseKrb5CCache (inCCache->context, inCCache->alternateV4Cache); }
        if (inCCache->context          != NULL) { krb5_free_context (inCCache->context); }
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
        if (inCCache->cache            != NULL) { __KLDestroyKrb5CCache (inCCache->context, inCCache->cache); }
        if (inCCache->alternateV4Cache != NULL) { __KLDestroyKrb5CCache (inCCache->context, inCCache->alternateV4Cache); }
        if (inCCache->context          != NULL) { krb5_free_context (inCCache->context); }
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
// Overwrites the v4 tickets in inDestinationCCache with any in inSourceCCache
// Does nothing if inSourceCCache == inDestinationCCache
// Creates inDestinationCCache if it is a pointer to NULL

static KLStatus __KLOverwriteV4Credentials (krb5_context  inSourceContext, 
                                            krb5_ccache   inSourceCCache, 
                                            krb5_context  inDestinationContext, 
                                            krb5_ccache  *ioDestinationCCache)
{
    KLStatus err = klNoErr;
    
    if (inSourceContext      == NULL)  { err = KLError_ (klParameterErr); }
    if (inSourceCCache       == NULL)  { err = KLError_ (klParameterErr); }
    if (inDestinationContext == NULL)  { err = KLError_ (klParameterErr); }
    if (ioDestinationCCache  == NULL)  { err = KLError_ (klParameterErr); }
    
    if (!err && !__KLKrb5CCachesAreEqual (inSourceContext, inSourceCCache, inDestinationContext, *ioDestinationCCache)) {
        // Caches are different.  clear out any v5 credentials in inDestination CCache
        cc_ccache_t cc_sourceCCache = NULL;
        cc_ccache_t cc_destinationCCache = NULL;
        cc_string_t principalString = NULL;
        
        err = __KLGetCCAPICCacheForKrb5CCache (inSourceContext, inSourceCCache, &cc_sourceCCache);
        
        if (!err) {
            err = cc_ccache_get_principal (cc_sourceCCache, cc_credentials_v4, &principalString);
        }

        if (!err) {
            if (*ioDestinationCCache == NULL) {
                // create the destination ccache
                cc_context_t cc_context = NULL;
                
                err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
                
                if (!err) {
                    err = cc_context_create_new_ccache (cc_context, cc_credentials_v4, principalString->data, &cc_destinationCCache);
                }
                
                if (!err) {
                    err = __KLGetKrb5CCacheForCCAPICCache (inDestinationContext, cc_destinationCCache, ioDestinationCCache);
                }
                
                if (cc_context != NULL) { cc_context_release (cc_context); }
                
            } else {
                // the destination already exists; remove any old v4 credentials in ioDestinationCCache
                err = __KLGetCCAPICCacheForKrb5CCache (inDestinationContext, *ioDestinationCCache, &cc_destinationCCache);
                    
                if (!err) {
                    cc_credentials_iterator_t iterator = NULL;
                    
                    err = cc_ccache_new_credentials_iterator (cc_destinationCCache, &iterator);
                    
                    while (!err) {
                        cc_credentials_t cc_creds = NULL;
                        
                        if (!err) {
                            err = cc_credentials_iterator_next (iterator, &cc_creds);
                        }
                        
                        if (!err) {
                            if (cc_creds->data->version == cc_credentials_v4) {
                                err = cc_ccache_remove_credentials (cc_destinationCCache, cc_creds);
                            }
                        }
                        
                        if (cc_creds != NULL) { cc_credentials_release (cc_creds); }
                    }
                    
                    if (err == ccIteratorEnd) { err = klNoErr; }
                    
                    if (iterator  != NULL) { cc_credentials_iterator_release (iterator); }                                              
                }
                
                if (!err) {
                    err = cc_ccache_set_principal (cc_destinationCCache, cc_credentials_v4, principalString->data);
                }
            }
        }
            
        if (!err) {
            // copy v4 credentials from the source to the destination
            cc_credentials_iterator_t iterator = NULL;
            
            err = cc_ccache_new_credentials_iterator (cc_sourceCCache, &iterator);
            
            while (!err) {
                cc_credentials_t cc_creds = NULL;
                
                if (!err) {
                    err = cc_credentials_iterator_next (iterator, &cc_creds);
                }
                
                if (!err) {
                    if (cc_creds->data->version == cc_credentials_v4) {
                        err = cc_ccache_store_credentials (cc_destinationCCache, cc_creds->data);
                    }
                }
                
                if (cc_creds != NULL) { cc_credentials_release (cc_creds); }
            }
            
            if (err == ccIteratorEnd) { err = klNoErr; }
            
            if (iterator  != NULL) { cc_credentials_iterator_release (iterator); }                                              
            
        }
        
        if (principalString      != NULL) { cc_string_release (principalString); }
        if (cc_sourceCCache      != NULL) { cc_ccache_release (cc_sourceCCache); }
        if (cc_destinationCCache != NULL) { cc_ccache_release (cc_destinationCCache); }
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLMoveCCache (KLCCache inSourceCCache, KLCCache inDestinationCCache)
{
    KLStatus    err = klNoErr;
    KLBoolean   sourceIsCCAPI = FALSE;        
    KLBoolean   destinationIsCCAPI = FALSE;
    krb5_ccache sourceV4CCache = NULL;
    KLBoolean   sourceHasV5 = FALSE;
    KLBoolean   sourceHasV4 = FALSE;
    
    if (inSourceCCache      == NULL)  { err = KLError_ (klParameterErr); }
    if (inDestinationCCache == NULL)  { err = KLError_ (klParameterErr); }
    
    if (!err) {
        sourceIsCCAPI      = __KLKrb5CCacheIsCCAPICCache (inSourceCCache->context,      inSourceCCache->cache);        
        destinationIsCCAPI = __KLKrb5CCacheIsCCAPICCache (inDestinationCCache->context, inDestinationCCache->cache);
 
        sourceV4CCache = ((inSourceCCache->alternateV4Cache != NULL) ? inSourceCCache->alternateV4Cache : inSourceCCache->cache);
        sourceHasV5 = __KLKrb5CacheHasV5Credentials (inSourceCCache->context, inSourceCCache->cache);
        sourceHasV4 = __KLKrb5CacheHasV4Credentials (inSourceCCache->context, sourceV4CCache);
        
        // Sanity check to prevent destroying inDestinationCCache on bad input
        if (!sourceHasV5 && !sourceHasV4) { err = KLError_ (klCacheDoesNotExistErr); } 
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
                    KLKerberosVersion version = 0;
                    if (sourceHasV5) { version |= kerberosVersion_V5; }
                    if (sourceHasV4) { version |= kerberosVersion_V4; }
                    
                    err = __KLCreateNewCCAPICCache (principal, version, &temporaryCCache);
                }
                
                if (!err && sourceHasV5) {
                    err = __KLOverwriteV5Credentials (inSourceCCache->context, inSourceCCache->cache, 
                                                      temporaryCCache->context, temporaryCCache->cache);
                }
                
                if (!err && sourceHasV4) {
                    err = __KLOverwriteV4Credentials (inSourceCCache->context, sourceV4CCache, 
                                                      temporaryCCache->context, &temporaryCCache->cache);
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
            // Destination is split cache: 
            
            if (!err && sourceHasV5) {
                err = __KLOverwriteV5Credentials (inSourceCCache->context, inSourceCCache->cache, 
                                                  inDestinationCCache->context, inDestinationCCache->cache);
            }
            
            if (!err && sourceHasV4) {
                err = __KLOverwriteV4Credentials (inSourceCCache->context, sourceV4CCache, 
                                                  inDestinationCCache->context, &inDestinationCCache->alternateV4Cache);
            }
            
            if (!err) {
                if (!sourceHasV4) {
                    // inDestinationCCache->alternateV4Cache can only contain v4 credentials and there aren't any.  Get rid of it.
                    // Only destroy if it's not one of the source caches!
                    if (!__KLKrb5CCachesAreEqual (inDestinationCCache->context, inDestinationCCache->alternateV4Cache, 
                                                  inSourceCCache->context, inSourceCCache->cache) &&
                        !__KLKrb5CCachesAreEqual (inDestinationCCache->context, inDestinationCCache->alternateV4Cache, 
                                                  inSourceCCache->context, inSourceCCache->alternateV4Cache)) {
                        __KLDestroyKrb5CCache (inDestinationCCache->context, inDestinationCCache->alternateV4Cache);
                    } else {
                        __KLCloseKrb5CCache (inDestinationCCache->context, inDestinationCCache->alternateV4Cache);                        
                    }
                    inDestinationCCache->alternateV4Cache = NULL;
                }
                
                if (!sourceHasV5) {
                    // inDestinationCCache->cache can only contain v5 credentials and there aren't any.  Replace with v4 cache.
                    // Only destroy if it's not one of the source caches!
                    if (!__KLKrb5CCachesAreEqual (inDestinationCCache->context, inDestinationCCache->cache, 
                                                  inSourceCCache->context, inSourceCCache->cache) &&
                        !__KLKrb5CCachesAreEqual (inDestinationCCache->context, inDestinationCCache->cache, 
                                                  inSourceCCache->context, inSourceCCache->alternateV4Cache)) {
                        __KLDestroyKrb5CCache (inDestinationCCache->context, inDestinationCCache->cache);
                    } else {
                        __KLCloseKrb5CCache (inDestinationCCache->context, inDestinationCCache->cache);                        
                    }
                    inDestinationCCache->cache = inDestinationCCache->alternateV4Cache;
                    inDestinationCCache->alternateV4Cache = NULL;
                }
            }
        }
        
        if (!err) {
            // Only destroy the source credentials if we copied to a *different* ccache
            if ((inSourceCCache->cache != NULL) &&
                !__KLKrb5CCachesAreEqual (inSourceCCache->context, inSourceCCache->cache, 
                                          inDestinationCCache->context, inDestinationCCache->cache) &&
                !__KLKrb5CCachesAreEqual (inSourceCCache->context, inSourceCCache->cache, 
                                          inDestinationCCache->context, inDestinationCCache->alternateV4Cache)) {
                __KLDestroyKrb5CCache (inSourceCCache->context, inSourceCCache->cache);
                inSourceCCache->cache = NULL;
            }
            
            if ((inSourceCCache->alternateV4Cache != NULL) &&
                !__KLKrb5CCachesAreEqual (inSourceCCache->context, inSourceCCache->alternateV4Cache, 
                                          inDestinationCCache->context, inDestinationCCache->cache) &&
                !__KLKrb5CCachesAreEqual (inSourceCCache->context, inSourceCCache->alternateV4Cache, 
                                          inDestinationCCache->context, inDestinationCCache->alternateV4Cache)) {
                __KLDestroyKrb5CCache (inSourceCCache->context, inSourceCCache->alternateV4Cache);
                inSourceCCache->alternateV4Cache = NULL;
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
                
                if ((*ioCCache)->alternateV4Cache != NULL) {
                    err = __KLSetKrb5CCacheToCCAPIDefault ((*ioCCache)->context, (*ioCCache)->alternateV4Cache);
                }
            }
            
            if (environmentCCache != NULL) { __KLCloseCCache (environmentCCache); }
            
        } else {
            // KRB5CCNAME is not set so we will store tickets in the CCAPI default ccache

            if (__KLKrb5CCacheIsCCAPICCache ((*ioCCache)->context, (*ioCCache)->cache)) {
                // inCCache is a CCAPI ccache:
                err = __KLSetKrb5CCacheToCCAPIDefault ((*ioCCache)->context, (*ioCCache)->cache);
            
            } else if ((*ioCCache)->alternateV4Cache != NULL) {
                // The v4 ccache exists... copy the v5 credentials into the v4 ccache and set that to the default
                err = __KLSetKrb5CCacheToCCAPIDefault ((*ioCCache)->context, (*ioCCache)->alternateV4Cache);
                
                if (!err) {
                    err = krb5_cc_copy_creds ((*ioCCache)->context, (*ioCCache)->cache, (*ioCCache)->alternateV4Cache);   
                }
                
                if (!err) {
                    err = __KLDestroyKrb5CCache ((*ioCCache)->context, (*ioCCache)->cache);
                }
                
                if (!err) {
                    (*ioCCache)->cache = (*ioCCache)->alternateV4Cache;
                    (*ioCCache)->alternateV4Cache = NULL;
                }
            
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
                        err = __KLCreateNewCCAPICCache (principal, kerberosVersion_V5, &destinationCCache);
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
// This function checks to see if they really exist.  It handles CCAPI ccaches
// separately because they may contain v4 tickets.

KLStatus __KLCCacheExists (KLCCache inCCache)
{
    KLStatus err = klNoErr;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        if (!__KLKrb5CCacheExists (inCCache->context, inCCache->cache) &&
            !__KLKrb5CCacheExists (inCCache->context, inCCache->alternateV4Cache)) {
            err = KLError_ (klCacheDoesNotExistErr);
        }
    }
    
    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLCCacheHasKerberos4 (KLCCache inCCache) 
{
    KLStatus err = klNoErr;
    KLBoolean hasKerberos4 = false;
    cc_ccache_t cc_ccache = NULL;
    cc_string_t principalString = NULL;
    KLPrincipal principal = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        krb5_ccache v4CCache = (inCCache->alternateV4Cache != NULL) ? inCCache->alternateV4Cache : inCCache->cache;
        err = __KLGetCCAPICCacheForKrb5CCache (inCCache->context, v4CCache, &cc_ccache);
    }
    
    if (!err) {
        err = cc_ccache_get_principal (cc_ccache, cc_credentials_v4, &principalString);
    }
    
    if (!err) {
        err = KLCreatePrincipalFromString (principalString->data, kerberosVersion_V4, &principal);
    }
    
    if (!err) {
        hasKerberos4 = __KLPrincipalHasKerberos4Profile (principal);
    }
    
    if (principal       != NULL) { KLDisposePrincipal (principal); }
    if (principalString != NULL) { cc_string_release (principalString); }
    if (cc_ccache       != NULL) { cc_ccache_release (cc_ccache); }
    
    return hasKerberos4;
}

// ---------------------------------------------------------------------------

KLBoolean __KLCCacheHasKerberos5 (KLCCache inCCache) 
{
    KLStatus       err = klNoErr;
    KLBoolean      hasKerberos5 = false;
    krb5_principal v5Principal = NULL;
    KLPrincipal    klPrincipal = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = krb5_cc_get_principal (inCCache->context, inCCache->cache, &v5Principal);
    }
    
    if (!err) {
        err = __KLCreatePrincipalFromKerberos5Principal (v5Principal, &klPrincipal);
    }
    
    if (!err) {
        hasKerberos5 = __KLPrincipalHasKerberos5Profile (klPrincipal);
    }
    
    if (klPrincipal != NULL) { KLDisposePrincipal (klPrincipal); }
    if (v5Principal != NULL) { krb5_free_principal (inCCache->context, v5Principal); }
    
    return hasKerberos5;
}


#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetValidV5TgtForCCache (KLCCache     inCCache, 
                                     krb5_creds **outV5Creds)
{
    KLStatus err = klNoErr;
    KLBoolean found = FALSE;
    krb5_cc_cursor cursor = NULL;
        
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    // outV5Creds maybe NULL if the caller only wants to get success/error
    
    if (!err) {
        err = krb5_cc_start_seq_get (inCCache->context, inCCache->cache, &cursor);
    }
    
    while (!err && !found) {
        krb5_creds  creds;
        KLBoolean   freeCreds = FALSE;
        KLPrincipal servicePrincipal = NULL;
        
        err = krb5_cc_next_cred (inCCache->context, inCCache->cache, &cursor, &creds);
        if (!err) { freeCreds = TRUE; }
        
        if (!err) {
            err = __KLCreatePrincipalFromKerberos5Principal (creds.server, &servicePrincipal);
        }
        
        if (!err) {
            if (__KLPrincipalIsTicketGrantingService (servicePrincipal, kerberosVersion_V5)) {
                err = __KLV5CredsAreValid (inCCache->context, &creds);

                if (!err) {
                    found = true;      // Break out of the loop
                    if (outV5Creds != NULL) {
                        err = krb5_copy_creds (inCCache->context, &creds, outV5Creds);
                    }
                }
            }
        }
        
        if (servicePrincipal != NULL) { KLDisposePrincipal (servicePrincipal); }
        if (freeCreds               ) { krb5_free_cred_contents (inCCache->context, &creds); }
    }
    if (err == KRB5_CC_END) { err = KLError_ (klNoCredentialsErr); }
    
    if (cursor != NULL) { krb5_cc_end_seq_get (inCCache->context, inCCache->cache, &cursor); }
    
    return KLError_ (err);
}


// ---------------------------------------------------------------------------

KLStatus __KLGetValidV4TgtForCCache (KLCCache     inCCache,  
                                     CREDENTIALS *outV4Creds)
{
    KLStatus err = klNoErr;
    KLBoolean found = false;
    cc_ccache_t cc_ccache = NULL;
    cc_credentials_iterator_t iterator = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    // outV4Creds maybe NULL if the caller only wants to get success/error
    
    if (!err) {
        krb5_ccache v4CCache = (inCCache->alternateV4Cache != NULL) ? inCCache->alternateV4Cache : inCCache->cache;
        err = __KLGetCCAPICCacheForKrb5CCache (inCCache->context, v4CCache, &cc_ccache);
    }
    
    if (!err) {
        err = cc_ccache_new_credentials_iterator (cc_ccache, &iterator);
    }
    
    while (!found && !err) {
        cc_credentials_t cc_creds = NULL;
        KLPrincipal      servicePrincipal = NULL;
        
        if (!err) {
            err = cc_credentials_iterator_next (iterator, &cc_creds);
        }
        
        if (!err) {
            if (cc_creds->data->version == cc_credentials_v4) {
                err = __KLCreatePrincipalFromTriplet (cc_creds->data->credentials.credentials_v4->service, 
                                                      cc_creds->data->credentials.credentials_v4->service_instance,
                                                      cc_creds->data->credentials.credentials_v4->realm,
                                                      kerberosVersion_V4, &servicePrincipal);

                if (!err) {
                    // Credentials match version... is it a valid tgs principal?
                    if (__KLPrincipalIsTicketGrantingService (servicePrincipal, kerberosVersion_V4)) {
                        CREDENTIALS creds;
                        cc_credentials_v4_t *cc_creds_v4 = cc_creds->data->credentials.credentials_v4;

                        strcpy (creds.pname,           cc_creds_v4->principal);
                        strcpy (creds.pinst,           cc_creds_v4->principal_instance);
                        strcpy (creds.service,         cc_creds_v4->service);
                        strcpy (creds.instance,        cc_creds_v4->service_instance);
                        strcpy (creds.realm,           cc_creds_v4->realm);
                        memmove (creds.session,        cc_creds_v4->session_key, sizeof (des_cblock));
                        creds.kvno =                   cc_creds_v4->kvno;
                        creds.stk_type =               cc_creds_v4->string_to_key_type;
                        creds.issue_date =             cc_creds_v4->issue_date;
                        creds.address =                cc_creds_v4->address;
                        // Fix the lifetime to something krb4 likes:
                        creds.lifetime = krb_time_to_life (cc_creds_v4->issue_date, cc_creds_v4->lifetime + cc_creds_v4->issue_date);
                        creds.ticket_st.length =       cc_creds_v4->ticket_size;
                        memmove (creds.ticket_st.dat,  cc_creds_v4->ticket, cc_creds_v4->ticket_size);
                        
                        err = __KLV4CredsAreValid (inCCache->context, &creds);
                        if (!err) {
                            found = true;           // Break out of the loop
                            if (outV4Creds != NULL) { // Return the creds to the caller
                                memcpy (outV4Creds, &creds, sizeof (CREDENTIALS));
                            }
                        }
                    }
                }
            }
        }
        
        if (servicePrincipal != NULL) { KLDisposePrincipal (servicePrincipal); }
        if (cc_creds         != NULL) { cc_credentials_release (cc_creds); }
    }
    
    if ((err == ccIteratorEnd) || (err == ccErrCCacheNotFound)) { 
        err = KLError_ (klNoCredentialsErr); 
    }
    
    if (iterator  != NULL) { cc_credentials_iterator_release (iterator); }
    if (cc_ccache != NULL) { cc_ccache_release (cc_ccache); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLCacheHasValidTickets (KLCCache inCCache, KLKerberosVersion inVersion)
{
    KLStatus         err = klNoErr;
    KLStatus         v5Err = klNoErr;
    KLStatus         v4Err = klNoErr;
    KLPrincipal      principal = NULL;
	
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }

    // Get the cache principal, if the cache exists:
    if (!err) {
        err = __KLGetPrincipalForCCache (inCCache, &principal);
    }

    if (!err) {
        // What credentials are in the ccache?
        v5Err = __KLGetValidV5TgtForCCache (inCCache, NULL);
        v4Err = __KLGetValidV4TgtForCCache (inCCache, NULL);

        // What credentials does the caller want?
        switch (inVersion) {
            case kerberosVersion_V5:
                err = KLError_ (v5Err);
                break;
                
            case kerberosVersion_V4:
                err = KLError_ (v4Err);
                break;
                
            case (kerberosVersion_V5 | kerberosVersion_V4):
                if (v5Err) {
                    err = KLError_ (v5Err); // favor v5 errors
                } else if (v4Err) {
                    err = KLError_ (v4Err); // then v4 errors
                } else {
                    err = KLError_ (klNoErr); // no error
                }
                break;
                
            case kerberosVersion_All:
                if (__KLPrincipalHasKerberos5Profile (principal) && v5Err) {
                    err = KLError_ (v5Err);
                } else if (__KLPrincipalHasKerberos4Profile (principal) && v4Err) {
                    err = KLError_ (v4Err);
                } else if (__KLPrincipalHasKerberos5Profile (principal) || !__KLPrincipalHasKerberos4Profile (principal)) {
                    err = KLError_ (v5Err); // has v5 profile or neither
                } else {
                    err = KLError_ (v4Err);
                }
                break;
                
            case kerberosVersion_Any:
                if (v4Err && v5Err) {
                    if (__KLPrincipalHasKerberos5Profile (principal) || !__KLPrincipalHasKerberos4Profile (principal)) {
                        err = KLError_ (v5Err); // has v5 profile or neither
                    } else {
                        err = KLError_ (v4Err);
                    }
                } else {
                    err = KLError_ (klNoErr);
                }
                break; 
                
            default:
                err = KLError_ (klInvalidVersionErr);
                break;
        }
    }

    if (principal != NULL) { KLDisposePrincipal (principal); }

    return KLError_ (err);
}	

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheExpirationTime (KLCCache           inCCache, 
                                      KLKerberosVersion  inVersion, 
                                      KLTime            *outExpirationTime)
{
    KLStatus err = klNoErr;
    KLTime   v5ExpirationTime = (inVersion == kerberosVersion_Any) ? 0 : 0xFFFFFFFF;
    KLTime   v4ExpirationTime = (inVersion == kerberosVersion_Any) ? 0 : 0xFFFFFFFF;
    
    if (!err) {
        err = __KLCacheHasValidTickets (inCCache, inVersion);
    }
    
    if (!err) {
        krb5_creds *v5Creds = NULL;
        
        if (__KLGetValidV5TgtForCCache (inCCache, &v5Creds) == klNoErr) {
            v5ExpirationTime = v5Creds->times.endtime;
        }
        
        if (v5Creds != NULL) { krb5_free_creds (inCCache->context, v5Creds); }
    }
    
    if (!err) {
        CREDENTIALS v4Creds;
        
        if (__KLGetValidV4TgtForCCache (inCCache, &v4Creds) == klNoErr) {
            v4ExpirationTime = krb_life_to_time (v4Creds.issue_date, v4Creds.lifetime);
        }
    }
    
    if (!err) {
        switch (inVersion) {
            case (kerberosVersion_V5 | kerberosVersion_V4):
            case kerberosVersion_All:
                // These cases are all the same because we know that some tickets are there
                // return the smaller expiration time:
                if (v5ExpirationTime < v4ExpirationTime) {
                    *outExpirationTime = v5ExpirationTime;
                } else {
                    *outExpirationTime = v4ExpirationTime;
                }
                break;
                
            case kerberosVersion_Any:
                // Return max expiration time (note initialization code special case):
                if (v5ExpirationTime > v4ExpirationTime) {
                    *outExpirationTime = v5ExpirationTime;
                } else {
                    *outExpirationTime = v4ExpirationTime;
                }
                break;
                
            case kerberosVersion_V5:
                *outExpirationTime = v5ExpirationTime;
                break;
                
            case kerberosVersion_V4:
                *outExpirationTime = v4ExpirationTime;
                break;
                
            default:
                err = KLError_ (klInvalidVersionErr);
        }
    }
    
    // This is usually, but not always a bug
    if (!err && (*outExpirationTime == 0 || *outExpirationTime == 0xFFFFFFFF)) {
        dprintf ("__KLGetCCacheExpirationTime returning suspicious expiration time %d", *outExpirationTime);
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheStartTime (KLCCache           inCCache, 
                                 KLKerberosVersion  inVersion, 
                                 KLTime            *outStartTime)
{
    KLStatus err = klNoErr;
    KLTime   v5StartTime = (inVersion == kerberosVersion_Any) ? 0xFFFFFFFF : 0;
    KLTime   v4StartTime = (inVersion == kerberosVersion_Any) ? 0xFFFFFFFF : 0;
    
    if (inCCache     == NULL) { err = KLError_ (klParameterErr); }
    if (outStartTime == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        err = __KLCacheHasValidTickets (inCCache, inVersion);
    }
    
    if (!err) {
        krb5_creds *v5Creds = NULL;
        
        if (__KLGetValidV5TgtForCCache (inCCache, &v5Creds) == klNoErr) {
            v5StartTime = v5Creds->times.starttime;
        }
        
        if (v5Creds != NULL) { krb5_free_creds (inCCache->context, v5Creds); }
    }
    
    if (!err) {
        CREDENTIALS v4Creds;
        
        if (__KLGetValidV4TgtForCCache (inCCache, &v4Creds) == klNoErr) {
            v4StartTime = v4Creds.issue_date;
        }
    }
    
    if (!err) {
        switch (inVersion) {
            case (kerberosVersion_V5 | kerberosVersion_V4):
            case kerberosVersion_All:
                // These cases are all the same because we know that the tickets are there
                // return the larger start time:
                if (v5StartTime > v4StartTime) {
                    *outStartTime = v5StartTime;
                } else {
                    *outStartTime = v4StartTime;
                }
                break;
                
            case kerberosVersion_Any:
                // Return min start time (note initialization code special case):
                if (v5StartTime < v4StartTime) {
                    *outStartTime = v5StartTime;
                } else {
                    *outStartTime = v4StartTime;
                }
                break;
                
            case kerberosVersion_V5:
                *outStartTime = v5StartTime;
                break;
                
            case kerberosVersion_V4:
                *outStartTime = v4StartTime;
                break;
                
            default:
                err = KLError_ (klInvalidVersionErr);
        }
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
    krb5_ccache  mainCCache = NULL;
    KLPrincipal  principal = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        if (__KLKrb5CCacheExists (inCCache->context, inCCache->cache)) {
            mainCCache = inCCache->cache;
        } else if (inCCache->alternateV4Cache != NULL) {
            mainCCache = inCCache->alternateV4Cache;            
        } else {
            err = KLError_ (klCacheDoesNotExistErr);
        }
    }

    if (!err) {
        if (__KLKrb5CCacheIsCCAPICCache (inCCache->context, mainCCache)) {
            // Either the main ccache is a CCAPI cache or it is empty
            cc_ccache_t       cc_ccache = NULL;
            cc_uint32         version = cc_credentials_v5;
            cc_string_t       principalString = NULL;
            KLKerberosVersion klVersion = kerberosVersion_V5;
            
            err = __KLGetCCAPICCacheForKrb5CCache (inCCache->context, mainCCache, &cc_ccache);
            
            if (!err) {
                err = cc_ccache_get_credentials_version (cc_ccache, &version);
            }
            
            if (!err) {
                version   = (version == cc_credentials_v4_v5) ? cc_credentials_v5  : version;
                klVersion = (version == cc_credentials_v4)    ? kerberosVersion_V4 : kerberosVersion_V5;
            }
            
            if (!err) {
                err = cc_ccache_get_principal (cc_ccache, version, &principalString);
            }
            
            if (!err) {
                err = KLCreatePrincipalFromString (principalString->data, klVersion, &principal);
            }
            
            if (principalString != NULL) { cc_string_release (principalString); }
            if (cc_ccache       != NULL) { cc_ccache_release (cc_ccache); }
            
        } else {
            // The main ccache is not a CCAPI ccache... so get the principal the normal way
            krb5_principal v5Principal = NULL;
            
            err = krb5_cc_get_principal (inCCache->context, mainCCache, &v5Principal);
            
            if (!err) {
                err = __KLCreatePrincipalFromKerberos5Principal (v5Principal, &principal);
            }
            
            if (v5Principal != NULL) { krb5_free_principal (inCCache->context, v5Principal); }
        }
    }
    
    if (!err) {
        if (outPrincipal != NULL) {
            *outPrincipal = principal;
            principal = NULL;
        }
    }
    
    if (principal != NULL) { KLDisposePrincipal (principal); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetNameForCCacheVersion (KLCCache            inCCache,
                                      KLKerberosVersion   inVersion,
                                      char              **outName)
{
    KLStatus     err = klNoErr;
    krb5_ccache  mainCCache = NULL;
    char        *resolveName = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (!err) {
        krb5_ccache v5CCache = (__KLKrb5CCacheExists (inCCache->context, inCCache->cache) ? inCCache->cache : NULL);
        krb5_ccache v4CCache = (__KLKrb5CCacheIsCCAPICCache (inCCache->context, inCCache->cache) ? 
                                inCCache->cache : inCCache->alternateV4Cache);
        
        if (inVersion == kerberosVersion_V5) {
            mainCCache = v5CCache;
        } else if (inVersion == kerberosVersion_V4) {
            mainCCache = v4CCache;
        } else {
#warning "Favor v4 cache because v4 callers just call cc_contect_open_ccache on the name they get back"
            mainCCache = (v4CCache != NULL) ? v4CCache : v5CCache;
        }
        
        if (mainCCache == NULL) {
            err = KLError_ (klCacheDoesNotExistErr); // couldn't find cache for requested version
        }
    }
    
    if (!err) {
        char *name = NULL;
        char *type = NULL;
        
        err = __KLGetCCacheTypeAndName (inCCache->context, mainCCache, &type, &name);
        
        if (!err) {
#warning "Compat support for v4 assumes CCAPI is default ccache type"
            if (__KLKrb5CCacheIsCCAPICCache (inCCache->context, mainCCache)) {
                resolveName = name;
                name = NULL;
            } else {
                err = __KLGetCCacheResolveName (type, name, &resolveName);
            }
        }
        
        if (type != NULL) { KLDisposeString (type); }
        if (name != NULL) { KLDisposeString (name); }
    }
    
    if (!err) {
        if (outName != NULL) {
            *outName = resolveName;
            resolveName = NULL;
        }
    }
    
    if (resolveName != NULL) { KLDisposeString (resolveName); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetNameForCCache (KLCCache   inCCache, 
                               char     **outName)
{
    return KLError_ (__KLGetNameForCCacheVersion (inCCache, kerberosVersion_Any, outName));
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
