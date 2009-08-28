/*
 * KerberosPrincipal.m
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

#import "KerberosPrincipal.h"
#import "KerberosErrorAlert.h"

@implementation KerberosPrincipal

// ---------------------------------------------------------------------------

- (id) initWithString: (NSString *) string
{
    if ((self = [super init])) {
        if (kim_identity_create_from_string (&identity, [string UTF8String])) {
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
{
    if ((self = [super init])) {
        if (kim_identity_create_from_components (&identity, 
                                                 [realm UTF8String], 
                                                 [name UTF8String], 
                                                 [instance UTF8String],
                                                 NULL)) {
            [self release];
            self = NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (id) initWithClientPrincipalFromCredentials: (cc_credentials_t) credentials
{
    if (credentials->data->version == cc_credentials_v5) {
        NSString *string = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v5->client];

        return [self initWithString: string];
        
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (id) initWithServicePrincipalFromCredentials: (cc_credentials_t) credentials
{
    if (credentials->data->version == cc_credentials_v5) {
        NSString *string = [NSString stringWithUTF8String: credentials->data->credentials.credentials_v5->server];

        return [self initWithString: string];
        
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    kim_identity_free (&identity);
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (NSString *) displayString
{
    NSString *string = @"";
    kim_string identity_string = NULL;
    kim_error err = KIM_NO_ERROR;
    
    err = kim_identity_get_display_string (identity, &identity_string);
    if (!err) {
        string = [NSString stringWithUTF8String: identity_string];
    } else {
        dprintf ("kim_identity_get_display_string returned %d '%s'", 
                 err, error_message (err));
    }
    
    kim_string_free (&identity_string);
    
    return string;    
}

// ---------------------------------------------------------------------------

- (NSString *) string
{
    NSString *string = @"";
    kim_string identity_string = NULL;
    kim_error err = KIM_NO_ERROR;
    
    err = kim_identity_get_string (identity, &identity_string);
    if (!err) {
        string = [NSString stringWithUTF8String: identity_string];
    } else {
        dprintf ("kim_identity_get_string returned %d '%s'", 
                 err, error_message (err));
    }
    
    kim_string_free (&identity_string);
    
    return string;    
}

// ---------------------------------------------------------------------------

- (NSArray *)componentsArray
{
    NSMutableArray *componentArray = nil;
    kim_error err = KIM_NO_ERROR;
    kim_count count = 0;
    
    componentArray = [NSMutableArray arrayWithCapacity: count];
    if (!componentArray) { err = ENOMEM; }
    
    if (!err) {
        err = kim_identity_get_number_of_components (identity, &count);
    }
    
    if (!err) {
        kim_count i;
        
        for (i = 0; !err && i < count; i++) {
            kim_string component = NULL;
            
            err = kim_identity_get_component_at_index (identity, i, &component);
            
            if (!err) {
                [componentArray addObject: [NSString stringWithUTF8String: component]];
            }
            
            kim_string_free (&component);
        }
    }
    
    return componentArray;
}

// ---------------------------------------------------------------------------

- (NSString *) realmString
{
    kim_error err = KIM_NO_ERROR;
    NSString *realm = nil;
    kim_string realm_string = NULL;
    
    err = kim_identity_get_realm (identity, &realm_string);
    
    if (!err) {
        realm = [NSString stringWithUTF8String: realm_string];
    }
    
    kim_string_free (&realm_string);

    return realm;
}

// ---------------------------------------------------------------------------

- (int) setKeychainPassword: (NSString *) password
{
    return __KLPrincipalSetKeychainPassword (identity, [password UTF8String]);
}

// ---------------------------------------------------------------------------

- (int) removeKeychainPassword
{
    return __KLRemoveKeychainPasswordForPrincipal (identity);
}

// ---------------------------------------------------------------------------
// May return NULL

- (NSString *) keychainPassword
{
    KLStatus err = klNoErr;
    char *password = NULL;
    NSString *passwordString = NULL;
    
    err = __KLGetKeychainPasswordForPrincipal (identity, &password);

    if (!err && password) {
        passwordString = [NSString stringWithUTF8String: password];
    }
    
    if (password) { KLDisposeString (password); }
    
    return passwordString;
}

// ---------------------------------------------------------------------------

- (BOOL) isTicketGrantingService
{
    return __KLPrincipalIsTicketGrantingService (identity);
}

// ---------------------------------------------------------------------------

- (int) changePassword
{
    return kim_identity_change_password (identity);
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
    KLStatus err = KLChangePasswordWithPasswords (identity, [oldPassword UTF8String], [newPassword UTF8String],
                                                  &outRejected, &outError, &outDescription);
    if (!err) {
        *rejected = (BOOL) outRejected;
        
        if (rejectionError) {
            NSString *string = outError ? [NSString stringWithUTF8String: outError] : @"";
            [rejectionError setString: string];
        }
        
        if (rejectionDescription) {
            NSString *string = outDescription ? [NSString stringWithUTF8String: outDescription] : @"";
            [rejectionDescription setString: string];
        }
    }
    
    if (outError      ) { KLDisposeString (outError); }
    if (outDescription) { KLDisposeString (outDescription); }
    
    return err;
}


// ---------------------------------------------------------------------------

- (int) setDefault
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    
    err = kim_ccache_create_from_client_identity (&ccache, identity);
    
    if (!err) {
        err = kim_ccache_set_default (ccache);
    }
    
    kim_ccache_free (&ccache);
    
    return err;
}

// ---------------------------------------------------------------------------

- (int) destroyTickets
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    
    err = kim_ccache_create_from_client_identity (&ccache, identity);
    
    if (!err) {
        err = kim_ccache_destroy (&ccache);
    }
    
    return err;
}

// ---------------------------------------------------------------------------

- (int) renewTickets
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    
    err = kim_ccache_create_from_client_identity (&ccache, identity);
    
    if (!err) {
        err = kim_ccache_renew (ccache, KIM_OPTIONS_DEFAULT);
        if (err) {
            kim_ccache new_ccache = NULL;
            
            err = kim_ccache_create_new (&new_ccache, 
                                         identity, KIM_OPTIONS_DEFAULT);
            
            kim_ccache_free (&new_ccache);
        }
    }
    
    kim_ccache_free (&ccache);
    
    return err;
}

// ---------------------------------------------------------------------------

- (int) renewTicketsIfPossibleInBackground
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    
    err = kim_ccache_create_from_client_identity (&ccache, identity);
    
    if (!err) {
        err = kim_ccache_renew (ccache, KIM_OPTIONS_DEFAULT);
    }
    
    kim_ccache_free (&ccache);
    
    return err;
}

// ---------------------------------------------------------------------------

- (int) validateTickets
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    
    err = kim_ccache_create_from_client_identity (&ccache, identity);
    
    if (!err) {
        err = kim_ccache_validate (ccache, KIM_OPTIONS_DEFAULT);
    }
    
    kim_ccache_free (&ccache);
    
    return err;
}

// ---------------------------------------------------------------------------

- (int) getTickets
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    
    err =  kim_ccache_create_new (&ccache, identity, KIM_OPTIONS_DEFAULT);
    
    kim_ccache_free (&ccache);
    
    return err;
}

// ---------------------------------------------------------------------------

- (int) getTicketsWithPassword: (NSString *) password 
                  loginOptions: (KLLoginOptions) loginOptions 
                     cacheName: (NSMutableString *) ioCacheName
{
    char *cacheName = NULL;
    KLStatus err = KLAcquireNewInitialTicketsWithPassword (identity, loginOptions, [password UTF8String], &cacheName);
    if (!err) {
        if (ioCacheName) { [ioCacheName setString: [NSString stringWithCString: cacheName]]; }
    }
    if (cacheName) { KLDisposeString (cacheName); }
    return err;
}

// ---------------------------------------------------------------------------

+ (int) getTickets
{
    kim_error err = KIM_NO_ERROR;
    kim_ccache ccache = NULL;
    
    err =  kim_ccache_create_new (&ccache, NULL, KIM_OPTIONS_DEFAULT);
    
    kim_ccache_free (&ccache);
    
    return err;
}

@end
