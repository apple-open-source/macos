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
#import <ipc/securityd_client.h>
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OctagonControlServer.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OctagonControlServer.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import "keychain/ot/proto/generated_source/OTPairingMessage.h"
#import "keychain/ot/proto/generated_source/OTApplicantToSponsorRound2M1.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound2M2.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound1M2.h"
#import "keychain/ot/proto/generated_source/OTPairingMessage.h"
#include <notify.h>

#import <compression.h>
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#import <MobileGestalt.h>
#endif

#import "utilities/SecADWrapper.h"

KCPairingIntent_Type KCPairingIntent_Type_None = @"none";
KCPairingIntent_Type KCPairingIntent_Type_SilentRepair = @"repair";
KCPairingIntent_Type KCPairingIntent_Type_UserDriven = @"userdriven";

typedef void(^KCPairingInternalCompletion)(BOOL complete, NSDictionary *outdict, NSError *error);
typedef void(^KCNextState)(NSDictionary *indict, KCPairingInternalCompletion complete);

NSString *kKCPairingChannelErrorDomain = @"com.apple.security.kcparingchannel";

const char* pairingScope = "ot-pairing";

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
        && ((!self->_uniqueClientID && !other->_uniqueClientID) || [self->_uniqueClientID isEqual:other->_uniqueClientID])
        && ((!self->_intent && !other->_intent) || [self->_intent isEqual:other->_intent])
    ;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_model forKey:@"model"];
    [coder encodeObject:_modelVersion forKey:@"modelVersion"];
    [coder encodeObject:_modelClass forKey:@"modelClass"];
    [coder encodeObject:_osVersion forKey:@"osVersion"];
    [coder encodeObject:_uniqueDeviceID forKey:@"uniqueDeviceID"];
    [coder encodeObject:_uniqueClientID forKey:@"uniqueClientID"];
    [coder encodeObject:_intent forKey:@"intent"];
}

- (nullable instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super init];
    if (self) {
        _model = [decoder decodeObjectOfClass:[NSString class] forKey:@"model"];
        _modelVersion = [decoder decodeObjectOfClass:[NSString class] forKey:@"modelVersion"];
        _modelClass = [decoder decodeObjectOfClass:[NSString class] forKey:@"modelClass"];
        _osVersion = [decoder decodeObjectOfClass:[NSString class] forKey:@"osVersion"];
        _uniqueDeviceID = [decoder decodeObjectOfClass:[NSString class] forKey:@"uniqueDeviceID"];
        _uniqueClientID = [decoder decodeObjectOfClass:[NSString class] forKey:@"uniqueClientID"];
        _intent = [decoder decodeObjectOfClass:[NSString class] forKey:@"intent"];

        /* validate intent if we have one */
        if (_intent != NULL &&
            !([_intent isEqualToString:KCPairingIntent_Type_None] ||
              [_intent isEqualToString:KCPairingIntent_Type_SilentRepair] ||
              [_intent isEqualToString:KCPairingIntent_Type_UserDriven]))
        {
            return nil;
        }
    }
    return self;
}

@end


@interface KCPairingChannel ()
@property (assign) KCPairingChannelContext *peerVersionContext;
@property (assign) bool initiator;
@property (assign) unsigned counter;
@property (assign) bool acceptorWillSendInitialSyncCredentials;
@property (assign) uint32_t acceptorInitialSyncCredentialsFlags;
@property (strong) NSXPCConnection *connection;
@property (strong) OTControl *otControl;
@property (strong) NSString* contextID;
@property (strong) OTNextState nextOctagonState;
@property (strong) KCNextState nextState;
@property (nonatomic, strong) OTJoiningConfiguration* joiningConfiguration;
@property (nonatomic) bool testFailSOS;
@property (nonatomic) bool testFailOctagon;

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
        _contextID = OTDefaultContext;

        /* only apply to acceptor right now */
        _sessionSupportsSOS = OctagonPlatformSupportsSOS();
        _sessionSupportsOctagon = OctagonIsEnabled();
        NSError *localError = nil;
        _otControl = [OTControl controlObject:true error:&localError];
        if(localError){
            secerror("could not stand up otcontrol connection");
        }
        _joiningConfiguration = [[OTJoiningConfiguration alloc]initWithProtocolType:OTProtocolPairing
                                                                     uniqueDeviceID:peerVersionContext.uniqueDeviceID
                                                                     uniqueClientID:peerVersionContext.uniqueClientID
                                                                        pairingUUID:[[NSUUID UUID] UUIDString]
                                                                      containerName:nil
                                                                          contextID:OTDefaultContext
                                                                              epoch:0
                                                                        isInitiator:initiator];
    }
    return self;
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
    NSMutableData *scratch = [NSMutableData dataWithLength:compression_encode_scratch_buffer_size(pairingCompression)];

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
    NSMutableData *scratch = [NSMutableData dataWithLength:compression_decode_scratch_buffer_size(pairingCompression)];

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

