/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 *  BLLoadXCOFFLoader.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 28 2002.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLLoadXCOFFLoader.c,v 1.3 2002/04/27 17:54:59 ssen Exp $
 *
 *  $Log: BLLoadXCOFFLoader.c,v $
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

static CFDataRef convertXCOFFImage (BLContext context, CFDataRef rawImage, u_int32_t *entryP,
			     u_int32_t *loadBaseP, u_int32_t *loadSizeP);

int BLLoadXCOFFLoader(BLContext context, unsigned char xcoff[],
                            u_int32_t *entrypoint, u_int32_t *loadbase,
                            u_int32_t *size, u_int32_t *checksum,
                            void /* CFDataRef */ **data) {
                            
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

    *data = (void *)cookedImage;
    return 0;
}


/* ***************************************************************** *
   accumulateSectionSpans
 * ***************************************************************** */
static void
accumulateSectionSpans (XSection *sectionP, u_int32_t *lowestAddress,
			u_int32_t *highestAddress)
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


static CFDataRef convertXCOFFImage (BLContext context, CFDataRef rawImage, u_int32_t *entryP,
			     u_int32_t *loadBaseP, u_int32_t *loadSizeP) {

  const u_int8_t             *xImageP        = CFDataGetBytePtr(rawImage);
  XFileHeader             *fileP          = (XFileHeader *) xImageP;
  XOptHeader              *optP           = (XOptHeader *) (fileP + 1);
  XSection                *sectionsP      = (XSection *) (optP + 1);
  XSection                *sectionP;
  u_int8_t                  *partImageP;
  u_int32_t                  partImageSize;
  u_int32_t                  lowestAddress   = ~0ul;
  u_int32_t                  highestAddress  = 0ul;
  const u_int32_t    kPageSize               = 4096;

  if (fileP->magic != kFileMagic || optP->magic != kOptHeaderMagic) {
    contextprintf(context, kBLLogLevelError,  "Bad SecondaryLoader XCOFF!\n" );
    return NULL;
  }

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

  // Copy TEXT section into partition image area
  sectionP = &sectionsP[optP->snText - 1];
  BlockMoveData ((u_int8_t *) xImageP + sectionP->sectionFileOffset,
		 partImageP + sectionP->vAddr - lowestAddress,
		 sectionP->size);

  // Copy DATA section into partition image area
  sectionP = &sectionsP[optP->snData - 1];
  BlockMoveData ((u_int8_t *) xImageP + sectionP->sectionFileOffset,
		 partImageP + sectionP->vAddr - lowestAddress,
		 sectionP->size);

  // Zero BSS section in partition image area
  sectionP = &sectionsP[optP->snBSS - 1];
  memset (partImageP + sectionP->vAddr - lowestAddress, 0,
	  sectionP->size);

  *loadBaseP = lowestAddress;
  *loadSizeP = partImageSize;
  *entryP = *(u_int32_t *) (partImageP + optP->entryPoint - lowestAddress);
  // Dereference transition vector[0]

  return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, partImageP, partImageSize,
				     kCFAllocatorMalloc);
}

