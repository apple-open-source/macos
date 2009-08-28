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
#include <getopt.h>
#include <sys/fcntl.h>

#include <mach/mach.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>

#include <MediaKit/MKMedia.h>
#include <MediaKit/MKMediaAccess.h>

#include "AppleRAIDUserLib.h"
#include "AppleRAIDMember.h"   // for V2 header

#define AUTO_YES	1
#define AUTO_NO		2

static int		autoRebuild = 0;
static UInt64		blockSize = 0;
static bool		extents = false;
static char *		hint = 0;
static char *		nickname = 0;
static int		quickRebuild = 0;
static UInt64		timeout = 0;
static bool		verbose = false;
static UInt64		volSize = 0;

static void
usage()
{
    printf("\n");
    printf("usage:\n");
    printf("\n");
    printf("artest --list\n");
    printf("artest --lvlist [--extents] <lv uuid | lvg uuid>\n");
    printf("\n");
    printf("artest --create --name <nickname> --level <level> <options> disk1s3 disk2s3 disk3s3 ...\n");
    printf("artest --destroy <set uuid>\n");
    printf("\n");
    printf("artest --add <set uuid> disk1s3 ...\n");
    printf("artest --spare <set uuid> disk1s3 ...\n");
    printf("artest --remove <set uuid> <member uuid> ...\n");
    printf("artest --modify <set uuid> <options>\n");
    printf("\n");
    printf("artest --lvcreate <lvg uuid> <lvoptions>\n");
    printf("artest --lvdestroy <lv uuid>\n");
    printf("\n");
    printf("artest --lvmodify <lv uuid> <lvoptions>\n");
    printf("artest --lvresize <lv uuid> [--size <number>]\n");
    printf("artest --lvsnap <lv uuid> [--size <number>] --level=\"snap ro\"|\"snap rw\">\n");
    printf("\n");
    printf("artest --erase disk1s3 disk2s3 disk3s3 ...\n");
    printf("artest --header disk1s3 disk2s3 disk3s3 ...\n");
    printf("\n");
    printf("parameters:\n");
    printf("	<uuid> = XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\n");
    printf("	<nickname> = \"the volume name\"\n");
    printf("	<level> = stripe, mirror, concat, lvg\n");
    printf("	<hint> = Apple_HFS, RAIDNoMedia, RAIDNoFS\n");
    printf("	<options> = [--auto-rebuild=Yes,No] [--block-size=0x8000] [--hint=<hint>]\n");
    printf("	<options> = [--name=<nickname>] [--timeout=30] [--quick-rebuild=Yes]\n");
    printf("	<lvoptions> = [--level=concat] [--hint=<hint>] [--name=<nickname>] [--size 0xXXXXXXXXXXXXXXXX]\n");
    printf("\n");
    printf("global options:\n");
    printf("	--verbose\n");
    printf("    --watch (also a command)\n");
}


// there must be something like this already?

static void
CFPrintf(CFStringRef format, ...)
{
    CFStringRef cfstring;
    va_list argList;

    va_start(argList, format);
    cfstring = CFStringCreateWithFormatAndArguments(NULL, NULL, format, argList);
    va_end(argList);

    CFIndex cfstringSize = CFStringGetLength(cfstring);
    CFIndex stringSize = CFStringGetMaximumSizeForEncoding(cfstringSize, kCFStringEncodingUTF8) + 1;
    char *string = malloc(stringSize);
    if (CFStringGetCString(cfstring, string, stringSize, kCFStringEncodingUTF8)) {
	printf("%s", string);
    }
    free(string);
}


#define PMROptions	(PMEXTENDEDMODE | PMSECTORIZE | PMSORTMAP)

#define	kRAID_ONLINE	"Apple_RAID"
#define	kRAID_OFFLINE	"Apple_RAID_Offline"

// switches partition type

