/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
 *  BLLoadXCOFFLoader.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 28 2002.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLLoadXCOFFLoader.c,v 1.16 2005/02/03 00:42:25 ssen Exp $
 *
 *  $Log: BLLoadXCOFFLoader.c,v $
 *  Revision 1.16  2005/02/03 00:42:25  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.15  2004/04/20 21:40:42  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.14  2004/03/17 01:21:53  ssen
 *  Initialize a uint32_t explicitly with 0xFFFFFFFF
 *
 *  Revision 1.13  2003/10/16 23:50:05  ssen
 *  Partially finish cleanup of headers to add "const" to char[] arguments
 *  that won't be modified.
 *
 *  Revision 1.12  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.11  2003/04/23 00:06:49  ssen
 *  Print checksum for xcoff
 *
 *  Revision 1.10  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.9  2003/04/16 23:57:31  ssen
 *  Update Copyrights
 *
 *  Revision 1.8  2003/04/12 03:52:19  ssen
 *  Straggling function prototype that still had void **data instead
 *  of CFDataRef *data
 *
 *  Revision 1.7  2003/03/20 05:06:20  ssen
 *  remove some more non-c99 types
 *
 *  Revision 1.6  2003/03/20 03:40:57  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.5.2.3  2003/03/20 03:23:32  ssen
 *  swap the entry point
 *
 *  Revision 1.5.2.2  2003/03/20 03:13:42  ssen
 *  typo
 *
 *  Revision 1.5.2.1  2003/03/20 03:11:42  ssen
 *  swap XCOFF data structures
 *
 *  Revision 1.5  2003/03/19 22:57:02  ssen
 *  C99 types
 *
 *  Revision 1.4  2002/06/11 00:50:46  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.3  2002/04/27 17:54:59  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.2  2002/04/25 07:27:27  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.1  2002/03/05 00:01:42  ssen
 *  code reorg of secondary loader
 *
 *
 */

#include <sys/fcntl.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "BLWriteStartupFile.h"

#include "bless.h"
#include "bless_private.h"

static CFDataRef convertXCOFFImage (BLContextPtr context, CFDataRef rawImage, uint32_t *entryP,
			     uint32_t *loadBaseP, uint32_t *loadSizeP);

int BLLoadXCOFFLoader(BLContextPtr context, const unsigned char xcoff[],
                            uint32_t *entrypoint, uint32_t *loadbase,
                            uint32_t *size, uint32_t *checksum,
                            CFDataRef *data) {
                            
  CFDataRef                rawImage;
  CFDataRef                cookedImage;
  CFURLRef                 loaderSrc;
  CFStringRef              loaderSrcPath;


    loaderSrcPath = CFStringCreateWithCString(kCFAllocatorDefault,
                                        xcoff,
                                        kCFStringEncodingUTF8);
                                        
    loaderSrc = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
					    loaderSrcPath,
					    kCFURLPOSIXPathStyle,
					    FALSE);

    CFRelease(loaderSrcPath); loaderSrcPath = NULL;

    if(!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
					       loaderSrc,
					       &rawImage,
					       NULL,
					       NULL,
					       NULL)) {
        contextprintf(context, kBLLogLevelError,  "Can't load loader %s\n", xcoff );
        CFRelease(loaderSrc);
        return 1;
    }
    
    CFRelease(loaderSrc); loaderSrc = NULL;
    contextprintf(context, kBLLogLevelVerbose,  "XCOFF loaded into memory\n" );


    cookedImage = convertXCOFFImage (context, rawImage, entrypoint, loadbase, size);
    if(NULL == cookedImage) {
        CFRelease(rawImage); rawImage = NULL;
        return 2;
    }

    CFRelease(rawImage); rawImage = NULL;


    *checksum = BLBlockChecksum(CFDataGetBytePtr(cookedImage), *size);
    contextprintf(context, kBLLogLevelVerbose,  "Checksumming XCOFF\n" );
    contextprintf(context, kBLLogLevelVerbose,  "Checksum is %u\n", *checksum );


    *data = cookedImage;
    return 0;
}


/* ***************************************************************** *
   accumulateSectionSpans
 * ***************************************************************** */
static void
accumulateSectionSpans (XSection *sectionP, uint32_t *lowestAddress,
			uint32_t *highestAddress)
{
  if (sectionP->vAddr < *lowestAddress)
    {
      *lowestAddress = sectionP->vAddr;
    }
  if (sectionP->vAddr + sectionP->size  > *highestAddress)
    {
      *highestAddress = sectionP->vAddr + sectionP->size;
    }
}

#define SWAP(size, x, field) (x->field) = CFSwapInt##size##BigToHost(x->field)

