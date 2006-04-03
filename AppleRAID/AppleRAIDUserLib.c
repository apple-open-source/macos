/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mach/mach.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>

#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

#include "AppleRAIDUserClient.h"
#include "AppleRAIDUserLib.h"
#include "AppleRAIDMember.h"

#ifdef DEBUG

#define IOLog1(...) { printf(__VA_ARGS__); fflush(stdout); }
//#define IOLog2(args...) { printf(args...); fflush(stdout); }

#endif DEBUG

#ifndef IOLog1
#define IOLog1(args...)
#endif
#ifndef IOLog2
#define IOLog2(args...) 
#endif



// ***************************************************************************************************
//
// open/close raid controller connection
// 
// ***************************************************************************************************

static io_connect_t gRAIDControllerPort = 0;

static io_connect_t
AppleRAIDOpenConnection()
{
    kern_return_t	kr; 
    CFDictionaryRef	classToMatch;
    io_service_t	serviceObject;

    if (gRAIDControllerPort) return gRAIDControllerPort;

    classToMatch = IOServiceMatching(kAppleRAIDUserClassName);
    if (classToMatch == NULL)
    {
        IOLog1("IOServiceMatching returned a NULL dictionary.\n");
	return kIOReturnNoResources;
    }
    
    serviceObject = IOServiceGetMatchingService(kIOMasterPortDefault, classToMatch);
    if (!serviceObject)
    {
        IOLog1("Couldn't find any matches.\n");
	return kIOReturnNoResources;
    }
    
    // This call will cause the user client to be instantiated.
    kr = IOServiceOpen(serviceObject, mach_task_self(), 0, &gRAIDControllerPort);
    
    // Release the io_service_t now that we're done with it.
    IOObjectRelease(serviceObject);
    
    if (kr != KERN_SUCCESS)
    {
        IOLog1("IOServiceOpen returned %d\n", kr);
	return kr;
    }

    kr = IOConnectMethodScalarIScalarO(gRAIDControllerPort, kAppleRAIDClientOpen, 0, 0);
    UInt32 count = 0;
    // retry for 1 minute
    while (kr == kIOReturnExclusiveAccess && count < 60)
    {
#ifdef DEBUG
	if ((count % 15) == 0) IOLog1("AppleRAID: controller object is busy, retrying...\n");
#endif
	(void)sleep(1);
	kr = IOConnectMethodScalarIScalarO(gRAIDControllerPort, kAppleRAIDClientOpen, 0, 0);
	count++;
    }
    if (kr != KERN_SUCCESS)
    {
	printf("AppleRAID: failed trying to get controller object, rc = 0x%x.\n", kr);
        
        // This closes the connection to our user client and destroys the connect handle.
        IOServiceClose(gRAIDControllerPort);
	gRAIDControllerPort = 0;
    }

    return gRAIDControllerPort;
}

static kern_return_t
AppleRAIDCloseConnection()
{
    kern_return_t 	kr;

    if (!gRAIDControllerPort) return kIOReturnSuccess;

    kr = IOConnectMethodScalarIScalarO(gRAIDControllerPort, kAppleRAIDClientClose, 0, 0);
    if (kr != KERN_SUCCESS)
    {
        IOLog1("AppleRAIDClientClose returned %d\n", kr);
	return kr;
    }

    // This closes the connection to our user client and destroys the connect handle.
    kr = IOServiceClose(gRAIDControllerPort);
    if (kr != KERN_SUCCESS)
    {
        IOLog1("IOServiceClose returned %d\n", kr);
	return kr;
    }

    gRAIDControllerPort = 0;
    
    return kr;
}


// ***************************************************************************************************
//
// set notifications
// 
// ***************************************************************************************************

typedef struct setChangedInfo {
    io_object_t			service;
    mach_port_t     		notifier;
    CFStringRef			uuidString;
} setChangedInfo_t;

static IONotificationPortRef	gNotifyPort;
static io_iterator_t		gRAIDSetIter;

static void
raidSetChanged(void *refcon, io_service_t service, natural_t messageType, void *messageArgument)
{
    setChangedInfo_t * setChangedInfo = (setChangedInfo_t *)refcon;

    if (messageType == kIOMessageServiceIsTerminated) {

	// broadcast "raid set died" notification
	CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					     CFSTR(kAppleRAIDNotificationSetTerminated),
					     setChangedInfo->uuidString,
					     NULL,           // CFDictionaryRef userInfo
					     false);

	IOObjectRelease(setChangedInfo->service);
	IOObjectRelease(setChangedInfo->notifier);
	CFRelease(setChangedInfo->uuidString);
	free(setChangedInfo);
	
	return;
    }

    IOLog2("raidSetChanged: messageType %08x, arg %08lx\n", messageType, (UInt32) messageArgument);

    // we only care about messages from the raid driver, toss all others.
    if (messageType != kAppleRAIDMessageSetChanged) return;

    // broadcast "raid set changed" notification
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					 CFSTR(kAppleRAIDNotificationSetChanged),
					 setChangedInfo->uuidString,
					 NULL,           // CFDictionaryRef userInfo
					 false);
}

void static
raidSetDetected(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_service_t		newSet;
    setChangedInfo_t *		setChangedInfo;
    CFMutableDictionaryRef  	registryEntry;
    CFStringRef			uuidString;

    while (newSet = IOIteratorNext(iterator)) {

	// fetch a copy of the in kernel registry object
	kr = IORegistryEntryCreateCFProperties(newSet, &registryEntry, kCFAllocatorDefault, 0);
	if (kr != KERN_SUCCESS) return;

	// get the set's UUID name
	uuidString = CFDictionaryGetValue(registryEntry, CFSTR(kAppleRAIDSetUUIDKey));
	if (uuidString) uuidString = CFStringCreateCopy(NULL, uuidString);
	CFRelease(registryEntry);
	if (!uuidString) return;

	setChangedInfo = calloc(1, sizeof(setChangedInfo_t));
	setChangedInfo->service = newSet;
	setChangedInfo->uuidString = uuidString;

	// set up notifications for any changes to this set
	kr = IOServiceAddInterestNotification(gNotifyPort, newSet, kIOGeneralInterest,
					      &raidSetChanged, (void *)setChangedInfo,
					      &setChangedInfo->notifier);
	if (kr != KERN_SUCCESS) {
	    free(setChangedInfo);
	    return;
	}

	// broadcast "new raid set" notification
	CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					     CFSTR(kAppleRAIDNotificationSetDiscovered),
					     uuidString,
					     NULL,	// CFDictionaryRef userInfo
					     false);
    }
}


