/*
 * KLCCacheManagement.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLogin/KLCCacheManagement.c,v 1.12 2003/09/11 20:57:01 lxs Exp $
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


static KLStatus __KLGetUnusedKerberos5Context (krb5_context *outContext);
static KLStatus __KLV4CredsAreValid (const cc_credentials_t inCreds);
static KLStatus __KLV5CredsAreValid (const cc_credentials_t inCreds);
static KLStatus __KLAddressIsValid (const krb5_address* address);
KLStatus __KLFreeAddressList (void);

#pragma mark -

// ---------------------------------------------------------------------------

// Many functions in krb5 don't even use krb5_contexts but take them
// These functions don't need a fresh one (which is expensive to create), 
// just give them any old crappy one, such as this one: 
//
// Note... don't use this with anything that does use the context
// or you will need to put a mutex around it!!!!

static KLStatus __KLGetUnusedKerberos5Context (krb5_context *outContext)
{
    static krb5_context sContext = NULL;
    KLStatus err = klNoErr;
    
    if (sContext == NULL) {
        err = krb5_init_context (&sContext);
    }
    
    if (err == klNoErr) {
        *outContext = sContext;
    }
    
    return KLError_ (err);
}


#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLCreateNewCCacheWithCredentials (KLPrincipal        inPrincipal,
                                             krb5_context       inContext,
                                             krb5_creds        *inV5Creds, 
                                             CREDENTIALS       *inV4Creds, 
                                             cc_ccache_t       *outCCache)
{
    KLStatus          err = klNoErr;
    cc_context_t      cc_context = NULL;
    cc_ccache_t	      ccache = NULL;
    char             *principalV5String = NULL;
    char             *principalV4String = NULL;
    cc_int32          version            = (inV5Creds == NULL) ? cc_credentials_v4  : cc_credentials_v5;
    char            **principalStringPtr = (inV5Creds == NULL) ? &principalV4String : &principalV5String;

    if (inPrincipal                       == NULL)  { err = KLError_ (klParameterErr); }
    if (outCCache                         == NULL)  { err = KLError_ (klParameterErr); }
    if ((inV5Creds == NULL) && (inV4Creds == NULL)) { err = KLError_ (klParameterErr); }

    if (inV5Creds != NULL) {
        if (err == klNoErr) {
            err = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V5, &principalV5String);
        }
    }

    if (inV4Creds != NULL) {
        if (err == klNoErr) {
            err = KLGetStringFromPrincipal (inPrincipal, kerberosVersion_V4, &principalV4String);
        }
    }

    if (err == klNoErr) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (err == klNoErr) {
        err = cc_context_create_new_ccache (cc_context, version, *principalStringPtr, &ccache);
    }
    
    if (inV5Creds != NULL) {
        if (err == klNoErr) {
            err = cc_ccache_set_principal (ccache, cc_credentials_v5, principalV5String);
        }

        if (err == klNoErr) {
            err = __KLStoreKerberos5CredentialsInCCache (inContext, inV5Creds, ccache);
        }
    }

    if (inV4Creds != NULL) {
        if (err == klNoErr) {
            err = cc_ccache_set_principal (ccache, cc_credentials_v4, principalV4String);
        }

        if (err == klNoErr) {
            err = __KLStoreKerberos4CredentialsInCCache (inV4Creds, ccache);
        }
    }

    if (err == klNoErr) {
        *outCCache = ccache;
    } else {
        if (ccache != NULL) { cc_ccache_destroy (ccache); }
    }
    
    if (principalV5String != NULL) { KLDisposeString (principalV5String); }    
    if (principalV4String != NULL) { KLDisposeString (principalV4String); }    
    if (cc_context != NULL) { cc_context_release (cc_context); }    
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLStoreKerberos5CredentialsInCCache (krb5_context inContext, krb5_creds *inV5Creds, const cc_ccache_t inCCache)
{	
    KLStatus             err = klNoErr;
    cc_credentials_union creds;
    cc_credentials_v5_t  ccV5Creds;
    char                *clientString = NULL;
    char                *serverString = NULL;
    char                *keyblockContents = NULL;
    char                *ticketData = NULL;
    char                *secondTicketData = NULL;
    cc_data            **addresses = NULL;
    cc_data            **authdata = NULL;
    krb5_int32           offset_seconds, offset_microseconds;

    // Initialization to avoid freeing random memory later:
    ccV5Creds.addresses = NULL;
    ccV5Creds.authdata = NULL;

    if (inV5Creds == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        creds.version                    = cc_credentials_v5;
        creds.credentials.credentials_v5 = &ccV5Creds;
    }
    
    if (err == klNoErr) {
        err = krb5_unparse_name (inContext, inV5Creds->client, &clientString);
    }
    
    if (err == klNoErr) {
        err = krb5_unparse_name (inContext, inV5Creds->server, &serverString);
    }

    if (err == klNoErr) {
        ticketData = (char *) malloc (sizeof (char) * inV5Creds->ticket.length);
        if (ticketData == NULL) {
            err = KLError_ (klMemFullErr);
        } else {
            memmove(ticketData, inV5Creds->ticket.data, inV5Creds->ticket.length);
        }
    }

    if (err == klNoErr) {
        keyblockContents = (char *) malloc (sizeof (char) * inV5Creds->keyblock.length);
        if (keyblockContents == NULL) { 
            err = KLError_ (klMemFullErr); 
        } else {
            memmove(keyblockContents, inV5Creds->keyblock.contents, inV5Creds->keyblock.length);
        }
    }

    if (err == klNoErr) {
        secondTicketData = (char *) malloc (sizeof (char) * inV5Creds->second_ticket.length);
        if (secondTicketData == NULL) { 
            err = KLError_ (klMemFullErr); 
        } else {
            memmove(secondTicketData, inV5Creds->second_ticket.data, inV5Creds->second_ticket.length);
        }
    }

    if (err == klNoErr) {
        err = krb5_get_time_offsets (inContext, &offset_seconds, &offset_microseconds);
    }
    
    if (err == klNoErr) {
        // Addresses: (use calloc so pointers get initialized to NULL)
        if (inV5Creds->addresses != NULL) {
            krb5_address  **addrPtr, *addr;
            cc_data       **dataPtr, *data;
            u_int32_t       numRecords = 0;
            
            // Allocate the array of pointers:
            for (addrPtr = inV5Creds->addresses; *addrPtr != NULL; numRecords++, addrPtr++) {}

            addresses = (cc_data **) calloc (numRecords + 1, sizeof (cc_data *));
            if (addresses == NULL) { err = KLError_ (klMemFullErr); }
            
            // Fill in the array, allocating the address structures:
            for (dataPtr = addresses, addrPtr = inV5Creds->addresses; *addrPtr != NULL; addrPtr++, dataPtr++) {
                
                *dataPtr = (cc_data *) calloc (1, sizeof (cc_data));
                if (*dataPtr == NULL) { err = KLError_ (klMemFullErr); break; }
                
                data     = *dataPtr;
                addr     = *addrPtr;
                
                data->type   = addr->addrtype;
                data->length = addr->length;
                data->data   = (char *) calloc (data->length, sizeof (char));
                if (data->data == NULL) { err = KLError_ (klMemFullErr); break; }

                memmove(data->data, addr->contents, data->length); // copy pointer
            }
            
            // Write terminator:
            *dataPtr = NULL;
        }
    }

    if (err == klNoErr) {
        // Authdata: (use calloc so pointers get initialized to NULL)
        if (inV5Creds->authdata != NULL) {
            krb5_authdata  **authPtr, *auth;
            cc_data        **dataPtr, *data;
            u_int32_t        numRecords = 0;
            
            // Allocate the array of pointers:
            for (authPtr = inV5Creds->authdata; *authPtr != NULL; numRecords++, authPtr++) {}

            authdata = (cc_data **) calloc (numRecords + 1, sizeof (cc_data *));
            if (authdata == NULL) { err = KLError_ (klMemFullErr); }
            
            if (err == klNoErr) {            
                // Fill in the array, allocating the address structures:
                for (dataPtr = authdata, authPtr = inV5Creds->authdata; *authPtr != NULL; authPtr++, dataPtr++) {
                
                    *dataPtr = (cc_data *) calloc (1, sizeof (cc_data));
                    if (*dataPtr == NULL) { err = KLError_ (klMemFullErr); break; }
    
                    data = *dataPtr;
                    auth = *authPtr;
                    
                    data->type   = auth->ad_type;
                    data->length = auth->length;
                    data->data   = (char *) calloc (data->length, sizeof (char));
                    if (data->data == NULL) { err = KLError_ (klMemFullErr); break; }
    
                    memmove(data->data, auth->contents, data->length); // copy pointer
                }
            }
        }
    }

    if (err == klNoErr) {
        // Client and server principals
        ccV5Creds.client = clientString;
        ccV5Creds.server = serverString;

        // Keyblock:
        ccV5Creds.keyblock.type   = inV5Creds->keyblock.enctype;
        ccV5Creds.keyblock.data   = keyblockContents;
        ccV5Creds.keyblock.length = inV5Creds->keyblock.length;

        // Times:
        ccV5Creds.authtime        = inV5Creds->times.authtime   + offset_seconds;
        ccV5Creds.starttime       = inV5Creds->times.starttime  + offset_seconds;
        ccV5Creds.endtime         = inV5Creds->times.endtime    + offset_seconds;
        ccV5Creds.renew_till      = inV5Creds->times.renew_till + offset_seconds;

        // Flags:
        ccV5Creds.is_skey         = inV5Creds->is_skey;
        ccV5Creds.ticket_flags    = inV5Creds->ticket_flags;

        // Addresses:
        ccV5Creds.addresses = addresses;
        
        // Ticket:
        ccV5Creds.ticket.length = inV5Creds->ticket.length;
        ccV5Creds.ticket.data = ticketData;

        // Second Ticket:
        ccV5Creds.second_ticket.length = inV5Creds->second_ticket.length;
        ccV5Creds.second_ticket.data = secondTicketData;

        // Authdata:
        ccV5Creds.authdata = authdata;
    }

    if (err == klNoErr) {
        err = cc_ccache_store_credentials (inCCache, &creds);
    }

    if (addresses != NULL) {
        cc_data **dataPtr, *data;
        for (dataPtr = addresses; *dataPtr != NULL; dataPtr++) {
            data = *dataPtr;
            
            if (data->data != NULL) { free (data->data); }
            if (data       != NULL) { free (data); }
        }
        free (addresses);
    }
    if (authdata != NULL) {
        cc_data **dataPtr, *data;
        for (dataPtr = authdata; *dataPtr != NULL; dataPtr++) {
            data = *dataPtr;
            
            if (data->data != NULL) { free (data->data); }
            if (data       != NULL) { free (data); }
        }
        free (authdata);
    }
    if (ticketData       != NULL) { free (ticketData); }
    if (secondTicketData != NULL) { free (secondTicketData); }
    if (keyblockContents != NULL) { free (keyblockContents); }    
    if (clientString     != NULL) { krb5_free_unparsed_name (inContext, clientString); }    
    if (serverString     != NULL) { krb5_free_unparsed_name (inContext, serverString); }    

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLStoreKerberos4CredentialsInCCache (CREDENTIALS *inV4Creds, const cc_ccache_t inCCache)
{
    KLStatus             err = klNoErr;
    cc_credentials_union creds;
    cc_credentials_v4_t  ccV4Creds;

    if (inV4Creds == NULL) { err = KLError_ (klParameterErr); }
    if (inCCache  == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
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
    
    if (err == klNoErr) {
        err = cc_ccache_store_credentials (inCCache, &creds);
    }

    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetKerberos5TgtForCCache (const cc_ccache_t inCCache, krb5_context inContext, krb5_creds *outCreds)
{
    KLStatus             err = klNoErr;
    cc_credentials_t     creds = NULL;
    cc_credentials_v5_t *v5Creds = NULL;
    krb5_int32           offset_seconds = 0;
    krb5_principal       client = NULL;
    krb5_principal       server = NULL;
    krb5_octet          *keyblock_contents = NULL;
    char                *ticket_data = NULL;
    char                *second_ticket_data = NULL;
    krb5_address       **addresses = NULL;
    krb5_authdata      **authdata = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    if (outCreds == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        krb5_int32 offset_microseconds = 0;
        err = krb5_get_time_offsets (inContext, &offset_seconds, &offset_microseconds);
    }

    if (err == klNoErr) {
        err = __KLGetValidTgtForCCache (inCCache, kerberosVersion_V5, &creds);
    }

    if (err == klNoErr) {
        v5Creds = creds->data->credentials.credentials_v5;
    }
    
    if (err == klNoErr) {
        err = krb5_parse_name (inContext, v5Creds->client, &client);
    }

    if (err == klNoErr) {
        err = krb5_parse_name (inContext, v5Creds->server, &server);
    }
    
    if (err == klNoErr) {
        keyblock_contents = (krb5_octet *) malloc (sizeof(krb5_octet) * v5Creds->keyblock.length);
        if (keyblock_contents == NULL) {
            err = KLError_ (klMemFullErr);
        } else {
            memmove (keyblock_contents, v5Creds->keyblock.data, v5Creds->keyblock.length);
        }
    }

    if (err == klNoErr) {
        ticket_data = (char *) malloc (v5Creds->ticket.length);
        if (ticket_data == NULL) {
            err = KLError_ (klMemFullErr);
        } else {
            memmove (ticket_data, v5Creds->ticket.data, v5Creds->ticket.length);
        }
    }

    if (err == klNoErr) {
        second_ticket_data = (char *) malloc (v5Creds->second_ticket.length);
        if (second_ticket_data == NULL) {
            err = KLError_ (klMemFullErr);
        } else {
            memmove (second_ticket_data, v5Creds->second_ticket.data, v5Creds->second_ticket.length);
        }
    }

    if (err == klNoErr) {
        if (v5Creds->addresses != NULL) {
            // Addresses: (use calloc so pointers get initialized to NULL)
            krb5_address **addrPtr, *addr;
	    cc_data      **dataPtr, *data;
	    unsigned int numRecords = 0;
            
            for (dataPtr = v5Creds->addresses; *dataPtr != NULL; numRecords++, dataPtr++) {}
            
            addresses = (krb5_address **) calloc (numRecords + 1, sizeof(krb5_address *));
            if (addresses == NULL) { err = KLError_ (klMemFullErr); }
            
            if (err == klNoErr) {
                for (dataPtr = v5Creds->addresses, addrPtr = addresses; *dataPtr != NULL; addrPtr++, dataPtr++) {
                    *addrPtr = (krb5_address *) calloc (1, sizeof(krb5_address));
                    if (*addrPtr != NULL) { err = KLError_ (klMemFullErr); break; }
                    
                    data = *dataPtr;
                    addr = *addrPtr;
            
                    addr->addrtype = data->type;
                    addr->magic    = KV5M_ADDRESS;
                    addr->length   = data->length;
                    addr->contents = (krb5_octet *) calloc (addr->length, sizeof(krb5_octet));
                    if (addr->contents == NULL) { err = KLError_ (klMemFullErr); break; }
                    
                    memmove(addr->contents, data->data, addr->length); /* copy contents */
                }
            }
        }
    }

    if (err == klNoErr) {
        if (v5Creds->authdata != NULL) {
            // Authdata: (use calloc so pointers get initialized to NULL)
            krb5_authdata **authPtr, *auth;
	    cc_data       **dataPtr, *data;
	    unsigned int  numRecords = 0;
            
            for (dataPtr = v5Creds->authdata; *dataPtr != NULL; numRecords++, dataPtr++) {}
            
            authdata = (krb5_authdata **) calloc (numRecords + 1, sizeof(krb5_authdata *));
            if (authdata == NULL) { err = KLError_ (klMemFullErr); }
            
            if (err == klNoErr) {
                for (dataPtr = v5Creds->authdata, authPtr = authdata; *dataPtr != NULL; authPtr++, dataPtr++) {
                    *authPtr = (krb5_authdata *) calloc (1, sizeof (krb5_authdata));
                    if (*authPtr != NULL) { err = KLError_ (klMemFullErr); break; }
                    
                    data = *dataPtr;
                    auth = *authPtr;
            
                    auth->ad_type = data->type;
                    auth->magic    = KV5M_ADDRESS;
                    auth->length   = data->length;
                    auth->contents = (krb5_octet *) calloc (auth->length, sizeof(krb5_octet));
                    if (auth->contents == NULL) { err = KLError_ (klMemFullErr); break; }
                    
                    memmove(auth->contents, data->data, auth->length); /* copy contents */
                }
            }
        }
    }

    if (err == klNoErr) {
        outCreds->client = client;
        outCreds->server = server;
        
        outCreds->keyblock.enctype = v5Creds->keyblock.type;
        outCreds->keyblock.length = v5Creds->keyblock.length;
        outCreds->keyblock.contents = keyblock_contents;
        
        outCreds->times.authtime   = v5Creds->authtime     + offset_seconds;
        outCreds->times.starttime  = v5Creds->starttime    + offset_seconds;
        outCreds->times.endtime    = v5Creds->endtime      + offset_seconds;
        outCreds->times.renew_till = v5Creds->renew_till   + offset_seconds;
        outCreds->is_skey          = v5Creds->is_skey;
        outCreds->ticket_flags     = v5Creds->ticket_flags;
        
        outCreds->addresses = addresses;

        outCreds->ticket.length = v5Creds->ticket.length;
        outCreds->ticket.data = ticket_data;
        
        outCreds->second_ticket.length = v5Creds->second_ticket.length;
        outCreds->second_ticket.data = second_ticket_data;
        
        outCreds->authdata = authdata;
    } else {
        // Free unused memory so we don't leak
        if (client             != NULL) { krb5_free_principal (inContext, client); }
        if (server             != NULL) { krb5_free_principal (inContext, server); }
        if (keyblock_contents  != NULL) { free (keyblock_contents); }
        if (ticket_data        != NULL) { free (ticket_data); }
        if (second_ticket_data != NULL) { free (second_ticket_data); }
        if (addresses != NULL) {
            krb5_address **addrPtr, *addr;
            for (addrPtr = addresses; *addrPtr != NULL; addrPtr++) {
                addr = *addrPtr;
                
                if (addr->contents != NULL) { free (addr->contents); }
                if (addr           != NULL) { free (addr); }
            }
            free (addresses);
        }
        if (authdata != NULL) {
            krb5_authdata **authPtr, *auth;
            for (authPtr = authdata; *authPtr != NULL; authPtr++) {
                auth = *authPtr;
                
                if (auth->contents != NULL) { free (auth->contents); }
                if (auth           != NULL) { free (auth); }
            }
            free (authdata);
        }
    }

    if (creds   != NULL) { cc_credentials_release (creds); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetKerberos4TgtForCCache (const cc_ccache_t inCCache, CREDENTIALS *outCreds)
{
    KLStatus             err = klNoErr;
    cc_credentials_t     creds = NULL;
    cc_credentials_v4_t *v4Creds = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    if (outCreds == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLGetValidTgtForCCache (inCCache, kerberosVersion_V4, &creds);
    }

    if (err == klNoErr) {
        v4Creds = creds->data->credentials.credentials_v4;
    }
    
    if (err == klNoErr) {
        strcpy (outCreds->pname,           v4Creds->principal);
        strcpy (outCreds->pinst,           v4Creds->principal_instance);
        strcpy (outCreds->service,         v4Creds->service);
        strcpy (outCreds->instance,        v4Creds->service_instance);
        strcpy (outCreds->realm,           v4Creds->realm);
        memmove (outCreds->session,        v4Creds->session_key, sizeof (des_cblock));
        outCreds->kvno =                   v4Creds->kvno;
        outCreds->stk_type =               v4Creds->string_to_key_type;
        outCreds->issue_date =             v4Creds->issue_date;
        outCreds->address =                v4Creds->address;
        // Fix the lifetime to something krb4 likes:
        outCreds->lifetime = krb_time_to_life (v4Creds->issue_date, v4Creds->lifetime + v4Creds->issue_date);
        outCreds->ticket_st.length =       v4Creds->ticket_size;
        memmove (outCreds->ticket_st.dat,  v4Creds->ticket, v4Creds->ticket_size);
    }
    
    if (creds != NULL) { cc_credentials_release (creds); }

    return KLError_ (err);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetSystemDefaultCCache (cc_ccache_t *outCCache)
{
    KLStatus err = klNoErr;
    cc_context_t cc_context = NULL;
    
    if (outCCache  == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (err == klNoErr) {
        err = cc_context_open_default_ccache (cc_context, outCCache);
        if (err == ccErrCCacheNotFound || err == ccErrBadName) {
            err = KLError_ (klSystemDefaultDoesNotExistErr);
        }
    }
    
    if (cc_context != NULL) { cc_context_release (cc_context); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetFirstCCacheForPrincipal (const KLPrincipal inPrincipal, cc_ccache_t *outCCache)
{
    KLStatus             err = klNoErr;
    KLBoolean            found = false;
    char                *principalV4String = NULL;
    char                *principalV5String = NULL;
    cc_context_t         cc_context = NULL;
    cc_ccache_iterator_t iterator = NULL;
    
    if (inPrincipal == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (err == klNoErr) {
        err = cc_context_new_ccache_iterator (cc_context, &iterator);
    }

    if (err == klNoErr) {
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

    while (!found && (err == klNoErr)) {
        cc_ccache_t ccache = NULL;
        cc_string_t cachePrincipal = NULL;
        cc_int32 version;
        
        if (err == klNoErr) {
            err = cc_ccache_iterator_next (iterator, &ccache);
        }
        
        if (err == klNoErr) {
            err = cc_ccache_get_credentials_version (ccache, &version);
        }

        if (err == klNoErr) {
            version = (version == cc_credentials_v4_v5) ? cc_credentials_v5 : version;
        }
        
        if (err == klNoErr) {
            err = cc_ccache_get_principal (ccache, version, &cachePrincipal);
        }
        
        if (err == klNoErr) {
            char *principalString = (version == cc_credentials_v5) ? principalV5String : principalV4String;
            if ((principalString != NULL) && (strcmp (cachePrincipal->data, principalString) == 0)) {
                *outCCache = ccache;
                ccache = NULL;  // So we don't free ccache
                found = true;   // force it to break out of the loop gracefully
            }
        }
        
        if (ccache         != NULL) { cc_ccache_release (ccache); }
        if (cachePrincipal != NULL) { cc_string_release (cachePrincipal); }
    }
    
    if ((err == ccIteratorEnd) || (err == ccErrCCacheNotFound) || (err == ccErrBadName)) { 
        err = KLError_ (klPrincipalDoesNotExistErr); 
    }
    
    if (principalV5String != NULL) { KLDisposeString (principalV5String); }
    if (principalV4String != NULL) { KLDisposeString (principalV4String); }
    if (iterator          != NULL) { cc_ccache_iterator_release (iterator); }
    if (cc_context        != NULL) { cc_context_release (cc_context); }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheByName (const char *inCacheName, cc_ccache_t *outCCache)
{
    KLStatus err = klNoErr;
    cc_context_t cc_context = NULL;
    
    if (inCacheName == NULL) { err = KLError_ (klParameterErr); }
    if (outCCache   == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = cc_initialize (&cc_context, ccapi_version_4, NULL, NULL);
    }
    
    if (err == klNoErr) {
        err = cc_context_open_ccache (cc_context, inCacheName, outCCache);
        if (err == ccErrCCacheNotFound || err == ccErrBadName) {
            err = KLError_ (klCacheDoesNotExistErr);
        }
    }
    
    if (cc_context != NULL) { cc_context_release (cc_context); }
    
    return KLError_ (err);
}


#pragma mark -

// ---------------------------------------------------------------------------

KLBoolean __KLCCacheHasKerberos4 (const cc_ccache_t inCCache) 
{
    KLStatus err = klNoErr;
    KLBoolean hasKerberos4 = false;
    cc_string_t principalString = NULL;
    KLPrincipal principal = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = cc_ccache_get_principal (inCCache, cc_credentials_v4, &principalString);
    }

    if (err == klNoErr) {
        err = KLCreatePrincipalFromString (principalString->data, kerberosVersion_V4, &principal);
    }
    
    if (err == klNoErr) {
        hasKerberos4 = __KLPrincipalHasKerberos4Profile (principal);
    }
    
    if (principal       != NULL) { KLDisposePrincipal (principal); }
    if (principalString != NULL) { cc_string_release (principalString); }
    
    return hasKerberos4;
}

// ---------------------------------------------------------------------------

KLBoolean __KLCCacheHasKerberos5 (const cc_ccache_t inCCache) 
{
    KLStatus err = klNoErr;
    KLBoolean hasKerberos5 = false;
    cc_string_t principalString = NULL;
    KLPrincipal principal = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = cc_ccache_get_principal (inCCache, cc_credentials_v5, &principalString);
    }

    if (err == klNoErr) {
        err = KLCreatePrincipalFromString (principalString->data, kerberosVersion_V5, &principal);
    }
    
    if (err == klNoErr) {
        hasKerberos5 = __KLPrincipalHasKerberos5Profile (principal);
    }
    
    if (principal       != NULL) { KLDisposePrincipal (principal); }
    if (principalString != NULL) { cc_string_release (principalString); }
    
    return hasKerberos5;
}


#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetValidTgtForCCache (const cc_ccache_t inCCache, KLKerberosVersion inVersion, cc_credentials_t *outCreds)
{
    KLStatus err = klNoErr;
    KLBoolean found = false;
    cc_credentials_iterator_t iterator = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    if ((inVersion != kerberosVersion_V4) && (inVersion != kerberosVersion_V5)) { err = KLError_ (klInvalidVersionErr); }
    // outCreds maybe NULL if the caller only wants to get success/error
    
    if (err == klNoErr) {
        err = cc_ccache_new_credentials_iterator (inCCache, &iterator);
    }
    
    while (!found && (err == klNoErr)) {
        cc_credentials_t creds = NULL;
        KLPrincipal servicePrincipal = NULL;
        
        if (err == klNoErr) {
            err = cc_credentials_iterator_next (iterator, &creds);
        }
        
        if (err == klNoErr) {
            if (((inVersion == kerberosVersion_V4) && (creds->data->version == cc_credentials_v4)) ||
                ((inVersion == kerberosVersion_V5) && (creds->data->version == cc_credentials_v5))) {
                
                if (creds->data->version == cc_credentials_v4) {
                    err = __KLCreatePrincipalFromTriplet (creds->data->credentials.credentials_v4->service, 
                                                          creds->data->credentials.credentials_v4->service_instance,
                                                          creds->data->credentials.credentials_v4->realm,
                                                          kerberosVersion_V4,
                                                          &servicePrincipal);
                } else if (creds->data->version == cc_credentials_v5) {
                    err = KLCreatePrincipalFromString (creds->data->credentials.credentials_v5->server,
                                                       kerberosVersion_V5, &servicePrincipal);
                } else {
                    err = KLError_ (klInvalidVersionErr);
                }
                if (err == klNoErr) {
                    // Credentials match version... is it a valid tgs principal?
                    if (__KLPrincipalIsTicketGrantingService (servicePrincipal, inVersion)) {
                        err = __KLCredsAreValid (creds);
                        if (err == klNoErr) {
                            found = true;      // Break out of the loop
                            if (outCreds != NULL) {
                                *outCreds = creds; // Return the creds to the caller
                                creds = NULL;      // Make sure they don't get released on us
                            }
                        }
                    }
                }
            }
        }
        
        if (servicePrincipal != NULL) { KLDisposePrincipal (servicePrincipal); }
        if (creds            != NULL) { cc_credentials_release (creds); }
    }

    if ((err == ccIteratorEnd) || (err == ccErrCCacheNotFound)) { 
        err = KLError_ (klNoCredentialsErr); 
    }
    
    if (iterator != NULL) { cc_credentials_iterator_release (iterator); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLCacheHasValidTickets (const cc_ccache_t inCCache, KLKerberosVersion inVersion)
{
    KLStatus         err = klNoErr;
    KLStatus         v5Err = klNoErr;
    KLStatus         v4Err = klNoErr;
    KLPrincipal      principal = NULL;
	
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }

    // Get the cache principal, if the cache exists:
    if (err == klNoErr) {
        err = __KLGetPrincipalForCCache (inCCache, &principal);
    }

    if (err == klNoErr) {
        // What credentials are in the ccache?
        v5Err = __KLGetValidTgtForCCache (inCCache, kerberosVersion_V5, NULL);
        v4Err = __KLGetValidTgtForCCache (inCCache, kerberosVersion_V4, NULL);

        // What credentials does the caller want?
        switch (inVersion) {
            case kerberosVersion_V5:
                err = KLError_ (v5Err);
                break;
                
            case kerberosVersion_V4:
                err = KLError_ (v4Err);
                break;
                
            case (kerberosVersion_V5 | kerberosVersion_V4):
                if (v5Err != klNoErr) {
                    err = KLError_ (v5Err); // favor v5 errors
                } else if (v4Err != klNoErr) {
                    err = KLError_ (v4Err); // then v4 errors
                } else {
                    err = KLError_ (klNoErr); // no error
                }
                break;
                
            case kerberosVersion_All:
                if (__KLPrincipalHasKerberos5Profile (principal) && (v5Err != klNoErr)) {
                    err = KLError_ (v5Err);
                } else if (__KLPrincipalHasKerberos4Profile (principal) && (v4Err != klNoErr)) {
                    err = KLError_ (v4Err);
                } else if (__KLPrincipalHasKerberos5Profile (principal) || !__KLPrincipalHasKerberos4Profile (principal)) {
                    err = KLError_ (v5Err); // has v5 profile or neither
                } else {
                    err = KLError_ (v4Err);
                }
                break;
                
            case kerberosVersion_Any:
                if ((v4Err != klNoErr) && (v5Err != klNoErr)) {
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

KLStatus __KLGetPrincipalForCCache (const cc_ccache_t inCCache, KLPrincipal *outPrincipal)
{
    KLStatus err = klNoErr;
    cc_int32 version = cc_credentials_v5;
    cc_string_t principalString = NULL;
    KLKerberosVersion klVersion = kerberosVersion_V5;
    KLPrincipal principal = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = cc_ccache_get_credentials_version (inCCache, &version);
    }
    
    if (err == klNoErr) {
        version   = (version == cc_credentials_v4_v5) ? cc_credentials_v5  : version;
        klVersion = (version == cc_credentials_v4)    ? kerberosVersion_V4 : kerberosVersion_V5;
    }
    
    if (err == klNoErr) {
        err = cc_ccache_get_principal (inCCache, version, &principalString);
    }

    if (err == klNoErr) {
        err = KLCreatePrincipalFromString (principalString->data, klVersion, &principal);
    }

    if (err == klNoErr) {
        if (outPrincipal != NULL) {
            *outPrincipal = principal;
            principal = NULL;
        }
    }

    if (principal       != NULL) { KLDisposePrincipal (principal); }
    if (principalString != NULL) { cc_string_release (principalString); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetNameForCCache (const cc_ccache_t inCCache, char **outName)
{
    KLStatus err = klNoErr;
    char *name = NULL;
    cc_string_t ccName = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = cc_ccache_get_name (inCCache, &ccName);
    }
    
    if (err == klNoErr) {
        err = __KLCreateString (ccName->data, &name);
    }

    if (err == klNoErr) {
        if (outName != NULL) {
            *outName = name;
            name = NULL;
        }
    }

    if (name   != NULL) { KLDisposeString (name); }
    if (ccName != NULL) { cc_string_release (ccName); }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus __KLGetPrincipalAndNameForCCache (const cc_ccache_t inCCache, KLPrincipal *outPrincipal, char **outName)
{
    KLStatus err = klNoErr;
    KLPrincipal principal = NULL;
    char *name = NULL;
    
    if (inCCache == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLGetPrincipalForCCache (inCCache, &principal);
    }
    
    if (err == klNoErr) {
        err = __KLGetNameForCCache (inCCache, &name);
    }

    if (err == klNoErr) {
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

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheExpirationTime (const cc_ccache_t inCCache, KLKerberosVersion inVersion, KLTime *outExpirationTime)
{
    KLStatus err = klNoErr;
    KLTime   v5ExpirationTime = (inVersion == kerberosVersion_Any) ? 0 : 0xFFFFFFFF;
    KLTime   v4ExpirationTime = (inVersion == kerberosVersion_Any) ? 0 : 0xFFFFFFFF;
	
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (inCCache, inVersion);
    }
    
    if (err == klNoErr) {
        cc_credentials_t creds = NULL;
        
        if (__KLGetValidTgtForCCache (inCCache, kerberosVersion_V5, &creds) == klNoErr) {
            err = __KLGetCredsExpirationTime (creds, &v5ExpirationTime);
        }
        
        if (creds != NULL) { cc_credentials_release (creds); }
    }
    
    if (err == klNoErr) {
        cc_credentials_t creds = NULL;
        
        if (__KLGetValidTgtForCCache (inCCache, kerberosVersion_V4, &creds) == klNoErr) {
            err = __KLGetCredsExpirationTime (creds, &v4ExpirationTime);
        }
        
        if (creds != NULL) { cc_credentials_release (creds); }
    }
    
    if (err == klNoErr) {
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
    
#if MACDEV_DEBUG
    // This is usually, but not always a bug
    if (err == klNoErr && (*outExpirationTime == 0 || *outExpirationTime == 0xFFFFFFFF)) {
        dprintf ("__KLGetCCacheExpirationTime returning suspicious expiration time %ld", *outExpirationTime);
    }
#endif

    return KLError_ (err);
}
	
// ---------------------------------------------------------------------------

KLStatus __KLGetCredsExpirationTime (const cc_credentials_t inCreds, KLTime *outExpirationTime)
{
    KLStatus err = klNoErr;
    
    if (inCreds           == NULL) { err = KLError_ (klParameterErr); }
    if (outExpirationTime == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        switch (inCreds->data->version) {
            case cc_credentials_v5:
                *outExpirationTime = inCreds->data->credentials.credentials_v5->endtime;
                break;	
				
            case cc_credentials_v4:
                *outExpirationTime = inCreds->data->credentials.credentials_v4->issue_date + 
                                     inCreds->data->credentials.credentials_v4->lifetime;
                break;
			
            default:
                err = KLError_ (klInvalidVersionErr);  // What kind of weird credentials are these? 
                break;
        }
	}
		
	return KLError_ (klNoErr);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLGetCCacheStartTime (const cc_ccache_t inCCache, KLKerberosVersion inVersion, KLTime *outStartTime)
{
	KLStatus err = klNoErr;
	KLTime   v5StartTime = (inVersion == kerberosVersion_Any) ? 0xFFFFFFFF : 0;
	KLTime   v4StartTime = (inVersion == kerberosVersion_Any) ? 0xFFFFFFFF : 0;
	
    if (inCCache     == NULL) { err = KLError_ (klParameterErr); }
    if (outStartTime == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        err = __KLCacheHasValidTickets (inCCache, inVersion);
    }
    
    if (err == klNoErr) {
        cc_credentials_t creds = NULL;
        
        if (__KLGetValidTgtForCCache (inCCache, kerberosVersion_V5, &creds) == klNoErr) {
            err = __KLGetCredsStartTime (creds, &v5StartTime);
        }
        
        if (creds != NULL) { cc_credentials_release (creds); }
    }
    
    if (err == klNoErr) {
        cc_credentials_t creds = NULL;
        
        if (__KLGetValidTgtForCCache (inCCache, kerberosVersion_V4, &creds) == klNoErr) {
            err = __KLGetCredsStartTime (creds, &v4StartTime);
        }
        
        if (creds != NULL) { cc_credentials_release (creds); }
    }
    
    if (err == klNoErr) {
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
    
#if MACDEV_DEBUG
	// This is usually, but not always a bug
	if (err == klNoErr && (*outStartTime == 0 || *outStartTime == 0xFFFFFFFF)) {
        dprintf ("__KLGetCCacheStartTime returning suspicious start time %ld", *outStartTime);
    }
#endif

    return KLError_ (err);
}
	
// ---------------------------------------------------------------------------

KLStatus __KLGetCredsStartTime (const cc_credentials_t inCreds, KLTime *outStartTime)
{
	KLStatus err = klNoErr;
    
    if (inCreds      == NULL) { err = KLError_ (klParameterErr); }
    if (outStartTime == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        switch (inCreds->data->version) {
            case cc_credentials_v5:
                *outStartTime = inCreds->data->credentials.credentials_v5->starttime;
                break;	
				
            case cc_credentials_v4:
                *outStartTime = inCreds->data->credentials.credentials_v4->issue_date;
                break;
			
            default:
                err = KLError_ (klInvalidVersionErr);  // What kind of weird credentials are these? 
                break;
        }
	}
		
	return KLError_ (klNoErr);
}

#pragma mark -

// ---------------------------------------------------------------------------

KLStatus __KLCredsAreValid (const cc_credentials_t inCreds)
{
    KLStatus err = klNoErr;
	
    if (inCreds == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        switch (inCreds->data->version) {
            case cc_credentials_v5:
                err =  __KLV5CredsAreValid (inCreds);
                break;
                
            case cc_credentials_v4:
                err = __KLV4CredsAreValid (inCreds);
                break;
            
            default:
                err = KLError_ (klInvalidVersionErr);
                break;
        }
    }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLV5CredsAreValid (const cc_credentials_t inCreds)
{
    KLStatus     err = klNoErr;
    krb5_int32   now = 0;
    krb5_context context = NULL;
    
    if (inCreds == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        err = __KLGetUnusedKerberos5Context (&context);
    }

    if (err == klNoErr) {
        krb5_int32 usec;
        
        err = krb5_us_timeofday (context, &now, &usec);
    }
    
    if (err == klNoErr) {
        if (inCreds->data->credentials.credentials_v5->endtime <= (cc_uint32) now) {
            err = KLError_ (klCredentialsExpiredErr); 
        }
    }
    
    if (err == klNoErr) {
        if ((inCreds->data->credentials.credentials_v5->ticket_flags & TKT_FLG_POSTDATED) && 
            (inCreds->data->credentials.credentials_v5->ticket_flags & TKT_FLG_INVALID)) {
            err = KLError_ (klCredentialsNeedValidationErr);
        }
    }

    if (err == klNoErr) {
        cc_data **cbase = inCreds->data->credentials.credentials_v5->addresses;
        
        if (cbase != NULL) {  // No addresses is always valid!
            for (; *cbase != NULL; *cbase++) {
                cc_data *ccAdr = *cbase;
                krb5_address address;
                
                // Eeeeeeeew!  Build a fake krb5_address.
                address.magic = KV5M_ADDRESS;
                address.addrtype = (int) ccAdr->type;
                address.length = (int) ccAdr->length;
                address.contents = (unsigned char *) ccAdr->data;
                
                err = __KLAddressIsValid (&address);
                if (err == klNoErr) { break; }  // If there's one valid address, it's valid
            }
        }
    }
    
    return KLError_ (err);
}

// ---------------------------------------------------------------------------

static KLStatus __KLV4CredsAreValid (const cc_credentials_t inCreds)
{
    KLStatus err = klNoErr;
    KLTime   now = 0;
    
    if (inCreds == NULL) { err = KLError_ (klParameterErr); }

    if (err == klNoErr) {
        struct timeval timeStruct;

        err = gettimeofday (&timeStruct, NULL);
        if (err == klNoErr) { 
            now = timeStruct.tv_sec;
        } else {
            err = KLError_ (errno); 
        }
    }
    
    if (err == klNoErr) {
        if ((inCreds->data->credentials.credentials_v4->issue_date + 
             inCreds->data->credentials.credentials_v4->lifetime) <= now) {
            err = KLError_ (klCredentialsExpiredErr); 
        }
    }
    
    if (err == klNoErr) {
        if (inCreds->data->credentials.credentials_v4->address != 0L) {
            krb5_address 	address;
            
            // Eeeeeeeew!  Build a fake krb5_address.
            address.magic = KV5M_ADDRESS;
            address.addrtype = AF_INET;
            address.length = INADDRSZ;
            address.contents = (unsigned char *)&inCreds->data->credentials.credentials_v4->address;
            
            err = __KLAddressIsValid (&address);
        }
    }
    
    return KLError_ (err);
}

#pragma mark -
// ---------------------------------------------------------------------------

krb5_address** gAddresses = NULL;
KLTime         gAddressChangeTime = 0;


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

static KLStatus __KLAddressIsValid (const krb5_address* address)
{
    KLStatus err = klNoErr;
    
    if (address == NULL) { err = KLError_ (klParameterErr); }
    
    if (err == klNoErr) {
        // update the address state
        __KLCheckAddresses ();
    }

    // TCP/IP has been unloaded so there are no addresses in the list,
    // assume tickets are still valid in case we return
    if ((gAddresses != NULL) && (gAddresses[0] != NULL)) {
        krb5_context context = NULL;

        if (err == klNoErr) {
            err = __KLGetUnusedKerberos5Context (&context);
        }

        if (err == klNoErr) {
            if (krb5_address_search (context, address, gAddresses) != true) {
                err = KLError_ (klCredentialsBadAddressErr);
            }
        }
    }

    return KLError_ (err);
}


KLTime __KLCheckAddresses (void)
{
    // Get the list of addresses from krb5
    KLStatus       err = klNoErr;
    krb5_context   context = NULL;
    krb5_address **addresses = NULL;
    KLIndex        i;
    KLBoolean      addressesChanged = false;

    if (err == klNoErr) {
        err = __KLGetUnusedKerberos5Context (&context);
    }

    if (err == klNoErr) {
        err = krb5_os_localaddr (context, &addresses);
    }
    
    if (err == klNoErr) {
        if (gAddresses == NULL) {
            // There is no address list now, so it changed:
            addressesChanged = true;
        } else {
            // Compare them: We aren't doing this in a terribly efficient way,
            // but most folks will only have a couple addresses anyway

            // Are all the addresses in addresses also in sAddresses?
            for (i = 0; addresses[i] != NULL && !addressesChanged; i++) {
                if (krb5_address_search (context, addresses[i], gAddresses) == false) {
                    // Address missing!
                    addressesChanged = true;
                    break;
                }
            }

            // Are all the addresses in sAddresses also in addresses?
            for (i = 0; gAddresses[i] != NULL && !addressesChanged; i++) {
                if (krb5_address_search (context, gAddresses[i], addresses) == false) {
                    // Address missing!
                    addressesChanged = true;
                    break;
                }
            }

            // We always destroy the old address list so both cases grab the new one
            krb5_free_addresses (context, gAddresses);
        }

        // save the new address list
        gAddresses = addresses;
    } else {
        dprintf ("UKLCCache::CheckAddresses(): krb5_os_localaddr failed (err = %d)\n", err);
    }

    if (addressesChanged) {
        gAddressChangeTime = time (NULL);
        dprintf ("__KLCheckAddresses(): ADDRESS CHANGED, returning new time %d\n", gAddressChangeTime);
    }

    return gAddressChangeTime;	
}

KLStatus __KLFreeAddressList (void)
{
    KLStatus err = klNoErr;
    
    if (gAddresses != NULL) {
        krb5_context context = NULL;
        
        err = __KLGetUnusedKerberos5Context (&context);
        if (err == klNoErr) {
            krb5_free_addresses (context, gAddresses);
            gAddresses = NULL;
        }
    }
    
    return KLError_ (err);
}