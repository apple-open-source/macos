#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSTransportMessage.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include "keychain/SecureObjectSync/SOSPeerCoder.h"
#include "keychain/SecureObjectSync/SOSEngine.h"
#import "keychain/SecureObjectSync/SOSPeerRateLimiter.h"
#import "keychain/SecureObjectSync/SOSPeerOTRTimer.h"
#include <utilities/SecADWrapper.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecPLWrappers.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"
#include "keychain/securityd/SecItemServer.h" // TODO: Remove this layer violation.

static const CFStringRef kSecSOSMessageRTT                   = CFSTR("com.apple.security.sos.messagertt");
static const CFStringRef kSecAccessGroupSecureBackupd        = CFSTR("com.apple.securebackupd");
static const CFStringRef kSecAccessGroupSBD                  = CFSTR("com.apple.sbd");
static const CFStringRef kSecAccessGroupCKKS                 = CFSTR("com.apple.security.ckks");

@class SOSMessage;

@implementation SOSMessage

@synthesize engine = engine;
@synthesize account = account;
@synthesize circleName = circleName;

-(id) initWithAccount:(SOSAccount*)acct andName:(NSString*)name
{
    self = [super init];
    if(self){
        SOSEngineRef e = SOSDataSourceFactoryGetEngineForDataSourceName(acct.factory, (__bridge CFStringRef)name, NULL);
        engine = e;
        account = acct;
        circleName = [[NSString alloc]initWithString:name];
    }
    return self;
}

-(CFTypeRef) SOSTransportMessageGetEngine
{
    return engine;
}

-(CFStringRef) SOSTransportMessageGetCircleName
{
    return (__bridge CFStringRef)(circleName);
}

-(CFIndex) SOSTransportMessageGetTransportType
{
    return kUnknown;
}

-(SOSAccount*) SOSTransportMessageGetAccount
{
    return account;
}

-(bool) SOSTransportMessageSendMessages:(SOSMessage*) transport pm:(CFDictionaryRef) peer_messages err:(CFErrorRef *)error
{
    return true;
}

-(bool) SOSTransportMessageFlushChanges:(SOSMessage*) transport err:(CFErrorRef *)error
{
    return true;
}

-(bool) SOSTransportMessageSyncWithPeers:(SOSMessage*) transport p:(CFSetRef) peers err:(CFErrorRef *)error{
    return true;
}
-(bool) SOSTransportMessageCleanupAfterPeerMessages:(SOSMessage*) transport peers:(CFDictionaryRef) peers err:(CFErrorRef*) error{
    return true;
}

-(bool) SOSTransportMessageSendMessage:(SOSMessage*) transport id:(CFStringRef) peerID messageToSend:(CFDataRef) message err:(CFErrorRef *)error
{
    CFDictionaryRef peerMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peerID, message, NULL);

    bool result = [self SOSTransportMessageSendMessages:transport pm:peerMessage err:error];

    CFReleaseNull(peerMessage);
    return result;
}

