/*
 * Copyright (c) 2001-2007 Apple Inc. All rights reserved.
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
#include <sys/param.h>

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
#include "AppleLVMGroup.h"
#include "AppleLVMVolume.h"
#include "AppleRAIDConcatSet.h"
#include "AppleRAIDMirrorSet.h"
#include "AppleRAIDStripeSet.h"

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
        IOLog2("Couldn't find any matches.\n");
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

    kr = IOConnectCallStructMethod(gRAIDControllerPort, kAppleRAIDClientOpen, 0, 0, 0, 0);
    UInt32 count = 0;
    // retry for 1 minute
    while (kr == kIOReturnExclusiveAccess && count < 60)
    {
#ifdef DEBUG
	if ((count % 15) == 0) IOLog1("AppleRAID: controller object is busy, retrying...\n");
#endif
	(void)sleep(1);
	kr = IOConnectCallStructMethod(gRAIDControllerPort, kAppleRAIDClientOpen, 0, 0, 0, 0);
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

    kr = IOConnectCallStructMethod(gRAIDControllerPort, kAppleRAIDClientClose, 0, 0, 0, 0);
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
// notifications
// 
// ***************************************************************************************************

typedef struct changeInfo {
    io_object_t			service;
    mach_port_t     		notifier;
    CFStringRef			uuidString;
} changeInfo_t;

static IONotificationPortRef	gNotifyPort;
static io_iterator_t		gRAIDSetIter;
static io_iterator_t		gLogicalVolumeIter;

static void
raidSetChanged(void *refcon, io_service_t service, natural_t messageType, void *messageArgument)
{
    changeInfo_t * changeInfo = (changeInfo_t *)refcon;

    if (messageType == kIOMessageServiceIsTerminated) {

	// broadcast "raid set died" notification
	CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					     CFSTR(kAppleRAIDNotificationSetTerminated),
					     changeInfo->uuidString,
					     NULL,           // CFDictionaryRef userInfo
					     false);

	IOObjectRelease(changeInfo->service);
	IOObjectRelease(changeInfo->notifier);
	CFRelease(changeInfo->uuidString);
	free(changeInfo);
	
	return;
    }

    IOLog2("raidSetChanged: messageType %08x, arg %08lx\n", messageType, (UInt32) messageArgument);

    // we only care about messages from the raid driver, toss all others.
    if (messageType != kAppleRAIDMessageSetChanged) return;

    // broadcast "raid set changed" notification
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					 CFSTR(kAppleRAIDNotificationSetChanged),
					 changeInfo->uuidString,
					 NULL,           // CFDictionaryRef userInfo
					 false);
}

void static
raidSetDetected(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_service_t		newSet;
    changeInfo_t *		changeInfo;
    CFMutableDictionaryRef  	registryEntry;
    CFStringRef			uuidString;

    while (newSet = IOIteratorNext(iterator)) {

	// fetch a copy of the in kernel registry object
	kr = IORegistryEntryCreateCFProperties(newSet, &registryEntry, kCFAllocatorDefault, 0);
	if (kr != KERN_SUCCESS) return;

	// get the set's UUID name, for stacked sets the member uuid is the correct UUID
	// to use for this notification, it also works for regular raid sets.
	uuidString = CFDictionaryGetValue(registryEntry, CFSTR(kAppleRAIDMemberUUIDKey));
	if (uuidString) uuidString = CFStringCreateCopy(NULL, uuidString);
	CFRelease(registryEntry);
	if (!uuidString) return;

	changeInfo = calloc(1, sizeof(changeInfo_t));
	changeInfo->service = newSet;
	changeInfo->uuidString = uuidString;

	// set up notifications for any changes to this set
	kr = IOServiceAddInterestNotification(gNotifyPort, newSet, kIOGeneralInterest,
					      &raidSetChanged, (void *)changeInfo,
					      &changeInfo->notifier);
	if (kr != KERN_SUCCESS) {
	    free(changeInfo);
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


static void
logicalVolumeChanged(void *refcon, io_service_t service, natural_t messageType, void *messageArgument)
{
    changeInfo_t * changeInfo = (changeInfo_t *)refcon;

    if (messageType == kIOMessageServiceIsTerminated) {

	// broadcast "logical volume died" notification
	CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					     CFSTR(kAppleLVMNotificationVolumeTerminated),
					     changeInfo->uuidString,
					     NULL,           // CFDictionaryRef userInfo
					     false);

	IOObjectRelease(changeInfo->service);
	IOObjectRelease(changeInfo->notifier);
	CFRelease(changeInfo->uuidString);
	free(changeInfo);
	
	return;
    }

    IOLog2("logicalVolumeChanged: messageType %08x, arg %08lx\n", messageType, (UInt32) messageArgument);

    // we only care about messages from the raid driver, toss all others.
    if (messageType != kAppleLVMMessageVolumeChanged) return;

    // broadcast "logical volume changed" notification
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					 CFSTR(kAppleLVMNotificationVolumeChanged),
					 changeInfo->uuidString,
					 NULL,           // CFDictionaryRef userInfo
					 false);
}

void static
logicalVolumeDetected(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_service_t		newVolume;
    changeInfo_t *		changeInfo;
    CFMutableDictionaryRef  	registryEntry;
    CFStringRef			uuidString;

    while (newVolume = IOIteratorNext(iterator)) {

	// fetch a copy of the in kernel registry object
	kr = IORegistryEntryCreateCFProperties(newVolume, &registryEntry, kCFAllocatorDefault, 0);
	if (kr != KERN_SUCCESS) return;

	// get the volume's UUID name
	uuidString = CFDictionaryGetValue(registryEntry, CFSTR("UUID"));
	if (uuidString) uuidString = CFStringCreateCopy(NULL, uuidString);
	CFRelease(registryEntry);
	if (!uuidString) return;

	changeInfo = calloc(1, sizeof(changeInfo_t));
	changeInfo->service = newVolume;
	changeInfo->uuidString = uuidString;

	// set up notifications for any changes to this volume
	kr = IOServiceAddInterestNotification(gNotifyPort, newVolume, kIOGeneralInterest,
					      &logicalVolumeChanged, (void *)changeInfo,
					      &changeInfo->notifier);
	if (kr != KERN_SUCCESS) {
	    free(changeInfo);
	    return;
	}

	// broadcast "new raid volume" notification
	CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
					     CFSTR(kAppleLVMNotificationVolumeDiscovered),
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

    gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);
    
    //
    // set up raid set notifications
    //

    classToMatch = IOServiceMatching(kAppleRAIDSetClassName);
    if (classToMatch == NULL)
    {
        IOLog1("IOServiceMatching returned a NULL dictionary.\n");
	return kIOReturnNoResources;
    }

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
    //
    // set up logical volume notifications
    //

    classToMatch = IOServiceMatching(kAppleLogicalVolumeClassName);
    if (classToMatch == NULL)
    {
        IOLog1("IOServiceMatching returned a NULL dictionary.\n");
	return kIOReturnNoResources;
    }

    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOFirstMatchNotification,
                                            classToMatch,
                                            logicalVolumeDetected,
                                            NULL,
                                            &gLogicalVolumeIter );
    if (kr != KERN_SUCCESS)
    {
        IOLog1("IOServiceAddMatchingNotification returned %d\n", kr);
	return kr;
    }
    
    logicalVolumeDetected(NULL, gLogicalVolumeIter);	// Iterate once to get already-present
							// devices and arm the notification
    return kr;
}

kern_return_t
AppleRAIDDisableNotifications(void)
{

    IONotificationPortDestroy(gNotifyPort);

    if (gRAIDSetIter) {
        IOObjectRelease(gRAIDSetIter);
        gRAIDSetIter = 0;
    }
    if (gLogicalVolumeIter) {
        IOObjectRelease(gLogicalVolumeIter);
        gLogicalVolumeIter = 0;
    }
    
    return KERN_SUCCESS;
}


// ***************************************************************************************************
//
// list of set, getSet, getMember
// 
// ***************************************************************************************************

typedef struct memberInfo {
    CFStringRef diskNameCF;
    io_name_t	diskName;
    io_name_t	wholeDiskName;
    unsigned int partitionNumber;
    char	devicePath[256];
    io_name_t	regName;

    // from media
    UInt64	size;
    UInt64	blockSize;
    bool	isWhole;
    bool	isRAID;
    CFStringRef uuidString;
    UInt64	headerOffset;

    // from header
    UInt64	chunkCount;
    UInt64	chunkSize;
    UInt64	primaryMetaDataSize;
    UInt64	secondaryMetaDataSize;
    UInt64	startOffset;		// jbod & lvg

    AppleRAIDPrimaryOnDisk * primaryData;
    void *	secondaryData;
} memberInfo_t;

static void
freeMemberInfo(memberInfo_t * m)
{
    if (m->diskNameCF) CFRelease(m->diskNameCF);
    if (m->uuidString) CFRelease(m->uuidString);
    if (m->primaryData) free(m->primaryData);
    if (m->secondaryData) free(m->secondaryData);
    free(m);
}

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

    strlcpy(mi->diskName, diskName, sizeof(io_name_t));
    snprintf(mi->devicePath, sizeof(mi->devicePath), "/dev/%s", diskName);

    IORegistryEntryGetName(obj, mi->regName);

    CFMutableDictionaryRef properties = NULL;
    IOReturn result = IORegistryEntryCreateCFProperties(obj, &properties, kCFAllocatorDefault, kNilOptions);

    if (!result && properties) {

	CFNumberRef number;

	number = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR(kIOMediaSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &mi->size);

	mi->headerOffset = ARHEADER_OFFSET(mi->size);
	
	number = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR(kIOMediaPreferredBlockSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &mi->blockSize);

	mi->isWhole = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR(kIOMediaWholeKey)) == kCFBooleanTrue;

	mi->isRAID = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR(kAppleRAIDIsRAIDKey)) == kCFBooleanTrue;

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
	freeMemberInfo(mi);
	return NULL;
    }

    return mi;
}

typedef struct setInfo {
    CFMutableDictionaryRef	setProps;
    CFIndex			memberCount;
    CFMutableArrayRef		members;
    CFMutableDictionaryRef *	memberProps;
    memberInfo_t **		memberInfo;
//    CFIndex			spareCount;
//    CFMutableArrayRef		spares;
//    CFMutableDictionaryRef	spareProps;
//    memberInfo_t **		spareInfo;
} setInfo_t;

static void
freeSetInfo(setInfo_t *setInfo)
{
    CFIndex i;
    if (setInfo->memberProps) {
	for (i=0; i < setInfo->memberCount; i++) {
	    if (setInfo->memberProps[i]) CFRelease(setInfo->memberProps[i]);
	}
	free(setInfo->memberProps);
    }
    if (setInfo->memberInfo) {
	for (i=0; i < setInfo->memberCount; i++) {
	    if (setInfo->memberInfo[i]) freeMemberInfo(setInfo->memberInfo[i]);
	}
	free(setInfo->memberInfo);
    }

    // XXX same for spares

    if (setInfo->setProps) CFRelease(setInfo->setProps);
    free(setInfo);
}

static setInfo_t *
getSetInfo(AppleRAIDSetRef setRef)
{
    setInfo_t * setInfo = calloc(1, sizeof(setInfo_t));
    if (!setInfo) return NULL;

    setInfo->setProps = AppleRAIDGetSetProperties(setRef);
    if (!setInfo->setProps) goto error;

    // find the members/spares in the this set
    setInfo->members = (CFMutableArrayRef)CFDictionaryGetValue(setInfo->setProps, CFSTR(kAppleRAIDMembersKey));
    setInfo->memberCount = setInfo->members ? CFArrayGetCount(setInfo->members) : 0;
//    setInfo->spares = (CFMutableArrayRef)CFDictionaryGetValue(setInfo->setProps, CFSTR(kAppleRAIDSparesKey));
//    setInfo->spareCount = setInfo->spares ? CFArrayGetCount(setInfo->spares) : 0;

    if (setInfo->memberCount) {
	setInfo->memberInfo = calloc(setInfo->memberCount, sizeof(memberInfo_t *));
	setInfo->memberProps = calloc(setInfo->memberCount, sizeof(CFMutableDictionaryRef));
    }
//    if (setInfo->spareCount) {
//	setInfo->spareInfo = calloc(setInfo->spareCount, sizeof(memberInfo_t *));
//	setInfo->spareProps = calloc(setInfo->spareCount, sizeof(CFMutableDictionaryRef));
//    }
    
    CFIndex i;
    for (i=0; i < setInfo->memberCount; i++) {
	CFStringRef member = (CFStringRef)CFArrayGetValueAtIndex(setInfo->members, i);
	if (member) {
	    setInfo->memberProps[i] = AppleRAIDGetMemberProperties(member);
	    if (setInfo->memberProps[i]) {
		CFStringRef partitionName = (CFStringRef)CFDictionaryGetValue(setInfo->memberProps[i], CFSTR(kIOBSDNameKey));
		if (partitionName) {
		    setInfo->memberInfo[i] = getMemberInfo(partitionName);
		}

		CFMutableDictionaryRef props = setInfo->memberProps[i];
		memberInfo_t * info = setInfo->memberInfo[i];

		CFNumberRef number;
		number = (CFNumberRef)CFDictionaryGetValue(setInfo->setProps, CFSTR(kAppleRAIDChunkSizeKey));   // per set
		if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &info->chunkSize);

		number = (CFNumberRef)CFDictionaryGetValue(props, CFSTR(kAppleRAIDChunkCountKey));		// per member
		if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &info->chunkCount);
		
		number = (CFNumberRef)CFDictionaryGetValue(props, CFSTR(kAppleRAIDPrimaryMetaDataUsedKey));	// per member
		if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &info->primaryMetaDataSize);

		number = (CFNumberRef)CFDictionaryGetValue(props, CFSTR(kAppleRAIDSecondaryMetaDataSizeKey));	// per member
		if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &info->secondaryMetaDataSize);

		number = (CFNumberRef)CFDictionaryGetValue(props, CFSTR(kAppleRAIDMemberStartKey));		// per member
		if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &info->startOffset);
	    }
	}
    }

    // XXX same for spares

    return setInfo;
    
error:
    freeSetInfo(setInfo);
    return NULL;
}


#define kMaxIOConnectTransferSize  4096

CFMutableArrayRef
AppleRAIDGetListOfSets(UInt32 filter)
{
    kern_return_t 	kr;
    size_t		listSize = kMaxIOConnectTransferSize;
    CFMutableArrayRef 	theList = NULL;

    char * listString = (char *)malloc((int)listSize);

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleRAIDGetListOfSets,	// an index to the function in the Kernel.
				   &filter,			// input
				   sizeof(filter),		// input size
				   listString,			// output
				   &listSize);			// output size (in/out)
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
    size_t		propSize = kMaxIOConnectTransferSize;
    CFMutableDictionaryRef props = NULL;
    size_t		bufferSize = kAppleRAIDMaxUUIDStringSize;
    char		buffer[bufferSize];

    if (!CFStringGetCString(setName, buffer, bufferSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleRAIDGetSetProperties() CFStringGetCString failed?\n");
	return NULL;
    }

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;
    
    char * propString = (char *)malloc(propSize);

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleRAIDGetSetProperties,	// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   propString,			// output
				   &propSize);			// output size (in/out)

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
    size_t		propSize = kMaxIOConnectTransferSize;
    CFMutableDictionaryRef props = NULL;
    size_t		bufferSize = kAppleRAIDMaxUUIDStringSize;
    char		buffer[bufferSize];

    if (!CFStringGetCString(memberName, buffer, bufferSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleRAIDGetMemberProperties() CFStringGetCString failed?\n");
	return NULL;
    }
    
    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;

    char * propString = (char *)malloc(propSize);

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleRAIDGetMemberProperties,	// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   propString,			// output
				   &propSize);			// output size (in/out)

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
	"<key>" kAppleRAIDLevelNameKey "</key>"		"<string>" kAppleRAIDLevelNameStripe "</string> \n"
	"<key>" kAppleRAIDMemberTypeKey "</key>"	"<array> \n"
								"<string>" kAppleRAIDMembersKey "</string> \n"
							"</array> \n"
	"<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSetQuickRebuildKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSetTimeoutKey "</key>"	"<integer size=\"32\">0</integer> \n"
	"<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x8000</integer> \n"

	"<key>" kAppleRAIDCanAddMembersKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDCanAddSparesKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSizesCanVaryKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>" kAppleRAIDRemovalNone "</string> \n"

	"<key>" kAppleRAIDCanBeConvertedToKey "</key>"	"<false/> \n"
    "</dict> \n"
    "<dict> \n"
	"<key>" kAppleRAIDLevelNameKey "</key>"		"<string>" kAppleRAIDLevelNameMirror "</string> \n"
	"<key>" kAppleRAIDMemberTypeKey "</key>"	"<array> \n"
								"<string>" kAppleRAIDMembersKey "</string> \n"
								"<string>" kAppleRAIDSparesKey "</string> \n"
							"</array> \n"
	"<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDSetQuickRebuildKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDSetTimeoutKey "</key>"	"<integer size=\"32\">30</integer> \n"
	"<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x8000</integer> \n"

	"<key>" kAppleRAIDCanAddMembersKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDCanAddSparesKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDSizesCanVaryKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>" kAppleRAIDRemovalAnyMember "</string> \n"

	"<key>" kAppleRAIDCanBeConvertedToKey "</key>"	"<true/> \n"
    "</dict> \n"
    "<dict> \n"
	"<key>" kAppleRAIDLevelNameKey "</key>"		"<string>" kAppleRAIDLevelNameConcat "</string> \n"
	"<key>" kAppleRAIDMemberTypeKey "</key>"	"<array> \n"
								"<string>" kAppleRAIDMembersKey "</string> \n"
							"</array> \n"
	"<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSetQuickRebuildKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSetTimeoutKey "</key>"	"<integer size=\"32\">0</integer> \n"
	"<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x8000</integer> \n"
    
	"<key>" kAppleRAIDCanAddMembersKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDCanAddSparesKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSizesCanVaryKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>" kAppleRAIDRemovalLastMember "</string> \n"

	"<key>" kAppleRAIDCanBeConvertedToKey "</key>"	"<true/> \n"
    "</dict> \n"
    "<dict> \n"
	"<key>" kAppleRAIDLevelNameKey "</key>"		"<string>" kAppleRAIDLevelNameLVG "</string> \n"
	"<key>" kAppleRAIDMemberTypeKey "</key>"	"<array> \n"
								"<string>" kAppleRAIDMembersKey "</string> \n"
							"</array> \n"
	"<key>" kAppleRAIDSetAutoRebuildKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSetTimeoutKey "</key>"	"<integer size=\"32\">0</integer> \n"
	"<key>" kAppleRAIDChunkSizeKey "</key>"		"<integer size=\"64\">0x8000</integer> \n"
    
	"<key>" kAppleRAIDCanAddMembersKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDCanAddSparesKey "</key>"	"<false/> \n"
	"<key>" kAppleRAIDSizesCanVaryKey "</key>"	"<true/> \n"
	"<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>" kAppleRAIDRemovalNone "</string> \n"

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
    "<key>" kAppleRAIDSetQuickRebuildKey "</key>"	"<false/> \n"					// mirror, raid v only
    "<key>" kAppleRAIDSetTimeoutKey "</key>"		"<integer size=\"32\">30</integer> \n"		// mirror, raid v only

    "<key>" kAppleRAIDCanAddMembersKey "</key>"		"<false/> \n"					// mirror, concat only
    "<key>" kAppleRAIDCanAddSparesKey "</key>"		"<false/> \n"
    "<key>" kAppleRAIDSizesCanVaryKey "</key>"		"<false/> \n"					// true for concat only
    "<key>" kAppleRAIDRemovalAllowedKey "</key>"	"<string>internal error</string> \n"

    "<key>" kAppleRAIDSetContentHintKey "</key>"	"<string/> \n"
" </dict> \n";


CFMutableDictionaryRef
AppleRAIDCreateSet(CFStringRef raidType, CFStringRef setName)
{
    CFStringRef errorString;

    CFMutableDictionaryRef setProps = (CFMutableDictionaryRef)IOCFUnserialize(defaultCreateSetBuffer, kCFAllocatorDefault, 0, &errorString);
    if (!setProps) {
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
    
    CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDSetUUIDKey), uuidString);  CFRelease(uuidString);
    CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDLevelNameKey), raidType);
    CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDSetNameKey), setName);

    // XXX could just pull these from GetSetDescriptions
    // AppleRAIDDefaultSetPropForKey(raidType, key);

    // overrides
    if (CFEqual(raidType, CFSTR("Stripe"))) {
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDRemovalAllowedKey), CFSTR(kAppleRAIDRemovalNone));
    }
    if (CFEqual(raidType, CFSTR("Concat"))) {
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDCanAddMembersKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDSizesCanVaryKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDRemovalAllowedKey), CFSTR(kAppleRAIDRemovalLastMember));
    }
    if (CFEqual(raidType, CFSTR("Mirror"))) {
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDCanAddMembersKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDCanAddSparesKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDRemovalAllowedKey), CFSTR(kAppleRAIDRemovalAnyMember));
    }
    if (CFEqual(raidType, CFSTR("LVG"))) {
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDCanAddMembersKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDSizesCanVaryKey), kCFBooleanTrue);
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDRemovalAllowedKey), CFSTR(kAppleRAIDRemovalAnyMember));
	CFDictionaryReplaceValue(setProps, CFSTR(kAppleRAIDSetContentHintKey), CFSTR(kAppleRAIDNoMediaExport));
    }
    
    return setProps;
}

bool
AppleRAIDModifySet(CFMutableDictionaryRef setProps, CFStringRef key, void * value)
{
    CFStringRef errorString;

//  AppleRAIDDefaultSetPropForKey(raidType, key);
    
    CFMutableDictionaryRef defaultSetProps = (CFMutableDictionaryRef)IOCFUnserialize(defaultCreateSetBuffer, kCFAllocatorDefault, 0, &errorString);
    if (!defaultSetProps) {
	CFIndex	bufferSize = CFStringGetLength(errorString);
	bufferSize = CFStringGetMaximumSizeForEncoding(bufferSize, kCFStringEncodingUTF8) + 1;
	char *buffer = malloc(bufferSize);
	if (!buffer || !CFStringGetCString(errorString, buffer, bufferSize, kCFStringEncodingUTF8)) {
	    goto error;
	}

	IOLog1("AppleRAIDModifySet - failed while parsing create set template file, error: %s\n", buffer);
	CFRelease(errorString);
	goto error;
    }

    const void * defaultValue = CFDictionaryGetValue(defaultSetProps, key);
    if (!defaultValue) goto error;

    if (CFGetTypeID(defaultValue) != CFGetTypeID(value)) goto error;

    // XXX if live, changing the chunksize means we have to change chunk count
    
    CFDictionarySetValue(setProps, key, value);

    CFRelease(defaultSetProps);

    return true;

error:
    if (defaultSetProps) CFRelease(defaultSetProps);
    return false;
}

// ***************************************************************************************************
//
// member creation
// 
// ***************************************************************************************************

AppleRAIDMemberRef
AppleRAIDAddMember(CFMutableDictionaryRef setProps, CFStringRef partitionName, CFStringRef memberType)
{
    memberInfo_t * memberInfo = getMemberInfo(partitionName);
    if (!memberInfo) return NULL;

    // whole raw disks are not supported
    if ((memberInfo->isWhole) && (!memberInfo->isRAID)) return NULL;

    // make sure we support this operation
    UInt32 version;
    CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDHeaderVersionKey));
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

    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setProps, memberType);
    if (!uuidArray) return NULL;
    // make sure that uuidArray is resizable
    uuidArray = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, uuidArray);
    if (!uuidArray) return NULL;
    CFDictionarySetValue(setProps, memberType, uuidArray);

    CFStringRef pathArrayName = 0;
    if (CFStringCompare(memberType, CFSTR(kAppleRAIDMembersKey), 0) == kCFCompareEqualTo) {
	pathArrayName = CFSTR("_member names_");
    }
    if (CFStringCompare(memberType, CFSTR(kAppleRAIDSparesKey), 0) == kCFCompareEqualTo) {
	pathArrayName = CFSTR("_spare names_");
    }
    if (!pathArrayName) return NULL;

    CFMutableArrayRef pathArray = (CFMutableArrayRef)CFDictionaryGetValue(setProps, pathArrayName);
    if (!pathArray) {
	pathArray = (CFMutableArrayRef)CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	if (pathArray) CFDictionarySetValue(setProps, pathArrayName, pathArray);
    }
    if (!pathArray) return NULL;

    CFArrayAppendValue(uuidArray, uuidString);
    CFArrayAppendValue(pathArray, partitionName);

    CFRelease(uuidString);

    // enable autorebuild if the set is not degraded and we are adding a spare
    if (CFStringCompare(memberType, CFSTR(kAppleRAIDSparesKey), 0) == kCFCompareEqualTo) {
	CFMutableStringRef status = (CFMutableStringRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDStatusKey));
	if (status) {
	    if (CFStringCompare(status, CFSTR(kAppleRAIDStatusOnline), 0) == kCFCompareEqualTo) {
		AppleRAIDModifySet(setProps, CFSTR(kAppleRAIDSetAutoRebuildKey), (void *)kCFBooleanTrue);
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
writeHeader(CFMutableDictionaryRef memberProps, memberInfo_t * memberInfo)
{
    AppleRAIDHeaderV2 * header = calloc(1, kAppleRAIDHeaderSize);
    if (!header) return false;

    strlcpy(header->raidSignature, kAppleRAIDSignature, sizeof(header->raidSignature));
    CFStringRef string;
    string = (CFStringRef)CFDictionaryGetValue(memberProps, CFSTR(kAppleRAIDSetUUIDKey));
    if (string) CFStringGetCString(string, header->raidUUID, 64, kCFStringEncodingUTF8);
    string = (CFStringRef)CFDictionaryGetValue(memberProps, CFSTR(kAppleRAIDMemberUUIDKey));
    if (string) CFStringGetCString(string, header->memberUUID, 64, kCFStringEncodingUTF8);

    header->size = memberInfo->chunkCount * memberInfo->chunkSize;
    ByteSwapHeaderV2(header);

    // strip any internal keys from header dictionary before writing to disk
    CFMutableDictionaryRef headerInfo = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, memberProps);
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
    CFDictionaryRemoveValue(headerInfo, CFSTR(kAppleRAIDSetQuickRebuildKey));	// redundant
    CFDictionaryRemoveValue(headerInfo, CFSTR(kAppleRAIDLVGExtentsKey));	// redundant
    CFDictionaryRemoveValue(headerInfo, CFSTR(kAppleRAIDLVGVolumeCountKey));	// redundant
    CFDictionaryRemoveValue(headerInfo, CFSTR(kAppleRAIDLVGFreeSpaceKey));	// redundant

    CFDataRef setData = IOCFSerialize(headerInfo, kNilOptions);
    if (!setData) {
	IOLog1("AppleRAIDLib - serialize on memberProps failed\n");
	return false;
    }
    bcopy(CFDataGetBytePtr(setData), header->plist, CFDataGetLength(setData));
    CFRelease(headerInfo);
    CFRelease(setData);

    int fd = open(memberInfo->devicePath, O_RDWR, 0);
    if (fd < 0) return false;
	
    IOLog1("writeHeader %s, header offset = %llu.\n", memberInfo->devicePath, memberInfo->headerOffset);
    
    off_t seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
    if (seek != memberInfo->headerOffset) goto ioerror;

    int length = write(fd, header, kAppleRAIDHeaderSize);
    if (length < kAppleRAIDHeaderSize) goto ioerror;
    
    close(fd);
    free(header);
    return true;

ioerror:
    close(fd);
    free(header);
    return false;
}

static CFDataRef
readHeader(memberInfo_t * memberInfo)
{
    CFDataRef headerData = NULL;

    int fd = open(memberInfo->devicePath, O_RDONLY, 0);
    if (fd < 0) return NULL;
	
    AppleRAIDHeaderV2 * header = calloc(1, kAppleRAIDHeaderSize);
    if (!header) goto error;

//    IOLog1("readHeader %s, header offset = %llu.\n", memberInfo->devicePath, memberInfo->headerOffset);
    
    off_t seek = lseek(fd, memberInfo->headerOffset, SEEK_SET);
    if (seek != memberInfo->headerOffset) goto error;

    int length = read(fd, header, kAppleRAIDHeaderSize);
    if (length < kAppleRAIDHeaderSize) goto error;

    if (!strncmp(header->raidSignature, kAppleRAIDSignature, sizeof(header->raidSignature))) {
	 headerData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)header, kAppleRAIDHeaderSize);
    }

error:
    close(fd);
    if (header) free(header);
    
    return headerData;
}

static AppleRAIDPrimaryOnDisk *
initPrimaryMetaDataForMirror(memberInfo_t * memberInfo)
{
    if (!memberInfo->primaryMetaDataSize) return false;

    void * bitmap = calloc(1, memberInfo->primaryMetaDataSize);
    if (!bitmap) return NULL;

    // setup bitmap using extents
    AppleRAIDPrimaryOnDisk * map = bitmap;
    strlcpy(map->priMagic, kAppleRAIDPrimaryMagic, sizeof(map->priMagic));
    map->priSize = memberInfo->primaryMetaDataSize;
    map->priType = kAppleRAIDPrimaryExtents;
    map->priSequenceNumber = 0;  // set by caller
    map->pri.extentCount = 1;
    map->priUsed = sizeof(AppleRAIDPrimaryOnDisk);

    AppleRAIDExtentOnDisk * extent = (AppleRAIDExtentOnDisk *)(map + 1);
    extent->extentByteOffset = 0;
    extent->extentByteCount = memberInfo->chunkCount * memberInfo->chunkSize;
    map->priUsed += sizeof(AppleRAIDExtentOnDisk);

    memberInfo->primaryData = bitmap;
	    
    return bitmap;
}

static AppleRAIDPrimaryOnDisk *
initPrimaryMetaDataForLVG(memberInfo_t * memberInfo)
{
    if (!memberInfo->primaryMetaDataSize) return false;

    void * primary = calloc(1, memberInfo->primaryMetaDataSize);
    if (!primary) return NULL;

    // setup primary header for LVG
    AppleRAIDPrimaryOnDisk * header = primary;
    strlcpy(header->priMagic, kAppleRAIDPrimaryMagic, sizeof(header->priMagic));
    header->priSize = memberInfo->primaryMetaDataSize;
    header->priType = kAppleRAIDPrimaryLVG;
    header->priSequenceNumber = 0;  // set by caller
    header->pri.volumeCount = 0;

    header->priUsed = sizeof(AppleRAIDPrimaryOnDisk);

    memberInfo->primaryData = primary;

    return primary;
}

static CFMutableDictionaryRef initLogicalVolumeProps(CFStringRef lvgUUIDString, CFStringRef volumeType, UInt64 size,
						     CFStringRef location, CFNumberRef sequenceNumber, CFDataRef extentData);
static AppleLVMVolumeOnDisk * buildLVMetaDataBlock(CFMutableDictionaryRef lvProps, CFDataRef extentData);

static void *
initSecondaryMetaDataForLVG(memberInfo_t * memberInfo, CFMutableDictionaryRef setProps)
{
    CFMutableDictionaryRef lvProps = 0;
    AppleLVMVolumeOnDisk * lvData = 0;
    void * secondary = 0;
    
    if (!memberInfo->secondaryMetaDataSize || !setProps) return NULL;

    CFStringRef lvgUUIDString = CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSetUUIDKey));
    if (!lvgUUIDString) return NULL;
    const void * sequenceProp = CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSequenceNumberKey));
    if (!sequenceProp) return NULL;
    CFStringRef volumeType = CFSTR(kAppleLVMVolumeTypeMaster);
    UInt64 volumeSize =  memberInfo->secondaryMetaDataSize;

    // the first logical volume is used to hold the logical volume
    // entries on its disk, since the lvg needs to be up before you
    // can add a logical volume it is special.  it needs to
    // work when disks are missing so it is relative to the member
    // instead of the logical volume group.  it is not listed in
    // the TOC but assumed to be the first entry in secondary
    // metadata area of the disk

    AppleRAIDExtentOnDisk extent;
    extent.extentByteOffset = memberInfo->chunkCount * memberInfo->chunkSize - memberInfo->secondaryMetaDataSize;
    extent.extentByteCount = memberInfo->secondaryMetaDataSize;

    CFDataRef extentData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)&extent, sizeof(AppleRAIDExtentOnDisk));
    if (!extentData) goto error;
    
    lvProps = initLogicalVolumeProps(lvgUUIDString, volumeType, volumeSize, CFSTR("meta"), sequenceProp, extentData);
    if (!lvProps) goto error;
    lvData = buildLVMetaDataBlock(lvProps, extentData);
    if (!lvData) goto error;

    secondary = calloc(1, memberInfo->secondaryMetaDataSize);
    if (!secondary) goto error;

    bcopy(lvData, secondary, lvData->lvHeaderSize);

    if (lvProps) CFRelease(lvProps);
    if (lvData) free(lvData);
    if (extentData) CFRelease(extentData);

    memberInfo->secondaryData = secondary;
    
    return secondary;

error:    
    if (lvProps) CFRelease(lvProps);
    if (lvData) free(lvData);
    if (extentData) CFRelease(extentData);
    if (secondary) free(secondary);

    return NULL;
}

static bool
writePrimaryMetaData(memberInfo_t * memberInfo)
{
    if (!memberInfo->primaryData) return false;
    if (!memberInfo->primaryMetaDataSize) return false;

#if defined(__LITTLE_ENDIAN__)
    AppleRAIDPrimaryOnDisk * header = memberInfo->primaryData;
    if (header->priType == kAppleRAIDPrimaryExtents) {
	int i;
	AppleRAIDExtentOnDisk * extent = (AppleRAIDExtentOnDisk *)(header + 1);	    
	for (i=0; i < header->pri.extentCount; i++) {
	    ByteSwapExtent(extent + i);
	}
    }
    ByteSwapPrimaryHeader(header);
#endif    

    int fd = open(memberInfo->devicePath, O_RDWR, 0);
    if (fd < 0) return false;
	
    off_t metaDataOffset = memberInfo->chunkCount * memberInfo->chunkSize;

    IOLog1("writePrimary %s, meta data offset = %llu.\n", memberInfo->devicePath, metaDataOffset);

    off_t seek = lseek(fd, metaDataOffset, SEEK_SET);
    if (seek != metaDataOffset) goto error;

    int length = write(fd, memberInfo->primaryData, memberInfo->primaryMetaDataSize);

    if (length < memberInfo->primaryMetaDataSize) goto error;
    
    close(fd);
    return true;

error:
    close(fd);
    return false;
}

#if 0
static AppleRAIDPrimaryOnDisk *
readPrimaryMetaData(memberInfo_t * memberInfo)
{
    UInt64 primaryOffset = memberInfo->chunkSize * memberInfo->chunkCount;
    UInt64 primarySize = memberInfo->primaryMetaDataSize;

    if (memberInfo->primaryData) free(memberInfo->primaryData);
    memberInfo->primaryData = NULL;

    AppleRAIDPrimaryOnDisk * primary = calloc(1, primarySize);
    if (!primary) return NULL;

    int fd = open(memberInfo->devicePath, O_RDONLY, 0);
    if (fd < 0) return NULL;
	
    IOLog1("readPrimary %s, offset = %llu, size = %llu.\n", memberInfo->devicePath, primaryOffset, primarySize);

    off_t seek = lseek(fd, primaryOffset, SEEK_SET);
    if (seek != primaryOffset) goto error;

    int length = read(fd, primary, primarySize);
	
    if (length < primarySize) goto error;

    if (!strncmp(primary->priMagic, kAppleRAIDPrimaryMagic, sizeof(primary->priMagic))) {
	memberInfo->primaryData = primary;
    } else {
	IOLog1("readPrimary, found bad magic on %s.\n", memberInfo->devicePath);
    }

error:
    close(fd);
    
#if defined(__LITTLE_ENDIAN__)
    AppleRAIDPrimaryOnDisk * header = memberInfo->primaryData;
    ByteSwapPrimaryHeader(header);
    if (header->priType == kAppleRAIDPrimaryExtents) {
	int i;
	AppleRAIDExtentOnDisk * extent = (AppleRAIDExtentOnDisk *)(header + 1);	    
	for (i=0; i < header->pri.extentCount; i++) {
	    ByteSwapExtent(extent + i);
	}
    }
#endif
    
    return memberInfo->primaryData;
}
#endif

// XXX instead of allocating a huge chunk of a memory and zeroing, the code
// could write a smaller chunk over and over same for primary data

static bool
writeSecondaryMetaData(memberInfo_t * memberInfo)
{
    if (!memberInfo->secondaryData) return false;
    if (!memberInfo->secondaryMetaDataSize) return false;

    AppleLVMVolumeOnDisk * header = memberInfo->secondaryData;
    AppleRAIDExtentOnDisk * extent = (AppleRAIDExtentOnDisk *)((char *)header + header->lvExtentsStart);
    // since this can only be called when creating a LVG, we know there is only one extent we need to swap
    assert(header->lvExtentsCount == 1);
    ByteSwapExtent(extent);
    ByteSwapLVMVolumeHeader(header);
    
    int fd = open(memberInfo->devicePath, O_RDWR, 0);
    if (fd < 0) return false;
	
    off_t metaDataOffset = memberInfo->chunkCount * memberInfo->chunkSize - memberInfo->secondaryMetaDataSize;

    IOLog1("writeSecondary %s, meta data offset = %llu, size = %llu.\n",
	   memberInfo->devicePath, metaDataOffset, memberInfo->secondaryMetaDataSize);

    off_t seek = lseek(fd, metaDataOffset, SEEK_SET);
    if (seek != metaDataOffset) goto error;

    int length = write(fd, memberInfo->secondaryData, memberInfo->secondaryMetaDataSize);

    if (length < memberInfo->secondaryMetaDataSize) goto error;
    
    close(fd);
    return true;

error:
    close(fd);
    return false;
}

static bool
updateLiveSet(CFMutableDictionaryRef setProps)
{
    CFStringRef setUUIDString = CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSetUUIDKey));

    // strip out any properties that haven't changed
    CFMutableDictionaryRef currentSet = AppleRAIDGetSetProperties(setUUIDString);
    CFMutableDictionaryRef updatedInfo = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, setProps);
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
    const void * seqNum = CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSequenceNumberKey));
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
    size_t		bufferSize = CFDataGetLength(setData);
    char		updateData[0x1000];
    size_t		updateDataSize = sizeof(updateData);

    if (!buffer) return false;

    IOLog1("update set changes = %s\n", buffer);

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleRAIDUpdateSet,		// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   updateData,			// output
				   &updateDataSize);		// output size (in/out)
    
    if (kr != KERN_SUCCESS) {
	IOLog1("AppleRAID - updateLiveSet failed with %x calling client.\n", kr);
	AppleRAIDCloseConnection();
	return false;
    }

    // get back the updated sequence number
    seqNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (UInt32 *)updateData);
    if (!seqNum) return false;
    CFDictionarySetValue(setProps, CFSTR(kAppleRAIDSequenceNumberKey), seqNum);

    CFRelease(setData);
    CFRelease(updatedInfo);
    
    AppleRAIDCloseConnection();
    
    return true;
}

// for each member initalize the following member specific values
//	kAppleRAIDMemberTypeKey - spare or member
//	kAppleRAIDMemberUUIDKey 
//	kAppleRAIDMemberIndexKey - index in set  (9999 for spare)
//	kAppleRAIDChunkCountKey - size of this member
	
static bool
createNewMembers(CFMutableDictionaryRef setProps, memberInfo_t ** memberInfo,
		 CFIndex memberCount, CFIndex spareCount,
		 CFIndex newMemberCount, CFIndex newSpareCount) 
{
    if (!memberInfo) return false;

    CFStringRef raidType = (CFStringRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDLevelNameKey));
    bool isLVG = CFEqual(raidType, CFSTR(kAppleRAIDLevelNameLVG));
    bool isMirror = CFEqual(raidType, CFSTR(kAppleRAIDLevelNameMirror));
	
    UInt32 i;
    for (i = 0; i < newMemberCount + newSpareCount; i++) {

	// whole raw disks are not supported
	if ((memberInfo[i]->isWhole) && (!memberInfo[i]->isRAID)) return false;

	CFStringRef typeString= 0, uuidString = 0;
	CFNumberRef index = 0, count = 0;
	if (i < newMemberCount) {

	    typeString = CFSTR(kAppleRAIDMembersKey);

	    UInt32 memberIndex = i + memberCount;
	    index = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &memberIndex);
		
	    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDMembersKey));
	    if (!uuidArray) return false;
	    uuidString = (CFStringRef)CFArrayGetValueAtIndex(uuidArray, memberIndex);

	} else {

	    typeString = CFSTR(kAppleRAIDSparesKey);
		
	    UInt32 spareIndex = kAppleRAIDDummySpareIndex;
	    index = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &spareIndex);

	    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSparesKey));
	    if (!uuidArray) return false;
	    spareIndex = i - newMemberCount + spareCount;
	    uuidString = (CFStringRef)CFArrayGetValueAtIndex(uuidArray, spareIndex);
	}
	    
	count = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &memberInfo[i]->chunkCount);

	if (typeString && index && uuidString && count) {
	    CFDictionarySetValue(setProps, CFSTR(kAppleRAIDMemberTypeKey), typeString);
	    CFDictionarySetValue(setProps, CFSTR(kAppleRAIDMemberIndexKey), index);
	    CFDictionarySetValue(setProps, CFSTR(kAppleRAIDMemberUUIDKey), uuidString);
	    CFDictionarySetValue(setProps, CFSTR(kAppleRAIDChunkCountKey), count);

	    CFRelease(index);
	    CFRelease(count);
	} else {
	    return false;
	}

	if (memberInfo[i]->primaryMetaDataSize) {
	    // layout the primary meta data
	    AppleRAIDPrimaryOnDisk * primary = NULL;
	    if (isLVG)    primary = initPrimaryMetaDataForLVG(memberInfo[i]);
	    if (isMirror) primary = initPrimaryMetaDataForMirror(memberInfo[i]);
	    if (!primary) {
		IOLog1("AppleRAIDUpdateSet - failed to create the primary metadata for partition \"%s\"\n", memberInfo[i]->diskName);
		return false;
	    }

	    // update the sequence number
	    const void * number = CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSequenceNumberKey));
	    if (!number) return false;
	    if (!CFNumberGetValue(number, kCFNumberSInt32Type, &primary->priSequenceNumber)) return false;

	    // set the amount used in the in the raid header
	    UInt64 usedSize = memberInfo[i]->primaryData->priUsed;
	    CFNumberRef meta1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &usedSize);
	    if (!meta1) return false;
	    CFDictionarySetValue(setProps, CFSTR(kAppleRAIDPrimaryMetaDataUsedKey), meta1);
	    CFRelease(meta1);
	}

	if (memberInfo[i]->secondaryMetaDataSize) {
	    void * secondary = NULL;
	    if (isLVG) secondary = initSecondaryMetaDataForLVG(memberInfo[i], setProps);
	    if (!secondary) {
		IOLog1("AppleRAIDUpdateSet - failed to create the secondary metadata for partition \"%s\"\n", memberInfo[i]->diskName);
		return false;
	    }

	    CFNumberRef meta2 = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &memberInfo[i]->secondaryMetaDataSize);
	    if (!meta2) return false;
	    CFDictionarySetValue(setProps, CFSTR(kAppleRAIDSecondaryMetaDataSizeKey), meta2);
	    CFRelease(meta2);
	}

	CFStringRef partitionName = CFStringCreateWithCString(kCFAllocatorDefault, memberInfo[i]->diskName, kCFStringEncodingUTF8);
	if (!partitionName) return false;
	bool success = AppleRAIDRemoveHeaders(partitionName);
	if (!success) {
	    IOLog1("AppleRAIDUpdateSet - there was a problem erasing the raid headers on partition \"%s\"\n", memberInfo[i]->diskName);
	    return false;
	}
	CFRelease(partitionName);

	if (memberInfo[i]->secondaryMetaDataSize && !writeSecondaryMetaData(memberInfo[i])) {
	    IOLog1("AppleRAIDUpdateSet - failed while writing secondary metadata to partition \"%s\"\n", memberInfo[i]->diskName);
	    return false;
	}

	if (memberInfo[i]->primaryMetaDataSize && !writePrimaryMetaData(memberInfo[i])) {
	    IOLog1("AppleRAIDUpdateSet - failed while writing primary metadata to partition \"%s\"\n", memberInfo[i]->diskName);
	    return false;
	}

	if (!writeHeader(setProps, memberInfo[i])) {
	    IOLog1("AppleRAIDUpdateSet - failed while writing RAID header to partition \"%s\"\n", memberInfo[i]->diskName);
	    return false;
	}

	//  if this is a stacked set then force the set to read the new headers 
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

static UInt64 calculateBitMapSize(UInt64 partitionSize, UInt64 chunkSize, UInt64 * remainingBytes);

AppleRAIDSetRef
AppleRAIDUpdateSet(CFMutableDictionaryRef setProps)
{
    CFStringRef setUUIDString = CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSetUUIDKey));
    CFRetain(setUUIDString);
    memberInfo_t ** memberInfo = 0;

#if DEBUG
    CFShow(setUUIDString);
#endif

    // pull out the fluff
    CFIndex memberCount = 0, spareCount = 0;
    CFIndex newMemberCount = 0, newSpareCount = 0;
    
    CFMutableArrayRef newMemberNames = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR("_member names_"));
    if (newMemberNames) {
	CFRetain(newMemberNames);
	CFDictionaryRemoveValue(setProps, CFSTR("_member names_"));
	newMemberCount = CFArrayGetCount(newMemberNames);
    }

    CFMutableArrayRef newSpareNames = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR("_spare names_"));
    if (newSpareNames) {
	CFRetain(newSpareNames);
	CFDictionaryRemoveValue(setProps, CFSTR("_spare names_"));
	newSpareCount = CFArrayGetCount(newSpareNames);
    }


    // if the raid set has status it is "live", get it's current member/spare counts
    bool liveSet = CFDictionaryContainsKey(setProps, CFSTR(kAppleRAIDStatusKey));  // this only works once
    if (liveSet) {
	CFDictionaryRemoveValue(setProps, CFSTR(kAppleRAIDStatusKey));

	CFMutableArrayRef tempMembers = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDMembersKey));
	if (tempMembers) {
	    memberCount = CFArrayGetCount(tempMembers) - newMemberCount;
	}
	CFMutableArrayRef tempSpares = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSparesKey));
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

	bool sizesCanVary = (CFBooleanRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSizesCanVaryKey)) == kCFBooleanTrue;
	bool quickRebuild = (CFBooleanRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSetQuickRebuildKey)) == kCFBooleanTrue;
	CFStringRef raidType = (CFStringRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDLevelNameKey));
	bool isLVG = CFEqual(raidType, CFSTR(kAppleRAIDLevelNameLVG));

	UInt64 metaDataSize = 0;
	if (quickRebuild) {
	    if (liveSet) {
		// XXX this is the used size, not whole size
		CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDPrimaryMetaDataUsedKey));
		if (!number || !CFNumberGetValue(number, kCFNumberSInt64Type, &metaDataSize) || !metaDataSize) {
		    printf("AppleRAID: Failed to find the size of the mirror quick rebuild bitmap.\n");
		    return NULL;
		}
	    }
	}

	// determine each partition's chunk count
	UInt64 chunkSize = 0;
	CFNumberRef number;
	number = (CFNumberRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDChunkSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &chunkSize);
	if (!chunkSize) return NULL;

	// find the smallest member (or spare)
	UInt64 smallestSize = 0;
	if (liveSet) {
	    // calculate the minimum required size for a member partition in this set
	    UInt64 chunkCount = 0;
	    CFNumberRef number;
	    number = (CFNumberRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDChunkCountKey));
	    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &chunkCount);

	    if (!chunkCount) return NULL;

	    smallestSize = chunkCount * chunkSize + metaDataSize + (UInt64)kAppleRAIDHeaderSize;
	} else {
	    smallestSize = memberInfo[0]->size;
	}
	if (!sizesCanVary) {
	    for (i = 0; i < newMemberCount + newSpareCount; i++) {
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

	//  if quick rebuilding then factor in the required meta data size
	if (quickRebuild && !metaDataSize) {

	    metaDataSize = calculateBitMapSize(smallestSize, chunkSize, NULL);
	    IOLog1("quick rebuild bit map size = %lld @ offset %lld\n", metaDataSize,
		   (ARHEADER_OFFSET(smallestSize) - metaDataSize) / chunkSize);
	}

	if (isLVG) metaDataSize = 0x100000;  // XXXTOC start with a meg
    
	for (i = 0; i < newMemberCount + newSpareCount; i++) {

	    memberInfo[i]->chunkSize = chunkSize;
	    memberInfo[i]->primaryMetaDataSize = metaDataSize;

	    // XXXTOC start with 4 megs which gives us 1024 min size volumes entries
	    // should be able calculate a better size based on the member size
	    if (isLVG) memberInfo[i]->secondaryMetaDataSize = 0x400000;
	    
	    if (sizesCanVary) {
		memberInfo[i]->chunkCount = (memberInfo[i]->headerOffset - metaDataSize) / chunkSize;
	    } else {
		memberInfo[i]->chunkCount = (ARHEADER_OFFSET(smallestSize) - metaDataSize) / chunkSize;
	    }
	}
    }

    // warn controller of set change prior to adding new members
    // add give the set a chance to reject anything it does not like

    if (liveSet) {
	if (!updateLiveSet(setProps)) return NULL;
    }

    // write out headers on new members/spares
	
    if (newSpareCount || newMemberCount) {
	if (!createNewMembers(setProps, memberInfo, memberCount,
			      spareCount, newMemberCount, newSpareCount)) return NULL;
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

    int fd = open(memberInfo->devicePath, O_RDWR, 0);
    if (fd < 0) return false;

    // look for v1 style header
    UInt64 headerOffset = 0;
    {
	IOLog2("AppleRAIDRemoveHeaders %s, scaning header offset = %llu.\n", memberInfo->devicePath, headerOffset);
    
	off_t seek = lseek(fd, headerOffset, SEEK_SET);
	if (seek != headerOffset) return false;

	int length = read(fd, header, kAppleRAIDHeaderSize);
	if (length < kAppleRAIDHeaderSize) return false;

	if (!strncmp(header->raidSignature, kAppleRAIDSignature, sizeof(header->raidSignature))) {
	    IOLog1("AppleRAIDRemoveHeaders %s, found ARv1 header at offset = %llu.\n", memberInfo->devicePath, headerOffset);

	    bzero(header, kAppleRAIDHeaderSize);

	    seek = lseek(fd, headerOffset, SEEK_SET);
	    if (seek != headerOffset) return false;
	    
	    length = write(fd, header, kAppleRAIDHeaderSize);
	    if (length < kAppleRAIDHeaderSize) return false;
	}
    }

    // scan for nested headers
    headerOffset = ARHEADER_OFFSET(memberInfo->size);
    int count = 5;
    while (headerOffset && count) {
	IOLog2("AppleRAIDRemoveHeaders %s, scanning header offset = %llu.\n", memberInfo->devicePath, headerOffset);
    
	off_t seek = lseek(fd, headerOffset, SEEK_SET);
	if (seek != headerOffset) break;

	int length = read(fd, header, kAppleRAIDHeaderSize);
	if (length < kAppleRAIDHeaderSize) break;

	ByteSwapHeaderV2(header);
	
	if (!strncmp(header->raidSignature, kAppleRAIDSignature, sizeof(header->raidSignature))) {
	    IOLog1("AppleRAIDRemoveHeaders %s, found ARv2 header at offset = %llu.\n", memberInfo->devicePath, headerOffset);

	    UInt64 memberSize = header->size;

	    bzero(header, kAppleRAIDHeaderSize);

	    seek = lseek(fd, headerOffset, SEEK_SET);
	    if (seek != headerOffset) break;
	    
	    length = write(fd, header, kAppleRAIDHeaderSize);
	    if (length < kAppleRAIDHeaderSize) break;

	    headerOffset = (memberSize < headerOffset) ? ARHEADER_OFFSET(memberSize) : 0;

	} else {

	    headerOffset = 0;
	}
	count--;
    }
	
    close(fd);
    freeMemberInfo(memberInfo);

    return headerOffset == 0;
}


bool
AppleRAIDRemoveMember(CFMutableDictionaryRef setProps, AppleRAIDMemberRef member)
{
    // make sure we support this operation
    UInt32 version;
    CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDHeaderVersionKey));
    if (!number || !CFNumberGetValue(number, kCFNumberSInt32Type, &version) || version < 0x00020000) {
	printf("AppleRAID: This operation is not supported on earlier RAID set revisions.\n");
	return NULL;
    }

    // find the member or spare
    CFMutableArrayRef uuidArray = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDMembersKey));
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
	uuidArray2 = (CFMutableArrayRef)CFDictionaryGetValue(setProps, CFSTR(kAppleRAIDSparesKey));
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
    CFMutableDictionaryRef setProps = AppleRAIDGetSetProperties(setName);
    if (!setProps) return false;

    UInt32 subCommand = kAppleRAIDUpdateDestroySet;
    CFNumberRef destroySubCommand = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &subCommand);
    CFDictionarySetValue(setProps, CFSTR("_update command_"), destroySubCommand);

    if (!updateLiveSet(setProps)) return false;

    CFRelease(setProps);

    return true;
}


#define kAppleRAIDMinBitMapBytesPerBit	(512 * 1024)		// min bytes allowed to be represented by one bit
#define kAppleRAIDMaxBitMapBytesPerBit	(32 * 1024 * 1024) 	// max bytes allowed to be represented by one bit

#define kAppleRAIDBitMapPageSize	(4 * 1024)		// the "offical" raid page size
#define kAppleRAIDMinBitMapSize		(32 * 4 * 1024)		// 128k

// the largest bitmap for a 0x10000000000000000 volume is 4GB

// if remainingBytes is set, we are trying to fit the bitmap into the partition, the bitmap covers the data section of the partition.
// if remainingBytes is not set, we are trying to find the size of a bitmap to cover the whole partition.
// in either case this function returns the size of the bitmap

// XXX this code is too agressive in bumping the number of bytes per bit

static UInt64 calculateBitMapSize(UInt64 partitionSize, UInt64 chunkSize, UInt64 * remainingBytes)
{
    UInt64 bytesPerBit = kAppleRAIDMinBitMapBytesPerBit;
    UInt64 bitMapSize = kAppleRAIDMinBitMapSize;
    UInt64 bitsNeeded;

    // adjust bytes per bit until we have a reasonable sized bitmap

    if (remainingBytes) {

	UInt64 availableBytes = ARHEADER_OFFSET(partitionSize) - sizeof(AppleRAIDPrimaryOnDisk);
	bitsNeeded = (availableBytes - bitMapSize) / bytesPerBit;
	bitsNeeded += (availableBytes - bitMapSize) % bytesPerBit ? 1 : 0;
	while (bitsNeeded > (bitMapSize * 8)) {

	    bytesPerBit *= 2;
	    if (bytesPerBit > kAppleRAIDMaxBitMapBytesPerBit) {
		bytesPerBit = kAppleRAIDMinBitMapBytesPerBit;
		bitMapSize += kAppleRAIDMinBitMapSize;
	    }
	    bitsNeeded = (availableBytes - bitMapSize) / bytesPerBit;
	    bitsNeeded += (availableBytes - bitMapSize) % bytesPerBit ? 1 : 0;
	}

	*remainingBytes = (availableBytes - bitMapSize) / chunkSize * chunkSize;

    } else {

	bitsNeeded = partitionSize / bytesPerBit;
	bitsNeeded += partitionSize % bytesPerBit ? 1 : 0;
	while (bitsNeeded > (bitMapSize * 8)) {

	    bytesPerBit *= 2;
	    if (bytesPerBit > kAppleRAIDMaxBitMapBytesPerBit) {
		bytesPerBit = kAppleRAIDMinBitMapBytesPerBit;
		bitMapSize += kAppleRAIDMinBitMapSize;
	    }
	    bitsNeeded = partitionSize / bytesPerBit;
	    bitsNeeded += partitionSize % bytesPerBit ? 1 : 0;
	}
    }

    return bitMapSize;
}

static AppleRAIDExtentOnDisk *
allocateExtent(AppleRAIDExtentOnDisk * firstExtent,  UInt64 lvgExtentCount, UInt64 size, CFStringRef location, UInt64 * extentCount)
{

    // XXXTOC need to look at location

    *extentCount = 0;
    AppleRAIDExtentOnDisk dummyExtent = {0, 0};

    AppleRAIDExtentOnDisk * newExtents = malloc(sizeof(AppleRAIDExtentOnDisk));
    if (!newExtents) return NULL;

    while (size) {
	
	AppleRAIDExtentOnDisk * prevExtent = &dummyExtent;
	AppleRAIDExtentOnDisk * nextExtent = firstExtent;
	AppleRAIDExtentOnDisk * prevLargestExtent = 0;
	AppleRAIDExtentOnDisk * nextLargestExtent = 0;
	UInt64 gap = 0;
	UInt64 largestGap = 0;
	
	// there should always be an ending extent for the metadata
	while (nextExtent < firstExtent + lvgExtentCount) {

	    gap = nextExtent->extentByteOffset - (prevExtent->extentByteOffset + prevExtent->extentByteCount);

	    // IOLog1("  existing extent at %lld, size %lld\n", prevExtent->extentByteOffset, prevExtent->extentByteCount);

	    if (gap >= size) break;

	    if (gap > largestGap) {
		largestGap = gap;
		prevLargestExtent = prevExtent;
		nextLargestExtent = nextExtent;
	    }
	
	    prevExtent = nextExtent;
	    nextExtent++;
	}

	if (largestGap && gap < size) {
	    prevExtent = prevLargestExtent;
	    nextExtent = nextLargestExtent;
	    gap = nextExtent->extentByteOffset - (prevExtent->extentByteOffset + prevExtent->extentByteCount);
	    IOLog1("largest extent found is %lld, wanted %lld\n", gap, size);
	}

	if (!gap) {
	    free(newExtents);
	    return NULL;
	}

	if (gap) {

	    if (*extentCount) {
		newExtents = reallocf(newExtents, sizeof(AppleRAIDExtentOnDisk) * (*extentCount + 1));
		if (!newExtents) return NULL;
	    }

	    newExtents[*extentCount].extentByteOffset = prevExtent->extentByteOffset + prevExtent->extentByteCount;
	    newExtents[*extentCount].extentByteCount = MIN(gap, size);

	    IOLog1("Allocated new extent at %lld, size %lld\n", newExtents[*extentCount].extentByteOffset, newExtents[*extentCount].extentByteCount);

	    prevExtent->extentByteCount += MIN(gap, size);  // this does not stick if it is the dummy extent (which is ok if we call this last)
	    
	    *extentCount += 1;
	    size -= MIN(gap, size);
	}
    }

    if (!size) return newExtents;

    free(newExtents);
    return NULL;
}


static UInt64 growLastExtent(CFMutableDataRef extentData, AppleRAIDExtentOnDisk * lvgExtentList, UInt64 lvgExtentCount, UInt64 newSize)
{
    CFIndex extentDataSize = CFDataGetLength(extentData);
    CFIndex index = 0;
    UInt64 volumeSize = 0;
    CFRange range;
    AppleRAIDExtentOnDisk foo, * extent = &foo;

    // find volume's last extent & recalculate it's size

    while (index < extentDataSize) {

	range = CFRangeMake(index, sizeof(AppleRAIDExtentOnDisk));
	CFDataGetBytes(extentData, range, (void *)extent);

	volumeSize += extent->extentByteCount;

	index += sizeof(AppleRAIDExtentOnDisk);
    }
    UInt64 volumeEnd = extent->extentByteOffset + extent->extentByteCount;

    UInt64 bytesNeeded = newSize - volumeSize;
    
    // find a gap in the used lvg extents that starts at the volume's end
    // XXX this should use a binary search
    
    AppleRAIDExtentOnDisk * lvgExtent;
    index = 0;
    UInt64 gapStart = 0;
    UInt64 gapSize;
    while (gapStart <= volumeEnd && index < (lvgExtentCount - 1)) {

	lvgExtent = lvgExtentList + index;
	
	gapStart = lvgExtent->extentByteOffset + lvgExtent->extentByteCount;
	gapSize = (lvgExtent + 1)->extentByteOffset - gapStart;

	// found something!
	if (gapStart == volumeEnd && gapSize) {

	    UInt64 bytesAvailable = MIN(bytesNeeded, gapSize);
	    extent->extentByteCount += bytesAvailable;
	    lvgExtent->extentByteCount += bytesAvailable;  // in case we reuse list later
	    CFDataReplaceBytes(extentData, range, (void *)extent, sizeof(AppleRAIDExtentOnDisk));

	    return volumeSize + bytesAvailable;
	}

	index++;
    }

    return 0;
}
	

static UInt64 truncateExtents(CFMutableDataRef extentData, UInt64 newSize)
{
    CFIndex extentDataSize = CFDataGetLength(extentData);
    CFIndex index = 0;
    UInt64 extentEnd = 0;
    CFRange range;
    AppleRAIDExtentOnDisk foo, * extent = &foo;
	
    while (index < extentDataSize) {

	range = CFRangeMake(index, sizeof(AppleRAIDExtentOnDisk));
	CFDataGetBytes(extentData, range, (void *)extent);

	extentEnd += extent->extentByteCount;

	if (newSize <= extentEnd) {		// found it
	    
	    extent->extentByteCount -= extentEnd - newSize;
	    CFDataReplaceBytes(extentData, range, (void *)extent, sizeof(AppleRAIDExtentOnDisk));
	    CFDataSetLength(extentData, index + sizeof(AppleRAIDExtentOnDisk));

	    return CFDataGetLength(extentData) / sizeof(AppleRAIDExtentOnDisk);  // the new extent count
	}
	
	index += sizeof(AppleRAIDExtentOnDisk);
    }
    
    // should never get here
    return 0;
}


UInt64 AppleRAIDGetUsableSize(UInt64 partitionSize, UInt64 chunkSize, UInt32 options)
{
    UInt64 usable = 0;

    if (!chunkSize) {
	IOLog1("AppleRAIDGetUseableSize: zero chunkSize?\n"); 
	return 0;
    }

    switch (options) {

    case kAppleRAIDUsableSizeOptionNone:
	usable = ARHEADER_OFFSET(partitionSize) / chunkSize * chunkSize;
	break;

    case kAppleRAIDUsableSizeOptionQuickRebuild:
	(void)calculateBitMapSize(partitionSize, chunkSize, &usable);
	break;

    default:
	break;
    }

    return usable;
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


// ***************************************************************************************************
//
// LVM interfaces
// 
// ***************************************************************************************************

CFMutableArrayRef
AppleLVMGetVolumesForGroup(AppleRAIDSetRef setRef, AppleRAIDMemberRef member)
{
    kern_return_t 	kr;
    size_t		propSize = kMaxIOConnectTransferSize;  // XXX buffer size?  use kAppleRAIDLVGVolumeCountKey
    CFMutableArrayRef	volumes = NULL;
    size_t		bufferSize = kAppleRAIDMaxUUIDStringSize * 2;
    char		buffer[bufferSize];

    if (!CFStringGetCString(setRef, buffer, kAppleRAIDMaxUUIDStringSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleLVMGetVolumesForGroup() CFStringGetCString failed on set ref?\n");
	return NULL;
    }

    if (member) {
	if (!CFStringGetCString(member, &buffer[kAppleRAIDMaxUUIDStringSize], kAppleRAIDMaxUUIDStringSize, kCFStringEncodingUTF8)) {
	    IOLog1("AppleLVMGetVolumesForGroup() CFStringGetCString failed on member ref?\n");
	    return NULL;
	}
    } else {
	buffer[kAppleRAIDMaxUUIDStringSize] = 0;
    }

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;
    
    char * propString = (char *)malloc(propSize);

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleLVMGetVolumesForGroup,	// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   propString,			// output
				   &propSize);			// output size (in/out)

    if (kr == KERN_SUCCESS) {
        IOLog2("AppleLVMGetVolumesForGroup was successful.\n");
        IOLog2("size = %d, prop = %s\n", (int)propSize, (char *)propString);

	volumes = (CFMutableArrayRef)IOCFUnserialize(propString, kCFAllocatorDefault, 0, NULL);
    } else {
        IOLog1("AppleLVMGetVolumesForGroup failed with 0x%x.\n", kr);
    }

    free(propString);

    AppleRAIDCloseConnection();
    
    return volumes;
}


CFMutableDictionaryRef
AppleLVMGetVolumeProperties(AppleLVMVolumeRef volRef)
{
    kern_return_t 	kr;
    size_t		propSize = kMaxIOConnectTransferSize;
    CFMutableDictionaryRef props = NULL;
    size_t		bufferSize = kAppleRAIDMaxUUIDStringSize;
    char		buffer[bufferSize];

    if (!CFStringGetCString(volRef, buffer, bufferSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleLVMGetVolumeProperties() CFStringGetCString failed?\n");
	return NULL;
    }

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;
    
    char * propString = (char *)malloc(propSize);

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleLVMGetVolumeProperties,// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   propString,			// output
				   &propSize);			// output size (in/out)

    if (kr == KERN_SUCCESS) {
        IOLog2("AppleLVMGetVolumeProperties was successful.\n");
        IOLog2("size = %d, prop = %s\n", (int)propSize, (char *)propString);

	props = (CFMutableDictionaryRef)IOCFUnserialize(propString, kCFAllocatorDefault, 0, NULL);
    }

    free(propString);

    AppleRAIDCloseConnection();
    
    return props;
}

static AppleRAIDExtentOnDisk *
getVolumeExtents(AppleLVMVolumeRef volRef, UInt64 * extentCount)
{
    kern_return_t 	kr;
    size_t		bufferSize = kAppleRAIDMaxUUIDStringSize;
    char		buffer[bufferSize];
    size_t		extentSize = kMaxIOConnectTransferSize;
    AppleRAIDExtentOnDisk * extents = NULL;

    if (!extentCount || !*extentCount) return NULL;

    if (!CFStringGetCString(volRef, buffer, bufferSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleLVMGetVolumeExtents() CFStringGetCString failed?\n");
	return NULL;
    }

    if (*extentCount * sizeof(AppleRAIDExtentOnDisk) > extentSize) return NULL;  // XXX buffer size

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;
    
    AppleRAIDExtentOnDisk * extentsBuffer = (AppleRAIDExtentOnDisk *)malloc(extentSize);

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleLVMGetVolumeExtents,	// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   extentsBuffer,		// output
				   &extentSize);		// output size (in/out)

    if (kr == KERN_SUCCESS) {
        IOLog2("AppleLVMGetVolumeExtents was successful.\n");
        IOLog2("size = %d, extent = %s\n", (int)extentSize, (char *)extentString);

	extents = extentsBuffer;
	*extentCount = extentSize / sizeof(AppleRAIDExtentOnDisk);
    } else {
        IOLog2("AppleLVMGetVolumeExtents failed.\n");
	free(extentsBuffer);
    }

    // XXX check for buffer too small error (first size is zero)

    AppleRAIDCloseConnection();
    
    return extents;
}


CFDataRef AppleLVMGetVolumeExtents(AppleLVMVolumeRef volRef)
{
    UInt64 extentCount = kMaxIOConnectTransferSize / sizeof(AppleRAIDExtentOnDisk);

    AppleRAIDExtentOnDisk * extentList = getVolumeExtents(volRef, &extentCount);
    if (!extentList) return NULL;

    if (extentList->extentByteCount == 0) {
	// retry with larger buffer
	extentCount = extentList->extentByteOffset;
	extentList = getVolumeExtents(volRef, &extentCount);
    }
    if (!extentList) return NULL;

    CFDataRef extentData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)extentList,
						       extentCount * sizeof(AppleRAIDExtentOnDisk), kCFAllocatorMalloc);
    return extentData;
}

static const char *lvDescriptionBuffer =
" <array> \n"
    "<dict> \n"
	"<key>" kAppleLVMVolumeTypeKey "</key>"		"<string>" kAppleLVMVolumeTypeConcat "</string> \n"
    "</dict> \n"
    "<dict> \n"
	"<key>" kAppleLVMVolumeTypeKey "</key>"		"<string>" kAppleLVMVolumeTypeSnapRO "</string> \n"
    "</dict> \n"
    "<dict> \n"
	"<key>" kAppleLVMVolumeTypeKey "</key>"		"<string>" kAppleLVMVolumeTypeSnapRW "</string> \n"
    "</dict> \n"
" </array> \n";

CFMutableArrayRef AppleLVMGetVolumeDescription(void)
{
    CFStringRef errorString;

    CFMutableArrayRef lvDescription = (CFMutableArrayRef)IOCFUnserialize(lvDescriptionBuffer, kCFAllocatorDefault, 0, &errorString);
    if (!lvDescription) {
	CFIndex	bufferSize = CFStringGetLength(errorString);
	bufferSize = CFStringGetMaximumSizeForEncoding(bufferSize, kCFStringEncodingUTF8) + 1;
	char *buffer = malloc(bufferSize);
	if (!buffer || !CFStringGetCString(errorString, buffer, bufferSize, kCFStringEncodingUTF8)) {
	    return NULL;
	}

	IOLog1("AppleLVMGetVolumeDescription - failed while parsing raid definition file, error: %s\n", buffer);
	CFRelease(errorString);
	return NULL;
    }

    return lvDescription;
}

static const char *defaultCreateLVBuffer =
" <dict> \n"
    "<key>" kAppleLVMVolumeVersionKey "</key>"		"<integer size=\"32\">0x00030000</integer> \n"
    "<key>" kAppleLVMGroupUUIDKey "</key>"		"<string>internal error</string> \n"
    "<key>" kAppleLVMVolumeUUIDKey "</key>"		"<string>internal error</string> \n"
    "<key>" kAppleLVMVolumeSequenceKey "</key>"		"<integer size=\"32\">0</integer> \n"
    "<key>" kAppleLVMVolumeSizeKey "</key>"		"<integer size=\"64\">0x00000000</integer> \n"
    "<key>" kAppleLVMVolumeExtentCountKey "</key>"	"<integer size=\"64\">0x00000001</integer> \n"
    "<key>" kAppleLVMVolumeTypeKey "</key>"		"<string>internal error</string> \n"
    "<key>" kAppleLVMVolumeLocationKey "</key>"		"<string/> \n"
    "<key>" kAppleLVMVolumeContentHintKey "</key>"	"<string/> \n"
    "<key>" kAppleLVMVolumeNameKey "</key>"		"<string/> \n"
" </dict> \n";

static CFMutableDictionaryRef
initLogicalVolumeProps(CFStringRef lvgUUIDString, CFStringRef volumeType, UInt64 size, CFStringRef location,
		       CFNumberRef sequenceNumber, CFDataRef extentData)
{
    CFStringRef errorString;
    UInt64 extentCount = CFDataGetLength(extentData) / sizeof(AppleRAIDExtentOnDisk);
    if (!extentCount) return NULL;

    CFMutableDictionaryRef lvProps = (CFMutableDictionaryRef)IOCFUnserialize(defaultCreateLVBuffer, kCFAllocatorDefault, 0, &errorString);
    if (!lvProps) {
	CFIndex	bufferSize = CFStringGetLength(errorString);
	bufferSize = CFStringGetMaximumSizeForEncoding(bufferSize, kCFStringEncodingUTF8) + 1;
	char *buffer = malloc(bufferSize);
	if (!buffer || !CFStringGetCString(errorString, buffer, bufferSize, kCFStringEncodingUTF8)) {
	    return NULL;
	}

	IOLog1("AppleLVMCreateVolume - failed while parsing logical volume template file, error: %s\n", buffer);
	CFRelease(errorString);
	return NULL;
    }

    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    if (!uuid) return NULL;
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, uuid);
    CFRelease(uuid);
    if (!uuidString) return NULL;

    CFNumberRef sizeProp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &size);
    if (!sizeProp) return NULL;
    
    CFNumberRef countProp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &extentCount);
    if (!countProp) return NULL;
    
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeUUIDKey), uuidString);
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMGroupUUIDKey), lvgUUIDString);
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeSequenceKey), sequenceNumber);
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey), sizeProp);
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeLocationKey), location);
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeTypeKey), volumeType);
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeExtentCountKey), countProp);

    CFDictionarySetValue(lvProps, CFSTR("_extent data_"), extentData);

    CFRelease(uuidString);
    CFRelease(sizeProp);
    CFRelease(countProp);

    return lvProps;
}


CFMutableDictionaryRef
AppleLVMCreateVolume(AppleRAIDSetRef setRef, CFStringRef volumeType, UInt64 volumeSize, CFStringRef volumeLocation)
{
    CFMutableDictionaryRef lvProps = 0;

    if (!setRef || !volumeType || !volumeSize || !volumeLocation) return NULL;
    
    setInfo_t * lvgInfo = getSetInfo(setRef);
    if (!lvgInfo) return NULL;

    // read in the logical volume group's free space data
    UInt64 lvgExtentCount = 0;
    CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(lvgInfo->setProps, CFSTR(kAppleRAIDLVGExtentsKey));
    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &lvgExtentCount);

    UInt64 lvgFreeSpace = 0;
    number = (CFNumberRef)CFDictionaryGetValue(lvgInfo->setProps, CFSTR(kAppleRAIDLVGFreeSpaceKey));
    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &lvgFreeSpace);

    if (volumeSize > lvgFreeSpace) {
	printf("AppleRAID: Insufficent free space to create requested logical volume.\n");
	return NULL;
    }

    // Ask for the extent list
    AppleRAIDExtentOnDisk * lvgExtentList = getVolumeExtents(setRef, &lvgExtentCount);
    if (!lvgExtentList) goto error;

    // XXXTOC use kAppleRAIDMemberStartKey to find a member's startOffset
    // and then use that move the extentList pointer to start of that member
    // could also use the lvg extents to calculate the free space per member
    // to help spread out new volumes

    UInt64 extentCount = 0;
    AppleRAIDExtentOnDisk * extentList = allocateExtent(lvgExtentList, lvgExtentCount, volumeSize, volumeLocation, &extentCount);
    if (!extentList) goto error;

    CFDataRef extentData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)extentList,
						       extentCount * sizeof(AppleRAIDExtentOnDisk), kCFAllocatorMalloc);
    if (!extentData) goto error;
    
    // set up disk block(s) for lv entry
    const void * sequenceProp = CFDictionaryGetValue(lvgInfo->setProps, CFSTR(kAppleRAIDSequenceNumberKey));
    if (!sequenceProp) goto error;
    CFStringRef lvgUUIDString = CFDictionaryGetValue(lvgInfo->setProps, CFSTR(kAppleRAIDSetUUIDKey));
    if (!lvgUUIDString) goto error;

    lvProps = initLogicalVolumeProps(lvgUUIDString, volumeType, volumeSize, volumeLocation, sequenceProp, extentData);
    if (!lvProps) goto error;

    freeSetInfo(lvgInfo);

    return lvProps;

error:
    // clean up
    if (lvProps) CFRelease(lvProps);
    freeSetInfo(lvgInfo);

    return NULL;
}


bool
AppleLVMModifyVolume(CFMutableDictionaryRef lvProps, CFStringRef key, void * value)
{
    CFStringRef errorString;

    CFMutableDictionaryRef defaultLVProps = (CFMutableDictionaryRef)IOCFUnserialize(defaultCreateLVBuffer, kCFAllocatorDefault, 0, &errorString);
    if (!defaultLVProps) {
	CFIndex	bufferSize = CFStringGetLength(errorString);
	bufferSize = CFStringGetMaximumSizeForEncoding(bufferSize, kCFStringEncodingUTF8) + 1;
	char *buffer = malloc(bufferSize);
	if (!buffer || !CFStringGetCString(errorString, buffer, bufferSize, kCFStringEncodingUTF8)) {
	    goto error;
	}

	IOLog1("AppleLVMModifyVolume - failed while parsing logical volume template file, error: %s\n", buffer);
	CFRelease(errorString);
	goto error;
    }

    const void * defaultValue = CFDictionaryGetValue(defaultLVProps, key);
    if (!defaultValue) goto error;

    if (CFGetTypeID(defaultValue) != CFGetTypeID(value)) goto error;

    AppleRAIDSetRef lvgRef = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMGroupUUIDKey));
    if (!lvgRef) return false;
    CFMutableDictionaryRef lvgProps = AppleRAIDGetSetProperties(lvgRef);
    if (!lvgProps) return false;
    const void * sequenceNumber = CFDictionaryGetValue(lvgProps, CFSTR(kAppleRAIDSequenceNumberKey));
    if (!sequenceNumber) return false;
    CFDictionarySetValue(lvProps, CFSTR(kAppleLVMVolumeSequenceKey), sequenceNumber);
    CFRelease(lvgProps);

    CFDictionarySetValue(lvProps, key, value);

    CFRelease(defaultLVProps);
    
    return true;

error:
    if (defaultLVProps) CFRelease(defaultLVProps);
    return false;
}

static AppleLVMVolumeOnDisk *
buildLVMetaDataBlock(CFMutableDictionaryRef lvProps, CFDataRef extentData)
{
    CFDataRef propData = 0;

    AppleRAIDExtentOnDisk * extentList = (AppleRAIDExtentOnDisk *)CFDataGetBytePtr(extentData);
    UInt64 extentCount = CFDataGetLength(extentData) / sizeof(AppleRAIDExtentOnDisk);
    if (!extentCount || !extentList) return NULL;
    
    AppleLVMVolumeOnDisk * lvData = calloc(1, kAppleLVMVolumeOnDiskMinSize);
    if (!lvData) return NULL;

    strlcpy(lvData->lvMagic, kAppleLVMVolumeMagic, sizeof(lvData->lvMagic));
    lvData->lvHeaderSize = kAppleLVMVolumeOnDiskMinSize;
    lvData->lvExtentsCount = extentCount;

    // strip any internal keys from the dictionary before writing to disk
    CFMutableDictionaryRef cleanProps = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, lvProps);
    if (!cleanProps) goto error; 
    CFIndex propCount = CFDictionaryGetCount(cleanProps);
    if (!propCount) goto error;
    const void ** keys = calloc(propCount, sizeof(void *));
    if (!keys) goto error;
    CFDictionaryGetKeysAndValues(cleanProps, keys, NULL);
    UInt32 i;
    for (i = 0; i < propCount; i++) {
	if (!CFStringHasPrefix(keys[i], CFSTR("AppleLVM-"))) {
	    CFDictionaryRemoveValue(cleanProps, keys[i]);
	}
    }
    CFDictionaryRemoveValue(cleanProps, CFSTR(kAppleLVMVolumeStatusKey));	// redundant

    propData = IOCFSerialize(cleanProps, kNilOptions);
    if (!propData) {
	IOLog1("AppleRAIDLib - serialize on logical data props failed\n");
	goto error;
    }
    bcopy(CFDataGetBytePtr(propData), lvData->plist, CFDataGetLength(propData));

    IOLog1("LogicalVolumeProps = %s\n", lvData->plist);
    
    // start extents on multiple of sizeof(AppleRAIDExtentOnDisk) after the plist
    UInt32 firstExtent = CFDataGetLength(propData);
    firstExtent = firstExtent + (UInt32)((char *)lvData->plist - (char *)lvData);
    firstExtent = firstExtent + sizeof(AppleRAIDExtentOnDisk) - 1;
    firstExtent = firstExtent / sizeof(AppleRAIDExtentOnDisk) * sizeof(AppleRAIDExtentOnDisk);

    lvData->lvExtentsStart = firstExtent;
    AppleRAIDExtentOnDisk * extent = (AppleRAIDExtentOnDisk *)((char *)lvData + firstExtent);

    // sanity check before copy
    if (lvData->lvExtentsStart + sizeof(AppleRAIDExtentOnDisk) > lvData->lvHeaderSize) goto error;

    for (i = 0; i < extentCount; i++) {

	IOLog1("  %20llu - %12llu (%llu)\n",
	       extentList->extentByteOffset,
	       extentList->extentByteOffset + extentList->extentByteCount - 1,
	       extentList->extentByteCount);

	*extent++ = *extentList++;
    }
    
    CFRelease(propData);
    CFRelease(cleanProps);
    
    return lvData;

error:    
    if (lvData) free(lvData);
    if (propData) CFRelease(propData);
    if (cleanProps) CFRelease(cleanProps);
    return NULL;
}


AppleLVMVolumeRef
AppleLVMUpdateVolume(CFMutableDictionaryRef volProps)
{
    CFStringRef volRef = (CFStringRef)CFDictionaryGetValue(volProps, CFSTR(kAppleLVMVolumeUUIDKey));
    if (!volRef) return NULL;

    CFDataRef extentData = (CFDataRef)CFDictionaryGetValue(volProps, CFSTR("_extent data_"));
    if (extentData) {
	CFRetain(extentData);
	CFDictionaryRemoveValue(volProps, CFSTR("_extent data_"));
    } else {
	extentData = AppleLVMGetVolumeExtents(volRef);
	if (!extentData) return NULL;
    }

    AppleLVMVolumeOnDisk * lvData = buildLVMetaDataBlock(volProps, extentData);
    if (!lvData) goto error;
    
    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) {
	IOLog1("AppleLVMUpdateVolume - failed connecting to raid controller object?\n");
	goto error;
    }
	
    kern_return_t 	kr;
    char *		buffer = (char *)lvData;
    size_t		bufferSize = kAppleLVMVolumeOnDiskMinSize;
    char		updateData[0x1000];
    size_t		updateDataSize = sizeof(updateData);

    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleLVMUpdateLogicalVolume,// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   updateData,			// output
				   &updateDataSize);		// output size (in/out)
    
    AppleRAIDCloseConnection();

    if (kr != KERN_SUCCESS) {
	IOLog1("AppleLVMUpdateVolume failed with %x calling client.\n", kr);
	goto error;
    }

    CFRelease(extentData);
    free(lvData);

    CFRetain(volRef);
    return volRef;

error:
    if (extentData) CFRelease(extentData);
    if (lvData) free(lvData);
    return NULL;
}


bool
AppleLVMDestroyVolume(AppleLVMVolumeRef volRef)
{
    kern_return_t 	kr;
    size_t		bufferSize = kAppleRAIDMaxUUIDStringSize;
    char		buffer[bufferSize];
    char		returnData[0x1000];
    size_t		returnDataSize = sizeof(returnData);

    if (!CFStringGetCString(volRef, buffer, bufferSize, kCFStringEncodingUTF8)) {
	IOLog1("AppleLVMDestroyVolume() CFStringGetCString failed?\n");
	return NULL;
    }

    io_connect_t raidControllerPort = AppleRAIDOpenConnection();
    if (!raidControllerPort) return NULL;
    
    kr = IOConnectCallStructMethod(raidControllerPort,		// an io_connect_t returned from IOServiceOpen().
				   kAppleLVMDestroyLogicalVolume,// an index to the function in the Kernel.
				   buffer,			// input
				   bufferSize,			// input size
				   returnData,			// output
				   &returnDataSize);		// output size (in/out)

    if (kr != KERN_SUCCESS) {
	IOLog1("AppleLVMDestroyVolume failed with %x calling client.\n", kr);
    }

    AppleRAIDCloseConnection();
    
    return (kr == KERN_SUCCESS);
}

// Logical Volume level manipulations

UInt64
AppleLVMResizeVolume(CFMutableDictionaryRef lvProps, UInt64 newSize)
{
    UInt64 currentSize = 0;
    CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey));
    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &currentSize);
    if (!number || !currentSize) return 0;
    if (!newSize) return currentSize;
    if (currentSize == newSize) return 0;  // keeps us from calling update

    CFStringRef volRef = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeUUIDKey));
    if (!volRef) return 0;
    AppleRAIDSetRef lvgRef = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMGroupUUIDKey));
    if (!lvgRef) return false;
    CFMutableDictionaryRef lvgProps = AppleRAIDGetSetProperties(lvgRef);
    if (!lvgProps) return false;

    if (newSize > currentSize) {
	UInt64 freeSpace = 0;
	number = (CFNumberRef)CFDictionaryGetValue(lvgProps, CFSTR(kAppleRAIDLVGFreeSpaceKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &freeSpace);

	if (newSize > freeSpace) {
	    printf("AppleRAID: Insufficent free space to resize the logical volume.\n");
	    return 0;
	}
    }

    CFDataRef originalExtentData;
    originalExtentData = (CFDataRef)CFDictionaryGetValue(lvProps, CFSTR("_extent data_"));
    if (!originalExtentData) {
	originalExtentData = AppleLVMGetVolumeExtents(volRef);
	if (!originalExtentData) return 0;
	CFDictionarySetValue(lvProps, CFSTR("_extent data_"), originalExtentData);
	CFRelease(originalExtentData);
    }

    CFMutableDataRef extentData = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, originalExtentData);    
    CFDictionarySetValue(lvProps, CFSTR("_extent data_"), extentData);
    CFRelease(extentData);

    const void * sequenceNumber = CFDictionaryGetValue(lvgProps, CFSTR(kAppleRAIDSequenceNumberKey));
    if (!sequenceNumber) return false;
    CFDictionarySetValue(lvProps, CFSTR(kAppleLVMVolumeSequenceKey), sequenceNumber);
    CFRelease(lvgProps);

    //
    // truncate volume
    //

    if (newSize < currentSize) {

	UInt64 newExtentCount = truncateExtents(extentData, newSize);
	if (!newExtentCount) return 0;

	// set the size and extent count properties
	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &newSize);
	CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey), number);
	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &newExtentCount);
	CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeExtentCountKey), number);
	
	return newSize;
    }

    // fetch the lvg extent lists

    AppleRAIDSetRef setRef = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMGroupUUIDKey));
    if (!setRef) return 0;
	
    setInfo_t * lvgInfo = getSetInfo(setRef);
    if (!lvgInfo) return 0;

    // read in the logical volume group's free space data
    UInt64 lvgExtentCount = 0;
    number = (CFNumberRef)CFDictionaryGetValue(lvgInfo->setProps, CFSTR(kAppleRAIDLVGExtentsKey));
    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &lvgExtentCount);

    // Ask for the extent list
    AppleRAIDExtentOnDisk * lvgExtentList = getVolumeExtents(setRef, &lvgExtentCount);
    if (!lvgExtentList) return 0;

    // and peferred allocation region
    CFStringRef volumeLocation = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeLocationKey));
    if (!volumeLocation) return 0;

    //
    // try to extend the current final extent
    //

    UInt64 size = growLastExtent(extentData, lvgExtentList, lvgExtentCount, newSize);
    if (size) {

	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &newSize);
	CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey), number);
	
	if (size == newSize) return newSize;

	// we got something, but not enough
	currentSize = size;
    }

    //
    // try to allocate a new extent(s)
    //

    UInt64 extentCount = 0;
    AppleRAIDExtentOnDisk * extentList = allocateExtent(lvgExtentList, lvgExtentCount, newSize - currentSize, volumeLocation, &extentCount);
    if (!extentList) return 0;

    CFDataAppendBytes(extentData, (const UInt8 *)extentList, extentCount * sizeof(AppleRAIDExtentOnDisk));
    free(extentList);

    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &newSize);
    CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey), number);

    UInt64 newExtentCount = 0;
    number = (CFNumberRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeExtentCountKey));
    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &newExtentCount);
    newExtentCount += extentCount;
    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &newExtentCount);
    if (number) CFDictionaryReplaceValue(lvProps, CFSTR(kAppleLVMVolumeExtentCountKey), number);
    if (number) CFRelease(number);
    
    return newSize;
}


CFMutableDictionaryRef
AppleLVMSnapShotVolume(CFMutableDictionaryRef lvProps, CFStringRef snapType, UInt64 snapSize)
{
    UInt64 lvSize = 0;
    CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey));
    if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &lvSize);
    if (!number || !lvSize) return NULL;

    snapSize = MIN(snapSize, lvSize);

    UInt64 bitmapSize = calculateBitMapSize(lvSize, 0, NULL);

    CFStringRef lvgUUID = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMGroupUUIDKey));
    if (!lvgUUID) return NULL;
    CFStringRef lvUUID = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeUUIDKey));
    if (!lvgUUID) return NULL;
    CFStringRef location = (CFStringRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeLocationKey));
    if (!lvgUUID) return NULL;

    // this needs two logical volumes, one for data and one for bitmap/extent (how to resize?)

    CFMutableDictionaryRef bitmap = AppleLVMCreateVolume(lvgUUID, CFSTR(kAppleLVMVolumeTypeBitMap), bitmapSize, CFSTR(kAppleLVMVolumeLocationFast));
    if (!bitmap) return NULL;
    CFDictionarySetValue(bitmap, CFSTR(kAppleLVMParentUUIDKey), lvUUID);
    CFStringRef bitmapUUID = AppleLVMUpdateVolume(bitmap);
    if (!bitmapUUID) return NULL;
    
    CFMutableDictionaryRef snap = AppleLVMCreateVolume(lvgUUID, snapType, snapSize, location);
    if (!snap) return NULL;
    CFDictionarySetValue(snap, CFSTR(kAppleLVMParentUUIDKey), lvUUID);
    CFRelease(bitmap);

    return snap;
}


bool
AppleLVMMigrateVolume(AppleLVMVolumeRef volRef, AppleRAIDMemberRef toRef, CFStringRef volumeLocation)
{
    return false;
}


// Logical Group Member level manipulations

AppleLVMVolumeRef
AppleLVMRemoveMember(AppleLVMVolumeRef volRef, AppleRAIDMemberRef memberRef)
{
    return NULL;
}


AppleLVMVolumeRef
AppleLVMMergeGroups(AppleRAIDSetRef setRef, AppleRAIDSetRef donorSetRef)
{
    return NULL;
}
