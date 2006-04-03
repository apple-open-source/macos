/*
 * KLLoginOptions.c
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

// The real login options structure
typedef struct OpaqueKLLoginOptions {
    krb5_get_init_creds_opt options;
    KLTime                  startTime;
    char                   *serviceName;
    krb5_context            context;
    krb5_address          **addresses;
} LoginOptions;

#pragma mark -

/* KLLoginOptions functions */

// ---------------------------------------------------------------------------

KLStatus KLCreateLoginOptions (KLLoginOptions *outOptions)
{
    KLStatus err = klNoErr;
    KLLoginOptions loginOptions = NULL;
    KLSize size;
    KLLifetime ticketLifetime;
    KLLifetime renewLifetime;
    KLBoolean forwardable, proxiable, addressless, renewable;
    
    // Pull defaults out of KLL preferences
    if (err == klNoErr) {
        size = sizeof (ticketLifetime);
        err = KLGetDefaultLoginOption (loginOption_DefaultTicketLifetime, &ticketLifetime, &size);
    }

    if (err == klNoErr) {
        size = sizeof (renewLifetime);
        err = KLGetDefaultLoginOption (loginOption_DefaultRenewableLifetime, &renewLifetime, &size);
    }

    if (err == klNoErr) {
        size = sizeof (renewable);
        err = KLGetDefaultLoginOption (loginOption_DefaultRenewableTicket, &renewable, &size);
    }

    if (err == klNoErr) {
        size = sizeof (forwardable);
        err = KLGetDefaultLoginOption (loginOption_DefaultForwardableTicket, &forwardable, &size);
    }

    if (err == klNoErr) {
        size = sizeof (proxiable);
        err = KLGetDefaultLoginOption (loginOption_DefaultProxiableTicket, &proxiable, &size);
    }

    if (err == klNoErr) {
        size = sizeof (addressless);
        err = KLGetDefaultLoginOption (loginOption_DefaultAddresslessTicket, &addressless, &size);
    }

    // Create the login options
    if (err == klNoErr) {
        loginOptions = (KLLoginOptions) malloc (sizeof (LoginOptions));
        if (loginOptions == NULL) { err = KLError_ (klMemFullErr); }
    }
    
    // Fill in the login options with defaults
    if (err == klNoErr) {
    	krb5_get_init_creds_opt_init (&loginOptions->options); 
        
        loginOptions->startTime = 0;
        loginOptions->serviceName = NULL;
        loginOptions->context = NULL;
        loginOptions->addresses = NULL;
    }

    if (err == klNoErr) {
        // Now that we have a valid login options, set up with KLL defaults
        KLLoginOptionsSetTicketLifetime (loginOptions, ticketLifetime);
        KLLoginOptionsSetForwardable (loginOptions, forwardable);
        KLLoginOptionsSetProxiable (loginOptions, proxiable);
        KLLoginOptionsSetAddressless (loginOptions, addressless);
        if (renewable) {
            KLLoginOptionsSetRenewableLifetime (loginOptions, renewLifetime);
        }
    }

    if (err == klNoErr) {
        *outOptions = loginOptions;
    } else {
        if (loginOptions != NULL) { free (loginOptions); }
    }
    
    return KLError_ (err);
}
     
// ---------------------------------------------------------------------------

KLStatus KLLoginOptionsSetTicketLifetime (KLLoginOptions  ioOptions,
                                          KLLifetime      inTicketLifetime)
{
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }
    
    krb5_get_init_creds_opt_set_tkt_life (&ioOptions->options, inTicketLifetime);
    
    return klNoErr;
}
     
// ---------------------------------------------------------------------------

KLStatus KLLoginOptionsSetForwardable (KLLoginOptions  ioOptions,
                                       KLBoolean       inForwardable)
{
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }

    krb5_get_init_creds_opt_set_forwardable (&ioOptions->options, inForwardable);

    return klNoErr;
}

// ---------------------------------------------------------------------------

KLStatus KLLoginOptionsSetProxiable (KLLoginOptions  ioOptions,
                                     KLBoolean       inProxiable)
{
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }
    
    krb5_get_init_creds_opt_set_proxiable (&ioOptions->options, inProxiable);

    return klNoErr;
}

