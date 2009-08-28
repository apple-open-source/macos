/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
    File:    UnpackPiData.c
    Written by:    Jeffrey Robbin
    Copyright:    © 1994, 1996 by Apple Computer, Inc., all rights reserved.

    File:    GetSymbolFromPEF.c
    Written by:    Jeffrey Robbin
    Copyright:    © 1994, 1996 by Apple Computer, Inc., all rights reserved.
*/

#include <CoreFoundation/CoreFoundation.h>
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

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>

#include "GetSymbolFromPEF.h"
#include <IOKit/graphics/IOGraphicsLib.h>
#include "IOGraphicsLibPrivate.h"
#include "IOGraphicsLibInternal.h"

enum 
{
    kIOPEFparamErr		  = 1001,
    kIOPEFmemFullErr		  = 1002,
    kIOPEFcfragFragmentFormatErr  = 1003,
    kIOPEFcfragNoSectionErr       = 1004,
    kIOPEFcfragNoSymbolErr        = 1005,
    kIOPEFcfragFragmentCorruptErr = 1006
};

/*******************************************************************************
*
*******************************************************************************/
static unsigned char PEFGetNextByte(
    unsigned char ** rawBuffer,
    int * rawBufferRemaining)
{
    *rawBufferRemaining = *rawBufferRemaining - 1;
    return *(*rawBuffer)++;
}


