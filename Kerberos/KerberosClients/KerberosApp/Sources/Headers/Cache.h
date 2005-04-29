/*
 * Cache.h
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/Cache.h,v 1.4 2004/09/20 20:34:14 lxs Exp $
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

#import "Credentials.h"

@interface Cache : NSObject
{
    cc_ccache_t ccache;
    cc_time_t lastChangeTime;
    cc_time_t lastDefaultTime;
    cc_uint32 version;
    BOOL isDefault;
    Credentials *v4Credentials;
    Credentials *v5Credentials;
}

- (id) initWithCCache: (cc_ccache_t) newCCache defaultCCache: (cc_ccache_t) defaultName;
- (void) dealloc;

- (int) synchronizeWithCCache: (cc_ccache_t) newCCache defaultCCache: (cc_ccache_t) defaultName;
- (BOOL) isEqualToCCache: (cc_ccache_t) compareCCache;

- (cc_int32) lastDefaultTime;
- (BOOL) isDefault;

- (BOOL) needsValidation;

- (int) stateAtTime: (time_t) atTime;
- (cc_time_t) timeRemainingAtTime: (time_t) atTime;

- (Credentials *) dominantCredentials;

- (NSString *) ccacheName;
- (Principal *) principal;
- (NSString *) principalString;

- (NSString *) stringValueForTimeRemainingScriptCommand;
- (NSString *) stringValueForDockIcon;
- (NSString *) stringValueForWindowTitle;
- (NSAttributedString *) stringValueForMenuWithFontSize: (float) fontSize;
- (NSAttributedString *) stringValueForTicketColumn;
- (NSAttributedString *) stringValueForLifetimeColumn;
- (int) numberOfCredentialsVersions;
- (Credentials *) credentialsVersionAtIndex: (int) rowIndex;

@end