- (void) attemptSosUpgrade
{
    [self.otControl attemptSosUpgrade:nil context:self.contextID reply:^(NSError *error) {
        if(error){
            secerror("pairing: failed to upgrade initiator into Octagon: %@", error);
        }
    }];
}

- (void)initiatorFirstPacket:(NSDictionary * __unused)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator packet 1");

    if (OctagonPlatformSupportsSOS() && ![self ensureControlChannel]) {
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoControlChannel userInfo:NULL] complete:complete];
        return;
    }

    if(self.sessionSupportsOctagon && self.sessionSupportsSOS && !self.testFailOctagon) {
        __weak typeof(self) weakSelf = self;
        self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
            [weakSelf initiatorSecondPacket:nsdata complete:kscomplete];
        };
        complete(false, @{ @"d" : @YES, @"o" : @{@"v" : @"O"} }, NULL);
        return;
    } else if (self.sessionSupportsOctagon && self.testFailOctagon) {
        complete(true, nil, NULL);
        return;
    } else if (self.sessionSupportsOctagon && !self.sessionSupportsSOS) {
        __weak typeof(self) weakSelf = self;
        self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
            [weakSelf initiatorSecondPacket:nsdata complete:kscomplete];
        };
        complete(false, @{ @"o" : @{@"v" : @"O"} }, NULL);
        return;
    }
    else {
        __weak typeof(self) weakSelf = self;
        self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
            [weakSelf initiatorSecondPacket:nsdata complete:kscomplete];
        };
        complete(false, @{ @"d" : @YES }, NULL);
    }
}

- (void)initiatorSecondPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator packet 2");

    NSData *octagonData = indata[@"o"];

    if(octagonData == nil) {
        secnotice("pairing", "acceptor didn't send a octagon packet, so skipping all octagon flows");
        self.sessionSupportsOctagon = false;
    }

    NSData *credential = indata[@"c"];
    if (![credential isKindOfClass:[NSData class]]) {
        if(!OctagonIsEnabled() && OctagonPlatformSupportsSOS()) {
            secnotice("pairing", "no credential");
            [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorAccountCredentialMissing userInfo:NULL] complete:complete];
            return;
        }
    }

    if (OctagonPlatformSupportsSOS() && indata[@"d"]) {
        secnotice("pairing", "acceptor will send send initial credentials");
        self.acceptorWillSendInitialSyncCredentials = true;
    }
    if(OctagonPlatformSupportsSOS()) {
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            complete(true, NULL, error);
        }] stashAccountCredential:credential complete:^(bool success, NSError *error) {
            [self setNextStateError:NULL complete:NULL];
            if (!success || self.testFailSOS) {
                secnotice("pairing", "failed stash credentials: %@", error);
                if(!OctagonIsEnabled() || !self.sessionSupportsOctagon){
                    complete(true, NULL, error);
                    return;
                } else {
                    [self initiatorCompleteSecondPacketOctagon:indata application:nil complete:complete];
                }
            } else{
                [self initiatorCompleteSecondPacketWithSOS:indata complete:complete];
            }
        }];
    } else if(OctagonIsEnabled() && self.sessionSupportsOctagon) {
        [self initiatorCompleteSecondPacketOctagon:indata application:nil complete:complete];
        return;
    }
}


