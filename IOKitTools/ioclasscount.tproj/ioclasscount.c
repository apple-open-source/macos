/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
 */
/*
cc ioclasscount.c -o /tmp/ioclasscount -Wall -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.Internal.sdk  -framework IOKit -framework CoreFoundation -framework CoreSymbolication -F/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.Internal.sdk/System/Library/PrivateFrameworks -g
 */

#include <sysexits.h>
#include <getopt.h>
#include <malloc/malloc.h>
#include <mach/mach_vm.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Kernel/IOKit/IOKitDebug.h>
#include <Kernel/libkern/OSKextLibPrivate.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <CoreSymbolication/CoreSymbolication.h>

/*********************************************************************
*********************************************************************/
static int compareClassNames(const void * left, const void * right)
{
    switch (CFStringCompare(*((CFStringRef *)left), *((CFStringRef *)right), 
        (CFStringCompareFlags)kCFCompareCaseInsensitive)) {
    case kCFCompareLessThan:
        return -1;
        break;
    case kCFCompareEqualTo:
        return 0;
        break;
    case kCFCompareGreaterThan:
        return 1;
        break;
    default:
        fprintf(stderr, "fatal error\n");
        exit(EX_OSERR);
        return 0;
        break;
    }
}

/*********************************************************************
*********************************************************************/
static Boolean printInstanceCount(
    CFDictionaryRef dict,
    CFStringRef     name,
    char         ** nameCString,
    Boolean         addNewlineFlag)
{
    int           result     = FALSE;
    CFIndex       nameLength = 0;
    static char * nameBuffer = NULL;  // free if nameCString is NULL
    CFNumberRef   num        = NULL;  // do not release
    SInt32	      num32      = 0;
    Boolean       gotName    = FALSE;
    Boolean       gotNum     = FALSE;

    nameLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name),
        kCFStringEncodingUTF8);
    if (!nameCString || !*nameCString) {
        nameBuffer = (char *)malloc((1 + nameLength) * sizeof(char));
    } else if ((1 + nameLength) > malloc_size(nameBuffer)) {
        nameBuffer = (char *)realloc(*nameCString,
            (1 + nameLength) * sizeof(char));
    }
    if (nameBuffer) {
        gotName = CFStringGetCString(name, nameBuffer, 1 + nameLength,
            kCFStringEncodingUTF8);
    } else {
        fprintf(stderr, "Memory allocation failure.\n");
        goto finish;
    }
    
   /* Note that errors displaying the name and value are not considered
    * fatal and do not affect the exit status of the program.
    */
    printf("%s = ", gotName ? nameBuffer : "??");

    num = (CFNumberRef)CFDictionaryGetValue(dict, name);
    if (num) {

        if (CFNumberGetTypeID() == CFGetTypeID(num)) {
            gotNum = CFNumberGetValue(num, kCFNumberSInt32Type, &num32);
        }
        if (gotNum) {
            printf("%lu", (unsigned long)num32);
        } else {
            printf("?? (error reading/converting value)");
        }
    } else {
        printf("<no such class>");
    }
    
    if (addNewlineFlag) {
        printf("\n");
    } else {
        printf(", ");
    }

    result = TRUE;

finish:
    if (nameCString) {
        *nameCString = nameBuffer;
    } else {
        if (nameBuffer) free(nameBuffer);
    }
    return result;
}

