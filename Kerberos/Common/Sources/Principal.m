/*
 * Principal.m
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
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

#import "Principal.h"
#import "ErrorAlert.h"

// ---------------------------------------------------------------------------

static KLKerberosVersion KLVersion (cc_int32 version)
{
    switch (version) {
        case cc_credentials_v5:
            return kerberosVersion_V5;
        case cc_credentials_v4:
            return kerberosVersion_V4;
        default:
            return kerberosVersion_Any;
    }
}

@implementation Principal

// ---------------------------------------------------------------------------

- (id) initWithString: (NSString *) string 
            klVersion: (KLKerberosVersion) version
{
    if ((self = [super init])) {
        if (KLCreatePrincipalFromString ([string UTF8String], version, &principal) != klNoErr) {
            [self release];
            self = NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (id) initWithName: (NSString *) name 
           instance: (NSString *) instance 
              realm: (NSString *) realm 
          klVersion: (KLKerberosVersion) version
{
    if ((self = [super init])) {
        if (__KLCreatePrincipalFromTriplet ([name UTF8String], [instance UTF8String], [realm UTF8String], 
                                            version, &principal) != klNoErr) {
            [self release];
            self = NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (id) initWithClientPrincipalFromCredentials: (cc_credentials_t) credentials
{
    if (credentials->data->version == cc_credentials_v4) {
        NSString *name     = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v4->principal];
        NSString *instance = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v4->principal_instance];
        NSString *realm    = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v4->realm];
        
        return [self initWithName: name instance: instance realm: realm klVersion: kerberosVersion_V4];
        
    } else if (credentials->data->version == cc_credentials_v5) {
        NSString *string = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v5->client];

        return [self initWithString: string klVersion: kerberosVersion_V5];
        
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (id) initWithServicePrincipalFromCredentials: (cc_credentials_t) credentials
{
    if (credentials->data->version == cc_credentials_v4) {
        NSString *name     = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v4->service];
        NSString *instance = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v4->service_instance];
        NSString *realm    = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v4->realm];
        
        return [self initWithName: name instance: instance realm: realm klVersion: kerberosVersion_V4];
        
    } else if (credentials->data->version == cc_credentials_v5) {
        NSString *string = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v5->server];

        return [self initWithString: string klVersion: kerberosVersion_V5];
        
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (principal != NULL) { KLDisposePrincipal (principal); }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (NSString *) displayString
{
    return [self displayStringForKLVersion: kerberosVersion_V5];
}

// ---------------------------------------------------------------------------

- (NSString *) displayStringForCCVersion: (cc_int32) version
{
    return [self displayStringForKLVersion: KLVersion (version)];
}

// ---------------------------------------------------------------------------

- (NSString *) displayStringForKLVersion: (KLKerberosVersion) version
{
    NSString *string = @"";
    char *klString = NULL;
    KLStatus err = KLGetDisplayStringFromPrincipal (principal, KLVersion (version), &klString);
    
    if (err == klNoErr) {
        string = [NSString stringWithUTF8String: klString];
    } else {
        dprintf ("KLGetDisplayStringFromPrincipal returned %d '%s'", err, error_message (err));
    }
    
    if (klString != NULL) { KLDisposeString (klString); }
    
    return string;    
}

// ---------------------------------------------------------------------------

- (NSString *) string
{
    return [self stringForKLVersion: kerberosVersion_V5];
}

// ---------------------------------------------------------------------------

- (NSString *) stringForCCVersion: (cc_int32) version
{
    return [self stringForKLVersion: KLVersion (version)];
}

// ---------------------------------------------------------------------------

- (NSString *) stringForKLVersion: (KLKerberosVersion) version
{
    NSString *string = @"";
    char *klString = NULL;
    
    KLStatus err = KLGetStringFromPrincipal (principal, version, &klString);
    
    if (err == klNoErr) {
        string = [NSString stringWithUTF8String: klString];
    } else {
        dprintf ("KLGetStringFromPrincipal returned %d '%s'", err, error_message (err));
    }
    
    if (klString != NULL) { KLDisposeString (klString); }
    
    return string;    
}

// ---------------------------------------------------------------------------

- (BOOL) isTicketGrantingServiceForCCVersion: (cc_int32) version
{
    return __KLPrincipalIsTicketGrantingService (principal, KLVersion (version));
}

// ---------------------------------------------------------------------------

- (BOOL) isTicketGrantingServiceForKLVersion: (KLKerberosVersion) version
{
    return __KLPrincipalIsTicketGrantingService (principal, version);
}

// ---------------------------------------------------------------------------

- (int) changePassword
{
    return KLChangePassword (principal);
}

// ---------------------------------------------------------------------------

- (int) changePasswordWithOldPassword: (NSString *) oldPassword
                          newPassword: (NSString *) newPassword
                             rejected: (BOOL *) rejected
                       rejectionError: (NSMutableString *) rejectionError
                 rejectionDescription: (NSMutableString *) rejectionDescription
{
    KLBoolean outRejected;
    char *outError = NULL;
    char *outDescription = NULL;
    KLStatus err = KLChangePasswordWithPasswords (principal, [oldPassword UTF8String], [newPassword UTF8String],
                                                  &outRejected, &outError, &outDescription);
    if (err == klNoErr) {
        *rejected = (BOOL) outRejected;
        
        if (rejectionError != NULL) {
            NSString *string = (outError != NULL) ? [NSString stringWithUTF8String: outError] : @"";
            [rejectionError setString: string];
        }
        
        if (rejectionDescription != NULL) {
            NSString *string = (outDescription != NULL) ? [NSString stringWithUTF8String: outDescription] : @"";
            [rejectionDescription setString: string];
        }
    }
    
    if (outError       != NULL) { KLDisposeString (outError); }
    if (outDescription != NULL) { KLDisposeString (outDescription); }
    
    return err;
}


// ---------------------------------------------------------------------------

- (int) setDefault
{
    return KLSetSystemDefaultCache (principal);
}

// ---------------------------------------------------------------------------

- (int) destroyTickets
{
    return KLDestroyTickets (principal);
}

// ---------------------------------------------------------------------------

- (int) renewTickets
{
    KLStatus err = KLRenewInitialTickets (principal, NULL, NULL, NULL);
    if (err != klNoErr && err != klUserCanceledErr) {
        err = KLAcquireNewInitialTickets (principal, NULL, NULL, NULL);
    }
    return err;
}

// ---------------------------------------------------------------------------

- (int) renewTicketsIfPossibleInBackground
{
    // This is used by callers who want to renew silently in the background
    return KLRenewInitialTickets (principal, NULL, NULL, NULL);
}

// ---------------------------------------------------------------------------

- (int) validateTickets
{
    // This is used by callers who want to renew silently in the background
    return KLValidateInitialTickets (principal, NULL, NULL);
}

// ---------------------------------------------------------------------------

- (int) getTickets
{
    return KLAcquireNewInitialTickets (principal, NULL, NULL, NULL);
}

// ---------------------------------------------------------------------------

- (int) getTicketsWithPassword: (NSString *) password 
                  loginOptions: (KLLoginOptions) loginOptions 
                     cacheName: (NSMutableString *) ioCacheName
{
    char *cacheName = NULL;
    KLStatus err = KLAcquireNewInitialTicketsWithPassword (principal, loginOptions, [password UTF8String], &cacheName);
    if (err == klNoErr) {
        if (ioCacheName != NULL) { [ioCacheName setString: [NSString stringWithCString: cacheName]]; }
    }
    if (cacheName != NULL) { KLDisposeString (cacheName); }
    return err;
}

// ---------------------------------------------------------------------------

+ (int) getTickets
{
    return KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);
}

@end