static bool
switchPartition(char * diskName, char * partitionType)
{
    char wholeDevicePath[256];
    sprintf(wholeDevicePath, "/dev/%s", diskName);

    unsigned int partitionNumber = 0;
    char * c = wholeDevicePath + 5 + 4;		        // skip over "/dev/disk"
    while (*c != 's' && *c++);				// look for 's'
    if (*c == 's') {
	*c = 0;						// clip off remainder
	sscanf(c+1, "%u", &partitionNumber);		// get partition number
    }
    if (!partitionNumber) return true;			// just assume it is a raid disk

#define LIVERAID
#ifdef LIVERAID
    char * optionString = "<dict> <key>Writable</key> <true/> <key>Shared Writer</key> <true/></dict>";
#else
    char * optionString = "<dict> <key>Writable</key> <true/> </dict>";
#endif    
    CFDictionaryRef options = IOCFUnserialize(optionString, kCFAllocatorDefault, 0, NULL);
    if (!options) exit(1);

    int32_t err;
    MKMediaRef device = MKMediaCreateWithPath(nil, wholeDevicePath, options, &err);
    CFRelease(options);
    if (!device || err) return false;
    
    options = NULL;
    MKStatus err2;
    CFMutableDictionaryRef media = MKCFReadMedia(options, device, &err2);
    if (!media || err2) goto Failure;

    // find and extract the 'Schemes' array 
    CFMutableArrayRef Schemes = (CFMutableArrayRef) CFDictionaryGetValue(media, CFSTR("Schemes"));
    if (!Schemes) goto Failure;

    // DMTool just grabs the first "default" scheme, so do the same
    CFMutableDictionaryRef Scheme = (CFMutableDictionaryRef) CFArrayGetValueAtIndex(Schemes, 0);
    if (!Scheme) goto Failure;

    // Then find and extract the 'Sections' array of that scheme:
    CFMutableArrayRef Sections = (CFMutableArrayRef) CFDictionaryGetValue(Scheme, CFSTR("Sections"));
    if (!Sections) goto Failure;

    // Every scheme can have multiple sections to it, we need to find the 'MAP' section:
    CFMutableDictionaryRef Section = (CFMutableDictionaryRef) CFArrayDictionarySearch(Sections, CFSTR("ID"), CFSTR("MAP"));
    if (!Section) goto Failure;

    // Then find and extract the 'Partitions' array of that section:
    CFMutableArrayRef Partitions = (CFMutableArrayRef) CFDictionaryGetValue(Section, CFSTR("Partitions"));
    if (!Partitions) goto Failure;

    CFNumberRef partitionIndex = CFNumberCreate(nil, kCFNumberSInt32Type, &partitionNumber);
    if (!partitionIndex) goto Failure;
    CFMutableDictionaryRef Partition = (CFMutableDictionaryRef) CFArrayDictionarySearch(Partitions, CFSTR("Partition ID"), partitionIndex);
    if (!Partition) goto Failure;

    // change the partition type (finally!)
    CFStringRef Type = CFStringCreateWithCString(nil, partitionType, kCFStringEncodingUTF8);
    if (!Type) goto Failure;
    CFDictionarySetValue(Partition, CFSTR("Type"), Type);

    CFMutableDictionaryRef woptions = CFDictionaryCreateMutable(kCFAllocatorDefault,
								2,
								&kCFTypeDictionaryKeyCallBacks,
								&kCFTypeDictionaryValueCallBacks);
    if (!woptions) goto Failure;
    CFDictionarySetValue(woptions, CFSTR("Retain existing content"), kCFBooleanTrue);
    CFDictionarySetValue(woptions, CFSTR("Direct Mode"), kCFBooleanTrue);

    err2 = MKCFWriteMedia(media, nil, nil, woptions, device);

    MKCFDisposeMedia(media);
    CFRelease(woptions);
    CFRelease(device);
    return !err2;

Failure:
    if (media) MKCFDisposeMedia(media);
    if (device) CFRelease(device);

    return false;
}

static void
addMember(char * uuid, CFStringRef type, int argc, char* argv[])
{
    if (argc < 1) {
	usage();
	exit(1);
    }

    CFStringRef setUUID = CFStringCreateWithCString(kCFAllocatorDefault, uuid, kCFStringEncodingUTF8);
    if (!setUUID) exit(1);

    CFMutableDictionaryRef setInfo = AppleRAIDGetSetProperties(setUUID);
    if (!setInfo) {
	printf("addMember - failed to find RAID set \"%s\"\n", uuid);
	exit(1);
    }

    int partitionCount = argc;
    char **firstPartition = argv;

    while (argc--) {
	printf("adding partition \"%s\"\n", *argv);
	CFStringRef partitionName = CFStringCreateWithCString(kCFAllocatorDefault, *argv, kCFStringEncodingUTF8);

	bool success = switchPartition(*argv, kRAID_OFFLINE);
	if (!success) {
	    printf("switching the partition on \"%s\" to %s FAILED.\n", *argv, kRAID_OFFLINE);
	    exit(1);
	}

	AppleRAIDMemberRef member = AppleRAIDAddMember(setInfo, partitionName, type);
	if (!member) {
	    printf("addMember - there was problem adding partition \"%s\"\n", *argv);
	    exit(1);
	}

	argv++;
    }

    printf("updating set \"%s\".\n", uuid);
    AppleRAIDSetRef set = AppleRAIDUpdateSet(setInfo);
    CFRelease(setInfo);

    if (!set) {
	printf("something went wrong adding members to the set\n");
	exit(1);
    }

    while (partitionCount--) {
	printf("switching the partition on \"%s\" to %s.\n", *firstPartition, kRAID_ONLINE);
	bool success = switchPartition(*firstPartition, kRAID_ONLINE);
	if (!success) {
	    printf("switching the partition on \"%s\" to %s FAILED.\n", *firstPartition, kRAID_ONLINE);
	    exit(1);
	}

	firstPartition++;
    }

    // at this point the code should wait for notifications that members and the set are available
}

