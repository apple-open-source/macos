/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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


// AppleCDDAFileSystemVFSOps.c created by CJS on Mon 10-Apr-2000


// System Includes
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ubc.h>
#include <mach/kmod.h>
#include <libkern/OSKextLib.h>

// Project Includes
#ifndef __APPLE_CDDA_FS_VFS_OPS_H__
#include "AppleCDDAFileSystemVFSOps.h"
#endif

#ifndef __APPLE_CDDA_FS_VNODE_OPS_H__
#include "AppleCDDAFileSystemVNodeOps.h"
#endif

#ifndef __APPLE_CDDA_FS_DEBUG_H__
#include "AppleCDDAFileSystemDebug.h"
#endif

#ifndef __APPLE_CDDA_FS_DEFINES_H__
#include "AppleCDDAFileSystemDefines.h"
#endif

#ifndef __APPLE_CDDA_FS_UTILS_H__
#include "AppleCDDAFileSystemUtils.h"
#endif

#ifndef __AIFF_SUPPORT_H__
#include "AIFFSupport.h"
#endif


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------


// Global variables defined in other modules
extern struct vnodeopv_desc		gCDDA_VNodeOperationsDesc;

// CDDA File System globals
static char gAppleCDDAName[MFSNAMELEN] = "cddafs";

static vfstable_t gCDDA_VFSTableEntry;
static struct vnodeopv_desc * gCDDA_VNodeOperationsDescList[1] =
{
	&gCDDA_VNodeOperationsDesc
};

// vfsops
struct vfsops gCDDA_VFSOps =
{
	CDDA_Mount,
	0,			// start
	CDDA_Unmount,
	CDDA_Root,
	0,			// quotactl
	CDDA_VFSGetAttributes,
	0,			// synchronize
	CDDA_VGet,
	0,			// fhtovp
	0,			// vptofh
	0,			// init
	0,			// sysctl
	0,			// setattr
	{ 0 }		// reserved
};

static void
FindVolumeName ( const char * mn, const char ** np, ssize_t * nl );


//-----------------------------------------------------------------------------
//	CDDA_Mount -	This routine is responsible for mounting the filesystem
//					in the desired path
//-----------------------------------------------------------------------------

