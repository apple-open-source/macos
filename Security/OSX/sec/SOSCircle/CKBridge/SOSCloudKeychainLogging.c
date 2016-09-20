//
//  SOSCloudKeychainLogging.c
//  sec
//
//  Created by Richard Murphy on 6/21/16.
//
//

#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>
//#include <syslog.h>
//#include <os/activity.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecXPCError.h>
#include "SOSCloudKeychainConstants.h"
#include "SOSCloudKeychainClient.h"
#include "SOSKVSKeys.h"
#include "SOSUserKeygen.h"
#include "SecOTRSession.h"
#include "SOSCloudKeychainLogging.h"


#define DATE_LENGTH 18

#define KVSLOGSTATE "kvsLogState"

static CFStringRef SOSCloudKVSCreateDateFromValue(CFDataRef valueAsData) {
    CFStringRef dateString = NULL;
    CFDataRef dateData = CFDataCreateCopyFromRange(kCFAllocatorDefault, valueAsData, CFRangeMake(0, DATE_LENGTH));
    require_quiet(dateData, retOut);
    dateString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, dateData, kCFStringEncodingUTF8);
    CFReleaseNull(dateData);
retOut:
    return dateString;
}

static CFDataRef SOSCloudKVSCreateDataFromValueAfterDate(CFDataRef valueAsData) {
    return CFDataCreateCopyFromPositions(kCFAllocatorDefault, valueAsData, DATE_LENGTH, CFDataGetLength(valueAsData));
}

static void SOSCloudKVSLogCircle(CFTypeRef key, CFStringRef dateString, CFTypeRef value) {
    if(!isData(value)) return;
    SOSCircleRef circle = SOSCircleCreateFromData(NULL, value, NULL);
    require_quiet(circle, retOut);
    secnotice(KVSLOGSTATE, "%@ %@:", key, dateString);
    SOSCircleLogState(KVSLOGSTATE, circle, NULL, NULL);
    CFReleaseSafe(circle);
retOut:
    return;
}

static void SOSCloudKVSLogLastCircle(CFTypeRef key, CFTypeRef value) {
    if(!isData(value)) return;
    CFStringRef circle = NULL;
    CFStringRef from = NULL;
    CFStringRef peerID = CFSTR("        ");
    bool parsed = SOSKVSKeyParse(kLastCircleKey, key, &circle, NULL, NULL, NULL, &from, NULL);
    if(parsed) {
        peerID = from;
    }
    CFStringRef speerID = CFStringCreateTruncatedCopy(peerID, 8);
    CFStringRef dateString = SOSCloudKVSCreateDateFromValue(value);
    CFDataRef circleData = SOSCloudKVSCreateDataFromValueAfterDate(value);
    CFStringRef keyPrefix = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ from %@: "), circle, speerID);
    SOSCloudKVSLogCircle(keyPrefix, dateString, circleData);
    CFReleaseNull(keyPrefix);
    CFReleaseNull(speerID);
    CFReleaseNull(from);
    CFReleaseNull(dateString);
    CFReleaseNull(circleData);
}

static void SOSCloudKVSLogKeyParameters(CFTypeRef key, CFStringRef dateString, CFTypeRef value) {
    if(!isData(value)) return;
    CFStringRef keyParameterDescription = UserParametersDescription(value);
    if(!keyParameterDescription) keyParameterDescription = CFDataCopyHexString(value);
    secnotice(KVSLOGSTATE, "%@: %@: %@", key, dateString, keyParameterDescription);
    CFReleaseNull(keyParameterDescription);
}

static void SOSCloudKVSLogLastKeyParameters(CFTypeRef key, CFTypeRef value) {
    if(!isData(value)) return;
    CFStringRef from = NULL;
    CFStringRef peerID = CFSTR("        ");
    bool parsed = SOSKVSKeyParse(kLastKeyParameterKey, key, NULL, NULL, NULL, NULL, &from, NULL);
    if(parsed) {
        peerID = from;
    }
    CFStringRef speerID = CFStringCreateTruncatedCopy(peerID, 8);
    CFDataRef keyParameterData = SOSCloudKVSCreateDataFromValueAfterDate(value);
    CFStringRef dateString = SOSCloudKVSCreateDateFromValue(value);
    CFStringRef keyPrefix = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("k%@ from %@: "), kSOSKVSKeyParametersKey, speerID);

    SOSCloudKVSLogKeyParameters(keyPrefix, dateString, keyParameterData);
    CFReleaseNull(keyPrefix);
    CFReleaseNull(speerID);
    CFReleaseNull(dateString);
    CFReleaseNull(from);
    CFReleaseNull(keyParameterData);
}

