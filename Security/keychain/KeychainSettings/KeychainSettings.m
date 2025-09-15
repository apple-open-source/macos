//
//  KeychainSettings.m
//  Security
//

#import "KeychainSettings.h"
#import <Security/Security.h>
#import <Security/OTControl.h>
#import <Security/CKKSControl.h>
#import <os/log.h>
#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/AppleAccount_Private.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#import <AuthKit/AKDeviceListDeltaMessagePayload.h>
#import <Foundation/NSDistributedNotificationCenter.h>

#import <AppleAccount/ACAccount+AppleAccount.h>
#import <Accounts/ACAccountStore.h>
#import <UIKit/UIKit.h>

#import "utilities/debugging.h"

#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/SecAsyncPiper.h"


#import "keychain/ot/proto/generated_source/OTAccountSettings.h"
#import "keychain/ot/proto/generated_source/OTWalrus.h"
#import "keychain/ot/proto/generated_source/OTWebAccess.h"

@interface KeychainSettings ()
@property (strong) OTControl* control;
@property (strong) NSDictionary* status;
@property (strong) NSString *statusError;

+ (OTControl *)sharedOTControl;
+ (CKKSControl *)sharedCKKSControl;
@end

@implementation KeychainSettings

- (id)init {
    if ((self = [super init]) != nil) {
        [self updateCircleStatus];
    }
    return self;
}

+ (OTControl *)sharedOTControl
{
    static OTControl *control;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSError *error = nil;

        control = [OTControl controlObject:true error:&error];
        if(error || !control) {
            os_log(OS_LOG_DEFAULT, "no OTControl, failed: %@", error);
        }
    });
    return control;
}

+ (CKKSControl *)sharedCKKSControl
{
    static CKKSControl *control;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSError *error = nil;

        // Use a synchronous control object
        control = [CKKSControl CKKSControlObject:true error:&error];
        if(error || !control) {
            os_log(OS_LOG_DEFAULT, "no CKKSControl, failed: %@", error);
        }
    });
    return control;

}


- (void)updateCircleStatus
{
    NSError* piperError = nil;
    SecAsyncPiper* piper = [[SecAsyncPiper alloc] initWithError:&piperError];
    if (piperError) {
        @synchronized (self) {
            self.status = nil;
            self.statusError = [piperError description];
        }
        return;
    }

    [[KeychainSettings sharedOTControl] status:[[OTControlArguments alloc] init]
                                         xpcFd:[piper xpcFd]
                                         reply:^(NSDictionary* result, NSError* _Nullable error) {
        if (error) {
            @synchronized (self) {
                self.status = nil;
                self.statusError = [error description];
            }
        } else {
            NSError* internalError = nil;
            NSDictionary* contextDump = [piper dictWithError:&internalError];
            if (internalError) {
                os_log(OS_LOG_DEFAULT, "Error decoding TPH json dump: %s\n", [[internalError description] UTF8String]);
                @synchronized (self) {
                    self.status = nil;
                    self.statusError = [internalError description];
                }
            } else {
                NSMutableDictionary* modifiableResult = [NSMutableDictionary dictionaryWithDictionary:result];
                modifiableResult[@"contextDump"] = contextDump;
                @synchronized (self) {
                    self.status = modifiableResult;
                    self.statusError = nil;
                }
            }
        }
    }];
}

- (NSArray *)specifiers {
    if (!_specifiers) {
        _specifiers = [self loadSpecifiersFromPlistName:@"KeychainSettings" target:self];
    }
    return _specifiers;
}

- (NSString *)octagonStatusString:(NSString *)key
{
    __block id status = nil;
    @synchronized (self) {
        if (self.status) {
            id value = self.status[key];
            if ([value isKindOfClass:[NSString class]]) {
                status = value;
            } else if ([value isKindOfClass:[NSNumber class]]) {
                NSNumber *number = value;
                status = [number stringValue];
            } else {
                status = [key description];
            }
        }
        if (status == nil && self.statusError) {
            status = self.statusError;
        }
    }
    if (status == nil) {
        status = @"<unset>";
    }
    return status;

}

- (NSNumber *)octagonStatusNumber:(NSString *)key
{
    __block NSNumber *status = nil;
    @synchronized (self) {
        NSNumber *value = self.status[key];
        if ([value isKindOfClass:[NSNumber class]]) {
            status = value;
        }

    }
    return status;

}

- (NSString *)octagonStateMachine:(PSSpecifier *)specifier
{
    return [self octagonStatusString:@"state"];
}

