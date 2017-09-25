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


static const CFStringRef OTRTimeoutsPerPeer = CFSTR("OTRTimeoutsPerPeer");
static const CFStringRef OTRConfigVersion = CFSTR("OTRConfigVersion");

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

static CFNumberRef SOSPeerOTRTimerCopyConfigVersion(SOSAccount* account)
{
    uint64_t v = 0;
    CFNumberRef versionFromAccount = NULL;
    CFNumberRef version = (CFNumberRef)SOSAccountGetValue(account, OTRConfigVersion, NULL);
    
    if(!version){
        versionFromAccount = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &v);
        SOSAccountSetValue(account, OTRConfigVersion, versionFromAccount, NULL);
    }
    else{
        versionFromAccount = CFRetainSafe(version);
    }
    return versionFromAccount;
}

void SOSPeerOTRTimerCreateKVSConfigDict(SOSAccount* account, CFNumberRef timeout, CFStringRef peerid)
{
    CFMutableDictionaryRef peerToTimeout = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(peerToTimeout, peerid, timeout);
    
    CFNumberRef versionFromAccount = SOSPeerOTRTimerCopyConfigVersion(account);
    
    CFMutableDictionaryRef peerTimeOutsAndVersion = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(peerTimeOutsAndVersion, OTRTimeoutsPerPeer, peerToTimeout);
    CFDictionaryAddValue(peerTimeOutsAndVersion, OTRConfigVersion, versionFromAccount);
    
    CFMutableDictionaryRef myPeerChanges = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFStringRef myPeerID = (__bridge CFStringRef) account.peerID;
    CFDictionaryAddValue(myPeerChanges, myPeerID, peerTimeOutsAndVersion);
    
    CFMutableDictionaryRef otrConfig = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(otrConfig, kSOSKVSOTRConfigVersion, myPeerChanges);
    
    SOSCloudKeychainPutObjectsInCloud(otrConfig, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
        }
    });
    secnotice("kvsconfig", "submitting config to KVS: %@", otrConfig);
    CFReleaseNull(myPeerChanges);
    CFReleaseNull(peerToTimeout);
    CFReleaseNull(versionFromAccount);
    CFReleaseNull(otrConfig);
}

//grab existing key from KVS
__unused __unused static CFDictionaryRef SOSPeerOTRTimerCopyConfigFromKVS()
{
    CFErrorRef error = NULL;
    __block CFTypeRef object = NULL;
    
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    const uint64_t maxTimeToWaitInSeconds = 5ull * NSEC_PER_SEC;
    
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);
    
    SOSCloudKeychainGetAllObjectsFromCloud(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef block_error) {
        secnotice("otrtimer", "SOSCloudKeychainGetObjectsFromCloud returned: %@", returnedValues);
        object = returnedValues;
        if (object)
            CFRetain(object);
        if (error)
        {
            secerror("SOSCloudKeychainGetObjectsFromCloud returned error: %@", error);
        }
        secnotice("otrtimer", "SOSCloudKeychainGetObjectsFromCloud block exit: %@", object);
        dispatch_semaphore_signal(waitSemaphore);
    });
    
    dispatch_semaphore_wait(waitSemaphore, finishTime);
    if (object && (CFGetTypeID(object) == CFNullGetTypeID()))   // return a NULL instead of a CFNull
    {
        CFRelease(object);
        object = NULL;
        return NULL;
    }
    if(isDictionary(object))
    {
        return CFRetainSafe((CFDictionaryRef)object);
    }
    return NULL;
}