kern_return_t
AppleRAIDEnableNotifications()
{
    kern_return_t 	kr;
    CFDictionaryRef	classToMatch;
    CFRunLoopSourceRef	runLoopSource;

    IOLog1("AppleRAIDEnableNotifications entered\n");

    classToMatch = IOServiceMatching(kAppleRAIDSetClassName);
    if (classToMatch == NULL)
    {
        IOLog1("IOServiceMatching returned a NULL dictionary.\n");
	return kIOReturnNoResources;
    }

    gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);
    
    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOFirstMatchNotification,
                                            classToMatch,
                                            raidSetDetected,
                                            NULL,
                                            &gRAIDSetIter );
    if (kr != KERN_SUCCESS)
    {
        IOLog1("IOServiceAddMatchingNotification returned %d\n", kr);
	return kr;
    }
    
    raidSetDetected(NULL, gRAIDSetIter);	// Iterate once to get already-present
						// devices and arm the notification
    return kr;
}

kern_return_t
AppleRAIDDisableNotifications(void)
{

    IONotificationPortDestroy(gNotifyPort);

    if (gRAIDSetIter) 
    {
        IOObjectRelease(gRAIDSetIter);
        gRAIDSetIter = 0;
    }
    
    return KERN_SUCCESS;
}


// ***************************************************************************************************
//
// list of set, getSet, getMember
// 
// ***************************************************************************************************

#define kMaxIOConnectTransferSize  4096


CFMutableArrayRef
AppleRAIDGetListOfSets(UInt32 filter)
{
    kern_return_t 	kr;
    IOByteCount		listSize = kMaxIOConnectTransferSize;
    CFMutableArrayRef 	theList = NULL;

    char * listString = (char *)malloc((int)listSize);

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;

    kr = IOConnectMethodScalarIStructureO(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
					  kAppleRAIDGetListOfSets,	// an index to the function in the Kernel.
					  1,				// the number of scalar input values.
					  &listSize,			// a pointer to the size of the output struct parameter.
					  filter,			// input
					  listString);			// output
    
    if (kr == KERN_SUCCESS) {
        IOLog2("AppleRAIDGetListOfSets was successful.\n");
        IOLog2("size = %d, theList = %s\n", (int)listSize, (char *)listString);

	theList = (CFMutableArrayRef)IOCFUnserialize(listString, kCFAllocatorDefault, 0, NULL);
    }

    free(listString);

    AppleRAIDCloseConnection();
    
    return theList;
}

