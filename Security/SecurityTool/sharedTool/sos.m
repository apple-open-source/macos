/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

/*
 * This is to fool os services to not provide the Keychain manager
 * interface that doesn't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <Security/SecureObjectSync/SOSTypes.h>
#include "keychain/SecureObjectSync/SOSControlHelper.h"
#import <ipc/securityd_client.h>
#import <err.h>
#import <getopt.h>

#include "builtin_commands.h"


@interface SOSStatus : NSObject
@property NSXPCConnection *connection;

- (void)printPerformanceCounters:(bool)asPList;
@end

@implementation SOSStatus

- (instancetype) init
{
    if ((self = [super init]) == NULL)
        return NULL;

    NSXPCInterface *interface = [NSXPCInterface interfaceWithProtocol:@protocol(SOSControlProtocol)];
    _SOSControlSetupInterface(interface);

    self.connection = [[NSXPCConnection alloc] initWithMachServiceName:@(kSecuritydSOSServiceName) options:0];
    if (self.connection == NULL){
        return NULL;
    }
    self.connection.remoteObjectInterface = interface;

    [self.connection resume];


    return self;
}

- (void)printPerformanceCounters:(bool)asPList
{
    dispatch_semaphore_t sema1 = dispatch_semaphore_create(0);
    dispatch_semaphore_t sema2 = dispatch_semaphore_create(0);
    dispatch_semaphore_t sema3 = dispatch_semaphore_create(0);
    
    NSMutableDictionary<NSString *, NSNumber *> *merged = [NSMutableDictionary dictionary];
    __block NSMutableDictionary<NSString *, NSString *> *diagnostics = [NSMutableDictionary dictionary];
    
    [[self.connection remoteObjectProxy] kvsPerformanceCounters:^(NSDictionary <NSString *, NSNumber *> *counters){
        if (counters == NULL){
            printf("no KVS counters!");
            return;
        }
       
        [merged addEntriesFromDictionary:counters];
        dispatch_semaphore_signal(sema1);
    }];
    
    [[self.connection remoteObjectProxy] rateLimitingPerformanceCounters:^(NSDictionary <NSString *, NSString *> *returnedDiagnostics){
        if (returnedDiagnostics == NULL){
            printf("no rate limiting counters!");
            return;
        }
        diagnostics = [[NSMutableDictionary alloc]initWithDictionary:returnedDiagnostics];
        dispatch_semaphore_signal(sema3);
    }];
    dispatch_semaphore_wait(sema1, DISPATCH_TIME_FOREVER);
    dispatch_semaphore_wait(sema2, DISPATCH_TIME_FOREVER);
    dispatch_semaphore_wait(sema3, DISPATCH_TIME_FOREVER);
    
    if (asPList) {
        NSData *data = [NSPropertyListSerialization dataWithPropertyList:merged format:NSPropertyListXMLFormat_v1_0 options:0 error:NULL];

        printf("%.*s\n", (int)[data length], [data bytes]);
    } else {
        [merged enumerateKeysAndObjectsUsingBlock:^(NSString * key, NSNumber * obj, BOOL *stop) {
            printf("%s - %lld\n", [key UTF8String], [obj longLongValue]);
        }];
    }
    if(diagnostics){
        [diagnostics enumerateKeysAndObjectsUsingBlock:^(NSString * key, NSString * obj, BOOL * stop) {
            printf("%s - %s\n", [key UTF8String], [obj UTF8String]);
        }];
    }
}

@end

static void
usage(const char *command, struct option *options)
{
    printf("%s %s [...options]\n", getprogname(), command);
    for (unsigned n = 0; options[n].name; n++) {
        printf("\t [-%c|--%s\n", options[n].val, options[n].name);
    }

}

int
command_sos_stats(__unused int argc, __unused char * const * argv)
{
    @autoreleasepool {
        int option_index = 0, ch;

        SOSStatus *control = [[SOSStatus alloc] init];

        bool asPlist = false;
        struct option long_options[] =
        {
            /* These options set a flag. */
            {"plist",    no_argument, NULL, 'p'},
            {0, 0, 0, 0}
        };

        while ((ch = getopt_long(argc, argv, "p", long_options, &option_index)) != -1) {
            switch  (ch) {
                case 'p': {
                    asPlist = true;
                    break;
                }
                case '?':
                default:
                {
                    usage("sos-stats", long_options);
                    return SHOW_USAGE_MESSAGE;
                }
            }
        }

        [control printPerformanceCounters:asPlist];
    }

    return 0;
}

static void printControlFailureMessage(NSError *error) {
    printf("%s", [[NSString stringWithFormat:@"Failed to send messages to soscontrol object: %@\n", error] UTF8String]);
}