- (void)initiatorCompleteSecondPacketWithSOS:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator complete second packet 2");

    __weak typeof(self) weakSelf = self;

    [self setNextStateError:NULL complete:NULL];

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] myPeerInfo:^(NSData *application, NSError *error) {
        if (application && !self.testFailSOS) {
            if(OctagonIsEnabled() && self.sessionSupportsOctagon) {
                [self initiatorCompleteSecondPacketOctagon:indata application:application complete:complete];
            } else{
                complete(false, @{ @"p" : application }, error);
                weakSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                    [weakSelf initiatorThirdPacket:nsdata complete:kscomplete];
                };
            }
        } else {
            if(OctagonIsEnabled() && self.sessionSupportsOctagon){
                [self initiatorCompleteSecondPacketOctagon:indata application:application complete:complete];
            } else {
                secnotice("pairing", "failed getting application: %@", error);
                complete(true, @{}, error);
            }
        }
    }];
}

- (void)initiatorCompleteSecondPacketOctagon:(NSDictionary*)indata application:(NSData*)application complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator complete second packet 2 with octagon");

    __weak typeof(self) weakSelf = self;
    NSData *octagonData = indata[@"o"];

    if(![octagonData isKindOfClass:[NSData class]]) {
        secnotice(pairingScope, "initiatorCompleteSecondPacketOctagon octagonData missing or wrong class");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }

    //handle epoch and create identity message
    [self.otControl rpcPrepareIdentityAsApplicantWithConfiguration:self.joiningConfiguration
                                                             reply:^(NSString *peerID,
                                                                     NSData *permanentInfo,
                                                                     NSData *permanentInfoSig,
                                                                     NSData *stableInfo,
                                                                     NSData *stableInfoSig,
                                                                     NSError *error) {
        if (error || self.testFailOctagon) {
            secerror("ot-pairing: failed to create %d message: %@", self.counter, error);
            complete(true, nil, error);
            return;
        } else {
            OTPairingMessage *octagonMessage = [[OTPairingMessage alloc]init];
            OTApplicantToSponsorRound2M1 *prepare = [[OTApplicantToSponsorRound2M1 alloc] init];
            prepare.peerID = peerID;
            prepare.permanentInfo = permanentInfo;
            prepare.permanentInfoSig = permanentInfoSig;
            prepare.stableInfo = stableInfo;
            prepare.stableInfoSig = stableInfoSig;
            octagonMessage.prepare = prepare;
            if(application){
                secnotice(pairingScope, "initiatorCompleteSecondPacketOctagon returning octagon and sos data");
                complete(false, @{ @"p" : application, @"o" : octagonMessage.data }, nil);
            } else {
                secnotice(pairingScope, "initiatorCompleteSecondPacketOctagon returning octagon data");
                complete(false, @{ @"o" : octagonMessage.data }, nil);
            }
            weakSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf initiatorThirdPacket:nsdata complete:kscomplete];
            };
        }
    }];
}