CFMutableDictionaryRef
AppleRAIDGetSetProperties(AppleRAIDSetRef setName)
{
    kern_return_t 	kr;
    IOByteCount		propSize = kMaxIOConnectTransferSize;
    CFMutableDictionaryRef props = NULL;
    CFIndex		bufferSize = kAppleRAIDMaxUUIDStringSize;
    char		buffer[bufferSize];

    if (!CFStringGetCString(setName, buffer, bufferSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleRAIDGetSetProperties() CFStringGetCString failed?\n");
	return NULL;
    }

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;
    
    char * propString = (char *)malloc(propSize);

    kr = IOConnectMethodStructureIStructureO(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
					     kAppleRAIDGetSetProperties,	// an index to the function in the Kernel.
					     bufferSize,			// the size of the input struct paramter.
					     &propSize,				// a pointer to the size of the output struct parameter.
					     buffer,				// a pointer to the input struct parameter.
					     propString);			// a pointer to the output struct parameter.
    
    if (kr == KERN_SUCCESS) {
        IOLog2("AppleRAIDGetSetProperties was successful.\n");
        IOLog2("size = %d, prop = %s\n", (int)propSize, (char *)propString);

	props = (CFMutableDictionaryRef)IOCFUnserialize(propString, kCFAllocatorDefault, 0, NULL);
    }

    free(propString);

    AppleRAIDCloseConnection();
    
    return props;
}

CFMutableDictionaryRef
AppleRAIDGetMemberProperties(AppleRAIDMemberRef memberName)
{
    kern_return_t 	kr;
    IOByteCount		propSize = kMaxIOConnectTransferSize;
    CFMutableDictionaryRef props = NULL;
    CFIndex		bufferSize = kAppleRAIDMaxUUIDStringSize;
    char		buffer[bufferSize];

    if (!CFStringGetCString(memberName, buffer, bufferSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleRAIDGetMemberProperties() CFStringGetCString failed?\n");
	return NULL;
    }
    
    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;

    char * propString = (char *)malloc(propSize);

    kr = IOConnectMethodStructureIStructureO(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
					     kAppleRAIDGetMemberProperties,	// an index to the function in the Kernel.
					     bufferSize,			// the size of the input struct paramter.
					     &propSize,				// a pointer to the size of the output struct parameter.
					     buffer,				// a pointer to the input struct parameter.
					     propString);			// a pointer to the output struct parameter.
    
    if (kr == KERN_SUCCESS) {
        IOLog2("AppleRAIDGetMemberProperties was successful.\n");
        IOLog2("size = %d, prop = %s\n", (int)propSize, (char *)propString);

	props = (CFMutableDictionaryRef)IOCFUnserialize(propString, kCFAllocatorDefault, 0, NULL);
    }

    free(propString);

    AppleRAIDCloseConnection();
    
    return props;
}

// ***************************************************************************************************
//
// set creation
// 
// ***************************************************************************************************

// XXX this should really be read out of a set files, one for each raid type
static const char *raidDescriptionBuffer =
" <array> \n"
    "<dict> \n"
	"<key>" kAppleRAIDLevelNameKey "</key>"		"<string>Stripe</string> \n"
	"<key>" kAppleRAIDMemberTypeKey "</key>"	"<array> \n"
								"<string>" kAppleRAIDMembersKey "</string> \n"
							"</array> \n"
	"<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSetTimeoutKey "</key>"	"<integer size=\"32\">0</integer> \n"
	"<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x8000</integer> \n"

	"<key>" kAppleRAIDCanAddMembersKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDCanAddSparesKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSizesCanVaryKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>" kAppleRAIDRemovalNone "</string> \n"

	"<key>" kAppleRAIDCanBeConvertedToKey "</key>"	"<false/> \n"
    "</dict> \n"
    "<dict> \n"
	"<key>" kAppleRAIDLevelNameKey "</key>"		"<string>Mirror</string> \n"
	"<key>" kAppleRAIDMemberTypeKey "</key>"	"<array> \n"
								"<string>" kAppleRAIDMembersKey "</string> \n"
								"<string>" kAppleRAIDSparesKey "</string> \n"
							"</array> \n"
	"<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDSetTimeoutKey "</key>"	"<integer size=\"32\">30</integer> \n"
	"<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x8000</integer> \n"

	"<key>" kAppleRAIDCanAddMembersKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDCanAddSparesKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDSizesCanVaryKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>" kAppleRAIDRemovalAnyMember "</string> \n"

	"<key>" kAppleRAIDCanBeConvertedToKey "</key>"	"<true/> \n"
    "</dict> \n"
    "<dict> \n"
	"<key>" kAppleRAIDLevelNameKey "</key>"		"<string>Concat</string> \n"
	"<key>" kAppleRAIDMemberTypeKey "</key>"	"<array> \n"
								"<string>" kAppleRAIDMembersKey "</string> \n"
								"<string>" kAppleRAIDSparesKey "</string> \n"
							"</array> \n"
	"<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSetTimeoutKey "</key>"	"<integer size=\"32\">0</integer> \n"
	"<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x8000</integer> \n"
    
	"<key>" kAppleRAIDCanAddMembersKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDCanAddSparesKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSizesCanVaryKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>" kAppleRAIDRemovalLastMember "</string> \n"

	"<key>" kAppleRAIDCanBeConvertedToKey "</key>"	"<true/> \n"
    "</dict> \n"
" </array> \n";


CFMutableArrayRef AppleRAIDGetSetDescriptions(void)
{
    CFStringRef errorString;

    CFMutableArrayRef setDescriptions = (CFMutableArrayRef)IOCFUnserialize(raidDescriptionBuffer, kCFAllocatorDefault, 0, &errorString);
    if (!setDescriptions) {
	CFIndex	bufferSize = CFStringGetLength(errorString);
	bufferSize = CFStringGetMaximumSizeForEncoding(bufferSize, kCFStringEncodingUTF8) + 1;
	char *buffer = malloc(bufferSize);
	if (!buffer || !CFStringGetCString(errorString, buffer, bufferSize, kCFStringEncodingUTF8)) {
	    return NULL;
	}

	IOLog1("AppleRAIDGetSetDescriptions - failed while parsing raid definition file, error: %s\n", buffer);
	CFRelease(errorString);
	return NULL;
    }

    return setDescriptions;
}


// XXX this should really be read out of a file based on raidType
// XXX timeouts don't apply to stripes, ...
static const char *defaultCreateSetBuffer =
" <dict> \n"
    "<key>" kAppleRAIDHeaderVersionKey "</key>"		"<integer size=\"32\">0x00020000</integer> \n"
    "<key>" kAppleRAIDLevelNameKey "</key>"		"<string>internal error</string> \n"
    "<key>" kAppleRAIDSetNameKey "</key>"		"<string>internal error</string> \n"
    "<key>" kAppleRAIDSetUUIDKey "</key>"		"<string>internal error</string> \n"
    "<key>" kAppleRAIDSequenceNumberKey "</key>"	"<integer size=\"32\">1</integer> \n"
    "<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x00008000</integer> \n"
    "<key>" kAppleRAIDChunkCountKey "</key>"		"<integer size=\"64\">0</integer> \n"		// per member
    "<key>" kAppleRAIDMembersKey "</key>"		"<array/> \n"
    "<key>" kAppleRAIDSparesKey "</key>"		"<array/> \n"

    "<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<false/> \n"					// mirror, raid v only
    "<key>" kAppleRAIDSetTimeoutKey "</key>"		"<integer size=\"32\">30</integer> \n"		// mirror, raid v only

    "<key>" kAppleRAIDCanAddMembersKey "</key>"		"<false/> \n"					// mirror, concat only
    "<key>" kAppleRAIDCanAddSparesKey "</key>"		"<false/> \n"
    "<key>" kAppleRAIDSizesCanVaryKey "</key>"		"<false/> \n"					// true for concat only
    "<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>internal error</string> \n"
" </dict> \n";


CFMutableDictionaryRef
AppleRAIDCreateSet(CFStringRef raidType, CFStringRef setName)
{
    CFStringRef errorString;

    CFMutableDictionaryRef setInfo = (CFMutableDictionaryRef)IOCFUnserialize(defaultCreateSetBuffer, kCFAllocatorDefault, 0, &errorString);
    if (!setInfo) {
	CFIndex	bufferSize = CFStringGetLength(errorString);
	bufferSize = CFStringGetMaximumSizeForEncoding(bufferSize, kCFStringEncodingUTF8) + 1;
	char *buffer = malloc(bufferSize);
	if (!buffer || !CFStringGetCString(errorString, buffer, bufferSize, kCFStringEncodingUTF8)) {
	    return NULL;
	}

	IOLog1("AppleRAIDCreateSet - failed while parsing create set template file, error: %s\n", buffer);
	CFRelease(errorString);
	return NULL;
    }

    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    if (!uuid) return NULL;
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, uuid);
    CFRelease(uuid);
    if (!uuidString) return NULL;
    
    CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDSetUUIDKey), uuidString);
    CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDLevelNameKey), raidType);
    CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDSetNameKey), setName);

    // XXX could just pull these from GetSetDescriptions

    // overrides
    if (CFEqual(raidType, CFSTR("Stripe"))) {
	CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDRemovalAllowedKey), CFSTR(kAppleRAIDRemovalNone));
    }
    if (CFEqual(raidType, CFSTR("Concat"))) {
	CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDCanAddMembersKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDSizesCanVaryKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDRemovalAllowedKey), CFSTR(kAppleRAIDRemovalLastMember));
    }
    if (CFEqual(raidType, CFSTR("Mirror"))) {
	CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDCanAddMembersKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDCanAddSparesKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setInfo, CFSTR(kAppleRAIDRemovalAllowedKey), CFSTR(kAppleRAIDRemovalAnyMember));
    }
    
    return setInfo;
}