int
CDDA_Mount ( mount_t					mountPtr,
			 vnode_t					blockDeviceVNodePtr,
			 user_addr_t				data,
			 __unused vfs_context_t		context )
{
	
	AppleCDDAMountPtr		cddaMountPtr	= NULL;
	AppleCDDANodePtr		cddaNodePtr		= NULL;
	void *					xmlData			= NULL;
	int						error			= 0;
	AppleCDDAArguments		cddaArgs;
	struct timeval			now;
	struct timespec			timespec;
	
	OSKextRetainKextWithLoadTag ( OSKextGetCurrentLoadTag ( ) );
	
	bzero ( &cddaArgs, sizeof ( cddaArgs ) );
	bzero ( &now, sizeof ( now ) );
	bzero ( &timespec, sizeof ( timespec ) );
	
	DebugLog ( ( "CDDA_Mount: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( context != NULL ) );
	
	error = copyin ( data, ( caddr_t ) &cddaArgs, sizeof ( cddaArgs ) );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Mount: copyin error = %d\n", error ) );
		goto ERROR;
		
	}
	
	if ( ( vfs_isrdwr ( mountPtr ) ) != 0 )
	{
		
		DebugLog ( ( "Returning EROFS...\n" ) );
		error = EROFS;
		goto ERROR;
		
	}
	
	// Update is a no-op
	if ( vfs_isupdate ( mountPtr ) )
	{
		
		DebugLog ( ( "Returning ENOTSUP...\n" ) );
		error = ENOTSUP;
		goto ERROR;
		
	}
	
	if ( ( cddaArgs.nameData == USER_ADDR_NULL ) || ( cddaArgs.xmlData == USER_ADDR_NULL ) )
	{
		
		DebugLog ( ( "cddaArgs.nameData = 0x%qX, cddaArgs.xmlData = 0x%qX, Returning EINVAL...\n", cddaArgs.nameData, cddaArgs.xmlData ) );
		error = EINVAL;
		goto ERROR;
		
	}
	
	if ( ( cddaArgs.nameDataSize == 0 ) || ( cddaArgs.nameDataSize > kMaxNameDataSize ) )
	{
		
		DebugLog ( ( "cddaArgs.nameDataSize = %u, invalid, Returning EINVAL...\n", cddaArgs.nameDataSize ) );
		error = EINVAL;
		goto ERROR;
		
	}
	
	if ( ( cddaArgs.xmlFileSize == 0 ) || ( cddaArgs.xmlFileSize > kMaxXMLDataSize ) )
	{
		
		DebugLog ( ( "cddaArgs.xmlFileSize = %u, invalid, Returning EINVAL...\n", cddaArgs.xmlFileSize ) );
		error = EINVAL;
		goto ERROR;
		
	}
	
	// Allocate memory for private mount data
	MALLOC ( cddaMountPtr, AppleCDDAMountPtr, sizeof ( AppleCDDAMount ), M_TEMP, M_WAITOK );
	
	// Zero the structure
	bzero ( cddaMountPtr, sizeof ( AppleCDDAMount ) );
	
	// Initialize the lock
	cddaMountPtr->cddaMountLockGroupAttr = lck_grp_attr_alloc_init ( );
	cddaMountPtr->cddaMountLockGroup	 = lck_grp_alloc_init ( "cddafs mount structure", cddaMountPtr->cddaMountLockGroupAttr );
	cddaMountPtr->cddaMountLockAttr		 = lck_attr_alloc_init ( );
	cddaMountPtr->cddaMountLock			 = lck_mtx_alloc_init ( cddaMountPtr->cddaMountLockGroup, cddaMountPtr->cddaMountLockAttr );
	
	// Save the number of audio tracks
	cddaMountPtr->numTracks = cddaArgs.numTracks;
	
	// Save file TYPE/CREATOR
	cddaMountPtr->fileType		= cddaArgs.fileType;
	cddaMountPtr->fileCreator	= cddaArgs.fileCreator;
	
	// Allocate memory for NodeInfo array
	MALLOC ( cddaMountPtr->nodeInfoArrayPtr, AppleCDDANodeInfoPtr,
			 sizeof ( AppleCDDANodeInfo ) * cddaMountPtr->numTracks, M_TEMP, M_WAITOK );
	
	// Zero the array
	bzero ( cddaMountPtr->nodeInfoArrayPtr, sizeof ( AppleCDDANodeInfo ) * cddaMountPtr->numTracks );
	
	// Fill in the mount time
	microtime ( &now );
	TIMEVAL_TO_TIMESPEC ( &now, &timespec );
	cddaMountPtr->mountTime = timespec;
	
	// Allocate memory for CD Track Names data
	MALLOC ( cddaMountPtr->nameData, UInt8 *, cddaArgs.nameDataSize, M_TEMP, M_WAITOK );
	cddaMountPtr->nameDataSize = cddaArgs.nameDataSize;
	
	DebugLog ( ( "cddaMountPtr->nameData = %p, cddaMountPtr->nameDataSize = %d\n", cddaMountPtr->nameData, cddaMountPtr->nameDataSize ) );
	
	error = copyin ( cddaArgs.nameData, ( caddr_t ) cddaMountPtr->nameData, cddaMountPtr->nameDataSize );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Mount: copyin failed with error = %d.\n", error ) );
		goto FREE_NODE_INFO_ERROR;
		
	}
	
	error = CreateNewCDDADirectory ( mountPtr,
									 kAppleCDDARootFileID,
									 &cddaMountPtr->root );
	
	if ( error != 0 )
	{
		
		// XXX fix error to return a valid error number here
		DebugLog ( ( "Returning error = %d after CreateNewCDDADirectory.\n", error ) );
		goto FREE_TRACK_NAMES;
		
	}
	
	// Cache the vid for the root vnode.
	cddaMountPtr->rootVID = vnode_vid ( cddaMountPtr->root );
	
	// Set the root vnode's blockDeviceVNodePtr
	cddaNodePtr = VTOCDDA ( cddaMountPtr->root );
	cddaNodePtr->blockDeviceVNodePtr = blockDeviceVNodePtr;
	
	vfs_setflags ( mountPtr, ( MNT_LOCAL | MNT_RDONLY | MNT_DOVOLFS ) );	// Local Read-Only FileSystem
	vfs_setfsprivate ( mountPtr, ( void * ) cddaMountPtr );					// Hang our data off the MountPoint
	
	// Get a filesystem ID for us
	vfs_getnewfsid ( mountPtr );
	
	// Allocate memory for xml data
	MALLOC ( xmlData, void *, cddaArgs.xmlFileSize, M_TEMP, M_WAITOK );
	
	error = copyin ( cddaArgs.xmlData, ( caddr_t ) xmlData, cddaArgs.xmlFileSize );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Mount: copyin 2 failed with error = %d.\n", error ) );
		goto FREE_ROOT_DIRECTORY;
		
	}
	
	cddaMountPtr->xmlData		= xmlData;
	cddaMountPtr->xmlDataSize	= cddaArgs.xmlFileSize;
	
	// Parse the TOC of the CD and build the tracks
	error = ParseTOC ( mountPtr, cddaMountPtr->numTracks );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Mount: Error = %d returned from ParseTOC.\n", error ) );
		goto FREE_ROOT_DIRECTORY;
		
	}
	
	// Keep a reference on the root directory, and unlock it
	vnode_ref ( cddaMountPtr->root );
	vnode_put ( cddaMountPtr->root );
	
	DebugLog ( ( "CDDA_Mount: Exiting CDDA_Mount.\n" ) );
	
	return ( 0 );
	
	
