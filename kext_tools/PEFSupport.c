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
/*
    File:    UnpackPiData.c
    Written by:    Jeffrey Robbin
    Copyright:    © 1994, 1996 by Apple Computer, Inc., all rights reserved.

    File:    GetSymbolFromPEF.c
    Written by:    Jeffrey Robbin
    Copyright:    © 1994, 1996 by Apple Computer, Inc., all rights reserved.
*/

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <stdlib.h>
#include <err.h>
#include <sys/file.h>
#include <nlist.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <paths.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>

#include "GetSymbolFromPEF.h"

/*******************************************************************************
*
*******************************************************************************/
static unsigned char PEFGetNextByte(
    unsigned char ** rawBuffer,
    long * rawBufferRemaining)
{
    *rawBufferRemaining = *rawBufferRemaining - 1;
    return *(*rawBuffer)++;
}


/*******************************************************************************
*
*******************************************************************************/
static unsigned long PEFGetCount(
    unsigned char ** rawBuffer,
    long * rawBufferRemaining)
{
    register unsigned char b;
    register unsigned long value = 0UL;

    /* Scan the count value. All required bytes MUST be present... */

    b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
    if (!IS_LAST_PICNT_BYTE(b)) {           // if 1st byte is not that last...
        value = CONCAT_PICNT(value, b);     // ...init value using 1st byte

        b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
        if (!IS_LAST_PICNT_BYTE(b)) {       // if 2nd byte is not the last...
            value = CONCAT_PICNT(value, b); // ...add in 2nd byte

            b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
            if (!IS_LAST_PICNT_BYTE(b)) {       // if 3rd byte is not the last...
                value = CONCAT_PICNT(value, b); // ...add in 3rd byte

                b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
                if (!IS_LAST_PICNT_BYTE(b)) {       // if 4th is not the last...
                    value = CONCAT_PICNT(value, b); // ...add in 4th byte

                    // 5th byte is definitly last!
                    b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
                }
            }
        }
    }

    value = CONCAT_PICNT(value, b); /* add in "last" byte (whichever one) */

    return value;
}



