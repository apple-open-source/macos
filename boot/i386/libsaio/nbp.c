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

#include "libsaio.h"
#include "nbp.h"

/*==========================================================================
 * Issue a command to the network loader.
 *
 * The 'cmd' command structure should be allocated on the stack to
 * ensure that it resides within the addressable range for the
 * network loader, which runs in real mode.
 */
UInt32
nbp(nbpCommandCode_t code, nbpCommand_u * cmd)
{
	loader(code, GET_FP((UInt32) cmd));

	// Must re-enable the A20 address line, the PXE firmware will
	// disable the A20 line control.
	//
	enableA20();

	return cmd->header.status;
}

/*==========================================================================
 * Execute a TFTP Read File command.
 *
 */
UInt32 nbpTFTPReadFile(UInt8 *  filename,   // name of the file
                       UInt32 * bufferSize, // [IN] size limit, [OUT} real size
                       UInt32   bufferAddr) // physical address
{
    nbpCommandTFTPReadFile_s  cmd;
	UInt32                    ret;

	strcpy(cmd.filename, filename);
	cmd.status     = nbpStatusFailed;
	cmd.bufferSize = *bufferSize;
	cmd.buffer     = bufferAddr;

	ret = nbp(nbpCommandTFTPReadFile, (nbpCommand_u *) &cmd);

	*bufferSize = cmd.bufferSize;  // bytes transferred

    return ret;
}

/*==========================================================================
 * Execute an Unload Base Code Stack command.
 *
 */
UInt32 nbpUnloadBaseCode()
{
    return nbp(nbpCommandUnloadBaseCode, (nbpCommand_u *) 0);
}