// ---------------------------------------------------------------------------

KLStatus KLLoginOptionsSetRenewableLifetime (KLLoginOptions ioOptions,
                                             KLLifetime     inRenewableLifetime)
{
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }

    krb5_get_init_creds_opt_set_renew_life (&ioOptions->options, inRenewableLifetime);

    return klNoErr;
}

// ---------------------------------------------------------------------------

KLStatus KLLoginOptionsSetAddressless (KLLoginOptions ioOptions,
                                       KLBoolean      inAddressless)
{
    KLStatus err = klNoErr;
    
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }
    
    // Free up any old address lists and contexts associated with them
    // Note that we should never have a non-nul address list and a nul context
    if (ioOptions->context != NULL) {
        if (ioOptions->addresses != NULL) {
            krb5_free_addresses (ioOptions->context, ioOptions->addresses);
        }
    	krb5_free_context (ioOptions->context);
    }
    
    // Reset the context and address list
    ioOptions->context = NULL;
    ioOptions->addresses = NULL;
    
    // If we are supposed to fill in with local addresses, create them and the context
    if (!inAddressless) {
        krb5_context context = NULL;
        krb5_address **addresses = NULL;
        
        if (err == klNoErr) {
            err = krb5_init_context (&context);
        }
        
        if (err == klNoErr) {
            err = krb5_os_localaddr (context, &addresses);
        }

        if (err == klNoErr) {
            ioOptions->context = context;
            ioOptions->addresses = addresses;
        } else {
            if (addresses != NULL) { krb5_free_addresses (context, addresses); }
            if (context   != NULL) { krb5_free_context (context); }
        }
    }
    
    if (err == klNoErr) {
        krb5_get_init_creds_opt_set_address_list (&ioOptions->options, ioOptions->addresses);
    }

    return KLError_ (err);
}

// ---------------------------------------------------------------------------

KLStatus KLLoginOptionsSetTicketStartTime (KLLoginOptions  ioOptions,
                                           KLTime          inStartTime)
{
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }

    ioOptions->startTime = inStartTime;

    return KLError_ (klNoErr);
}

// ---------------------------------------------------------------------------

KLStatus KLLoginOptionsSetServiceName (KLLoginOptions  ioOptions,
                                       const char     *inServiceName)
{
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }
    
    // Free up any old memory
    if (ioOptions->serviceName != NULL) { 
        free (ioOptions->serviceName); 
        ioOptions->serviceName = NULL;
    }
    
    if (inServiceName != NULL) {
        // Allocate memory for the service name
        ioOptions->serviceName = (char *) malloc (sizeof(char) * (strlen (inServiceName) + 1));
        if (ioOptions->serviceName == NULL) { return KLError_ (klMemFullErr); }

        // Copy the service name
        strcpy (ioOptions->serviceName, inServiceName);
    }

    return KLError_ (klNoErr);
}

// ---------------------------------------------------------------------------

KLStatus KLDisposeLoginOptions (KLLoginOptions ioOptions)
{
    if (ioOptions == NULL) { return KLError_ (klBadLoginOptionsErr); }
    
    // Free the service name
    if (ioOptions->serviceName != NULL) { free (ioOptions->serviceName); }
     
    // Free the context and address list
    if (ioOptions->context != NULL) {
        if (ioOptions->addresses != NULL) {
            krb5_free_addresses (ioOptions->context, ioOptions->addresses);
        }
    	krb5_free_context (ioOptions->context);
    }
    
    free (ioOptions);
    
    return KLError_ (klNoErr);
}

#pragma mark -

// ---------------------------------------------------------------------------

krb5_get_init_creds_opt *__KLLoginOptionsGetKerberos5Options (KLLoginOptions ioOptions)
{
    return &ioOptions->options;
}

// ---------------------------------------------------------------------------

KLTime __KLLoginOptionsGetStartTime (KLLoginOptions ioOptions)
{
    return ioOptions->startTime;
}

// ---------------------------------------------------------------------------

char * __KLLoginOptionsGetServiceName (KLLoginOptions ioOptions)
{
    return ioOptions->serviceName;
}


