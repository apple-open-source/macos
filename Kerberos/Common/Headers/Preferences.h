/*
 * Preferences.h
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


typedef enum _LaunchAction {
    LaunchActionAlwaysOpenTicketWindow = 1,
    LaunchActionNeverOpenTicketWindow,
    LaunchActionRememberOpenTicketWindow
} LaunchAction;

@interface Preferences : NSObject
{
}

+ (Preferences *) sharedPreferences;

- (BOOL) autoRenewTickets;
- (void) setAutoRenewTickets: (BOOL) autoRenewTickets;

- (BOOL) showTimeInDockIcon;
- (void) setShowTimeInDockIcon: (BOOL) showTimeInDockIcon;

- (LaunchAction) launchAction;
- (void) setLaunchAction: (LaunchAction) launchAction;

- (BOOL) ticketWindowLastOpen;
- (void) setTicketWindowLastOpen: (BOOL) ticketWindowLastOpen;

- (BOOL) ticketWindowDefaultPosition;
- (void) setTicketWindowDefaultPosition: (BOOL) ticketWindowDefaultPosition;


- (BOOL) rememberShowOptions;
- (void) setRememberShowOptions: (BOOL) rememberShowOptions;

- (BOOL) rememberPrincipalFromLastLogin;
- (void) setRememberPrincipalFromLastLogin: (BOOL) rememberPrincipalFromLastLogin;

- (NSString *) defaultName;
- (void) setDefaultName: (NSString *) defaultName;

- (NSString *) defaultRealm;
- (void) setDefaultRealm: (NSString *) defaultRealm;

- (NSArray *) realms;


- (BOOL) showOptions;
- (void) setShowOptions: (BOOL) showOptions;

- (BOOL) rememberOptionsFromLastLogin;
- (void) setRememberOptionsFromLastLogin: (BOOL) rememberOptionsFromLastLogin;

- (time_t) defaultLifetime;
- (void) setDefaultLifetime: (time_t) defaultLifetime;

- (BOOL) defaultForwardable;
- (void) setDefaultForwardable: (BOOL) defaultForwardable;

- (BOOL) defaultProxiable;
- (void) setDefaultProxiable: (BOOL) defaultProxiable;

- (BOOL) defaultAddressless;
- (void) setDefaultAddressless: (BOOL) defaultAddressless;

- (BOOL) defaultRenewable;
- (void) setDefaultRenewable: (BOOL) defaultRenewable;

- (time_t) defaultRenewableLifetime;
- (void) setDefaultRenewableLifetime: (time_t) defaultRenewableLifetime;


- (time_t) lifetimeMaximum;
- (void) setLifetimeMaximum: (time_t) lifetimeMaximum;

- (time_t) lifetimeMinimum;
- (void) setLifetimeMinimum: (time_t) lifetimeMinimum;

- (time_t) renewableLifetimeMaximum;
- (void) setRenewableLifetimeMaximum: (time_t) renewableLifetimeMaximum;

- (time_t) renewableLifetimeMinimum;
- (void) setRenewableLifetimeMinimum: (time_t) renewableLifetimeMinimum;

@end