/*******************************************************************************
* UnpackPiData expands a compressed section into memory.
*******************************************************************************/
static OSErr UnpackPiData(
    LogicalAddress   thePEFPtr,
    SectionHeaderPtr sectionHeaderPtr,
    LogicalAddress * theData)
{
    long             cntX, cnt, rpt, dcnt, delta;
    unsigned char    op, b;
    unsigned char *  unpackBuffer;
    unsigned char *  originalUnpackBuffer;
    unsigned char *  endUnpackBuffer;
    unsigned char *  oldRawBuffer;
    long             oldRawBufferRemaining;
    unsigned char *  rawBuffer;
    long             rawBufferRemaining;
    
    // Verify incoming section is packed.
    if (sectionHeaderPtr->regionKind != kPIDataSection) {
        return paramErr;
    }
    
    // Allocate memory to unpack into
    originalUnpackBuffer = (unsigned char*)NewPtrSys(sectionHeaderPtr->initSize);
    if (originalUnpackBuffer == nil) {
        return memFullErr;
    }

    unpackBuffer = originalUnpackBuffer;
    endUnpackBuffer = unpackBuffer + sectionHeaderPtr->initSize;
    rawBuffer = (unsigned char*)((unsigned long)thePEFPtr +
        sectionHeaderPtr->containerOffset);
    rawBufferRemaining = sectionHeaderPtr->rawSize;


   /* Expand the pidata instructions.  EOF will terminate processing
    * through the setjmp on pidData_jmpbuf above...
    */
    while (rawBufferRemaining > 0) {

       /*****
        * The first byte of each instruction contains the opcode and a count. 
        * If the countis 0, the count starts in the next byte...
        */

       /* Pick up the opcode and first count operand...
        */
        b = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);

        op  = PIOP(b);
        cnt = PICNT(b);

        if (cnt == 0) {
            cnt = PEFGetCount(&rawBuffer, &rawBufferRemaining);
        }

       /* Unpack the data as a function of the opcode...
        */
        switch (op) {
          case kZero:  // zero out cnt bytes...
            if (unpackBuffer + cnt > endUnpackBuffer) {
                goto Error;
            }
            memset(unpackBuffer, 0, cnt);
            unpackBuffer += cnt;
            break;

          case kBlock: // copy cnt bytes...
            if (unpackBuffer + cnt > endUnpackBuffer) {
                goto Error;
            }
            while (--cnt >= 0) {
                *unpackBuffer++ = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
            }
            break;

          case kRepeat: // copy cnt bytes rpt times...
            rpt = PEFGetCount(&rawBuffer, &rawBufferRemaining) + 1;

            if (cnt == 1) {
                if (unpackBuffer + rpt > endUnpackBuffer) {
                    goto Error;
                }
                b = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
                memset(unpackBuffer, b, rpt);
                unpackBuffer += rpt;
            } else {
                oldRawBufferRemaining    = rawBufferRemaining;
                oldRawBuffer            = rawBuffer;
                while (--rpt >= 0) {
                    if (unpackBuffer + cnt > endUnpackBuffer) {
                        goto Error;
                    }
                    rawBufferRemaining    = oldRawBufferRemaining;
                    rawBuffer            = oldRawBuffer;
                    cntX = cnt;
                    while (--cntX >= 0) {
                        *unpackBuffer++ = PEFGetNextByte(&rawBuffer,
                            &rawBufferRemaining);
                    }
                }
            }
            break;

          case kRepeatZero: //copy cnt 0's and dcnt bytes rpt times
            dcnt = PEFGetCount(&rawBuffer, &rawBufferRemaining); // ...then copy cnt more 0's
            rpt = PEFGetCount(&rawBuffer, &rawBufferRemaining);

            goto rptPart1; // jump into loop to copy 0's first...

            while (--rpt >= 0) {
                if (unpackBuffer + dcnt > endUnpackBuffer) {
                    goto Error;
                }
                cntX = dcnt; // cnt repeating parts follow each other
                while (--cntX >= 0) {
                    *unpackBuffer++ = PEFGetNextByte(&rawBuffer,
                        &rawBufferRemaining);
                }

rptPart1: // non-repeating part is always 0's...
                if (unpackBuffer + cnt > endUnpackBuffer) {
                    goto Error;
                }
                memset(unpackBuffer, 0, cnt);
                unpackBuffer += cnt;
            }
            break;

          case kRepeatBlock: // copy cnt repeating bytes and dcnt
            dcnt = PEFGetCount(&rawBuffer, &rawBufferRemaining);                    /* non-repating bytes rcnt times...            */
            rpt = PEFGetCount(&rawBuffer, &rawBufferRemaining);                    /* ...then copy cnt repeating bytes            */

            oldRawBufferRemaining    = rawBufferRemaining;
            oldRawBuffer            = rawBuffer;
            delta                    = 0;  /*  the repeating part and each non-rep    */

            goto rptPart2;  /* jump into loop to copy rptng part 1st */

            while (--rpt >= 0) {
                if (unpackBuffer + dcnt > endUnpackBuffer) {
                    goto Error;
                }

                rawBuffer            = oldRawBuffer + cnt + delta;
                rawBufferRemaining    = oldRawBufferRemaining - (cnt + delta);
                cntX = dcnt;
                while (--cntX >= 0) {
                    *unpackBuffer++ = PEFGetNextByte(&rawBuffer,
                        &rawBufferRemaining);
                }
                delta += dcnt;

rptPart2:
                if (unpackBuffer + cnt > endUnpackBuffer) {
                    goto Error;
                }
                rawBuffer            = oldRawBuffer;
                rawBufferRemaining    = oldRawBufferRemaining;
                cntX = cnt;
                while (--cntX >= 0) {
                    *unpackBuffer++ = PEFGetNextByte(&rawBuffer,
                        &rawBufferRemaining);
                }
            }

            rawBuffer            = oldRawBuffer + cnt + delta;
            rawBufferRemaining    = oldRawBufferRemaining - (cnt + delta);
            break;

            default:
              goto Error;
              break;
        } /* switch */
    } /* for */
    
    *theData = originalUnpackBuffer;
        
    return noErr;