- (void)initiatorThirdPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;
    secnotice("pairing", "initiator packet 3");

    [self setNextStateError:NULL complete:NULL];

    NSData *circleBlob = indata[@"b"];

    if (circleBlob == NULL) {
        if(!OctagonIsEnabled() && OctagonPlatformSupportsSOS()) {
            secnotice("pairing", "no sos circle");
            complete(true, NULL, NULL);
            return;
        }
    } else if(OctagonPlatformSupportsSOS()) {
        if(![circleBlob isKindOfClass:[NSData class]]) {
            complete(true, NULL, [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorTypeConfusion userInfo:NULL]);
            return;
        }

        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            complete(true, NULL, error);
        }] joinCircleWithBlob:circleBlob version:kPiggyV1 complete:^(bool success, NSError *error){
            if(error || self.testFailSOS) {
                if(OctagonIsEnabled() && self.sessionSupportsOctagon) {
                    secnotice("pairing", "failed to join circle with blob, continuing to handle octagon protocol");
                } else {
                    secnotice("pairing", "failed to join circle with blob");
                    complete(true, NULL, NULL);
                }
            } else{
                if(OctagonIsEnabled() && self.sessionSupportsOctagon) {
                    secnotice("pairing","initiator circle join complete");
                } else {
                    //kick off SOS ugprade
                    if(OctagonIsEnabled() && !self.sessionSupportsOctagon) {
                        [self attemptSosUpgrade];
                    }
                    typeof(self) strongSelf = weakSelf;
                    secnotice("pairing", "initiator circle join complete, more data: %s: %@",
                              strongSelf->_acceptorWillSendInitialSyncCredentials ? "yes" : "no", error);

                    if (strongSelf->_acceptorWillSendInitialSyncCredentials) {
                        strongSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                            [weakSelf initiatorFourthPacket:nsdata complete:kscomplete];
                        };

                        complete(false, @{}, NULL);
                    } else {
                        complete(true, NULL, NULL);
                    }
                }
            }
        }];
    }
    if(OctagonIsEnabled() && self.sessionSupportsOctagon){
        NSData *octagonData = indata[@"o"];
        if(![octagonData isKindOfClass:[NSData class]]) {
            secnotice(pairingScope, "initiatorThirdPacket octagonData missing or wrong class");
            [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
            return;
        }
        OTPairingMessage *pairingMessage = [[OTPairingMessage alloc] initWithData:octagonData];
        if(!pairingMessage.hasVoucher){
            secnotice(pairingScope, "initiatorThirdPacket pairingMessage has no voucher");
           [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
            return;
        }
        OTSponsorToApplicantRound2M2 *voucher = pairingMessage.voucher;

        //handle voucher and join octagon
        [self.otControl rpcJoinWithConfiguration:self.joiningConfiguration
                                       vouchData:voucher.voucher
                                        vouchSig:voucher.voucherSignature
                                           reply:^(NSError *error) {
            if (error || self.testFailOctagon) {
                secerror("ot-pairing: failed to create %d message: %@", self.counter, error);
                complete(true, NULL, error);
                return;
            }else{
                secnotice(pairingScope, "initiatorThirdPacket successfully joined Octagon");
                typeof(self) strongSelf = weakSelf;
                if(OctagonPlatformSupportsSOS() && strongSelf->_acceptorWillSendInitialSyncCredentials) {
                    strongSelf.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                        [weakSelf initiatorFourthPacket:nsdata complete:kscomplete];
                    };
                    complete(false, @{}, nil);

                } else {
                    complete(true, nil, nil);

                }
            }
        }];
    }
}

- (void)initiatorFourthPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "initiator packet 4");

    [self setNextStateError:NULL complete:NULL];

    NSArray *items = indata[@"d"];
    if (![items isKindOfClass:[NSArray class]]) {
        secnotice("pairing", "initiator no items to import");
        complete(true, NULL, NULL);
        return;
    }

    secnotice("pairing", "importing %lu items", (unsigned long)[items count]);

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] importInitialSyncCredentials:items complete:^(bool success, NSError *error) {
        secnotice("pairing", "initiator importInitialSyncCredentials: %s: %@", success ? "yes" : "no", error);
        if (success)
            self->_needInitialSync = false;
        complete(true, nil, nil);
    }];
}



//MARK: - Acceptor