- (NSString *)prettyifyProtobufString:(NSString *)string
{
    return [string.capitalizedString stringByReplacingOccurrencesOfString:@"_" withString:@" "];
}

- (NSString *)octagonTrustState:(PSSpecifier *)specifier
{
    return [self prettyifyProtobufString:OTAccountMetadataClassC_TrustStateAsString([[self octagonStatusNumber:@"memoizedTrustState"] intValue])];
}

- (NSString *)octagonAccountState:(PSSpecifier *)specifier
{
    return [self prettyifyProtobufString:OTAccountMetadataClassC_AccountStateAsString([[self octagonStatusNumber:@"memoizedAccountState"] intValue])];
}

- (NSString *)ckksAggregateStatus:(PSSpecifier *)specifier
{
    __block NSString *status = NULL;
    __block bool foundNonReady = false;
    __block bool foundOne = false;

    void (^replyBlock)(NSArray<NSDictionary *> * _Nullable result, NSError * _Nullable error) = ^(NSArray<NSDictionary *>* result, NSError* _Nullable error){
        if (error) {
            status = [NSString stringWithFormat:@"error: %@", error];
        }
        for(NSDictionary* view in result) {
            NSString* viewName = view[@"view"];
            if (viewName == NULL || [viewName isEqualToString:@"global"]) {
                return;
            }
            foundOne = true;
            NSString *viewStatus = view[@"keystate"];

            if (![viewStatus isKindOfClass:[NSString class]] ||
                !([viewStatus isEqualToString:@"ready"] || [viewStatus isEqualToString:@"readypendingunlock"])) {
                foundNonReady = true;
            }
        }
    };

    [[KeychainSettings sharedCKKSControl] rpcFastStatus:NULL reply: replyBlock];

    if (status) {
        /* something already provided status */
    } else if (foundNonReady) {
        status = @"not ready";
    } else if (foundOne) {
        status = @"ready";
    } else {
        status = @"no status";
    }

    return status;
}

- (NSString* _Nullable)primaryiCloudAccountAltDSID
{
    ACAccountStore *store = [ACAccountStore defaultStore];
    ACAccount* primaryAccount = [store aa_primaryAppleAccount];
    if(!primaryAccount) {
        return nil;
    }

    return [primaryAccount aa_altDSID];
}

- (void) resetOctagon:(PSSpecifier *)specifier
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    OTConfigurationContext* configurationContext = [[OTConfigurationContext alloc] init];

    OTControlArguments* arguments = [[OTControlArguments alloc] initWithConfiguration:configurationContext];
    __block OTAccountSettings* accountSettings = nil;
    __block NSError* fetchError = nil;
    [[KeychainSettings sharedOTControl] fetchAccountWideSettingsWithForceFetch:true arguments:arguments reply:^(OTAccountSettings * _Nullable retAccountSetting, NSError * _Nullable retError) {
        accountSettings = retAccountSetting;
        fetchError = retError;
    }];
    BOOL accountIsW = NO;
    if (accountSettings.hasWalrus) {
        accountIsW = accountSettings.walrus.enabled ? YES : NO;
    }

    arguments.altDSID = [self primaryiCloudAccountAltDSID];
    [[KeychainSettings sharedOTControl] resetAndEstablish:arguments
                                              resetReason:CuttlefishResetReasonUserInitiatedReset
                                        idmsTargetContext:nil
                                   idmsCuttlefishPassword:nil
                                               notifyIdMS:false
                                          accountSettings:nil
                                               accountIsW:accountIsW
                                                    reply:^(NSError * _Nullable error) {
                                                        if(error) {

                                                        }
                                                        dispatch_semaphore_signal(sema);

                                                    }];
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        secerror("timed out attempting to reset octagon");
    }
}

@end


@implementation KeychainSettingsOctagonPeers

- (void)addPeerIDs:(NSArray<NSString*>*)peerIDs
             peers:(NSMutableDictionary<NSString*, NSDictionary*>*)peers
      toSpecifiers:(NSMutableArray*)specifiers
{
    NSMutableArray<NSDictionary*>* selectedPeers = [NSMutableArray array];

    for (NSString *peerID in peerIDs) {
        NSDictionary *peer = peers[peerID];
        if (peer) {
            [selectedPeers addObject:peer];
        }
    }

    [selectedPeers sortUsingComparator:^NSComparisonResult(NSDictionary *_Nonnull obj1, NSDictionary *_Nonnull obj2) {
        return [obj1[@"stableInfo"][@"device_name"] compare: obj2[@"stableInfo"][@"device_name"]];
    }];

    for (NSDictionary *peer in selectedPeers) {
        PSSpecifier* spec = [PSSpecifier preferenceSpecifierNamed:peer[@"stableInfo"][@"device_name"] target:self set:nil get:nil detail:nil cell:PSTitleValueCell edit:nil];

        [specifiers addObject:spec];
    }

}

