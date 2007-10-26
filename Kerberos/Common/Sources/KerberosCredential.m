/*
 * KerberosCredential.m
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

#import "KerberosCacheCollection.h"
#import "KerberosCredential.h"
#import "KerberosPreferences.h"
#import "KerberosLifetimeFormatter.h"

#define KerberosCredentialString(key) NSLocalizedStringFromTable (key, @"KerberosCredential", NULL)

@implementation KerberosCredential

// ---------------------------------------------------------------------------

+ (NSString *) stringForState: (int) state
                  shortFormat: (BOOL) shortFormat
{
    NSString *shortStateString = NULL;
    NSString *longStateString = NULL;
    
    if (state == CredentialValid) {
        shortStateString = KerberosCredentialString (@"KCredentialStringValid");
    } else if (state & CredentialExpired) {
        shortStateString = KerberosCredentialString (@"KCredentialStringExpired");
    } else {
        shortStateString = KerberosCredentialString (@"KCredentialStringNotValid");
    }
    
    if (!shortFormat) {
        if (state & CredentialBeforeStartTime) {
            longStateString = KerberosCredentialString (@"KCredentialStringBeforeStartTime");
        } else if (state & CredentialNeedsValidation) {
            longStateString = KerberosCredentialString (@"KCredentialStringNeedsValidation");            
        } else if (state & CredentialBadAddress) {
            longStateString = KerberosCredentialString (@"KCredentialStringBadAddress");                            
        }
    }
    
    if (longStateString) {
        return [NSString stringWithFormat: KerberosCredentialString (@"KCredentialStringLongStateFormat"),
            shortStateString, longStateString];
    } else {
        return shortStateString;
    }
}

// ---------------------------------------------------------------------------

+ (NSString *) stringForTimeRemaining: (cc_time_t) timeRemaining
                                state: (int) state
                          shortFormat: (BOOL) shortFormat
{
    if (state == CredentialValid) {
        KerberosLifetimeFormatter *lifetimeFormatter = [[KerberosLifetimeFormatter alloc] initWithDisplaySeconds: NO
                                                                                                     shortFormat: shortFormat];
        NSString *string = [lifetimeFormatter stringForLifetime: timeRemaining];
        [lifetimeFormatter release];
        
        return string;
    } else {
        return [KerberosCredential stringForState: state shortFormat: shortFormat]; 
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

- (id) initWithCredentials: (cc_credentials_t) creds
{
    if ((self = [super init])) {
        dprintf ("Entering initWithCredentials: ...");
        
        credentials = NULL;
        ticketFlags = 0;
        isTGT = 0;
        clientPrincipal = NULL;
        servicePrincipal = NULL;
        addressesArray = NULL;
        infoWindowController = NULL;
        credentialTimer = NULL;
        failCount = 0;

        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: @selector (windowWillClose:)
                                                     name: NSWindowWillCloseNotification
                                                   object: nil];    
        
        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: @selector (credentialTimerNeedsReset:)
                                                     name: KerberosCredentialTimersNeedResetNotification
                                                   object: nil];    
                
        if ([self synchronizeWithCredentials: creds] != ccNoError) {
            [self release];
            self = NULL;
        }
        dprintf ("KerberosCredential for %s initializing", [[self servicePrincipalString] UTF8String]);
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("KerberosCredential '%s' for '%s' releasing",
             [[self servicePrincipalString] UTF8String], 
             [[self clientPrincipalString] UTF8String]);
    
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver: self];

    if (credentialTimer) {
        // invalidate a TargetOwnedTimer before releasing it
        [credentialTimer invalidate];
        [credentialTimer release];
    }
    
    if (infoWindowController) { [infoWindowController release]; }
    if (credentials         ) { cc_credentials_release (credentials); }
    if (clientPrincipal     ) { [clientPrincipal release]; }
    if (servicePrincipal    ) { [servicePrincipal release]; }
    if (addressesArray      ) { [addressesArray release]; }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (int) synchronizeWithCredentials: (cc_credentials_t) creds
{
    cc_int32 err = ccNoError;
    KerberosPrincipal *newClientPrincipal = NULL;
    KerberosPrincipal *newServicePrincipal = NULL;
    NSMutableArray *newAddressesArray = NULL;
    
    dprintf ("Entering synchronizeWithCredentials: ...");
    
    if (!err) {
        newClientPrincipal = [[KerberosPrincipal alloc] initWithClientPrincipalFromCredentials: creds];
        if (!newClientPrincipal) { err = ccErrNoMem; }
    }
    
    if (!err) {
        newServicePrincipal = [[KerberosPrincipal alloc] initWithServicePrincipalFromCredentials: creds];
        if (!newServicePrincipal) { err = ccErrNoMem; }
    }
    
    if (!err) {
        newAddressesArray = [[NSMutableArray alloc] init];
        if (!newAddressesArray) { err = ccErrNoMem; }
    }
    
    if (!err) {
        if (creds->data->version == cc_credentials_v5) {
            cc_data **cbase = creds->data->credentials.credentials_v5->addresses;
            if (cbase) {
                for (; *cbase != NULL; *cbase++) {
                    cc_data *ccAdr = *cbase;
                    KerberosAddress *address = NULL;
                    
                    if (!err) {
                        address = [[[KerberosAddress alloc] initWithType: ccAdr->type 
                                                          length: ccAdr->length
                                                        contents: ccAdr->data] autorelease];
                        if (!address) { err = ccErrNoMem; }
                    }
                    
                    if (!err) {
                        [newAddressesArray addObject: address];
                    }
                }
            }
        } else if (creds->data->version == cc_credentials_v4) {
            krb5_octet *data = (krb5_octet *) &creds->data->credentials.credentials_v4->address;
            KerberosAddress *address = [[[KerberosAddress alloc] initWithType: ADDRTYPE_INET 
                                                       length: INADDRSZ
                                                     contents: data] autorelease];
            if (!address) { err = ccErrNoMem; }
        
            if (!err) {
                [newAddressesArray addObject: address];
            }
        } else {
            err = ccErrBadCredentialsVersion;
        }
    }
    
    if (!err) {
        if (clientPrincipal) { [clientPrincipal release]; }
        clientPrincipal = newClientPrincipal;
        newClientPrincipal = NULL;  // don't free

        if (servicePrincipal) { [servicePrincipal release]; }
        servicePrincipal = newServicePrincipal;
        newServicePrincipal = NULL;  // don't free

        if (addressesArray) { [addressesArray release]; }
        addressesArray = newAddressesArray;
        newAddressesArray = NULL;  // don't free
        
        if (credentials) { cc_credentials_release (credentials); }
        credentials = creds;

        // for convenience
        if (creds->data->version == cc_credentials_v5) {
            ticketFlags = credentials->data->credentials.credentials_v5->ticket_flags;  
        } else {
            ticketFlags = 0;
        }
        isTGT = [servicePrincipal isTicketGrantingService];
        
        // Set up the timer now that the credentials are set up
        [self resetCredentialTimer];
    } else {
        dprintf ("synchronizeWithCCache:defaultCCache: returning error %d (%s)", err, error_message (err));
    }
    
    if (newClientPrincipal ) { [newClientPrincipal release]; }
    if (newServicePrincipal) { [newServicePrincipal release]; }
    
    return err;
}

// ---------------------------------------------------------------------------

- (void) resetCredentialTimer
{
    if ([self renewable] && [self initial]) {
        time_t timeRemaining = [self timeRemaining];
        time_t renewTimeRemaining = [self renewTimeRemaining];
        
        // Don't bother trying to if the tickets are expired, we wouldn't get longer tickets,
        // or in the last minute.
        if ((timeRemaining > 0) && (timeRemaining < renewTimeRemaining) && (renewTimeRemaining > 60)) {
            time_t now = [self currentTime];
            time_t lifetime = [self expirationTime] - [self startTime];
            
            NSDate *halfTimeRemainingDate = [NSDate dateWithTimeIntervalSince1970: now + (timeRemaining / 2)];
            NSDate *halfExpiredDate = [NSDate dateWithTimeIntervalSince1970: [self startTime] + (lifetime / 2)];
            NSDate *minuteDate = [NSDate dateWithTimeIntervalSinceNow: 60];
            NSDate *asapDate = [NSDate dateWithTimeIntervalSinceNow: 10];
            
            NSDate *newFireDate = NULL;
            dprintf ("halfTimeRemainingDate is %s", [[halfTimeRemainingDate description] UTF8String]);
            dprintf ("halfExpiredDate is %s", [[halfExpiredDate description] UTF8String]);
            dprintf ("minuteDate is %s", [[minuteDate description] UTF8String]);
            dprintf ("asapDate is %s", [[asapDate description] UTF8String]);
            
            if ([halfExpiredDate timeIntervalSinceNow] <= 0) {
                // If the tickets are more than half expired then fire immediately unless we are
                // rescheduling from a failed attempt.  If the last attempt failed try once in a
                // minute in case the net is loading and then back off exponentially.
                dprintf ("Tickets more than half expired.  FailCount is %d", failCount);
                if (failCount == 0) {
                    newFireDate = asapDate;
                } else if (failCount == 1) {
                    newFireDate = minuteDate;
                } else {
                    newFireDate = halfTimeRemainingDate;
                }
            } else {
                // Credentials are not yet half expired so try to renew them then
                dprintf ("Tickets not yet half expired");
                newFireDate = halfExpiredDate;
            }
            
            dprintf ("Setting timer to renew credentials for %s at %s", 
                     [[self clientPrincipalString] UTF8String], [[newFireDate description] UTF8String]);
            
            if (credentialTimer) {
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
    int err = 0;
    
    dprintf ("CredentialTimer firing... (credential '%s' for '%s')",
             [[self servicePrincipalString] UTF8String], 
             [[self clientPrincipalString] UTF8String]);

    if ([[KerberosPreferences sharedPreferences] autoRenewTickets]) {
        err = [clientPrincipal renewTicketsIfPossibleInBackground];
    }
    
    // KerberosCredential timer was invalidated when it fired (it doesn't repeat)
    [credentialTimer release];
    credentialTimer = NULL;

    // If the tickets got renewed then the credential will get invalidated 
    // and will get a new timer when it gets rescheduled.  So only reschedule
    // the timer if we didn't renew the tickets
    if (!err) {
        failCount = 0;
        [[KerberosCacheCollection sharedCacheCollection] update];
    } else {
        failCount++;
        [self resetCredentialTimer];
    }
}

// ---------------------------------------------------------------------------

- (BOOL) isEqualToCredentials: (cc_credentials_t) compareCredentials
{
    cc_uint32 equal = NO;
    cc_int32  err = ccNoError;
    
    if (!credentials       ) { err = ccErrInvalidCredentials; }
    if (!compareCredentials) { err = ccErrInvalidCredentials; }
    
    if (!err) {
        err = cc_credentials_compare (credentials, compareCredentials, &equal);
    }
    
    if (!err) {
        return equal;
    } else {
        // errors such as ccErrInvalidCCache mean they are not equal!
        return NO;
    }
}

// ---------------------------------------------------------------------------

- (time_t) currentTime
{
    return time (NULL);
}

// ---------------------------------------------------------------------------

- (time_t) issueTime
{
    if (credentials->data->version == cc_credentials_v5) {
        return credentials->data->credentials.credentials_v5->authtime;
    } else {
        return 0;
    }
}

// ---------------------------------------------------------------------------

- (time_t) startTime
{
    if (credentials->data->version == cc_credentials_v5) {
        return credentials->data->credentials.credentials_v5->starttime;
    } else {
        return 0;
    }
}

// ---------------------------------------------------------------------------

- (time_t) expirationTime
{
    if (credentials->data->version == cc_credentials_v5) {
        return credentials->data->credentials.credentials_v5->endtime;
    } else {
        return 0;
    }
}

// ---------------------------------------------------------------------------

- (time_t) renewUntilTime
{
    if (credentials->data->version == cc_credentials_v5) {
        return credentials->data->credentials.credentials_v5->renew_till;
    } else {
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
    return isTGT;
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

- (int) state
{
    time_t now = [self currentTime];
    int state = CredentialValid;  // no bad states
    
    // account for 5 minute offset so we don't get confused by brand new tickets
    if ([self needsValidation]) { 
        state |= CredentialNeedsValidation; 
        if (now < [self startTime])          { state |= CredentialBeforeStartTime; } 
    } else {
        // account for 5 minute offset so we don't get confused by brand new tickets
        if ((now + 5*60) < [self startTime]) { state |= CredentialBeforeStartTime; } 
    }
    if (![self hasValidAddress])             { state |= CredentialBadAddress; }
    if (now >= [self expirationTime])        { state |= CredentialExpired;    }
    
    return state;
}

// ---------------------------------------------------------------------------

- (NSString *) shortStateString
{
    return [KerberosCredential stringForState: [self state] shortFormat: YES]; 
}

// ---------------------------------------------------------------------------

- (NSString *) longStateString
{
    return [KerberosCredential stringForState: [self state] shortFormat: NO]; 
}

// ---------------------------------------------------------------------------

- (cc_time_t) timeRemaining
{
    time_t now = [self currentTime];
    cc_time_t expirationTime = [self expirationTime];
    return ((now < 0) || (expirationTime > (unsigned) now)) ? (expirationTime - now) : 0;
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

- (cc_time_t) renewTimeRemaining
{
    time_t now = [self currentTime];
    cc_time_t renewUntilTime = [self renewUntilTime];
    return ((now < 0) || (renewUntilTime > (unsigned) now)) ? (renewUntilTime - now) : 0;
}

// ---------------------------------------------------------------------------

- (NSArray *) addresses
{
    return addressesArray;
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
        if (!err) {
            string = [NSString stringWithUTF8String: enctypeString];
        }
    }

    return (string != NULL) ? string : KerberosCredentialString (@"KCredentialStringUnknownEnctype");
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
        
        if (!err) {
            err = krb5_decode_ticket (&ticketData, &decodedTicket);
        }
        
        if (!err) {
            err = krb5_enctype_to_string (decodedTicket->enc_part.enctype, encTypeString, sizeof (encTypeString));
        }
        
        if (!err) {
            string = [NSString stringWithUTF8String: encTypeString];
        }
        
        if (decodedTicket) { krb5_free_ticket (context, decodedTicket); }
        if (context      ) { krb5_free_context (context); }
    }
    
    return (string != NULL) ? string : KerberosCredentialString (@"KCredentialStringUnknownEnctype");
}

// ---------------------------------------------------------------------------

- (NSString *) clientPrincipalString
{
    return [clientPrincipal displayString];
}

// ---------------------------------------------------------------------------

- (NSString *) servicePrincipalString
{
    return [servicePrincipal displayString];
}

// ---------------------------------------------------------------------------

- (NSString *) versionString
{
    NSString *key = @"KCredentialStringCredentialsVersionNone";
    
    if (credentials->data->version == cc_credentials_v5) {
        key = @"KCredentialStringCredentialsVersionV5";
    } else if (credentials->data->version == cc_credentials_v4) {
        key = @"KCredentialStringCredentialsVersionV4";        
    } else if (credentials->data->version == cc_credentials_v4_v5) {
        key = @"KCredentialStringCredentialsVersionV4V5";        
    }
    
    return KerberosCredentialString (key);
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

- (void) setInfoWindowController: (NSWindowController *) newInfoWindowController
{
    infoWindowController = [newInfoWindowController retain];
}

// ---------------------------------------------------------------------------

- (NSWindowController *) infoWindowController
{
    return infoWindowController;
}

// ---------------------------------------------------------------------------

- (void) windowWillClose: (NSNotification *) notification
{
    if ([infoWindowController window] == [notification object]) {
        dprintf ("KerberosCredential noticed info Window closing...");
        [infoWindowController release];
        infoWindowController = NULL;
    }
}

// ---------------------------------------------------------------------------

- (void) credentialTimerNeedsReset: (NSNotification *) notification
{
    dprintf ("KerberosCredential '%s' for '%s' noticed credential timer needs reset",
             [[self servicePrincipalString] UTF8String], 
             [[self clientPrincipalString] UTF8String]);
    [self resetCredentialTimer];
}

@end
