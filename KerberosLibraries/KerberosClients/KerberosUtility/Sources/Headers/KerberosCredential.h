/*
 * KerberosCredential.h
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
#import "KerberosAddress.h"
#import "TargetOwnedTimer.h"

#define KerberosCredentialTimersNeedResetNotification @"KerberosCredentialTimersNeedReset"

#define CredentialValid           0x00000000
#define CredentialBeforeStartTime 0x00000001
#define CredentialNeedsValidation 0x00000002
#define CredentialBadAddress      0x00000004
#define CredentialExpired         0x00000008
#define CredentialInvalid         0xFFFFFFFF

// Time Remaining Formats
enum  {
    kShortFormat = 1,
    kLongFormat
};

@interface KerberosCredential : NSObject
{
    cc_credentials_t credentials;
    cc_uint32 ticketFlags;
    int isTGT;
    KerberosPrincipal *clientPrincipal;
    KerberosPrincipal *servicePrincipal;
    NSMutableArray *addressesArray;
    NSWindowController *infoWindowController;
    TargetOwnedTimer *credentialTimer;
    int failCount;
}

+ (NSString *) stringForState: (int) state
                  shortFormat: (BOOL) shortFormat;

+ (NSString *) stringForTimeRemaining: (cc_time_t) timeRemaining
                                state: (int) state
                          shortFormat: (BOOL) shortFormat;

- (id) initWithCredentials: (cc_credentials_t) creds;
- (void) dealloc;

- (int) synchronizeWithCredentials: (cc_credentials_t) creds;

- (void) resetCredentialTimer;
- (void) credentialTimer: (TargetOwnedTimer *) timer;

- (BOOL) isEqualToCredentials: (cc_credentials_t) compareCredentials;

- (time_t) currentTime;
- (time_t) issueTime;
- (time_t) startTime;
- (time_t) expirationTime;
- (time_t) renewUntilTime;

- (BOOL) hasValidAddress;
- (BOOL) isTGT;
- (BOOL) needsValidation;
- (cc_uint32) version;
- (BOOL) forwardable;
- (BOOL) forwarded;
- (BOOL) proxiable;
- (BOOL) proxied;
- (BOOL) postdatable;
- (BOOL) postdated;
- (BOOL) invalid;
- (BOOL) renewable;
- (BOOL) initial;
- (BOOL) preauthenticated;
- (BOOL) hardwareAuthenticated;
- (BOOL) isSKey;

- (int) state;
- (NSString *) shortStateString;
- (NSString *) longStateString;

- (cc_time_t) timeRemaining;
- (NSString *) shortTimeRemainingString;
- (NSString *) longTimeRemainingString;

- (cc_time_t) renewTimeRemaining;

- (NSArray *) addresses;

- (NSString *) sessionKeyEnctypeString;
- (NSString *) servicePrincipalKeyEnctypeString;
- (NSString *) clientPrincipalString;
- (NSString *) servicePrincipalString;
- (NSString *) versionString;

- (int) numberOfChildren;
- (id) childAtIndex: (int) rowIndex;

- (void) setInfoWindowController: (NSWindowController *) newWindowController;
- (NSWindowController *) infoWindowController;

- (void) windowWillClose: (NSNotification *) notification;
- (void) credentialTimerNeedsReset: (NSNotification *) notification;

@end