bool
AppleRAIDModifySet(CFMutableDictionaryRef setInfo, CFStringRef key, void * value)
{
    // XXX add some simple sanity checks?
    // like the key needs to be an existing key
    // value needs to be of the same CF type

    // if live, changing the chunksize means we have to change chunk count
    
    CFDictionarySetValue(setInfo, key, value);

    return true;
}

// ***************************************************************************************************
//
// member creation
// 
// ***************************************************************************************************

struct memberInfo {
    CFStringRef diskNameCF;
    io_name_t	diskName;
    io_name_t	wholeDiskName;
    unsigned int partitionNumber;
    io_name_t	regName;
    UInt64	size;
    UInt64	blockSize;
    UInt64	chunkCount;
    UInt64	chunkSize;
    UInt64	headerOffset;
    bool	isWhole;
    bool	isRAID;
    CFStringRef uuidString;
};
typedef struct memberInfo memberInfo_t;

static memberInfo_t *
getMemberInfo(CFStringRef partitionName)
{
    // sigh...
    CFIndex diskNameSize = CFStringGetLength(partitionName);
    diskNameSize = CFStringGetMaximumSizeForEncoding(diskNameSize, kCFStringEncodingUTF8) + 1;
    char *diskName = malloc(diskNameSize);
    if (!CFStringGetCString(partitionName, diskName, diskNameSize, kCFStringEncodingUTF8)) return NULL;
    
    io_registry_entry_t obj = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, diskName));
    if (!obj){
        IOLog1("AppleRAIDLib - getMemberInfo: IOServiceGetMatchingService failed for %s\n", diskName);
	return NULL;
    }

    memberInfo_t * mi = calloc(1, sizeof(memberInfo_t));

    mi->diskNameCF = partitionName;
    CFRetain(partitionName);

    strncpy(mi->diskName, diskName, sizeof(io_name_t));

    IORegistryEntryGetName(obj, mi->regName);

    CFMutableDictionaryRef properties = NULL;
    IOReturn result = IORegistryEntryCreateCFProperties (obj, &properties, kCFAllocatorDefault, kNilOptions);

    if (!result && properties) {

	CFNumberRef number;

	number = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR(kIOMediaSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &mi->size);

	number = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR(kIOMediaPreferredBlockSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &mi->blockSize);

	mi->isWhole = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR(kIOMediaWholeKey)) == kCFBooleanTrue;

	mi->isRAID = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR(kAppleRAIDIsRAIDKey)) == kCFBooleanTrue;

#define kIOMediaUUIDKey "UUID"
	mi->uuidString = (CFStringRef)CFDictionaryGetValue(properties, CFSTR(kIOMediaUUIDKey));
	if (mi->uuidString) CFRetain(mi->uuidString);

	strcpy(mi->wholeDiskName, mi->diskName);

	if (!mi->isWhole) {
	    char * c = mi->wholeDiskName + 4;				// skip over disk
	    while (*c != 's' && *c++);					// look for 's'
	    if (*c == 's') {
		*c = 0;							// clip off remainder
		sscanf(c+1, "%u", &mi->partitionNumber);		// get partition number
	    }
	}

    } else {
	free(mi);
	return NULL;
    }

    return mi;
}

static void
freeMemberInfo(memberInfo_t * m)
{
    if (m->diskNameCF) CFRelease(m->diskNameCF);
    if (m->uuidString) CFRelease(m->uuidString);
    free(m);
}

AppleRAIDMemberRef
AppleRAIDAddMember(CFMutableDictionaryRef setInfo, CFStringRef partitionName, CFStringRef memberType)
{
    memberInfo_t * memberInfo = getMemberInfo(partitionName);
    if (!memberInfo) return NULL;

    // whole raw disks are not supported
    if ((memberInfo->isWhole) && (!memberInfo->isRAID)) return NULL;

    // make sure we support this operation
    UInt32 version;
    CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDHeaderVersionKey));
    if (!number || !CFNumberGetValue(number, kCFNumberSInt32Type, &version) || version < 0x00020000) {
	printf("AppleRAID: This operation is not supported on earlier RAID set revisions.\n");
	return NULL;
    }

    // get/build UUID string
    CFStringRef uuidString = 0;
    if (memberInfo->isRAID) {
	uuidString = memberInfo->uuidString;
	if (uuidString) CFRetain(uuidString);
    } else {
	CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
	if (!uuid) return NULL;
	uuidString = CFUUIDCreateString(kCFAllocatorDefault, uuid);
	CFRelease(uuid);
    }
    freeMemberInfo(memberInfo);
    if (!uuidString) return NULL;

    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, memberType);
    if (!uuidArray) return NULL;
    // make sure that uuidArray is resizable
    uuidArray = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, uuidArray);
    if (!uuidArray) return NULL;
    CFDictionarySetValue(setInfo, memberType, uuidArray);

    CFStringRef pathArrayName = 0;
    if (CFStringCompare(memberType, CFSTR(kAppleRAIDMembersKey), 0) == kCFCompareEqualTo) {
	pathArrayName = CFSTR("_member names_");
    }
    if (CFStringCompare(memberType, CFSTR(kAppleRAIDSparesKey), 0) == kCFCompareEqualTo) {
	pathArrayName = CFSTR("_spare names_");
    }
    if (!pathArrayName) return NULL;

    CFMutableArrayRef pathArray = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, pathArrayName);
    if (!pathArray) {
	pathArray = (CFMutableArrayRef)CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	if (pathArray) CFDictionarySetValue(setInfo, pathArrayName, pathArray);
    }
    if (!pathArray) return NULL;

    CFArrayAppendValue(uuidArray, uuidString);
    CFArrayAppendValue(pathArray, partitionName);

    CFRelease(uuidString);

    // enable autorebuild if the set is not degraded and we are adding a spare
    if (CFStringCompare(memberType, CFSTR(kAppleRAIDSparesKey), 0) == kCFCompareEqualTo) {
	CFMutableStringRef status = (CFMutableStringRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDStatusKey));
	if (status) {
	    if (CFStringCompare(status, CFSTR(kAppleRAIDStatusOnline), 0) == kCFCompareEqualTo) {
		AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetAutoRebuildKey), (void *)kCFBooleanTrue);
	    }
	}
    }

    return (AppleRAIDMemberRef)uuidString;
}