bool SOSEngineHandleCodedMessage(SOSAccount* account, SOSEngineRef engine, CFStringRef peerID, CFDataRef codedMessage, CFErrorRef*error) {
    __block bool result = true;
    __block bool somethingChanged = false;
    result &= SOSEngineWithPeerID(engine, peerID, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *shouldSave) {
        CFDataRef decodedMessage = NULL;
        enum SOSCoderUnwrapStatus uwstatus = SOSPeerHandleCoderMessage(peer, coder, peerID, codedMessage, &decodedMessage, shouldSave, error);
        NSMutableDictionary* attemptsPerPeer = (__bridge NSMutableDictionary*)SOSAccountGetValue(account, kSOSAccountRenegotiationRetryCount, NULL);
        //clear the max retry only if negotiation has finished and a counter exists
        if(coder != NULL && attemptsPerPeer != nil && !SOSCoderIsCoderInAwaitingState(coder) && ([attemptsPerPeer objectForKey:(__bridge NSString*)peerID] != NULL)){
            secnotice("otrtimer", "otr negotiation completed! clearing max retry counter");
            SOSPeerOTRTimerClearMaxRetryCount(account, (__bridge NSString*)peerID);
        }
        switch (uwstatus) {
            case SOSCoderUnwrapDecoded: {
                SOSMessageRef message =  NULL;
                if (decodedMessage && CFDataGetLength(decodedMessage)) {
                    // Only hand non empty messages to the engine, empty messages are an artifact
                    // of coder startup.
                    message = SOSMessageCreateWithData(kCFAllocatorDefault, decodedMessage, error);
                }
                if (message) {
                    bool engineHandleMessageDoesNotGetToRollbackTransactions = true;
                    result = SOSEngineHandleMessage_locked(engine, peerID, message, txn, &engineHandleMessageDoesNotGetToRollbackTransactions, &somethingChanged, error);
                    CFReleaseSafe(message);
                    if (!result) {
                        secnotice("engine", "Failed to handle message from peer %@: %@", peerID, error != NULL ? *error : NULL);
                    }
                } else {
                    secnotice("engine", "Failed to turn a data gram into an SOSMessage: %@", error != NULL ? *error : NULL);
                    result = SOSErrorCreate(KSOSCantParseSOSMessage, error, NULL, CFSTR("Failed to parse SOSMessage"));
                }
                break;
            }
            case SOSCoderUnwrapHandled: {
                secnotice("engine", "coder handled a negotiation message");
                result = true;
                break;
            }
            case SOSCoderUnwrapError:
            default: {
                secnotice("engine", "coder handled a error message: %d (error: %@)", (int)uwstatus, error != NULL ? *error : NULL);
                result = false;
                break;
            }
        }
        CFReleaseNull(decodedMessage);
    });

    if (somethingChanged) {
        SecKeychainChanged();
    }

    if (result) {
        SOSCCRequestSyncWithPeer(peerID);
    }

    return result;
}
-(CFDictionaryRef) SOSTransportMessageHandlePeerMessageReturnsHandledCopy:(SOSMessage*) transport peerMessages:(CFMutableDictionaryRef) circle_peer_messages_table err:(CFErrorRef *)error
{
    return CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
}

-(void) SOSTransportMessageCalculateNextTimer:(SOSAccount*)account rtt:(int)rtt peerid:(NSString*)peerid
{
    const int minRTT = 60;
    const int maxRTT = 3600;
    int newrtt = rtt *2;

    NSMutableDictionary *peerRTTs = (__bridge NSMutableDictionary*)SOSAccountGetValue(account,kSOSAccountPeerNegotiationTimeouts, NULL);
    if(!peerRTTs) {
        peerRTTs = [NSMutableDictionary dictionary];
    }

    // if we already have a longer rtt than requested then bail, unless it got excessive previously
    NSNumber *timeout = [peerRTTs objectForKey:peerid];
    if(timeout && (timeout.intValue >= rtt*2) && (newrtt <= maxRTT)) {
        return;
    }

    // make sure newrtt is a value between minRTT and maxRTT
    newrtt = MAX(newrtt, minRTT);
    newrtt = MIN(newrtt, maxRTT);

    NSNumber* newTimeout = [[NSNumber alloc] initWithInt: (newrtt)];
    [peerRTTs setObject:newTimeout forKey:peerid];
    secnotice("otrtimer", "peerID: %@ New OTR RTT: %d", peerid, newrtt);
    SOSAccountSetValue(account,kSOSAccountPeerNegotiationTimeouts, (__bridge CFMutableDictionaryRef)peerRTTs, NULL);
}

