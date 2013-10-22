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

// AppleCDDAFileSystemVFSOps.h created by CJS on Mon 27-Apr-2000

#ifndef __APPLE_CDDA_FS_VFS_OPS_H__
#define __APPLE_CDDA_FS_VFS_OPS_H__

#ifdef __cplusplus
extern "C" {
#endif

// Project Includes
#ifndef __APPLE_CDDA_FS_VNODE_OPS_H__
#include "AppleCDDAFileSystemVNodeOps.h"
#endif

#ifdef KERNEL

#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#endif


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

enum
{
	kAppleCDDARootFileID	= 2,
	kNumberOfFakeDirEntries	= 3,
	kOffsetForFiles			= 100
};

extern int ( **gCDDA_VNodeOp_p )( void * );


//-----------------------------------------------------------------------------
//	Function Prototypes
//-----------------------------------------------------------------------------


int CDDA_Mount 					( mount_t mountPtr,
								  vnode_t blockDeviceVNodePtr,
								  user_addr_t data,
								  vfs_context_t context );
int CDDA_Unmount				( mount_t mountPtr,
								  int theFlags,
								  vfs_context_t context );
int CDDA_Root					( mount_t mountPtr,
								  vnode_t * vnodeHandle,
								  vfs_context_t context );
int CDDA_VFSGetAttributes		( mount_t mountPtr,
    							  struct vfs_attr * attrPtr,
 								  vfs_context_t context );
int	CDDA_VGet					( mount_t mountPtr,
								  ino64_t  nodeID,
								  vnode_t * vNodeHandle,
								  vfs_context_t context );
extern struct vfsops gCDDA_VFSOps;

// Private internal methods
int
CDDA_VGetInternal ( mount_t 				mountPtr,
					ino64_t  				ino,
					vnode_t					parentVNodePtr,
					struct componentname *	compNamePtr,
					vnode_t * 				vNodeHandle );


#ifdef __cplusplus
}
#endif

#endif // __APPLE_CDDA_FS_VFS_OPS_H__