FREE_ROOT_DIRECTORY:
	
	
	if ( cddaMountPtr->root != NULL )
	{
		
		// Release the root directory vnode we got above
		vnode_put ( cddaMountPtr->root );
		
	}
	
	
FREE_TRACK_NAMES:
	
	
	if ( cddaMountPtr->nameData != NULL )
	{
		
		FREE ( ( caddr_t ) cddaMountPtr->nameData, M_TEMP );
		cddaMountPtr->nameData		= NULL;
		cddaMountPtr->nameDataSize	= 0;
		
	}
	
	
FREE_NODE_INFO_ERROR:
	
	
	if ( cddaMountPtr->nodeInfoArrayPtr != NULL )
	{
		
		// Free memory allocated for NodeInfo array
		FREE ( ( caddr_t ) cddaMountPtr->nodeInfoArrayPtr, M_TEMP );
		cddaMountPtr->nodeInfoArrayPtr = NULL;
		
	}
	
	
	if ( cddaMountPtr != NULL )
	{
		
		// Free memory allocated for mount structure
		FREE ( ( caddr_t ) cddaMountPtr, M_TEMP );
		cddaMountPtr = NULL;
		
	}
	
	
ERROR:
	
	
	OSKextReleaseKextWithLoadTag ( OSKextGetCurrentLoadTag ( ) );
	
	return error;
	
}


//-----------------------------------------------------------------------------
//	CDDA_Unmount -	This routine is called to unmount the disc at the
//					specified mount point
//-----------------------------------------------------------------------------

