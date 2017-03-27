//
//  accountCirclesViewsPrint.c
//  Security
//
//  Created by Richard Murphy on 12/8/16.
//
//

#include "accountCirclesViewsPrint.h"

//
//  SOSSysdiagnose.c
//  sec
//
//  Created by Richard Murphy on 1/27/16.
//
//


#include "SOSCloudCircleInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <time.h>
#include <notify.h>
#include <pwd.h>

#include <Security/SecItem.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <securityd/SOSCloudCircleServer.h>
#include <Security/SecOTRSession.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include <SecurityTool/readline.h>

#include "keychain_log.h"
#include "secToolFileIO.h"
#include "secViewDisplay.h"


#include <Security/SecPasswordGenerate.h>

/* Copied from CFPriv.h */
// #include <CoreFoundation/CFPriv.h>

CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT const CFStringRef _kCFSystemVersionProductNameKey;
CF_EXPORT const CFStringRef _kCFSystemVersionProductVersionKey;
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;

#define MAXKVSKEYTYPE kUnknownKey
#define DATE_LENGTH 18

#include <utilities/SecCFWrappers.h>


static const char *getSOSCCStatusDescription(SOSCCStatus ccstatus)
{
    switch (ccstatus)
    {
        case kSOSCCInCircle:        return "In Circle";
        case kSOSCCNotInCircle:     return "Not in Circle";
        case kSOSCCRequestPending:  return "Request pending";
        case kSOSCCCircleAbsent:    return "Circle absent";
        case kSOSCCError:           return "Circle error";
            
        default:
            return "<unknown ccstatus>";
            break;
    }
}

static void printPeerInfos(char *label, CFArrayRef (^getArray)(CFErrorRef *error)) {
    CFErrorRef error = NULL;
    CFArrayRef ppi = getArray(&error);
    SOSPeerInfoRef me = SOSCCCopyMyPeerInfo(NULL);
    CFStringRef mypeerID = SOSPeerInfoGetPeerID(me);
    
    if(ppi) {
        printmsg(CFSTR("%s count: %ld\n"), label, (long)CFArrayGetCount(ppi));
        CFArrayForEach(ppi, ^(const void *value) {
            char buf[160];
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFIndex version = SOSPeerInfoGetVersion(peer);
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            CFStringRef devtype = SOSPeerInfoGetPeerDeviceType(peer);
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            CFStringRef transportType = CFSTR("KVS");
            CFStringRef deviceID = CFSTR("");
            CFDictionaryRef gestalt = SOSPeerInfoCopyPeerGestalt(peer);
            CFStringRef osVersion = CFDictionaryGetValue(gestalt, CFSTR("OSVersion"));
            CFReleaseNull(gestalt);
            
            
            if(version >= 2){
                CFDictionaryRef v2Dictionary = peer->v2Dictionary;
                transportType = CFDictionaryGetValue(v2Dictionary, sTransportType);
                deviceID = CFDictionaryGetValue(v2Dictionary, sDeviceID);
            }
            char *pname = CFStringToCString(peerName);
            char *dname = CFStringToCString(devtype);
            char *tname = CFStringToCString(transportType);
            char *iname = CFStringToCString(deviceID);
            char *osname = CFStringToCString(osVersion);
            const char *me = CFEqualSafe(mypeerID, peerID) ? "me>" : "   ";
            
            
            snprintf(buf, 160, "%s %s: %-16s %-16s %-16s %-16s", me, label, pname, dname, tname, iname);
            
            free(pname);
            free(dname);
            CFStringRef pid = SOSPeerInfoGetPeerID(peer);
            CFIndex vers = SOSPeerInfoGetVersion(peer);
            printmsg(CFSTR("%s %@ V%d OS:%s\n"), buf, pid, vers, osname);
            free(osname);
        });
    } else {
        printmsg(CFSTR("No %s, error: %@\n"), label, error);
    }
    CFReleaseNull(ppi);
    CFReleaseNull(error);
}