__unused static bool SOSPeerOTRTimerShouldWriteConfig(CFDictionaryRef config, CFStringRef myID, CFStringRef peerid, CFNumberRef currentConfigVersion, CFNumberRef localTimeout)
{
    bool result = true;
    secnotice("otrtimer", "grabbed config from KVS: %@", config);
    CFDictionaryRef myPeerConfigSettings = NULL;
    CFNumberRef versionFromKVS = NULL;
    CFDictionaryRef peerToTimeouts = NULL;
    CFNumberRef timeoutInKVS = NULL;
    CFDictionaryRef otrConfig = NULL;
    
    require_action_quiet(currentConfigVersion, fail, secnotice("otrtimer","current config version is null"));
    require_action_quiet(localTimeout, fail, secnotice("otrtimer", "local timeout is null"));
    
    otrConfig = CFDictionaryGetValue(config, kSOSKVSOTRConfigVersion);
    require_action_quiet(otrConfig, fail, secnotice("otrtimer","otr config does not exist"));
    
    myPeerConfigSettings = CFDictionaryGetValue(otrConfig, myID);
    require_action_quiet(myPeerConfigSettings, fail, secnotice("otrtimer","my peer config settings dictionary is null"));
    
    versionFromKVS = CFDictionaryGetValue(myPeerConfigSettings, OTRConfigVersion);
    require_action_quiet(versionFromKVS, fail, secnotice("otrtimer", "version from KVS is null"));
    
    peerToTimeouts = CFDictionaryGetValue(myPeerConfigSettings, OTRTimeoutsPerPeer);
    require_action_quiet(peerToTimeouts, fail, secnotice("otrtimer", "dictionary of peerids and timeout values is null"));
    
    timeoutInKVS = CFDictionaryGetValue(peerToTimeouts, peerid);
    require_action_quiet(timeoutInKVS, fail, secnotice("otrtimer", "timeout value from kvs is null"));
    
    if(kCFCompareEqualTo == CFNumberCompare(currentConfigVersion, versionFromKVS, NULL) &&
       (CFNumberCompare(timeoutInKVS, localTimeout, NULL) == kCFCompareEqualTo)){
        secnotice("otrtimer", "versions match, can write new config");
    }else if(CFNumberCompare(versionFromKVS, currentConfigVersion, NULL) == kCFCompareGreaterThan){
        result = false;
        secnotice("otrtimer", "versions do not match, cannot write a new config");
    }else{
        secnotice("otrtimer", "config versions match, going to write current configuration of peerids to timeouts to KVS");
    }
    
fail:
    return result;
    
}

__unused static CFNumberRef SOSPeerOTRTimerCopyOTRConfigVersionFromAccount(SOSAccount* account)
{
    CFNumberRef version = SOSAccountGetValue(account, OTRConfigVersion, NULL);
    if(!version){
        uint64_t v = 0;
        version = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &v);
        SOSAccountSetValue(account, OTRConfigVersion, version, NULL);
    }else{
        return CFRetainSafe(version);
    }
    
    return version;
}

