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

#include <Security/SecItem.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <securityd/SOSCloudCircleServer.h>

#include <CKBridge/SOSCloudKeychainClient.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include <SecurityTool/readline.h>
#include <notify.h>

#include "SOSCommands.h"

#define printmsg(format, ...) _printcfmsg(stdout, format, __VA_ARGS__)
#define printerr(format, ...) _printcfmsg(stderr, format, __VA_ARGS__)

static void _printcfmsg(FILE *ff, CFStringRef format, ...)
{
    va_list args;
    va_start(args, format);
    CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
    va_end(args);
    CFStringPerformWithCString(message, ^(const char *utf8String) { fprintf(ff, utf8String, ""); });
    CFRelease(message);
}

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

static void dumpCircleInfo()
{
    CFErrorRef error = NULL;
    CFArrayRef peerPeerInfos = NULL;
    CFArrayRef applicants = NULL;
    CFArrayRef retirees = NULL;
    CFArrayRef peerInfos = NULL;
    CFArrayRef generations = NULL;
    bool is_user_public_trusted = false;
    __block int count = 0;
    
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    printmsg(CFSTR("ccstatus: %s (%d), error: %@\n"), getSOSCCStatusDescription(ccstatus), ccstatus, error);
    
    if(ccstatus == kSOSCCError) {
        printmsg(CFSTR("End of Dump - unable to proceed due to ccstatus -\n\t%s\n"), getSOSCCStatusDescription(ccstatus));
        return;
    }
    
    is_user_public_trusted = SOSCCValidateUserPublic(&error);
    if(is_user_public_trusted)
        printmsg(CFSTR("Account user public is trusted%@"),CFSTR("\n"));
    else
        printmsg(CFSTR("Account user public is not trusted%@"),CFSTR("\n"));

    // Now look at current peers
    peerPeerInfos = SOSCCCopyValidPeerPeerInfo(&error);

    if (peerPeerInfos)
    {
        printmsg(CFSTR("Valid Peers: %ld, error: %@\n"), (long)CFArrayGetCount(peerPeerInfos), error);
        CFArrayForEach(peerPeerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            printmsg(CFSTR("Valid Peer: %@ (%@)\n"), peerName, peer);
        });
    }
    else
        printmsg(CFSTR("No peers, error: %@\n"), error);
    
    CFReleaseNull(peerPeerInfos);
    peerPeerInfos = SOSCCCopyNotValidPeerPeerInfo(&error);
    
    if (peerPeerInfos)
    {
        printmsg(CFSTR("Non Valid Peers: %ld, error: %@\n"), (long)CFArrayGetCount(peerPeerInfos), error);
        CFArrayForEach(peerPeerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            printmsg(CFSTR("Not Valid Peer: %@ (%@)\n"), peerName, peer);
        });
    }
    CFReleaseNull(peerPeerInfos);

    applicants = SOSCCCopyApplicantPeerInfo(&error);
    if (applicants)
    {
        printmsg(CFSTR("Applicants: %ld, error: %@\n"), (long)CFArrayGetCount(applicants), error);
        CFArrayForEach(applicants, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            printmsg(CFSTR("Applicant: %@ (%@)\n"), peerName, peer);
        });
    }
    else
        printmsg(CFSTR("No applicants, error: %@\n"), error);
    CFReleaseNull(applicants);

    
    peerInfos = SOSCCCopyConcurringPeerPeerInfo(&error);
    if (peerInfos)
    {
        printmsg(CFSTR("Concurring Peers: %ld, error: %@\n"), (long)CFArrayGetCount(peerInfos), error);
        CFArrayForEach(peerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            printmsg(CFSTR("Concurr: %@ (%@)\n"), peerName, peer);
        });
    }
    else
        printmsg(CFSTR("No concurring peers, error: %@\n"), error);
    
    CFReleaseNull(peerInfos);
    retirees = SOSCCCopyRetirementPeerInfo(&error);
    if (retirees)
    {
        printmsg(CFSTR("Retired Peers: %ld, error: %@\n"), (long)CFArrayGetCount(retirees), error);
        CFArrayForEach(retirees, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            printmsg(CFSTR("Retiree: %@ (%@)\n"), peerName, peer);
        });
    }
    else
        printmsg(CFSTR("No retired peers, error: %@\n"), error);
    CFReleaseNull(retirees);
    
    generations = SOSCCCopyGenerationPeerInfo(&error);
    if(generations)
    {
        CFArrayForEach(generations, ^(const void *value) {
            count++;
            if(count%2 != 0)
                printmsg(CFSTR("Circle name: %@, "),value);
            
            if(count%2 == 0)
                printmsg(CFSTR("Generation Count: %@\n"), value);
        });
    }
    else
        printmsg(CFSTR("No generation count: %@\n"), error);
    CFReleaseNull(generations);
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
        break;
    case kSOSCCNotInCircle:
        hadError = !SOSCCRequestToJoinCircle(error);
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
        secerror("SOSCloudKeychainGetObjectsFromCloud returned: %@", returnedValues);
        object = returnedValues;
        if (object)
            CFRetain(object);
        if (error)
        {
            secerror("SOSCloudKeychainGetObjectsFromCloud returned error: %@", error);
            //       CFRelease(*error);
        }
        dispatch_group_leave(dgroup);
        secerror("SOSCloudKeychainGetObjectsFromCloud block exit: %@", object);
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

