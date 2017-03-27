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
#import <Security/Security.h>

#import <CKBridge/SOSCloudKeychainClient.h>

#import <dispatch/dispatch.h>

#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>

#import <Security/SecureObjectSync/SOSInternal.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>

#include "secToolFileIO.h"
#include "accountCirclesViewsPrint.h"


#include <stdio.h>

@interface NSString (FileOutput)
- (void) writeTo: (FILE*) file;
- (void) writeToStdOut;
- (void) writeToStdErr;
@end

@implementation NSString (FileOutput)

- (void) writeTo: (FILE*) file {
    CFStringPerformWithCString((__bridge CFStringRef) self, ^(const char *utf8String) { fputs(utf8String, file); });
}

- (void) writeToStdOut {
    [self writeTo: stdout];
}
- (void) writeToStdErr {
    [self writeTo: stderr];
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


static void
circle_sysdiagnose(void)
{
    SOSLogSetOutputTo(NULL,NULL);
    SOSCCDumpCircleInformation();
}

static void
engine_sysdiagnose(void)
{
    [@"Engine state:\n" writeToStdOut];

    CFErrorRef error = NULL;

    if (!SOSCCForEachEngineStateAsString(&error, ^(CFStringRef oneStateString) {
        [(__bridge NSString*) oneStateString writeToStdOut];
        [@"\n" writeToStdOut];
    })) {
        [[NSString stringWithFormat: @"No engine state, got error: %@", error] writeToStdOut];
    }
}

static void
homekit_sysdiagnose(void)
{
}

static void
unlock_sysdiagnose(void)
{
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
        // Keep this one last
        kvs_sysdiagnose();
    }
    return 0;
}