__unused static bool SOSPeerOTRTimerShouldUseTimeoutValueFromKVS(CFDictionaryRef otrConfigFromKVS, CFStringRef myID, CFNumberRef localConfigVersion){
    bool shouldUseTimeoutFromKVS = false;
    CFDictionaryRef otrConfig = NULL;
    CFDictionaryRef myPeerConfigSettings = NULL;
    CFNumberRef versionFromKVS = NULL;
    
    require_action_quiet(otrConfigFromKVS, xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    otrConfig = CFDictionaryGetValue(otrConfigFromKVS, kSOSKVSOTRConfigVersion);
    require_action_quiet(otrConfig, xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    myPeerConfigSettings = CFDictionaryGetValue(otrConfig, myID);
    require_action_quiet(myPeerConfigSettings, xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    versionFromKVS = CFDictionaryGetValue(myPeerConfigSettings, OTRConfigVersion);
    require_action_quiet(versionFromKVS && (CFGetTypeID(versionFromKVS) != CFNullGetTypeID()), xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    if(CFNumberCompare(versionFromKVS, localConfigVersion, NULL) == kCFCompareGreaterThan){
        secnotice("otrtimer", "should use timeout from kvs");
        shouldUseTimeoutFromKVS = true;
    }
    
xit:
    return shouldUseTimeoutFromKVS;
}

__unused static CFNumberRef SOSPeerOTRTimerTimeoutFromKVS(CFDictionaryRef otrConfigFromKVS, CFStringRef myID, CFStringRef peerID)
{
    CFNumberRef timeout = NULL;
    CFDictionaryRef otrConfig = NULL;
    CFDictionaryRef myPeerConfigSettings = NULL;
    CFDictionaryRef peerToTimeoutDictionary = NULL;
    
    require_action_quiet(otrConfigFromKVS, xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    otrConfig = CFDictionaryGetValue(otrConfigFromKVS, kSOSKVSOTRConfigVersion);
    require_action_quiet(otrConfig, xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    myPeerConfigSettings = CFDictionaryGetValue(otrConfig, myID);
    require_action_quiet(myPeerConfigSettings, xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    peerToTimeoutDictionary = CFDictionaryGetValue(myPeerConfigSettings, OTRTimeoutsPerPeer);
    require_action_quiet(peerToTimeoutDictionary, xit, secnotice("otrtimer", "configuration file from kvs does not exist"));
    
    timeout = CFDictionaryGetValue(peerToTimeoutDictionary, peerID);
xit:
    return timeout;
}

int SOSPeerOTRTimerTimeoutValue(SOSAccount* account, SOSPeerRef peer)
{
    CFErrorRef error = NULL;
    //bool shouldWriteConfig = true;
    //bool shouldUseTimeoutFromKVS = false;
    int timeoutIntValue = 0;
    
    //CFDictionaryRef configFromKVS = SOSPeerOTRTimerCopyConfigFromKVS();
    
    //CFNumberRef configVersion = SOSPeerOTRTimerCopyOTRConfigVersionFromAccount(account);
    //shouldUseTimeoutFromKVS = SOSPeerOTRTimerShouldUseTimeoutValueFromKVS(configFromKVS, (__bridge CFStringRef)account.peerID,configVersion);
    //CFReleaseNull(configVersion);
    
    //if(shouldUseTimeoutFromKVS){
      //  secnotice("otrtimer", "using timeout from kvs");
        //CFNumberRef timeoutFromKVS = SOSPeerOTRTimerTimeoutFromKVS(configFromKVS, (__bridge CFStringRef)account.peerID, //SOSPeerGetID(peer));
        //CFReleaseNull(configFromKVS);
        //return [(__bridge NSNumber*)timeoutFromKVS intValue];
   // }
    
    CFMutableDictionaryRef timeouts = (CFMutableDictionaryRef)asDictionary(SOSAccountGetValue(account, kSOSAccountPeerNegotiationTimeouts, &error), NULL);
    require_action_quiet(timeouts, xit, secnotice("otrtimer","deadline value not available yet"));
    
    CFNumberRef timeout = CFDictionaryGetValue(timeouts, SOSPeerGetID(peer));
    require_action_quiet(timeout, xit, secnotice("otrtimer","deadline value not available yet"));
    
    secnotice("otrtimer", "decided to wait %d before restarting negotiation", [(__bridge NSNumber*)timeout intValue]);
    timeoutIntValue = [(__bridge NSNumber*)timeout intValue];
    
    //CFNumberRef localConfigVersion = SOSPeerOTRTimerCopyOTRConfigVersionFromAccount(account);
    /*
    if(localConfigVersion){
        shouldWriteConfig = SOSPeerOTRTimerShouldWriteConfig(configFromKVS, (__bridge CFStringRef)account.peerID, SOSPeerGetID(peer), localConfigVersion, timeout);
    }
    
    if(shouldWriteConfig)
        SOSPeerOTRTimerCreateKVSConfigDict(account, timeout, SOSPeerGetID(peer));
    */
    
xit:
   // CFReleaseNull(configVersion);
    //CFReleaseNull(localConfigVersion);
    //CFReleaseNull(configFromKVS);
    
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

