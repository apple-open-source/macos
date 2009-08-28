/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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

// AppleCDDAFileSystemVNodeOps.h created by CJS on Mon 13-Apr-2000

#ifndef __APPLE_CDDA_FS_VNODE_OPS_H__
#define __APPLE_CDDA_FS_VNODE_OPS_H__

#include <sys/param.h>

#ifdef __cplusplus
extern "C" {
#endif


// BlockSize constants
enum
{
	kPhysicalMediaBlockSize		= 2352,
	kMaxBlocksPerRead			= MAXBSIZE / kPhysicalMediaBlockSize,			// Max blocks to read per bread()
	kMaxBytesPerRead			= kMaxBlocksPerRead * kPhysicalMediaBlockSize	// Max bytes to read per bread()
};

#ifdef __cplusplus
}
#endif

#endif // __APPLE_CDDA_FS_VNODE_OPS_H__


//-----------------------------------------------------------------------------
//				End				Of			File
//-----------------------------------------------------------------------------