static void
createSet(char * levelCString, char * nameCString, int argc, char* argv[])
{
    if (!levelCString || !nameCString || argc < 2) {
	usage();
	exit(1);
    }

    CFStringRef level = CFStringCreateWithCString(kCFAllocatorDefault, levelCString, kCFStringEncodingUTF8);
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!level || !name) exit(1);

    // get list of available raid set descriptions
    CFMutableArrayRef descriptionArray = AppleRAIDGetSetDescriptions();

    CFIndex dCount = CFArrayGetCount(descriptionArray);
    CFIndex dIndex = 0;
    CFStringRef dLevel = 0;
    for (dIndex = 0; dIndex < dCount; dIndex++) {
	CFMutableDictionaryRef setDescription = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(descriptionArray, dIndex);
	dLevel = (CFStringRef)CFDictionaryGetValue(setDescription, CFSTR(kAppleRAIDLevelNameKey));
	if (!dLevel) break;

	if (CFStringCompare(dLevel, level, kCFCompareCaseInsensitive) == kCFCompareEqualTo) break;
	dLevel = 0;
    }
    if (dLevel == 0) {
	printf("raid level \"%s\" is not valid?.\n", levelCString);
	exit(1);
    }

    CFMutableDictionaryRef setInfo = AppleRAIDCreateSet(dLevel, name);
    if (!setInfo) exit(1);

    char **firstPartition = argv;
    int partitionCount = argc;

    while (argc--) {
	printf("adding partition \"%s\"\n", *argv);
	CFStringRef partitionName = CFStringCreateWithCString(kCFAllocatorDefault, *argv, kCFStringEncodingUTF8);

	AppleRAIDMemberRef member = AppleRAIDAddMember(setInfo, partitionName, CFSTR(kAppleRAIDMembersKey));
	if (!member) {
	    printf("there was problem adding partition \"%s\"\n", *argv);
	    exit(1);
	}
	CFShow(member);

	bool success = switchPartition(*argv, kRAID_OFFLINE);
	if (!success) {
	    printf("switching the partition on \"%s\" to %s FAILED.\n", *argv, kRAID_OFFLINE);
	    exit(1);
	}

	argv++;
    }

    if (autoRebuild == AUTO_YES) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetAutoRebuildKey), (void *)kCFBooleanTrue);
    if (quickRebuild == AUTO_YES) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetQuickRebuildKey), (void *)kCFBooleanTrue);
    if (blockSize) {
	CFNumberRef blockSizeCF = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &blockSize);
	if (blockSizeCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDChunkSizeKey), (void *)blockSizeCF);
    }
    if (hint) {
	CFStringRef hintCF = CFStringCreateWithCString(kCFAllocatorDefault, hint, kCFStringEncodingUTF8);
	if (hintCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetContentHintKey), (void *)hintCF);
    }
    if (timeout) {
	CFNumberRef timeoutCF = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &timeout);
	if (timeoutCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetTimeoutKey), (void *)timeoutCF);
    }

    printf("creating the set \"%s\".\n", nameCString);
    AppleRAIDSetRef set = AppleRAIDUpdateSet(setInfo);

    CFPrintf(CFSTR("created set %@\n"), set);
    CFRelease(setInfo);

    if (!set) {
	printf("something went wrong creating the set\n");
	exit(0);
    }

    while (partitionCount--) {
	printf("switching the partition on \"%s\" to %s.\n", *firstPartition, kRAID_ONLINE);
	bool success = switchPartition(*firstPartition, kRAID_ONLINE);
	if (!success) {
	    printf("switching the partition on \"%s\" to %s FAILED.\n", *firstPartition, kRAID_ONLINE);
	    exit(1);
	}

	firstPartition++;
    }

    // at this point the code should wait for notifications that members and the set are available
}

static void
destroySet(char * nameCString, int argc, char* argv[])
{
    if (!nameCString || argc) {
	usage();
	exit(1);
    }

    CFStringRef setUUID = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!setUUID) exit(1);

    bool success = AppleRAIDDestroySet(setUUID);
    if (!success) {
	printf("there was a problem destroying the set %s.\n", nameCString);
    }
}


static void
erasePartition(int argc, char* argv[])
{
    if (argc < 1) {
	usage();
	exit(1);
    }

    while (argc--) {

	printf("switching the partition type on \"%s\" to %s.\n", *argv, kRAID_OFFLINE);
	bool success = switchPartition(*argv, kRAID_OFFLINE);
	if (!success) {
	    printf("switching partition type FAILED.\n");
	}
	
	printf("erasing raid headers on partition \"%s\"\n", *argv);
	CFStringRef partitionName = CFStringCreateWithCString(kCFAllocatorDefault, *argv, kCFStringEncodingUTF8);
	success = AppleRAIDRemoveHeaders(partitionName);
	if (!success) {
	    printf("erasing the raid headers on partition \"%s\" FAILED.\n", *argv);
	}

	argv++;
    }
}

static void
dumpHeader(int argc, char* argv[])
{
    if (argc < 1) {
	usage();
	exit(1);
    }

    while (argc--) {

//	printf("dumping the raid header on \"%s\".\n", *argv);

	CFStringRef partitionName = CFStringCreateWithCString(kCFAllocatorDefault, *argv, kCFStringEncodingUTF8);
	CFDataRef data = AppleRAIDDumpHeader(partitionName);

	if (data) {
	    AppleRAIDHeaderV2 * header = (AppleRAIDHeaderV2 *)CFDataGetBytePtr(data);
	    if (header) printf("%s\n", header->plist);
	}

	argv++;
    }
}

static void
dumpSetProperties(CFMutableDictionaryRef set)
{
    CFPrintf(CFSTR("\n%@\n"), CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetUUIDKey)));
    CFPrintf(CFSTR("\t\"%@\" type = %@ /dev/%@\n"),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetNameKey)),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDLevelNameKey)),
	     CFDictionaryGetValue(set, CFSTR("BSD Name")));
    CFPrintf(CFSTR("\tstatus = %@, sequence = %@\n"),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDStatusKey)),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDSequenceNumberKey)));
    CFPrintf(CFSTR("\tchunk count = %@, chunk size = %@\n"),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDChunkCountKey)),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDChunkSizeKey)));

    CFStringRef level = CFDictionaryGetValue(set, CFSTR(kAppleRAIDLevelNameKey));
    if (CFStringCompare(level, CFSTR("mirror"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	CFPrintf(CFSTR("\tcontent hint = %@, auto = %@, quick = %@, timeout = %@\n"),
		 CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetContentHintKey)),
		 CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetAutoRebuildKey)),
		 CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetQuickRebuildKey)),
		 CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetTimeoutKey)));
    } else if (CFStringCompare(level, CFSTR("LVG"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	CFPrintf(CFSTR("\tcontent hint = %@, lv count = %@, free space %@\n"),
		 CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetContentHintKey)),
		 CFDictionaryGetValue(set, CFSTR(kAppleRAIDLVGVolumeCountKey)),
		 CFDictionaryGetValue(set, CFSTR(kAppleRAIDLVGFreeSpaceKey)));
    } else {
	CFPrintf(CFSTR("\tcontent hint = %@\n"), CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetContentHintKey)));
    }

    if (verbose) CFShow(set);
}

