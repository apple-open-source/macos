/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
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
#endif __APPLE_CDDA_FS_VNODE_OPS_H__

#ifdef KERNEL

#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

enum
{
	kAppleCDDARootFileID	= 2,
	kNumberOfFakeDirEntries	= 3,
	kOffsetForFiles			= 100
};

extern int ( **gCDDA_VNodeOp_p )( void * );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Function Prototypes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


int	CDDA_Init 					( struct vfsconf * vfsPtr );
int CDDA_Mount 					( struct mount * mountPtr,
								  char * path,
								  caddr_t data,
								  struct nameidata * ndp,
								  struct proc * theProc );
int CDDA_Start					( struct mount * mountPtr,
								  int theFlags,
								  struct proc * theProc );
int CDDA_Unmount				( struct mount * mountPtr,
								  int theFlags,
								  struct proc * theProc );
int CDDA_Root					( struct mount * mountPtr,
								  struct vnode ** vnodeHandle );
int CDDA_Statfs					( struct mount * mountPtr,
								  struct statfs * statFSPtr,
								  struct proc * theProc );
int	CDDA_VGet					( struct mount * mountPtr,
								  void * nodeID,
								  struct vnode ** vNodeHandle );
int	CDDA_FileHandleToVNodePtr 	( struct mount * mountPtr,
								  struct fid * fileHandlePtr,
								  struct mbuf * networkAddressPtr,
								  struct vnode ** vNodeHandle,
								  int * exportFlagsPtr,
								  struct ucred ** anonymousCredHandle );
int CDDA_QuotaControl 			( struct mount * mountPtr,
								  int commands,
								  uid_t userID,
								  caddr_t arguments,
								  struct proc * theProcPtr );
int	CDDA_Synchronize 			( struct mount * mountPtr,
								  int waitForIOCompletion,
								  struct ucred * userCredPtr,
								  struct proc * theProcPtr );
int	CDDA_SystemControl 			( int * name,
								  u_int nameLength,
								  void * oldPtr,
								  size_t * oldLengthPtr,
								  void * newPtr,
								  size_t newLength,
								  struct proc * theProcPtr );
int CDDA_VNodePtrToFileHandle 	( struct vnode * vNodePtr,
								  struct fid * fileHandlePtr );

								  
extern struct vfsops gCDDA_VFSOps;


#ifdef __cplusplus
}
#endif

#endif // __APPLE_CDDA_FS_VFS_OPS_H__