Error:
    if (unpackBuffer)
        DisposePtr((Ptr)originalUnpackBuffer);
    
    *theData = nil;

    return paramErr;
}


/*******************************************************************************
* GetSymbolFromPEF will extract from a PEF container the data associated
* with a given symbol name.  It requires that the PEF file have been previously
* loaded into memory.
*******************************************************************************/
static OSStatus GetSymbolFromPEF(
    StringPtr theSymbolName,
    const LogicalAddress thePEFPtr,
    LogicalAddress theSymbolPtr,
    ByteCount theSymbolSize)
{
    ContainerHeaderPtr  containerHeaderPtr;  // Pointer to the Container Header
    SectionHeaderPtr    loaderSectionPtr = 0; // Ptr to Loader Section Header
    SectionHeaderPtr    exportSectionPtr;    // Ptr to Section Header with symbol
    short               currentSection;
    Boolean             foundSection;
    Boolean             foundSymbol;
    long                numExportSymbols;
    LoaderHeaderPtr     loaderHeaderPtr;
    ExportSymbolEntryPtr       exportSymbolEntryPtr;
    LoaderExportChainEntryPtr  exportChainEntryPtr;
    StringPtr           exportSymbolName;
    LogicalAddress      expandedDataPtr;
    unsigned char *     sourceDataPtr;
    unsigned char *     destDataPtr;

    containerHeaderPtr = (ContainerHeaderPtr)thePEFPtr;
    
    // Does the magic cookie match?
    if (containerHeaderPtr->magicCookie != 'Joy!') {
        return cfragFragmentFormatErr;
    }
    // Is this a known PEF container format?
    if (containerHeaderPtr->containerID != 'peff') {
        return cfragFragmentFormatErr;
    }

    // Validate parameters
    if (theSymbolPtr == nil) {
        return paramErr;
    }
    
    // Find the loader section.
    foundSection = false;
    for (currentSection = 0;
         currentSection < containerHeaderPtr->nbrOfSections;
         currentSection++) {

        loaderSectionPtr = (SectionHeaderPtr)((unsigned long)containerHeaderPtr +
            sizeof(ContainerHeader) +
            (sizeof(SectionHeader) * currentSection));

        if (loaderSectionPtr->regionKind == kLoaderSection) {
            foundSection = true;
            break;
        }
    }

    if (foundSection == false) {
        return cfragNoSectionErr;
    }

    // Get the number of export symbols.
    loaderHeaderPtr = (LoaderHeaderPtr)((unsigned long)thePEFPtr +
        loaderSectionPtr->containerOffset);
    numExportSymbols = loaderHeaderPtr->nbrExportSyms;
    
    // Start at the first exported symbol.
    exportSymbolEntryPtr = (ExportSymbolEntryPtr)((unsigned long)loaderHeaderPtr +
        loaderHeaderPtr->slotTblOffset +
        (sizeof(LoaderHashSlotEntry) * (1<<loaderHeaderPtr->hashSlotTblSz)) +
        (sizeof(LoaderExportChainEntry) * numExportSymbols));

    exportChainEntryPtr = (LoaderExportChainEntryPtr)
        ((unsigned long)loaderHeaderPtr +
        loaderHeaderPtr->slotTblOffset +
        (sizeof(LoaderHashSlotEntry) * (1<<loaderHeaderPtr->hashSlotTblSz)));

    foundSymbol = false;
    while (numExportSymbols-- > 0) {
        exportSymbolName = (StringPtr)((unsigned long)loaderHeaderPtr +
             loaderHeaderPtr->strTblOffset +
             (exportSymbolEntryPtr->class_and_name & 0x00FFFFFF));

        if (SymbolCompare(theSymbolName, exportSymbolName,
                exportChainEntryPtr->_h._h_h._nameLength)) {

            foundSymbol = true;
            break;
        }
        exportSymbolEntryPtr = (ExportSymbolEntryPtr)
            (((int)exportSymbolEntryPtr) + 10);
        exportChainEntryPtr++;
    }

    if (foundSymbol == false) {
        return cfragNoSymbolErr;
    }
    
    // Found the symbol, so... let's go get the data!
    exportSectionPtr = (SectionHeaderPtr)((unsigned long)containerHeaderPtr +
        sizeof(ContainerHeader) +
        (sizeof(SectionHeader) * exportSymbolEntryPtr->sectionNumber));

    expandedDataPtr = nil;

    switch (exportSectionPtr -> regionKind) {
      case kPIDataSection:
        // Expand the data!  (Not yet... :)
        if (UnpackPiData(thePEFPtr, exportSectionPtr, &expandedDataPtr) != noErr) {
            return cfragFragmentCorruptErr;
        }

        sourceDataPtr = (unsigned char*)((unsigned long)expandedDataPtr +
            exportSymbolEntryPtr->address);
        break;

      default:
        sourceDataPtr = (unsigned char*)((unsigned long)thePEFPtr +
            exportSectionPtr->containerOffset +
            exportSymbolEntryPtr->address);
        break;
    }

    
    // Copy the data!
    destDataPtr = (unsigned char*)theSymbolPtr;

    while (theSymbolSize-- > 0) {
        *destDataPtr++ = *sourceDataPtr++;
    }
    
    // Cleanup any expanded data

    if (expandedDataPtr != nil) {
        DisposePtr((Ptr)expandedDataPtr);
    }

    return noErr;
}


