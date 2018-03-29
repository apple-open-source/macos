//
//  SOSPeerOTRTimer.m
//

#import <Foundation/Foundation.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCoder.h>
#include <Security/SecureObjectSync/SOSEngine.h>
#include <Security/SecureObjectSync/SOSDataSource.h>
#include <Security/SecureObjectSync/SOSAccountTransaction.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSPeerOTRTimer.h>
#include <Security/CKBridge/SOSCloudKeychainClient.h>

#include <utilities/SecADWrapper.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>
#include "SOSInternal.h"

//AGGD
NSString* const SecSOSAggdMaxRenegotiation   = @"com.apple.security.sos.otrrenegotiationmaxretries";

__unused static int initialOTRTimeoutValue = 60; //best round trip time in KVS plus extra for good measure
static int maxRetryCount = 7; //max number of times to attempt restarting OTR negotiation

bool SOSPeerOTRTimerHaveReachedMaxRetryAllowance(SOSAccount* account, NSString* peerid){
    bool reachedMax = false;
    NSMutableDictionary* attemptsPerPeer = (__bridge NSMutableDictionary*)SOSAccountGetValue(account, kSOSAccountRenegotiationRetryCount, NULL);
    if(!attemptsPerPeer){
        attemptsPerPeer = [NSMutableDictionary dictionary];
    }
    NSNumber* attempt = [attemptsPerPeer objectForKey:peerid];
    if(attempt && [attempt intValue] >= maxRetryCount)
    {
        reachedMax = true;
        SecADAddValueForScalarKey((__bridge CFStringRef) SecSOSAggdMaxRenegotiation,1);
    }
    return reachedMax;
}

//used when evaluating whether or not securityd should start a timer for negotiation
bool SOSPeerOTRTimerHaveAnRTTAvailable(SOSAccount* account, NSString* peerid)
{
    CFErrorRef error = NULL;
    CFNumberRef timeout = NULL;
    bool doesRTTExist = false;
    
    CFMutableDictionaryRef timeouts = (CFMutableDictionaryRef)asDictionary(SOSAccountGetValue(account, kSOSAccountPeerNegotiationTimeouts, &error), NULL);
    require_action_quiet(timeouts, exit, secnotice("otrtimer", "do not have an rtt yet"));
    
    timeout = CFDictionaryGetValue(timeouts, (__bridge CFStringRef)peerid);
    require_action_quiet(timeout, exit, secnotice("otrtimer", "do not have an rtt yet"));
    
    doesRTTExist = true;
exit:
    return doesRTTExist;
}

//call when a timer has fired, remove the current rtt entry as the existing one isn't working
void SOSPeerOTRTimerRemoveRTTTimeoutForPeer(SOSAccount* account, NSString* peerid)
{
    CFNumberRef timeout = NULL;
    CFErrorRef error = NULL;
    
    CFMutableDictionaryRef timeouts = (CFMutableDictionaryRef)asDictionary(SOSAccountGetValue(account, kSOSAccountPeerNegotiationTimeouts, &error), NULL);
    require_action_quiet(timeouts && (error == NULL), exit, secnotice("otrtimer","timeout dictionary doesn't exist"));
    
    timeout = CFDictionaryGetValue(timeouts, (__bridge CFStringRef)peerid);
    require_action_quiet(timeout, exit, secnotice("otrtimer","timeout for peerid: %@, doesn't exist", (__bridge CFStringRef)peerid));
    
    CFDictionaryRemoveValue(timeouts, (__bridge CFStringRef)peerid);
    SOSAccountSetValue(account, kSOSAccountPeerNegotiationTimeouts, timeouts, &error);
    if(error){
        secnotice("otrtimer","SOSAccountSetValue threw an error for key kSOSAccountPeerNegotiationTimeouts: %@", error);
    }
exit:
    CFReleaseNull(error);
}