-(void) SOSTransportMessageUpdateRTTs:(NSString*)peerid
{
    SOSAccount* account = [self SOSTransportMessageGetAccount];
    NSMutableDictionary *peerToTimeLastSentDict = (__bridge NSMutableDictionary*)SOSAccountGetValue(account, kSOSAccountPeerLastSentTimestamp, NULL);
    if(peerToTimeLastSentDict){
        NSDate* storedDate = [peerToTimeLastSentDict objectForKey:peerid];
        if(storedDate){
            NSDate* currentDate = [NSDate date];
            int rtt = [currentDate timeIntervalSinceDate:storedDate];
            secnotice("otrtimer","peerID: %@ current date: %@, stored date: %@", peerid, currentDate, storedDate);
            secnotice("otrtimer", "rtt: %d", rtt);
            [self SOSTransportMessageCalculateNextTimer:account rtt:rtt peerid:peerid];
            
            SecADClientPushValueForDistributionKey(kSecSOSMessageRTT, rtt);
            [peerToTimeLastSentDict removeObjectForKey:peerid]; //remove last sent message date
            SOSAccountSetValue(account, kSOSAccountPeerLastSentTimestamp, (__bridge CFMutableDictionaryRef)peerToTimeLastSentDict, NULL);
        }
    }
}

-(bool) SOSTransportMessageHandlePeerMessage:(SOSMessage*) transport id:(CFStringRef) peer_id cm:(CFDataRef) codedMessage err:(CFErrorRef *)error
{
    [self SOSTransportMessageUpdateRTTs:(__bridge NSString*)peer_id];
    bool result = false;
    require_quiet(SecRequirementError(transport.engine != NULL, error, CFSTR("Missing engine")), done);
    result = SOSEngineHandleCodedMessage([transport SOSTransportMessageGetAccount], (SOSEngineRef)transport.engine, peer_id, codedMessage, error);
done:
    return result;
}

static PeerRateLimiter* getRateLimiter(SOSPeerRef peer)
{
    PeerRateLimiter *limiter = nil;
    
    if(!(limiter = (__bridge PeerRateLimiter*)SOSPeerGetRateLimiter(peer))){
        limiter = [[PeerRateLimiter alloc] initWithPeer:peer];
        SOSPeerSetRateLimiter(peer, (__bridge void*)limiter);
    }
    return limiter;
}

static void SOSPeerSetNextTimeToSend(PeerRateLimiter* limiter, int nextTimeToSync, NSString *accessGroup, SOSPeerRef peer, SOSMessage* transport, CFDataRef message_to_send)
{
    secnotice("ratelimit", "SOSPeerSetNextTimeToSend next time: %d", nextTimeToSync);
    
    __block dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, nextTimeToSync * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    secnotice("ratelimit", "SOSPeerSetNextTimeToSend next time to sync: %llu", (nextTimeToSync * NSEC_PER_SEC));
    
    [limiter.accessGroupToNextMessageToSend setObject:(__bridge NSData*)message_to_send forKey:accessGroup];
    
    CFStringRef peerid = CFRetainSafe(SOSPeerGetID(peer));
    CFStringRef accessGroupRetained = CFRetainSafe((__bridge CFStringRef)accessGroup);
    
    dispatch_source_set_event_handler(timer, ^{
        SOSCCPeerRateLimiterSendNextMessage_Server(peerid, accessGroupRetained);
    });
    
    dispatch_source_set_cancel_handler(timer, ^{
        CFReleaseSafe(peerid);
        CFReleaseSafe(accessGroupRetained);
    });
    
    dispatch_resume(timer);
    [limiter.accessGroupToTimer setObject:timer forKey:accessGroup];
}

