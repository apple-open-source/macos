/*
 * KerberosApplication.m
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

/*
 * This is a category for NSApplication which implements AppleScript commands
 * specific to Kerberos 
 */

#import "KerberosErrorAlert.h"

#import "KerberosApplication.h"

@implementation NSApplication (KerberosApplication) 

// ---------------------------------------------------------------------------

- (KerberosPrincipal *) getPrincipalArgumentForCommand: (NSScriptCommand *) command
{
    NSDictionary *args = [command evaluatedArguments];
    NSString *principalString = [args objectForKey: @"forPrincipal"];
    
    if (principalString) {
        return [[[KerberosPrincipal alloc] initWithString: principalString] autorelease];
    }
    return NULL;
}

#pragma mark -- Commands --

// ---------------------------------------------------------------------------

- (id) handleShowTicketListScriptCommand: (NSScriptCommand *) command
{
    //NSLog (@"Entering %s....", __FUNCTION__);
    [[self delegate] showTicketList: self];
    
    return NULL;
}

// ---------------------------------------------------------------------------

- (id) handleGetTicketsScriptCommand: (NSScriptCommand *) command
{
    //NSLog (@"Entering %s....", __FUNCTION__);
    KerberosPrincipal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal) {
        KLStatus err = [principal getTickets];
        if (!err) {
            [[KerberosCacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr && err != KIM_USER_CANCELED_ERR) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosGetTicketsAction];
        }
    } else {
        [[self delegate] getTickets: self];
    }
    return NULL;
}

// ---------------------------------------------------------------------------

- (id) handleRenewTicketsScriptCommand: (NSScriptCommand *) command
{
    //NSLog (@"Entering %s....", __FUNCTION__);
    KerberosPrincipal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal) {
        KLStatus err = [principal renewTickets];
        if (!err) {
            [[KerberosCacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr && err != KIM_USER_CANCELED_ERR) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosRenewTicketsAction];
        }
    } else {
        [[self delegate] renewTicketsForActiveUser: self];
    }
    return NULL;
}

// ---------------------------------------------------------------------------

- (id) handleDestroyTicketsScriptCommand: (NSScriptCommand *) command
{
    //NSLog (@"Entering %s....", __FUNCTION__);
    KerberosPrincipal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal) {
        KLStatus err = [principal destroyTickets];
        if (!err) {
            [[KerberosCacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr && err != KIM_USER_CANCELED_ERR) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosDestroyTicketsAction];
        }
    } else {
        [[self delegate] destroyTicketsForActiveUser: self];
    }
    return NULL;
}

// ---------------------------------------------------------------------------

- (id) handleChangePasswordScriptCommand: (NSScriptCommand *) command
{
    //NSLog (@"Entering %s....", __FUNCTION__);
    KerberosPrincipal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal) {
        KLStatus err = [principal changePassword];
        if (!err) {
            [[KerberosCacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr && err != KIM_USER_CANCELED_ERR) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosChangePasswordAction];
        }
    } else {
        [[self delegate] changePasswordForActiveUser: self];
    }
    return NULL;
}

#pragma mark -- Attributes --

// ---------------------------------------------------------------------------

- (NSString *) testString
{
    return @"Hello world";  
}

// ---------------------------------------------------------------------------

- (KerberosCacheCollection *) cacheCollection
{
    return [KerberosCacheCollection sharedCacheCollection];  
}

// ---------------------------------------------------------------------------

- (NSArray *) caches
{
    return [[KerberosCacheCollection sharedCacheCollection] caches];  
}

// ---------------------------------------------------------------------------

- (KerberosCache *) defaultCache
{
    return [[KerberosCacheCollection sharedCacheCollection] defaultCache];  
}


@end
