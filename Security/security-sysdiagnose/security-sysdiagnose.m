/*
 * Copyright (c) 2009-2010,2012-2015 Apple Inc. All Rights Reserved.
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
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/Security.h>

#import <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#import <dispatch/dispatch.h>

#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>

#import <Security/SecureObjectSync/SOSInternal.h>
#import <Security/CKKSControlProtocol.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>

#include "secToolFileIO.h"
#include "accountCirclesViewsPrint.h"
#import "CKKSControlProtocol.h"
#import "SecItemPriv.h"
#import "supdProtocol.h"

#include <stdio.h>

@interface NSString (FileOutput)
- (void) writeToStdOut;
- (void) writeToStdErr;
@end

@implementation NSString (FileOutput)

- (void) writeToStdOut {
    fputs([self UTF8String], stdout);
}
- (void) writeToStdErr {
    fputs([self UTF8String], stderr);
}

@end

@interface NSData (Hexinization)

- (NSString*) asHexString;

@end

@implementation NSData (Hexinization)

- (NSString*) asHexString {
    return (__bridge_transfer NSString*) CFDataCopyHexString((__bridge CFDataRef)self);
}

@end

static NSString *dictionaryToString(NSDictionary *dict) {
    NSMutableString *result = [NSMutableString stringWithCapacity:0];
    [dict enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        [result appendFormat:@"%@=%@,", key, obj];
    }];
    return [result substringToIndex:result.length-(result.length>0)];
}

@implementation NSDictionary (OneLiner)

- (NSString*) asOneLineString {
    return dictionaryToString(self);
}

@end

static void
circle_sysdiagnose(void)
{
    SOSLogSetOutputTo(NULL,NULL);
    SOSCCDumpCircleInformation();
}

static void
engine_sysdiagnose(void)
{
    SOSCCDumpEngineInformation();
}

/*
    Here are the commands to dump out all keychain entries used by HomeKit:
        security item class=genp,sync=1,agrp=com.apple.hap.pairing;
        security item class=genp,sync=0,agrp=com.apple.hap.pairing;
        security item class=genp,sync=0,agrp=com.apple.hap.metadata
*/

static void printSecItems(NSString *subsystem, CFTypeRef result) {
    if (result) {
        if (CFGetTypeID(result) == CFArrayGetTypeID()) {
            NSArray *items = (__bridge NSArray *)(result);
            NSObject *item;
            for (item in items) {
                if ([item respondsToSelector:@selector(asOneLineString)]) {
                    [[NSString stringWithFormat: @"%@: %@\n", subsystem, [(NSMutableDictionary *)item asOneLineString]] writeToStdOut];
                }
            }
        } else {
            NSObject *item = (__bridge NSObject *)(result);
            if ([item respondsToSelector:@selector(asOneLineString)]) {
                [[NSString stringWithFormat: @"%@: %@\n", subsystem, [(NSMutableDictionary *)item asOneLineString]] writeToStdOut];
            }
        }
    }
}

static void
homekit_sysdiagnose(void)
{
    NSString *kAccessGroupHapPairing  = @"com.apple.hap.pairing";
    NSString *kAccessGroupHapMetadata = @"com.apple.hap.metadata";

    [@"HomeKit keychain state:\n" writeToStdOut];

    // First look for syncable hap.pairing items
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroupHapPairing,
        (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @NO,
    } mutableCopy];

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"HomeKit", result);
    }
    CFReleaseNull(result);

    // Now look for non-syncable hap.pairing items
    query[(id)kSecAttrSynchronizable] = @NO;
    status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"HomeKit", result);
    }
    CFReleaseNull(result);

    // Finally look for non-syncable hap.metadata items
    query[(id)kSecAttrAccessGroup] = kAccessGroupHapMetadata;
    status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"HomeKit", result);
    }
    CFReleaseNull(result);
}

