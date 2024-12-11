//
//  Security
//

#import "PairingChannel.h"
#import <Foundation/NSXPCConnection_Private.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFPropertyList_Private.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <Security/SecureObjectSync/SOSTypes.h>
#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>
#import "utilities/SecCoreAnalytics.h"
#import <ipc/securityd_client.h>
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OctagonControlServer.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OctagonControlServer.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import "keychain/ot/proto/generated_source/OTApplicantToSponsorRound2M1.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound2M2.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound1M2.h"
#import "keychain/ot/proto/generated_source/OTGlobalEnums.h"
#import "keychain/ot/proto/generated_source/OTSupportSOSMessage.h"
#import "keychain/ot/proto/generated_source/OTSupportOctagonMessage.h"
#import "keychain/ot/proto/generated_source/OTPairingMessage.h"
#import "keychain/SigninMetrics/OctagonSignPosts.h"
#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>
#import "keychain/categories/NSError+UsefulConstructors.h"
#import <CloudServices/SecureBackup.h>
#import <Accounts/Accounts.h>
#import <Accounts/Accounts_Private.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#include <utilities/SecAKSWrappers.h>
#include "keychain/securityd/SecKeybagSupport.h"

#import "utilities/SecTapToRadar.h"
#import <os/feature_private.h>

#import <SoftLinking/SoftLinking.h>

#import "MetricsOverrideForTests.h"

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, AAAFoundation);
SOFT_LINK_CLASS(AAAFoundation, AAFAnalyticsEvent);
SOFT_LINK_CONSTANT(AAAFoundation, kSecurityRTCEventCategoryAccountDataAccessRecovery, NSNumber*);


#include <notify.h>

#import <compression.h>
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#import <MobileGestalt.h>
#endif

KCPairingIntent_Type const KCPairingIntent_Type_None = @"none";
KCPairingIntent_Type const KCPairingIntent_Type_SilentRepair = @"repair";
KCPairingIntent_Type const KCPairingIntent_Type_UserDriven = @"userdriven";

KCPairingIntent_Capability const KCPairingIntent_Capability_FullPeer = @"full";
KCPairingIntent_Capability const KCPairingIntent_Capability_LimitedPeer = @"limited";

PairingPacketKey const PairingPacketKey_Device = @"x";
PairingPacketKey const PairingPacketKey_OctagonData = @"o";
PairingPacketKey const PairingPacketKey_PeerJoinBlob = @"p";
PairingPacketKey const PairingPacketKey_Credential = @"c";
PairingPacketKey const PairingPacketKey_CircleBlob = @"b";
PairingPacketKey const PairingPacketKey_InitialCredentialsFromAcceptor = @"d";
PairingPacketKey const PairingPacketKey_Version = @"v";
typedef NS_ENUM(int64_t, KCPairingSupportsTrustSystem) {
    KCPairingSupportsTrustSystemUnknown = 0,
    KCPairingSupportsTrustSystemSOS,
    KCPairingSupportsTrustSystemOctagon,
    KCPairingSupportsTrustSystemBoth
};

typedef void(^KCPairingInternalCompletion)(BOOL complete, NSDictionary *outdict, NSError *error);
typedef void(^KCNextState)(NSDictionary *indict, KCPairingInternalCompletion complete);

NSString *kKCPairingChannelErrorDomain = @"com.apple.security.kcparingchannel";

const char* pairingScope = "ot-pairing";
// constants capturing the max number of retries per pairing rpc
int epochMaxRetry = 3;
int prepareMaxRetry = 3;
int vouchMaxRetry = 3;
int joinMaxRetry = 3;


typedef void(^OTPairingInternalCompletion)(BOOL complete, NSData * _Nullable outData, NSError * _Nullable error);
typedef void(^OTNextState)(NSData *inData, OTPairingInternalCompletion complete);

@implementation KCPairingChannelContext

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (BOOL)isEqual:(id)object
{
    KCPairingChannelContext *other = (KCPairingChannelContext *)object;

    return [other isMemberOfClass:[self class]]
        && ((!self->_model && !other->_model) || [self->_model isEqual:other->_model])
        && ((!self->_modelVersion && !other->_modelVersion) || [self->_modelVersion isEqual:other->_modelVersion])
        && ((!self->_modelClass && !other->_modelClass) || [self->_modelClass isEqual:other->_modelClass])
        && ((!self->_osVersion && !other->_osVersion) || [self->_osVersion isEqual:other->_osVersion])
        && ((!self->_uniqueDeviceID && !other->_uniqueDeviceID) || [self->_uniqueDeviceID isEqual:other->_uniqueDeviceID])
        && ((!self->_altDSID && !other->_altDSID) || [self->_altDSID isEqual:other->_altDSID])
        && ((!self->_uniqueClientID && !other->_uniqueClientID) || [self->_uniqueClientID isEqual:other->_uniqueClientID])
        && ((!self->_intent && !other->_intent) || [self->_intent isEqual:other->_intent])
        && ((!self->_capability && !other->_capability) || [self->_capability isEqual:other->_capability])
        && ((!self->_flowID && !other->_flowID) || [self->_flowID isEqual:other->_flowID])
        && ((!self->_deviceSessionID && !other->_deviceSessionID) || [self->_flowID isEqual:other->_deviceSessionID])
    ;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_model forKey:@"model"];
    [coder encodeObject:_modelVersion forKey:@"modelVersion"];
    [coder encodeObject:_modelClass forKey:@"modelClass"];
    [coder encodeObject:_osVersion forKey:@"osVersion"];
    [coder encodeObject:_altDSID forKey:@"altDSID"];
    [coder encodeObject:_uniqueDeviceID forKey:@"uniqueDeviceID"];
    [coder encodeObject:_uniqueClientID forKey:@"uniqueClientID"];
    [coder encodeObject:_intent forKey:@"intent"];
    [coder encodeObject:_capability forKey:@"capability"];
    [coder encodeObject:_flowID forKey:@"flowID"];
    [coder encodeObject:_deviceSessionID forKey:@"deviceSessionID"];
}

- (nullable instancetype)initWithCoder:(NSCoder *)decoder
{
    if ((self = [super init])) {
        _model = [decoder decodeObjectOfClass:[NSString class] forKey:@"model"];
        _modelVersion = [decoder decodeObjectOfClass:[NSString class] forKey:@"modelVersion"];
        _modelClass = [decoder decodeObjectOfClass:[NSString class] forKey:@"modelClass"];
        _osVersion = [decoder decodeObjectOfClass:[NSString class] forKey:@"osVersion"];
        _altDSID = [decoder decodeObjectOfClass:[NSString class] forKey:@"altDSID"];
        _uniqueDeviceID = [decoder decodeObjectOfClass:[NSString class] forKey:@"uniqueDeviceID"];
        _uniqueClientID = [decoder decodeObjectOfClass:[NSString class] forKey:@"uniqueClientID"];
        _intent = [decoder decodeObjectOfClass:[NSString class] forKey:@"intent"];
        _capability = [decoder decodeObjectOfClass:[NSString class] forKey:@"capability"];
        _flowID = [decoder decodeObjectOfClass:[NSString class] forKey:@"flowID"];
        _deviceSessionID = [decoder decodeObjectOfClass:[NSString class] forKey:@"deviceSessionID"];

        /* validate intent if we have one */
        if (_intent != NULL &&
            !([_intent isEqualToString:KCPairingIntent_Type_None] ||
              [_intent isEqualToString:KCPairingIntent_Type_SilentRepair] ||
              [_intent isEqualToString:KCPairingIntent_Type_UserDriven]))
        {
            return nil;
        }
        /* validate device if we have one */
        if (_capability != NULL &&
            !(
              [_capability isEqualToString:KCPairingIntent_Capability_FullPeer] ||
              [_capability isEqualToString:KCPairingIntent_Capability_LimitedPeer]))
        {
            return nil;
        }
    }
    return self;
}