/*******************************************************************************
*
*******************************************************************************/
static IOByteCount GetPEFLen(LogicalAddress thePEFPtr)
{
    ContainerHeaderPtr containerHeaderPtr; // Pointer to the Container Header
    SectionHeaderPtr sections;
    short currentSection;
    long  lastOffset = 0;
    long  len = 0;

    containerHeaderPtr = (ContainerHeaderPtr)thePEFPtr;
    
    // Does the magic cookie match?
    if (containerHeaderPtr->magicCookie != 'Joy!') {
        return 0;
    }
    
    // Is this a known PEF container format?
    if (containerHeaderPtr->containerID != 'peff') {
        return 0;
    }
    
    // Find the loader section.
    sections = (SectionHeaderPtr) (containerHeaderPtr + 1);
    for (currentSection = 0;
         currentSection < containerHeaderPtr->nbrOfSections;
         currentSection++) {

        if (sections[currentSection].containerOffset > lastOffset) {
            lastOffset = sections[currentSection].containerOffset;
            len = sections[currentSection].rawSize;
        }
    }

    return lastOffset + len;
}

/*******************************************************************************
* theExportSymbol is NOT null-terminated, so use theExportSymbolLength.
*******************************************************************************/
static Boolean SymbolCompare(
    StringPtr theLookedForSymbol,
    StringPtr theExportSymbol,
    unsigned long theExportSymbolLength)
{
    unsigned char * p1 = (unsigned char*)theLookedForSymbol;
    unsigned char * p2 = (unsigned char*)theExportSymbol;
    
    // Same length?
    // (skip over p string len byte)
    if (theExportSymbolLength != *p1++) {
        return false;
    }

    while (theExportSymbolLength-- != 0) {
        if (*p1++ != *p2++) {
            return false;
        }
    }

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
static int readFile(char * path, char ** objAddr, long * objSize)
{
    int fd;
    int err;
    struct stat stat_buf;

    *objAddr = 0;
    *objSize = 0;

    if ((fd = open(path, O_RDONLY)) == -1) {
        return errno;
    }

    do {
        if (fstat(fd, &stat_buf) == -1) {
            err = errno;
            continue;
        }

        // Hack to allow a directory inside the AppleNDRV folder;
        // readFile returns zero error code but zero for bytes & length if
        // path doesn't denote a regular file.
        //
        if ( ! (stat_buf.st_mode & S_IFREG) ) {
            *objAddr = 0;
            *objSize = 0;
            err = 0;
            continue;
        }
        *objSize = stat_buf.st_size;

        if (KERN_SUCCESS != map_fd(fd, 0, (vm_offset_t *) objAddr,
            TRUE, *objSize)) {

            *objAddr = 0;
            *objSize = 0;
            err = errno;
            continue;
        }

        err = 0;

    } while (false);

    close(fd);

    return err;
}


/*******************************************************************************
*
*******************************************************************************/
// The Driver Description
enum {
    kInitialDriverDescriptor    = 0,
    kVersionOneDriverDescriptor    = 1,
    kTheDescriptionSignature    = 'mtej',
};

struct DriverType {
    unsigned char nameInfoStr[32]; // Driver Name/Info String
    unsigned long    version;         // Driver Version Number - really NumVersion
};
typedef struct DriverType DriverType;

struct DriverDescription {
    unsigned long driverDescSignature; // Signature field of this structure
    unsigned long driverDescVersion;   // Version of this data structure
    DriverType    driverType;          // Type of Driver
    char          otherStuff[512];
};
typedef struct DriverDescription DriverDescription;

/*******************************************************************************
*
*******************************************************************************/
static void ExaminePEF(
    mach_port_t masterPort,
    char * pef,
    IOByteCount pefLen,
    CFDictionaryRef allMatching)
{
    char                   descripName[] = "\pTheDriverDescription";
    long                   err;
    DriverDescription      descrip;
    DriverDescription      curDesc;
    char                   matchName[40];
    unsigned long          newVersion;
    unsigned long          curVersion;
    IOReturn               kr;
    io_iterator_t          iter;
    io_service_t           service;
    io_service_t           child;
    io_string_t            path;
    CFStringRef            ndrvPropName = CFSTR("driver,AAPL,MacOS,PowerPC");
    CFDataRef              ndrv;
    CFStringRef		   matchKey;
    CFTypeRef		   value = 0;
    CFDictionaryRef	   matching = 0;
    CFMutableDictionaryRef dict;

    err = GetSymbolFromPEF(descripName, pef, &descrip, sizeof(descrip));
    if (err != 0) {
        printf("\nGetSymbolFromPEF returns %ld\n",err);
        return;
    }
    if ((descrip.driverDescSignature != kTheDescriptionSignature) ||
       (descrip.driverDescVersion != kInitialDriverDescriptor)) {

        return;
    }

    strncpy(matchName, descrip.driverType.nameInfoStr + 1,
        descrip.driverType.nameInfoStr[0]);
    matchName[descrip.driverType.nameInfoStr[0]] = 0;

    matchKey = CFStringCreateWithCString( kCFAllocatorDefault, matchName,
					    kCFStringEncodingMacRoman );
    if (!matchKey)
	return;

    if (allMatching)
    {
	value = CFDictionaryGetValue(allMatching, matchKey);
	if (value)
	{
	    if (value && (CFDictionaryGetTypeID() == CFGetTypeID(value)))
		matching = CFRetain(value);
	    else if (value && (CFStringGetTypeID() == CFGetTypeID(value)))
	    {
		CFRelease(matchKey);
		matchKey = CFRetain(value);
	    }
	}
    }

    if (!matching)
    {
	CFStringRef nameMatchKey = CFSTR(kIONameMatchKey);
	matching = CFDictionaryCreate( kCFAllocatorDefault,
					(const void **) &nameMatchKey, (const void **) &matchKey, 1,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks );
    }
    CFRelease(matchKey);
    if (!matching)
        return;

    kr = IOServiceGetMatchingServices(masterPort, matching, &iter);
    if (kIOReturnSuccess != kr)
        return;

    newVersion = descrip.driverType.version;
    if ((newVersion & 0xffff) == 0x8000) {
        // final stage, release rev
        newVersion |= 0xff;
    }

    for ( ; (service = IOIteratorNext(iter)); IOObjectRelease(service))
    {
        kr = IORegistryEntryGetPath(service, kIOServicePlane, path);
        if (kIOReturnSuccess == kr)
            printf("Name %s matches %s, ", matchName, path);

        ndrv = (CFDataRef) IORegistryEntryCreateCFProperty(service, ndrvPropName,
            kCFAllocatorDefault, kNilOptions);

        if (ndrv)
	{
            err = GetSymbolFromPEF(descripName,
                (const LogicalAddress)CFDataGetBytePtr(ndrv),
                &curDesc, sizeof(curDesc));
            if (err != noErr)
                printf("GetSymbolFromPEF returns %ld\n",err);
            else
	    {
                if ((curDesc.driverDescSignature == kTheDescriptionSignature) &&
                    (curDesc.driverDescVersion == kInitialDriverDescriptor)) {

                    curVersion = curDesc.driverType.version;
                    printf("new version %08lx, current version %08lx\n",
                        newVersion, curVersion);

                    if ((curVersion & 0xffff) == 0x8000) {
                        // final stage, release rev
                        curVersion |= 0xff;
                    }
                    if (newVersion <= curVersion) {
                        pefLen = 0;
                    }
                }
            }
            CFRelease(ndrv);
        }

        if (pefLen == 0)
            continue;

        ndrv = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                           pef, pefLen, kCFAllocatorNull);
        if (ndrv == 0)
            continue;

        printf("Installing ndrv (");
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

        if (dict)
	{
            CFDictionarySetValue(dict, ndrvPropName, ndrv);
            kr = IORegistryEntryGetChildEntry(service, kIOServicePlane, &child);
            if (kr == kIOReturnSuccess)
	    {
                kr = IORegistryEntrySetCFProperties(child, dict);
                IOObjectRelease(child);
            }
            CFRelease(dict);
        } else
            kr = kIOReturnNoMemory;

        CFRelease(ndrv);
        printf("%08x)\n", kr);
    }
    IOObjectRelease(iter);

    return;
}