void SOSCCDumpCircleInformation()
{
    CFErrorRef error = NULL;
    CFArrayRef generations = NULL;
    bool is_user_public_trusted = false;
    __block int count = 0;
    
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    printmsg(CFSTR("ccstatus: %s (%d)\n"), getSOSCCStatusDescription(ccstatus), ccstatus);
    
    is_user_public_trusted = SOSCCValidateUserPublic(&error);
    if(is_user_public_trusted)
        printmsg(CFSTR("Account user public is trusted%@"),CFSTR("\n"));
    else
        printmsg(CFSTR("Account user public is not trusted error:(%@)\n"), error);
    CFReleaseNull(error);
    
    generations = SOSCCCopyGenerationPeerInfo(&error);
    if(generations) {
        CFArrayForEach(generations, ^(const void *value) {
            count++;
            if(count%2 == 0)
                printmsg(CFSTR("Circle name: %@, "),value);
            
            if(count%2 != 0) {
                CFStringRef genDesc = SOSGenerationCountCopyDescription(value);
                printmsg(CFSTR("Generation Count: %@"), genDesc);
                CFReleaseNull(genDesc);
            }
            printmsg(CFSTR("%s\n"), "");
        });
    } else {
        printmsg(CFSTR("No generation count: %@\n"), error);
    }
    CFReleaseNull(generations);
    CFReleaseNull(error);
    
    printPeerInfos("     Peers", ^(CFErrorRef *error) { return SOSCCCopyValidPeerPeerInfo(error); });
    printPeerInfos("   Invalid", ^(CFErrorRef *error) { return SOSCCCopyNotValidPeerPeerInfo(error); });
    printPeerInfos("   Retired", ^(CFErrorRef *error) { return SOSCCCopyRetirementPeerInfo(error); });
    printPeerInfos("    Concur", ^(CFErrorRef *error) { return SOSCCCopyConcurringPeerPeerInfo(error); });
    printPeerInfos("Applicants", ^(CFErrorRef *error) { return SOSCCCopyApplicantPeerInfo(error); });
    
    CFReleaseNull(error);
}




/* KVS Dumping Support for iCloud Keychain */

static CFTypeRef getObjectsFromCloud(CFArrayRef keysToGet, dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    __block CFTypeRef object = NULL;
    
    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);
    
    dispatch_group_enter(dgroup);
    
    CloudKeychainReplyBlock replyBlock =
    ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        secinfo("sync", "SOSCloudKeychainGetObjectsFromCloud returned: %@", returnedValues);
        object = returnedValues;
        if (object)
            CFRetain(object);
        if (error)
        {
            secerror("SOSCloudKeychainGetObjectsFromCloud returned error: %@", error);
        }
        dispatch_group_leave(dgroup);
        secinfo("sync", "SOSCloudKeychainGetObjectsFromCloud block exit: %@", object);
        dispatch_semaphore_signal(waitSemaphore);
    };
    
    if (!keysToGet)
        SOSCloudKeychainGetAllObjectsFromCloud(processQueue, replyBlock);
    else
        SOSCloudKeychainGetObjectsFromCloud(keysToGet, processQueue, replyBlock);
    
    dispatch_semaphore_wait(waitSemaphore, finishTime);
    dispatch_release(waitSemaphore);
    if (object && (CFGetTypeID(object) == CFNullGetTypeID()))   // return a NULL instead of a CFNull
    {
        CFRelease(object);
        object = NULL;
    }
    secerror("returned: %@", object);
    return object;
}

static CFStringRef printFullDataString(CFDataRef data){
    __block CFStringRef fullData = NULL;
    
    BufferPerformWithHexString(CFDataGetBytePtr(data), CFDataGetLength(data), ^(CFStringRef dataHex) {
        fullData = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), dataHex);
    });
    
    return fullData;
}

static void displayLastKeyParameters(CFTypeRef key, CFTypeRef value)
{
    CFDataRef valueAsData = asData(value, NULL);
    if(valueAsData){
        CFDataRef dateData = CFDataCreateCopyFromRange(kCFAllocatorDefault, valueAsData, CFRangeMake(0, DATE_LENGTH));
        CFDataRef keyParameterData = CFDataCreateCopyFromPositions(kCFAllocatorDefault, valueAsData, DATE_LENGTH, CFDataGetLength(valueAsData));
        CFStringRef dateString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, dateData, kCFStringEncodingUTF8);
        CFStringRef keyParameterDescription = UserParametersDescription(keyParameterData);
        if(keyParameterDescription)
            printmsg(CFSTR("%@: %@: %@\n"), key, dateString, keyParameterDescription);
        else
            printmsg(CFSTR("%@: %@\n"), key, printFullDataString(value));
        CFReleaseNull(dateString);
        CFReleaseNull(keyParameterData);
        CFReleaseNull(dateData);
        CFReleaseNull(keyParameterDescription);
    }
    else{
        printmsg(CFSTR("%@: %@\n"), key, value);
    }
}

static void displayKeyParameters(CFTypeRef key, CFTypeRef value)
{
    if(isData(value)){
        CFStringRef keyParameterDescription = UserParametersDescription((CFDataRef)value);
        
        if(keyParameterDescription)
            printmsg(CFSTR("%@: %@\n"), key, keyParameterDescription);
        else
            printmsg(CFSTR("%@: %@\n"), key, value);
        
        CFReleaseNull(keyParameterDescription);
    }
    else{
        printmsg(CFSTR("%@: %@\n"), key, value);
    }
}