static void setRateLimitingCounters(SOSAccount* account, PeerRateLimiter* limiter, NSString* attribute)
{
    CFErrorRef error = NULL;
    
    NSMutableDictionary* counters = (__bridge NSMutableDictionary*) SOSAccountGetValue(account, kSOSRateLimitingCounters, &error);
    
    if(!counters){
        counters = [[NSMutableDictionary alloc]init];
    }
    
    [counters setObject:[limiter diagnostics] forKey:attribute];
    SOSAccountSetValue(account, kSOSRateLimitingCounters, (__bridge CFDictionaryRef)counters, &error);
}
//figure out whether or not an access group should be judged, held back, or sent
static bool SOSPeerShouldSend(CFArrayRef attributes, SOSPeerRef peer, SOSMessage* transport, CFDataRef message_to_send)
{
    NSDate *date = [NSDate date];
    __block bool peerShouldSend = false;
    PeerRateLimiter *limiter = getRateLimiter(peer);
    __block NSMutableDictionary *powerlogPayload = nil;
    static NSMutableDictionary* attributeRateLimiting = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        attributeRateLimiting = [[NSMutableDictionary alloc] init];
    });

    if(!attributes){
        peerShouldSend = true;
    }
    else{
        secnotice("ratelimit", "number of attributes to review: %lu", CFArrayGetCount(attributes));
        [(__bridge NSArray*)attributes enumerateObjectsUsingBlock:^(NSString* attribute, NSUInteger idx, BOOL * _Nonnull stop) {
            enum RateLimitState state = [limiter stateForAccessGroup:attribute];

            switch(state){
                case RateLimitStateCanSend:
                {
                    NSDate *limit = nil;
                    KeychainItem *item = [[KeychainItem alloc] initWithAccessGroup: attribute];
                    NSInteger badness = [limiter judge:item at:date limitTime:&limit];
                    secnotice("ratelimit","accessGroup: %@, judged: %lu", attribute, (long)badness);
                    
                    NSNumber *currentBadness = attributeRateLimiting[attribute];
                    NSNumber *newBadness = @(badness);

                    if (![currentBadness isEqual:newBadness]) {
                        attributeRateLimiting[attribute] = newBadness;
                        if (powerlogPayload == NULL) {
                            powerlogPayload = [[NSMutableDictionary alloc] init];
                        }
                        powerlogPayload[attribute] = newBadness;
                    }

                    double delta = [limit timeIntervalSinceDate:date];
                    
                    if(delta > 0){
                        //setting counters for attribute being rate limited
                        setRateLimitingCounters([transport SOSTransportMessageGetAccount],limiter, attribute);
                        
                        secnotice("ratelimit", "setting a timer for next sync: %@", limit);
                        
                        SOSPeerSetNextTimeToSend(limiter, delta, attribute, peer, transport, message_to_send);
                        //set rate limit state to hold
                        [limiter.accessGroupRateLimitState setObject:[[NSNumber alloc]initWithLong:RateLimitStateHoldMessage] forKey:attribute];
                    }else{
                        peerShouldSend = true;
                    }
                }
                    break;
                case RateLimitStateHoldMessage:
                {
                    secnotice("ratelimit","access group: %@ is being rate limited", attribute);
                }
                    break;
                default:
                {
                    secnotice("ratelimit","no state for limiter for peer: %@", peer);
                }
            };
        }];
    }
    if ([powerlogPayload count]) {
        SecPLLogRegisteredEvent(@"SOSKVSRateLimitingEvent",
                                @{
                                    @"timestamp" : @([date timeIntervalSince1970]),
                                    @"peerShouldSend" : @(peerShouldSend),
                                    @"attributeBadness" : powerlogPayload
                                 });
    }
    return peerShouldSend;
}

//if a separate message containing a rate limited access group is going to be sent, make sure to send any pending messages first otherwise the OTR ordering will be wrong!
static void SOSTransportSendPendingMessage(CFArrayRef attributes, SOSMessage* transport, SOSPeerRef peer){
    PeerRateLimiter *limiter = getRateLimiter(peer);
    
    [(__bridge NSArray*)attributes enumerateObjectsUsingBlock:^(NSString* attribute, NSUInteger idx, BOOL * _Nonnull stop) {
        NSData* message = [limiter.accessGroupToNextMessageToSend objectForKey:attribute];
        if(message){
            CFErrorRef error = NULL;
            bool sendResult = [transport SOSTransportMessageSendMessage:transport id:SOSPeerGetID(peer) messageToSend:(__bridge CFDataRef)message err:&error];
            
            if(!sendResult || error){
                secnotice("ratelimit", "SOSTransportSendPendingMessage: could not send message: %@", error);
            }
            else{
                secnotice("ratelimit", "SOSTransportSendPendingMessage: sent pending message: %@ for access group: %@", message, attribute);
            }
            [limiter.accessGroupToNextMessageToSend removeObjectForKey:attribute];
            //cancel dispatch timer
            dispatch_source_t timer = [limiter.accessGroupToTimer objectForKey:attribute];
            if(timer)
                dispatch_cancel(timer);
            [limiter.accessGroupToTimer removeObjectForKey:attribute];
            [limiter.accessGroupRateLimitState setObject:[[NSNumber alloc]initWithLong:RateLimitStateCanSend] forKey:attribute];
        }
    }];
}