@end


@interface KCPairingChannel ()
@property (nonatomic, strong) KCPairingChannelContext *peerVersionContext;
@property (assign) bool initiator;
@property (assign) unsigned counter;
@property (assign) bool acceptorWillSendInitialSyncCredentials;
@property (assign) uint32_t acceptorInitialSyncCredentialsFlags;
@property (strong) NSXPCConnection *connection;
@property (strong) OTControl *otControl;
@property (strong) OTNextState nextOctagonState;
@property (strong) KCNextState nextState;
@property (nonatomic, strong) OTJoiningConfiguration* joiningConfiguration;
@property (nonatomic, strong) OTControlArguments* controlArguments;
@property (nonatomic) bool testFailSOS;
@property (nonatomic) bool testFailOctagon;
@property (nonatomic, copy, nullable) NSString* altDSID;

@property (assign) bool sessionSupportsSOS;
@property (assign) bool sessionSupportsOctagon;
@end


@implementation KCPairingChannel

+ (instancetype)pairingChannelInitiator:(KCPairingChannelContext *)peerVersionContext
{
    return [[KCPairingChannel alloc] initAsInitiator:true version:peerVersionContext];
}

+ (instancetype)pairingChannelAcceptor:(KCPairingChannelContext *)peerVersionContext
{
    return [[KCPairingChannel alloc] initAsInitiator:false version:peerVersionContext];
}

- (instancetype)initAsInitiator:(bool)initiator version:(KCPairingChannelContext *)peerVersionContext
{
    if (![KCPairingChannel isSupportedPlatform]) {
        secerror("platform not supported for pairing");
        return NULL;
    }

    if (self = [super init]) {
        __weak typeof(self) weakSelf = self;
        _initiator = initiator;
        _peerVersionContext = peerVersionContext;

        if (_initiator) {
            _nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf initiatorFirstPacket:nsdata complete:kscomplete];
            };
        } else {
            _nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf acceptorFirstPacket:nsdata complete:kscomplete];
            };
        }
        _needInitialSync = true;
        _testFailSOS = false;

        /* only apply to acceptor right now */
        _sessionSupportsSOS = SOSCCIsSOSTrustAndSyncingEnabled();
        _sessionSupportsOctagon = true;
        NSError *localError = nil;
        _otControl = [OTControl controlObject:true error:&localError];
        if(localError){
            secerror("could not stand up otcontrol connection");
        }
        _joiningConfiguration = [[OTJoiningConfiguration alloc]initWithProtocolType:OTProtocolPairing
                                                                     uniqueDeviceID:peerVersionContext.uniqueDeviceID
                                                                     uniqueClientID:peerVersionContext.uniqueClientID
                                                                        pairingUUID:[[NSUUID UUID] UUIDString]
                                                                              epoch:0
                                                                        isInitiator:initiator];
        _controlArguments = [[OTControlArguments alloc] initWithAltDSID:peerVersionContext.altDSID 
                                                                 flowID:peerVersionContext.flowID
                                                        deviceSessionID:peerVersionContext.deviceSessionID];
    }
    return self;
}

+(BOOL)_isRetryableNSURLError:(NSError*)error {

    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
            case NSURLErrorTimedOut:
            case NSURLErrorNotConnectedToInternet:
            case NSURLErrorNetworkConnectionLost:
                return true;
            default:
                return false;
        }
    }
    return false;
}

+ (bool)retryable:(NSError *_Nonnull)error
{
    return ([error.domain isEqualToString:NSCocoaErrorDomain] && error.code == NSXPCConnectionInterrupted) ||
           ([error.domain isEqualToString:OctagonErrorDomain] && error.code == OctagonErrorICloudAccountStateUnknown) ||
            [KCPairingChannel _isRetryableNSURLError: error] ||
            [KCPairingChannel _isRetryableNSURLError:error.userInfo[NSUnderlyingErrorKey]];
}

+ (bool)isSupportedPlatform
{
    return true;
}

- (void)oneStepTooMany:(NSDictionary * __unused)indata complete:(KCPairingInternalCompletion)complete
{
    secerror("pairingchannel: one step too many");
    complete(false, NULL, [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorTooManySteps userInfo:NULL]);
}

- (void)setNextStateError:(NSError *)error complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;
    self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
        [weakSelf oneStepTooMany:nsdata complete:kscomplete];
    };
    if (complete) {
        if (error)
            secerror("pairingchannel: failed pairing with: %@", error);
        complete(false, NULL, error);
    }
}

//MARK: - Compression

const compression_algorithm pairingCompression = COMPRESSION_LZFSE;
#define EXTRA_SIZE 100

+ (NSData *)pairingChannelCompressData:(NSData *)data
{
    static NSMutableData *scratch = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^ {
        scratch = [NSMutableData dataWithLength:compression_encode_scratch_buffer_size(pairingCompression)];
    });

    NSUInteger outLength = [data length];
    if (outLength > NSUIntegerMax - EXTRA_SIZE)
        return nil;
    outLength += EXTRA_SIZE;

    NSMutableData *o = [NSMutableData dataWithLength:outLength];
    size_t result = compression_encode_buffer([o mutableBytes], [o length], [data bytes], [data length], [scratch mutableBytes], pairingCompression);
    if (result == 0)
        return nil;

    [o setLength:result];

    return o;
}

+ (NSData *)pairingChannelDecompressData:(NSData *)data
{
    static NSMutableData *scratch = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^ {
        scratch = [NSMutableData dataWithLength:compression_decode_scratch_buffer_size(pairingCompression)];
    });

    size_t outLength = [data length];
    size_t result;
    NSMutableData *o = NULL;

    do {
        size_t size;
        if (__builtin_umull_overflow(outLength, 2, &size))
            return nil;
        outLength = size;
        o = [NSMutableData dataWithLength:outLength];

        result = compression_decode_buffer([o mutableBytes], outLength, [data bytes], [data length], [scratch mutableBytes], pairingCompression);
        if (result == 0)
            return nil;
    } while(result == outLength);

    [o setLength:result];

    return o;
}



//MARK: - Initiator

- (void)waitForOctagonUpgrade
{
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                   altDSID:self.peerVersionContext.altDSID
                                                                                                    flowID:self.peerVersionContext.flowID
                                                                                           deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                                 eventName:kSecurityRTCEventNameInitiatorWaitsForUpgrade
                                                                                           testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    [self.otControl waitForOctagonUpgrade:self.controlArguments reply:^(NSError *error) {
        if (error){
            secerror("pairing: failed to upgrade initiator into Octagon: %@", error);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
        } else {
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        }
    }];
}

