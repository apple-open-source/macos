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
 *  BLWriteStartupFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Jun 25 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLWriteStartupFile.c,v 1.7 2002/04/27 17:54:59 ssen Exp $
 *
 *  $Log: BLWriteStartupFile.c,v $
 *  Revision 1.7  2002/04/27 17:54:59  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.6  2002/04/25 07:27:27  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.5  2002/03/08 07:15:42  ssen
 *  Add #if !(DARWIN), and add headerdoc
 *
 *  Revision 1.4  2002/03/05 00:01:42  ssen
 *  code reorg of secondary loader
 *
 *  Revision 1.3  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2001/12/06 23:37:25  ssen
 *  For unpartitioned devices, don't try to update the pmap
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.10  2001/11/11 06:19:08  ssen
 *  revert to -pre-libbless
 *
 *  Revision 1.8  2001/10/26 04:15:01  ssen
 *  add dollar Id and dollar Log
 *
 *
 */

#if !defined(DARWIN)

#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/storage/IOMediaBSDClient.h>

#include "compatMediaKit.h"

#include "bless.h"
#include "bless_private.h"

#define SECTORSIZE 512


int BLWriteStartupFile(BLContext context, unsigned char xcoff[],
                    unsigned char partitionDev[],
                    unsigned char parentDev[],
                    unsigned long partitionNum) {

  MediaDescriptor         *mfd;
  CFDataRef                cookedImage;
  u_int32_t                   checksum = 0;
  u_int32_t                   sfEntry;
  u_int32_t                   sfBase;
  u_int32_t                   sfSize;
  u_int32_t                   startupSectors;
  u_int32_t					blockCount;
  char                     sector[SECTORSIZE];
  u_int32_t                  *bootVals;

  OSErr ret;

	{ // get block count
		int dfd;
        
		dfd  = open(partitionDev, O_RDONLY );
        if (dfd == -1) {
			contextprintf(context, kBLLogLevelError,  "Could not get device size: %s\n", strerror(errno) );
			return 1;
		}
		
        ret = ioctl(dfd,DKIOCGETBLOCKCOUNT,&blockCount); 
        if (ret < 0) {
			contextprintf(context, kBLLogLevelError,  "Could not get device size: %s\n", strerror(errno) );
			return 1;
		}

		close(dfd);
	}
  
  ret = _BLMKMediaDeviceOpen(partitionDev, ioreadcmd, &mfd);

  if(ret) {
    contextprintf(context, kBLLogLevelError,  "Cannot open device %s (%d)\n", partitionDev, ret );
    return 1;
  }

    ret = BLLoadXCOFFLoader(context, xcoff, &sfEntry, &sfBase, &sfSize, &checksum,
           (void **)&cookedImage );

  if (ret) {
    contextprintf(context, kBLLogLevelError,  "Could not load XCOFF: %d", ret );
    ret = _BLMKMediaDeviceClose(mfd);
    return 2;
  }

  contextprintf(context, kBLLogLevelVerbose,  "Creating StartupFile\n" );

    /* this will check for HFS+ for us */
  ret = _BLMKCreateStartupFile(_BLMKMediaDeviceIO, mfd, blockCount);

  if (ret && ret != startupFileExistsErr) {
    contextprintf(context, kBLLogLevelError,  "Error %d from MKCreateStartupFile()", ret );
    ret = _BLMKMediaDeviceClose(mfd);
    return 1;
  }

  contextprintf(context, kBLLogLevelVerbose,  "Checking size of StartupFile.\n" );
  ret = _BLMKStartupFileSize(_BLMKMediaDeviceIO, mfd, &startupSectors);
  if (!ret && (startupSectors < (sfSize / SECTORSIZE))) {
    ret = startupTooSmallErr;
  }

  if (ret && ret != startupFileExistsErr) {
    contextprintf(context, kBLLogLevelError,  "Error %d from MKStartupFileSize()", ret );
    ret = _BLMKMediaDeviceClose(mfd);
    return 1;
  }

  contextprintf(context, kBLLogLevelVerbose,  "StartupFile is %u bytes\n", startupSectors * SECTORSIZE );

  contextprintf(context, kBLLogLevelVerbose, "Writing StartupFile (%u bytes, %u %d byte blocks)\n",
            sfSize, sfSize / SECTORSIZE, SECTORSIZE );

  ret = _BLMKReadWriteStartupFile(_BLMKMediaDeviceIO, mfd, iowritecmd, 0,
			       sfSize / SECTORSIZE,
			       (char *)CFDataGetBytePtr(cookedImage));

  if(ret) {
      contextprintf(context, kBLLogLevelError,  "Error %d from WriteStartupFile()\n", ret );
      ret = _BLMKMediaDeviceClose(mfd);
      return 1;
  }

  contextprintf(context, kBLLogLevelVerbose,  "Appending partition info (0x%08x)\n", checksum );
  memset(sector, 0, SECTORSIZE);
  bootVals = (u_int32_t *) &(sector[ SECTORSIZE -
				  (sizeof(u_int32_t) * NUM_BOOTVALS) ]) ;
  bootVals[CHECKSUM_OFF]      = checksum;
  bootVals[ENTRY_OFF]         = sfEntry;
  bootVals[BASE_OFF]          = sfBase;
  bootVals[SIZE_OFF]          = sfSize;
  ret = _BLMKReadWriteStartupFile(_BLMKMediaDeviceIO,
                                    mfd,
                                    iowritecmd,
                                    startupSectors - 1,
                                    1,
                                    sector);
  if (ret) {
	  contextprintf(context, kBLLogLevelError,  "Error %d from second WriteStartupFile()\n", ret );
	  ret = _BLMKMediaDeviceClose(mfd);
	  return 1;
  }

  contextprintf(context, kBLLogLevelVerbose,  "Closing volume\n" );
  ret = _BLMKMediaDeviceClose(mfd);
  if (ret) {
	  contextprintf(context, kBLLogLevelError,  "Error %d from MKMediaDeviceClose()", ret );
	  return 1;
  }

  if(partitionNum == 0) {
	  contextprintf(context, kBLLogLevelVerbose,  "Skipping append to partition map, because this is an unpartitioned device\n" );
  } else {
	  ret = _BLMKMediaDeviceOpen(parentDev, ioreadcmd, &mfd);
	  if (ret) {
		  contextprintf(context, kBLLogLevelError, "Unable to open %s to write StartupFile partition info. (%d)\n",
				parentDev, ret);
		  return 1;
	  }

	  contextprintf(context, kBLLogLevelVerbose,  "Writing extra partition info to %s\n", parentDev );
	  ret = _BLMKWriteStartupPartInfo(_BLMKMediaDeviceIO, mfd,
								   partitionNum-1);
	  if (ret) {
		  contextprintf(context, kBLLogLevelError,  "Error %d from MKWriteStartupPartInfo()\n", ret );
		  ret = _BLMKMediaDeviceClose(mfd);
		  return 2;
	  }

	  ret = _BLMKMediaDeviceClose(mfd);
	  if (ret) {
		  contextprintf(context, kBLLogLevelError,  "Error %d from MKMediaDeviceClose()\n", ret );
	  }	  
  }

  contextprintf(context, kBLLogLevelVerbose,  "StartupFile installed\n" );

  return 0;
}

#endif /* !DARWIN */