void SOSPeerOTRTimerRemoveLastSentMessageTimestamp(SOSAccount* account, NSString* peerid)
{
    NSMutableDictionary *peerToTimeLastSentDict = (__bridge NSMutableDictionary*)SOSAccountGetValue(account, kSOSAccountPeerLastSentTimestamp, NULL);
    
    if(peerToTimeLastSentDict){
        NSDate* storedDate = [peerToTimeLastSentDict objectForKey:peerid];
        if(storedDate){
            [peerToTimeLastSentDict removeObjectForKey:peerid];
            SOSAccountSetValue(account, kSOSAccountPeerLastSentTimestamp, (__bridge CFMutableDictionaryRef)peerToTimeLastSentDict, NULL);
        }
    }
}

void SOSPeerOTRTimerIncreaseOTRNegotiationRetryCount(SOSAccount* account, NSString* peerid)
{
    NSMutableDictionary* attemptsPerPeer = (__bridge NSMutableDictionary*)SOSAccountGetValue(account, kSOSAccountRenegotiationRetryCount, NULL);
    if(!attemptsPerPeer){
        attemptsPerPeer = [NSMutableDictionary dictionary];
    }
    NSNumber* attempt = [attemptsPerPeer objectForKey:peerid];
    if(!attempt){
        attempt = [[NSNumber alloc] initWithInt:1];
        [attemptsPerPeer setObject:attempt forKey:peerid];
    }
    else{
        NSNumber* newCount = [[NSNumber alloc]initWithInt:([attempt intValue]+1)];
        [attemptsPerPeer setObject:newCount forKey:peerid];
        secnotice("otr","OTR negotiation retry count: %d", [newCount intValue]);
    }
    SOSAccountSetValue(account, kSOSAccountRenegotiationRetryCount, (__bridge CFMutableDictionaryRef)attemptsPerPeer, NULL);
}

int SOSPeerOTRTimerTimeoutValue(SOSAccount* account, SOSPeerRef peer)
{
    CFErrorRef error = NULL;
    int timeoutIntValue = 0;

    CFMutableDictionaryRef timeouts = (CFMutableDictionaryRef)asDictionary(SOSAccountGetValue(account, kSOSAccountPeerNegotiationTimeouts, &error), NULL);
    require_action_quiet(timeouts, xit, secnotice("otrtimer","deadline value not available yet"));

    CFNumberRef timeout = CFDictionaryGetValue(timeouts, SOSPeerGetID(peer));
    require_action_quiet(timeout, xit, secnotice("otrtimer","deadline value not available yet"));

    secnotice("otrtimer", "decided to wait %d before restarting negotiation", [(__bridge NSNumber*)timeout intValue]);
    timeoutIntValue = [(__bridge NSNumber*)timeout intValue];

xit:
    return timeoutIntValue;
}

void SOSPeerOTRTimerSetupAwaitingTimer(SOSAccount* account, SOSPeerRef peer, SOSEngineRef engine, SOSCoderRef coder)
{
    //check which timeout value to use
    int timeoutValue = SOSPeerOTRTimerTimeoutValue(account, peer);
    CFStringRef peerid = CFRetainSafe(SOSPeerGetID(peer));

    secnotice("otrtimer", "setting timer for peer: %@", peer);
    __block dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, timeoutValue * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    dispatch_source_set_event_handler(timer, ^{
        secnotice("otrtimer","otrTimerFired fired");
        SOSCCResetOTRNegotiation_Server(peerid);
        
    });
    
    dispatch_source_set_cancel_handler(timer, ^{
        CFReleaseSafe(peerid);
    });
    
    dispatch_resume(timer);
    
    SOSPeerSetOTRTimer(peer, timer);
}

//clear the max retry counter in the account object
void SOSPeerOTRTimerClearMaxRetryCount(SOSAccount* account, NSString* peerid)
{
    secnotice("otrtimer", "negotiation finished! clearing max retry counter");
    NSMutableDictionary* attemptsPerPeer = (__bridge NSMutableDictionary*)SOSAccountGetValue(account, kSOSAccountRenegotiationRetryCount, NULL);
    if(!attemptsPerPeer){
        attemptsPerPeer = [NSMutableDictionary dictionary];
    }
    [attemptsPerPeer removeObjectForKey:peerid];
    SOSAccountSetValue(account, kSOSAccountRenegotiationRetryCount, (__bridge CFMutableDictionaryRef)attemptsPerPeer, NULL);
}

