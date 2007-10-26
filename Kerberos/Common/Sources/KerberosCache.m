/*
 * KerberosCache.m
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

#import "KerberosCache.h"
#import "KerberosCredential.h"

@implementation KerberosCache

// ---------------------------------------------------------------------------

- (id) initWithCCache: (cc_ccache_t) newCCache defaultCCache: (cc_ccache_t) defaultCCache
{
    if ((self = [super init])) {
        dprintf ("Entering initWithCCache:defaultCCache: ...");
        ccache = NULL;

        principal = NULL;
        credentialsArray = NULL;
        tgtIndex = -1;
        
        lastChangeTime = 0;
        lastDefaultTime = 0;
        isDefault = NO;
        
        if ([self synchronizeWithCCache: newCCache defaultCCache: defaultCCache] != ccNoError) {
            [self release];
            self = NULL;
        }
    }
    
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (ccache          ) { cc_ccache_release (ccache); }    
    if (principal       ) { [principal release]; }
    if (credentialsArray) { [credentialsArray release]; }
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (int) synchronizeWithCCache: (cc_ccache_t) newCCache defaultCCache: (cc_ccache_t) defaultCCache
{
    cc_int32                   err = ccNoError;
    cc_time_t                  newLastChangeTime = 0;
    cc_time_t                  newLastDefaultTime = 0;
    cc_string_t                credsPrincipal = NULL;
    KerberosPrincipal         *newPrincipal = NULL;
    cc_credentials_iterator_t  iterator = NULL;
    int                        newTGTIndex = -1;
    NSMutableArray            *newCredentialsArray = NULL;
    
    dprintf ("Entering synchronizeWithCCache:defaultCCache: ...");
    
    if (!err) {
        err = cc_ccache_get_change_time (newCCache, &newLastChangeTime);
    }
    
    if (!err) {
        // When this changes, the ccache change time does not necessarily change
        err = cc_ccache_get_last_default_time (newCCache, &newLastDefaultTime);
        if (err == ccErrNeverDefault) {
            err = ccNoError;
            newLastDefaultTime = 0;
        }
    }
    
    if (!err && newLastChangeTime > lastChangeTime) {
        dprintf ("synchronizeWithCCache cache got new change time %d", newLastChangeTime);

        if (!err) {
            err = cc_ccache_get_principal (newCCache, cc_credentials_v5, &credsPrincipal);
        }
        
        if (!err) {
            newPrincipal = [[KerberosPrincipal alloc] initWithString: [NSString stringWithUTF8String: credsPrincipal->data]];
            if (!newPrincipal) { err = ccErrNoMem; }
        }
                
        if (!err) {
            newCredentialsArray = [[NSMutableArray alloc] init];
            if (!newCredentialsArray) { err = ccErrNoMem; }
        }
        
        if (!err) {
            err = cc_ccache_new_credentials_iterator (newCCache, &iterator);
        }
        
        while (!err) {
            cc_credentials_t creds = NULL;
            KerberosCredential *credential = NULL;
            
            if (!err) {
                err = cc_credentials_iterator_next (iterator, &creds);
            }
            
            if (!err) {
                if (creds->data->version == cc_credentials_v5) {
                    credential = [self findCredentialForCredentials: creds];
                    if (credential) { 
                        err = [credential synchronizeWithCredentials: creds];
                    } else {
                        credential = [[[KerberosCredential alloc] initWithCredentials: creds] autorelease];
                        if (!credential) { err = ccErrNoMem; }
                    }
                    
                    if (!err)  {
                        if (newTGTIndex < 0 && [credential isTGT]) {
                            newTGTIndex = [newCredentialsArray count];  // Remember first TGT of this version
                            dprintf ("TGT index is %d", newTGTIndex);
                        }
                        
                        [newCredentialsArray addObject: credential];
                        credential = NULL;  // don't free
                        creds = NULL;       // credential takes ownership of this
                    }
                }
            }
            
            if (credential) { [credential release]; }
            if (creds     ) { cc_credentials_release (creds); }
        }
        
        if (err == ccIteratorEnd) {
            err = ccNoError;
        }
        
        if (!err) {
            if (ccache) { cc_ccache_release (ccache); }
            ccache = newCCache;
            
            if (credentialsArray) { [credentialsArray release]; }
            credentialsArray = newCredentialsArray;
            newCredentialsArray = NULL;
            
            if (!principal) { [principal release]; }
            principal = newPrincipal;
            newPrincipal = NULL;
            
            tgtIndex = newTGTIndex;
            lastChangeTime = newLastChangeTime;
        }
    }
    
    if (!err) {
        // For some reason cache default changes don't always update the cache change time.
        // Do this after everything else is set up
        isDefault = [self isEqualToCCache: defaultCCache];
        lastDefaultTime = newLastDefaultTime;
    } else {
        dprintf ("synchronizeWithCCache:defaultCCache: returning error %d (%s)", err, error_message (err));
    }
    
    if (iterator           ) { cc_credentials_iterator_release (iterator); }
    if (newCredentialsArray) { [newCredentialsArray release]; }
    if (newPrincipal       ) { [newPrincipal release]; }
    if (credsPrincipal     ) { cc_string_release (credsPrincipal); }

    return err;
}

// ---------------------------------------------------------------------------

- (KerberosCredential *) findCredentialForCredentials: (cc_credentials_t) creds
{
    unsigned int i = 0;
    
    for (i = 0; i < [credentialsArray count]; i++) {
        KerberosCredential *credential = [credentialsArray objectAtIndex: i];
        if ([credential isEqualToCredentials: creds]) {
            return credential;
        }
    }
    
    return NULL;
}

// ---------------------------------------------------------------------------

- (BOOL) isEqualToCCache: (cc_ccache_t) compareCCache
{
    cc_uint32 equal = NO;
    cc_int32  err = ccNoError;
 
    if (!ccache       ) { err = ccErrInvalidCCache; }
    if (!compareCCache) { err = ccErrInvalidCCache; }
    
    if (!err) {
        err = cc_ccache_compare (ccache, compareCCache, &equal);
    }
    
    if (!err) {
        return equal;
    } else {
        // errors such as ccErrInvalidCCache mean they are not equal!
        return NO;
    }
}

// ---------------------------------------------------------------------------

- (cc_int32) lastDefaultTime
{
    return lastDefaultTime;
}

// ---------------------------------------------------------------------------

- (BOOL) isDefault
{
    return isDefault;
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

- (int) state
{
    int state = CredentialValid;
    
    if (tgtIndex >= 0) {
        // prefer tgt time
        state = [[credentialsArray objectAtIndex: tgtIndex] state];
    } else {
        if ([credentialsArray count] > 0) {
            unsigned int i; // return union of bad states on service ticket if there is no tgt
            for (i = 0; i < [credentialsArray count]; i++) {
                state |= [[credentialsArray objectAtIndex: i] state];
            }
        } else {
            state = CredentialInvalid;  // no tickets at all
        }
    }
    return state;
}

// ---------------------------------------------------------------------------
    
- (cc_time_t) timeRemaining
{
    cc_time_t timeRemaining = 0;  // default if there are no tickets at all
    
    if (tgtIndex >= 0) {
        // prefer tgt time
        timeRemaining = [[credentialsArray objectAtIndex: tgtIndex] timeRemaining];
    } else {
        if ([credentialsArray count] > 0) {
            // return smallest time of service tickets if there is no tgt
            timeRemaining = [[credentialsArray objectAtIndex: 0] timeRemaining];
            unsigned int i;
            for (i = 1; i < [credentialsArray count]; i++) {
                cc_time_t temp = [[credentialsArray objectAtIndex: i] timeRemaining];
                if (temp < timeRemaining) {
                    timeRemaining = temp;
                }
            }
        }
    }
    
    return timeRemaining;
}

// ---------------------------------------------------------------------------

- (NSString *) shortTimeRemainingString
{
    return [KerberosCredential stringForTimeRemaining: [self timeRemaining]
                                                state: [self state]
                                          shortFormat: YES];
}

// ---------------------------------------------------------------------------

- (NSString *) longTimeRemainingString
{
    return [KerberosCredential stringForTimeRemaining: [self timeRemaining]
                                                state: [self state]
                                          shortFormat: NO];
}

// ---------------------------------------------------------------------------

- (NSString *) ccacheName
{
    NSString *string = NULL;
    cc_string_t name = NULL;
    
    if (cc_ccache_get_name (ccache, &name) == ccNoError) {
        string = [NSString stringWithCString: name->data];
    }
 
    if (name) { cc_string_release (name); }
    
    return string;
}

// ---------------------------------------------------------------------------

- (KerberosPrincipal *) principal
{
    return principal;  
}

// ---------------------------------------------------------------------------

- (NSString *) principalString
{
    return [principal displayString];  
}

// ---------------------------------------------------------------------------

- (int) numberOfCredentials
{
    return [credentialsArray count];
}

// ---------------------------------------------------------------------------

- (id) credentialAtIndex: (int) rowIndex
{
    return [credentialsArray objectAtIndex: rowIndex];
}

@end
