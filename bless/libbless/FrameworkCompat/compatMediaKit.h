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
 *  compatMediaKit.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jun 28 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: compatMediaKit.h,v 1.2 2002/03/10 23:51:10 ssen Exp $
 *
 *  $Log: compatMediaKit.h,v $
 *  Revision 1.2  2002/03/10 23:51:10  ssen
 *  BLFrmatHFS now just takes the number of bytes to reserve
 *
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.13  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.11  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#if !defined(DARWIN)

typedef void MediaDescriptor;

typedef int16_t (*BIOVector)(void *Refcon,u_int8_t Opcode,u_int16_t Blocksize,u_int32_t Blockno,u_int32_t Blockcount,void *Buffer);

enum {
	ioreadcmd		=	0x02,			//	Read	Blocks
	iowritecmd		=	0x03,			//	Write	Blocks

	startupFileExistsErr	= -5313		,	//	-5313 The startup file already exists
	badStartupFileErr	= -5312			,	//	-5312 The startup file is damaged
	startupTooSmallErr	= -5311			,	//	-5311 The startup file is too 
};

#define NUM_BOOTVALS	4
#define CHECKSUM_OFF	0
#define ENTRY_OFF		1
#define BASE_OFF		2
#define SIZE_OFF		3


int16_t _BLMKMediaDeviceIO(void *Refcon,u_int8_t Opcode,u_int16_t Blocksize,u_int32_t Blockno,u_int32_t Blockcount,void *Buffer);
int _BLMKMediaDeviceOpen(const char *Devicename,int openmode,MediaDescriptor **mfd);
int _BLMKMediaDeviceClose(MediaDescriptor *mfd);
int _BLMKCreateStartupFile(BIOVector ioVec, void *refcon, u_int32_t driveBlocks);
int _BLMKStartupFileSize(BIOVector ioVec, void *refcon, u_int32_t *numSectors);
int _BLMKReadWriteStartupFile(BIOVector ioVec, void *refcon, u_int8_t opcode, u_int32_t 	sectorOffset, u_int32_t numSectors, void *buffer);
int _BLMKWriteStartupPartInfo(BIOVector ioVec, void *refcon, u_int16_t partIdx);


#endif /* !DARWIN */

int isMediaKitAvailable();