static void displayCircles(CFTypeRef objects)
{
    // SOSCCCopyApplicantPeerInfo doesn't display all info, e.g. in the case where we are not in circle
    CFDictionaryForEach(objects, ^(const void *key, const void *value) {
        if (SOSKVSKeyGetKeyType(key) == kCircleKey)
        {
            CFErrorRef localError = NULL;
            if (isData(value))
            {
                SOSCircleRef circle = SOSCircleCreateFromData(NULL, (CFDataRef) value, &localError);
                printmsg(CFSTR("circle: %@ %@"), key, circle);
                CFReleaseSafe(circle);
            }
            else
                printmsg(CFSTR("non-circle: %@ %@"), key, value);
        }
    });
}

static bool dumpKVS(char *itemName, CFErrorRef *err)
{
    CFArrayRef keysToGet = NULL;
    if (itemName)
    {
        CFStringRef itemStr = CFStringCreateWithCString(kCFAllocatorDefault, itemName, kCFStringEncodingUTF8);
        printf("Retrieving %s from KVS\n", itemName);
        keysToGet = CFArrayCreateForCFTypes(kCFAllocatorDefault, itemStr, NULL);
        CFReleaseSafe(itemStr);
    }
    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);
    dispatch_group_t work_group = dispatch_group_create();
    CFTypeRef objects = getObjectsFromCloud(keysToGet, generalq, work_group);
    CFReleaseSafe(keysToGet);
    printmsg(CFSTR("   : %@\n"), objects);
    if (objects)
        displayCircles(objects);
    printf("\n");
    return false;
}

static bool syncAndWait(char *itemName, CFErrorRef *err)
{
    CFArrayRef keysToGet = NULL;
    __block CFTypeRef objects = NULL;
    if (!itemName)
    {
        fprintf(stderr, "No item keys supplied\n");
        return false;
    }

    CFStringRef itemStr = CFStringCreateWithCString(kCFAllocatorDefault, itemName, kCFStringEncodingUTF8);
    printf("Retrieving %s from KVS\n", itemName);
    keysToGet = CFArrayCreateForCFTypes(kCFAllocatorDefault, itemStr, NULL);
    CFReleaseSafe(itemStr);

    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);

    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    CloudKeychainReplyBlock replyBlock = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        secerror("SOSCloudKeychainSynchronizeAndWait returned: %@", returnedValues);
        if (error)
            secerror("SOSCloudKeychainSynchronizeAndWait returned error: %@", error);
        objects = returnedValues;
        if (objects)
            CFRetain(objects);
        secerror("SOSCloudKeychainGetObjectsFromCloud block exit: %@", objects);
        dispatch_semaphore_signal(waitSemaphore);
    };

    SOSCloudKeychainSynchronizeAndWait(keysToGet, generalq, replyBlock);

	dispatch_semaphore_wait(waitSemaphore, finishTime);
	dispatch_release(waitSemaphore);

    CFReleaseSafe(keysToGet);
    printmsg(CFSTR("   : %@\n"), objects);
    if (objects)
        displayCircles(objects);
    printf("\n");
    return false;
}

