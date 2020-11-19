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
#include <sys/param.h>
#include <sys/sysctl.h>
#include <getopt.h>
#include <libproc.h>
#include <malloc/malloc.h>
#include <mach/mach_vm.h>
#include <mach/vm_statistics.h>
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

static CFStringRef
CreateStringForUUID(const CFUUIDBytes * uuidBytes)
{
    CFUUIDRef   uuid  = 0;
    CFStringRef cfstr = 0;

    if (uuidBytes) uuid = CFUUIDCreateFromUUIDBytes(NULL, *uuidBytes);
    if (uuid)
    {
        cfstr = CFUUIDCreateString(kCFAllocatorDefault, uuid);
        CFRelease(uuid);
    }
    return (cfstr);
}

#define kAddressKey CFSTR("Address")
#define kNameKey CFSTR("Name")
#define kPathKey CFSTR("Path")
#define kSegmentsKey CFSTR("Segments")
#define kSizeKey CFSTR("Size")
#define kUuidKey CFSTR("UUID")

static CFDictionaryRef
CreateSymbolOwnerSummary(CSSymbolOwnerRef owner)
{
    CFMutableDictionaryRef summaryDict = CFDictionaryCreateMutable(NULL, 0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (summaryDict == NULL) {
        return NULL;
    }
    CFMutableDictionaryRef rval = NULL;
    __block CSSegmentRef textSegment = kCSNull, textExecSegment = kCSNull;
    CFStringRef path = NULL, name = NULL, uuidString = NULL;
    /* This is released by  textSegment, textExecSegment, or
     * CSSymbolOwnerForeachSegment
     */
    CSSegmentRef segment = kCSNull;
    CSRange range;
    CFNumberRef address = NULL, size = NULL;

    uuidString = CreateStringForUUID(CSSymbolOwnerGetCFUUIDBytes(owner));
    if (!uuidString) goto end;

    CFDictionarySetValue(summaryDict, kUuidKey, uuidString);

    path = CFStringCreateWithCString(kCFAllocatorDefault, CSSymbolOwnerGetPath(owner), kCFStringEncodingUTF8);
    if (!path) goto end;
    CFDictionarySetValue(summaryDict, kPathKey, path);

    name = CFStringCreateWithCString(kCFAllocatorDefault, CSSymbolOwnerGetName(owner), kCFStringEncodingUTF8);
    if (!name) goto end;
    CFDictionarySetValue(summaryDict, kNameKey, name);

    /*
     * This assumes there is only one TEXT OR one TEXT_EXEC
     * segment inside the symbol owner
     */
    CSSymbolOwnerForeachSegment(owner, ^(CSSegmentRef segment) {
        if (strcmp(CSRegionGetName(segment), "__TEXT SEGMENT") == 0)
        {
            textSegment = segment;
            CSRetain(textSegment);
        }
        else if (strcmp(CSRegionGetName(segment), "__TEXT_EXEC SEGMENT") == 0)
        {
            textExecSegment = segment;
            CSRetain(textExecSegment);
        }
    });

    segment = !CSIsNull(textExecSegment) ? textExecSegment : textSegment;
    if (CSIsNull(segment)) goto end;

    range = CSRegionGetRange(segment);

    address = CFNumberCreate(NULL, kCFNumberLongLongType, &range.location);
    if (address == NULL) goto end;
    size = CFNumberCreate(NULL, kCFNumberLongLongType, &range.length);
    if (size == NULL) goto end;
    CFDictionarySetValue(summaryDict, kAddressKey, address);
    CFDictionarySetValue(summaryDict, kSizeKey, size);

    rval = summaryDict;

end:
    if (size) CFRelease(size);
    if (address) CFRelease(address);
    if (!CSIsNull(textExecSegment)) CSRelease(textExecSegment);
    if (!CSIsNull(textSegment)) CSRelease(textSegment);
    if (name) CFRelease(name);
    if (path) CFRelease(path);
    if (uuidString) CFRelease(uuidString);

    if (!rval) CFRelease(summaryDict);

    return rval;
}

static void
GetBacktraceSymbols(CSSymbolicatorRef symbolicator, uint64_t addr,
                const char ** pModuleName, const char ** pSymbolName, uint64_t * pOffset, CFMutableDictionaryRef binaryImages)
{
    static char           unknownKernel[38];
    static char           unknownKernelSymbol[38];
    const char          * symbolName;
    CSSymbolOwnerRef       owner;
    CSSymbolRef            symbol;
    CSRange                range;

    owner = CSSymbolicatorGetSymbolOwnerWithAddressAtTime(symbolicator, addr, kCSNow);
    if (CSIsNull(owner))
    {
        if (!unknownKernel[0])
        {
            size_t uuidSize = sizeof(unknownKernel);
            if (sysctlbyname("kern.uuid", unknownKernel, &uuidSize, NULL, 0)) snprintf(unknownKernel, sizeof(unknownKernel), "unknown kernel");
            snprintf(unknownKernelSymbol, sizeof(unknownKernelSymbol), "kernel");
        }
        *pModuleName = unknownKernel;
        *pSymbolName = unknownKernelSymbol;
        *pOffset = 0;
    }
    else
    {

        *pModuleName = CSSymbolOwnerGetName(owner);
        symbol       = CSSymbolOwnerGetSymbolWithAddress(owner, addr);
        if (!CSIsNull(symbol) && (symbolName = CSSymbolGetName(symbol)))
        {
            range        = CSSymbolGetRange(symbol);
            *pSymbolName = symbolName;
            *pOffset     = addr - range.location;
        }
        else
        {
            symbolName   = CSSymbolOwnerGetName(owner);
            *pSymbolName = symbolName;
            *pOffset     = addr - CSSymbolOwnerGetBaseAddress(owner);
        }
        if (CSSymbolicatorIsKernelSymbolicator(symbolicator))
        {
            CFStringRef uuidString = CreateStringForUUID(CSSymbolOwnerGetCFUUIDBytes(owner));
            if (!CFDictionaryContainsKey(binaryImages, uuidString))
            {
                CFDictionaryRef summary = CreateSymbolOwnerSummary(owner);
                if (summary != NULL) {
                    CFDictionarySetValue(binaryImages, uuidString, summary);
                    CFRelease(summary);
                }
            }
            CFRelease(uuidString);
        }
    }
}

#define NSITES_MAX 64

static bool matchAnySearchString(const char *str, const char *searchStrings[NSITES_MAX])
{
    uint32_t i;
    for (i = 0; searchStrings[i] != NULL && i < NSITES_MAX; i++) {
        if (NULL != strnstr(str, searchStrings[i], strlen(str))) {
            return true;
        }
    }
    return false;
}

void
ShowBinaryImage(const void *key, const void *value, void *context)
{
    char nameString[256] = {0}, uuidString[256] = {0}, pathString[256] = {0};
    CFStringRef uuid = (CFStringRef)key;
    CFStringGetCString(uuid, uuidString, sizeof(uuidString), kCFStringEncodingASCII);
    CFDictionaryRef summary = (CFDictionaryRef)value;

    CFStringRef name = CFDictionaryGetValue(summary, kNameKey);
    CFStringGetCString(name, nameString, sizeof(nameString), kCFStringEncodingASCII);
    CFStringRef path = CFDictionaryGetValue(summary, kPathKey);
    CFStringGetCString(path, pathString, sizeof(pathString), kCFStringEncodingASCII);
    CFNumberRef addressNumber = CFDictionaryGetValue(summary, kAddressKey);
    CFNumberRef sizeNumber = CFDictionaryGetValue(summary, kSizeKey);
    int64_t address, size;
    CFNumberGetValue(addressNumber, kCFNumberSInt64Type, &address);
    CFNumberGetValue(sizeNumber, kCFNumberSInt64Type, &size);

    printf("%p - %p %s <%s> %s\n", (void*)address, (void*)address + size, nameString, uuidString, pathString);
}

/*********************************************************************
*********************************************************************/
static void
ProcessBacktraces(void * output, size_t outputSize, pid_t pid, const char * searchStrings[NSITES_MAX])
{
    struct IOTrackingCallSiteInfo * sites;
    struct IOTrackingCallSiteInfo * site;
    uint32_t                        num, idx, printIdx, j, btIdx, userBT, numBTs;
    uint64_t                        offset, total;
    const char                    * fileName;
    const char                    * symbolName;
    const char                    * moduleName;
    CSSymbolicatorRef               sym[2];
    CSSourceInfoRef                 sourceInfo;
    CFMutableDictionaryRef          binaryImages;
    char                            procname[(2*MAXCOMLEN)+1];
    bool                            search, found;
    char                            line[1024];

    binaryImages = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    sym[0] = CSSymbolicatorCreateWithMachKernel();

    sites = (typeof(sites)) output;
    num   = (uint32_t)(outputSize / sizeof(sites[0]));
    total = 0;
    for (printIdx = idx = 0; idx < num; idx++)
    {
	search = (searchStrings[0] != NULL);
	found  = false;
	do
	{
	    site = &sites[idx];
	    if (!search)
	    {
		if (site->address)
		{
		    proc_name(site->addressPID, &procname[0], sizeof(procname));
		    printf("\n[0x%qx, 0x%qx] %s(%d) [%d]\n",
			site->address, site->size[0] + site->size[1],
			procname, site->addressPID, printIdx++);
		}
		else
		{
		    printf("\n0x%qx bytes (0x%qx + 0x%qx), %d call%s [%d]\n",
			site->size[0] + site->size[1],
			site->size[0], site->size[1],
			site->count, (site->count != 1) ? "s" : "", printIdx++);
		}
		total += site->size[0] + site->size[1];
	    }

	    numBTs = 1;
	    if (site->btPID)
	    {
		if (proc_name(site->btPID, &procname[0], sizeof(procname)) > 0) {
		    sym[1] = CSSymbolicatorCreateWithPid(site->btPID);
		    numBTs = 2;
		} else {
		    snprintf(procname, sizeof(procname), "exited process");
		}
	    }

	    for (userBT = 0, btIdx = 0; userBT < numBTs; userBT++)
	    {
		for (j = 0; j < kIOTrackingCallSiteBTs; j++, btIdx++)
		{
		    mach_vm_address_t addr = site->bt[userBT][j];

		    if (!addr) break;
                    GetBacktraceSymbols(sym[userBT], addr, &moduleName, &symbolName, &offset, binaryImages);
		    snprintf(line, sizeof(line), "%2d %-36s    %#018llx ", btIdx, moduleName, addr);
		    if (!search) printf("%s", line);
		    else
		    {
			found = matchAnySearchString(line, searchStrings);
		        if (found) break;
		    }
		    if (offset) snprintf(line, sizeof(line), "%s + 0x%llx", symbolName, offset);
		    else        snprintf(line, sizeof(line), "%s", symbolName);
		    if (!search) printf("%s", line);
		    else
		    {
			found = matchAnySearchString(line, searchStrings);
			if (found) break;
		    }

		    sourceInfo = CSSymbolicatorGetSourceInfoWithAddressAtTime(sym[userBT], addr, kCSNow);
		    fileName = CSSourceInfoGetPath(sourceInfo);
		    if (fileName)
		    {
			snprintf(line, sizeof(line), " (%s:%d)", fileName, CSSourceInfoGetLineNumber(sourceInfo));
			if (!search) printf("%s", line);
			else
			{
			    found = matchAnySearchString(line, searchStrings);
			    if (found) break;
			}
		    }

		    if (!search) printf("\n");
		}
		if (found) break;
	    }
	    if (site->btPID && !search) printf("<%s(%d)>\n", procname, site->btPID);
	    if (numBTs == 2) CSRelease(sym[1]);
	    if (!found || !search) break;
	    search = false;
	}
	while (true);
    }
    printf("\n0x%qx bytes total\n", total);

    if (CFDictionaryGetCount(binaryImages) > 0)
    {
        printf("\nBinary Images:\n");
        CFDictionaryApplyFunction(binaryImages, ShowBinaryImage, NULL);
    }
    else
    {
        printf("No binary images\n");
    }
    CFRelease(binaryImages);
    CSRelease(sym[0]);
}


/*********************************************************************
 *********************************************************************/
void PrintLogHeader(int argc, char ** argv)
{
    int nc = 0;
    nc += printf("kernel memory allocations ('");

    for (int i = 0; i < argc; i++) {
        nc += printf("%s", argv[i]);
        if (i != argc - 1) nc += printf(" ");
    }
    nc += printf("')");
    printf("\n");
    while (nc--) {
        printf("-");
    }
    printf("\n");
}

/*********************************************************************
*********************************************************************/
void usage(void)
{
    printf("usage: ioclasscount [--track] [--leaks] [--reset] [--start] [--stop]\n");
    printf("                    [--exclude] [--maps=PID] [--size=BYTES] [--capsize=BYTES]\n");
    printf("                    [--tag=vmtag] [--zsize=BYTES]\n");
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
    pid_t                  pid;
    size_t                 size;
    uint32_t               tag;
    uint32_t               zsize;
    size_t                 len;
    const char           * sites[NSITES_MAX] = {NULL};
    size_t                 nsites      = 0;

    command   = kIOTrackingInvalid;
    exclude   = false;
    size      = 0;
    tag       = 0;
    zsize     = 0;
    pid       = 0;
    
    /*static*/ struct option longopts[] = {
	{ "track",   no_argument,       &command,  kIOTrackingGetTracking },
	{ "reset",   no_argument,       &command,  kIOTrackingResetTracking },
	{ "start",   no_argument,       &command,  kIOTrackingStartCapture },
	{ "stop",    no_argument,       &command,  kIOTrackingStopCapture },
        { "leaks",   no_argument,       &command,  kIOTrackingLeaks },
	{ "exclude", no_argument,       &exclude,  true },
	{ "site",    required_argument, NULL,      't' },
	{ "maps",    required_argument, NULL,      'm' },
	{ "size",    required_argument, NULL,      's' },
	{ "tag",     required_argument, NULL,      'g' },
	{ "zsize",   required_argument, NULL,      'z' },
	{ "capsize", required_argument, NULL,      'c' },
	{ NULL,      0,                 NULL,      0 }
    };

    while (-1 != (c = getopt_long(argc, argv, "", longopts, NULL)))
    {
	if (!c) continue;
	switch (c)
	{
	    case 's': size      = strtol(optarg, NULL, 0); break;
	    case 'g': tag       = atoi(optarg);            break;
	    case 'z': zsize     = atoi(optarg);            break;
	    case 't': if (nsites < NSITES_MAX) { sites[nsites++] = optarg; } break;
	    case 'c': size      = strtol(optarg, NULL, 0); command = kIOTrackingSetMinCaptureSize; break;
	    case 'm': pid       = (pid_t) strtol(optarg, NULL, 0); command = kIOTrackingGetMappings; break;
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
	if ((kIOTrackingGetTracking == command)
	 || (kIOTrackingLeaks       == command)
	 || (kIOTrackingGetMappings == command)) outputSize = kIOConnectMethodVarOutputSize;

	params->size  = size;
	params->value = pid;
	params->tag   = tag;
	params->zsize = zsize;
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

	if (outputSize)
        {
            PrintLogHeader(argc, argv);
            ProcessBacktraces(output, outputSize, pid, sites);
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

