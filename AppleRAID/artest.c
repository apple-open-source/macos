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

static int autoRebuild = 0;
static char * hint = 0;
static UInt64 blockSize = 0;
static UInt64 timeout = 0;
static bool verbose = false;
static char * volName = 0;

static void
usage()
{
    printf("\n");
    printf("usage:\n");
    printf("\n");
    printf("artest --list\n\n");
    printf("\n");
    printf("artest --create --name <volname> --level <level> <options> disk1s3 disk2s3 disk3s3 ...\n");
    printf("artest --destroy <set uuid>\n\n");
    printf("artest --modify <set uuid> <options>\n");
    printf("\n");
    printf("artest --add <set uuid> disk1s3 ...\n");
    printf("artest --spare <set uuid> disk1s3 ...\n");
    printf("artest --remove <set uuid> disk1s3 ...\n");
    printf("\n");
    printf("artest --erase disk1s3 disk2s3 disk3s3 ...\n");
    printf("artest --header disk1s3 disk2s3 disk3s3 ...\n");
    printf("\n");
    printf("parameters:\n");
    printf("	<volname> = \"the volume name\"\n");
    printf("	<level> = stripe, mirror, concat\n");
    printf("	<options> = [--auto-rebuild=Yes] [--block-size=0x8000] [--hint=\"hint string\"] [--timeout=30]\n");
    printf("	<set uuid> = XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\n");
    printf("\n");
    printf("global options:\n");
    printf("	--verbose\n");
    printf("    --watch (also a command)\n");
}


// there must be something like this already?

void CFPrintf(CFStringRef format, ...)
{
    CFStringRef cfstring;
    va_list argList;

    va_start(argList, format);
    cfstring = CFStringCreateWithFormatAndArguments(NULL, NULL, format, argList);
    va_end(argList);

    CFIndex cfstringSize = CFStringGetLength(cfstring);
    CFIndex stringSize = CFStringGetMaximumSizeForEncoding(cfstringSize, kCFStringEncodingUTF8) + 1;
    char *string = malloc(stringSize);
    if (CFStringGetCString(cfstring, string, stringSize, kCFStringEncodingUTF8));

    printf("%s", string);
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
    if (!partitionNumber) return true;			// just assume it a raid disk
    
    char * optionString = "<dict> <key>Writable</key> <true/> </dict>";
    CFDictionaryRef options = IOCFUnserialize(optionString, kCFAllocatorDefault, 0, NULL);
    if (!options) exit(1);

    int32_t err;
    MKMediaRef device = MKMediaCreateWithPath(nil, wholeDevicePath, options, &err);
    CFRelease(options);
    if (!device || err) return false;
    
    optionString = "<dict> <key>Include all</key> <true/> </dict>";
    options = IOCFUnserialize(optionString, kCFAllocatorDefault, 0, NULL);
    if (!options) exit(1);
	
    MKStatus err2;
    CFMutableDictionaryRef media = MKCFReadMedia(options, device, &err2);
    CFRelease(options);
    if (!media || err2) goto Failure;

    // find and extract the 'Schemes' array 
    CFMutableArrayRef Schemes = (CFMutableArrayRef) CFDictionaryGetValue(media, CFSTR("Schemes"));
    if (!Schemes) goto Failure;

    // Search for the Apple Partition Scheme in the schemes array:
    // CFMutableDictionaryRef Scheme = (CFMutableDictionaryRef) CFArrayDictionarySearch(Schemes, CFSTR("ID"), CFSTR("APM"));
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

    err2 = MKCFWriteMedia(media, nil, nil, nil, device);

    MKCFDisposeMedia(media);
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

    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, nameCString, kCFStringEncodingUTF8);
    if (!name) exit(1);

    bool success = AppleRAIDDestroySet(name);
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
    CFPrintf(CFSTR("\tcontent hint = %@, auto rebuild = %@, timeout = %@\n"),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetContentHintKey)),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetAutoRebuildKey)),
	     CFDictionaryGetValue(set, CFSTR(kAppleRAIDSetTimeoutKey)));

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
    CFPrintf(CFSTR("\t\tstatus = %@, chunk count = %@, rebuild = %@%%\n"),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDMemberStatusKey)),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDChunkCountKey)),
	     CFDictionaryGetValue(member, CFSTR(kAppleRAIDRebuildStatus)));

    if (verbose) CFShow(member);
}