static void
dumpMemberProperties(CFMutableDictionaryRef member)
{
    CFPrintf(CFSTR("\t%@\n"), CFDictionaryGetValue(member, CFSTR(kAppleRAIDMemberUUIDKey)));
    CFPrintf(CFSTR("\t\tmember index = %@, sequence = %@, /dev/%@\n"),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDMemberIndexKey)),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDSequenceNumberKey)),
	     CFDictionaryGetValue(member, CFSTR("BSD Name")));
    CFPrintf(CFSTR("\t\tstatus = %@, chunk count = %@, rebuild = %@\n"),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDMemberStatusKey)),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDChunkCountKey)),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDRebuildStatus)));

    if (verbose) CFShow(member);
}

static void
listRAIDSets()
{
    UInt32 filter = kAppleRAIDAllSets;
    CFMutableArrayRef theList = AppleRAIDGetListOfSets(filter);
    CFIndex setCount = theList ? CFArrayGetCount(theList) : 0;

    printf("AppleRAIDGetListOfSets found %d sets\n", (int)setCount); fflush(stdout);

    // get each set's properties
    CFIndex i;
    for (i=0; i < setCount; i++) {

	CFStringRef setName = (CFStringRef)CFArrayGetValueAtIndex(theList, i);
	if (setName) {
	    CFMutableDictionaryRef setProp = AppleRAIDGetSetProperties(setName);
	    if (!setProp) {
		CFPrintf(CFSTR("%@\n\t(lookup failed)\n"), setName);
		continue;
	    }
	    dumpSetProperties(setProp);
	    
	    // get each member's properties
	    bool spares = false;
	    CFMutableArrayRef members = (CFMutableArrayRef)CFDictionaryGetValue(setProp, CFSTR(kAppleRAIDMembersKey));
	    CFIndex j, memberCount = members ? CFArrayGetCount(members) : 0;

	    printf("members (%d):\n", (int)memberCount);

	again:
	    for (j=0; j < memberCount; j++) {

		CFStringRef memberName = (CFStringRef)CFArrayGetValueAtIndex(members, j);
		if (memberName) {
		    CFMutableDictionaryRef memberProp = AppleRAIDGetMemberProperties(memberName);	
		    if (memberProp) {
			dumpMemberProperties(memberProp);
			CFRelease(memberProp);
		    } else {
			CFPrintf(CFSTR("\t%@\n\t\t(lookup failed)\n"), memberName);
		    }
		}
	    }
		    
	    if (!spares) {
		members = (CFMutableArrayRef)CFDictionaryGetValue(setProp, CFSTR(kAppleRAIDSparesKey));
		memberCount = members ? CFArrayGetCount(members) : 0;
		if (memberCount) printf("spares: (%d)\n", (int)memberCount);
		spares = true;
		goto again;
	    }
	    spares = false;
	    CFRelease(setProp);
	}
    }
    if (theList) CFRelease(theList);
}

static void
modifySet(char * setUUIDCString, int argc, char* argv[])
{
    if (!setUUIDCString || argc) {
	usage();
	exit(1);
    }
    
    CFStringRef setUUID = CFStringCreateWithCString(kCFAllocatorDefault, setUUIDCString, kCFStringEncodingUTF8);
    if (!setUUID) exit(1);

    CFMutableDictionaryRef setInfo = AppleRAIDGetSetProperties(setUUID);
    if (!setInfo) {
	printf("modifySet - failed to find RAID set \"%s\"\n", setUUIDCString);
	exit(1);
    }

    if (autoRebuild == AUTO_YES) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetAutoRebuildKey), (void *)kCFBooleanTrue);
    if (autoRebuild == AUTO_NO) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetAutoRebuildKey), (void *)kCFBooleanFalse);
    if (quickRebuild == AUTO_YES) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetQuickRebuildKey), (void *)kCFBooleanTrue);
    if (quickRebuild == AUTO_NO) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetQuickRebuildKey), (void *)kCFBooleanFalse);
    if (blockSize) {
	CFNumberRef blockSizeCF = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &blockSize);
	if (blockSizeCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDChunkSizeKey), (void *)blockSizeCF);
    }
    if (hint) {
	CFStringRef hintCF = CFStringCreateWithCString(kCFAllocatorDefault, hint, kCFStringEncodingUTF8);
	if (hintCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetContentHintKey), (void *)hintCF);
    }
    if (timeout) {
	CFNumberRef timeoutCF = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &timeout);
	if (timeoutCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetTimeoutKey), (void *)timeoutCF);
    }
    if (nickname) {
	CFStringRef nicknameCF = CFStringCreateWithCString(kCFAllocatorDefault, nickname, kCFStringEncodingUTF8);
	if (nicknameCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetNameKey), (void *)nicknameCF);
    }

    printf("modifying the set \"%s\".\n", setUUIDCString);
    AppleRAIDSetRef set = AppleRAIDUpdateSet(setInfo);

    if (!set) printf("something went wrong updating the set\n");

    if (set) CFRelease(set);
    CFRelease(setInfo);
    CFRelease(setUUID);
}