- (void)initiatorFirstPacket:(NSDictionary * __unused)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator packet 1");
    
    bool subTaskSuccess = false;
    OctagonSignpost setupPairingChannelSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorMessage1);

    NSInteger trustSystem = KCPairingSupportsTrustSystemUnknown;

    if (self.sessionSupportsSOS && self.sessionSupportsOctagon) {
        trustSystem = KCPairingSupportsTrustSystemBoth;
    } else if (self.sessionSupportsOctagon) {
        trustSystem = KCPairingSupportsTrustSystemOctagon;
    } else if (self.sessionSupportsSOS) {
        trustSystem = KCPairingSupportsTrustSystemSOS;
    }

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:@{kSecurityRTCFieldSupportedTrustSystem:@(trustSystem)}
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameInitiatorCreatesPacket1
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    if (SOSCCIsSOSTrustAndSyncingEnabled() && ![self ensureControlChannel]) {
        NSError* localError = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoControlChannel userInfo:NULL];
        [self setNextStateError:localError complete:complete];
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage1), (int)subTaskSuccess);

        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        return;
    }

    if(self.sessionSupportsOctagon && self.sessionSupportsSOS && !self.testFailOctagon) {
        __weak typeof(self) weakSelf = self;
        self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
            [weakSelf initiatorSecondPacket:nsdata complete:kscomplete];
        };
        subTaskSuccess = true;
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage1), (int)subTaskSuccess);

        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];

        complete(false, @{ PairingPacketKey_InitialCredentialsFromAcceptor : @YES,
                           PairingPacketKey_OctagonData : @{PairingPacketKey_Version : @"O"}}, NULL);
        return;
    } else if (self.sessionSupportsOctagon && self.testFailOctagon) {
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage1), (int)subTaskSuccess);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        complete(true, nil, NULL);
        return;
    } else if (self.sessionSupportsOctagon && !self.sessionSupportsSOS) {
        __weak typeof(self) weakSelf = self;
        self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
            [weakSelf initiatorSecondPacket:nsdata complete:kscomplete];
        };
        
        subTaskSuccess = true;
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage1), (int)subTaskSuccess);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        complete(false, @{ PairingPacketKey_OctagonData : @{PairingPacketKey_Version : @"O"} }, NULL);
        return;
    }
    else {
        __weak typeof(self) weakSelf = self;
        self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
            [weakSelf initiatorSecondPacket:nsdata complete:kscomplete];
        };
        subTaskSuccess = true;
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage1), (int)subTaskSuccess);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        complete(false, @{ PairingPacketKey_InitialCredentialsFromAcceptor : @YES }, NULL);
    }
}

- (void)initiatorSecondPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator packet 2");

    __block bool subTaskSuccess = false;
    OctagonSignpost setupPairingChannelSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorMessage2);

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameInitiatorCreatesPacket3
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    NSData *octagonData = indata[PairingPacketKey_OctagonData];

    if(octagonData == nil) {
        secnotice("pairing", "acceptor didn't send a octagon packet, so skipping all octagon flows");
        self.sessionSupportsOctagon = false;
    }

    NSData *credential = indata[PairingPacketKey_Credential];

    if (SOSCCIsSOSTrustAndSyncingEnabled() && indata[PairingPacketKey_InitialCredentialsFromAcceptor]) {
        secnotice("pairing", "acceptor will send initial credentials");
        self.acceptorWillSendInitialSyncCredentials = true;
    }
    if(SOSCCIsSOSTrustAndSyncingEnabled()) {
        __block bool stashSuccess = false;
        OctagonSignpost stashSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorStashAccountCredential);
        
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            OctagonSignpostEnd(stashSignPost, OctagonSignpostNamePairingChannelInitiatorStashAccountCredential, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorStashAccountCredential), (int)stashSuccess);
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage2), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
            complete(true, NULL, error);
        }] stashAccountCredential:credential
         altDSID:self.peerVersionContext.altDSID
         flowID:self.peerVersionContext.flowID
         deviceSessionID:self.peerVersionContext.deviceSessionID
         canSendMetrics:YES
         complete:^(bool success, NSError *error) {
            if (success && error == nil) {
                stashSuccess = true;
            }
            OctagonSignpostEnd(stashSignPost, OctagonSignpostNamePairingChannelInitiatorStashAccountCredential, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorStashAccountCredential), (int)stashSuccess);

            [self setNextStateError:NULL complete:NULL];
            if (!success || self.testFailSOS) {
                secnotice("pairing", "failed stash credentials: %@", error);

                if(!self.sessionSupportsOctagon){
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage2), (int)subTaskSuccess);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
                    complete(true, NULL, error);
                    return;
                } else {
                    [self initiatorCompleteSecondPacketOctagon:indata application:nil complete:^(BOOL retComplete, NSDictionary *outdict, NSError *retError) {
                        if (retError == nil) {
                            subTaskSuccess = true;
                        }
                        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage2), (int)subTaskSuccess);
                        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:(subTaskSuccess ? YES : NO) error:retError];
                        complete(retComplete, outdict, retError);
                    }];
                }
            } else {
                [self initiatorCompleteSecondPacketWithSOS:indata complete:^(BOOL retSuccess, NSDictionary *outdict, NSError *retError) {
                    if (retError == nil) {
                        subTaskSuccess = true;
                    }
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage2), (int)subTaskSuccess);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:(subTaskSuccess ? YES : NO) error:retError];
                    complete(retSuccess, outdict, retError);
                }];
            }
        }];
    } else if(self.sessionSupportsOctagon) {
        [self initiatorCompleteSecondPacketOctagon:indata application:nil complete:^(BOOL retComplete, NSDictionary *outdict, NSError *retError) {
            if (retError == nil) {
                subTaskSuccess = true;
            }
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage2), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:(subTaskSuccess ? YES : NO) error:retError];
            complete(retComplete, outdict, retError);
        }];
        return;
    }
}


- (void)initiatorCompleteSecondPacketWithSOS:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator complete second packet 2");

    __weak typeof(self) weakSelf = self;

    [self setNextStateError:NULL complete:NULL];

    __block bool myPeerInfoSuccess = false;
    OctagonSignpost myPeerInfoSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorMakeSOSPeer);

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        OctagonSignpostEnd(myPeerInfoSignPost, OctagonSignpostNamePairingChannelInitiatorMakeSOSPeer, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMakeSOSPeer), (int)myPeerInfoSuccess);
        complete(true, NULL, error);
    }] myPeerInfo:self.peerVersionContext.altDSID flowID:self.peerVersionContext.flowID deviceSessionID:self.peerVersionContext.deviceSessionID canSendMetrics:YES complete:^(NSData *application, NSError *error) {
        if (application && error == nil) {
            myPeerInfoSuccess = true;
        }
        OctagonSignpostEnd(myPeerInfoSignPost, OctagonSignpostNamePairingChannelInitiatorMakeSOSPeer, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMakeSOSPeer), (int)myPeerInfoSuccess);

        if (application && !self.testFailSOS) {
            if(self.sessionSupportsOctagon) {
                [self initiatorCompleteSecondPacketOctagon:indata application:application complete:complete];
            } else {
                complete(false, @{ PairingPacketKey_PeerJoinBlob : application }, error);
                weakSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                    [weakSelf initiatorThirdPacket:nsdata complete:kscomplete];
                };
            }
        } else {
            if(self.sessionSupportsOctagon){
                [self initiatorCompleteSecondPacketOctagon:indata application:application complete:complete];
            } else {
                secnotice("pairing", "failed getting application: %@", error);
                complete(true, @{}, error);
            }
        }
    }];
}