//update last time message sent for this peer
-(void) SOSTransportMessageUpdateLastMessageSentTimetstamp:(SOSAccount*)account peer:(SOSPeerRef)peer
{
    NSMutableDictionary *peerRTTs = (__bridge NSMutableDictionary*)SOSAccountGetValue(account, kSOSAccountPeerLastSentTimestamp, NULL);
    if(!peerRTTs){
        peerRTTs = [NSMutableDictionary dictionary];
    }
    if([peerRTTs objectForKey:(__bridge NSString*)SOSPeerGetID(peer) ] == nil){
        [peerRTTs setObject:[NSDate date] forKey:(__bridge NSString*)SOSPeerGetID(peer)];
        SOSAccountSetValue(account, kSOSAccountPeerLastSentTimestamp, (__bridge CFMutableDictionaryRef)peerRTTs, NULL);
    }
}
-(bool) SOSTransportMessageSendMessageIfNeeded:(SOSMessage*)transport
                                      circleName:(CFStringRef)circle_id
                                           pID:(CFStringRef)peer_id
                                           err:(CFErrorRef *)error
{
    __block bool ok = true;
    SOSAccount* account = transport.account;

    BOOL initialSyncDone = SOSAccountHasCompletedInitialSync(account);
    ok &= SOSEngineWithPeerID((SOSEngineRef)transport.engine, peer_id, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        // Now under engine lock do stuff
        CFDataRef message_to_send = NULL;
        SOSEnginePeerMessageSentCallback* sentCallback = NULL;
        CFMutableArrayRef attributes = NULL;
        ok = SOSPeerCoderSendMessageIfNeeded([transport SOSTransportMessageGetAccount],(SOSEngineRef)transport.engine, txn, peer, coder, &message_to_send, peer_id, &attributes, &sentCallback, error);
        secnotice("ratelimit","attribute list: %@", attributes);
        bool shouldSend = true;

        if(attributes == NULL){ //no attribute but still should be rate limited
            attributes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            CFArrayAppendValue(attributes, CFSTR("NoAttribute"));
        }

        if(!initialSyncDone){
            secnotice("ratelimit","not going to rate limit, currently in initial sync");
        }
        if(initialSyncDone && message_to_send){ //need to judge the message if not in initial sync
            secnotice("ratelimit","not in initial sync!");
            shouldSend = SOSPeerShouldSend(attributes, peer, transport, message_to_send);
           CFRange range = CFRangeMake(0, CFArrayGetCount(attributes));
            if(CFArrayContainsValue(attributes, range, kSecAccessGroupCKKS) ||
               CFArrayContainsValue(attributes, range, kSecAccessGroupSBD) ||
               CFArrayContainsValue(attributes, range, kSecAccessGroupSecureBackupd)){
                shouldSend = true;
            }

            secnotice("ratelimit","should send? : %@", shouldSend ? @"YES" : @"NO");
        }
        if (shouldSend && message_to_send) {
            SOSTransportSendPendingMessage(attributes, transport, peer);
            ok = ok && [transport SOSTransportMessageSendMessage:transport id:peer_id messageToSend:message_to_send err:error];

            SOSEngineMessageCallCallback(sentCallback, ok);

            [transport SOSTransportMessageUpdateLastMessageSentTimetstamp:account peer:peer];
            
        }else if(!shouldSend){
            secnotice("ratelimit", "peer is rate limited: %@", peer_id);
        }else{
            secnotice("transport", "no message to send to peer: %@", peer_id);
        }

        SOSEngineFreeMessageCallback(sentCallback);
        sentCallback = NULL;
        CFReleaseSafe(message_to_send);
        CFReleaseNull(attributes);

        *forceSaveState = ok;
    });

    return ok;
}

@end