// ***************************************************************************************************
//
// set modification
// 
// ***************************************************************************************************

#include <sys/fcntl.h>

static bool
writeHeader(CFMutableDictionaryRef setInfo, memberInfo_t * memberInfo)
{
    AppleRAIDHeaderV2 * header = calloc(1, kAppleRAIDHeaderSize);
    if (!header) return false;

    strncpy(header->raidSignature, kAppleRAIDSignature, 16);
    CFStringRef string;
    string = (CFStringRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSetUUIDKey));
    if (string) CFStringGetCString(string, header->raidUUID, 64, kCFStringEncodingUTF8);
    string = (CFStringRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDMemberUUIDKey));
    if (string) CFStringGetCString(string, header->memberUUID, 64, kCFStringEncodingUTF8);

    header->size = memberInfo->chunkCount * memberInfo->chunkSize;

    // strip any internal keys from header dictionary before writing to disk
    CFMutableDictionaryRef headerInfo = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, setInfo);
    if (!headerInfo) return false;
    CFIndex propCount = CFDictionaryGetCount(headerInfo);
    if (!propCount) return false;
    const void ** keys = calloc(propCount, sizeof(void *));
    if (!keys) return false;
    CFDictionaryGetKeysAndValues(headerInfo, keys, NULL);
    CFIndex i;
    for (i = 0; i < propCount; i++) {
	if (!CFStringHasPrefix(keys[i], CFSTR("AppleRAID-"))) {
	    CFDictionaryRemoveValue(headerInfo, keys[i]);
	}
    }

    CFDataRef setData = IOCFSerialize(headerInfo, kNilOptions);
    if (!setData) {
	IOLog1("AppleRAIDLib - serialize on setInfo failed\n");
	return false;
    }
    bcopy(CFDataGetBytePtr(setData), header->plist, CFDataGetLength(setData));
    CFRelease(headerInfo);
    CFRelease(setData);

    char devicePath[256];
    sprintf(devicePath, "/dev/%s", memberInfo->diskName);

    int fd = open(devicePath, O_RDWR, 0);
    if (fd < 0) return false;
	
    IOLog1("writeHeader %s, header offset = %llu.\n", devicePath, memberInfo->headerOffset);
    
    off_t seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
    if (seek != memberInfo->headerOffset) return false;

    int length = write(fd, header, kAppleRAIDHeaderSize);
	
    close(fd);

    free(header);
	
    if (length < kAppleRAIDHeaderSize) return false;

    return true;
}

static CFDataRef
readHeader(memberInfo_t * memberInfo)
{
    AppleRAIDHeaderV2 * header = calloc(1, kAppleRAIDHeaderSize);
    if (!header) return NULL;

    char devicePath[256];
    sprintf(devicePath, "/dev/%s", memberInfo->diskName);

    int fd = open(devicePath, O_RDONLY, 0);
    if (fd < 0) return NULL;
	
    memberInfo->headerOffset = ARHEADER_OFFSET(memberInfo->size);

//    IOLog1("readHeader %s, header offset = %llu.\n", devicePath, memberInfo->headerOffset);
    
    off_t seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
    if (seek != memberInfo->headerOffset) return NULL;

    int length = read(fd, header, kAppleRAIDHeaderSize);
	
    close(fd);
	
    if (length < kAppleRAIDHeaderSize) return NULL;
    if (strncmp(header->raidSignature, kAppleRAIDSignature, 16)) return NULL;

    CFDataRef headerData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)header, kAppleRAIDHeaderSize);
    free(header);
    
    return headerData;
}

static bool
updateLiveSet(CFMutableDictionaryRef setInfo)    
{
    CFStringRef setUUIDString = CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSetUUIDKey));

    // strip out any properties that haven't changed
    CFMutableDictionaryRef currentSet = AppleRAIDGetSetProperties(setUUIDString);
    CFMutableDictionaryRef updatedInfo = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, setInfo);
    if (!updatedInfo) return false;

    CFIndex propCount = CFDictionaryGetCount(updatedInfo);
    const void **newKeys = (const void **)malloc(2 * propCount * sizeof(void *));
    const void **newValues = newKeys + propCount;
    CFDictionaryGetKeysAndValues(updatedInfo, newKeys, newValues);

    CFIndex i;
    for (i = 0; i < propCount; i++) {
	const void * oldValue = 0;
	if (CFDictionaryGetValueIfPresent(currentSet, newKeys[i], &oldValue)) {
	    if (CFEqual(newValues[i], oldValue)) {
		CFDictionaryRemoveValue(updatedInfo, newKeys[i]);
	    }
	}
    }
    propCount = CFDictionaryGetCount(updatedInfo);

    // hm, nothing changed?
    if (!propCount) {
	IOLog1("AppleRAID - updateLiveSet: nothing was changed in the set?\n");
	return false;
    }

    // put the set uuid back in
    CFDictionarySetValue(updatedInfo, CFSTR(kAppleRAIDSetUUIDKey), setUUIDString);

    // put the sequence number back in (in case the set changed underneath us)
    const void * seqNum = CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSequenceNumberKey));
    if (!seqNum) return false;
    CFDictionarySetValue(updatedInfo, CFSTR(kAppleRAIDSequenceNumberKey), seqNum);

    // Serialize what is left
    CFDataRef setData = IOCFSerialize(updatedInfo, kNilOptions);
    if (!setData) {
	IOLog1("AppleRAID - updateLiveSet failed serializing on updatedInfo.\n");
	return false;
    }

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) {
	IOLog1("AppleRAID - updateLiveSet - failed connecting to raid controller object?\n");
	return false;
    }
	
    kern_return_t 	kr;
    char *		buffer = (char *)CFDataGetBytePtr(setData);
    CFIndex		bufferSize = CFDataGetLength(setData);
    char		updateData[0x1000];
    IOByteCount		updateDataSize = sizeof(updateData);

    if (!buffer) return false;

    IOLog1("update set changes = %s\n", buffer);

    kr = IOConnectMethodStructureIStructureO(raidControllerPort,	// an io_connect_t returned from IOServiceOpen().
					     kAppleRAIDUpdateSet,	// an index to the function in the Kernel.
					     bufferSize,		// the size of the input struct paramter.
					     &updateDataSize,		// a pointer to the size of the output struct parameter.
					     buffer,			// a pointer to the input struct parameter.
					     updateData);		// a pointer to the output struct parameter.
    
    if (kr != KERN_SUCCESS) {
	IOLog1("AppleRAID - updateLiveSet failed with %x calling client.\n", kr);
	return false;
    }

    // get back the updated sequence number
    seqNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (UInt32 *)updateData);
    if (!seqNum) return false;
    CFDictionarySetValue(setInfo, CFSTR(kAppleRAIDSequenceNumberKey), seqNum);

    CFRelease(setData);
    CFRelease(updatedInfo);
    
    AppleRAIDCloseConnection();
    
    return true;
}

