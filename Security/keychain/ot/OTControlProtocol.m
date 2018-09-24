/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Foundation/Foundation.h>

#import "keychain/ot/OTControlProtocol.h"

#if OCTAGON
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <utilities/debugging.h>
#include <dlfcn.h>
#import <SecurityFoundation/SFKey.h>
#endif // OCTAGON

NSXPCInterface* OTSetupControlProtocol(NSXPCInterface* interface) {
#if OCTAGON
    static NSMutableSet *errClasses;
    
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        errClasses = [NSMutableSet set];
        char *classes[] = {
            "NSArray",
            "NSData",
            "NSDate",
            "NSDictionary",
            "NSError",
            "NSNull",
            "NSNumber",
            "NSOrderedSet",
            "NSSet",
            "NSString",
            "NSURL",
        };
        
        for (unsigned n = 0; n < sizeof(classes)/sizeof(classes[0]); n++) {
            Class cls = objc_getClass(classes[n]);
            if (cls) {
                [errClasses addObject:cls];
            }
        }
    });
    
    @try {
        [interface setClasses:errClasses forSelector:@selector(restore:dsid:secret:escrowRecordID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(octagonEncryptionPublicKey:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(octagonSigningPublicKey:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(listOfEligibleBottledPeerRecords:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(signOut:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(signIn:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(reset:) argumentIndex:0 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(preflightBottledPeer:dsid:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(launchBottledPeer:bottleID:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(scrubBottledPeer:bottleID:reply:) argumentIndex:0 ofReply:YES];
    }
    @catch(NSException* e) {
        secerror("OTSetupControlProtocol failed, continuing, but you might crash later: %@", e);
#if DEBUG
        @throw e;
#endif
    }
#endif
    
    return interface;
}