static void
removeMember(char * setUUIDCString, int argc, char* argv[])
{
    if (!setUUIDCString || argc < 1) {
	usage();
	exit(1);
    }
    
    CFStringRef setUUID = CFStringCreateWithCString(kCFAllocatorDefault, setUUIDCString, kCFStringEncodingUTF8);
    if (!setUUID) exit(1);

    CFMutableDictionaryRef setInfo = AppleRAIDGetSetProperties(setUUID);
    if (!setInfo) {
	printf("removeMember - failed to find RAID set \"%s\"\n", setUUIDCString);
	exit(1);
    }

    int partitionCount = argc;
    char **firstPartition = argv;

    while (argc--) {
	printf("removing member \"%s\"\n", *argv);

	CFStringRef memberUUID = CFStringCreateWithCString(kCFAllocatorDefault, *argv, kCFStringEncodingUTF8);

	bool success = AppleRAIDRemoveMember(setInfo, memberUUID);
	if (!success) {
	    printf("there was problem removing the member \"%s\"\n", *argv);
	}

	CFRelease(memberUUID);
	argv++;
    }

    AppleRAIDSetRef set = AppleRAIDUpdateSet(setInfo);

    if (!set) printf("something went wrong updating the set\n");

    if (set) CFRelease(set);
    CFRelease(setInfo);
    CFRelease(setUUID);

    while (partitionCount--) {
	printf("switching the partition on \"%s\" to %s.\n", *firstPartition, kRAID_OFFLINE);
	bool success = switchPartition(*firstPartition, kRAID_OFFLINE);
	if (!success) {
	    printf("switching the partition on \"%s\" to %s FAILED.\n", *firstPartition, kRAID_OFFLINE);
	}

	firstPartition++;
    }
}

static void
createLogicalVolume(char * nameCString, char * volTypeCString, int argc, char* argv[])
{
    if (!nameCString || argc) {
	usage();
	exit(1);
    }

    CFStringRef lvgUUID = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!lvgUUID) exit(1);

    if (!volSize) volSize = 0x40000000;

    CFStringRef volType = 0;
    if (volTypeCString) {
	volType = CFStringCreateWithCString(kCFAllocatorDefault, volTypeCString, kCFStringEncodingUTF8);
    } else {
	volType = CFSTR(kAppleLVMVolumeTypeConcat);
    }
    if (!volType) exit(1);

    CFMutableDictionaryRef lvDict = AppleLVMCreateVolume(lvgUUID, volType, volSize, CFSTR(kAppleLVMVolumeLocationFast));
    if (!lvDict) {
	printf("there was a problem allocating/setting up the logical volume\n");
	exit(1);
    }

    if (nickname) {
	CFStringRef nameCF = CFStringCreateWithCString(kCFAllocatorDefault, nickname, kCFStringEncodingUTF8);
	if (nameCF) AppleLVMModifyVolume(lvDict, CFSTR(kAppleLVMVolumeNameKey), (void *)nameCF);
    }

    if (hint) {
	CFStringRef hintCF = CFStringCreateWithCString(kCFAllocatorDefault, hint, kCFStringEncodingUTF8);
	if (hintCF) AppleLVMModifyVolume(lvDict, CFSTR(kAppleLVMVolumeContentHintKey), (void *)hintCF);
    }

    AppleLVMVolumeRef volRef = AppleLVMUpdateVolume(lvDict);
    if (!volRef) {
	printf("there was a problem writing out the logical volume onto the group %s.\n", nameCString);
	exit(2);
    }
}

static void
destroyLogicalVolume(char * nameCString, int argc, char* argv[])
{
    if (!nameCString || argc) {
	usage();
	exit(1);
    }

    CFStringRef lvUUID = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!lvUUID) exit(1);

    bool success = AppleLVMDestroyVolume(lvUUID);
    if (!success) {
	printf("there was a problem destroying the logical volume %s.\n", nameCString);
    }
}

static void
dumpLogicalVolumeExtents(CFMutableDictionaryRef lv)
{
    CFStringRef lvUUID = CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeUUIDKey));
    if (!lvUUID) { printf("\ninternal error, no uuid in lv dict\n"); return; };
    
    CFDataRef extentData = (CFDataRef)AppleLVMGetVolumeExtents(lvUUID);
    if (!extentData) { printf("\nno extent data found?\n"); return; };

    AppleRAIDExtentOnDisk * extentList = (AppleRAIDExtentOnDisk *)CFDataGetBytePtr(extentData);
    UInt64 extentCount = CFDataGetLength(extentData) / sizeof(AppleRAIDExtentOnDisk);
    if (!extentCount || !extentList) { printf("\nextent data empty?\n"); return; };

    printf("\textent list:\n");
	       
    UInt32 i;
    for (i = 0; i < extentCount; i++) {
	printf("  %20llu - %12llu (%llu)\n",
	       extentList[i].extentByteOffset,
	       extentList[i].extentByteOffset + extentList[i].extentByteCount - 1,
	       extentList[i].extentByteCount);
    }
}