- (BOOL)fetchPrepare:(NSMutableDictionary**)reply
         application:(NSData*)application
               error:(NSError**)error
{
    __weak typeof(self) weakSelf = self;
    __block BOOL didSucceed = NO;
    __block NSError* localError = nil;
    __block NSMutableDictionary *localReply = nil;

    //handle epoch and create identity message
    [self.otControl rpcPrepareIdentityAsApplicantWithArguments:self.controlArguments
                                                 configuration:self.joiningConfiguration
                                                             reply:^(NSString *peerID,
                                                                     NSData *permanentInfo,
                                                                     NSData *permanentInfoSig,
                                                                     NSData *stableInfo,
                                                                     NSData *stableInfoSig,
                                                                     NSError *rpcError) {
        if (rpcError || self.testFailOctagon) {
            secerror("ot-pairing: failed to create %d message: %@", self.counter, rpcError);
            localError = rpcError;
            return;
        } else {
            didSucceed = YES;

            OTPairingMessage *octagonMessage = [[OTPairingMessage alloc]init];
            octagonMessage.supportsSOS = [[OTSupportSOSMessage alloc] init];
            octagonMessage.supportsOctagon = [[OTSupportOctagonMessage alloc] init];
            OTApplicantToSponsorRound2M1 *prepare = [[OTApplicantToSponsorRound2M1 alloc] init];
            prepare.peerID = peerID;
            prepare.permanentInfo = permanentInfo;
            prepare.permanentInfoSig = permanentInfoSig;
            prepare.stableInfo = stableInfo;
            prepare.stableInfoSig = stableInfoSig;

            octagonMessage.supportsSOS.supported = SOSCCIsSOSTrustAndSyncingEnabled() ? OTSupportType_supported : OTSupportType_not_supported;
            octagonMessage.supportsOctagon.supported = OTSupportType_supported;
            octagonMessage.prepare = prepare;

            if (application){
                secnotice(pairingScope, "initiatorCompleteSecondPacketOctagon returning octagon and sos data");
                localReply = @{ PairingPacketKey_PeerJoinBlob : application, PairingPacketKey_OctagonData : octagonMessage.data}.mutableCopy;
            } else {
                secnotice(pairingScope, "initiatorCompleteSecondPacketOctagon returning octagon data");
                localReply = @{ PairingPacketKey_OctagonData : octagonMessage.data}.mutableCopy;
            }
            weakSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf initiatorThirdPacket:nsdata complete:kscomplete];
            };
        }
    }];

    if (reply) {
        if (localReply) {
            *reply = localReply;
        }
    }

    if (error) {
        if (localError) {
            *error = localError;
        }
    }

    return didSucceed;
}

- (void)initiatorCompleteSecondPacketOctagon:(NSDictionary*)indata application:(NSData*)application complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator complete second packet 2 with octagon");

    NSData *octagonData = indata[PairingPacketKey_OctagonData];

    if(![octagonData isKindOfClass:[NSData class]]) {
        secnotice(pairingScope, "initiatorCompleteSecondPacketOctagon octagonData missing or wrong class");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }

    NSError* localError = nil;
    int retry = 0;
    do {
        localError = nil;
        NSMutableDictionary* reply = [NSMutableDictionary dictionary];
        secnotice(pairingScope, "Attempt %d, calling fetchPrepare", retry+1);

        BOOL result = [self fetchPrepare:&reply application:application error:&localError];
        if (result) {
            complete(false, reply, nil);
            return;
        } else {
            if ([KCPairingChannel retryable:localError]) {
                retry += 1;
                secnotice(pairingScope, "Attempt %d, retrying fetching prepare", retry+1);
            } else {
                secerror("%s: Attempt %d, failed fetching prepare %@", pairingScope, retry+1, localError);
                complete(true, nil, localError);
                return;
            }
        }
    } while (retry < prepareMaxRetry);

    secerror("pairing: failed to fetch prepare %d times, bailing.", prepareMaxRetry);

    complete(true, nil, localError);
}

- (BOOL)join:(NSMutableDictionary**)reply
     voucher:(OTSponsorToApplicantRound2M2 *)voucher
      eventS:(AAFAnalyticsEventSecurity*)eventS
setupPairingChannelSignPost:(OctagonSignpost)setupPairingChannelSignPost
finishPairing:(BOOL*)finishPairing
       error:(NSError**)error
{
    __block BOOL didSucceed = NO;
    __block NSError* localError = nil;
    __block bool subTaskSuccess = false;
    __block NSMutableDictionary* localReply = nil;
    __weak typeof(self) weakSelf = self;

    //handle voucher and join octagon
    [self.otControl rpcJoinWithArguments:self.controlArguments
                           configuration:self.joiningConfiguration
                                   vouchData:voucher.voucher
                                    vouchSig:voucher.voucherSignature
                                       reply:^(NSError *rpcError) {
        if (rpcError || self.testFailOctagon) {
            secerror("ot-pairing: failed to create %d message: %@", self.counter, rpcError);
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:rpcError];
            localError = rpcError;
            return;
        } else {
            secnotice(pairingScope, "initiatorThirdPacket successfully joined Octagon");
            didSucceed = YES;
            typeof(self) strongSelf = weakSelf;
            if(SOSCCIsSOSTrustAndSyncingEnabled() && strongSelf->_acceptorWillSendInitialSyncCredentials) {
                strongSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                    [weakSelf initiatorFourthPacket:nsdata complete:kscomplete];
                };
                subTaskSuccess = true;
                OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                localReply = @{}.mutableCopy;
                *finishPairing = NO;
            }
            else {
                subTaskSuccess = true;
                OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                *finishPairing = YES;
            }
        }
    }];
    if (error) {
        if (localError) {
            *error = localError;
        }
    }
    if (reply) {
        *reply = localReply;
    }

    return didSucceed;
}

