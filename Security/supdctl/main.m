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
#include "lib/SecArgParse.h"
#import "supd/supdProtocol.h"
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SFAnalytics.h>

/* Internal Topic Names */
NSString* const SFAnalyticsTopicKeySync = @"KeySyncTopic";
NSString* const SFAnaltyicsTopicTrust = @"TrustTopic";

static void nsprintf(NSString *fmt, ...) NS_FORMAT_FUNCTION(1, 2);
static void nsprintf(NSString *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    NSString *str = [[NSString alloc] initWithFormat:fmt arguments:ap];
    va_end(ap);

    puts([str UTF8String]);
#if !__has_feature(objc_arc)
    [str release];
#endif
}

static NSXPCConnection* getConnection()
{
    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithMachServiceName:@"com.apple.securityuploadd" options:0];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(supdProtocol)];
    [connection resume];
    return connection;
}

static void getSysdiagnoseDump(void)
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    NSXPCConnection* connection = getConnection();
    [[connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        nsprintf(@"Could not communicate with supd: %@", error);
        dispatch_semaphore_signal(sema);
    }] getSysdiagnoseDumpWithReply:^(NSString * sysdiagnoseString) {
        nsprintf(@"Analytics sysdiagnose: \n%@", sysdiagnoseString);
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 20)) != 0) {
        printf("\n\nError: timed out waiting for response from supd\n");
    }
    [connection invalidate];
}

static void getLoggingJSON(char *topicName)
{
    NSString *topic = topicName ? [NSString stringWithUTF8String:topicName] : SFAnalyticsTopicKeySync;
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    NSXPCConnection* connection = getConnection();
    [[connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        nsprintf(@"Could not communicate with supd: %@", error);
        dispatch_semaphore_signal(sema);
    }] getLoggingJSON:YES topic:topic reply:^(NSData* data, NSError* error) {
        if (data) {
            // Success! Only print the JSON blob to make output easier to parse
            nsprintf(@"%@", [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
        } else {
            nsprintf(@"supd gave us an error: %@", error);
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 20)) != 0) {
        printf("\n\nError: timed out waiting for response from supd\n");
    }
    [connection invalidate];
}

static void forceUploadAnalytics(void)
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    NSXPCConnection* connection = getConnection();
    [[connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        nsprintf(@"Could not communicate with supd: %@", error);
        dispatch_semaphore_signal(sema);
    }] forceUploadWithReply:^(BOOL success, NSError *error) {
        if (success) {
            printf("Supd reports successful upload\n");
        } else {
            nsprintf(@"Supd reports failure: %@", error);
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 20)) != 0) {
        printf("\n\nError: timed out waiting for response from supd\n");
    }
    [connection invalidate];
}

static int forceUpload = false;
static int getJSON = false;
static int getSysdiagnose = false;
static char *topicName = nil;

int main(int argc, char **argv)
{
    static struct argument options[] = {
        { .shortname='t', .longname="topicName", .argument=&topicName, .description="Operate on a non-default topic"},

        { .command="sysdiagnose", .flag=&getSysdiagnose, .flagval=true, .description="Retrieve the current sysdiagnose dump for security analytics"},
        { .command="get", .flag=&getJSON, .flagval=true, .description="Get the JSON blob we would upload to the server if an upload were due"},
        { .command="upload", .flag=&forceUpload, .flagval=true, .description="Force an upload of analytics data to server (ignoring privacy settings)"},
        {}  // Need this!
    };

    static struct arguments args = {
        .programname="supdctl",
        .description="Control and report on security analytics",
        .arguments = options,
    };

    if(!options_parse(argc, argv, &args)) {
        printf("\n");
        print_usage(&args);
        return -1;
    }

    @autoreleasepool {
        if (forceUpload) {
            forceUploadAnalytics();
        } else if (getJSON) {
            getLoggingJSON(topicName);
        } else if (getSysdiagnose) {
            getSysdiagnoseDump();
        } else {
            print_usage(&args);
            return -1;
        }
    }
    return 0;
}