static void
dumpLogicalVolumeProperties(CFMutableDictionaryRef lv)
{
    CFPrintf(CFSTR("\n%@\n"), CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeUUIDKey)));
    CFPrintf(CFSTR("\t\"%@\" type = %@ /dev/%@\n"),
	     CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeNameKey)),
	     CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeTypeKey)),
	     CFDictionaryGetValue(lv, CFSTR("BSD Name")));
    CFPrintf(CFSTR("\tbyte size = %@, content hint = %@\n"),
	     CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeSizeKey)),
	     CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeContentHintKey)));
    CFPrintf(CFSTR("\tstatus = %@, sequence = %@ extent count = %@\n"),
	     CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeStatusKey)),
	     CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeSequenceKey)),
	     CFDictionaryGetValue(lv, CFSTR(kAppleLVMVolumeExtentCountKey)));
    CFStringRef parent = CFDictionaryGetValue(lv, CFSTR(kAppleLVMParentUUIDKey));
    if (parent) CFPrintf(CFSTR("\tparent = %@\n"), parent);

    if (extents) dumpLogicalVolumeExtents(lv);

    if (verbose) CFShow(lv);
}

static void listLogicalVolumes(char * nameCString, int argc, char* argv[]);

static void
listAllLogicalVolumes()
{
    UInt32 filter = kAppleRAIDAllSets;
    CFMutableArrayRef theList = AppleRAIDGetListOfSets(filter);
    CFIndex setCount = theList ? CFArrayGetCount(theList) : 0;

    CFIndex i;
    for (i=0; i < setCount; i++) {

	CFStringRef setName = (CFStringRef)CFArrayGetValueAtIndex(theList, i);
	if (setName) {
	    CFMutableDictionaryRef setProp = AppleRAIDGetSetProperties(setName);
	    if (!setProp) continue;
	    
	    CFStringRef level = CFDictionaryGetValue(setProp, CFSTR(kAppleRAIDLevelNameKey));
	    if (!level) continue;
	    
	    if (CFStringCompare(level, CFSTR("LVG"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {

		dumpSetProperties(setProp);

		char uuid[256];
		if (CFStringGetCString(setName, uuid, 256, kCFStringEncodingUTF8)) {
		    listLogicalVolumes(uuid, 0, NULL);
		}
	    }

	    CFRelease(setProp);
	}
    }
    if (theList) CFRelease(theList);
}


static void
listLogicalVolumes(char * nameCString, int argc, char* argv[])
{
    if (!nameCString && !argc) {
	listAllLogicalVolumes();
	exit(0);
    }

    if (!nameCString && argc == 1) nameCString = argv[0];  // hack
    if (!nameCString) { 
	usage();
	exit(1);
    }

    CFStringRef lvUUID = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!lvUUID) exit(1);

    // try for one logical volume
    CFMutableDictionaryRef props = AppleLVMGetVolumeProperties(lvUUID);
    if (props) {
	dumpLogicalVolumeProperties(props);
	CFRelease(props);
	return;
    }

    // go for all volumes in the group
    CFMutableArrayRef volumes = AppleLVMGetVolumesForGroup(lvUUID, NULL);
    if (!volumes) {
	printf("there was a problem finding the logical volume/group %s.\n", nameCString);
	exit(1);
    }

    int count = CFArrayGetCount(volumes);
    if (count == 0) {
	printf("\n%s contains no logical volumes.\n", nameCString);
	return;
    }
    
    int i;
    for (i = 0; i < count; i++) {
	
	CFStringRef UUID = (CFStringRef)CFArrayGetValueAtIndex(volumes, i);

	CFMutableDictionaryRef props = AppleLVMGetVolumeProperties(UUID);
	if (props) {
	    dumpLogicalVolumeProperties(props);
	    CFRelease(props);
	}
    }
    printf("\n");
}

static void
modifyLogicalVolume(char * nameCString, int argc, char* argv[])
{
    if (!nameCString || argc) {
	usage();
	exit(1);
    }

    CFStringRef lvUUID = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!lvUUID) exit(1);

    CFMutableDictionaryRef lvDict = AppleLVMGetVolumeProperties(lvUUID);
    if (!lvDict) {
	printf("there was a problem finding the logical volume %s.\n", nameCString);
	exit(1);
    }

    if (nickname) {
	CFStringRef nameCF = CFStringCreateWithCString(kCFAllocatorDefault, nickname, kCFStringEncodingUTF8);
	if (nameCF) AppleLVMModifyVolume(lvDict, CFSTR(kAppleLVMVolumeNameKey), (void *)nameCF);
    }

    if (hint) {
	CFStringRef hintCF = CFStringCreateWithCString(kCFAllocatorDefault, hint, kCFStringEncodingUTF8);
	if (hintCF) AppleLVMModifyVolume(lvDict, CFSTR(kAppleLVMVolumeContentHintKey), (void *)hintCF);
    }

    AppleLVMVolumeRef volRef = AppleLVMUpdateVolume(lvDict);
    if (!volRef) {
	printf("there was a problem updating the logical volume %s.\n", nameCString);
	exit(2);
    }
}

static void
resizeLogicalVolume(char * nameCString, int argc, char* argv[])
{
    if (!nameCString || argc) {
	usage();
	exit(1);
    }

    CFStringRef lvUUID = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!lvUUID) exit(1);

    CFMutableDictionaryRef lvProps = AppleLVMGetVolumeProperties(lvUUID);
    if (!lvProps) {
	printf("there was a problem finding the logical volume %s.\n", nameCString);
	exit(1);
    }

    if (!volSize) {
	CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &volSize);
	volSize += 0x40000000;
    }

    UInt64 newSize = AppleLVMResizeVolume(lvProps, volSize);
    if (!newSize) {
	printf("there was a problem resizing the logical volume %s.\n", nameCString);
	exit(1);
    }
    
    AppleLVMVolumeRef volRef = AppleLVMUpdateVolume(lvProps);
    if (!volRef) {
	printf("there was a problem updating the logical volume %s.\n", nameCString);
	exit(2);
    }

    printf("the logical volume %s has been resized to 0x%lld.\n", nameCString, newSize);
}