- (void)initiatorThirdPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator packet 3");
    OctagonSignpost setupPairingChannelSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorMessage3);

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameInitiatorJoinsTrustSystems
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    [self setNextStateError:NULL complete:NULL];

    NSData *circleBlob = indata[PairingPacketKey_CircleBlob];
    
    __block bool subTaskSuccess = false;
    __weak typeof(self) weakSelf = self;
   
    if(circleBlob != NULL && SOSCCIsSOSTrustAndSyncingEnabled()) {
        if(![circleBlob isKindOfClass:[NSData class]]) {
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
            NSError* localError = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorTypeConfusion userInfo:NULL];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            complete(true, NULL, localError);
            return;
        }

        __block bool joinSuccess = false;
        OctagonSignpost joinSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorJoinSOS);

        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            OctagonSignpostEnd(joinSignPost, OctagonSignpostNamePairingChannelInitiatorJoinSOS, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorJoinSOS), (int)joinSuccess);
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
            complete(true, NULL, error);
        }] joinCircleWithBlob:circleBlob altDSID:self.peerVersionContext.altDSID flowID:self.peerVersionContext.flowID deviceSessionID:self.peerVersionContext.deviceSessionID canSendMetrics:YES version:kPiggyV1 complete:^(bool success, NSError *error){
            if (success && error == nil) {
                joinSuccess = true;
            }
            OctagonSignpostEnd(joinSignPost, OctagonSignpostNamePairingChannelInitiatorJoinSOS, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorJoinSOS), (int)joinSuccess);

            if(error || self.testFailSOS) {
                if(self.sessionSupportsOctagon) {
                    secnotice("pairing", "failed to join circle with blob, continuing to handle octagon protocol");
                } else {
                    secnotice("pairing", "failed to join circle with blob");
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
                    complete(true, NULL, NULL);
                }
            } else {
                if(self.sessionSupportsOctagon) {
                    secnotice("pairing","initiator circle join complete");
                } else {
                    //kick off SOS ugprade
                    if(!self.sessionSupportsOctagon) {
                        [self waitForOctagonUpgrade];
                    }
                    typeof(self) strongSelf = weakSelf;
                    secnotice("pairing", "initiator circle join complete, more data: %{BOOL}d: %@",
                              strongSelf->_acceptorWillSendInitialSyncCredentials, error);
                    if (strongSelf->_acceptorWillSendInitialSyncCredentials) {
                        strongSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                            [weakSelf initiatorFourthPacket:nsdata complete:kscomplete];
                        };
                        
                        subTaskSuccess = true;
                        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
                        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                        complete(false, @{}, NULL);
                    } 
                    else {
                        subTaskSuccess = true;
                        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
                        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                        complete(true, NULL, NULL);
                    }
                }
            }
        }];
    }
    if(self.sessionSupportsOctagon){
        NSData *octagonData = indata[PairingPacketKey_OctagonData];
        if(![octagonData isKindOfClass:[NSData class]]) {
            secnotice(pairingScope, "initiatorThirdPacket octagonData missing or wrong class");
            NSError* localError = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL];
            [self setNextStateError:localError complete:complete];
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            return;
        }
        OTPairingMessage *pairingMessage = [[OTPairingMessage alloc] initWithData:octagonData];
        if(!pairingMessage.hasVoucher){
            secnotice(pairingScope, "initiatorThirdPacket pairingMessage has no voucher");
            NSError* localError = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL];
            [self setNextStateError:localError complete:complete];
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage3), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            return;
        }
        OTSponsorToApplicantRound2M2 *voucher = pairingMessage.voucher;
        NSMutableDictionary* reply = nil;
        BOOL finishedPairing = NO;
        NSError* localError = nil;

        int retry = 0;
        do {
            secnotice(pairingScope, "Attempt %d, calling join", retry+1);

            localError = nil;
            BOOL result = [self join:&reply 
                             voucher:voucher
                              eventS:eventS 
         setupPairingChannelSignPost:setupPairingChannelSignPost
                       finishPairing:&finishedPairing
                               error:&localError];
            if (result) {
                complete(finishedPairing ? true : false, reply, nil);
                return;
            } else {
                if ([KCPairingChannel retryable:localError]) {
                    retry += 1;
                    secnotice(pairingScope, "Attempt %d retrying join", retry+1);
                } else {
                    secerror("%s: Attempt %d failed join: %@", pairingScope, retry+1, localError);
                    complete(true, nil, localError);
                    return;
                }
            }
        } while (retry < joinMaxRetry);

        secerror("pairing: failed to join %d times, bailing.", joinMaxRetry);

        complete(true, nil, localError);
    }
}

- (void)initiatorFourthPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator packet 4");
    __block bool subTaskSuccess = false;
    OctagonSignpost setupPairingChannelSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorMessage4);

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameInitiatorImportsInitialSyncData
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    [self setNextStateError:NULL complete:NULL];

    NSArray *items = indata[PairingPacketKey_InitialCredentialsFromAcceptor];
    if (![items isKindOfClass:[NSArray class]]) {
        secnotice("pairing", "initiator no items to import");
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage4, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage4), (int)subTaskSuccess);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        complete(true, NULL, NULL);
        return;
    }

    [eventS addMetrics:@{kSecurityRTCFieldNumberOfKeychainItemsAdded : @(items.count)}];
    secnotice("pairing", "importing %lu items", (unsigned long)[items count]);

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage4, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage4), (int)subTaskSuccess);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
        complete(true, NULL, error);
    }] importInitialSyncCredentials:items complete:^(bool success, NSError *error) {
        secnotice("pairing", "initiator importInitialSyncCredentials: %{BOOL}d: %@", success, error);
        if (success) {
            self->_needInitialSync = false;
        }
            subTaskSuccess = true;
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelInitiatorMessage4, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorMessage4), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:success ? YES : NO error:error];
            complete(true, nil, nil);
    }];
}


//MARK: - Acceptor

