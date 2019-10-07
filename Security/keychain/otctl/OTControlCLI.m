

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecItemPriv.h>
#import <Security/Security.h>
#import <xpc/xpc.h>

#include "utilities/SecCFWrappers.h"
#include "utilities/SecInternalReleasePriv.h"
#import "utilities/debugging.h"

#import "keychain/ot/OT.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTControl.h"
#import "keychain/otctl/OTControlCLI.h"

// Mutual recursion to set up an object for jsonification
static NSDictionary* cleanDictionaryForJSON(NSDictionary* dict);

static id cleanObjectForJSON(id obj) {
    if(!obj) {
        return nil;
    }
    if([obj isKindOfClass:[NSError class]]) {
        NSError* obje = (NSError*) obj;
        NSMutableDictionary* newErrorDict = [@{@"code": @(obje.code), @"domain": obje.domain} mutableCopy];
        newErrorDict[@"userInfo"] = cleanDictionaryForJSON(obje.userInfo);
        return newErrorDict;
    } else if([NSJSONSerialization isValidJSONObject:obj]) {
        return obj;

    } else if ([obj isKindOfClass: [NSNumber class]]) {
        return obj;

    } else if([obj isKindOfClass: [NSData class]]) {
        NSData* dataObj = (NSData*)obj;
        return [dataObj base64EncodedStringWithOptions:0];

    } else if ([obj isKindOfClass: [NSDictionary class]]) {
        return cleanDictionaryForJSON((NSDictionary*) obj);

    } else if ([obj isKindOfClass: [NSArray class]]) {
        NSArray* arrayObj = (NSArray*)obj;
        NSMutableArray* cleanArray = [NSMutableArray arrayWithCapacity:arrayObj.count];

        for(id x in arrayObj) {
            [cleanArray addObject: cleanObjectForJSON(x)];
        }
        return cleanArray;

    } else {
        return [obj description];
    }
}

static NSDictionary* cleanDictionaryForJSON(NSDictionary* dict) {
    if(!dict) {
        return nil;
    }
    NSMutableDictionary* mutDict = [dict mutableCopy];
    for(id key in mutDict.allKeys) {
        id obj = mutDict[key];
        mutDict[key] = cleanObjectForJSON(obj);
    }
    return mutDict;
}

static void print_json(NSDictionary* dict)
{
    NSError* err;

    NSData* json = [NSJSONSerialization dataWithJSONObject:cleanDictionaryForJSON(dict)
                                                   options:(NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys)
                                                     error:&err];
    if(!json) {
        NSLog(@"error during JSONification: %@", err.localizedDescription);
    } else {
        printf("%s\n", [[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding] UTF8String]);
    }
}


@implementation OTControlCLI

- (instancetype)initWithOTControl:(OTControl*)control {
    if((self = [super init])) {
        _control = control;
    }

    return self;
}

- (long)startOctagonStateMachine:(NSString*)container context:(NSString*)contextID {
#if OCTAGON
    __block long ret = -1;

    [self.control startOctagonStateMachine:container
                                   context:contextID
                                     reply:^(NSError* _Nullable error) {
                                         if(error) {
                                             printf("Error starting state machine: %s\n", [[error description] UTF8String]);
                                             ret = -1;
                                         } else {
                                             printf("state machine started.\n");
                                         }
                                     }];

    return ret;
#else
    printf("Unimplemented.\n");
    return -1;
#endif
}

- (long)signIn:(NSString*)altDSID container:(NSString* _Nullable)container context:(NSString*)contextID {
#if OCTAGON
    __block long ret = -1;

    [self.control signIn:altDSID
               container:container
                 context:contextID
                   reply:^(NSError* _Nullable error) {
                       if(error) {
                           printf("Error signing in: %s\n", [[error description] UTF8String]);
                       } else {
                           printf("Sign in complete.\n");
                           ret = 0;
                       }
                   }];

    return ret;
#else
    printf("Unimplemented.\n");
    return -1;
#endif
}

- (long)signOut:(NSString* _Nullable)container context:(NSString*)contextID {
#if OCTAGON
    __block long ret = -1;
    [self.control signOut:container
                  context:contextID
                    reply:^(NSError* _Nullable error) {
                        if(error) {
                            printf("Error signing out: %s\n", [[error description] UTF8String]);
                        } else {
                            printf("Sign out complete.\n");
                            ret = 0;
                        }
                    }];
    return ret;
#else
    printf("Unimplemented.\n");
    return -1;
#endif
}

- (long)depart:(NSString* _Nullable)container context:(NSString*)contextID {
#if OCTAGON
    __block long ret = -1;

    [self.control leaveClique:container
                      context:contextID
                        reply:^(NSError* _Nullable error) {
                            if(error) {
                                printf("Error departing clique: %s\n", [[error description] UTF8String]);
                            } else {
                                printf("Departing clique completed.\n");
                                ret = 0;
                            }
                        }];
    return ret;
#else
    printf("Unimplemented.\n");
    return -1;
#endif
}