// for each member initalize the following member specific values
//	kAppleRAIDMemberTypeKey - spare or member
//	kAppleRAIDMemberUUIDKey 
//	kAppleRAIDMemberIndexKey - index in set  (zero for spare)
//	kAppleRAIDChunkCountKey - size of this member
	
static bool
createNewMembers(CFMutableDictionaryRef setInfo, memberInfo_t ** memberInfo,
		 CFIndex memberCount, CFIndex spareCount,
		 CFIndex newMemberCount, CFIndex newSpareCount) 
{
    if (!memberInfo) return false;
	
    UInt32 i;
    for (i = 0; i < newMemberCount + newSpareCount; i++) {

	CFStringRef typeString= 0, uuidString = 0;
	CFNumberRef index = 0, count = 0;
	if (i < newMemberCount) {

	    typeString = CFSTR(kAppleRAIDMembersKey);

	    UInt32 memberIndex = i + memberCount;
	    index = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &memberIndex);
		
	    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDMembersKey));
	    if (!uuidArray) return false;
	    uuidString = (CFStringRef)CFArrayGetValueAtIndex(uuidArray, memberIndex);

	} else {

	    typeString = CFSTR(kAppleRAIDSparesKey);
		
	    UInt32 spareIndex = 9999;
	    index = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &spareIndex);

	    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSparesKey));
	    if (!uuidArray) return false;
	    spareIndex = i - newMemberCount + spareCount;
	    uuidString = (CFStringRef)CFArrayGetValueAtIndex(uuidArray, spareIndex);
	}
	    
	count = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &memberInfo[i]->chunkCount);

	if (typeString && index && uuidString && count) {
	    CFDictionarySetValue(setInfo, CFSTR(kAppleRAIDMemberTypeKey), typeString);
	    CFDictionarySetValue(setInfo, CFSTR(kAppleRAIDMemberIndexKey), index);
	    CFDictionarySetValue(setInfo, CFSTR(kAppleRAIDMemberUUIDKey), uuidString);
	    CFDictionarySetValue(setInfo, CFSTR(kAppleRAIDChunkCountKey), count);

	    CFRelease(index);
	    CFRelease(count);
	} else {
	    return false;
	}

	// whole raw disks are not supported
	if ((memberInfo[i]->isWhole) && (!memberInfo[i]->isRAID)) return false;

	CFStringRef partitionName = CFStringCreateWithCString(kCFAllocatorDefault, memberInfo[i]->diskName, kCFStringEncodingUTF8);
	if (!partitionName) return false;
	bool success = AppleRAIDRemoveHeaders(partitionName);
	if (!success) {
	    IOLog1("there was a problem erasing the raid headers on partition \"%s\"\n", memberInfo[i]->diskName);
	    return false;
	}
	CFRelease(partitionName);

	if (!writeHeader(setInfo, memberInfo[i])) {
	    IOLog1("AppleRAIDUpdateSet - failed while writing RAID header to partition \"%s\"\n", memberInfo[i]->diskName);
	    return false;
	}

	// if this stacked raid set we need to
	//   - force set to read the new headers 
	//   - reset the intermediate iomedia nubs (dead danglers)
	if (memberInfo[i]->isRAID) {

	    CFMutableDictionaryRef updateInfo = CFDictionaryCreateMutable(kCFAllocatorDefault,
									  3,					// count
									  &kCFTypeDictionaryKeyCallBacks,
									  &kCFTypeDictionaryValueCallBacks);
	    if (!updateInfo) return false;
	    CFDictionarySetValue(updateInfo, CFSTR(kAppleRAIDSetUUIDKey), memberInfo[i]->uuidString);
	    UInt32 zero = 0;
	    CFNumberRef seqNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &zero);
	    if (!seqNum) return false;
	    CFDictionarySetValue(updateInfo, CFSTR(kAppleRAIDSequenceNumberKey), seqNum);

	    UInt32 subCommand = kAppleRAIDUpdateResetSet;
	    CFNumberRef updateSubCommand = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &subCommand);
	    CFDictionarySetValue(updateInfo, CFSTR("_update command_"), updateSubCommand);

	    updateLiveSet(updateInfo);
	}
    }
    
    return true;
}


AppleRAIDSetRef
AppleRAIDUpdateSet(CFMutableDictionaryRef setInfo)
{
    CFStringRef setUUIDString = CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSetUUIDKey));
    CFRetain(setUUIDString);
    memberInfo_t ** memberInfo = 0;

#if DEBUG
    CFShow(setUUIDString);
