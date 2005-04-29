/*
 * Cache.m
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Cache.m,v 1.7 2005/01/31 20:50:34 lxs Exp $
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

#import "Cache.h"
#import "Credentials.h"
#import "Credential.h"
#import "Utilities.h"

@implementation Cache

// ---------------------------------------------------------------------------

- (id) initWithCCache: (cc_ccache_t) newCCache defaultCCache: (cc_ccache_t) defaultCCache
{
    if ((self = [super init])) {
        dprintf ("Entering initWithCCache:defaultCCache: ...");
        ccache = NULL;

        v5Credentials = NULL;
        v4Credentials = NULL;

        version = 0;
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
    if (ccache        != NULL) { cc_ccache_release (ccache); }
    if (v5Credentials != NULL) { [v5Credentials release]; }
    if (v4Credentials != NULL) { [v4Credentials release]; }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (int) synchronizeWithCCache: (cc_ccache_t) newCCache defaultCCache: (cc_ccache_t) defaultCCache
{
    cc_int32    err = ccNoError;
    cc_time_t   newLastChangeTime = 0;
    cc_time_t   newLastDefaultTime = 0;
    
    dprintf ("Entering synchronizeWithCCache:defaultCCache: ...");
    
    if (err == ccNoError) {
        err = cc_ccache_get_change_time (newCCache, &newLastChangeTime);
    }
    
    if (err == ccNoError) {
        // When this changes, the ccache change time does not necessarily change
        err = cc_ccache_get_last_default_time (newCCache, &newLastDefaultTime);
        if (err == ccErrNeverDefault) {
            err = ccNoError;
            newLastDefaultTime = 0;
        }
    }
    
    if (err == ccNoError) {
        if (newLastChangeTime > lastChangeTime) {
            dprintf ("synchronizeWithCCache cache got new change time %ld", newLastChangeTime);
            cc_uint32    newVersion = 0;
            Credentials *newV4Credentials = NULL;
            Credentials *newV5Credentials = NULL;
            
            if (err == ccNoError) {
                err = cc_ccache_get_credentials_version (newCCache, &newVersion);
            }
            
            if (err == ccNoError) {
                if ((newVersion == cc_credentials_v4_v5) || (newVersion == cc_credentials_v5)) {
                    if (v5Credentials != NULL) {
                        newV5Credentials = v5Credentials;
                        err = [v5Credentials synchronizeWithCCache: newCCache version: cc_credentials_v5];
                    } else {
                        newV5Credentials = [[Credentials alloc] initWithCCache: newCCache version: cc_credentials_v5];
                        if (newV5Credentials == NULL) { err = ccErrNoMem; }
                    }
                }
            }
                
            if (err == ccNoError) {
                if ((newVersion == cc_credentials_v4_v5) || (newVersion == cc_credentials_v4)) {
                    if (v4Credentials != NULL) {
                        newV4Credentials = v4Credentials;
                        err = [v4Credentials synchronizeWithCCache: newCCache version: cc_credentials_v4];
                    } else {
                        newV4Credentials = [[Credentials alloc] initWithCCache: newCCache version: cc_credentials_v4];
                        if (newV4Credentials == NULL) { err = ccErrNoMem; }
                    }
                }
            }
            
            if (err == ccNoError) {
                if (ccache != NULL) { cc_ccache_release (ccache); }
                ccache = newCCache;
                
                if ((v5Credentials != NULL) && (v5Credentials != newV5Credentials)) { [v5Credentials release]; }
                v5Credentials = newV5Credentials;
                newV5Credentials = NULL;
                
                if ((v4Credentials != NULL) && (v4Credentials != newV4Credentials)) { [v4Credentials release]; }
                v4Credentials = newV4Credentials;
                newV4Credentials = NULL;
                
                version = newVersion;
                lastChangeTime = newLastChangeTime;
            }

            if (newV5Credentials != NULL) { [newV5Credentials release]; }
            if (newV4Credentials != NULL) { [newV4Credentials release]; }
        }
    }
    
    if (err == ccNoError) {
        // For some reason cache default changes don't always update the cache change time.
        // Do this after everything else is set up
        isDefault = [self isEqualToCCache: defaultCCache];
        lastDefaultTime = newLastDefaultTime;
    } else {
        dprintf ("synchronizeWithCCache:defaultCCache: returning error %d (%s)", err, error_message (err));
    }
    
    return err;
}

// ---------------------------------------------------------------------------

- (BOOL) isEqualToCCache: (cc_ccache_t) compareCCache
{
    cc_uint32 equal = NO;
    cc_int32  err = ccNoError;
 
    if (ccache        == NULL) { err = ccErrInvalidCCache; }
    if (compareCCache == NULL) { err = ccErrInvalidCCache; }
    
    if (err == ccNoError) {
        err = cc_ccache_compare (ccache, compareCCache, &equal);
    }
    
    if (err == ccNoError) {
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
    return ((v5Credentials != NULL) && [v5Credentials needsValidation]);
}

// ---------------------------------------------------------------------------

- (int) stateAtTime: (time_t) atTime
{
    int state = CredentialValid;
    
    if (v5Credentials != NULL) { state |= [v5Credentials stateAtTime: atTime]; }
    if (v4Credentials != NULL) { state |= [v4Credentials stateAtTime: atTime]; }
    
    return state;
}

// ---------------------------------------------------------------------------
    
- (cc_time_t) timeRemainingAtTime: (time_t) atTime
{
    if ((v5Credentials != NULL) && (v4Credentials != NULL)) {
        cc_time_t v5Remaining = [v5Credentials timeRemainingAtTime: atTime];
        cc_time_t v4Remaining = [v4Credentials timeRemainingAtTime: atTime];
        return (v5Remaining < v4Remaining) ? v5Remaining : v4Remaining;  // return minimum

    } else if ((v5Credentials != NULL) && (v4Credentials == NULL)) {
        return [v5Credentials timeRemainingAtTime: atTime];
    
    } else if ((v5Credentials == NULL) && (v4Credentials != NULL)) {
        return [v4Credentials timeRemainingAtTime: atTime];
    
    } else {
        return 0;
    }
}

// ---------------------------------------------------------------------------

- (Credentials *) dominantCredentials
{
    // favor v5 over v4
    if (v5Credentials != NULL) { return v5Credentials; }
    if (v4Credentials != NULL) { return v4Credentials; }
    dprintf ("Warning! dominantCredentials returning NULL!");
    return NULL;
}

// ---------------------------------------------------------------------------

- (NSString *) ccacheName
{
    NSString *string = NULL;
    cc_string_t name = NULL;
    
    if (cc_ccache_get_name (ccache, &name) == ccNoError) {
        string = [NSString stringWithCString: name->data];
    }
 
    if (name != NULL) { cc_string_release (name); }
    
    return string;
}

// ---------------------------------------------------------------------------

- (Principal *) principal
{
    Credentials *dominantCredentials = [self dominantCredentials];
    if (dominantCredentials != NULL) {
        return [dominantCredentials principal];
    } else {
        return NULL;
    }    
}

// ---------------------------------------------------------------------------

- (NSString *) principalString
{
    Credentials *dominantCredentials = [self dominantCredentials];
    if (dominantCredentials != NULL) {
        return [dominantCredentials principalString];
    } else {
        return NULL;
    }    
}

// ---------------------------------------------------------------------------

- (NSString *) stringValueForTimeRemainingScriptCommand
{
    time_t now = time (NULL);
    int state = [self stateAtTime: now];
    cc_time_t timeRemaining = [self timeRemainingAtTime: now];
    
    return [Utilities stringForTimeRemaining: timeRemaining 
                                       state: state 
                                      format: kLongFormat];
}

// ---------------------------------------------------------------------------

- (NSString *) stringValueForDockIcon
{
    time_t now = time (NULL);
    int state = [self stateAtTime: now];
    cc_time_t timeRemaining = [self timeRemainingAtTime: now];
    
    return [Utilities stringForTimeRemaining: timeRemaining 
                                       state: state 
                                      format: kShortFormat];
}

// ---------------------------------------------------------------------------

- (NSString *) stringValueForWindowTitle
{
    time_t now = time (NULL);
    int state = [self stateAtTime: now];
    cc_time_t timeRemaining = [self timeRemainingAtTime: now];
    
    NSString *format = NSLocalizedString (@"KAppStringWindowTitleFormat", NULL);
    NSString *principalString = [self principalString];
    NSString *timeRemainingString = [Utilities stringForTimeRemaining: timeRemaining 
                                                                state: state 
                                                               format: kLongFormat];
    
    return [NSString stringWithFormat: format, principalString, timeRemainingString];
}

// ---------------------------------------------------------------------------

- (NSAttributedString *) stringValueForMenuWithFontSize: (float) fontSize
{
    NSString *string = [self principalString];
    NSDictionary *attributes = [Utilities attributesForMenuItemOfFontSize: fontSize 
                                                                   italic: ([self stateAtTime: time (NULL)] != CredentialValid)];
    
    return [[[NSAttributedString alloc] initWithString: string attributes: attributes] autorelease];
}

// ---------------------------------------------------------------------------

- (NSAttributedString *) stringValueForTicketColumn
{
    NSString *string = [NSString stringWithFormat: @"(%@) %@", [Utilities stringForCCVersion: version], [self principalString]];
    NSDictionary *attributes = [Utilities attributesForTicketColumnCellOfControlSize: NSRegularControlSize
                                                                                bold: isDefault
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
    NSDictionary *attributes = [Utilities attributesForLifetimeColumnCellOfControlSize: NSRegularControlSize 
                                                                                  bold: isDefault
                                                                                 state: state
                                                                         timeRemaining: timeRemaining];
    
    return [[[NSAttributedString alloc] initWithString: string attributes: attributes] autorelease];
}


// ---------------------------------------------------------------------------

- (int) numberOfCredentialsVersions
{
    int count = 0;
    if (v5Credentials != NULL) { count++; }
    if (v4Credentials != NULL) { count++; }
    return count;
}

// ---------------------------------------------------------------------------

- (Credentials *) credentialsVersionAtIndex: (int) rowIndex
{
    if (rowIndex == 0) {
        if (v5Credentials != NULL) { return v5Credentials; }
        if (v4Credentials != NULL) { return v4Credentials; }
    } else if (rowIndex == 1) {
        if (v4Credentials != NULL) { return v4Credentials; }
        if (v5Credentials != NULL) { return v5Credentials; }
    }
    return NULL;
}

@end