- (long)resetOctagon:(NSString*)container context:(NSString*)contextID altDSID:(NSString*)altDSID {
#if OCTAGON
    __block long ret = -1;

    [self.control resetAndEstablish:container
                            context:contextID
                            altDSID:altDSID
                              reply:^(NSError* _Nullable error) {
                                  if(error) {
                                      printf("Error resetting: %s\n", [[error description] UTF8String]);
                                  } else {
                                      printf("reset and establish:\n");
                                      ret = 0;
                                  }
                              }];

    return ret;
#else
    printf("Unimplemented.\n");
    return -1;
#endif
}

- (void)printPeer:(NSDictionary*)peerInformation prefix:(NSString* _Nullable)prefix {
    NSString* peerID = peerInformation[@"peerID"];
    NSString* model = peerInformation[@"permanentInfo"][@"model_id"];
    NSNumber* epoch = peerInformation[@"permanentInfo"][@"epoch"];
    NSString* deviceName = peerInformation[@"stableInfo"][@"device_name"];
    NSString* serialNumber = peerInformation[@"stableInfo"][@"serial_number"];
    NSString* os = peerInformation[@"stableInfo"][@"os_version"];

    printf("%s%s hw:'%s' name:'%s' serial: '%s' os:'%s' epoch:%d\n",
           (prefix ? [prefix UTF8String] : ""),
           [peerID UTF8String],
           [model UTF8String],
           [deviceName UTF8String],
           [serialNumber UTF8String],
           [os UTF8String],
           [epoch intValue]);
}

- (void)printPeers:(NSArray<NSString*>*)peerIDs
             egoPeerID:(NSString* _Nullable)egoPeerID
    informationOnPeers:(NSDictionary<NSString*, NSDictionary*>*)informationOnPeers {
    for(NSString* peerID in peerIDs) {
        NSDictionary* peerInformation = informationOnPeers[peerID];

        if(!peerInformation) {
            printf("    Peer:  %s; further information missing\n", [peerID UTF8String]);
            continue;
        }

        if([peerID isEqualToString:egoPeerID]) {
            [self printPeer:peerInformation prefix:@"    Self: "];
        } else {
            [self printPeer:peerInformation prefix:@"    Peer: "];
        }
    }
}

- (long)status:(NSString* _Nullable)container context:(NSString*)contextID json:(bool)json {
#if OCTAGON
    __block long ret = 0;

    [self.control status:container
                 context:contextID
                   reply:^(NSDictionary* result, NSError* _Nullable error) {
                       if(error) {
                           if(json) {
                               print_json(@{@"error" : [error description]});
                           } else {
                               printf("Error fetching status: %s\n", [[error description] UTF8String]);
                           }
                           ret = -1;
                       } else {
                           if(json) {
                               print_json(result);
                           } else {
                               printf("Status for %s,%s:\n", [result[@"containerName"] UTF8String], [result[@"contextID"] UTF8String]);

                               printf("\n");
                               printf("State: %s\n", [[result[@"state"] description] UTF8String]);
                               printf("Flags: %s; Flags Pending: %s\n\n",
                                      ([result[@"stateFlags"] count] == 0u) ? "none" : [[result[@"stateFlags"] description] UTF8String],
                                      ([result[@"statePendingFlags"] count] == 0u) ? "none" : [[result[@"statePendingFlags"] description] UTF8String]);

                               NSDictionary* contextDump = result[@"contextDump"];

                               // Make it easy to find peer information
                               NSMutableDictionary<NSString*, NSDictionary*>* peers = [NSMutableDictionary dictionary];
                               NSMutableArray<NSString*>* allPeerIDs = [NSMutableArray array];
                               for(NSDictionary* peerInformation in contextDump[@"peers"]) {
                                   NSString* peerID = peerInformation[@"peerID"];
                                   if(peerID) {
                                       peers[peerID] = peerInformation;
                                       [allPeerIDs addObject:peerID];
                                   }
                               }

                               NSDictionary* egoInformation = contextDump[@"self"];
                               NSString* egoPeerID = egoInformation[@"peerID"];
                               NSDictionary* egoDynamicInfo = egoInformation[@"dynamicInfo"];

                               if(egoPeerID) {
                                   NSMutableArray *otherPeers = [allPeerIDs mutableCopy];
                                   [self printPeer:egoInformation prefix:@"    Self: "];
                                   printf("\n");

                                   // The self peer is technically a peer, so, shove it on in there
                                   peers[egoPeerID] = egoInformation;

                                   NSArray<NSString*>* includedPeers = egoDynamicInfo[@"included"];
                                   printf("Trusted peers (by me):\n");
                                   if(includedPeers && includedPeers.count > 0) {
                                       [self printPeers:includedPeers egoPeerID:egoPeerID informationOnPeers:peers];
                                       [otherPeers removeObjectsInArray:includedPeers];
                                   } else {
                                       printf("    No trusted peers.\n");
                                   }
                                   printf("\n");

                                   NSArray<NSString*>* excludedPeers = egoDynamicInfo[@"excluded"];
                                   printf("Excluded peers (by me):\n");
                                   if(excludedPeers && excludedPeers.count > 0) {
                                       [self printPeers:excludedPeers egoPeerID:egoPeerID informationOnPeers:peers];
                                       [otherPeers removeObjectsInArray:excludedPeers];
                                   } else {
                                       printf("    No excluded peers.\n");
                                   }
                                   printf("\n");

                                   printf("Other peers (included/excluded by others):\n");
                                   if(otherPeers.count > 0) {
                                       [self printPeers:otherPeers egoPeerID:egoPeerID informationOnPeers:peers];
                                   } else {
                                       printf("    No other peers.\n");
                                   }
                                   printf("\n");

                               } else {
                                   printf("No current identity for this device.\n\n");

                                   if(allPeerIDs.count > 0) {
                                       printf("All peers currently in this account:\n");
                                       [self printPeers:allPeerIDs egoPeerID:nil informationOnPeers:peers];
                                   } else {
                                       printf("No peers currently exist for this account.\n");
                                   }
                               }

                               printf("\n");
                           }
                       }
                   }];

    return ret;
#else
    printf("Unimplemented.\n");
    return -1;
#endif
}

