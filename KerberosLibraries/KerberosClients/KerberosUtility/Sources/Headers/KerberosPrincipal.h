/*
 * KerberosPrincipal.h
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

#include <Kerberos/Kerberos.h>

@interface KerberosPrincipal : NSObject
{
    kim_identity identity;
}

- (id) initWithString: (NSString *) string;

- (id) initWithName: (NSString *) name 
           instance: (NSString *) instance 
              realm: (NSString *) realm;

- (id) initWithClientPrincipalFromCredentials: (cc_credentials_t) credentials;
- (id) initWithServicePrincipalFromCredentials: (cc_credentials_t) credentials;

- (void) dealloc;

- (int) setKeychainPassword: (NSString *) password;
- (int) removeKeychainPassword;
- (NSString *) keychainPassword;

- (BOOL) isTicketGrantingService;

- (NSString *) displayString;

- (NSString *) string;

- (NSArray *)componentsArray;

- (NSString *)realmString;

- (int) changePassword;

- (int) changePasswordWithOldPassword: (NSString *) oldPassword
                          newPassword: (NSString *) newPassword
                             rejected: (BOOL *) rejected
                       rejectionError: (NSMutableString *) rejectionError
                 rejectionDescription: (NSMutableString *) rejectionDescription;

- (int) setDefault;

- (int) destroyTickets;

- (int) renewTickets;

- (int) renewTicketsIfPossibleInBackground;

- (int) validateTickets;

- (int) getTickets;

- (int) getTicketsWithPassword: (NSString *) password 
                  loginOptions: (KLLoginOptions) loginOptions 
                     cacheName: (NSMutableString *) ioCacheName;

+ (int) getTickets;

@end
