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

#import "keychain/ckks/CKKSControlProtocol.h"

#if OCTAGON
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <objc/runtime.h>
#import "utilities/debugging.h"
#include <dlfcn.h>
#import <Security/SecXPCHelper.h>
#import <Security/CKKSExternalTLKClient.h>

// Weak-link CloudKit, until we can get ckksctl out of base system
static void *cloudKit = NULL;

static void
initCloudKit(void)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cloudKit = dlopen("/System/Library/Frameworks/CloudKit.framework/CloudKit", RTLD_LAZY);
    });
}

static void
getCloudKitSymbol(void **sym, const char *name)
{
    initCloudKit();
    if (!sym || *sym) {
        return;
    }
    *sym = dlsym(cloudKit, name);
    if (*sym == NULL) {
        fprintf(stderr, "symbol %s is missing", name);
        abort();
    }
}
#endif // OCTAGON

NSXPCInterface* CKKSSetupControlProtocol(NSXPCInterface* interface) {
#if OCTAGON
    static NSMutableSet *errClasses;

    static NSSet* tlkShareArrayClasses;
    static NSSet* tlkArrayClasses;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        __typeof(CKAcceptableValueClasses) *soft_CKAcceptableValueClasses = NULL;
        getCloudKitSymbol((void **)&soft_CKAcceptableValueClasses, "CKAcceptableValueClasses");
        errClasses = [NSMutableSet setWithSet:soft_CKAcceptableValueClasses()];
        [errClasses unionSet:[SecXPCHelper safeErrorClasses]];

        tlkArrayClasses = [NSSet setWithArray:@[[NSArray class], [CKKSExternalKey class]]];
        tlkShareArrayClasses = [NSSet setWithArray:@[[NSArray class], [CKKSExternalTLKShare class]]];
    });

    @try {
        [interface setClasses:errClasses forSelector:@selector(rpcResetLocal:reply:)                   argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcResetCloudKit:reason:reply:)         argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcResync:reply:)                       argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcResyncLocal:reply:)                  argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcStatus:fast:waitForNonTransientState:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcFetchAndProcessChanges:classA:onlyIfNoRecentFetch:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcPushOutgoingChanges:reply:)          argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcGetCKDeviceIDWithReply:)             argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(rpcCKMetric:attributes:reply:)          argumentIndex:0 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(proposeTLKForSEView:proposedTLK:wrappedOldTLK:tlkShares:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchSEViewKeyHierarchy:forceFetch:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(modifyTLKSharesForSEView:adding:deleting:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(deleteSEView:reply:) argumentIndex:0 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(pcsMirrorKeysForServices:reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:tlkShareArrayClasses forSelector:@selector(proposeTLKForSEView:proposedTLK:wrappedOldTLK:tlkShares:reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:tlkArrayClasses      forSelector:@selector(fetchSEViewKeyHierarchy:forceFetch:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:tlkShareArrayClasses forSelector:@selector(fetchSEViewKeyHierarchy:forceFetch:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:tlkShareArrayClasses forSelector:@selector(modifyTLKSharesForSEView:adding:deleting:reply:) argumentIndex:1 ofReply:NO];
        [interface setClasses:tlkShareArrayClasses forSelector:@selector(modifyTLKSharesForSEView:adding:deleting:reply:) argumentIndex:2 ofReply:NO];
    }

    @catch(NSException* e) {
        secerror("CKKSSetupControlProtocol failed, continuing, but you might crash later: %@", e);
        @throw e;
    }
#endif

    return interface;
}