- (long)recoverUsingBottleID:(NSString*)bottleID
                     entropy:(NSData*)entropy
                     altDSID:(NSString*)altDSID
               containerName:(NSString*)containerName
                     context:(NSString*)context
                     control:(OTControl*)control {
    __block long ret = 0;

#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [control restore:containerName
           contextID:context
          bottleSalt:altDSID
             entropy:entropy
            bottleID:bottleID
               reply:^(NSError* _Nullable error) {
                   if(error) {
                       ret = -1;
                       printf("Error recovering: %s\n", [[error description] UTF8String]);
                   } else {
                       printf("Succeeded recovering bottled peer %s\n", [[bottleID description] UTF8String]);
                   }
                   dispatch_semaphore_signal(sema);
               }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        printf("timed out waiting for restore/recover\n");
        ret = -1;
    }

    return ret;
#else
    ret = -1;
    return ret;
#endif
}

- (long)fetchAllBottles:(NSString*)altDSID
          containerName:(NSString*)containerName
                context:(NSString*)context
                control:(OTControl*)control {
    __block long ret = 0;

#if OCTAGON
    __block NSError* localError = nil;

    __block NSArray<NSString*>* localViableBottleIDs = nil;
    __block NSArray<NSString*>* localPartiallyViableBottleIDs = nil;

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [control fetchAllViableBottles:containerName
                           context:context
                             reply:^(NSArray<NSString*>* _Nullable sortedBottleIDs,
                                     NSArray<NSString*>* _Nullable sortedPartialBottleIDs,
                                     NSError* _Nullable controlError) {
                                 if(controlError) {
                                     secnotice("clique", "findOptimalBottleIDsWithContextData errored: %@\n", controlError);
                                 } else {
                                     secnotice("clique", "findOptimalBottleIDsWithContextData succeeded: %@, %@\n", sortedBottleIDs, sortedPartialBottleIDs);
                                 }
                                 localError = controlError;
                                 localViableBottleIDs = sortedBottleIDs;
                                 localPartiallyViableBottleIDs = sortedPartialBottleIDs;
                                 dispatch_semaphore_signal(sema);
                             }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        secnotice("clique", "findOptimalBottleIDsWithContextData failed to fetch bottles\n");
        return -1;
    }

    [localViableBottleIDs enumerateObjectsUsingBlock:^(NSString* obj, NSUInteger idx, BOOL* stop) {
        printf("preferred bottleID: %s\n", [obj UTF8String]);
    }];

    [localPartiallyViableBottleIDs enumerateObjectsUsingBlock:^(NSString* obj, NSUInteger idx, BOOL* stop) {
        printf("partial recovery bottleID: %s\n", [obj UTF8String]);
    }];

    return ret;
#else
    ret = -1;
    return ret;
#endif
}

- (long)healthCheck:(NSString* _Nullable)container context:(NSString*)contextID skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
{
#if OCTAGON
    __block long ret = -1;

    [self.control healthCheck:container
                      context:contextID
        skipRateLimitingCheck:skipRateLimitingCheck
                        reply:^(NSError* _Nullable error) {
                            if(error) {
                                printf("Error checking health: %s\n", [[error description] UTF8String]);
                            } else {
                                printf("Checking Octagon Health completed.\n");
                                ret = 0;
                            }
                        }];
    return ret;
#else
    printf("Unimplemented.\n");
    return -1;
#endif
}

- (long)tapToRadar:(NSString *)action description:(NSString *)description radar:(NSString *)radar
{
#if OCTAGON
    __block long ret = 1;

    [self.control tapToRadar:action
                 description:description
                       radar:radar
                       reply:^(NSError* _Nullable error) {
        if(error) {
            printf("Error trigger TTR: %s\n", [[error description] UTF8String]);
        } else {
            printf("Trigger TTR completed.\n");
            ret = 0;
        }
    }];
    return ret;
#else
    printf("Unimplemented.\n");
    return 1;
#endif
}

@end