static void
listSets()
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
    if (volName) {
	CFStringRef volNameCF = CFStringCreateWithCString(kCFAllocatorDefault, volName, kCFStringEncodingUTF8);
	if (volNameCF) AppleRAIDModifySet(setInfo, CFSTR(kAppleRAIDSetNameKey), (void *)volNameCF);
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
    bool list = false, modify = false, remove = false, spare = false, watch = false;
    char * setLevel = 0, * setName = 0;

    /* options descriptor */
    static struct option longopts[] = {
	{ "auto-rebuild",required_argument,	0,		'A' },
	{ "block-size",	required_argument,	0,		'B' },
	{ "hint",	required_argument,	0,		'H' },
	{ "level",	required_argument,	0,		'L' },
	{ "name",	required_argument,	0,		'N' },
	{ "timeout",	required_argument,	0,		'T' },

	{ "add",	required_argument,	0,		'a' },
	{ "create",	no_argument,		0,		'c' },
	{ "destroy",	required_argument,	0,		'd' },
	{ "erase",	optional_argument,	0,		'e' },
	{ "header",	optional_argument,	0,		'h' },
	{ "help",	no_argument,		0,		'?' },
	{ "list",	no_argument,		0,		'l' },
	{ "modify",	required_argument,	0,		'm' },
	{ "remove",	required_argument,	0,		'r' },
	{ "spare",	required_argument,	0,		's' },
	{ "verbose",	no_argument,		0,		'v' },
	{ "watch",	no_argument,		0,		'w' },
	{ 0,		0,			0,		0 }
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "A:B:H:L:N:T:a:cd:ehlm:r:s:vw?", longopts, NULL)) != -1) {
	
	switch(ch) {
	case 'A':
	    autoRebuild = ((optarg[0] == 'Y') || (optarg[0] == 'y')) ? AUTO_YES : AUTO_NO;
	    break;
	case 'B':
	    sscanf(optarg, "%llx", &blockSize);
	    break;
	case 'H':
	    hint = strdup(optarg);
	    break;
	case 'L':
	    setLevel = optarg;
	    break;
	case 'N':
	    volName = optarg;
	    break;
	case 'T':
	    sscanf(optarg, "%llx", &timeout);
	    break;

	case 'a':
	    add = true;
	    setName = optarg;
	    break;
	case 'c':
	    create = true;
	    break;
	case 'd':
	    destroy = true;
	    setName = optarg;
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
	    setName = optarg;
	    break;
	case 'r':
	    remove = true;
	    setName = optarg;
	    break;
	case 's':
	    spare = true;
	    setName = optarg;
	    break;
	case 'v':
	    verbose = true;
	    break;
	case 'w':
	    watch = true;
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

    if (!add && !create && !destroy && !erase && !header && !list && !modify && !remove && !spare && !watch) {
	usage();
	exit(0);
    }

    if (list) {
	listSets();
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

	// this will not fail if there is no raid controller, ie, if AppleRAID class is not instantiated in the kernel

	AppleRAIDEnableNotifications();
    }




    if (add) addMember(setName, CFSTR(kAppleRAIDMembersKey), argc, argv);
    if (create) createSet(setLevel, volName, argc, argv);
    if (destroy) destroySet(setName, argc, argv);
    if (modify) modifySet(setName, argc, argv);
    if (remove) removeMember(setName, argc, argv);
    if (spare) addMember(setName, CFSTR(kAppleRAIDSparesKey), argc, argv);



    
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