- (void)acceptorFirstPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;

    secnotice("pairing", "acceptor packet 1");
    [self setNextStateError:NULL complete:NULL];

    if (self.sessionSupportsSOS && ![self ensureControlChannel]) {
        secnotice("pairing", "unable to establish a channel to sos control");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoControlChannel userInfo:NULL] complete:complete];
        return;
    }

    if (self.sessionSupportsSOS && indata[@"d"]) {
        secnotice("pairing", "acceptor initialSyncCredentials requested");
        self.acceptorWillSendInitialSyncCredentials = true;
        self.acceptorInitialSyncCredentialsFlags =
            SOSControlInitialSyncFlagTLK|
            SOSControlInitialSyncFlagPCS|
            SOSControlInitialSyncFlagBluetoothMigration;

    }

    if (indata[@"o"] == nil) {
        secnotice("pairing", "initiator didn't send a octagon packet, so skipping all octagon flows");
        self.sessionSupportsOctagon = false;
    }
    // XXX Before we go here we should check if we are trusted or not, if we are not, there is no point proposing octagon

    NSMutableDictionary *reply = [NSMutableDictionary dictionary];

    if(self.sessionSupportsSOS) {
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            complete(true, NULL, error);
        }] validatedStashedAccountCredential:^(NSData *credential, NSError *error) {
            secnotice("pairing", "acceptor validatedStashedAccountCredential: %@ (%@)", credential != NULL ? @"yes" : @"no", error);
            if(credential){
                reply[@"c"] = credential;

                if (self.acceptorWillSendInitialSyncCredentials) {
                    reply[@"d"] = @YES;
                };
                if(self.sessionSupportsOctagon) {
                    [self acceptorFirstOctagonPacket:indata reply:reply complete:complete];
                } else {
                    self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                        [weakSelf acceptorSecondPacket:nsdata complete:kscomplete];
                    };
                    secnotice("pairing", "acceptor reply to packet 1");
                    complete(false, reply, NULL);
                }
            }
            else if ((!credential && !self.sessionSupportsOctagon) || self.testFailSOS) {
                secnotice("pairing", "acceptor doesn't have a stashed credential: %@", error);
                NSError *noStashCredentail = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoStashedCredential userInfo:NULL];
                [self setNextStateError:noStashCredentail complete:complete];
                return;
            }
            else if(self.sessionSupportsOctagon) {
                [self acceptorFirstOctagonPacket:indata reply:reply complete:complete];
            } 
        }];
    } else if(self.sessionSupportsOctagon){
        [self acceptorFirstOctagonPacket:indata reply:reply complete:complete];
    } else {
        secnotice("pairing", "acceptor neither of octagon nor SOS");
        NSError *notSupported = [NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorNoTrustAvailable userInfo:NULL];
        [self setNextStateError:notSupported complete:complete];
    }
}
- (void)acceptorFirstOctagonPacket:(NSDictionary *)indata reply:(NSMutableDictionary*)reply complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;
    NSDictionary *octagonData = indata[@"o"];

    if(![octagonData isKindOfClass:[NSDictionary class]]) {
        secnotice(pairingScope, "acceptorFirstOctagonPacket octagon data missing");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }

    NSDictionary *unpackedMessage = octagonData;

    if(![unpackedMessage[@"v"] isEqualToString:@"O"]){
        secnotice(pairingScope, "acceptorFirstOctagonPacket 'v' contents wrong");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }

    //handle epoch request and fetch epoch
    [self.otControl rpcEpochWithConfiguration:self.joiningConfiguration reply:^(uint64_t epoch, NSError * _Nullable error) {
        secnotice("pairing", "acceptor rpcEpochWithConfiguration: %ld (%@)", (long)epoch, error);
        if(error || self.testFailOctagon){
            secerror("error acceptor handling packet %d", self.counter);
            complete(true, nil, error);
        }else{
            secnotice(pairingScope, "acceptor handled packet %d", self.counter);
            self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                [weakSelf acceptorSecondPacket:nsdata complete:kscomplete];
            };
            OTPairingMessage *response = [[OTPairingMessage alloc] init];
            response.epoch = [[OTSponsorToApplicantRound1M2 alloc] init];
            response.epoch.epoch = epoch;
            reply[@"o"] = response.data;

            secnotice("pairing", "acceptor reply to packet 1");
            complete(false, reply, error);
        }
    }];
}

- (void)acceptorSecondPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;

    [self setNextStateError:NULL complete:NULL];

    secnotice("pairing", "acceptor packet 2");
    __block NSMutableDictionary *reply = [NSMutableDictionary dictionary];

    NSData *peerJoinBlob = indata[@"p"];

    if(self.sessionSupportsSOS && [peerJoinBlob isKindOfClass:[NSData class]]) {
        [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            complete(true, NULL, error);
        }] circleJoiningBlob:peerJoinBlob complete:^(NSData *blob, NSError *error){

            if (blob) {
                secnotice("pairing", "acceptor pairing complete (will send: %s): %@",
                          self.acceptorWillSendInitialSyncCredentials ? "YES" : "NO",
                          error);

                reply[@"b"] = blob;
            }
            if(self.sessionSupportsOctagon) {
                [self acceptorSecondOctagonPacket:indata reply:reply complete:complete];
            } else {
                secnotice("pairing", "posting kSOSCCCircleOctagonKeysChangedNotification");
                notify_post(kSOSCCCircleOctagonKeysChangedNotification);
                if (self.acceptorWillSendInitialSyncCredentials) {
                    self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                        [weakSelf acceptorThirdPacket:nsdata complete:kscomplete];
                    };

                    complete(false, reply, NULL);
                } else {
                    complete(true, reply, NULL);
                }
                secnotice("pairing", "acceptor reply to packet 2");
            }
        }];
    } else if(self.sessionSupportsOctagon){
        [self acceptorSecondOctagonPacket:indata reply:reply complete:complete];
    }
}

