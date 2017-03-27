/*
 * Copyright (c) 2003-2007,2009-2010,2013-2014 Apple Inc. All Rights Reserved.
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
 *
 * keychain_add.c
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <time.h>

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
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecOTRSession.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include <SecurityTool/readline.h>
#include <notify.h>

#include "keychain_sync.h"
#include "keychain_log.h"
#include "syncbackup.h"

#include "secToolFileIO.h"
#include "secViewDisplay.h"

#include <Security/SecPasswordGenerate.h>

#define MAXKVSKEYTYPE kUnknownKey
#define DATE_LENGTH 18


static bool clearAllKVS(CFErrorRef *error)
{
    __block bool result = false;
    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);
    
    SOSCloudKeychainClearAll(processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef cerror)
    {
        result = (cerror != NULL);
        dispatch_semaphore_signal(waitSemaphore);
    });
    
	dispatch_semaphore_wait(waitSemaphore, finishTime);
    dispatch_release(waitSemaphore);

    return result;
}

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

static void dumpCircleInfo()
{
    CFErrorRef error = NULL;
    CFArrayRef generations = NULL;
    CFArrayRef confirmedDigests = NULL;
    bool is_user_public_trusted = false;
    __block int count = 0;
    
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    if(ccstatus == kSOSCCError) {
        printmsg(CFSTR("End of Dump - unable to proceed due to ccstatus (%s) error: %@\n"), getSOSCCStatusDescription(ccstatus), error);
        return;
    }
    printmsg(CFSTR("ccstatus: %s (%d)\n"), getSOSCCStatusDescription(ccstatus), ccstatus, error);
    
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

    if (!SOSCCForEachEngineStateAsString(&error, ^(CFStringRef oneStateString) {
        printmsg(CFSTR("%@\n"), oneStateString);
    })) {
        printmsg(CFSTR("No engine peers: %@\n"), error);
    }

    CFReleaseNull(error);
    CFReleaseNull(confirmedDigests);
}

static bool enableDefaultViews()
{
    bool result = false;
    CFMutableSetRef viewsToEnable = SOSViewCopyViewSet(kViewSetV0);
    CFMutableSetRef viewsToDisable = CFSetCreateMutable(NULL, 0, NULL);
    
    result = SOSCCViewSet(viewsToEnable, viewsToDisable);
    CFRelease(viewsToEnable);
    CFRelease(viewsToDisable);
    return result;
}

static bool requestToJoinCircle(CFErrorRef *error)
{
    // Set the visual state of switch based on membership in circle
    bool hadError = false;
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(error);
    
    switch (ccstatus)
    {
    case kSOSCCCircleAbsent:
        hadError = !SOSCCResetToOffering(error);
        hadError &= enableDefaultViews();
        break;
    case kSOSCCNotInCircle:
        hadError = !SOSCCRequestToJoinCircle(error);
        hadError &= enableDefaultViews();
        break;
    default:
        printerr(CFSTR("Request to join circle with bad status:  %@ (%d)\n"), SOSCCGetStatusDescription(ccstatus), ccstatus);
        break;
    }
    return hadError;
}

static bool setPassword(char *labelAndPassword, CFErrorRef *err)
{
    char *last = NULL;
    char *token0 = strtok_r(labelAndPassword, ":", &last);
    char *token1 = strtok_r(NULL, "", &last);
    CFStringRef label = token1 ? CFStringCreateWithCString(NULL, token0, kCFStringEncodingUTF8) : CFSTR("security command line tool");
    char *password_token = token1 ? token1 : token0;
    password_token = password_token ? password_token : "";
    CFDataRef password = CFDataCreate(NULL, (const UInt8*) password_token, strlen(password_token));
    bool returned = !SOSCCSetUserCredentials(label, password, err);
    CFRelease(label);
    CFRelease(password);
    return returned;
}

static bool tryPassword(char *labelAndPassword, CFErrorRef *err)
{
    char *last = NULL;
    char *token0 = strtok_r(labelAndPassword, ":", &last);
    char *token1 = strtok_r(NULL, "", &last);
    CFStringRef label = token1 ? CFStringCreateWithCString(NULL, token0, kCFStringEncodingUTF8) : CFSTR("security command line tool");
    char *password_token = token1 ? token1 : token0;
    password_token = password_token ? password_token : "";
    CFDataRef password = CFDataCreate(NULL, (const UInt8*) password_token, strlen(password_token));
    bool returned = !SOSCCTryUserCredentials(label, password, err);
    CFRelease(label);
    CFRelease(password);
    return returned;
}

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
static bool dumpKVS(char *itemName, CFErrorRef *err)
{
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

static bool syncAndWait(CFErrorRef *err)
{
    __block CFTypeRef objects = NULL;

    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);

    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    CloudKeychainReplyBlock replyBlock = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        secinfo("sync", "SOSCloudKeychainSynchronizeAndWait returned: %@", returnedValues);
        if (error)
            secerror("SOSCloudKeychainSynchronizeAndWait returned error: %@", error);
        objects = CFRetainSafe(returnedValues);

        secinfo("sync", "SOSCloudKeychainGetObjectsFromCloud block exit: %@", objects);
        dispatch_semaphore_signal(waitSemaphore);
    };

    SOSCloudKeychainSynchronizeAndWait(generalq, replyBlock);

	dispatch_semaphore_wait(waitSemaphore, finishTime);
	dispatch_release(waitSemaphore);

    dumpKVS(NULL, NULL);
    fprintf(outFile, "\n");
    return false;
}

static CFStringRef convertStringToProperty(char *propertyname) {
    CFStringRef propertyspec = NULL;
    
    if(strcmp(propertyname, "hasentropy") == 0) {
        propertyspec = kSOSSecPropertyHasEntropy;
    } else if(strcmp(propertyname, "screenlock") == 0) {
        propertyspec = kSOSSecPropertyScreenLock;
    } else if(strcmp(propertyname, "SEP") == 0) {
        propertyspec = kSOSSecPropertySEP;
    } else if(strcmp(propertyname, "IOS") == 0) {
        propertyspec = kSOSSecPropertyIOS;
    }
    return propertyspec;
}


static CFStringRef convertPropertyReturnCodeToString(SOSSecurityPropertyResultCode ac) {
    CFStringRef retval = NULL;
    switch(ac) {
        case kSOSCCGeneralSecurityPropertyError:
            retval = CFSTR("General Error"); break;
        case kSOSCCSecurityPropertyValid:
            retval = CFSTR("Is Member of Security Property"); break;
        case kSOSCCSecurityPropertyNotValid:
            retval = CFSTR("Is Not Member of Security Property"); break;
        case kSOSCCSecurityPropertyNotQualified:
            retval = CFSTR("Is not qualified for Security Property"); break;
        case kSOSCCNoSuchSecurityProperty:
            retval = CFSTR("No Such Security Property"); break;
    }
    return retval;
}


static bool SecPropertycmd(char *itemName, CFErrorRef *err) {
    char *cmd, *propertyname;
    SOSSecurityPropertyActionCode ac = kSOSCCSecurityPropertyQuery;
    CFStringRef propertyspec;
    
    propertyname = strchr(itemName, ':');
    if(propertyname == NULL) return false;
    *propertyname = 0;
    propertyname++;
    cmd = itemName;
    
    if(strcmp(cmd, "enable") == 0) {
        ac = kSOSCCSecurityPropertyEnable;
    } else if(strcmp(cmd, "disable") == 0) {
        ac = kSOSCCSecurityPropertyDisable;
    } else if(strcmp(cmd, "query") == 0) {
        ac = kSOSCCSecurityPropertyQuery;
    } else {
        return false;
    }
    
    propertyspec = convertStringToProperty(propertyname);
    if(!propertyspec) return false;
    
    SOSSecurityPropertyResultCode rc = SOSCCSecurityProperty(propertyspec, ac, err);
    CFStringRef resultString = convertPropertyReturnCodeToString(rc);
    
    printmsg(CFSTR("Property Result: %@ : %@\n"), resultString, propertyspec);
    return true;
}


static void dumpStringSet(CFStringRef label, CFSetRef s) {
    if(!s || !label) return;
    
    printmsg(CFSTR("%@: { "), label);
    __block bool first = true;
    CFSetForEach(s, ^(const void *p) {
        CFStringRef fmt = CFSTR(", %@");
        if(first) {
            fmt = CFSTR("%@");
        }
        CFStringRef string = (CFStringRef) p;
        printmsg(fmt, string);
        first=false;
    });
    printmsg(CFSTR(" }\n"), NULL);
}

static bool dumpMyPeer(CFErrorRef *error) {
    SOSPeerInfoRef myPeer = SOSCCCopyMyPeerInfo(error);

    if (!myPeer) return false;
    
    CFStringRef peerID = SOSPeerInfoGetPeerID(myPeer);
    CFStringRef peerName = SOSPeerInfoGetPeerName(myPeer);
    CFIndex peerVersion = SOSPeerInfoGetVersion(myPeer);
    bool retirement = SOSPeerInfoIsRetirementTicket(myPeer);
    
    printmsg(CFSTR("Peer Name: %@    PeerID: %@   Version: %d\n"), peerName, peerID, peerVersion);
    if(retirement) {
        CFDateRef retdate = SOSPeerInfoGetRetirementDate(myPeer);
        printmsg(CFSTR("Retired: %@\n"), retdate);

    }
    
    if(peerVersion >= 2) {
        CFMutableSetRef views = SOSPeerInfoV2DictionaryCopySet(myPeer, sViewsKey);
        CFStringRef serialNumber = SOSPeerInfoV2DictionaryCopyString(myPeer, sSerialNumberKey);
        CFBooleanRef preferIDS = SOSPeerInfoV2DictionaryCopyBoolean(myPeer, sPreferIDS);
        CFBooleanRef preferIDSFragmentation = SOSPeerInfoV2DictionaryCopyBoolean(myPeer, sPreferIDSFragmentation);
        CFBooleanRef preferIDSACKModel = SOSPeerInfoV2DictionaryCopyBoolean(myPeer, sPreferIDSACKModel);
        CFStringRef transportType = SOSPeerInfoV2DictionaryCopyString(myPeer, sTransportType);
        CFStringRef idsDeviceID = SOSPeerInfoV2DictionaryCopyString(myPeer, sDeviceID);
        CFMutableSetRef properties = SOSPeerInfoV2DictionaryCopySet(myPeer, sSecurityPropertiesKey);
        
        printmsg(CFSTR("Serial#: %@   PrefIDS#: %@   PrefFragmentation#: %@   PrefACK#: %@   transportType#: %@   idsDeviceID#: %@\n"),
                 serialNumber, preferIDS, preferIDSFragmentation, preferIDSACKModel, transportType, idsDeviceID);
        dumpStringSet(CFSTR("             Views: "), views);
        dumpStringSet(CFSTR("SecurityProperties: "), properties);
        
        CFReleaseSafe(serialNumber);
        CFReleaseSafe(preferIDS);
        CFReleaseSafe(preferIDSFragmentation);
        CFReleaseSafe(views);
        CFReleaseSafe(transportType);
        CFReleaseSafe(idsDeviceID);
        CFReleaseSafe(properties);
        
    }

    return myPeer != NULL;
}

static bool setBag(char *itemName, CFErrorRef *err)
{
    __block bool success = false;
    __block CFErrorRef error = NULL;

    CFStringRef random = SecPasswordCreateWithRandomDigits(10, NULL);

    CFStringPerformWithUTF8CFData(random, ^(CFDataRef stringAsData) {
        if (0 == strncasecmp(optarg, "single", 6) || 0 == strncasecmp(optarg, "all", 3)) {
            bool includeV0 = (0 == strncasecmp(optarg, "all", 3));
            printmsg(CFSTR("Setting iCSC single using entropy from string: %@\n"), random);
            CFDataRef aks_bag = SecAKSCopyBackupBagWithSecret(CFDataGetLength(stringAsData), (uint8_t*)CFDataGetBytePtr(stringAsData), &error);

            if (aks_bag) {
                success = SOSCCRegisterSingleRecoverySecret(aks_bag, includeV0, &error);
                if (!success) {
                    printmsg(CFSTR("Failed registering single secret %@"), error);
                    CFReleaseNull(aks_bag);
                }
            } else {
                printmsg(CFSTR("Failed to create aks_bag: %@"), error);
            }
            CFReleaseNull(aks_bag);
        } else if (0 == strncasecmp(optarg, "device", 6)) {
            printmsg(CFSTR("Setting Device Secret using entropy from string: %@\n"), random);

            SOSPeerInfoRef me = SOSCCCopyMyPeerWithNewDeviceRecoverySecret(stringAsData, &error);

            success = me != NULL;

            if (!success)
                printmsg(CFSTR("Failed: %@\n"), err);
            CFReleaseNull(me);
        } else {
            printmsg(CFSTR("Unrecognized argument to -b %s\n"), optarg);
        }
    });


    return success;
}

static void prClientViewState(char *label, bool result) {
    fprintf(outFile, "Sync Status for %s: %s\n", label, (result) ? "enabled": "not enabled");
}

static bool clientViewStatus(CFErrorRef *error) {
    prClientViewState("KeychainV0", SOSCCIsIcloudKeychainSyncing());
    prClientViewState("Safari", SOSCCIsSafariSyncing());
    prClientViewState("AppleTV", SOSCCIsAppleTVSyncing());
    prClientViewState("HomeKit", SOSCCIsHomeKitSyncing());
    prClientViewState("Wifi", SOSCCIsWiFiSyncing());
    prClientViewState("AlwaysOnNoInitialSync", SOSCCIsContinuityUnlockSyncing());
    return false;
}


static bool dumpYetToSync(CFErrorRef *error) {
    CFArrayRef yetToSyncViews = SOSCCCopyYetToSyncViewsList(error);

    bool hadError = yetToSyncViews;

    if (yetToSyncViews) {
        __block CFStringRef separator = CFSTR("");

        printmsg(CFSTR("Yet to sync views: ["), NULL);

        CFArrayForEach(yetToSyncViews, ^(const void *value) {
            if (isString(value)) {
                printmsg(CFSTR("%@%@"), separator, value);

                separator = CFSTR(", ");
            }
        });
        printmsg(CFSTR("]\n"), NULL);
    }

    return !hadError;
}


// enable, disable, accept, reject, status, Reset, Clear
int
keychain_sync(int argc, char * const *argv)
{
    /*
	 "Keychain Syncing"
	 "    -d     disable"
	 "    -e     enable (join/create circle)"
	 "    -i     info (current status)"
	 "    -m     dump my peer"
	 "
	 "Account/Circle Management"
	 "    -a     accept all applicants"
	 "    -l     [reason] sign out of circle + set custom departure reason"
	 "    -q     sign out of circle"
	 "    -r     reject all applicants"
	 "    -E     ensure fresh parameters"
     "    -b     device|all|single Register a backup bag - THIS RESETS BACKUPS!\n"
     "    -A     Apply to a ring\n"
     "    -B     Withdrawl from a ring\n"
     "    -G     Enable Ring\n"
     "    -F     Ring Status\n"
     "    -I     Dump Ring Information\n"

	 "    -N     (re-)set to new account (USE WITH CARE: device will not leave circle before resetting account!)"
	 "    -O     reset to offering"
	 "    -R     reset circle"
	 "    -X     [limit]  best effort bail from circle in limit seconds"
     "    -o     list view unaware peers in circle"
     "    -0     boot view unaware peers from circle"
     "    -1     grab account state from the keychain"
     "    -2     delete account state from the keychain"
     "    -3     grab engine state from the keychain"
     "    -4     delete engine state from the keychain"
	 "
	 "IDS"
	 "    -g     set IDS device id"
	 "    -p     retrieve IDS device id"
	 "    -x     ping all devices in an IDS account"
	 "    -w     check IDS availability"
	 "    -z     retrieve IDS id through KeychainSyncingOverIDSProxy"
	 "
	 "Password"
	 "    -P     [label:]password  set password (optionally for a given label) for sync"
	 "    -T     [label:]password  try password (optionally for a given label) for sync"
	 "
	 "KVS"
	 "    -k     pend all registered kvs keys"
	 "    -C     clear all values from KVS"
	 "    -D     [itemName]  dump contents of KVS"
     "    -W     sync and dump"
	 "
	 "Misc"
	 "    -v     [enable|disable|query:viewname] enable, disable, or query my PeerInfo's view set"
	 "             viewnames are: keychain|masterkey|iclouddrive|photos|cloudkit|escrow|fde|maildrop|icloudbackup|notes|imessage|appletv|homekit|"
     "                            wifi|passwords|creditcards|icloudidentity|othersyncable"
     "    -L     list all known view and their status"
	 "    -S     [enable|disable|propertyname] enable, disable, or query my PeerInfo's Security Property set"
	 "             propertynames are: hasentropy|screenlock|SEP|IOS\n"
	 "    -U     purge private key material cache\n"
     "    -V     Report View Sync Status on all known clients.\n"
     "    -H     Set escrow record.\n"
     "    -J     Get the escrow record.\n"
     "    -M     Check peer availability.\n"
     */
	int ch, result = 0;
    CFErrorRef error = NULL;
    bool hadError = false;
    SOSLogSetOutputTo(NULL, NULL);

    while ((ch = getopt(argc, argv, "ab:deg:hikl:mopq:rSv:w:x:zA:B:MNJCDEF:HG:ILOP:RT:UWX:VY01234")) != -1)
        switch  (ch) {
		case 'l':
		{
			fprintf(outFile, "Signing out of circle\n");
			hadError = !SOSCCSignedOut(true, &error);
			if (!hadError) {
				errno = 0;
				int reason = (int) strtoul(optarg, NULL, 10);
				if (errno != 0 ||
					reason < kSOSDepartureReasonError ||
					reason >= kSOSNumDepartureReasons) {
					fprintf(errFile, "Invalid custom departure reason %s\n", optarg);
				} else {
					fprintf(outFile, "Setting custom departure reason %d\n", reason);
					hadError = !SOSCCSetLastDepartureReason(reason, &error);
					notify_post(kSOSCCCircleChangedNotification);
				}
			}
			break;
		}
			
		case 'q':
		{
			fprintf(outFile, "Signing out of circle\n");
			bool signOutImmediately = false;
			if (strcasecmp(optarg, "true") == 0) {
				signOutImmediately = true;
			} else if (strcasecmp(optarg, "false") == 0) {
				signOutImmediately = false;
			} else {
				fprintf(outFile, "Please provide a \"true\" or \"false\" whether you'd like to leave the circle immediately\n");
			}
			hadError = !SOSCCSignedOut(signOutImmediately, &error);
			notify_post(kSOSCCCircleChangedNotification);
			break;
		}
			
		case 'p':
		{
			fprintf(outFile, "Grabbing DS ID\n");
			CFStringRef deviceID = SOSCCCopyDeviceID(&error);
			if (error) {
				hadError = true;
				break;
			}
			if (!isNull(deviceID)) {
				const char *id = CFStringGetCStringPtr(deviceID, kCFStringEncodingUTF8);
				if (id)
					fprintf(outFile, "IDS Device ID: %s\n", id);
				else
					fprintf(outFile, "IDS Device ID is null!\n");
			}
            CFReleaseNull(deviceID);
			break;
		}
			
		case 'g':
		{
			fprintf(outFile, "Setting DS ID: %s\n", optarg);
			CFStringRef deviceID = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
			hadError = SOSCCSetDeviceID(deviceID, &error);
			CFReleaseNull(deviceID);
			break;
		}
			
		case 'w':
		{
			fprintf(outFile, "Attempting to send this message over IDS: %s\n", optarg);
			CFStringRef message = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
			hadError = SOSCCIDSServiceRegistrationTest(message, &error);
			if (error) {
				printerr(CFSTR("IDS is not ready: %@\n"), error);
				CFRelease(error);
			}
			CFReleaseNull(message);
			break;
		}
			
		case 'x':
		{
			fprintf(outFile, "Starting ping test using this message: %s\n", optarg);
			CFStringRef message = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
			hadError = SOSCCIDSPingTest(message, &error);
			if (error) {
				printerr(CFSTR("Ping test failed to start: %@\n"), error);
				CFRelease(error);
			}
			CFReleaseNull(message);
			break;
		}
			
		case 'z':
			hadError = SOSCCIDSDeviceIDIsAvailableTest(&error);
			if (error) {
				printerr(CFSTR("Failed to retrieve IDS device ID: %@\n"), error);
				CFRelease(error);
			}
			break;
			
		case 'e':
			fprintf(outFile, "Turning ON keychain syncing\n");
			hadError = requestToJoinCircle(&error);
			break;
			
		case 'd':
			fprintf(outFile, "Turning OFF keychain syncing\n");
			hadError = !SOSCCRemoveThisDeviceFromCircle(&error);
			break;
			
		case 'a':
		{
			CFArrayRef applicants = SOSCCCopyApplicantPeerInfo(NULL);
			if (applicants) {
				hadError = !SOSCCAcceptApplicants(applicants, &error);
				CFRelease(applicants);
			} else {
				fprintf(errFile, "No applicants to accept\n");
			}
			break;
		}
			
		case 'r':
		{
			CFArrayRef applicants = SOSCCCopyApplicantPeerInfo(NULL);
			if (applicants)	{
				hadError = !SOSCCRejectApplicants(applicants, &error);
				CFRelease(applicants);
			} else {
				fprintf(errFile, "No applicants to reject\n");
			}
			break;
		}
			
		case 'i':
			dumpCircleInfo();
			break;
			
		case 'k':
			notify_post("com.apple.security.cloudkeychain.forceupdate");
			break;

        case 'o':
        {
            printPeerInfos("view-unaware", ^(CFErrorRef *error) { return SOSCCCopyViewUnawarePeerInfo(error); });
            break;
        }

        case '0':
        {
            CFArrayRef unawares = SOSCCCopyViewUnawarePeerInfo(&error);
            if (unawares) {
                hadError = !SOSCCRemovePeersFromCircle(unawares, &error);
            } else {
                hadError = true;
            }
            CFReleaseNull(unawares);
            break;
        }
        case '1':
        {
            CFDataRef accountState = SOSCCCopyAccountState(&error);
            if (accountState) {
                printmsg(CFSTR(" %@\n"), CFDataCopyHexString(accountState));
            } else {
                hadError = true;
            }
            CFReleaseNull(accountState);
            break;
        }
        case '2':
        {
            bool status = SOSCCDeleteAccountState(&error);
            if (status) {
                printmsg(CFSTR("Deleted account from the keychain %d\n"), status);
            } else {
                hadError = true;
            }
            break;
        }
        case '3':
        {
            CFDataRef engineState = SOSCCCopyEngineData(&error);
            if (engineState) {
                printmsg(CFSTR(" %@\n"), CFDataCopyHexString(engineState));
            } else {
                hadError = true;
            }
            CFReleaseNull(engineState);
            break;
        }
        case '4':
        {
            bool status = SOSCCDeleteEngineState(&error);
            if (status) {
                printmsg(CFSTR("Deleted engine-state from the keychain %d\n"), status);
            } else {
                hadError = true;
            }
            break;
        }

		case 'E':
		{
			fprintf(outFile, "Ensuring Fresh Parameters\n");
			bool result = SOSCCRequestEnsureFreshParameters(&error);
			if (error) {
				hadError = true;
				break;
			}
			if (result) {
				fprintf(outFile, "Refreshed Parameters Ensured!\n");
			} else {
				fprintf(outFile, "Problem trying to ensure fresh parameters\n");
			}
			break;
		}
		case 'A':
		{
			fprintf(outFile, "Applying to Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCApplyToARing(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
		case 'B':
		{
			fprintf(outFile, "Withdrawing from Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCWithdrawlFromARing(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
		case 'F':
		{
			fprintf(outFile, "Status of this device in the Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCRingStatus(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
		case 'G':
		{
			fprintf(outFile, "Enabling Ring\n");
			CFStringRef ringName = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
			hadError = SOSCCEnableRing(ringName, &error);
            CFReleaseNull(ringName);
			break;
		}
        case 'H':
        {
            fprintf(outFile, "Setting random escrow record\n");
            bool success = SOSCCSetEscrowRecord(CFSTR("label"), 8, &error);
            if(success)
                hadError = false;
            else
                hadError = true;
            break;
        }
        case 'J':
        {
            CFDictionaryRef attempts = SOSCCCopyEscrowRecord(&error);
            if(attempts){
                CFDictionaryForEach(attempts, ^(const void *key, const void *value) {
                    if(isString(key)){
                        char *keyString = CFStringToCString(key);
                        fprintf(outFile, "%s:\n", keyString);
                        free(keyString);
                    }
                    if(isDictionary(value)){
                        CFDictionaryForEach(value, ^(const void *key, const void *value) {
                            if(isString(key)){
                                char *keyString = CFStringToCString(key);
                                fprintf(outFile, "%s: ", keyString);
                                free(keyString);
                            }
                            if(isString(value)){
                                char *time = CFStringToCString(value);
                                fprintf(outFile, "timestamp: %s\n", time);
                                free(time);
                            }
                            else if(isNumber(value)){
                                uint64_t tries;
                                CFNumberGetValue(value, kCFNumberLongLongType, &tries);
                                fprintf(outFile, "date: %llu\n", tries);
                            }
                        });
                    }
                  
               });
            }
            CFReleaseNull(attempts);
            hadError = false;
            break;
        }
        case 'M':
        {
            bool success = SOSCCCheckPeerAvailability(&error);
            if(success)
                hadError = false;
            else
                hadError = true;
            break;
        }
		case 'I':
		{
			fprintf(outFile, "Printing all the rings\n");
			CFStringRef ringdescription = SOSCCGetAllTheRings(&error);
			if(!ringdescription)
				hadError = true;
			else
				fprintf(outFile, "Rings: %s", CFStringToCString(ringdescription));
			
			break;
		}

		case 'N':
			hadError = !SOSCCAccountSetToNew(&error);
			if (!hadError)
				notify_post(kSOSCCCircleChangedNotification);
			break;

		case 'R':
			hadError = !SOSCCResetToEmpty(&error);
			break;

		case 'O':
			hadError = !SOSCCResetToOffering(&error);
			break;

		case 'm':
			hadError = !dumpMyPeer(&error);
			break;

		case 'C':
			hadError = clearAllKVS(&error);
			break;

		case 'P':
			hadError = setPassword(optarg, &error);
			break;

		case 'T':
			hadError = tryPassword(optarg, &error);
			break;

		case 'X':
		{
			uint64_t limit = strtoul(optarg, NULL, 10);
			hadError = !SOSCCBailFromCircle_BestEffort(limit, &error);
			break;
		}

		case 'U':
			hadError = !SOSCCPurgeUserCredentials(&error);
			break;

		case 'D':
			hadError = !dumpKVS(optarg, &error);
			break;

		case 'W':
			hadError = syncAndWait(&error);
			break;

		case 'v':
			hadError = !viewcmd(optarg, &error);
			break;

        case 'V':
            hadError = clientViewStatus(&error);
            break;
        case 'L':
            hadError = !listviewcmd(&error);
            break;

		case 'S':
			hadError = !SecPropertycmd(optarg, &error);
			break;

        case 'b':
            hadError = setBag(optarg, &error);
            break;

        case 'Y':
            hadError = dumpYetToSync(&error);
            break;
        case '?':
		default:
			return 2; /* Return 2 triggers usage message. */
	}

	if (hadError)
        printerr(CFSTR("Error: %@\n"), error);

	return result;
}