static void displayLastCircle(CFTypeRef key, CFTypeRef value)
{
    CFDataRef valueAsData = asData(value, NULL);
    if(valueAsData){
        CFErrorRef localError = NULL;
        
        CFDataRef dateData = CFDataCreateCopyFromRange(kCFAllocatorDefault, valueAsData, CFRangeMake(0, DATE_LENGTH));
        CFDataRef circleData = CFDataCreateCopyFromPositions(kCFAllocatorDefault, valueAsData, DATE_LENGTH, CFDataGetLength(valueAsData));
        CFStringRef dateString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, dateData, kCFStringEncodingUTF8);
        SOSCircleRef circle = SOSCircleCreateFromData(NULL, (CFDataRef) circleData, &localError);
        
        if(circle){
            CFIndex size = 5;
            CFNumberRef idLength = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &size);
            CFDictionaryRef format = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, CFSTR("SyncD"), CFSTR("SyncD"), CFSTR("idLength"), idLength, NULL);
            printmsgWithFormatOptions(format, CFSTR("%@: %@: %@\n"), key, dateString, circle);
            CFReleaseNull(idLength);
            CFReleaseNull(format);
            
        }
        else
            printmsg(CFSTR("%@: %@\n"), key, printFullDataString(circleData));
        
        CFReleaseNull(dateString);
        CFReleaseNull(circleData);
        CFReleaseSafe(circle);
        CFReleaseNull(dateData);
        CFReleaseNull(localError);
    }
    else{
        printmsg(CFSTR("%@: %@\n"), key, value);
    }
}

static void displayCircle(CFTypeRef key, CFTypeRef value)
{
    CFDataRef circleData = (CFDataRef)value;
    
    CFErrorRef localError = NULL;
    if (isData(circleData))
    {
        CFIndex size = 5;
        CFNumberRef idLength = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &size);
        CFDictionaryRef format = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, CFSTR("SyncD"), CFSTR("SyncD"), CFSTR("idLength"), idLength, NULL);
        SOSCircleRef circle = SOSCircleCreateFromData(NULL, circleData, &localError);
        printmsgWithFormatOptions(format, CFSTR("%@: %@\n"), key, circle);
        CFReleaseSafe(circle);
        CFReleaseNull(idLength);
        CFReleaseNull(format);
        
    }
    else
        printmsg(CFSTR("%@: %@\n"), key, value);
}

static void displayMessage(CFTypeRef key, CFTypeRef value)
{
    CFDataRef message = (CFDataRef)value;
    if(isData(message)){
        const char* messageType = SecOTRPacketTypeString(message);
        printmsg(CFSTR("%@: %s: %ld\n"), key, messageType, CFDataGetLength(message));
    }
    else
        printmsg(CFSTR("%@: %@\n"), key, value);
}

static void printEverything(CFTypeRef objects)
{
    CFDictionaryForEach(objects, ^(const void *key, const void *value) {
        if (isData(value))
        {
            printmsg(CFSTR("%@: %@\n\n"), key, printFullDataString(value));
        }
        else
            printmsg(CFSTR("%@: %@\n"), key, value);
    });
    
}

static void decodeForKeyType(CFTypeRef key, CFTypeRef value, SOSKVSKeyType type){
    switch (type) {
        case kCircleKey:
            displayCircle(key, value);
            break;
        case kRetirementKey:
        case kMessageKey:
            displayMessage(key, value);
            break;
        case kParametersKey:
            displayKeyParameters(key, value);
            break;
        case kLastKeyParameterKey:
            displayLastKeyParameters(key, value);
            break;
        case kLastCircleKey:
            displayLastCircle(key, value);
            break;
        case kInitialSyncKey:
        case kAccountChangedKey:
        case kDebugInfoKey:
        case kRingKey:
        case kPeerInfoKey:
        default:
            printmsg(CFSTR("%@: %@\n"), key, value);
            break;
    }
}

static void decodeAllTheValues(CFTypeRef objects){
    SOSKVSKeyType keyType = 0;
    __block bool didPrint = false;
    
    for (keyType = 0; keyType <= MAXKVSKEYTYPE; keyType++){
        CFDictionaryForEach(objects, ^(const void *key, const void *value) {
            if(SOSKVSKeyGetKeyType(key) == keyType){
                decodeForKeyType(key, value, keyType);
                didPrint = true;
            }
        });
        if(didPrint)
            printmsg(CFSTR("%@\n"), CFSTR(""));
        didPrint = false;
    }
}

bool SOSCCDumpCircleKVSInformation(char *itemName) {
    CFArrayRef keysToGet = NULL;
    if (itemName)
    {
        CFStringRef itemStr = CFStringCreateWithCString(kCFAllocatorDefault, itemName, kCFStringEncodingUTF8);
        fprintf(outFile, "Retrieving %s from KVS\n", itemName);
        keysToGet = CFArrayCreateForCFTypes(kCFAllocatorDefault, itemStr, NULL);
        CFReleaseSafe(itemStr);
    }
    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);
    dispatch_group_t work_group = dispatch_group_create();
    CFTypeRef objects = getObjectsFromCloud(keysToGet, generalq, work_group);
    CFReleaseSafe(keysToGet);
    if (objects)
    {
        fprintf(outFile, "All keys and values straight from KVS\n");
        printEverything(objects);
        fprintf(outFile, "\nAll values in decoded form...\n");
        decodeAllTheValues(objects);
    }
    fprintf(outFile, "\n");
    return true;
}
