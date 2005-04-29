/*
 * Credentials.m
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Credentials.m,v 1.5 2004/09/20 20:32:29 lxs Exp $
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

#import "Credentials.h"
#import "Utilities.h"

@implementation Credentials

// ---------------------------------------------------------------------------

- (id) initWithCCache: (cc_ccache_t) ccache version: (cc_int32) credsVersion
{
    if ((self = [super init])) {
        dprintf ("Entering initWithCCache:version: ...");
        principal = NULL;
        credentialsArray = NULL;
        tgtIndex = -1;
        version = kerberosVersion_Any;
        
        if ([self synchronizeWithCCache: ccache version: credsVersion] != ccNoError) {
            [self release];
            self = NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (principal        != NULL) { [principal release]; }
    if (credentialsArray != NULL) { [credentialsArray release]; }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (int) synchronizeWithCCache: (cc_ccache_t) ccache version: (cc_int32) credsVersion
{
    dprintf ("Entering synchronizeWithCCache:version: ...");
        
    cc_int32                   err = ccNoError;
    cc_string_t                credsPrincipal = NULL;
    Principal                 *newPrincipal = NULL;
    cc_credentials_iterator_t  iterator = NULL;
    int                        newTGTIndex = -1;
    NSMutableArray            *newCredentialsArray = NULL;
    
    if (err == ccNoError) {
        err = cc_ccache_get_principal (ccache, credsVersion, &credsPrincipal);
    }
    
    if (err == ccNoError) {
        NSString *credsPrincipalString = [NSString stringWithUTF8String: credsPrincipal->data];
        KLKerberosVersion klCredsVersion = (credsVersion == cc_credentials_v4) ? kerberosVersion_V4 : kerberosVersion_V5;
        
        newPrincipal = [[Principal alloc] initWithString: credsPrincipalString klVersion: klCredsVersion];
        if (newPrincipal == NULL) { err = ccErrNoMem; }
    }
    
    if (err == ccNoError) {
        newCredentialsArray = [[NSMutableArray alloc] init];
        if (newCredentialsArray == NULL) { err = ccErrNoMem; }
    }
    
    if (err == ccNoError) {
        err = cc_ccache_new_credentials_iterator (ccache, &iterator);
    }
    
    while (err == ccNoError) {
        cc_credentials_t creds = NULL;
        Credential *credential = NULL;
        
        if (err == ccNoError) {
            err = cc_credentials_iterator_next (iterator, &creds);
        }
        
        if (err == ccNoError) {
            if (creds->data->version == credsVersion) {
                credential = [self findCredentialForCredentials: creds];
                if (credential != NULL) { 
                    err = [credential synchronizeWithCredentials: creds];
                } else {
                    credential = [[[Credential alloc] initWithCredentials: creds] autorelease];
                    if (credential == NULL) { err = ccErrNoMem; }
                }
            
                if (err == ccNoError)  {
                    if (newTGTIndex < 0 && [credential isTGT]) {
                        newTGTIndex = [newCredentialsArray count];  // Remember first TGT of this version
                        dprintf ("TGT index for version %s is %d", 
                               [[Utilities stringForCCVersion: credsVersion] UTF8String], newTGTIndex);
                    }
                    
                    [newCredentialsArray addObject: credential];
                    credential = NULL;  // don't free
                    creds = NULL;       // credential takes ownership of this
                }
            }
        }
        
        if (credential != NULL) { [credential release]; }
        if (creds != NULL) { cc_credentials_release (creds); }
    }
    
    if (err == ccIteratorEnd) {
        err = ccNoError;
    }
    
    if (err == ccNoError) {
        if (credentialsArray != NULL) { [credentialsArray release]; }
        credentialsArray = newCredentialsArray;
        
        if (principal != NULL) { [principal release]; }
        principal = newPrincipal;
        newPrincipal = NULL;

        tgtIndex = newTGTIndex;
        version = credsVersion;
    } else {
        dprintf ("synchronizeWithCCache:version: returning error %d (%s)", err, error_message (err));
    }
    
    if (iterator       != NULL) { cc_credentials_iterator_release (iterator); }
    if (newPrincipal   != NULL) { [newPrincipal release]; }
    if (credsPrincipal != NULL) { cc_string_release (credsPrincipal); }

    return err;
}

// ---------------------------------------------------------------------------

- (Credential *) findCredentialForCredentials: (cc_credentials_t) creds
{
    unsigned int i = 0;
    
    for (i = 0; i < [credentialsArray count]; i++) {
        Credential *credential = [credentialsArray objectAtIndex: i];
        if ([credential isEqualToCredentials: creds]) {
            return credential;
        }
    }
    
    return NULL;
}

// ---------------------------------------------------------------------------

- (BOOL) needsValidation
{
    BOOL needsValidation = NO;
    
    if (tgtIndex >= 0) {
        return [[credentialsArray objectAtIndex: tgtIndex] needsValidation];
    } else {
        if ([credentialsArray count] > 0) {
            unsigned int i; // return union of bad states on service ticket if there is no tgt
            for (i = 0; i < [credentialsArray count]; i++) {
                if ([[credentialsArray objectAtIndex: i]  needsValidation]) {
                    needsValidation = YES;
                    break;
                }
            }
        }
    }
    return needsValidation;
}

// ---------------------------------------------------------------------------

- (int) stateAtTime: (time_t) atTime
{
    int state = CredentialValid;
    
    if (tgtIndex >= 0) {
        // prefer tgt time
        state = [[credentialsArray objectAtIndex: tgtIndex] stateAtTime: atTime];
    } else {
        if ([credentialsArray count] > 0) {
            unsigned int i; // return union of bad states on service ticket if there is no tgt
            for (i = 0; i < [credentialsArray count]; i++) {
                state |= [[credentialsArray objectAtIndex: i] timeRemainingAtTime: atTime];
            }
        } else {
            state = CredentialInvalid;  // no tickets at all
        }
    }
    return state;
}

// ---------------------------------------------------------------------------

- (cc_time_t) timeRemainingAtTime: (time_t) atTime
{
    cc_time_t timeRemaining = 0;  // default if there are no tickets at all
    
    if (tgtIndex >= 0) {
        // prefer tgt time
        timeRemaining = [[credentialsArray objectAtIndex: tgtIndex] timeRemainingAtTime: atTime];
    } else {
        if ([credentialsArray count] > 0) {
            // return smallest time of service tickets if there is no tgt
            timeRemaining = [[credentialsArray objectAtIndex: 0] timeRemainingAtTime: atTime];
            unsigned int i;
            for (i = 1; i < [credentialsArray count]; i++) {
                cc_time_t temp = [[credentialsArray objectAtIndex: 0] timeRemainingAtTime: atTime];
                if (temp < timeRemaining) {
                    timeRemaining = temp;
                }
            }
        }
    }
    return timeRemaining;
}

// ---------------------------------------------------------------------------

- (Principal *) principal
{
    return principal;    
}

// ---------------------------------------------------------------------------

- (NSString *) principalString
{
    return [principal displayStringForCCVersion: version];
}

// ---------------------------------------------------------------------------

- (NSAttributedString *) stringValueForTicketColumn
{
    NSString *string = [NSString stringWithFormat: @"(%@) %@", [Utilities stringForCCVersion: version], [self principalString]];
    NSDictionary *attributes = [Utilities attributesForTicketColumnCellOfControlSize: NSSmallControlSize
                                                                                bold: YES 
                                                                              italic: ([self stateAtTime: time (NULL)] != CredentialValid)];
    
    return [[[NSAttributedString alloc] initWithString: string attributes: attributes] autorelease];
}

// ---------------------------------------------------------------------------

- (NSAttributedString *) stringValueForLifetimeColumn
{
    time_t now = time (NULL);
    int state = [self stateAtTime: now];
    cc_time_t timeRemaining = [self timeRemainingAtTime: now];
    
    NSString *string = [Utilities stringForTimeRemaining: timeRemaining 
                                                   state: state 
                                                  format: kShortFormat];
    NSDictionary *attributes = [Utilities attributesForLifetimeColumnCellOfControlSize: NSSmallControlSize
                                                                                  bold: YES
                                                                                 state: state
                                                                         timeRemaining: timeRemaining];
    
    return [[NSAttributedString alloc] initWithString: string attributes: attributes];
}

// ---------------------------------------------------------------------------

- (int) numberOfChildren
{
    return [credentialsArray count];
}

// ---------------------------------------------------------------------------

- (id) childAtIndex: (int) rowIndex
{
    return [credentialsArray objectAtIndex: rowIndex];
}


@end