/*********************************************************************
*********************************************************************/
static void
ProcessBacktraces(void * output, size_t outputSize)
{
    struct IOTrackingCallSiteInfo * sites;
    struct IOTrackingCallSiteInfo * site;
    uint32_t                        num, idx, btIdx;
    const char                    * fileName;
    CSSymbolicatorRef               sym;
    CSSymbolRef                     symbol;
    const char                    * symbolName;
    CSSymbolOwnerRef                owner;
    CSSourceInfoRef                 sourceInfo;
    CSRange                         range;

    sym = CSSymbolicatorCreateWithMachKernel();

    sites = (typeof(sites)) output;
    num   = (uint32_t)(outputSize / sizeof(sites[0]));

    for (idx = 0; idx < num; idx++)
    {
	site = &sites[idx];
	printf("\n0x%lx bytes (0x%lx + 0x%lx), %d call%s, [%d]\n",
	    site->size[0] + site->size[1], 
	    site->size[0], site->size[1], 
	    site->count, (site->count != 1) ? "s" : "", idx);
	uintptr_t * bt = &site->bt[0];
	for (btIdx = 0; btIdx < kIOTrackingCallSiteBTs; btIdx++)
	{
	    mach_vm_address_t addr = bt[btIdx];

	    if (!addr) break;
	
	    symbol = CSSymbolicatorGetSymbolWithAddressAtTime(sym, addr, kCSNow);
	    owner  = CSSymbolGetSymbolOwner(symbol);

	    printf("%2d %-24s      0x%llx ", btIdx, CSSymbolOwnerGetName(owner), addr);

	    symbolName = CSSymbolGetName(symbol);
	    if (symbolName)
	    {
		range = CSSymbolGetRange(symbol);
		printf("%s + 0x%llx", symbolName, addr - range.location);
	    }
	    else {}

	    sourceInfo = CSSymbolicatorGetSourceInfoWithAddressAtTime(sym, addr, kCSNow);
	    fileName = CSSourceInfoGetPath(sourceInfo);
	    if (fileName) printf(" (%s:%d)", fileName, CSSourceInfoGetLineNumber(sourceInfo));

	    printf("\n");
	}
    }
}

/*********************************************************************
*********************************************************************/
void usage(void)
{
    printf("usage: ioclasscount [--track] [--leaks] [--reset] [--start] [--stop]\n");
    printf("                    [--exclude] [--print] [--size=BYTES] [--capsize=BYTES]\n");
    printf("                    [classname] [...] \n");
}