- (PSSpecifier *)groupSpecifier:(NSString *)name
{
    return [PSSpecifier preferenceSpecifierNamed:name target:self set:nil get:nil detail:nil cell:PSGroupCell edit:nil];
}

- (NSArray *)specifiers {

    if (!_specifiers) {
        NSMutableArray* specifiers = [NSMutableArray array];

        [specifiers addObjectsFromArray: [self loadSpecifiersFromPlistName:@"KeychainSettingsOctagonPeers" target:self]];

        NSError* piperError = nil;
        SecAsyncPiper* piper = [[SecAsyncPiper alloc] initWithError:&piperError];
        if (piperError) {
            os_log(OS_LOG_DEFAULT, "Error creating piper: %s\n", [[piperError description] UTF8String]);
            return nil;
        }

        void (^replyBlock)(NSDictionary* result, NSError* _Nullable error) = ^(NSDictionary* result, NSError* _Nullable error) {
            if (error) {
                os_log(OS_LOG_DEFAULT, "Error getting TPH status: %s\n", [[error description] UTF8String]);
                return;
            }

            NSError* internalError = nil;
            NSDictionary* contextDump = [piper dictWithError:&internalError];
            if (internalError) {
                os_log(OS_LOG_DEFAULT, "Error decoding TPH json dump: %s\n", [[internalError description] UTF8String]);
                return;
            }

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

            if(egoPeerID && egoInformation && egoDynamicInfo) {

                peers[egoPeerID] = egoInformation;

                [specifiers addObject:[self groupSpecifier:@"Me"]];
                [self addPeerIDs:@[egoPeerID] peers:peers toSpecifiers:specifiers];

                NSArray<NSString*>* included = egoDynamicInfo[@"included"];
                [specifiers addObject:[self groupSpecifier:@"Included"]];
                [self addPeerIDs:included peers:peers toSpecifiers:specifiers];
                [peers removeObjectsForKeys:included];

                NSArray<NSString*>* excluded = egoDynamicInfo[@"excluded"];
                [specifiers addObject:[self groupSpecifier:@"Excluded"]];
                [self addPeerIDs:excluded peers:peers toSpecifiers:specifiers];
                [peers removeObjectsForKeys:excluded];

            } else {
                [specifiers addObject:[self groupSpecifier:@"Me (untrusted)"]];
            }

            if (peers.count) {
                [specifiers addObject:[self groupSpecifier:@"Other peers"]];
                [self addPeerIDs:peers.allKeys peers:peers toSpecifiers:specifiers];
            }

        };

        [[KeychainSettings sharedOTControl] status:[[OTControlArguments alloc] init]
                                             xpcFd:[piper xpcFd]
                                             reply:replyBlock];

        _specifiers = specifiers;
    }

    return _specifiers;
}

@end

@implementation KeychainSettingsCKKSViews

- (NSArray *)specifiers {

    if (!_specifiers) {
        NSMutableArray* specifiers = [NSMutableArray array];

        [specifiers addObjectsFromArray: [self loadSpecifiersFromPlistName:@"KeychainSettingsCKKSViews" target:self]];

        void (^replyBlock)(NSArray<NSDictionary *> * _Nullable result, NSError * _Nullable error) = ^(NSArray<NSDictionary *>* result, NSError* _Nullable error){

            NSMutableArray<NSDictionary*>* views = [NSMutableArray array];
            for(NSDictionary* view in result) {
                NSString* viewName = view[@"view"];
                if (viewName == NULL || [viewName isEqualToString:@"global"]) {
                    return;
                }

                [views addObject:view];
            }

            [views sortUsingComparator:^NSComparisonResult(NSDictionary *_Nonnull obj1, NSDictionary *_Nonnull obj2) {
                return [obj1[@"view"] compare: obj2[@"view"]];
            }];

            for (NSDictionary *view in views) {
                NSString *description = [NSString stringWithFormat:@"%@ - %@", view[@"view"], view[@"keystate"]];
                PSSpecifier* spec = [PSSpecifier preferenceSpecifierNamed:description target:self set:nil get:nil detail:nil cell:PSTitleValueCell edit:nil];

                [specifiers addObject:spec];
            }
        };
        [[KeychainSettings sharedCKKSControl] rpcFastStatus:NULL reply: replyBlock];

        _specifiers = specifiers;
    }

    return _specifiers;
}



@end