void _swapXFileHeader(XFileHeader *fileP) {
    SWAP(16, fileP, magic);
    SWAP(16, fileP, nSections);
    SWAP(32, fileP, timeAndDate);
    SWAP(32, fileP, symPtr);
    SWAP(32, fileP, nSyms);
    SWAP(16, fileP, optHeaderSize);
    SWAP(16, fileP, flags);
}

void _swapXOptHeader(XOptHeader *optP) {
    SWAP(16, optP, magic);
    SWAP(16, optP, version);
    SWAP(32, optP, textSize);
    SWAP(32, optP, dataSize);
    SWAP(32, optP, BSSSize);
    SWAP(32, optP, entryPoint);
    SWAP(32, optP, textStart);
    SWAP(32, optP, dataStart);
    SWAP(32, optP, toc);
    SWAP(16, optP, snEntry);
    SWAP(16, optP, snText);
    SWAP(16, optP, snData);
    SWAP(16, optP, snTOC);
    SWAP(16, optP, snLoader);
    SWAP(16, optP, snBSS);    
}

void _swapXSection(XSection *secP) {
    SWAP(32, secP, pAddr);
    SWAP(32, secP, vAddr);
    SWAP(32, secP, size);
    SWAP(32, secP, sectionFileOffset);
    SWAP(32, secP, relocationsFileOffset);
    SWAP(32, secP, lineNumbersFileOffset);
    SWAP(16, secP, nRelocations);
    SWAP(16, secP, nLineNumbers);
    SWAP(32, secP, flags);
}

static CFDataRef convertXCOFFImage (BLContextPtr context, CFDataRef rawImage, uint32_t *entryP,
			     uint32_t *loadBaseP, uint32_t *loadSizeP) {

  const uint8_t             *xImageP        = CFDataGetBytePtr(rawImage);
  XFileHeader             *fileP          = (XFileHeader *) xImageP;
  XOptHeader              *optP           = (XOptHeader *) (fileP + 1);
  XSection                *sectionsP      = (XSection *) (optP + 1);
  XSection                *sectionP;
  uint8_t                  *partImageP;
  uint32_t                  partImageSize;
  uint32_t                  lowestAddress   = 0xFFFFFFFF;
  uint32_t                  highestAddress  = 0x00000000;
  const uint32_t    kPageSize               = 4096;

  _swapXFileHeader(fileP);
  _swapXOptHeader(optP);
  
  if (fileP->magic != kFileMagic || optP->magic != kOptHeaderMagic) {
    contextprintf(context, kBLLogLevelError,  "Bad SecondaryLoader XCOFF!\n" );
    return NULL;
  }

  _swapXSection(&sectionsP[optP->snText - 1]);
  _swapXSection(&sectionsP[optP->snData - 1]);
  _swapXSection(&sectionsP[optP->snBSS  - 1]);
  
  accumulateSectionSpans (&sectionsP[optP->snText - 1], &lowestAddress,
			  &highestAddress);
  accumulateSectionSpans (&sectionsP[optP->snData - 1], &lowestAddress,
			  &highestAddress);
  accumulateSectionSpans (&sectionsP[optP->snBSS  - 1], &lowestAddress,
			  &highestAddress);

  // Round to page multiples (OF 1.0.5 bug)
  lowestAddress &= -kPageSize;
  highestAddress = (highestAddress + kPageSize - 1) & -kPageSize;
  partImageSize = highestAddress - lowestAddress;

  partImageP = (char *)calloc(partImageSize, sizeof(char));
  if (partImageP == 0) {
    contextprintf(context, kBLLogLevelError,  "Can't allocate memory (%u bytes) for SecondaryLoader image\n", partImageSize );
    return NULL;
  }

  // these swapped above
  
  // Copy TEXT section into partition image area
  sectionP = &sectionsP[optP->snText - 1];
  BlockMoveData ((uint8_t *) xImageP + sectionP->sectionFileOffset,
		 partImageP + sectionP->vAddr - lowestAddress,
		 sectionP->size);

  // Copy DATA section into partition image area
  sectionP = &sectionsP[optP->snData - 1];
  BlockMoveData ((uint8_t *) xImageP + sectionP->sectionFileOffset,
		 partImageP + sectionP->vAddr - lowestAddress,
		 sectionP->size);

  // Zero BSS section in partition image area
  sectionP = &sectionsP[optP->snBSS - 1];
  memset (partImageP + sectionP->vAddr - lowestAddress, 0,
	  sectionP->size);

  *loadBaseP = lowestAddress;
  *loadSizeP = partImageSize;
  *entryP = CFSwapInt32BigToHost(*(uint32_t *) (partImageP + optP->entryPoint - lowestAddress));
  // Dereference transition vector[0]

  return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, partImageP, partImageSize,
				     kCFAllocatorMalloc);
}

