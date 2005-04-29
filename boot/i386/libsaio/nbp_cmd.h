/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __LIBSAIO_NBP_CMD_H
#define __LIBSAIO_NBP_CMD_H

#include <IOKit/IOTypes.h>

/*==========================================================================
 * NBP return status codes.
 */
typedef enum {
    nbpStatusSuccess          = 0,
    nbpStatusFailed,
	nbpStatusInvalid,
} nbpStatus_t;

/*==========================================================================
 * NBP commands codes.
 */
typedef enum {
    nbpCommandTFTPReadFile    = 1,
    nbpCommandTFTPGetFileSize,
    nbpCommandUnloadBaseCode,
} nbpCommandCode_t;

/*==========================================================================
 * NBP commands.
 */
typedef struct {
    UInt32    status;         /* return code from NBP */
} nbpCommandHeader_s;

typedef struct {
    UInt32   status;         /* return code from NBP */
    UInt8    filename[128];  /* name of file to be downloaded */
    UInt32   bufferSize;     /* size of the download buffer */
    UInt32   buffer;         /* physical address of the download buffer */
} nbpCommandTFTPReadFile_s;

typedef struct {
    UInt32   status;         /* return code from NBP */
    UInt8    filename[128];  /* name of file to be downloaded */
    UInt32   filesize;       /* size of the file specified */
} nbpCommandTFTPGetFileSize_s;

typedef struct {
    UInt32   status;         /* return code from NBP */
    UInt8    sname[64];      /* server name */
    UInt32   CIP;            /* client IP address */
    UInt32   SIP;            /* server IP address */
    UInt32   GIP;            /* gateway IP address */
} nbpCommandGetNetworkInfo_s;

/*==========================================================================
 * An union of all NBP command structures.
 */
typedef union {
	nbpCommandHeader_s           header;
	nbpCommandTFTPReadFile_s     tftpReadFile;
	nbpCommandTFTPGetFileSize_s  tftpFileSize;
} nbpCommand_u;

#endif /* !__LIBSAIO_NBP_CMD_H */