static void
snapshotLogicalVolume(char * nameCString, char * snapType, int argc, char* argv[])
{
    if (!nameCString || argc) {
	usage();
	exit(1);
    }

    CFStringRef lvUUID = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!lvUUID) exit(1);

    CFMutableDictionaryRef lvProps = AppleLVMGetVolumeProperties(lvUUID);
    if (!lvProps) {
	printf("there was a problem finding the logical volume %s.\n", nameCString);
	exit(1);
    }

    CFStringRef lvType = 0;
    if (snapType) {
	lvType = CFStringCreateWithCString(kCFAllocatorDefault, snapType, kCFStringEncodingUTF8);
    } else {
	lvType = CFSTR(kAppleLVMVolumeTypeSnapRO);
    }
    if (!lvType) exit(1);

    UInt64 percent = 0;
    if (volSize < 100) percent = volSize;
    if (volSize <= 100) volSize = 0;

    if (!volSize) {
	CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(lvProps, CFSTR(kAppleLVMVolumeSizeKey));
	if (number) CFNumberGetValue(number, kCFNumberSInt64Type, &volSize);
    }
    if (percent) volSize = volSize * percent / (UInt64)100;

    // create the snap
    CFMutableDictionaryRef snapProps = AppleLVMSnapShotVolume(lvProps, lvType, volSize);
    if (!snapProps) {
	printf("there was a problem initializing the snapshot for %s.\n", nameCString);
	exit(1);
    }

    // write to disk
    AppleLVMVolumeRef snapRef = AppleLVMUpdateVolume(snapProps);
    if (!snapRef) {
	printf("there was a problem updating the snapshot volume.\n");
	exit(2);
    }

    printf("the logical volume %s now has a snapshot.\n", nameCString);
}

static void
callBack(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    char setName[128];
    if (!CFStringGetCString(object, setName, 128, kCFStringEncodingUTF8)) bcopy("bogus set name?", setName, 128);
    
    char event[128];
    if (!CFStringGetCString(name, event, 128, kCFStringEncodingUTF8)) bcopy("bogus event string?", event, 128);
    
    printf("Notification for %s, event = %s.\n", setName, event); fflush(stdout);
}

static void signalHandler(int sigraised);

