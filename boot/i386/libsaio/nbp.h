/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
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

#ifndef __LIBSAIO_NBP_H
#define __LIBSAIO_NBP_H

#include "nbp_cmd.h"

/*==========================================================================
 * Call the network loader's exported entry point.
 * Function is in asm.s.
 */
extern void loader(UInt32 code, UInt32 cmdptr);

/*==========================================================================
 * Convert zero-based linear address to far pointer.
 */
#define GET_FP(x)         ( (((x) & 0xffff0000) << (16 - 4)) | \
                             ((x) & 0xffff) )

/*==========================================================================
 * Issue a command to the network loader.
 */
extern UInt32 nbp(nbpCommandCode_t code, nbpCommand_u * cmd);

/*==========================================================================
 * Execute a TFTP Read File command.
 */
extern UInt32 nbpTFTPReadFile(UInt8 *  filename,
                              UInt32 * bufferSize,
                              UInt32   bufferAddr);

/*==========================================================================
 * Execute an Unload Base Code Stack command.
 */
extern UInt32 nbpUnloadBaseCode();

#endif /* !__LIBSAIO_NBP_H */