// enable, disable, accept, reject, status, Reset, Clear
int
keychain_sync(int argc, char * const *argv)
{
    /*
     "    -e     Enable Keychain Syncing (join/create circle)\n"
     "    -d     Disable Keychain Syncing\n"
     "    -a     Accept all applicants\n"
     "    -r     Reject all applicants\n"
     "    -i     Info\n"
     "    -k     Pend all registered kvs keys\n"
     "    -s     Schedule sync with all peers\n"
     "    -E     Ensure Fresh Parameters\n"
     "    -R     Reset\n"
     "    -O     ResetToOffering\n"
     "    -q     Sign out of Circle\n"
     "    -C     Clear all values from KVS\n"
     "    -P    [label:]password  Set password (optionally for a given label) for sync\n"
     "    -D    [itemName]  Dump contents of KVS\n"
     "    -P    [label:]password  Set password (optionally for a given label) for sync\n"
     "    -T    [label:]password  Try password (optionally for a given label) for sync\n"
     "    -U     Purge private key material cache\n"
     "    -D    [itemName]  Dump contents of KVS\n"
     "    -W    itemNames  sync and dump\n"
     "    -X    [limit]  Best effort bail from circle in limit seconds\n"
     "    -p     Retrieve IDS Device ID\n"
     "    -g     Set IDS Device ID\n"
     */
	int ch, result = 0;
    CFErrorRef error = NULL;
    bool hadError = false;
    
	while ((ch = getopt(argc, argv, "pedakrisEROChP:T:DW:UX:g:q:")) != -1)
	{
		switch  (ch)
		{
        case 'q':
            {
                printf("Signing out of circle\n");
                bool immediately = false;
                CFStringRef leaveImmediately = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
                if(CFStringCompare(CFSTR("true"), leaveImmediately, 0) == 0){
                    immediately = true;
                }
                else if(CFStringCompare(CFSTR("false"), leaveImmediately, 0) == 0){
                    immediately = false;
                }
                else{
                    printf("Please provide a \"true\" or \"false\" whether you'd like to leave the circle immediately\n");
                }
                hadError = !SOSCCSignedOut(immediately, &error);
            }
                break;
        case 'p':
            {
                printf("Grabbing DS ID\n");
                CFStringRef deviceID = SOSCCRequestDeviceID(&error);
                if(error){
                    hadError = true;
                    break;
                }
                if(!isNull(deviceID)){
                    const char* id = CFStringGetCStringPtr(deviceID, kCFStringEncodingUTF8);
                    if(id)
                        printf("IDS Device ID: %s\n", id);
                    else
                        printf("IDS Device ID is null!\n");
                }
            }
            break;
        case 'g':
            {
                CFStringRef deviceID = CFStringCreateWithCString(kCFAllocatorDefault, (char *)optarg, kCFStringEncodingUTF8);
                printf("Setting DS ID: %s\n", optarg);
                hadError = SOSCCSetDeviceID(deviceID, &error);
                CFReleaseNull(deviceID);
            }
            break;
        case 'e':
            printf("Keychain syncing is being turned ON\n");
            hadError = requestToJoinCircle(&error);
            break;
        case 'd':
            printf("Keychain syncing is being turned OFF\n");
            hadError = !SOSCCRemoveThisDeviceFromCircle(&error);
            break;
        case 'a':
            {
                CFArrayRef applicants = SOSCCCopyApplicantPeerInfo(NULL);
                if (applicants)
                {
                    hadError = !SOSCCAcceptApplicants(applicants, &error);
                    CFRelease(applicants);
                }
                else
                    fprintf(stderr, "No applicants to accept\n");
            }
            break;
        case 'r':
            {
                CFArrayRef applicants = SOSCCCopyApplicantPeerInfo(NULL);
                if (applicants)
                {
                    hadError = !SOSCCRejectApplicants(applicants, &error);
                    CFRelease(applicants);
                }
                else
                    fprintf(stderr, "No applicants to reject\n");
            }
            break;
        case 'i':
            dumpCircleInfo();
            break;
        case 'k':
            notify_post("com.apple.security.cloudkeychain.forceupdate");
                break;
            case 's':
                //SOSCloudKeychainRequestSyncWithAllPeers(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
                break;
            case 'E':
            {
                printf("Ensuring Fresh Parameters\n");
                bool result = SOSCCRequestEnsureFreshParameters(&error);
                if(error){
                    hadError = true;
                    break;
                }
                if(result){
                    printf("Refreshed Parameters Ensured!\n");
                }
                else{
                    printf("Problem trying to ensure fresh parameters\n");
                }
            }
                break;
            case 'R':
                hadError = !SOSCCResetToEmpty(&error);
                break;
            case 'O':
                hadError = !SOSCCResetToOffering(&error);
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
            }
            break;
        case 'U':
            hadError = !SOSCCPurgeUserCredentials(&error);
            break;
        case 'D':
            hadError = dumpKVS(optarg, &error);
            break;
        case 'W':
            hadError = syncAndWait(optarg, &error);
            break;
		case '?':
		default:
			return 2; /* Return 2 triggers usage message. */
		}
	}

	//argc -= optind;
	//argv += optind;

//	if (argc == 0)
//		return 2;

	if (hadError)
        printerr(CFSTR("Error: %@\n"), error);

    // 		sec_perror("SecItemAdd", result);

	return result;
}
