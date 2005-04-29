/*
 * KerberosApplication.m
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/KerberosApplication.m,v 1.2 2004/10/13 16:06:46 lxs Exp $
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

#import "ErrorAlert.h"

#import "KerberosApplication.h"

@implementation NSApplication (KerberosApplication) 

// ---------------------------------------------------------------------------

- (Principal *) getPrincipalArgumentForCommand: (NSScriptCommand *) command
{
    NSDictionary *args = [command evaluatedArguments];
    NSString *principalString = [args objectForKey: @"forPrincipal"];
    
    if (principalString != NULL) {
        return [[[Principal alloc] initWithString: principalString klVersion: kerberosVersion_V5] autorelease];
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
    Principal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal != NULL) {
        KLStatus err = [principal getTickets];
        if (err == klNoErr) {
            [[CacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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
    Principal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal != NULL) {
        KLStatus err = [principal renewTickets];
        if (err == klNoErr) {
            [[CacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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
    Principal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal != NULL) {
        KLStatus err = [principal destroyTickets];
        if (err == klNoErr) {
            [[CacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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
    Principal *principal = [self getPrincipalArgumentForCommand: command];
    if (principal != NULL) {
        KLStatus err = [principal changePassword];
        if (err == klNoErr) {
            [[CacheCollection sharedCacheCollection] update];
        } else if (err != klUserCanceledErr) {
            [ErrorAlert alertForError: err
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

- (CacheCollection *) cacheCollection
{
    return [CacheCollection sharedCacheCollection];  
}

// ---------------------------------------------------------------------------

- (NSArray *) caches
{
    return [[CacheCollection sharedCacheCollection] caches];  
}

// ---------------------------------------------------------------------------

- (Cache *) defaultCache
{
    return [[CacheCollection sharedCacheCollection] defaultCache];  
}


@end