int
command_sos_control(__unused int argc, __unused char * const * argv)
{
    @autoreleasepool {
        int option_index = 0, ch;

        bool assertStashAccountKey = false;
        bool triggerSync = false;
        NSMutableArray<NSString *>* syncingPeers = NULL;
        bool triggerBackup = false;
        NSMutableArray<NSString*>* backupPeers = NULL;
        SOSAccountGhostBustingOptions gboptions = 0;
        bool gbinfo = false;
        bool gbtriggered = false;
        bool circleHash = false;
        bool triggerRingUpdate = false;
        bool iCloudIdentityStatus = false;
        bool removeV0Peers = false;

        static struct option long_options[] =
        {
            /* These options set a flag. */
            {"assertStashAccountKey",   no_argument, NULL, 'a'},
            {"trigger-backup",   optional_argument, NULL, 'B'},
            {"trigger-ring-update",   no_argument, NULL, 'R'},
            {"trigger-sync",   optional_argument, NULL, 's'},
            {"circle-hash", optional_argument, NULL, 'H'},
            {"ghostbustByMID",   optional_argument, NULL, 'M'},
            {"ghostbustBySerial",   optional_argument, NULL, 'S'},
            {"ghostbustiCloudIdentities",   optional_argument, NULL, 'I'},
            {"ghostbustByAge",   optional_argument, NULL, 'A'},
            {"ghostbustInfo",   optional_argument, NULL, 'G'},
            {"ghostbustTriggered",   optional_argument, NULL, 'T'},
            {"icloudIdentityStatus",   optional_argument, NULL, 'i'},
            {"removeV0Peers",   optional_argument, NULL, 'V'},
            {0, 0, 0, 0}
        };

        while ((ch = getopt_long(argc, argv, "as:AB:GHIMRSTiV", long_options, &option_index)) != -1) {
            switch  (ch) {
                case 'a': {
                    assertStashAccountKey = true;
                    break;
                }
                case 's': {
                    triggerSync = true;
                    if (optarg) {
                        if (syncingPeers == NULL) {
                            syncingPeers = [NSMutableArray array];
                        }
                        [syncingPeers addObject:[NSString stringWithUTF8String:optarg]];
                    }
                    break;
                }
                case 'A': {
                    gboptions |= SOSGhostBustSerialByAge;
                    break;
                }
                case 'B': {
                    triggerBackup = true;
                    if (optarg) {
                        if (backupPeers == NULL) {
                            backupPeers = [NSMutableArray array];
                        }
                        [backupPeers addObject:[NSString stringWithUTF8String:optarg]];
                    }
                    break;
                }
                case 'G': {
                    gbinfo = true;
                    break;
                }
                case 'M': {
                    gboptions |= SOSGhostBustByMID;
                    break;
                }
                case 'R':
                    triggerRingUpdate = true;
                    break;
                case 'S': {
                    gboptions |= SOSGhostBustBySerialNumber;
                    break;
                }
                case 'I': {
                    gboptions |= SOSGhostBustiCloudIdentities;
                    break;
                }
                case 'T': {
                    gbtriggered = true;
                    break;
                }
                case 'H':
                    circleHash = true;
                    break;
                case 'i':
                    iCloudIdentityStatus = true;
                    break;
                case 'V':
                    removeV0Peers = true;
                    break;
                case '?':
                default:
                {
                    usage("sos-control", long_options);
                    return SHOW_USAGE_MESSAGE;
                }
            }
        }
        
        SOSStatus *control = [[SOSStatus alloc] init];
        if (control == NULL)
            errx(1, "no SOS control object");

        if( gbtriggered) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] ghostBustTriggerTimed:gboptions complete:^(bool ghostBusted, NSError *error) {
                if (ghostBusted) {
                    printf("ghostbusted devices\n");
                } else if (error) {
                    printf("%s", [[NSString stringWithFormat:@"Failed ghostbusting: %@\n", error] UTF8String]);
                } else {
                    printf("no ghosts found\n");
                }
            }];
        } else if( gbinfo) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] ghostBustInfo: ^(NSData *json, NSError *error) {
                printf("%s\n", [[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding] UTF8String]);
            }];
        } else if (gboptions != 0) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] ghostBust:gboptions complete:^(bool ghostBusted, NSError *error) {
                if (ghostBusted) {
                    printf("ghostbusted devices\n");
                } else if (error) {
                    printf("%s", [[NSString stringWithFormat:@"Failed ghostbusting: %@\n", error] UTF8String]);
                } else {
                    printf("no ghosts found\n");
                }
            }];
        } else if (triggerSync) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] rpcTriggerSync:syncingPeers complete:^(bool res, NSError *error) {
                if (res) {
                    printf("starting to sync was successful\n");
                } else {
                    printf("%s", [[NSString stringWithFormat:@"Failed to start sync: %@\n", error] UTF8String]);
                }
            }];
        } else if (triggerBackup) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] rpcTriggerBackup:backupPeers complete:^(NSError *error) {
                if (error == NULL) {
                    printf("trigger backup was successful\n");
                } else {
                    printf("%s", [[NSString stringWithFormat:@"Failed to start backup: %@\n", error] UTF8String]);
                }
            }];
        } else if (triggerRingUpdate) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] rpcTriggerRingUpdate:^(NSError *error) {
                if (error == NULL) {
                    printf("trigger ring update was successful\n");
                } else {
                    printf("%s", [[NSString stringWithFormat:@"Failed to start ring update: %@\n", error] UTF8String]);
                }
            }];

        } else if (assertStashAccountKey) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] assertStashedAccountCredential:^(BOOL res, NSError *error) {
                if (res) {
                    printf("successfully asserted stashed credential\n");
                } else {
                    printf("%s", [[NSString stringWithFormat:@"failed to assert stashed credential: %@\n", error] UTF8String]);
                }
            }];
        } else if (circleHash) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] circleHash:^(NSString *hash, NSError *error) {
                if (hash) {
                    printf("%s", [[NSString stringWithFormat:@"circle hash: %@\n", hash] UTF8String]);
                } else {
                    printf("%s", [[NSString stringWithFormat:@"failed to get circle digest: %@\n", error] UTF8String]);
                }
            }];

        } else if (iCloudIdentityStatus) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] iCloudIdentityStatus:^(NSData *json, NSError *error) {
                if (json) {
                    printf("iCloudIdentityStatus:\n%s\n", [[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding] UTF8String]);
                } else {
                    printf("%s", [[NSString stringWithFormat:@"failed to get iCloudIdentityStatus: %@\n", error] UTF8String]);
                }
            }];

        } else if (removeV0Peers) {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] removeV0Peers:^(bool removedV0Peer, NSError *error) {
                printf("removed v0 peers:%d\n", removedV0Peer);
                if (error) {
                    printf("%s", [[NSString stringWithFormat:@"failed to remove V0 Peers: %@\n", error] UTF8String]);
                }
            }];
        } else {
            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] userPublicKey:^(BOOL trusted, NSData *spki, NSError *error) {
                printf("trusted: %s\n", trusted ? "yes" : "no");
                printf("userPublicKey: %s\n", [[spki base64EncodedStringWithOptions:0] UTF8String]);
            }];

            [[control.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *error) {
                printControlFailureMessage(error);
            }] stashedCredentialPublicKey:^(NSData *spki, NSError *error) {
                NSString *pkey = [spki base64EncodedStringWithOptions:0];
                if (pkey == NULL)
                    pkey = @"not available";
                printf("cachedCredentialPublicKey: %s\n", [pkey UTF8String]);
            }];
        }

    }
    return 0;
}