/*********************************************************************
*********************************************************************/
int main(int argc, char ** argv)
{
    int                    result      = EX_OSERR;
    kern_return_t          status      = KERN_FAILURE;
    io_registry_entry_t    root        = IO_OBJECT_NULL;  // must IOObjectRelease()
    CFMutableDictionaryRef rootProps   = NULL;            // must release
    CFDictionaryRef        diagnostics = NULL;            // do not release
    CFDictionaryRef        classes     = NULL;            // do not release
    CFStringRef          * classNames  = NULL;            // must free
    CFStringRef            className   = NULL;            // must release
    char                 * nameCString = NULL;            // must free

    int                    c;
    int                    command;
    int                    exclude;
    size_t                 size;
    size_t                 len;

    command = kIOTrackingInvalid;
    exclude = false;
    size    = 0;
    
    /*static*/ struct option longopts[] = {
	{ "track",   no_argument,       &command,  kIOTrackingGetTracking },
	{ "reset",   no_argument,       &command,  kIOTrackingResetTracking },
	{ "start",   no_argument,       &command,  kIOTrackingStartCapture },
	{ "stop",    no_argument,       &command,  kIOTrackingStopCapture },
        { "print",   no_argument,       &command,  kIOTrackingPrintTracking },
        { "leaks",   no_argument,       &command,  kIOTrackingLeaks },
	{ "exclude", no_argument,       &exclude,  true },
	{ "size",    required_argument, NULL,      's' },
	{ "capsize", required_argument, NULL,      'c' },
	{ NULL,      0,                 NULL,      0 }
    };

    while (-1 != (c = getopt_long(argc, argv, "", longopts, NULL)))
    {
	if (!c) continue;
	switch (c)
	{
	    case 's': size = strtol(optarg, NULL, 0); break;
	    case 'c': size = strtol(optarg, NULL, 0); command = kIOTrackingSetMinCaptureSize; break;
	    default:
		usage();
		exit(1);
	}
    }

    if (kIOTrackingInvalid != command)
    {
	IOKitDiagnosticsParameters * params;
	io_connect_t connect;
	IOReturn     err;
	uint32_t     idx;
        const char * name;    
	char       * next;
	void       * output;
	size_t       outputSize;

	len = 1 + sizeof(IOKitDiagnosticsParameters);
	for (idx = optind; idx < argc; idx++) len += 1 + strlen(argv[idx]);

	params = (typeof(params)) malloc(len);
	bzero(params, len);
	next = (typeof(next))(params + 1);
	if (optind < argc)
	{
	    for (idx = optind; idx < argc; idx++)
	    {
		name = argv[idx];
		len = strlen(name);
		next[0] = len;
		next++;
		strncpy(next, name, len);
		next += len;
	    }
	    next[0] = 0;
	    next++;
	}

	root = IORegistryEntryFromPath(kIOMasterPortDefault, kIOServicePlane ":/");
	err = IOServiceOpen(root, mach_task_self(), kIOKitDiagnosticsClientType, &connect);
	IOObjectRelease(root);

	if (kIOReturnSuccess != err)
	{
	    fprintf(stderr, "open %s (0x%x), need DEVELOPMENT kernel\n", mach_error_string(err), err);
	    exit(1);
	}

	output = NULL;
	outputSize = 0;
	if ((kIOTrackingGetTracking == command) || (kIOTrackingLeaks == command)) outputSize = kIOConnectMethodVarOutputSize;

	params->size = size;
	if (exclude) params->options |= kIOTrackingExcludeNames;

	err = IOConnectCallMethod(connect, command,
				  NULL, 0,
				  params, (next - (char * )params),
				  NULL, 0,
				  &output,
				  &outputSize);
	if (kIOReturnSuccess != err)
	{
	    fprintf(stderr, "method 0x%x %s (0x%x), check boot-arg io=0x00400000\n", command, mach_error_string(err), err);
	    exit(1);
	}

	if ((kIOTrackingGetTracking == command) || (kIOTrackingLeaks == command))
        {
            ProcessBacktraces(output, outputSize);
        }

	free(params);
	err = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t) output, outputSize);
	if (KERN_SUCCESS != err)
	{
	    fprintf(stderr, "mach_vm_deallocate %s (0x%x)\n", mach_error_string(err), err);
	    exit(1);
	}
	exit(0);
    }

    // Obtain the registry root entry.
    root = IORegistryGetRootEntry(kIOMasterPortDefault);
    if (!root) {
        fprintf(stderr, "Error: Can't get registry root.\n");
        goto finish;
    }
    
    status = IORegistryEntryCreateCFProperties(root,
        &rootProps, kCFAllocatorDefault, kNilOptions);
    if (KERN_SUCCESS != status) {
        fprintf(stderr, "Error: Can't read registry root properties.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(rootProps)) {
        fprintf(stderr, "Error: Registry root properties not a dictionary.\n");
        goto finish;
    }
    
    diagnostics = (CFDictionaryRef)CFDictionaryGetValue(rootProps,
        CFSTR(kIOKitDiagnosticsKey));
    if (!diagnostics) {
        fprintf(stderr, "Error: Allocation information missing.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(diagnostics)) {
        fprintf(stderr, "Error: Allocation information not a dictionary.\n");
        goto finish;
    }

    classes = (CFDictionaryRef)CFDictionaryGetValue(diagnostics, CFSTR("Classes"));
    if (!classes) {
        fprintf(stderr, "Error: Class information missing.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(classes)) {
        fprintf(stderr, "Error: Class information not a dictionary.\n");
        goto finish;
    }
    
    if (optind >= argc) {
        CFIndex       index, count;
        
        count = CFDictionaryGetCount(classes);
        classNames = (CFStringRef *)calloc(count, sizeof(CFStringRef));
        if (!classNames) {
            fprintf(stderr, "Memory allocation failure.\n");
            goto finish;
        }
        CFDictionaryGetKeysAndValues(classes, (const void **)classNames, NULL);
        qsort(classNames, count, sizeof(CFStringRef), &compareClassNames);
        
        for (index = 0; index < count; index++) {
            printInstanceCount(classes, classNames[index], &nameCString,
                /* addNewline? */ TRUE);
        }
        
    } else {
        uint32_t index = 0;
        for (index = optind; index < argc; index++ ) {

            if (className) CFRelease(className);
            className = NULL;

            className = CFStringCreateWithCString(kCFAllocatorDefault,
                argv[index], kCFStringEncodingUTF8);
            if (!className) {
                fprintf(stderr, "Error: Can't create CFString for '%s'.\n",
                    argv[index]);
                goto finish;
            }
            printInstanceCount(classes, className, &nameCString,
                /* addNewline? */ (index + 1 == argc));
        }
    }
    
    result = EX_OK;
finish:
    if (rootProps)              CFRelease(rootProps);
    if (root != IO_OBJECT_NULL) IOObjectRelease(root);
    if (classNames)             free(classNames);
    if (className)              CFRelease(className);
    if (nameCString)            free(nameCString);

    return result;
}

