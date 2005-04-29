/*
 * Credential.m
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Credential.m,v 1.12 2005/01/31 20:50:51 lxs Exp $
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

#import "CacheCollection.h"
#import "Credential.h"
#import "Utilities.h"
#import "TicketInfoController.h"
#import "Preferences.h"


@implementation Credential

// ---------------------------------------------------------------------------

- (id) initWithCredentials: (cc_credentials_t) creds
{
    if ((self = [super init])) {
        dprintf ("Entering initWithCredentials: ...");
        
        credentials = NULL;
        ticketFlags = 0;
        clientPrincipal = NULL;
        servicePrincipal = NULL;
        addressesArray = NULL;
        infoWindowController = NULL;
        credentialTimer = NULL;

        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: @selector (windowWillClose:)
                                                     name: @"NSWindowWillCloseNotification" 
                                                   object: nil];    
        
        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: @selector (wakeFromSleep:)
                                                     name: WakeFromSleepNotification 
                                                   object: nil];    
        
        if ([self synchronizeWithCredentials: creds] != ccNoError) {
            [self release];
            self = NULL;
        }
        dprintf ("Credential for %s initializing", [[self servicePrincipalString] UTF8String]);
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("Credential '%s' for '%s' releasing",
             [[self servicePrincipalString] UTF8String], 
             [[self clientPrincipalString] UTF8String]);
    
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    if (credentialTimer != NULL) {
        // invalidate a TargetOwnedTimer before releasing it
        [credentialTimer invalidate];
        [credentialTimer release];
    }
    
    if (infoWindowController != NULL) { [infoWindowController release]; }
    if (credentials          != NULL) { cc_credentials_release (credentials); }
    if (clientPrincipal      != NULL) { [clientPrincipal release]; }
    if (servicePrincipal     != NULL) { [servicePrincipal release]; }
    if (addressesArray       != NULL) { [addressesArray release]; }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (int) synchronizeWithCredentials: (cc_credentials_t) creds
{
    cc_int32 err = ccNoError;
    Principal *newClientPrincipal = NULL;
    Principal *newServicePrincipal = NULL;
    NSMutableArray *newAddressesArray = NULL;
    
    dprintf ("Entering synchronizeWithCredentials: ...");
    
    if (err == ccNoError) {
        newClientPrincipal = [[Principal alloc] initWithClientPrincipalFromCredentials: creds];
        if (newClientPrincipal == NULL) { err = ccErrNoMem; }
    }
    
    if (err == ccNoError) {
        newServicePrincipal = [[Principal alloc] initWithServicePrincipalFromCredentials: creds];
        if (newServicePrincipal == NULL) { err = ccErrNoMem; }
    }
    
    if (err == ccNoError) {
        newAddressesArray = [[NSMutableArray alloc] init];
        if (newAddressesArray == NULL) { err = ccErrNoMem; }
    }
    
    if (err == ccNoError) {
        if (creds->data->version == cc_credentials_v5) {
            cc_data **cbase = creds->data->credentials.credentials_v5->addresses;
            if (cbase != NULL) {
                for (; *cbase != NULL; *cbase++) {
                    cc_data *ccAdr = *cbase;
                    Address *address = NULL;
                    
                    if (err == ccNoError) {
                        address = [[[Address alloc] initWithType: ccAdr->type 
                                                          length: ccAdr->length
                                                        contents: ccAdr->data] autorelease];
                        if (address == NULL) { err = ccErrNoMem; }
                    }
                    
                    if (err == ccNoError) {
                        [newAddressesArray addObject: address];
                    }
                }
            }
        } else if (creds->data->version == cc_credentials_v4) {
            krb5_octet *data = (krb5_octet *) &creds->data->credentials.credentials_v4->address;
            Address *address = [[[Address alloc] initWithType: ADDRTYPE_INET 
                                                       length: INADDRSZ
                                                     contents: data] autorelease];
            if (address == NULL) { err = ccErrNoMem; }
        
            if (err == ccNoError) {
                [newAddressesArray addObject: address];
            }
        } else {
            err = ccErrBadCredentialsVersion;
        }
    }
    
    if (err == ccNoError) {
        if (clientPrincipal != NULL) { [clientPrincipal release]; }
        clientPrincipal = newClientPrincipal;
        newClientPrincipal = NULL;  // don't free

        if (servicePrincipal != NULL) { [servicePrincipal release]; }
        servicePrincipal = newServicePrincipal;
        newServicePrincipal = NULL;  // don't free

        if (addressesArray != NULL) { [addressesArray release]; }
        addressesArray = newAddressesArray;
        newAddressesArray = NULL;  // don't free
        
        if (credentials != NULL) { cc_credentials_release (credentials); }
        credentials = creds;

        // for convenience
        if (creds->data->version == cc_credentials_v5) {
            ticketFlags = credentials->data->credentials.credentials_v5->ticket_flags;  
        } else {
            ticketFlags = 0;
        }
        
        // Set up the timer now that the credentials are set up
        [self setupTimerLastAttemptFailed: NO];
    } else {
        dprintf ("synchronizeWithCCache:defaultCCache: returning error %d (%s)", err, error_message (err));
    }
    
    if (newClientPrincipal  != NULL) { [newClientPrincipal release]; }
    if (newServicePrincipal != NULL) { [newServicePrincipal release]; }
    
    return err;
}

// ---------------------------------------------------------------------------

- (void) setupTimerLastAttemptFailed: (BOOL) lastAttemptFailed
{
    if ([self renewable] && [self initial]) {
        
        time_t now = time (NULL);
        time_t timeRemaining = [self timeRemainingAtTime: now];
        
        if (timeRemaining > 0) {
            time_t lifetime = [self expirationTime] - [self startTime];
            
            NSDate *halfTimeRemainingDate = [NSDate dateWithTimeIntervalSince1970: now + (timeRemaining / 2)];
            NSDate *halfExpiredDate = [NSDate dateWithTimeIntervalSince1970: [self startTime] + (lifetime / 2)];
            NSDate *asapDate = [NSDate dateWithTimeIntervalSinceNow: 1];

            NSDate *newFireDate = NULL;
            dprintf ("halfTimeRemainingDate is %s", [[halfTimeRemainingDate description] UTF8String]);
            dprintf ("halfExpiredDate is %s", [[halfExpiredDate description] UTF8String]);
            dprintf ("asapDate is %s", [[asapDate description] UTF8String]);
            
            if ([halfExpiredDate timeIntervalSinceNow] <= 0) {
                // If the tickets are more than half expired then fire immediately unless we are
                // rescheduling from an failed attempt because then this one will probably fail too
                // If that happens use halfTimeRemainingDate to exponentially back off
                dprintf ("Tickets more than half expired");
                newFireDate = lastAttemptFailed ? halfTimeRemainingDate : asapDate;
            } else {
                // Credentials are not yet half expired so try to renew them then
                dprintf ("Tickets not yet half expired");
                newFireDate = halfExpiredDate;
            }
            
            dprintf ("Setting timer to renew credentials for %s at %s", 
                   [[self clientPrincipalString] UTF8String], [[newFireDate description] UTF8String]);
                        
            if (credentialTimer != NULL) {
                [credentialTimer invalidate];
                [credentialTimer release];
            }

            credentialTimer = [[TargetOwnedTimer scheduledTimerWithFireDate: newFireDate
                                                                   interval: 0
                                                                     target: self 
                                                                   selector: @selector (credentialTimer:) 
                                                                   userInfo: NULL
                                                                    repeats: NO] retain];
        }
    }    
}

// ---------------------------------------------------------------------------

- (void) credentialTimer: (TargetOwnedTimer *) timer 
{
    BOOL renewed = NO;
    
    dprintf ("CredentialTimer firing... (credential '%s' for '%s')",
             [[self servicePrincipalString] UTF8String], 
             [[self clientPrincipalString] UTF8String]);

    if ([[Preferences sharedPreferences] autoRenewTickets]) {
        renewed = [clientPrincipal renewTicketsIfPossibleInBackground];
    }
    
    // Credential timer was invalidated when it fired (it doesn't repeat)
    [credentialTimer release];
    credentialTimer = NULL;

    // If the tickets got renewed then the credential will get invalidated 
    // and will get a new timer when it gets rescheduled.  So only reschedule
    // the timer if we didn't renew the tickets
    if (renewed) {
        [[CacheCollection sharedCacheCollection] update];
    } else {
        [self setupTimerLastAttemptFailed: YES];
    }
}

// ---------------------------------------------------------------------------

- (BOOL) isEqualToCredentials: (cc_credentials_t) compareCredentials
{
    cc_uint32 equal = NO;
    cc_int32  err = ccNoError;
    
    if (credentials        == NULL) { err = ccErrInvalidCredentials; }
    if (compareCredentials == NULL) { err = ccErrInvalidCredentials; }
    
    if (err == ccNoError) {
        err = cc_credentials_compare (credentials, compareCredentials, &equal);
    }
    
    if (err == ccNoError) {
        return equal;
    } else {
        // errors such as ccErrInvalidCCache mean they are not equal!
        return NO;
    }
}

// ---------------------------------------------------------------------------

- (time_t) issueTime
{
    switch (credentials->data->version) {
        case cc_credentials_v5:
            return credentials->data->credentials.credentials_v5->authtime;
        case cc_credentials_v4:
            return credentials->data->credentials.credentials_v4->issue_date;
        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------

- (time_t) startTime
{
    switch (credentials->data->version) {
        case cc_credentials_v5:
            return credentials->data->credentials.credentials_v5->starttime;
        case cc_credentials_v4:
            return credentials->data->credentials.credentials_v4->issue_date;
        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------

- (time_t) expirationTime
{
    switch (credentials->data->version) {
        case cc_credentials_v5:
            return credentials->data->credentials.credentials_v5->endtime;
        case cc_credentials_v4:
            return (credentials->data->credentials.credentials_v4->issue_date + 
                    credentials->data->credentials.credentials_v4->lifetime);
        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------

- (time_t) renewUntilTime
{
    switch (credentials->data->version) {
        case cc_credentials_v5:
            return credentials->data->credentials.credentials_v5->renew_till;
        case cc_credentials_v4:
        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------

- (BOOL) hasValidAddress
{
    BOOL foundValidAddress = NO;
    
    if ([addressesArray count] == 0) {
        // ticket has no addresses
        foundValidAddress = YES;
    } else {
        // ticket has addresses 
        krb5_context context = NULL;
        
        if (krb5_init_context (&context) == 0) {
            krb5_address **localAddresses = NULL;

            if (krb5_os_localaddr (context, &localAddresses) == 0) {
                // Machine has addresses.  Check each ticket address
                unsigned int i;
                for (i = 0; i < [addressesArray count]; i++) {
                    krb5_address *ticketAddress = [[addressesArray objectAtIndex: i] krb5_address];
                    if (krb5_address_search (context, ticketAddress, localAddresses) == TRUE) {
                        foundValidAddress = YES; 
                        break;
                    }
                }
                krb5_free_addresses (context, localAddresses);
            } else {
                // Machine has no addresses (off net) so we assume ticket addresses are valid
                foundValidAddress = YES; 
            }
            krb5_free_context (context);
        }
    }
    
    return foundValidAddress;
}

// ---------------------------------------------------------------------------

- (BOOL) isTGT
{
    if ([self version] == cc_credentials_v5) {
        return [self initial];
    } else {
        return [servicePrincipal isTicketGrantingServiceForCCVersion: credentials->data->version];
    }
}

// ---------------------------------------------------------------------------

- (BOOL) needsValidation
{
    return ([self postdated] && [self invalid]);
}

// ---------------------------------------------------------------------------

- (cc_uint32) version
{
    return (credentials->data->version);
}

// ---------------------------------------------------------------------------

- (BOOL) forwardable
{
    return ((ticketFlags & TKT_FLG_FORWARDABLE) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) forwarded
{
    return ((ticketFlags & TKT_FLG_FORWARDED) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) proxiable
{
    return ((ticketFlags & TKT_FLG_PROXIABLE) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) proxied
{
    return ((ticketFlags & TKT_FLG_PROXY) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) postdatable
{
    return ((ticketFlags & TKT_FLG_MAY_POSTDATE) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) postdated
{
    return ((ticketFlags & TKT_FLG_POSTDATED) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) invalid
{
    return ((ticketFlags & TKT_FLG_INVALID) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) renewable
{
    return ((ticketFlags & TKT_FLG_RENEWABLE) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) initial
{
    return ((ticketFlags & TKT_FLG_INITIAL) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) preauthenticated
{
    return ((ticketFlags & TKT_FLG_PRE_AUTH) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) hardwareAuthenticated
{
    return ((ticketFlags & TKT_FLG_HW_AUTH) != 0);
}

// ---------------------------------------------------------------------------

- (BOOL) isSKey
{
    if (credentials->data->version == cc_credentials_v5) {
        return (credentials->data->credentials.credentials_v5->is_skey);
    } else {
        return NO;
    }
}

// ---------------------------------------------------------------------------

- (int) stateAtTime: (time_t) atTime
{
    int state = CredentialValid;  // no bad states
    
    // account for 5 minute offset so we don't get confused by brand new tickets
    if ([self needsValidation]) { 
        state |= CredentialNeedsValidation; 
        if (atTime < [self startTime])          { state |= CredentialBeforeStartTime; } 
    } else {
        // account for 5 minute offset so we don't get confused by brand new tickets
        if ((atTime + 5*60) < [self startTime]) { state |= CredentialBeforeStartTime; } 
    }
    if (![self hasValidAddress])                { state |= CredentialBadAddress; }
    if (atTime >= [self expirationTime])        { state |= CredentialExpired;    }
    
    return state;
}

// ---------------------------------------------------------------------------

- (cc_time_t) timeRemainingAtTime: (time_t) atTime
{
    cc_time_t expirationTime = [self expirationTime];
    return ((atTime < 0) || (expirationTime > (unsigned) atTime)) ? (expirationTime - atTime) : 0;
}

// ---------------------------------------------------------------------------

- (NSArray *) addresses
{
    return addressesArray;
}

// ---------------------------------------------------------------------------

- (NSString *) stringToKeyTypeString
{
    NSString *string = NULL;

    switch (credentials->data->credentials.credentials_v4->string_to_key_type) {
        case cc_v4_stk_afs:
            string = NSLocalizedString (@"KAppStringAFSEnctype", NULL);
            break;
            
        case cc_v4_stk_des:
            string = NSLocalizedString (@"KAppStringDESEnctype", NULL);
            break;
            
        case cc_v4_stk_columbia_special:
            string = NSLocalizedString (@"KAppStringColumbiaEnctype", NULL);
            break;
            
        case cc_v4_stk_unknown:
            // we autodetect, and always fill string_to_key in as unknown
            // but we'll label it "Automatic" to make users feel better
            string = NSLocalizedString (@"KAppStringAutomaticEnctype", NULL);
            break;
    }
    
    return (string != NULL) ? string : NSLocalizedString (@"KAppStringUnknownEnctype", NULL);
}

// ---------------------------------------------------------------------------

- (NSString *) sessionKeyEnctypeString
{
    NSString *string = NULL;

    if (credentials->data->version == cc_credentials_v5) {
        krb5_error_code err = 0;
        char enctypeString[BUFSIZ];
        krb5_enctype enctype = credentials->data->credentials.credentials_v5->keyblock.type;
        
        err = krb5_enctype_to_string (enctype, enctypeString, sizeof (enctypeString));
        if (err == 0) {
            string = [NSString stringWithUTF8String: enctypeString];
        }
    }

    return (string != NULL) ? string : NSLocalizedString (@"KAppStringUnknownEnctype", NULL);
}

// ---------------------------------------------------------------------------

- (NSString *) servicePrincipalKeyEnctypeString
{
    NSString *string = NULL;
    
    if (credentials->data->version == cc_credentials_v5) {
        krb5_error_code err = 0;
        krb5_context context = NULL;
        krb5_ticket *decodedTicket = NULL;
        krb5_data ticketData = { 
            0,  /* magic */
            credentials->data->credentials.credentials_v5->ticket.length,
            credentials->data->credentials.credentials_v5->ticket.data 
        };
        char encTypeString[BUFSIZ];
        
        err = krb5_init_context (&context);
        
        if (err == 0) {
            err = krb5_decode_ticket (&ticketData, &decodedTicket);
        }
        
        if (err == 0) {
            err = krb5_enctype_to_string (decodedTicket->enc_part.enctype, encTypeString, sizeof (encTypeString));
        }
        
        if (err == 0) {
            string = [NSString stringWithUTF8String: encTypeString];
        }
        
        if (decodedTicket != NULL) { krb5_free_ticket (context, decodedTicket); }
        if (context       != NULL) { krb5_free_context (context); }
    }
    
    return (string != NULL) ? string : NSLocalizedString (@"KAppStringUnknownEnctype", NULL);
}