/*******************************************************************************
*
*******************************************************************************/

static int PEFExamineFile(mach_port_t masterPort, CFURLRef file, CFDictionaryRef props)
{
    char *          pefBytes;
    char *          pef;
    long            pefFileLen;
    IOByteCount     pefLen, pos = 0;
    int             err;
    CFDictionaryRef fileMatch;
    CFDictionaryRef matching = 0;
    Boolean	    matches = true;
    char            cFile[MAXPATHLEN];

    do
    {
	if (!props)
	    continue;

	fileMatch = CFDictionaryGetValue(props, CFSTR("IONDRVFileMatching"));
        if (fileMatch && (CFDictionaryGetTypeID() != CFGetTypeID(fileMatch)))
            fileMatch = 0;

	if (fileMatch)
	{
	    io_service_t service;

	    CFRetain(fileMatch);
	    service = IOServiceGetMatchingService(masterPort, fileMatch);
	    matches = (MACH_PORT_NULL != service);
	    if (matches)
		IOObjectRelease(service);
	    continue;
	}

        matching = CFDictionaryGetValue(props, CFSTR("IONDRVMatching"));
        if (matching && (CFDictionaryGetTypeID() != CFGetTypeID(matching)))
            matching = 0;

    }
    while (false);

    if (!matches)
	return (kIOReturnSuccess);

    if (CFURLGetFileSystemRepresentation(file, TRUE, cFile, MAXPATHLEN))
        err = readFile(cFile, &pefBytes, &pefFileLen);
    else
        err = kIOReturnIOError;

    // Hack to allow a directory inside the AppleNDRV folder;
    // readFile returns zero error code but zero for bytes & length if
    // path doesn't denote a regular file.
    //
    if (err || !pefBytes)
        return err;

    pef = pefBytes;
    while ((pos < pefFileLen) && (pefLen = GetPEFLen(pef)))
    {
        ExaminePEF(masterPort, pef, pefLen, matching);
        pefLen = (pefLen + 15) & ~15;
        pef += pefLen;
        pos += pefLen;
    }

    if (pefBytes)
        vm_deallocate(mach_task_self(), (vm_address_t) pefBytes, pefFileLen);

    return (0);
}