- (void)acceptorSecondOctagonPacket:(NSDictionary*)indata reply:(NSMutableDictionary*)reply complete:(KCPairingInternalCompletion)complete
{
    __weak typeof(self) weakSelf = self;
    NSData *octagonData = indata[@"o"];

    if(![octagonData isKindOfClass:[NSData class]]) {
        secnotice(pairingScope, "acceptorSecondOctagonPacket octagon data missing");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }

    OTPairingMessage *pairingMessage = [[OTPairingMessage alloc] initWithData:octagonData];
    if(!pairingMessage.hasPrepare){
        secerror("ot-pairing: acceptorSecondOctagonPacket: no octagon message");
        [self setNextStateError:[NSError errorWithDomain:kKCPairingChannelErrorDomain code:KCPairingErrorOctagonMessageMissing userInfo:NULL] complete:complete];
        return;
    }
    OTApplicantToSponsorRound2M1 *prepare = pairingMessage.prepare;

    //handle identity and fetch voucher
    [self.otControl rpcVoucherWithConfiguration:self.joiningConfiguration
                                         peerID:prepare.peerID
                                  permanentInfo:prepare.permanentInfo
                               permanentInfoSig:prepare.permanentInfoSig
                                     stableInfo:prepare.stableInfo
                                  stableInfoSig:prepare.stableInfoSig
                                          reply:^(NSData *voucher,
                                                  NSData *voucherSig,
                                                  NSError *error) {
        if(error || self.testFailOctagon){
            secerror("error acceptor handling octagon packet %d", self.counter);
            complete(true, nil, error);
            return;
        } else {
            bool finished = true;

            secnotice(pairingScope, "acceptor handled octagon packet %d", self.counter);
            if (OctagonPlatformSupportsSOS() && self.acceptorWillSendInitialSyncCredentials) {
                self.nextState = ^(NSDictionary *nsdata, KCPairingInternalCompletion kscomplete){
                    [weakSelf acceptorThirdPacket:nsdata complete:kscomplete];
                };
                finished = false;
            }
            OTPairingMessage *response = [[OTPairingMessage alloc] init];
            response.voucher = [[OTSponsorToApplicantRound2M2 alloc] init];
            response.voucher.voucher = voucher;
            response.voucher.voucherSignature = voucherSig;

            if (self.acceptorWillSendInitialSyncCredentials) {
                // no need to share TLKs over the pairing channel, that's provided by octagon
                self.acceptorInitialSyncCredentialsFlags &= ~(SOSControlInitialSyncFlagTLK | SOSControlInitialSyncFlagPCS);
            }

            reply[@"o"] = response.data;

            secnotice("pairing", "acceptor reply to packet 2");
            complete(finished ? true : false, reply, error);
        }
    }];
}

- (void)acceptorThirdPacket:(NSDictionary *)indata complete:(KCPairingInternalCompletion)complete
{
    secnotice("pairing", "acceptor packet 3");

    const uint32_t initialSyncCredentialsFlags = self.acceptorInitialSyncCredentialsFlags;

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        complete(true, NULL, error);
    }] initialSyncCredentials:initialSyncCredentialsFlags complete:^(NSArray *items, NSError *error2) {
        NSMutableDictionary *reply = [NSMutableDictionary dictionary];

        secnotice("pairing", "acceptor initialSyncCredentials complete: items %u: %@", (unsigned)[items count], error2);
        if (items) {
            reply[@"d"] = items;
        }
        secnotice("pairing", "acceptor reply to packet 3");
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
                SecADClientPushValueForDistributionKey((__bridge CFStringRef)key, [compressedData length]);
                secnotice("pairing", "pairing packet size %lu", (unsigned long)[compressedData length]);
            }
        }
        secnotice("pairing", "Exchange packet complete data: %@: %@", outdata ? @"YES" : @"NO", error);
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

- (void)setControlObject:(OTControl *)control
{
    self.otControl = control;
}

- (void)setConfiguration:(OTJoiningConfiguration *)config
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