static void SOSCloudKVSLogMessage(CFTypeRef key, CFTypeRef value) {
    CFStringRef circle = NULL;
    CFStringRef from = NULL;
    CFStringRef to = NULL;
    bool parsed = SOSKVSKeyParse(kMessageKey, key, &circle, NULL, NULL, NULL, &from, &to);
    if(parsed) {
        CFStringRef sfrom = CFStringCreateTruncatedCopy(from, 8);
        CFStringRef sto = CFStringCreateTruncatedCopy(to, 8);
        if(isData(value)){
            const char* messageType = SecOTRPacketTypeString(value);
            secnotice(KVSLOGSTATE, "message packet from: %@ to: %@ : %s: %ld", sfrom, sto, messageType, CFDataGetLength(value));
        } else {
            secnotice(KVSLOGSTATE, "message packet from: %@ to: %@: %@", sfrom, sto, value);
        }
        CFReleaseNull(sfrom);
        CFReleaseNull(sto);
    } else {
        secnotice(KVSLOGSTATE, "%@: %@", key, value);
    }
    CFReleaseNull(circle);
    CFReleaseNull(from);
    CFReleaseNull(to);
}

static void SOSCloudKVSLogRetirement(CFTypeRef key, CFTypeRef value) {
    CFStringRef circle = NULL;
    CFStringRef from = NULL;
    bool parsed = SOSKVSKeyParse(kRetirementKey, key, &circle, NULL, NULL, NULL, &from, NULL);
    if(parsed) {
        CFStringRef sfrom = CFStringCreateTruncatedCopy(from, 8);
        secnotice(KVSLOGSTATE, "Retired Peer: %@, from Circle: %@", sfrom, circle);
        CFReleaseNull(sfrom);
    } else {
        secnotice(KVSLOGSTATE, "Retired Peer format unknown - %@", key);
    }
    CFReleaseNull(circle);
    CFReleaseNull(from);
}

static void SOSCloudKVSLogKeyType(CFTypeRef key, CFTypeRef value, SOSKVSKeyType type){
    switch (type) {
        case kCircleKey:
            SOSCloudKVSLogCircle(key, CFSTR("     Current     "), value);
            break;
        case kRetirementKey:
            SOSCloudKVSLogRetirement(key, value);
            break;
        case kMessageKey:
            SOSCloudKVSLogMessage(key, value);
            break;
        case kParametersKey:
            SOSCloudKVSLogKeyParameters(key, CFSTR("     Current     "), value);
            break;
        case kLastKeyParameterKey:
            SOSCloudKVSLogLastKeyParameters(key, value);
            break;
        case kLastCircleKey:
            SOSCloudKVSLogLastCircle(key, value);
            break;
        case kInitialSyncKey:
        case kAccountChangedKey:
        case kDebugInfoKey:
        case kRingKey:
        case kPeerInfoKey:
        default:
            break;
    }
}

void SOSCloudKVSLogState(void) {
    static int ordering[] = {
        kParametersKey,
        kLastKeyParameterKey,
        kCircleKey,
        kLastCircleKey,
        kRetirementKey,
        kMessageKey,
        kInitialSyncKey,
        kAccountChangedKey,
        kDebugInfoKey,
        kRingKey,
        kPeerInfoKey,
        kUnknownKey,
    };
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, 10ull * NSEC_PER_SEC);
    static volatile bool inUse = false; // Don't let log attempts stack

    if(!inUse) {
        inUse = true;
        dispatch_retain(waitSemaphore);
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                CFDictionaryRef kvsDictionary = SOSCloudCopyKVSState();
                if(kvsDictionary){
                    secnotice(KVSLOGSTATE, "Start");
                    // if we have anything to log - do it here.
                    for (size_t i = 0; i < (sizeof(ordering) / sizeof(SOSKVSKeyType)); i++){
                        CFDictionaryForEach(kvsDictionary, ^(const void *key, const void *value) {
                            if(SOSKVSKeyGetKeyType(key) == ordering[i]){
                                SOSCloudKVSLogKeyType(key, value, ordering[i]);
                            }
                        });
                    }
                    secnotice(KVSLOGSTATE, "Finish");
                    CFReleaseNull(kvsDictionary);
                } else{
                    secnotice(KVSLOGSTATE, "dictionary from KVS is NULL");
                }

            inUse=false;
            dispatch_semaphore_signal(waitSemaphore);
            dispatch_release(waitSemaphore);
        });
    }

    dispatch_semaphore_wait(waitSemaphore, finishTime);
    dispatch_release(waitSemaphore);
    
}

