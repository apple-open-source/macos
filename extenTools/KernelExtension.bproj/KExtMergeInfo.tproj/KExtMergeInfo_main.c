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
static const char *outputname;
static CFMutableArrayRef inputXMLList = 0;

#define v_printf(x)	do { if (verbose) printf x; } while (0)

#define cfStringGetUTFSize(str) CFStringGetMaximumSizeForEncoding \
    (CFStringGetLength(str), kCFStringEncodingUTF8)


static void
usage(void)
{
    fprintf(stderr, "Usage: %s -o outputFile PB.project CustomInfo.xml "
            "[*.kmodproj/*.xml...]\n", cmdname);
    exit(1);
}

static void init(int argc, char **argv)
{ 
    int c, i;
    CFStringRef tempStr;

    // Get the command name and strip the leading dirpath
    cmdname = strrchr(argv[0], '/');
    if (cmdname)
	cmdname++;
    else
	cmdname = argv[0];

    while ((c = getopt(argc, argv, "vo:")) != -1) {
        switch (c) {
        case 'v':
            verbose = 1;
            break;
        case 'o':
            outputname = optarg;
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 2 || !outputname)
        usage();

    inputXMLList = CFArrayCreateMutable
        (kCFAllocatorSystemDefault, 16, &kCFTypeArrayCallBacks);
    if (!inputXMLList) {
        fprintf(stderr, "%s: unable to key array\n", cmdname);
        usage();
    }

    for (i = 0; i < argc; i++) {
        // Strip PB.project from argument list.
        if ( strcmp(argv[i], "PB.project") ) {
            tempStr = CFStringCreateWithCStringNoCopy(kCFAllocatorSystemDefault,
                                                      argv[i],
                                                      kCFStringEncodingMacRoman,
                                                      kCFAllocatorNull);
            CFArrayAppendValue(inputXMLList, tempStr);
        }
    }
}

static CFMutableDictionaryRef loadPListDict(CFStringRef filename)
{
    int filedesc;
    struct stat statBuf;
    char *xmlBuf;
    CFPropertyListRef plist;
    CFDataRef xmldata;
    CFMutableDictionaryRef plistDict;
    CFStringRef errorString;

    char *cfilename = (char *) malloc(cfStringGetUTFSize(filename));
    if (!cfilename)
            return NULL;

    if (!CFStringGetCString(filename, cfilename, PATH_MAX, kCFStringEncodingUTF8) ||
	((filedesc = open(cfilename, O_RDONLY, (mode_t)0)) == -1)) {
      free(cfilename);
      return NULL;
    }

    if ((stat(cfilename, &statBuf) == -1) ||
	((xmlBuf = mmap((caddr_t)0, statBuf.st_size,
		       PROT_READ, MAP_FILE|MAP_PRIVATE,
		       filedesc, (off_t)0)) == (caddr_t)-1) ||
	(!(xmldata = CFDataCreateWithBytesNoCopy(NULL, xmlBuf, statBuf.st_size, kCFAllocatorNull)))) {
      free(cfilename);
      close(filedesc);
      return NULL;
    }

    free(cfilename);

    plist = CFPropertyListCreateFromXMLData(kCFAllocatorSystemDefault,
                                            xmldata,
                                            kCFPropertyListImmutable,
                                            &errorString);

    munmap(xmlBuf, statBuf.st_size);
    close(filedesc);

    if (plist && CFDictionaryGetTypeID() == CFGetTypeID(plist))
      plistDict = CFDictionaryCreateMutableCopy(kCFAllocatorSystemDefault,
						CFDictionaryGetCount(plist)+2,
						plist);

    return plistDict;
}

static void savePListDict(CFDictionaryRef plist, CFStringRef filename)
{
    CFDataRef xmldata = CFPropertyListCreateXMLData(kCFAllocatorSystemDefault, plist);

    if (xmldata) {
	int filedesc;
        size_t length = CFDataGetLength(xmldata);
	const void *bytes = (length) ? CFDataGetBytePtr(xmldata)
                                     : (const void *)"";
        char *cfilename = (char *) malloc(cfStringGetUTFSize(filename));
        if (!cfilename)
                return;

        if (!CFStringGetCString(filename, cfilename, PATH_MAX, kCFStringEncodingUTF8) ||
	    ((filedesc = open(cfilename, O_WRONLY|O_CREAT|O_TRUNC, (mode_t)0666)) == -1)) {
	  free(cfilename);
	  return;
	}

	write(filedesc, bytes, length); /* No place to return an error, so we don't check */

	fsync(filedesc);
	close(filedesc);
    }
}

static void copyValue(const void *val, void *context)
{
    CFMutableArrayRef target = (CFMutableArrayRef) context;
    CFArrayAppendValue(target, val);
}

static void mergePLists(CFMutableArrayRef target, CFDictionaryRef source,
                        CFStringRef singular, CFStringRef plural)
{
    CFTypeRef childPList;

    if (!target || !source)
        return;

    childPList = CFDictionaryGetValue(source, singular);
    if (childPList)
        CFArrayAppendValue(target, childPList);

    childPList = CFDictionaryGetValue(source, plural);
    if (childPList) {
        CFArrayApplyFunction(childPList,
                             CFRangeMake(0, CFArrayGetCount(childPList)),
                             copyValue, target);
    }
}

int main (int argc, const char *argv[])
{
    char buffer1[8*1024], buffer2[8*1024];
    CFIndex i, numXMLs;
    CFStringRef xmlname;
    CFMutableDictionaryRef outXML, curPList;
    CFMutableArrayRef kmods, personalities;

    CFStringRef moduleStr		= CFSTR("Module");
    CFStringRef modulesStr		= CFSTR("Modules");
    CFStringRef personalityStr		= CFSTR("Personality");
    CFStringRef personalitiesStr	= CFSTR("Personalities");

    init(argc, argv);

    xmlname = CFArrayGetValueAtIndex(inputXMLList, 0);
    numXMLs = CFArrayGetCount(inputXMLList);
    if (1 == numXMLs) {
        CFStringGetCString(xmlname, buffer1, sizeof(buffer1),
                           kCFStringEncodingMacRoman);
        snprintf(buffer2, sizeof(buffer2), "cp %s %s", buffer1, outputname);
        system(buffer2);
        exit(0);
    }

    outXML = loadPListDict(xmlname);
    if (!outXML) {
        CFStringGetCString(xmlname, buffer1, sizeof(buffer1),
                           kCFStringEncodingMacRoman);
        fprintf(stderr, "%s: unable to load property list - %s\n",
                cmdname, buffer1);
        exit(2);
    }

    kmods = CFArrayCreateMutable
        (kCFAllocatorSystemDefault, 256, &kCFTypeArrayCallBacks);
    personalities = CFArrayCreateMutable
        (kCFAllocatorSystemDefault, 256, &kCFTypeArrayCallBacks);
    if (!kmods || !personalities) {
        fprintf(stderr, "%s: unable to plist arrays\n", cmdname);
        exit(2);
    }

    for (i = 1; i < numXMLs; i++) {
        xmlname = CFArrayGetValueAtIndex(inputXMLList, i);
        curPList = loadPListDict(xmlname);
	if (!curPList) {
	    CFStringGetCString(xmlname, buffer1, sizeof(buffer1),
	                       kCFStringEncodingMacRoman);
	    fprintf(stderr, "%s: unable to load property list - %s\n",
	            cmdname, buffer1);
	    exit(2);
	}
        mergePLists(kmods, curPList, moduleStr, modulesStr);
        mergePLists(personalities, curPList, personalityStr, personalitiesStr);
    }

    i = CFArrayGetCount(personalities);
    if (1 == i) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(personalities, 0);
        CFDictionarySetValue(outXML, personalityStr, dict);
    } else if (1 < i)
        CFDictionarySetValue(outXML, personalitiesStr, personalities);

    i = CFArrayGetCount(kmods);
    if (1 == i) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(kmods, 0);
        CFDictionarySetValue(outXML, moduleStr, dict);
    } else if (1 < i)
        CFDictionarySetValue(outXML, modulesStr, kmods);

    xmlname = CFStringCreateWithCStringNoCopy(kCFAllocatorSystemDefault,
                                              outputname,
                                              kCFStringEncodingMacRoman,
                                              kCFAllocatorNull);
    savePListDict(outXML, xmlname);

    exit(0);       // insure the process exit status is 0
    return 0;      // ...and make main fit the ANSI spec.
}
