/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#include <sys/cdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <CoreFoundation/CoreFoundation.h>

static const char *cmdname;
static int verbose = 0;
static CFPropertyListRef inputXMLplist = 0;
static CFMutableArrayRef inputKeys = 0;

#define v_printf(x)	do { if (verbose) printf x; } while (0)

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-v] filename keyref...\n", cmdname);
    fprintf(stderr, "Where keyref is a / seperated list of keys\n");
    exit(1);
}

static void init(int argc, char **argv)
{
    int c, i;
    CFStringRef tempStr;
    int filedesc;
    struct stat statBuf;
    char *xmlBuf = NULL;
    CFDataRef fileXMLData = NULL;

    // Get the command name and strip the leading dirpath
    cmdname = strrchr(argv[0], '/');
    if (cmdname)
	cmdname++;
    else
	cmdname = argv[0];

    while ((c = getopt(argc, argv, "v")) != -1) {
        switch (c) {
        case 'v':
            verbose = 1;
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 2)
        usage();

    if ((filedesc = open(argv[0], O_RDONLY, (mode_t)0)) == -1) {
        fprintf(stderr, "%s: unable to open file %s\n", cmdname, argv[0]);
        usage();
    }

    if ((stat(argv[0], &statBuf) == -1) ||
	((xmlBuf = mmap((caddr_t)0, statBuf.st_size,
			PROT_READ, MAP_FILE|MAP_PRIVATE,
			filedesc, (off_t)0)) == (caddr_t)-1) ||
	(!(fileXMLData = CFDataCreateWithBytesNoCopy(NULL, xmlBuf, statBuf.st_size, kCFAllocatorNull)))) {
      close(filedesc);
      fprintf(stderr, "%s: unable to read file %s\n", cmdname, argv[0]);
      usage();
    }

    inputXMLplist = CFPropertyListCreateFromXMLData(kCFAllocatorSystemDefault,
                                                    fileXMLData,
                                                    kCFPropertyListImmutable,
                                                    &tempStr);
    
    munmap(xmlBuf, statBuf.st_size);
    close(filedesc);

    inputKeys = CFArrayCreateMutable(kCFAllocatorSystemDefault,
                                     16,
                                     &kCFTypeArrayCallBacks);
    if (!inputKeys) {
        fprintf(stderr, "%s: unable to key array\n", cmdname);
        usage();
    }

    for (i = 1; i < argc; i++) {
        tempStr = CFStringCreateWithCStringNoCopy(kCFAllocatorSystemDefault,
                                                  argv[i],
                                                  kCFStringEncodingMacRoman,
                                                  kCFAllocatorNull);
        CFArrayAppendValue(inputKeys, tempStr);
    }
}

int main (int argc, const char *argv[])
{
    CFIndex keyInd, lastKey;
    char buffer[16*1024];

    init(argc, argv);

    lastKey = CFArrayGetCount(inputKeys);
    if ( !lastKey ) {
        fprintf(stderr,
                "%s: No key requested\n", cmdname);
        usage();
    }

    if (!inputXMLplist
    ||   CFGetTypeID(inputXMLplist) != CFDictionaryGetTypeID()) {
        fprintf(stderr, "%s: Couldn't interpret input property list\n",
                cmdname);
        usage();
    }

    for (keyInd = 0; keyInd < lastKey; keyInd++) {
        CFArrayRef keySpec;
        CFStringRef keyString, curKey;
        CFTypeRef curObj;
        CFIndex i, numKeys;

        keyString = CFArrayGetValueAtIndex(inputKeys, keyInd);
        keySpec = CFStringCreateArrayBySeparatingStrings(
                            kCFAllocatorSystemDefault, keyString, CFSTR("/"));
        numKeys = CFArrayGetCount(keySpec);
        if (!keySpec || numKeys < 1) {
            CFStringGetCString(keyString, buffer, sizeof(buffer),
                               kCFStringEncodingMacRoman);
            fprintf(stderr, "%s: Can't interpret key - %s\n", cmdname, buffer);
            continue;
        }

        curObj = inputXMLplist;
        for (i = 0; i < numKeys; i++) {
            CFArrayRef arrayCheck;
            CFIndex arrayInd;
    
            curKey = CFArrayGetValueAtIndex(keySpec, i);
            arrayCheck = CFStringCreateArrayBySeparatingStrings(
                            kCFAllocatorSystemDefault, curKey, CFSTR("["));
            if (CFArrayGetCount(arrayCheck) <= 1)
                curObj = CFDictionaryGetValue(curObj, curKey);
            else {
                curObj = CFDictionaryGetValue(
                                curObj, CFArrayGetValueAtIndex(arrayCheck, 0));
                if (!curObj || CFArrayGetTypeID() != CFGetTypeID(curObj)) {
                    CFStringGetCString(curKey, buffer, sizeof(buffer),
                                    kCFStringEncodingMacRoman);
                    fprintf(stderr,
                            "%s: key %s is not an array\n", cmdname, buffer);
                    break;
                }
                arrayInd = CFStringGetIntValue(
                                    CFArrayGetValueAtIndex(arrayCheck, 1));
                if (arrayInd >= CFArrayGetCount(curObj)) {
                    CFStringGetCString(curKey, buffer, sizeof(buffer),
                                    kCFStringEncodingMacRoman);
                    fprintf(stderr,
                            "%s: key %s is out of bounds\n", cmdname, buffer);
                    break;
                }
                curObj = CFArrayGetValueAtIndex(curObj, arrayInd);
            }
            if (!curObj
            || (i < numKeys - 1
              && CFDictionaryGetTypeID() != CFGetTypeID(curObj)) ) {
                CFStringGetCString(curKey, buffer, sizeof(buffer),
                                   kCFStringEncodingMacRoman);
                fprintf(stderr, "%s: Can't find key - %s\n", cmdname, buffer);
                break;
            }
            if (i == numKeys - 1) {
                if (CFGetTypeID(curObj) != CFStringGetTypeID()) {
                    CFStringGetCString(curKey, buffer, sizeof(buffer),
                                    kCFStringEncodingMacRoman);
                    fprintf(stderr, "%s: Can't find string - %s\n",
                            cmdname, buffer);
                    break;
                }

                CFStringGetCString(curObj, buffer, sizeof(buffer),
                                   kCFStringEncodingMacRoman);
                puts(buffer);
            }
        }
    }

    exit(0);       // insure the process exit status is 0
    return 0;      // ...and make main fit the ANSI spec.
}