int
main(int argc, char* argv[])
{
    bool add = false, create = false, destroy = false, erase = false, header = false;
    bool list = false, lvcreate = false, lvdestroy = false, lvlist = false, lvmodify = false, lvresize = false, lvsnap = false;
    bool modify = false, remove = false, spare = false, watch = false;
    char * setLevel = 0, * setName = 0; 

    /* options descriptor */
    static struct option longopts[] = {
	{ "add",	required_argument,	0,	'a' },
	{ "create",	no_argument,		0,	'c' },
	{ "destroy",	required_argument,	0,	'd' },
	{ "erase",	no_argument,		0,	'e' },
	{ "header",	no_argument,		0,	'h' },
	{ "list",	no_argument,		0,	'l' },
	{ "modify",	required_argument,	0,	'm' },
	{ "remove",	required_argument,	0,	'r' },
	{ "spare",	required_argument,	0,	's' },
	{ "watch",	no_argument,		0,	'w' },
	
	{ "lvcreate",	required_argument,	0,	'C' },
	{ "lvdestroy",	required_argument,	0,	'D' },
	{ "lvlist",	no_argument,		0,	'L' },
	{ "lvmodify",	required_argument,	0,	'M' },
	{ "lvresize",	required_argument,	0,	'R' },
	{ "lvsnap",	required_argument,	0,	'S' },
	
	{ "auto-rebuild",required_argument,	0,	'A' },
	{ "block-size", required_argument,	0,	'B' },
	{ "extents",	no_argument,		0,	'E' },
	{ "hint",	required_argument,	0,	'H' },
	{ "level",	required_argument,	0,	'V' },
	{ "name",	required_argument,	0,	'N' },
	{ "quick-rebuild",required_argument,	0,	'Q' },
	{ "size",	required_argument,	0,	'Z' },
	{ "timeout",	required_argument,	0,	'T' },

	{ "verbose",	no_argument,		0,	'v' },
	{ "help",	no_argument,		0,	'?' },
	{ 0,		0,			0,	0   }
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "a:cd:ehlm:r:s:wC:D:LM:R:S:A:B:EH:V:N:Q:Z:T:v?", longopts, NULL)) != -1) {
	
	switch(ch) {

	case 'a':
	    add = true;
	    setName = strdup(optarg);
	    break;
	case 'c':
	    create = true;
	    break;
	case 'd':
	    destroy = true;
	    setName = strdup(optarg);
	    break;
	case 'e':
	    erase = true;
	    break;
	case 'h':
	    header = true;
	    break;
	case 'l':
	    list = true;
	    break;
	case 'm':
	    modify = true;
	    setName = strdup(optarg);
	    break;
	case 'r':
	    remove = true;
	    setName = strdup(optarg);
	    break;
	case 's':
	    spare = true;
	    setName = strdup(optarg);
	    break;
	case 'w':
	    watch = true;
	    break;

	    
	case 'C':
	    lvcreate = true;
	    setName = strdup(optarg);
	    break;
	case 'D':
	    lvdestroy = true;
	    setName = strdup(optarg);
	    break;
	case 'L':
	    lvlist = true;
	    break;
	case 'M':
	    lvmodify = true;
	    setName = strdup(optarg);
	    break;
	case 'R':
	    lvresize = true;
	    setName = strdup(optarg);
	    break;
	case 'S':
	    lvsnap = true;
	    setName = strdup(optarg);
	    break;


	case 'A':
	    autoRebuild = ((optarg[0] == 'Y') || (optarg[0] == 'y')) ? AUTO_YES : AUTO_NO;
	    break;
	case 'B':
	    sscanf(optarg, "%lli", &blockSize);
	    break;
	case 'E':
	    extents = true;
	    break;
	case 'H':
	    hint = strdup(optarg);
	    break;
	case 'V':
	    setLevel = strdup(optarg);
	    break;
	case 'N':
	    nickname = strdup(optarg);
	    break;
	case 'Q':
	    quickRebuild = ((optarg[0] == 'Y') || (optarg[0] == 'y')) ? AUTO_YES : AUTO_NO;
	    break;
	case 'Z':
	    sscanf(optarg, "%lli", &volSize);
	    break;
	case 'T':
	    sscanf(optarg, "%lli", &timeout);
	    break;


	case 'v':
	    verbose = true;
	    break;
	case 0:
	case '?':
	default:
	    usage();
	    exit(0);
	}
    }
    argc -= optind;
    argv += optind;

    if (!add && !create && !destroy && !erase && !header && !list && !modify && !remove && !spare && !watch &&
	!lvcreate && !lvdestroy && !lvlist && !lvmodify && !lvresize && !lvsnap) {
	usage();
	exit(0);
    }

    if (list) {
	listRAIDSets();
	exit(0);
    }

    if (lvlist) {
	listLogicalVolumes(NULL, argc, argv);
	exit(0);
    }

    if (geteuid()) {
	printf("ERROR: you must be super user for this operation.\n");
	exit(1);
    }
	
    if (erase) {
	erasePartition(argc, argv);
	exit(0);
    };

    if (header) {
	dumpHeader(argc, argv);
	exit(0);
    };

    if (watch) {

	CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
					NULL,					// const void *observer
					callBack,
					CFSTR(kAppleRAIDNotificationSetDiscovered),
					NULL,					// const void *object
					CFNotificationSuspensionBehaviorHold);

	CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
					NULL,					// const void *observer
					callBack,
					CFSTR(kAppleRAIDNotificationSetTerminated),
					NULL,					// const void *object
					CFNotificationSuspensionBehaviorHold);

	CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
					NULL,					// const void *observer
					callBack,
					CFSTR(kAppleRAIDNotificationSetChanged),
					NULL,					// const void *object
					CFNotificationSuspensionBehaviorHold);

	CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
					NULL,					// const void *observer
					callBack,
					CFSTR(kAppleLVMNotificationVolumeDiscovered),
					NULL,					// const void *object
					CFNotificationSuspensionBehaviorHold);

	CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
					NULL,					// const void *observer
					callBack,
					CFSTR(kAppleLVMNotificationVolumeTerminated),
					NULL,					// const void *object
					CFNotificationSuspensionBehaviorHold);

	CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
					NULL,					// const void *observer
					callBack,
					CFSTR(kAppleLVMNotificationVolumeChanged),
					NULL,					// const void *object
					CFNotificationSuspensionBehaviorHold);

	// this will not fail if there is no raid controller, ie, if AppleRAID class is not instantiated in the kernel

	AppleRAIDEnableNotifications();
    }


    if (add) addMember(setName, CFSTR(kAppleRAIDMembersKey), argc, argv);
    if (create) createSet(setLevel, nickname, argc, argv);
    if (destroy) destroySet(setName, argc, argv);
    if (modify) modifySet(setName, argc, argv);
    if (remove) removeMember(setName, argc, argv);
    if (spare) addMember(setName, CFSTR(kAppleRAIDSparesKey), argc, argv);

    
    if (lvcreate) createLogicalVolume(setName, setLevel, argc, argv);
    if (lvdestroy) destroyLogicalVolume(setName, argc, argv);
    if (lvmodify) modifyLogicalVolume(setName, argc, argv);
    if (lvresize) resizeLogicalVolume(setName, argc, argv);
    if (lvsnap) snapshotLogicalVolume(setName, setLevel, argc, argv);

    
    if (watch) {

	printf("watching...\n");

	// Set up a signal handler so we can clean up when we're interrupted from the command line
	// Otherwise we stay in our run loop forever.
	sig_t oldHandler = signal(SIGINT, signalHandler);
	if (oldHandler == SIG_ERR) {
	    printf("Could not establish new signal handler");
	    exit(1);
	}

	// Start the run loop. Now we'll receive notifications.
	//
	printf("Starting run loop.\n");
	CFRunLoopRun();
        
	printf("Unexpectedly back from CFRunLoopRun()!\n");
    }

    return 0;
}

static void
signalHandler(int sigraised)
{
    printf("\nInterrupted.\n");
   
    AppleRAIDDisableNotifications();
    
    _exit(0);
}
