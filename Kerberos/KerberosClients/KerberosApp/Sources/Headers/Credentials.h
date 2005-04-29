/*
 * Credentials.h
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/Credentials.h,v 1.4 2004/09/20 20:32:28 lxs Exp $
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

#import "Credential.h"
#import "Principal.h"

@interface Credentials : NSObject
{
    cc_uint32 version;
    Principal *principal;
    NSMutableArray *credentialsArray;
    int tgtIndex;
}

- (id) initWithCCache: (cc_ccache_t) ccache version: (cc_int32) credsVersion;
- (void) dealloc;

- (int) synchronizeWithCCache: (cc_ccache_t) ccache version: (cc_int32) credsVersion;
- (Credential *) findCredentialForCredentials: (cc_credentials_t) creds;

- (BOOL) needsValidation;

- (int) stateAtTime: (time_t) atTime;
- (cc_time_t) timeRemainingAtTime: (time_t) atTime;

- (Principal *) principal;
- (NSString *) principalString;

- (NSAttributedString *) stringValueForTicketColumn;
- (NSAttributedString *) stringValueForLifetimeColumn;

- (int) numberOfChildren;
- (id) childAtIndex: (int) rowIndex;

@end
