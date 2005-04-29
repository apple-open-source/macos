/*
 * KerberosApplication.h
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/KerberosApplication.h,v 1.1 2004/09/20 20:34:17 lxs Exp $
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

#import "KerberosController.h"
#import "CacheCollection.h"
#import "Principal.h"

/*
 * This is a category for NSApplication which implements AppleScript commands
 * specific to Kerberos and relays them to the appropriate object
 */

@interface NSApplication (KerberosApplication) 

- (Principal *) getPrincipalArgumentForCommand: (NSScriptCommand *) command;

- (id) handleShowTicketListScriptCommand: (NSScriptCommand *) command;
- (id) handleGetTicketsScriptCommand: (NSScriptCommand *) command;
- (id) handleRenewTicketsScriptCommand: (NSScriptCommand *) command;
- (id) handleDestroyTicketsScriptCommand: (NSScriptCommand *) command;
- (id) handleChangePasswordScriptCommand: (NSScriptCommand *) command;

- (NSString *) testString;
- (CacheCollection *) cacheCollection;
- (NSArray *) caches;
- (Cache *) defaultCache;

@end