- (void)acceptorFirstPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;

    secnotice("pairing", "acceptor packet 1");
    [self setNextStateError:NULL complete:NULL];

    NSInteger trustSystem = KCPairingSupportsTrustSystemUnknown;

    if (self.sessionSupportsSOS && self.sessionSupportsOctagon) {
        trustSystem = KCPairingSupportsTrustSystemBoth;
    } else if (self.sessionSupportsOctagon) {
        trustSystem = KCPairingSupportsTrustSystemOctagon;
    } else if (self.sessionSupportsSOS) {
        trustSystem = KCPairingSupportsTrustSystemSOS;
    }
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:@{kSecurityRTCFieldSupportedTrustSystem:@(trustSystem)}
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameAcceptorCreatesPacket2
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    __block bool subTaskSuccess = false;
    OctagonSignpost setupPairingChannelSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelAcceptorMessage1);

    if (self.sessionSupportsSOS && ![self ensureControlChannel]) {
        secnotice("pairing", "unable to establish a channel to sos control");
        NSError* localError = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoControlChannel userInfo:NULL];

        [self setNextStateError:localError complete:complete];

        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);
        return;
    }

    if (self.sessionSupportsSOS && indata[PairingPacketKey_InitialCredentialsFromAcceptor]) {
        secnotice("pairing", "acceptor initialSyncCredentials requested");
        self.acceptorWillSendInitialSyncCredentials = true;
        self.acceptorInitialSyncCredentialsFlags =
            SOSControlInitialSyncFlagTLK|
            SOSControlInitialSyncFlagPCS|
            SOSControlInitialSyncFlagBluetoothMigration;
    }

    if (indata[PairingPacketKey_OctagonData] == nil) {
        secnotice("pairing", "initiator didn't send a octagon packet, so skipping all octagon flows");
        self.sessionSupportsOctagon = false;
    } else {
    }

    OTOperationConfiguration* config = [[OTOperationConfiguration alloc] init];
    NSMutableDictionary *reply = [NSMutableDictionary dictionary];


    __block CliqueStatus trustStatus = CliqueStatusNotIn;
    __block NSError* localError = nil;

    [self.otControl fetchTrustStatus:self.controlArguments configuration:config reply:^(CliqueStatus status,
                                                                                        NSString * _Nullable peerID,
                                                                                        NSNumber * _Nullable numberOfOctagonPeers,
                                                                                        BOOL isExcluded,
                                                                                        NSError * _Nullable retError) {
        trustStatus = status;
        localError = retError;
    }];

    if (trustStatus != CliqueStatusIn) {
        secerror ("pairing: device is not trusted, stopping the pairing flow");
        complete(YES, nil, localError ?: [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNotTrustedInOctagon description:@"device cannot support pairing, not trusted in Octagon"]);
        return;
    }

    if (localError) {
        secerror("pairing: failed to check trust status: %@", localError);
        complete(YES, nil, localError);
        return;
    }

    if(self.sessionSupportsSOS) {
        OctagonSignpost fetchStashSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelAcceptorFetchStashCredential);
        __block bool fetchSubtaskSuccess = false;
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull connectionError) {
            OctagonSignpostEnd(fetchStashSignPost, OctagonSignpostNamePairingChannelAcceptorFetchStashCredential, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorFetchStashCredential), (int)fetchSubtaskSuccess);
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:connectionError];
            complete(true, NULL, connectionError);
        }] validatedStashedAccountCredential:self.peerVersionContext.altDSID flowID:self.peerVersionContext.flowID deviceSessionID:self.peerVersionContext.deviceSessionID canSendMetrics:YES complete:^(NSData *credential, NSError *error) {
            secnotice("pairing", "acceptor validatedStashedAccountCredential: %{BOOL}d (%@)", credential != NULL, error);
            if (credential && error == nil) {
                fetchSubtaskSuccess = true;
            }
            OctagonSignpostEnd(fetchStashSignPost, OctagonSignpostNamePairingChannelAcceptorFetchStashCredential, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorFetchStashCredential), (int)fetchSubtaskSuccess);

            if(credential){
                reply[PairingPacketKey_Credential] = credential;

                if (self.acceptorWillSendInitialSyncCredentials) {
                    reply[PairingPacketKey_InitialCredentialsFromAcceptor] = @YES;
                };
                if(self.sessionSupportsOctagon) {
                    [self acceptorFirstOctagonPacket:indata reply:reply complete:^(BOOL retComplete, NSDictionary *outdict, NSError *retError) {
                        if(retError == nil) {
                            subTaskSuccess = true;
                        }

                        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);
                        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:subTaskSuccess ? YES : NO error:retError];
                        complete(retComplete, outdict, retError);
                    }];
                } else {
                    self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                        [weakSelf acceptorSecondPacket:nsdata complete:kscomplete];
                    };
                    secnotice("pairing", "acceptor reply to packet 1");
                    subTaskSuccess = true;
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                    complete(false, reply, NULL);
                }
            }
            else if ((!credential && !self.sessionSupportsOctagon) || self.testFailSOS) {
                secnotice("pairing", "acceptor doesn't have a stashed credential: %@", error);
                NSError *noStashCredentail = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoStashedCredential userInfo:NULL];
                [self setNextStateError:noStashCredentail complete:complete];
                OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:noStashCredentail];
                return;
            }
            else if(self.sessionSupportsOctagon) {
                [self acceptorFirstOctagonPacket:indata reply:reply complete:^(BOOL retComplete, NSDictionary *outdict, NSError *retError) {
                    if(retError == nil) {
                        subTaskSuccess = true;
                    }
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:subTaskSuccess ? YES : NO error:retError];
                    complete(retComplete, outdict, retError);
                }];
            }
        }];
    } else if(self.sessionSupportsOctagon){
        [self acceptorFirstOctagonPacket:indata reply:reply complete:^(BOOL retComplete, NSDictionary *outdict, NSError *retError) {
            if(retError == nil) {
                subTaskSuccess = true;
            }
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:subTaskSuccess ? YES : NO error:retError];
            complete(retComplete, outdict, retError);
        }];
    } else {
        secnotice("pairing", "acceptor neither of octagon nor SOS");
        NSError *notSupported = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoTrustAvailable userInfo:NULL];
        [self setNextStateError:notSupported complete:complete];
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage1, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage1), (int)subTaskSuccess);

        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:notSupported];
    }
}

- (BOOL)fetchEpoch:(NSMutableDictionary*)reply error:(NSError**)error {
    __block BOOL success = NO;
    __block NSError* retError = nil;
    __weak typeof(self) weakSelf = self;

    //fetch epoch
    [self.otControl rpcEpochWithArguments:self.controlArguments 
                            configuration:self.joiningConfiguration
                                    reply:^(uint64_t epoch, NSError * _Nullable rpcError) {
        secnotice("pairing", "acceptor rpcEpochWithArguments: %ld (%@)", (long)epoch, rpcError);
       
        if(rpcError || self.testFailOctagon){
            secerror("error acceptor handling packet %d", self.counter);
            retError = rpcError;
        } else {
            secnotice(pairingScope, "acceptor handled packet %d", self.counter);
           
            self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf acceptorSecondPacket:nsdata complete:kscomplete];
            };

            OTPairingMessage *response = [[OTPairingMessage alloc] init];
            response.supportsSOS = [[OTSupportSOSMessage alloc] init];
            response.supportsOctagon = [[OTSupportOctagonMessage alloc] init];
            response.epoch = [[OTSponsorToApplicantRound1M2 alloc] init];
            response.epoch.epoch = epoch;
            response.supportsSOS.supported = SOSCCIsSOSTrustAndSyncingEnabled() ? OTSupportType_supported : OTSupportType_not_supported;
            response.supportsOctagon.supported = OTSupportType_supported;
            reply[PairingPacketKey_OctagonData] = response.data;
            
            secnotice(pairingScope, "acceptor reply to packet 1");
            success = YES;
        }
    }];

    if (error) {
        if (retError) {
            *error = retError;
        }
    }
    return success;
}

- (void)acceptorFirstOctagonPacket:(NSDictionary *)indata reply:(NSMutableDictionary*)reply complete:(KCPairingInternalCompletion)complete
{
    NSDictionary *octagonData = indata[PairingPacketKey_OctagonData];

    if(![octagonData isKindOfClass:[NSDictionary class]]) {
        secnotice(pairingScope, "acceptorFirstOctagonPacket octagon data missing");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }

    NSDictionary *unpackedMessage = octagonData;

    if(![unpackedMessage[PairingPacketKey_Version] isEqualToString:@"O"]){
        secnotice(pairingScope, "acceptorFirstOctagonPacket 'v' contents wrong");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }

    int retry = 0;
    NSError* localError = nil;
    do {
        secnotice(pairingScope, "Attempt %d fetching epoch", retry+1);

        localError = nil;
        BOOL result = [self fetchEpoch:reply error:&localError];
        if (result) {
            complete(false, reply, nil);
            return;
        } else {
            if ([KCPairingChannel retryable:localError]) {
                retry += 1;
                secnotice(pairingScope, "Attempt %d retrying fetching epoch", retry+1);
            } else {
                secerror("%s: Attempt %d failed fetching epoch: %@", pairingScope, retry+1, localError);
                complete(true, nil, localError);
                return;
            }
        }
    } while (retry < epochMaxRetry);

    secerror("pairing: failed to fetch epoch %d times, bailing.", epochMaxRetry);

    complete(true, nil, localError);
}

