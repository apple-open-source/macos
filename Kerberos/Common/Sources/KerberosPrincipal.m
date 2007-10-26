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
        if (KLCreatePrincipalFromString ([string UTF8String], kerberosVersion_V5, &principal) != klNoErr) {
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
        if (__KLCreatePrincipalFromTriplet ([name UTF8String], [instance UTF8String], [realm UTF8String], 
                                            kerberosVersion_V5, &principal) != klNoErr) {
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
    if (principal) { KLDisposePrincipal (principal); }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (NSString *) displayString
{
    NSString *string = @"";
    char *klString = NULL;
    KLStatus err = KLGetDisplayStringFromPrincipal (principal, kerberosVersion_V5, &klString);
    
    if (!err) {
        string = [NSString stringWithUTF8String: klString];
    } else {
        dprintf ("KLGetDisplayStringFromPrincipal returned %d '%s'", err, error_message (err));
    }
    
    if (klString) { KLDisposeString (klString); }
    
    return string;    
}

// ---------------------------------------------------------------------------

- (NSString *) string
{
    NSString *string = @"";
    char *klString = NULL;
    
    KLStatus err = KLGetStringFromPrincipal (principal, kerberosVersion_V5, &klString);
    
    if (!err) {
        string = [NSString stringWithUTF8String: klString];
    } else {
        dprintf ("KLGetStringFromPrincipal returned %d '%s'", err, error_message (err));
    }
    
    if (klString) { KLDisposeString (klString); }
    
    return string;    
}

// ---------------------------------------------------------------------------

- (NSArray *)componentsArray
{
	NSMutableArray *resultArray = nil;
	NSString *resultString = nil;
	KLStatus err;
	krb5_context k5_context = NULL;
	krb5_principal k5_principal = NULL;	
	int i, size;
	
	err = krb5_init_context(&k5_context);
	
	if (!err) {
		err = __KLGetKerberos5PrincipalFromPrincipal(principal, k5_context, &k5_principal);
	}
	if (!err) {
		size = krb5_princ_size(k5_context, k5_principal);
		resultArray = [NSMutableArray arrayWithCapacity:size];
		
		for (i = 0; i < size; i++) {
			resultString = [[NSString alloc] initWithBytes:krb5_princ_component(k5_context, k5_principal, i)->data length:krb5_princ_component(k5_context, k5_principal, i)->length encoding:NSUTF8StringEncoding];
			[resultArray addObject:resultString];
			[resultString release];
		}
	}
	if (k5_principal)	{ krb5_free_principal(k5_context, k5_principal); }
	if (k5_context)		{ krb5_free_context(k5_context); }
	
	return resultArray;
}

// ---------------------------------------------------------------------------

- (NSString *)realmString
{
	NSString *resultString = nil;
	KLStatus err;
	krb5_context k5_context = NULL;
	krb5_principal k5_principal = NULL;	
	
	err = krb5_init_context(&k5_context);
	
	if (!err) {
		err = __KLGetKerberos5PrincipalFromPrincipal(principal, k5_context, &k5_principal);
	}	
	if (!err) {
		resultString = [[NSString alloc] initWithBytes:krb5_princ_realm(k5_context, k5_principal)->data length:krb5_princ_realm(k5_context, k5_principal)->length encoding:NSUTF8StringEncoding];
	}
	if (k5_principal)	{ krb5_free_principal(k5_context, k5_principal); }
	if (k5_context)		{ krb5_free_context(k5_context); }
	
	return resultString;
}

// ---------------------------------------------------------------------------

- (BOOL) isTicketGrantingService
{
    return __KLPrincipalIsTicketGrantingService (principal);
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
    if (!err) {
        *rejected = (BOOL) outRejected;
        
        if (rejectionError) {
            NSString *string = (outError != NULL) ? [NSString stringWithUTF8String: outError] : @"";
            [rejectionError setString: string];
        }
        
        if (rejectionDescription) {
            NSString *string = (outDescription != NULL) ? [NSString stringWithUTF8String: outDescription] : @"";
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
    if (!err) {
        if (ioCacheName) { [ioCacheName setString: [NSString stringWithCString: cacheName]]; }
    }
    if (cacheName) { KLDisposeString (cacheName); }
    return err;
}

// ---------------------------------------------------------------------------

+ (int) getTickets
{
    return KLAcquireNewInitialTickets (NULL, NULL, NULL, NULL);
}

@end