static void
unlock_sysdiagnose(void)
{
    NSString *kAccessGroupAutoUnlock  = @"com.apple.continuity.unlock";

    [@"AutoUnlock keychain state:\n" writeToStdOut];

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroupAutoUnlock,
        (id)kSecAttrAccount : @"com.apple.continuity.auto-unlock.sync",
        (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @NO,
    };

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status == noErr) {
        printSecItems(@"AutoUnlock", result);
    }
    CFReleaseNull(result);
}

static void idsproxy_print_message(CFDictionaryRef messages)
{
    NSDictionary<NSString*, NSDictionary*> *idsMessages = (__bridge NSDictionary *)messages;

    printf("IDS messages in flight: %d\n", (int)[idsMessages count]);

    [idsMessages enumerateKeysAndObjectsUsingBlock:^(NSString*  _Nonnull identifier, NSDictionary*  _Nonnull messageDictionary, BOOL * _Nonnull stop) {
        printf("message identifier: %s\n", [identifier cStringUsingEncoding:NSUTF8StringEncoding]);

        NSDictionary *messageDataAndPeerID = [messageDictionary valueForKey:(__bridge NSString*)kIDSMessageToSendKey];
        [messageDataAndPeerID enumerateKeysAndObjectsUsingBlock:^(NSString*  _Nonnull peerID, NSData*  _Nonnull messageData, BOOL * _Nonnull stop1) {
            if(messageData)
                printf("size of message to recipient: %lu\n", (unsigned long)[messageData length]);
        }];

        NSString *deviceID = [messageDictionary valueForKey:(__bridge NSString*)kIDSMessageRecipientDeviceID];
        if(deviceID)
            printf("recipient device id: %s\n", [deviceID cStringUsingEncoding:NSUTF8StringEncoding]);

    }];
}

static void
idsproxy_sysdiagnose(void)
{

    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    __block CFDictionaryRef returned = NULL;

    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    SOSCloudKeychainRetrievePendingMessageFromProxy(processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef error) {
        secdebug("SOSCloudKeychainRetrievePendingMessageFromProxy", "returned: %@", returnedValues);
        CFRetainAssign(returned, returnedValues);
        dispatch_semaphore_signal(wait_for);
    });

    dispatch_semaphore_wait(wait_for, dispatch_time(DISPATCH_TIME_NOW, 2ull * NSEC_PER_SEC));
    secdebug("idsproxy sysdiagnose", "messages: %@", returned);

    idsproxy_print_message(returned);
}

static void
analytics_sysdiagnose(void)
{
    NSXPCConnection* xpcConnection = [[NSXPCConnection alloc] initWithMachServiceName:@"com.apple.securityuploadd" options:NSXPCConnectionPrivileged];
    if (!xpcConnection) {
        [@"failed to setup xpc connection for securityuploadd\n" writeToStdErr];
        return;
    }
    xpcConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(supdProtocol)];
    [xpcConnection resume];
    
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [[xpcConnection remoteObjectProxyWithErrorHandler:^(NSError* rpcError) {
        [[NSString stringWithFormat:@"Error talking with daemon: %@\n", rpcError] writeToStdErr];
        dispatch_semaphore_signal(semaphore);
    }] getSysdiagnoseDumpWithReply:^(NSString* sysdiagnose) {
        if (sysdiagnose) {
            [[NSString stringWithFormat:@"\nAnalytics sysdiagnose:\n\n%@\n", sysdiagnose] writeToStdOut];
        }
        dispatch_semaphore_signal(semaphore);
    }];
    
    if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        [@"\n\nError: timed out waiting for response\n" writeToStdErr];
    }
}

static void
kvs_sysdiagnose(void) {
    SOSLogSetOutputTo(NULL,NULL);
    SOSCCDumpCircleKVSInformation(NULL);
}

int
main(int argc, const char ** argv)
{
    @autoreleasepool {
        printf("sysdiagnose keychain\n");

        circle_sysdiagnose();
        engine_sysdiagnose();
        homekit_sysdiagnose();
        unlock_sysdiagnose();
        idsproxy_sysdiagnose();
        analytics_sysdiagnose();
        
        // Keep this one last
        kvs_sysdiagnose();
    }
    return 0;
}