- (void)acceptorSecondPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;

    [self setNextStateError:NULL complete:NULL];
    __block bool subTaskSuccess = false;
    OctagonSignpost setupPairingChannelSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelAcceptorMessage2);

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameAcceptorCreatesPacket4
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    secnotice("pairing", "acceptor packet 2");
    __block NSMutableDictionary *reply = [NSMutableDictionary dictionary];

    NSData *peerJoinBlob = indata[PairingPacketKey_PeerJoinBlob];

    if(self.sessionSupportsSOS && [peerJoinBlob isKindOfClass:[NSData class]]) {
        __block bool joinSubTaskSuccess = false;
        OctagonSignpost joinSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelAcceptorCircleJoiningBlob);
        
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            OctagonSignpostEnd(joinSignPost, OctagonSignpostNamePairingChannelAcceptorCircleJoiningBlob, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorCircleJoiningBlob), (int)joinSubTaskSuccess);
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage2), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
            complete(true, NULL, error);
        }] circleJoiningBlob:self.peerVersionContext.altDSID flowID:self.peerVersionContext.flowID deviceSessionID:self.peerVersionContext.deviceSessionID canSendMetrics:YES applicant:peerJoinBlob complete:^(NSData *blob, NSError *error){
            if (blob && error == nil) {
                joinSubTaskSuccess = true;
            }
            OctagonSignpostEnd(joinSignPost, OctagonSignpostNamePairingChannelAcceptorCircleJoiningBlob, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorCircleJoiningBlob), (int)joinSubTaskSuccess);

            if (blob) {
                secnotice("pairing", "acceptor pairing complete (will send: %{BOOL}d): %@",
                          self.acceptorWillSendInitialSyncCredentials,
                          error);

                reply[PairingPacketKey_CircleBlob] = blob;
            }
            
            if(self.sessionSupportsOctagon) {
                [self acceptorSecondOctagonPacket:indata reply:reply complete:^(BOOL retComplete, NSDictionary *outdict, NSError *retError) {
                    if (retError == nil) {
                        subTaskSuccess = true;
                    }
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage2), (int)subTaskSuccess);

                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:(subTaskSuccess ? YES : NO) error:retError];
                    complete(retComplete, outdict, retError);
                }];
            } else {
                secnotice("pairing", "posting kSOSCCCircleOctagonKeysChangedNotification");
                notify_post(kSOSCCCircleOctagonKeysChangedNotification);
                if (self.acceptorWillSendInitialSyncCredentials) {
                    self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                        [weakSelf acceptorThirdPacket:nsdata complete:kscomplete];
                    };
                    subTaskSuccess = true;
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage2), (int)subTaskSuccess);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                    complete(false, reply, NULL);
                } else {
                    subTaskSuccess = true;
                    OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage2), (int)subTaskSuccess);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                    complete(true, reply, NULL);
                }
                secnotice("pairing", "acceptor reply to packet 2");
            }
        }];
    } else if(self.sessionSupportsOctagon){
        [self acceptorSecondOctagonPacket:indata reply:reply complete:^(BOOL retComplete, NSDictionary *outdict, NSError *retError) {
            if (retError == nil) {
                subTaskSuccess = true;
            }
            OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage2, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage2), (int)subTaskSuccess);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:(subTaskSuccess ? YES : NO) error:retError];
            complete(retComplete, outdict, retError);
        }];
    }
}

- (BOOL)fetchVoucher:(NSMutableDictionary*)reply
             prepare:(OTApplicantToSponsorRound2M1 *)prepare
              eventS:(AAFAnalyticsEventSecurity *)eventS
     finishedPairing:(BOOL*)finishedPairing
       maxCapability:(KCPairingIntent_Capability)maxCapability
               error:(NSError**)error
{
    __block BOOL didSucceed = NO;
    __block NSError* localError = nil;
    __weak typeof(self) weakSelf = self;

    //handle identity and fetch voucher
    [self.otControl rpcVoucherWithArguments:self.controlArguments
                              configuration:self.joiningConfiguration
                                     peerID:prepare.peerID
                              permanentInfo:prepare.permanentInfo
                           permanentInfoSig:prepare.permanentInfoSig
                                 stableInfo:prepare.stableInfo
                              stableInfoSig:prepare.stableInfoSig
                              maxCapability:maxCapability
                                      reply:^(NSData *voucher,
                                              NSData *voucherSig,
                                              NSError *rpcError) {
        if (rpcError || self.testFailOctagon){
            secerror("error acceptor handling octagon packet %d", self.counter);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:rpcError];
            localError = rpcError;
            return;
        } else {
            bool finished = true;

            secnotice(pairingScope, "acceptor handled octagon packet %d", self.counter);
            if (SOSCCIsSOSTrustAndSyncingEnabled() && self.acceptorWillSendInitialSyncCredentials) {
                self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                    [weakSelf acceptorThirdPacket:nsdata complete:kscomplete];
                };
                finished = false;
            }
            OTPairingMessage *response = [[OTPairingMessage alloc] init];
            response.supportsSOS = [[OTSupportSOSMessage alloc] init];
            response.supportsOctagon = [[OTSupportOctagonMessage alloc] init];
            response.voucher = [[OTSponsorToApplicantRound2M2 alloc] init];
            response.voucher.voucher = voucher;
            response.voucher.voucherSignature = voucherSig;
            response.supportsSOS.supported = SOSCCIsSOSTrustAndSyncingEnabled() ? OTSupportType_supported : OTSupportType_not_supported;
            response.supportsOctagon.supported = OTSupportType_supported;

            if (self.acceptorWillSendInitialSyncCredentials) {
                // no need to share TLKs over the pairing channel, that's provided by octagon
                self.acceptorInitialSyncCredentialsFlags &= ~(SOSControlInitialSyncFlagTLK | SOSControlInitialSyncFlagPCS);
            }

            reply[PairingPacketKey_OctagonData] = response.data;

            secnotice("pairing", "acceptor reply to packet 2");
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
            didSucceed = YES;
            *finishedPairing = finished ? YES : NO;

        }
    }];
    if (error) {
        if (localError) {
            *error = localError;
        }
    }
    return didSucceed;
}