static char * CFURLCopyCString(CFURLRef anURL)
{
    char * string = NULL; // returned
    CFIndex bufferLength;
    CFStringRef urlString = NULL;  // must release
    Boolean error = false;

    urlString = CFURLCopyFileSystemPath(anURL, kCFURLPOSIXPathStyle);
    if (!urlString) {
        goto finish;
    }

    bufferLength = 1 + CFStringGetLength(urlString);

    string = (char *)malloc(bufferLength * sizeof(char));
    if (!string) {
        goto finish;
    }

    if (!CFStringGetCString(urlString, string, bufferLength,
           kCFStringEncodingMacRoman)) {

        error = true;
        goto finish;
    }

finish:
    if (error) {
        free(string);
        string = NULL;
    }
    if (urlString) CFRelease(urlString);
    return string;
}

static void _PEFExamineFile(mach_port_t masterPort, CFURLRef ndrvURL, CFDictionaryRef plist)
{
    if (PEFExamineFile(masterPort, ndrvURL, plist))
    {
	char * ndrv_path = CFURLCopyCString(ndrvURL);
	    printf("error processing NDRV \"%s\"",
		ndrv_path ? ndrv_path : "(unknown)");
	if (ndrv_path)
	    free(ndrv_path);
    }
}

static void PEFExamineBundle( mach_port_t masterPort, CFBundleRef bdl )
{
    CFURLRef	    ndrvURL;
    CFDictionaryRef plist;

    plist = CFBundleGetInfoDictionary(bdl);
    if (!plist)
	return;
    ndrvURL = CFBundleCopyExecutableURL(bdl);
    if (!ndrvURL)
	return;

    _PEFExamineFile(kIOMasterPortDefault, ndrvURL, plist);

    CFRelease(ndrvURL);
}