// ---------------------------------------------------------------------------

- (NSString *) clientPrincipalString
{
    return [clientPrincipal displayStringForCCVersion: credentials->data->version];
}

// ---------------------------------------------------------------------------

- (NSString *) servicePrincipalString
{
    return [servicePrincipal displayStringForCCVersion: credentials->data->version];
}

// ---------------------------------------------------------------------------

- (NSString *) versionString
{
    return [Utilities stringForCCVersion: credentials->data->version];
}

// ---------------------------------------------------------------------------

- (NSAttributedString *) stringValueForTicketInfoWindow
{
    time_t now = time (NULL);
    int state = [self stateAtTime: now];
    
    NSString *string = [Utilities stringForCredentialState: state 
                                                    format: kLongFormat];
    NSDictionary *attributes = [Utilities attributesForInfoWindowWithTicketState: state];
    
    return [[NSAttributedString alloc] initWithString: string attributes: attributes];
}

// ---------------------------------------------------------------------------

- (NSAttributedString *) stringValueForTicketColumn
{
    NSString *string = [self servicePrincipalString];
    NSDictionary *attributes = [Utilities attributesForTicketColumnCellOfControlSize: NSSmallControlSize
                                                                                bold: NO
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
                                                                                  bold: NO
                                                                                 state: state
                                                                         timeRemaining: timeRemaining];
    
    return [[[NSAttributedString alloc] initWithString: string attributes: attributes] autorelease];
}