- (void)acceptorSecondOctagonPacket:(NSDictionary*)indata reply:(NSMutableDictionary*)reply complete:(KCPairingInternalCompletion)complete
{
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameAcceptorCreatesVoucher
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    NSData *octagonData = indata[PairingPacketKey_OctagonData];

    if(![octagonData isKindOfClass:[NSData class]]) {
        secnotice(pairingScope, "acceptorSecondOctagonPacket octagon data missing");
        NSError* localError = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL];
        [self setNextStateError:localError complete:complete];
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        return;
    }

    OTPairingMessage *pairingMessage = [[OTPairingMessage alloc] initWithData:octagonData];
    if(!pairingMessage.hasPrepare){
        secerror("ot-pairing: acceptorSecondOctagonPacket: no octagon message");
        NSError* localError = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL];
        [self setNextStateError:localError complete:complete];
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        return;
    }
    OTApplicantToSponsorRound2M1 *prepare = pairingMessage.prepare;

    BOOL finishPairing = NO;
    NSError* localError = nil;
    int retry = 0;

    // Max channel capability - default to full-peer
    NSString * maxCap = KCPairingIntent_Capability_FullPeer;
    if (self.peerVersionContext.capability){
        maxCap = self.peerVersionContext.capability;
    }
    secnotice(pairingScope, "acceptor channel max capability set to %@", maxCap);

    do {
        secnotice(pairingScope, "Attempt %d fetching voucher", retry+1);

        localError = nil;
        BOOL result = [self fetchVoucher:reply prepare:prepare eventS:eventS finishedPairing:&finishPairing maxCapability:maxCap error:&localError];

        if (result) {
            complete(finishPairing ? true : false, reply, nil);
            return;
        } else {
            if ([KCPairingChannel retryable:localError]) {
                retry += 1;
                secnotice(pairingScope, "Attempt %d retrying fetching voucher", retry+1);
            } else {
                secerror("%s Attempt %d failed fetching voucher: %@", pairingScope, retry+1, localError);
                complete(true, nil, localError);
                return;
            }
        }
    } while (retry < vouchMaxRetry);

    secerror("pairing: failed to fetch voucher %d times, bailing.", vouchMaxRetry);

    complete(true, nil, localError);
}

- (void)acceptorThirdPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "acceptor packet 3");
    __block bool subTaskSuccess = false;
    OctagonSignpost setupPairingChannelSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelAcceptorMessage3);

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.peerVersionContext.altDSID
                                                                                                  flowID:self.peerVersionContext.flowID
                                                                                         deviceSessionID:self.peerVersionContext.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameAcceptorCreatesPacket5
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    const uint32_t initialSyncCredentialsFlags = self.acceptorInitialSyncCredentialsFlags;

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage3), (int)subTaskSuccess);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
        complete(true, NULL, error);
    }] initialSyncCredentials:initialSyncCredentialsFlags altDSID:self.peerVersionContext.altDSID flowID:self.peerVersionContext.flowID deviceSessionID:self.peerVersionContext.deviceSessionID canSendMetrics:YES complete:^(NSArray *items, NSError *error2) {
        NSMutableDictionary *reply = [NSMutableDictionary dictionary];

        secnotice("pairing", "acceptor initialSyncCredentials complete: items %u: %@", (unsigned)[items count], error2);
        if (items) {
            reply[PairingPacketKey_InitialCredentialsFromAcceptor] = items;
        }
        secnotice("pairing", "acceptor reply to packet 3");
        subTaskSuccess = true;
        OctagonSignpostEnd(setupPairingChannelSignPost, OctagonSignpostNamePairingChannelAcceptorMessage3, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorMessage3), (int)subTaskSuccess);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:error2 ? YES : NO error:error2];
        complete(true, reply, NULL);
    }];
}


//MARK: - Helper

- (bool)ensureControlChannel
{
    if (self.connection)
        return true;

    NSXPCInterface *interface = [NSXPCInterface interfaceWithProtocol:@protocol(SOSControlProtocol)];

    self.connection = [[NSXPCConnection alloc] initWithMachServiceName:@(kSecuritydSOSServiceName) options:0];
    if (self.connection == NULL){
        return false;
    }
    self.connection.remoteObjectInterface = interface;

    [self.connection resume];

    return true;
}

- (void)validateStart:(void(^)(bool result, NSError *error))complete
{
    if (!self.initiator) {
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            complete(false, error);
        }] stashedCredentialPublicKey:^(NSData *publicKey, NSError *error) {
            complete(publicKey != NULL, error);
        }];
    } else {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            complete(true, NULL);
        });
    }
}

- (void)exchangePacket:(NSData *)inputCompressedData complete:(KCPairingChannelCompletion)complete
{
    NSDictionary *indict = NULL;
    secnotice("pairing", "Exchange packet: %u", self.counter);
    self.counter++;

    if (inputCompressedData) {

        NSData *data = [[self class] pairingChannelDecompressData:inputCompressedData];
        if (data == NULL) {
            secnotice("pairing", "failed to decompress");
            complete(true, NULL, NULL);
            return;
        }

        NSError *error = NULL;
        indict = [NSPropertyListSerialization propertyListWithData:data
                                                           options:(NSPropertyListReadOptions)kCFPropertyListSupportedFormatBinary_v1_0
                                                            format:NULL
                                                             error:&error];
        if (indict == NULL) {
            secnotice("pairing", "failed to deserialize");
            complete(true, NULL, error);
            return;
        }
    }
    self.nextState(indict, ^(BOOL completed, NSDictionary *outdict, NSError *error) {
        NSData *outdata = NULL, *compressedData = NULL;
        if (outdict) {
            NSError *error2 = NULL;
            outdata = [NSPropertyListSerialization dataWithPropertyList:outdict format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error2];
            if (outdata == NULL && error)
                error = error2;
            if (outdata)
                compressedData = [[self class] pairingChannelCompressData:outdata];

            if (compressedData) {
                NSString *key = [NSString stringWithFormat:@"com.apple.ckks.pairing.packet-size.%s.%u",
                                 self->_initiator ? "initiator" : "acceptor", self->_counter];
                [SecCoreAnalytics sendEvent:key event:@{SecCoreAnalyticsValue: @([compressedData length]) }];
                secnotice("pairing", "pairing packet size %lu", (unsigned long)[compressedData length]);
            }
        }
        secnotice("pairing", "Exchange packet complete data: %{BOOL}d: %@", outdata != nil, error);
        complete(completed, compressedData, error);
    });
}

- (NSData *)exchangePacket:(NSData *)data complete:(bool *)complete error:(NSError * __autoreleasing *)error
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSData *rData = NULL;
    __block NSError* processingError;
    [self exchangePacket:data complete:^(BOOL cComplete, NSData *cData, NSError *cError) {
        *complete = cComplete;
        rData = cData;
        processingError = cError;
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    if (error)
        *error = processingError;
    return rData;
}

- (void)setXPCConnectionObject:(NSXPCConnection *)connection
{
    self.connection = connection;
}

- (void)setControlObject:(id)control
{
    self.otControl = control;
}

- (void)setSessionControlArguments:(id)controlArguments
{
    self.controlArguments = controlArguments;
}

- (void)setConfiguration:(id)config
{
    self.joiningConfiguration = config;
}

- (void)setSOSMessageFailForTesting:(BOOL)value
{
    self.testFailSOS = value;
}
- (void)setOctagonMessageFailForTesting:(BOOL)value
{
    self.testFailOctagon = value;
}

- (void)setSessionSupportsOctagonForTesting:(bool)value
{
    self.sessionSupportsOctagon = value;
}
@end