__private_extern__ 
void IOLoadPEFsFromURL( CFURLRef ndrvDirURL, io_service_t service )
{
    CFIndex     ndrvCount, n;
    CFArrayRef  ndrvDirContents = NULL; // must release
    SInt32      error;

    ndrvDirContents = (CFArrayRef) CFURLCreatePropertyFromResource(
	kCFAllocatorDefault, ndrvDirURL, kCFURLFileDirectoryContents,
	&error);

    ndrvCount = ndrvDirContents ? CFArrayGetCount(ndrvDirContents) : 0;

    for (n = 0; n < ndrvCount; n++)
    {
	CFURLRef	ndrvURL = NULL;  // don't release
	CFNumberRef	num;
	CFBundleRef	bdl;
	CFStringRef	ext;
	SInt32		mode;
	Boolean		skip;

	ndrvURL = (CFURLRef)CFArrayGetValueAtIndex(ndrvDirContents, n);

	bdl = CFBundleCreate(kCFAllocatorDefault, ndrvURL);
	if (bdl)
	{
	    PEFExamineBundle(kIOMasterPortDefault, bdl);
	    CFRelease(bdl);
	    continue;
	}

	ext = CFURLCopyPathExtension(ndrvURL);
	if (ext)
	{
	    skip = CFEqual(ext, CFSTR(".plist"));
	    CFRelease(ext);
	    if (skip)
		continue;
	}

	num = (CFNumberRef) CFURLCreatePropertyFromResource(
	    kCFAllocatorDefault, ndrvURL, kCFURLFilePOSIXMode, &error);
	if (!num)
	    continue;
	CFNumberGetValue(num, kCFNumberSInt32Type, (SInt32 *) &mode);
	CFRelease(num);
	if ((mode & S_IFMT) == S_IFREG)
	    _PEFExamineFile(kIOMasterPortDefault, ndrvURL, NULL);
    }

    if (ndrvDirContents)
	CFRelease(ndrvDirContents);
}