int
CDDA_Unmount ( mount_t					mountPtr,
			   int						theFlags,
			   unused vfs_context_t		context )
{
	
	vnode_t					rootVNodePtr		= NULLVP;
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	AppleCDDANodePtr		cddaNodePtr			= NULL;
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr	= NULL;
	UInt8 *					nameData			= NULL;
	UInt8 *					xmlData				= NULL;
	int						error				= 0;
	int						flags				= 0;
	
	DebugLog ( ( "CDDA_Unmount: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( context != NULL ) );
	
	cddaMountPtr = VFSTOCDDA ( mountPtr );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	rootVNodePtr = cddaMountPtr->root;
	DebugAssert ( ( rootVNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( rootVNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	if ( theFlags & MNT_FORCE )
	{
		
		DebugLog ( ( "CDDA_Unmount: Setting forceclose.\n" ) );
		flags |= FORCECLOSE;
		
	}
	
	DebugAssert ( ( rootVNodePtr != NULL ) );
	
	// Check for open files
	error = vflush ( mountPtr, rootVNodePtr, flags );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Unmount: Returning error = %d after vflush.\n", error ) );
		return ( error );
		
	}
	
	// Release the underlying root vnode
	vnode_rele ( rootVNodePtr );
	
	error = vflush ( mountPtr, NULLVP, FORCECLOSE );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Unmount: Returning error = %d after vflush.\n", error ) );
		return ( error );
		
	}
	
	DebugLog ( ( "CDDA_Unmount: released the root vnode.\n" ) );
	DebugLog ( ( "CDDA_Unmount: All vnodes killed!\n" ) );
	
	// Get a pointer to the name data
	nameData = VFSTONAMEINFO ( mountPtr );
	
	// Get a pointer to the XML data
	xmlData	 = VFSTOXMLDATA ( mountPtr );
	
	DebugLog ( ( "CDDA_Unmount: Free the name data.\n" ) );
	
	// Free the name data
	FREE ( ( caddr_t ) nameData, M_TEMP );

	DebugLog ( ( "CDDA_Unmount: Free the XML data.\n" ) );
	
	// Free the XML data
	FREE ( ( caddr_t ) xmlData, M_TEMP );
	
	// Get a pointer to the NodeInfo Array	
	nodeInfoArrayPtr = VFSTONODEINFO ( mountPtr );
	
	DebugLog ( ( "CDDA_Unmount: Free the nodeinfo array.\n" ) );
	
	// Free the NodeInfo Array we allocated at mount time
	FREE ( nodeInfoArrayPtr, M_TEMP );
	
	DebugLog ( ( "CDDA_Unmount: Free the nodeinfo lock.\n" ) );
	
	lck_mtx_free ( cddaMountPtr->cddaMountLock, cddaMountPtr->cddaMountLockGroup );
	lck_attr_free ( cddaMountPtr->cddaMountLockAttr );
	lck_grp_free ( cddaMountPtr->cddaMountLockGroup );
	lck_grp_attr_free ( cddaMountPtr->cddaMountLockGroupAttr );
	
	DebugLog ( ( "CDDA_Unmount: Free the mount point data.\n" ) );
	
	// Finally, free the mount-specific data we allocated at mount time
	FREE ( vfs_fsprivate ( mountPtr ), M_TEMP );
	
	// Point the pointer to nothing
	vfs_setfsprivate ( mountPtr, NULL );
	
	DebugLog ( ( "CDDA_Unmount: Exiting, returning error = %d.\n", error ) );
	
	OSKextReleaseKextWithLoadTag ( OSKextGetCurrentLoadTag ( ) );
	
	return 0;
	
}


//-----------------------------------------------------------------------------
//	CDDA_Root - This routine is called to get a vnode pointer to the root
//				vnode of the filesystem
//-----------------------------------------------------------------------------

int
CDDA_Root ( mount_t			mountPtr,
			vnode_t *		vNodeHandle,
			vfs_context_t	context )
{
	
	int			error	= 0;
	ino64_t		inode	= kAppleCDDARootFileID;
	
	DebugLog ( ( "CDDA_Root: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	DebugAssert ( ( context != NULL ) );
	
	error = CDDA_VGet ( mountPtr, inode, vNodeHandle, context );
	
	DebugLog ( ( "CDDA_Root: exiting...\n" ) );
	
	return ( error );
	
}


//-----------------------------------------------------------------------------
//	CDDA_VFSGetAttributes -	This routine is called to get filesystem attributes.
//-----------------------------------------------------------------------------

int
CDDA_VFSGetAttributes ( mount_t					mountPtr,
						struct vfs_attr *		attrPtr,
						unused vfs_context_t	context )
{
	
	AppleCDDAMountPtr			cddaMountPtr	= NULL;
	AppleCDDANodePtr			rootCDDANodePtr = NULL;
	vol_capabilities_attr_t *	capabilities	= NULL;
	vol_attributes_attr_t *		attributes		= NULL;
	
	DebugLog ( ( "CDDA_VFSGetAttributes: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( attrPtr != NULL ) );
	
	capabilities	= &attrPtr->f_capabilities;
	attributes		= &attrPtr->f_attributes;	
	
	cddaMountPtr = VFSTOCDDA ( mountPtr );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	rootCDDANodePtr = VTOCDDA ( cddaMountPtr->root );
	DebugAssert ( ( rootCDDANodePtr != NULL ) );
	
	DebugAssert ( ( rootCDDANodePtr->nodeType == kAppleCDDADirectoryType ) );
	
	//
	// The +1's below are to account for the ".TOC.plist" file
	//
	VFSATTR_RETURN ( attrPtr, f_objcount, cddaMountPtr->numTracks + 1 );
	VFSATTR_RETURN ( attrPtr, f_filecount, cddaMountPtr->numTracks + 1 );
	VFSATTR_RETURN ( attrPtr, f_dircount, 0 );
	VFSATTR_RETURN ( attrPtr, f_maxobjcount, cddaMountPtr->numTracks + 1 );
	VFSATTR_RETURN ( attrPtr, f_iosize, kMaxBytesPerRead * 2 );
	VFSATTR_RETURN ( attrPtr, f_blocks, rootCDDANodePtr->u.directory.directorySize / kPhysicalMediaBlockSize );
	VFSATTR_RETURN ( attrPtr, f_bfree, 0 );
	VFSATTR_RETURN ( attrPtr, f_bavail, 0 );
	VFSATTR_RETURN ( attrPtr, f_bused, attrPtr->f_blocks );
	VFSATTR_RETURN ( attrPtr, f_files, rootCDDANodePtr->u.directory.entryCount );
	VFSATTR_RETURN ( attrPtr, f_ffree, 0 );
	VFSATTR_RETURN ( attrPtr, f_bsize, kPhysicalMediaBlockSize );
	
	VFSATTR_RETURN ( attrPtr, f_create_time, cddaMountPtr->mountTime );
	VFSATTR_RETURN ( attrPtr, f_modify_time, cddaMountPtr->mountTime );
	
	if ( VFSATTR_IS_ACTIVE ( attrPtr, f_backup_time ) )
	{
		
		attrPtr->f_backup_time.tv_sec = 0;
		attrPtr->f_backup_time.tv_nsec = 0;
		VFSATTR_SET_SUPPORTED ( attrPtr, f_backup_time );
		
	}
	
	if ( VFSATTR_IS_ACTIVE ( attrPtr, f_vol_name ) )
	{
		
		const char *	vname 	= NULL;
		ssize_t			length 	= 0;
		
		FindVolumeName ( vfs_statfs ( mountPtr )->f_mntonname, &vname, &length );
		
		if ( vname != NULL )
 		{
			
 			strlcpy ( attrPtr->f_vol_name, vname, MAXPATHLEN );
 			VFSATTR_SET_SUPPORTED ( attrPtr, f_vol_name );
			
		}
		
	}
	
	// XXX these will go away soon
	VFSATTR_RETURN ( attrPtr, f_fsid, vfs_statfs ( mountPtr )->f_fsid );
	
	// Set the signature to 'BD'.
	VFSATTR_RETURN ( attrPtr, f_signature, kAppleCDDAFileSystemVolumeSignature );
	
	// Set the carbon FSID to 'JH'.
	VFSATTR_RETURN ( attrPtr, f_carbon_fsid, kAppleCDDAFileSystemVCBFSID );
	
	// We understand the following.
	capabilities->valid[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_PERSISTENTOBJECTIDS |
			VOL_CAP_FMT_SYMBOLICLINKS |
			VOL_CAP_FMT_HARDLINKS |
			VOL_CAP_FMT_JOURNAL |
			VOL_CAP_FMT_JOURNAL_ACTIVE |
			VOL_CAP_FMT_NO_ROOT_TIMES |
			VOL_CAP_FMT_SPARSE_FILES |
			VOL_CAP_FMT_ZERO_RUNS |
			VOL_CAP_FMT_CASE_SENSITIVE |
			VOL_CAP_FMT_CASE_PRESERVING |
			VOL_CAP_FMT_FAST_STATFS |
			VOL_CAP_FMT_PATH_FROM_ID;
	
	// We understand the following interfaces.
	capabilities->valid[VOL_CAPABILITIES_INTERFACES] =
			VOL_CAP_INT_SEARCHFS |
			VOL_CAP_INT_ATTRLIST |
			VOL_CAP_INT_NFSEXPORT |
			VOL_CAP_INT_READDIRATTR |
			VOL_CAP_INT_EXCHANGEDATA |
			VOL_CAP_INT_COPYFILE |
			VOL_CAP_INT_ALLOCATE |
			VOL_CAP_INT_VOL_RENAME |
			VOL_CAP_INT_ADVLOCK |
			VOL_CAP_INT_FLOCK |
			VOL_CAP_INT_EXTENDED_ATTR;
	
	// We only support these bits of the above recognized things.
	capabilities->capabilities[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_PATH_FROM_ID |
			VOL_CAP_FMT_PERSISTENTOBJECTIDS |
			VOL_CAP_FMT_FAST_STATFS |
			VOL_CAP_FMT_NO_ROOT_TIMES;
	
	// We only support these things of the above recognized ones.
	capabilities->capabilities[VOL_CAPABILITIES_INTERFACES] =
			VOL_CAP_INT_ATTRLIST |
			VOL_CAP_INT_EXTENDED_ATTR;
	
	// Reserved. Zero them.
	capabilities->capabilities[VOL_CAPABILITIES_RESERVED1]	= 0;
	capabilities->capabilities[VOL_CAPABILITIES_RESERVED2]	= 0;
	capabilities->valid[VOL_CAPABILITIES_RESERVED1]			= 0;
	capabilities->valid[VOL_CAPABILITIES_RESERVED2]			= 0;
	
	// Set the attributes.
	VFSATTR_SET_SUPPORTED ( attrPtr, f_capabilities );
	
	attributes->validattr.commonattr	= kAppleCDDACommonAttributesValidMask;
	attributes->validattr.volattr		= kAppleCDDAVolumeAttributesValidMask;
	attributes->validattr.dirattr		= kAppleCDDADirectoryAttributesValidMask;
	attributes->validattr.fileattr		= kAppleCDDAFileAttributesValidMask;
	attributes->validattr.forkattr		= kAppleCDDAForkAttributesValidMask;
	
	attributes->nativeattr.commonattr	= kAppleCDDACommonAttributesValidMask;
	attributes->nativeattr.volattr		= kAppleCDDAVolumeAttributesValidMask;
	attributes->nativeattr.dirattr		= kAppleCDDADirectoryAttributesValidMask;
	attributes->nativeattr.fileattr		= kAppleCDDAFileAttributesValidMask;
	attributes->nativeattr.forkattr		= kAppleCDDAForkAttributesValidMask;
	
	// Set the attributes.
	VFSATTR_SET_SUPPORTED ( attrPtr, f_attributes );
	
	return ( 0 );
	
}


//-----------------------------------------------------------------------------
//	CDDA_VGet - This routine is responsible for getting the desired vnode.
//-----------------------------------------------------------------------------

int
CDDA_VGet ( mount_t					mountPtr,
			ino64_t					ino,
			vnode_t *				vNodeHandle,
			unused vfs_context_t	context )
{
	
	DebugLog ( ( "CDDA_VGet: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	DebugAssert ( ( context != NULL ) );
	
	return CDDA_VGetInternal ( mountPtr, ino, NULL, NULL, vNodeHandle );
	
}


//-----------------------------------------------------------------------------
//	CDDA_VGetInternal - This routine is responsible for getting the
//						desired vnode.
//-----------------------------------------------------------------------------

int
CDDA_VGetInternal ( mount_t					mountPtr,
					ino64_t					ino,
					vnode_t					parentVNodePtr,
					struct componentname *	compNamePtr,
					vnode_t *				vNodeHandle )
{
	
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	AppleCDDANodePtr		parentCDDANodePtr	= NULL;
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr	= NULL;
	vnode_t					vNodePtr			= NULLVP;
	int						error				= 0;
	uint32_t				index				= 0;
	uint32_t				vid					= 0;
	
	cddaMountPtr = VFSTOCDDA ( mountPtr );
	
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	if ( ino == kAppleCDDARootFileID )
	{
		
		DebugLog ( ( "Root vnode asked for!\n" ) );
		
		// Get the root vnode.
		error = vnode_getwithvid ( cddaMountPtr->root, cddaMountPtr->rootVID );
		if ( error == 0 )
		{
			
			// Return the root vnode to the caller since we have an iocount
			// on it now.
			*vNodeHandle = cddaMountPtr->root;
			
		}
		
		goto Exit;
		
	}
	
	else if ( ino == kAppleCDDAXMLFileID )
	{
		
		DebugLog ( ( "XML vnode asked for!\n" ) );
		
		lck_mtx_lock ( cddaMountPtr->cddaMountLock );
		
		while ( cddaMountPtr->xmlFileFlags & kAppleCDDANodeBusyMask )
		{
			
			cddaMountPtr->xmlFileFlags |= kAppleCDDANodeWantedMask;
			( void ) msleep ( &cddaMountPtr->xmlFileFlags,
						cddaMountPtr->cddaMountLock,
						PINOD,
						"CDDA_VGetInternal(XML)",
						0 );
			
		}
		
		cddaMountPtr->xmlFileFlags |= kAppleCDDANodeBusyMask;
		vNodePtr = cddaMountPtr->xmlFileVNodePtr;
		
		if ( vNodePtr != NULL )
		{
			
			// Get an io_count on the vnode
			vid = vnode_vid ( vNodePtr );
			
			// Release the lock on our nodeInfo structure first
			lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
			
			error = vnode_getwithvid ( vNodePtr, vid );

			lck_mtx_lock ( cddaMountPtr->cddaMountLock );
						
			if ( error == 0 )
			{
				
				// Return the vnode to the caller since we have an iocount
				// on it now.
				*vNodeHandle = vNodePtr;
				
			}
			
		}
		
		else
		{
			
			lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
			
			error = CreateNewXMLFile ( mountPtr,
									   cddaMountPtr->xmlDataSize,
									   cddaMountPtr->xmlData,
									   parentVNodePtr,
									   compNamePtr,
									   &vNodePtr );
			
			lck_mtx_lock ( cddaMountPtr->cddaMountLock );
			
			if ( error == 0 )
			{
				
				cddaMountPtr->xmlFileVNodePtr = vNodePtr;
				*vNodeHandle = vNodePtr;
				
			}
			
		}
		
		cddaMountPtr->xmlFileFlags &= ~kAppleCDDANodeBusyMask;
		lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
		
		if ( cddaMountPtr->xmlFileFlags & kAppleCDDANodeWantedMask )
		{
			
			cddaMountPtr->xmlFileFlags &= ~kAppleCDDANodeWantedMask;
			wakeup ( &cddaMountPtr->xmlFileFlags );
			
		}
		
		goto Exit;
		
	}
	
	else if ( ( ino > 100 ) && ( ino < 200 ) )
	{
		
		parentCDDANodePtr = VTOCDDA ( cddaMountPtr->root );
		
		// subtract our file offset to get to the real nodeID
		ino -= kOffsetForFiles;
		
		// Look in our NodeInfo array to see if a vnode has been created for this
		// track yet.
		
		nodeInfoArrayPtr = VFSTONODEINFO ( mountPtr );
		DebugAssert ( ( nodeInfoArrayPtr != NULL ) );

		DebugLog ( ( "Locking cddaMountLock.\n" ) );
		DebugLog ( ( "Looking for nodeID = %lld.\n", ino ) );
		
		lck_mtx_lock ( cddaMountPtr->cddaMountLock );
		
		index = 0;
		
		while ( index < ( parentCDDANodePtr->u.directory.entryCount - kNumberOfFakeDirEntries ) )
		{
			
			if ( nodeInfoArrayPtr->trackDescriptor.point == ( UInt8 ) ino )
			{
				
				while ( nodeInfoArrayPtr->flags & kAppleCDDANodeBusyMask )
				{
					
					nodeInfoArrayPtr->flags |= kAppleCDDANodeWantedMask;
					( void ) msleep ( &nodeInfoArrayPtr->flags,
								cddaMountPtr->cddaMountLock,
								PINOD,
								"CDDA_VGetInternal(Track)",
								0 );
					
				}
				
				nodeInfoArrayPtr->flags |= kAppleCDDANodeBusyMask;
				vNodePtr = nodeInfoArrayPtr->vNodePtr;
				
				// See if the vNodePtr was attached (vNodePtr is only non-NULL if the node has been created)
				if ( vNodePtr != NULL )
				{
					
					// Get the vid, so we can get an io_count on the vnode.
					vid = vnode_vid ( vNodePtr );
					
					DebugLog ( ( "Releasing cddaMountLock.\n" ) );
					
					// Release the lock on our nodeInfo structure first.
					lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
					
					// Get the iocount.
					error = vnode_getwithvid ( vNodePtr, vid );
					
					// Grab our lock again.
					lck_mtx_lock ( cddaMountPtr->cddaMountLock );
					
					if ( error == 0 )
					{
						
						// The specified vNode was found and we got
						// an iocount on it. Return this vnode.
						*vNodeHandle = vNodePtr;
						
					}
					
				}
				
				else
				{
					
					DebugLog ( ( "Couldn't find the vnode...Calling CreateNewCDDAFile\n" ) );

					// Release the lock. Creating a vnode could cause another one to be reclaimed.
					// We don't want to deadlock in reclaim because we forgot to drop the lock here!
					lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
					
					// If we get here, it doesn't exist yet, so create it
					error = CreateNewCDDAFile ( mountPtr,
												nodeInfoArrayPtr->trackDescriptor.point + kOffsetForFiles,
												nodeInfoArrayPtr,
												parentVNodePtr,
												compNamePtr,
												&vNodePtr );
					
					lck_mtx_lock ( cddaMountPtr->cddaMountLock );
					
					if ( error == 0 )
					{
						
						// Make sure we mark this vnode as being in the array now.
						*vNodeHandle = vNodePtr;
						nodeInfoArrayPtr->vNodePtr = vNodePtr;
						
					}
					
				}
				
				nodeInfoArrayPtr->flags &= ~kAppleCDDANodeBusyMask;
				lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
				
				if ( nodeInfoArrayPtr->flags & kAppleCDDANodeWantedMask )
				{
					
					nodeInfoArrayPtr->flags &= ~kAppleCDDANodeWantedMask;
					wakeup ( &nodeInfoArrayPtr->flags );
					
				}
				
				goto Exit;
				
			}
			
			index++;
			nodeInfoArrayPtr++;
			
		}
		
		DebugLog ( ( "Releasing cddaMountLock...About to return ENOENT.\n" ) );
		
		// Now we can release our lock because we're getting out
		lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
				
		// If we get here, we couldn't find anything with that name. Return ENOENT.
		error = ENOENT;
		
	}
	
	else
	{
		
		error = ENOENT;
		
	}
	
	
Exit:
	
	DebugLog ( ( "CDDA_VGetInternal: exiting...\n" ) );
	
	return ( error );
	
}


//-----------------------------------------------------------------------------
//	FindVolumeName - Cribbed from vfs_attrlist.c. Gets volume name.
//-----------------------------------------------------------------------------

static void
FindVolumeName ( const char * mn, const char ** np, ssize_t * nl )
{
	
	int				counting = 0;
	const char *	cp		 = NULL;
	
	// We're looking for the last sequence of non-'/' characters, but
	// not including any trailing '/' characters.
	*np 		= NULL;
	*nl 		= 0;
	
	for ( cp = mn; *cp != 0; cp++ )
	{
		
		if ( !counting )
		{
			
			// start of run of chars
			if ( *cp != '/' )
			{
				
				*np = cp;
				counting = 1;
				
			}
			
		}
		
		else
		{
			
			// end of run of chars
			if ( *cp == '/' )
			{
				
				*nl = cp - *np;
				counting = 0;
				
			}
			
		}
		
	}
	
	// Need to close run?
	if ( counting )
		*nl = cp - *np;
	
}


//-----------------------------------------------------------------------------
//	Apple_CDDA_FS_Module_Start -	This routine is responsible for
//									all the initialization that would
//									ordinarily be done as part of the
//									system startup
//-----------------------------------------------------------------------------

int
Apple_CDDA_FS_Module_Start ( unused kmod_info_t * moduleInfo,
							 unused void * loadArgument )
{
	
	errno_t				error		= KERN_FAILURE;
	struct vfs_fsentry	vfsEntry;
	
	bzero ( &vfsEntry, sizeof ( vfsEntry ) );
	
	vfsEntry.vfe_vfsops		= &gCDDA_VFSOps;
	vfsEntry.vfe_vopcnt		= 1;	// Just one vnode operation table
	vfsEntry.vfe_opvdescs	= gCDDA_VNodeOperationsDescList;
	vfsEntry.vfe_flags		= VFS_TBLNOTYPENUM | VFS_TBLLOCALVOL | VFS_TBL64BITREADY | VFS_TBLTHREADSAFE | VFS_TBLFSNODELOCK;
	
	strlcpy ( vfsEntry.vfe_fsname, gAppleCDDAName, sizeof ( gAppleCDDAName ) ); 
	
	error = vfs_fsadd ( &vfsEntry, &gCDDA_VFSTableEntry );
	
	return error ? KERN_FAILURE : KERN_SUCCESS;
	
}


//-----------------------------------------------------------------------------
//	Apple_CDDA_FS_Module_Stop - This routine is responsible for stopping
//								filesystem services
//-----------------------------------------------------------------------------

int
Apple_CDDA_FS_Module_Stop ( unused kmod_info_t * moduleInfo,
							unused void * unloadArgument )
{
	
	errno_t 	error = KERN_SUCCESS;
	
	error = vfs_fsremove ( gCDDA_VFSTableEntry );
	
	return error ? KERN_FAILURE : KERN_SUCCESS;
	
}


//-----------------------------------------------------------------------------
//				End				Of			File
//-----------------------------------------------------------------------------