#endif

    // pull out the fluff
    CFIndex memberCount = 0, spareCount = 0;
    CFIndex newMemberCount = 0, newSpareCount = 0;
    
    CFMutableArrayRef newMemberNames = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR("_member names_"));
    if (newMemberNames) {
	CFRetain(newMemberNames);
	CFDictionaryRemoveValue(setInfo, CFSTR("_member names_"));
	newMemberCount = CFArrayGetCount(newMemberNames);
    }

    CFMutableArrayRef newSpareNames = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR("_spare names_"));
    if (newSpareNames) {
	CFRetain(newSpareNames);
	CFDictionaryRemoveValue(setInfo, CFSTR("_spare names_"));
	newSpareCount = CFArrayGetCount(newSpareNames);
    }


    // if the raid set has status it is "live", get it's current member/spare counts
    bool liveSet = CFDictionaryContainsKey(setInfo, CFSTR(kAppleRAIDStatusKey));  // this only works once
    if (liveSet) {
	CFDictionaryRemoveValue(setInfo, CFSTR(kAppleRAIDStatusKey));

	CFMutableArrayRef tempMembers = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDMembersKey));
	if (tempMembers) {
	    memberCount = CFArrayGetCount(tempMembers) - newMemberCount;
	}
	CFMutableArrayRef tempSpares = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSparesKey));
	if (tempSpares) {
	    spareCount = CFArrayGetCount(tempSpares) - newSpareCount;
	}
    }

    // get info for new members and/or spares
    if (newSpareCount || newMemberCount) {

	memberInfo = calloc(newMemberCount + newSpareCount, sizeof(memberInfo_t *));
	if (!memberInfo) return NULL;
	
	UInt32 i;
	for (i = 0; i < newMemberCount + newSpareCount; i++) {
	    CFStringRef diskName;
	    if (i < newMemberCount) {
		diskName = (CFStringRef)CFArrayGetValueAtIndex(newMemberNames, i);
	    } else {
		diskName = (CFStringRef)CFArrayGetValueAtIndex(newSpareNames, i - newMemberCount);
	    }

	    memberInfo[i] = getMemberInfo(diskName);
	    if (!memberInfo[i]) return NULL;
#ifdef DEBUG
	    if (memberInfo[i]) {
		IOLog1("\t%s: regName = \"%s\" size = %lld block size = %lld whole = %s raid = %s uuid = %p\n",
		       memberInfo[i]->diskName, memberInfo[i]->regName, memberInfo[i]->size, memberInfo[i]->blockSize,
		       memberInfo[i]->isWhole?"true":"false", memberInfo[i]->isRAID?"true":"false", memberInfo[i]->uuidString);
	    }
#endif
	}
	if (newMemberNames) CFRelease(newMemberNames);
	if (newSpareNames) CFRelease(newSpareNames);

	bool sizesCanVary = (CFBooleanRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSizesCanVaryKey)) == kCFBooleanTrue;

	// find the smallest member (or spare)
	UInt64 smallestSize = 0;
	if (liveSet) {
	    // calculate the minimum required size for a member partition in this set
	    UInt64 chunkSize = 0, chunkCount = 0;
	    CFNumberRef number;
	    number = (CFNumberRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDChunkSizeKey));
	    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &chunkSize);

	    number = (CFNumberRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDChunkCountKey));
	    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &chunkCount);

	    if (!chunkSize || !chunkCount) return NULL;

	    smallestSize = chunkCount * chunkSize + (UInt64)kAppleRAIDHeaderSize;
	} else {
	    smallestSize = memberInfo[0]->size;
	}
	if (!sizesCanVary) {
	    for (i = 0; i < newMemberCount + newSpareCount; i++) {
		// XXX if smaller than minimum raid set size  (1MB ?)
		if (liveSet) {
		    if (memberInfo[i]->size < smallestSize) {
			IOLog1("AppleRAIDUpdateSet() new member is too small to add to set.\n");
			return NULL;
		    }
		} else {
		    if (memberInfo[i]->size < smallestSize) smallestSize = memberInfo[i]->size;
		}
	    }
	    IOLog1("smallest member size %lld\n", smallestSize);
	}

	// determine each partition's chunk count
	UInt64 chunkSize = 0;
	CFNumberRef number;
	number = (CFNumberRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDChunkSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &chunkSize);
	if (!chunkSize) return NULL;

	for (i = 0; i < newMemberCount + newSpareCount; i++) {
	    memberInfo[i]->headerOffset = ARHEADER_OFFSET(memberInfo[i]->size);
	    if (sizesCanVary) {
		memberInfo[i]->chunkCount = ARCHUNK_COUNT(memberInfo[i]->size, chunkSize);
	    } else {
		memberInfo[i]->chunkCount = ARCHUNK_COUNT(smallestSize, chunkSize);
	    }
	    memberInfo[i]->chunkSize = chunkSize;
	}
    }

    // warn controller of set change prior to adding new members
    // add give the set a chance to reject anything it does not like

    if (liveSet) {
	if (!updateLiveSet(setInfo)) return NULL;
    }

    // write out headers on new members/spares
	
    if (newSpareCount || newMemberCount) {
	if (!createNewMembers(setInfo, memberInfo,
			      memberCount, spareCount,
			      newMemberCount, newSpareCount)) return NULL;
    }

    if (newSpareCount || newMemberCount) {
	UInt32 i;
	for (i=0; i < newSpareCount + newMemberCount; i++) {
	    freeMemberInfo(memberInfo[i]);
	}
	free(memberInfo);
    }
    
    return setUUIDString;
}

// ***************************************************************************************************
//
// set and member deletion
// 
// ***************************************************************************************************