/*******************************************************************************
*
*******************************************************************************/
static uint32_t PEFGetCount(
    unsigned char ** rawBuffer,
    int * rawBufferRemaining)
{
    register unsigned char b;
    register uint32_t value = 0UL;

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
    int             cntX, cnt, rpt, dcnt, delta;
    unsigned char    op, b;
    unsigned char *  unpackBuffer;
    unsigned char *  originalUnpackBuffer;
    unsigned char *  endUnpackBuffer;
    unsigned char *  oldRawBuffer;
    int             oldRawBufferRemaining;
    unsigned char *  rawBuffer;
    int             rawBufferRemaining;
    
    // Verify incoming section is packed.
    if (sectionHeaderPtr->regionKind != kPIDataSection) {
        return kIOPEFparamErr;
    }
    
    // Allocate memory to unpack into
    originalUnpackBuffer = (unsigned char*)NewPtrSys(sectionHeaderPtr->initSize);
    if (originalUnpackBuffer == nil) {
        return kIOPEFmemFullErr;
    }

    unpackBuffer = originalUnpackBuffer;
    endUnpackBuffer = unpackBuffer + sectionHeaderPtr->initSize;
    rawBuffer = (unsigned char*)((uintptr_t)thePEFPtr +
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

    return kIOPEFparamErr;
}


/*******************************************************************************
* GetSymbolFromPEF will extract from a PEF container the data associated
* with a given symbol name.  It requires that the PEF file have been previously
* loaded into memory.
*******************************************************************************/
static OSStatus GetSymbolFromPEF(
    char *inSymbolName,
    const LogicalAddress thePEFPtr,
    LogicalAddress theSymbolPtr,
    ByteCount theSymbolSize)
{
    StringPtr           theSymbolName = (StringPtr) inSymbolName;
    ContainerHeaderPtr  containerHeaderPtr;  // Pointer to the Container Header
    SectionHeaderPtr    loaderSectionPtr = 0; // Ptr to Loader Section Header
    SectionHeaderPtr    exportSectionPtr;    // Ptr to Section Header with symbol
    short               currentSection;
    Boolean             foundSection;
    Boolean             foundSymbol;
    int                numExportSymbols;
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
        return kIOPEFcfragFragmentFormatErr;
    }
    // Is this a known PEF container format?
    if (containerHeaderPtr->containerID != 'peff') {
        return kIOPEFcfragFragmentFormatErr;
    }

    // Validate parameters
    if (theSymbolPtr == nil) {
        return kIOPEFparamErr;
    }
    
    // Find the loader section.
    foundSection = false;
    for (currentSection = 0;
         currentSection < containerHeaderPtr->nbrOfSections;
         currentSection++) {

        loaderSectionPtr = (SectionHeaderPtr)((uintptr_t)containerHeaderPtr +
            sizeof(ContainerHeader) +
            (sizeof(SectionHeader) * currentSection));

        if (loaderSectionPtr->regionKind == kLoaderSection) {
            foundSection = true;
            break;
        }
    }

    if (foundSection == false) {
        return kIOPEFcfragNoSectionErr;
    }

    // Get the number of export symbols.
    loaderHeaderPtr = (LoaderHeaderPtr)((uintptr_t)thePEFPtr +
        loaderSectionPtr->containerOffset);
    numExportSymbols = loaderHeaderPtr->nbrExportSyms;
    
    // Start at the first exported symbol.
    exportSymbolEntryPtr = (ExportSymbolEntryPtr)((uintptr_t)loaderHeaderPtr +
        loaderHeaderPtr->slotTblOffset +
        (sizeof(LoaderHashSlotEntry) * (1<<loaderHeaderPtr->hashSlotTblSz)) +
        (sizeof(LoaderExportChainEntry) * numExportSymbols));

    exportChainEntryPtr = (LoaderExportChainEntryPtr)
        ((uintptr_t)loaderHeaderPtr +
        loaderHeaderPtr->slotTblOffset +
        (sizeof(LoaderHashSlotEntry) * (1<<loaderHeaderPtr->hashSlotTblSz)));

    foundSymbol = false;
    while (numExportSymbols-- > 0) {
        exportSymbolName = (StringPtr)((uintptr_t)loaderHeaderPtr +
             loaderHeaderPtr->strTblOffset +
             (exportSymbolEntryPtr->class_and_name & 0x00FFFFFF));

        if (SymbolCompare(theSymbolName, exportSymbolName,
                exportChainEntryPtr->_h._h_h._nameLength)) {

            foundSymbol = true;
            break;
        }
        exportSymbolEntryPtr = (ExportSymbolEntryPtr)
            (((intptr_t) exportSymbolEntryPtr) + 10);
        exportChainEntryPtr++;
    }

    if (foundSymbol == false) {
        return kIOPEFcfragNoSymbolErr;
    }
    
    // Found the symbol, so... let's go get the data!
    exportSectionPtr = (SectionHeaderPtr)((uintptr_t)containerHeaderPtr +
        sizeof(ContainerHeader) +
        (sizeof(SectionHeader) * exportSymbolEntryPtr->sectionNumber));

    expandedDataPtr = nil;

    switch (exportSectionPtr -> regionKind) {
      case kPIDataSection:
        // Expand the data!  (Not yet... :)
        if (UnpackPiData(thePEFPtr, exportSectionPtr, &expandedDataPtr) != noErr) {
            return kIOPEFcfragFragmentCorruptErr;
        }

        sourceDataPtr = (unsigned char*)((uintptr_t)expandedDataPtr +
            exportSymbolEntryPtr->address);
        break;

      default:
        sourceDataPtr = (unsigned char*)((uintptr_t)thePEFPtr +
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
    int  lastOffset = 0;
    int  len = 0;

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
    uint32_t theExportSymbolLength)
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
// The Driver Description
enum {
    kInitialDriverDescriptor    = 0,
    kVersionOneDriverDescriptor    = 1,
    kTheDescriptionSignature    = 'mtej',
};

struct DriverType {
    unsigned char nameInfoStr[32]; // Driver Name/Info String
    uint32_t    version;         // Driver Version Number - really NumVersion
};
typedef struct DriverType DriverType;

struct DriverDescription {
    uint32_t driverDescSignature; // Signature field of this structure
    uint32_t driverDescVersion;   // Version of this data structure
    DriverType    driverType;          // Type of Driver
    // other data follows...
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
    int                   err;
    DriverDescription      descrip;
    char                   matchName[40];
    uint32_t          newVersion;
    uint32_t          curVersion;
    IOReturn               kr;
    io_iterator_t          iter;
    io_service_t           service;
    io_string_t            path;
    CFStringRef            ndrvPropName = CFSTR("driver,AAPL,MacOS,PowerPC");
    CFDataRef              ndrv;
    CFStringRef		   matchKey;
    CFTypeRef		   value = 0;
    CFDictionaryRef	   matching = 0;
    CFMutableDictionaryRef dict;

    err = GetSymbolFromPEF(descripName, pef, &descrip, sizeof(descrip));
    if (err != 0) {
        printf("\nGetSymbolFromPEF returns %d\n",err);
        return;
    }
    if ((descrip.driverDescSignature != kTheDescriptionSignature) ||
       (descrip.driverDescVersion != kInitialDriverDescriptor)) {

        return;
    }

    strncpy(matchName, (char *) descrip.driverType.nameInfoStr + 1,
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
	    DriverDescription   _curDesc;
	    DriverDescription * curDesc;

	    curDesc = (DriverDescription *) CFDataGetBytePtr(ndrv);
	    err = noErr;

	    if ((sizeof(DriverDescription) > (size_t) CFDataGetLength(ndrv))
	     || (curDesc->driverDescSignature != kTheDescriptionSignature))
	    {
		curDesc = &_curDesc;
		err = GetSymbolFromPEF(descripName,
		    (const LogicalAddress)CFDataGetBytePtr(ndrv),
		    curDesc, sizeof(DriverDescription));
	    }
	    if (err != noErr)
		printf("GetSymbolFromPEF returns %d\n",err);
            else
	    {
                if ((curDesc->driverDescSignature == kTheDescriptionSignature) &&
                    (curDesc->driverDescVersion == kInitialDriverDescriptor))
		{
                    curVersion = curDesc->driverType.version;
                    printf("new version %08x, current version %08x\n",
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
                           (UInt8 *) pef, pefLen, kCFAllocatorNull);
        if (ndrv == 0)
            continue;

        printf("Installing ndrv (");
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

        if (dict)
	{
	    io_service_t child = MACH_PORT_NULL;
	    io_iterator_t iter;

            CFDictionarySetValue(dict, ndrvPropName, ndrv);

	    kr = IORegistryEntryGetChildIterator(service, kIOServicePlane, &iter);
            if (kr == kIOReturnSuccess)
	    {
		kr = kIOReturnNotFound;
		for( ;
		    (child = IOIteratorNext(iter));
		    IOObjectRelease(child)) {
	    
		    if (IOObjectConformsTo(child, "IOFramebuffer"))
			break;
		}
		IOObjectRelease(iter);
	    }
            if (child)
	    {
                kr = IORegistryEntrySetCFProperties(child, dict);
                IOObjectRelease(child);
            }
            CFRelease(dict);
        }
	else
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
    vm_offset_t	    pefBytes;
    vm_size_t       pefFileLen;
    char *          pef;
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

    if (CFURLGetFileSystemRepresentation(file, TRUE, (UInt8 *) cFile, MAXPATHLEN))
        err = readFile(cFile, &pefBytes, &pefFileLen);
    else
        err = kIOReturnIOError;

    if (kIOReturnSuccess != err)
        return (err);

    pef = (char *) pefBytes;
    while ((pos < pefFileLen) && (pefLen = GetPEFLen(pef)))
    {
        ExaminePEF(masterPort, pef, pefLen, matching);
        pefLen = (pefLen + 15) & ~15;
        pef += pefLen;
        pos += pefLen;
    }

    if (pefBytes)
        vm_deallocate(mach_task_self(), pefBytes, pefFileLen);

    return (0);
}

static void _PEFExamineFile(mach_port_t masterPort, CFURLRef ndrvURL, CFDictionaryRef plist)
{
    if (PEFExamineFile(masterPort, ndrvURL, plist))
    {
	char buf[PATH_MAX];
	char * ndrv_path;
	if(CFURLGetFileSystemRepresentation(ndrvURL, true /*resolve*/,
		(UInt8*)buf, PATH_MAX)) {
	    ndrv_path = buf;
	} else {
	    ndrv_path = "(unknown)";
	}

	printf("error processing NDRV \"%s\"", ndrv_path);
    }
}

static void PEFExamineBundle( mach_port_t masterPort __unused, CFBundleRef bdl )
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

void IOLoadPEFsFromURL( CFURLRef ndrvDirURL, io_service_t service __unused )
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