int command_watchdog(int argc, char* const * argv)
{
    SOSStatus* control = [[SOSStatus alloc] init];
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    if (argc < 3) {
        printf("getting watchdog parameters...\n");
        [[control.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
            printControlFailureMessage(error);
            dispatch_semaphore_signal(semaphore);
        }] getWatchdogParameters:^(NSDictionary* parameters, NSError* error) {
            if (error) {
                printf("error getting watchdog parameters: %s", error.localizedDescription.UTF8String);
            }
            else {
                printf("watchdog parameters:\n");
                [parameters enumerateKeysAndObjectsUsingBlock:^(NSString* key, NSObject* value, BOOL* stop) {
                    printf("\t%s - %s\n", key.description.UTF8String, value.description.UTF8String);
                }];
            }

            dispatch_semaphore_signal(semaphore);
        }];
    }
    else {
        printf("attempting to set watchdog parameters...\n");
        NSString* parameter = [[NSString alloc] initWithUTF8String:argv[1]];
        NSInteger value = [[[NSString alloc] initWithUTF8String:argv[2]] integerValue];
        [[control.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
            printControlFailureMessage(error);
            dispatch_semaphore_signal(semaphore);
        }] setWatchdogParmeters:@{parameter : @(value)} complete:^(NSError* error) {
            if (error) {
                printf("error attempting to set watchdog parameters: %s\n", error.localizedDescription.UTF8String);
            }
            else {
                printf("successfully set watchdog parameter\n");
            }

            dispatch_semaphore_signal(semaphore);
        }];
    }

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

    return 0;
}