// ---------------------------------------------------------------------------

- (int) numberOfChildren
{
    return 0;
}

// ---------------------------------------------------------------------------

- (id) childAtIndex: (int) rowIndex
{
    return NULL;
}

// ---------------------------------------------------------------------------

- (NSPoint) showInfoWindowCascadingFromPoint: (NSPoint) cascadePoint
{
    if (infoWindowController == NULL) {
        infoWindowController = [[TicketInfoController alloc] initWithCredential: self];
    }
    NSPoint newCascadePoint = [[infoWindowController window] cascadeTopLeftFromPoint: cascadePoint];
    [infoWindowController showWindow: self];
    return newCascadePoint;
}

// ---------------------------------------------------------------------------

- (void) windowWillClose: (NSNotification *) notification
{
    if ([infoWindowController window] == [notification object]) {
        dprintf ("Credential noticed info Window closing...");
        [infoWindowController release];
        infoWindowController = NULL;
    }
}

// ---------------------------------------------------------------------------

- (void) wakeFromSleep: (NSNotification *) notification
{
    dprintf ("Credential '%s' for '%s' noticed wake from sleep... rescheduling timer",
             [[self servicePrincipalString] UTF8String], 
             [[self clientPrincipalString] UTF8String]);
    [self setupTimerLastAttemptFailed: NO];
}

@end