bool 
AppleRAIDRemoveHeaders(CFStringRef partitionName)
{
    memberInfo_t * memberInfo = getMemberInfo(partitionName);
    if (!memberInfo) return false;

    // look for block zero header (old raid)
    
    AppleRAIDHeaderV2 * header = calloc(1, kAppleRAIDHeaderSize);
    if (!header) return false;

    char devicePath[256];
    sprintf(devicePath, "/dev/%s", memberInfo->diskName);

    int fd = open(devicePath, O_RDWR, 0);
    if (fd < 0) return false;

    // look for v1 style header
    memberInfo->headerOffset = 0;
    {
	IOLog2("AppleRAIDRemoveHeaders %s, scaning header offset = %llu.\n", devicePath, memberInfo->headerOffset);
    
	off_t seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
	if (seek != memberInfo->headerOffset) return false;

	int length = read(fd, header, kAppleRAIDHeaderSize);
	if (length < kAppleRAIDHeaderSize) return false;

	if (!strncmp(header->raidSignature, kAppleRAIDSignature, 16)) {
	    IOLog1("AppleRAIDRemoveHeaders %s, found header at offset = %llu.\n", devicePath, memberInfo->headerOffset);

	    bzero(header, kAppleRAIDHeaderSize);

	    seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
	    if (seek != memberInfo->headerOffset) return false;
	    
	    length = write(fd, header, kAppleRAIDHeaderSize);
	    if (length < kAppleRAIDHeaderSize) return false;
	}
    }

    // scan for nested headers
    memberInfo->headerOffset = ARHEADER_OFFSET(memberInfo->size);
    int count = 5;
    while (memberInfo->headerOffset && count) {
	IOLog2("AppleRAIDRemoveHeaders %s, scanning header offset = %llu.\n", devicePath, memberInfo->headerOffset);
    
	off_t seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
	if (seek != memberInfo->headerOffset) return false;

	int length = read(fd, header, kAppleRAIDHeaderSize);
	if (length < kAppleRAIDHeaderSize) return false;

	if (!strncmp(header->raidSignature, kAppleRAIDSignature, 16)) {
	    IOLog1("AppleRAIDRemoveHeaders %s, found header at offset = %llu.\n", devicePath, memberInfo->headerOffset);

	    UInt64 nextOffset = header->size;

	    bzero(header, kAppleRAIDHeaderSize);

	    seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
	    if (seek != memberInfo->headerOffset) return false;
	    
	    length = write(fd, header, kAppleRAIDHeaderSize);
	    if (length < kAppleRAIDHeaderSize) return false;

	    memberInfo->headerOffset = ARHEADER_OFFSET(nextOffset);

	} else {

	    memberInfo->headerOffset = 0;
	}
	count--;
    }
	
    close(fd);
    free(memberInfo);

    return true;
}


bool
AppleRAIDRemoveMember(CFMutableDictionaryRef setInfo, AppleRAIDMemberRef member)
{
    // find the member or spare
    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDMembersKey));
    CFMutableArrayRef uuidArray2 = 0;
    if (!uuidArray) return NULL;
    CFIndex count = 0;
    CFIndex index;

again:

    count = CFArrayGetCount(uuidArray);
    for (index = 0; index < count; index++) {
	CFStringRef uuidString = (CFStringRef)CFArrayGetValueAtIndex(uuidArray, index);
	if (CFStringCompare(member, uuidString, 0) == kCFCompareEqualTo) {
	    CFArraySetValueAtIndex(uuidArray, index, CFSTR(kAppleRAIDDeletedUUID));
	    return true;
	}
    }

    // same for spares array
    if (!uuidArray2) {
	uuidArray2 = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSparesKey));
	if (uuidArray2 && CFArrayGetCount(uuidArray2)) {
	    uuidArray = uuidArray2;
	    goto again;
	}
    }

    return false;
}


bool
AppleRAIDDestroySet(AppleRAIDSetRef setName)
{
    CFMutableDictionaryRef setInfo = AppleRAIDGetSetProperties(setName);
    if (!setInfo) return false;

    // find the members/spares in the this set
    CFMutableArrayRef members = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDMembersKey));
    CFIndex memberCount = members ? CFArrayGetCount(members) : 0;
    CFMutableArrayRef spares = (CFMutableArrayRef)CFDictionaryGetValue(setInfo, CFSTR(kAppleRAIDSparesKey));
    CFIndex spareCount = spares ? CFArrayGetCount(spares) : 0;

    UInt32 memberInfoCount = 0;
    memberInfo_t ** memberInfo = calloc(memberCount + spareCount, sizeof(memberInfo_t *));
    
    bool twice = false;
    CFIndex i;
 
getMoreMembers:
    
    for (i=0; i < memberCount; i++) {
	CFStringRef memberName = (CFStringRef)CFArrayGetValueAtIndex(members, i);
	if (memberName) {
	    CFMutableDictionaryRef memberProps = AppleRAIDGetMemberProperties(memberName);	
	    if (memberProps) {
		CFStringRef partitionName = (CFStringRef)CFDictionaryGetValue(memberProps, CFSTR(kIOBSDNameKey));
		if (partitionName) {

		    memberInfo_t * info = getMemberInfo(partitionName);
		    if (info) {
			memberInfo[memberInfoCount++] = info;
		    } else {
			IOLog1("AppleRAIDDestroySet - getMemberInfo failed for %s.\n", info->diskName);
		    }
		}
		CFRelease(memberProps);
	    }
	}
    }

    if (!twice) {
	twice = true;
	members = spares;
	memberCount = spareCount;
	goto getMoreMembers;
    }

    UInt32 subCommand = kAppleRAIDUpdateDestroySet;
    CFNumberRef destroySubCommand = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &subCommand);
    CFDictionarySetValue(setInfo, CFSTR("_update command_"), destroySubCommand);

    if (!updateLiveSet(setInfo)) return false;

    CFRelease(setInfo);

    // clean up
    for (i=0; i < memberInfoCount; i++) {
	freeMemberInfo(memberInfo[i]);
    }

    return true;
}


UInt64 AppleRAIDGetUsableSize(UInt64 partitionSize, UInt64 chunkSize)
{
    UInt64 chunkCount = ARCHUNK_COUNT(partitionSize, chunkSize);

    return chunkCount * chunkSize;
}


CFDataRef
AppleRAIDDumpHeader(CFStringRef partitionName)
{
    memberInfo_t * memberInfo = getMemberInfo(partitionName);
    if (!memberInfo) return NULL;

    CFDataRef data = readHeader(memberInfo);

    freeMemberInfo(memberInfo);

    return data;
}
