/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


// Project Includes
#ifndef __APPLE_CDDA_FS_VFS_OPS_H__
#include "AppleCDDAFileSystemVFSOps.h"
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

// System Includes
#include <sys/domain.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/ubc.h>
#include <sys/vnode.h>
#include <sys/disk.h>

// To get funnel prototypes
#include <kern/thread.h>
#include <sys/systm.h>

#include <miscfs/specfs/specdev.h>

// Declarations
typedef int (*PFI)();

extern char *	strncpy __P ( ( char *, const char *, size_t ) );	// Kernel already includes a copy
extern int		strcmp __P	( ( const char *, const char * ) );		// Kernel already includes a copy


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Globals
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct slock	gCDDANumberOfInstancesLock;
UInt32			gCDDANumberOfInstances = 0;

// CDDA File System globals
static char gAppleCDDAName[MFSNAMELEN] = "cddafs";

// Global variables defined in other modules
extern struct vnodeopv_desc		gCDDA_VNodeOperationsDesc;

// The following refer to kernel global variables used in the loading/initialization:
extern int	maxvfsconf;			// The highest fs type number [old-style ID] in use [despite its name]
extern int	vfs_opv_numops;		// The total number of defined vnode operations

// vfsops
struct vfsops gCDDA_VFSOps =
{
	CDDA_Mount,
	CDDA_Start,
	CDDA_Unmount,
	CDDA_Root,
	CDDA_QuotaControl,
	CDDA_Statfs,
	CDDA_Synchronize,
	CDDA_VGet,
	CDDA_FileHandleToVNodePtr,
	CDDA_VNodePtrToFileHandle,
	CDDA_Init,
	CDDA_SystemControl
};


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Init - This routine is responsible for all the initialization for
//				this instance of the filesystem
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Init ( struct vfsconf * vfsConfPtr )
{

	DebugLog ( ( "CDDA_Init: Entering.\n" ) );

	DebugAssert ( ( vfsConfPtr != NULL ) );
		
	DebugLog ( ( "CDDA_Init: exiting...\n" ) );
	
	return ( 0 );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Mount -	This routine is responsible for mounting the filesystem
//					in the desired path
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Mount ( struct mount * mountPtr,
			 char * path,
			 caddr_t data,
			 struct nameidata * nameiDataPtr,
			 struct proc * theProcPtr )
{
	
	AppleCDDAMountPtr		cddaMountPtr			= NULL;
	AppleCDDAArguments		cddaArgs;
	AppleCDDANodePtr		cddaNodePtr				= NULL;
	struct vnode *			blockDeviceVNodePtr		= NULL;
	size_t					size					= 0;
	int						error					= 0;
	struct ucred *			credPtr					= NULL;
	struct timeval			now;
	struct timespec			timespec;
	void *					xmlData					= NULL;
	
	DebugLog ( ( "CDDA_Mount: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( path != NULL ) );
	DebugAssert ( ( nameiDataPtr != NULL ) );
	DebugAssert ( ( theProcPtr != NULL ) );
	
	error = copyin ( data, ( caddr_t ) &cddaArgs, sizeof ( cddaArgs ) );
	if ( error != 0 )
	{

		goto ERROR;

	}
		
	if ( ( mountPtr->mnt_flag & MNT_RDONLY ) == 0 )
	{
		
		DebugLog ( ( "Returning EROFS...\n" ) );
		error = EROFS;
		goto ERROR;
	
	}
	
	// Update is a no-op
	if ( mountPtr->mnt_flag & MNT_UPDATE )
	{

		DebugLog ( ( "Returning EOPNOTSUPP...\n" ) );
		error = EOPNOTSUPP;
		goto ERROR;

	}
	
	DebugLog ( ( "CDDA_Mount: cddaArgs.device = %s.\n", cddaArgs.device ) );	
	
	// Not an update, or updating the name: look up the name
	// and verify that it refers to a sensible block device.
	NDINIT ( nameiDataPtr, LOOKUP | LOCKLEAF, FOLLOW, UIO_USERSPACE, cddaArgs.device, theProcPtr );
	error = namei ( nameiDataPtr );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Mount: Error getting device: %s.\n", cddaArgs.device ) );
		goto ERROR;
		
	}

	blockDeviceVNodePtr = nameiDataPtr->ni_vp;
	if ( blockDeviceVNodePtr == NULL )
	{
		
		DebugLog ( ( "CDDA_Mount: blockDeviceVNodePtr is NULL.\n" ) );
		error = ENOTBLK;
		goto RELEASE_DEV_NODE_ERROR;
		
	}
	
	if ( blockDeviceVNodePtr->v_type != VBLK )
	{
		
		DebugLog ( ( "CDDA_Mount: Not a block device.\n" ) );
		error = ENOTBLK;
		goto RELEASE_DEV_NODE_ERROR;
				
	}
		
	// If mount by non-root, then verify that user has necessary
	// permissions on the device
	if ( theProcPtr->p_ucred->cr_uid != 0 )
	{
		
		// No need to lock here, since we used LOCKLEAF in NDINIT
		// Call VOP_ACCESS on device node to see if this user has ok credentials
		error = VOP_ACCESS ( blockDeviceVNodePtr, VREAD, theProcPtr->p_ucred, theProcPtr );
		
		if ( error != 0 )
		{
			
			goto RELEASE_DEV_NODE_ERROR;
		
		}
			
	}
	
	error = VOP_OPEN ( blockDeviceVNodePtr, FREAD, FSCRED, theProcPtr );
	if ( error != 0 )
	{
	
		DebugLog ( ( "CDDA_Mount: VOP_OPEN on block device returned an error = %d.\n", error ) );
		goto RELEASE_DEV_NODE_ERROR;
	
	}
	
	// Set the credentials
	credPtr = ( theProcPtr != NULL ) ? theProcPtr->p_ucred : NOCRED;
	
	// Allocate memory for private mount data
	MALLOC ( cddaMountPtr, AppleCDDAMountPtr, sizeof ( AppleCDDAMount ), M_TEMP, M_WAITOK );
	
	// Zero the structure
	bzero ( cddaMountPtr, sizeof ( AppleCDDAMount ) );
	
	// initialize the lock
	lockinit ( &cddaMountPtr->nodeInfoLock, PINOD, "cddamountlock", 0, 0 );
	
	// Save the number of audio tracks
	cddaMountPtr->numTracks = cddaArgs.numTracks;
	
	// Save file TYPE/CREATOR
	cddaMountPtr->fileType		= cddaArgs.fileType;
	cddaMountPtr->fileCreator	= cddaArgs.fileCreator;
	
	DebugLog ( ( "fileType = 0x%08x, fileCreator = 0x%08x\n", cddaMountPtr->fileType, cddaMountPtr->fileCreator ) );
	
	// Allocate memory for NodeInfo array
	MALLOC ( cddaMountPtr->nodeInfoArrayPtr, AppleCDDANodeInfoPtr,
			 sizeof ( AppleCDDANodeInfo ) * cddaMountPtr->numTracks, M_TEMP, M_WAITOK );
	
	// Zero the array
	bzero ( cddaMountPtr->nodeInfoArrayPtr, sizeof ( AppleCDDANodeInfo ) * cddaMountPtr->numTracks );
			
	// Copy in the path name to the mountdata
	( void ) copyinstr ( path, mountPtr->mnt_stat.f_mntonname, MNAMELEN - 1, &size );
	bzero ( mountPtr->mnt_stat.f_mntonname + size, MNAMELEN - size );
		
	// Copy the device name to the mountdata
	( void ) copyinstr ( cddaArgs.device, mountPtr->mnt_stat.f_mntfromname, MNAMELEN - 1, &size );
	bzero ( mountPtr->mnt_stat.f_mntfromname + size, MNAMELEN - size );
		
	// Fill in the mount time
	now = time;
	TIMEVAL_TO_TIMESPEC ( &now, &timespec );
	cddaMountPtr->mountTime = timespec;

	// Allocate memory for CD Track Names data
	MALLOC ( cddaMountPtr->nameData, UInt8 *, cddaArgs.nameDataSize, M_TEMP, M_WAITOK );
	cddaMountPtr->nameDataSize = cddaArgs.nameDataSize;
	
	error = copyin ( cddaArgs.nameData, ( caddr_t ) cddaMountPtr->nameData, cddaMountPtr->nameDataSize );
	if ( error != 0 )
	{
		
		goto FREE_NODE_INFO_ERROR;
		
	}
	
	error = CreateNewCDDADirectory ( mountPtr,
									 ( char * ) &mountPtr->mnt_stat.f_mntonname[1],
									 kAppleCDDARootFileID,
									 theProcPtr,
									 &cddaMountPtr->root );
	
	if ( error != 0 )
	{
		
		//Ê¥¥¥ fix error to return a valid error number here
		DebugLog ( ( "Returning error = %d after CreateNewCDDADirectory.\n", error ) );
		goto FREE_TRACK_NAMES;
		
	}
	
	// Tell the system that this is the root directory
	cddaMountPtr->root->v_flag |= VROOT;
	
	// Set the root vnode's blockDeviceVNodePtr
	cddaNodePtr = VTOCDDA ( cddaMountPtr->root );
	cddaNodePtr->blockDeviceVNodePtr = blockDeviceVNodePtr;
	
	mountPtr->mnt_flag	|= ( MNT_LOCAL | MNT_RDONLY | MNT_DOVOLFS );	// Local Read-Only FileSystem
	mountPtr->mnt_data	= ( qaddr_t ) cddaMountPtr;		// Hang our data off the MountPoint's mnt_data
	
	// Get a filesystem ID for us
	vfs_getnewfsid ( mountPtr );
		
	// Allocate memory for xml data
	MALLOC ( xmlData, void *, cddaArgs.xmlFileSize, M_TEMP, M_WAITOK );
	
	error = copyin ( cddaArgs.xmlData, ( caddr_t ) xmlData, cddaArgs.xmlFileSize );
	if ( error != 0 )
	{
		
		goto FREE_ROOT_DIRECTORY;
		
	}

	// Parse the TOC of the CD and build the tracks
	error = ParseTOC ( mountPtr, cddaMountPtr->numTracks, cddaArgs.xmlFileSize, xmlData, theProcPtr );
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Mount: Error = %d returned from ParseTOC.\n", error ) );
		goto FREE_ROOT_DIRECTORY;
		
	}
	
	// Unlock the root vnode
	VOP_UNLOCK ( cddaMountPtr->root, 0, theProcPtr );
	
	DebugLog ( ( "CDDA_Mount: Exiting CDDA_Mount.\n" ) );
	
	// Grab the lock so nothing else touches our global while we do
	simple_lock ( &gCDDANumberOfInstancesLock );
	
	// Increment the number of instances of this filesystem since we are successfully
	// mounting here
	gCDDANumberOfInstances++;
 
	// Unlock our lock
	simple_unlock ( &gCDDANumberOfInstancesLock );	
	
	return ( 0 );


FREE_ROOT_DIRECTORY:
	
	if ( cddaMountPtr->root != NULL )
	{
		
		// Following what's done in CDDA_Unmount until advised otherwise
		vrele ( cddaMountPtr->root );
		vgone ( cddaMountPtr->root );
		
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


RELEASE_DEV_NODE_ERROR:
	
	vput ( blockDeviceVNodePtr );
	

ERROR:
	
	return error;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Start -	This routine is responsible for binding and does nothing
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Start ( struct mount * mountPtr,
			 int theFlags,
			 struct proc * theProcPtr )
{

	DebugLog ( ( "CDDA_Start: Entering.\n" ) );

	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( theProcPtr != NULL ) );

	// Stub code for use when necessary

	DebugLog ( ( "CDDA_Start: exiting...\n" ) );

	return ( 0 );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Unmount -	This routine is called to unmount the disc at the
//					specified mount point
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Unmount ( struct mount * mountPtr,
			   int theFlags,
			   struct proc * theProcPtr )
{
	
	struct vnode *			rootVNodePtr		= NULL;
	struct vnode *			xmlVNodePtr			= NULL;
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	AppleCDDANodePtr		cddaNodePtr			= NULL;
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr	= NULL;
	int						error				= 0;
	int						flags				= 0;
	
	DebugLog ( ( "CDDA_Unmount: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( theProcPtr != NULL ) );
	
	cddaMountPtr = VFSTOCDDA ( mountPtr );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	rootVNodePtr = cddaMountPtr->root;
	DebugAssert ( ( rootVNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( rootVNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	xmlVNodePtr = cddaMountPtr->xmlFileVNodePtr;
	DebugAssert ( ( xmlVNodePtr != NULL ) );
	
	if ( theFlags & MNT_FORCE )
	{

		DebugLog ( ( "CDDA_Unmount: Setting forceclose.\n" ) );
		flags |= FORCECLOSE;
		
	}
	
	DebugAssert ( ( rootVNodePtr != NULL ) );
	DebugLog ( ( "root node's v_usecount = %d\n", rootVNodePtr->v_usecount ) );
	
	// cheat a little :-)
	// The XML file has a reference from the kernel, so
	// set 1 in the tookref parameter of ubc_isinuse()
	// if the kernel is the only user we can skip this in vflush()
	
	if ( ( UBCISVALID ( xmlVNodePtr ) && !ubc_isinuse ( xmlVNodePtr, 1 ) ) ||
		 !UBCINFOEXISTS ( xmlVNodePtr ) )
	{

		DebugLog ( ( "CDDA_Unmount: setting SKIPSYSTEM.\n" ) );
		
		// Set the VSYSTEM flag for now as the cheat and pass SKIPSYSTEM
		// in the flags to vflush() so that it skips this vnode for the XML
		// file
		xmlVNodePtr->v_flag |= VSYSTEM;
		flags |= SKIPSYSTEM;
		
	}
	
	// Call vflush to take care of other vnodes
	error = vflush ( mountPtr, rootVNodePtr, flags );
	
	// Undo the cheat!
	xmlVNodePtr->v_flag &= ~VSYSTEM;
	
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Unmount: Returning error = %d after vflush.\n", error ) );
		return ( error );
	
	}
	
	if ( rootVNodePtr->v_usecount > 1 && ( flags & FORCECLOSE != FORCECLOSE ) )
	{
	
		DebugLog ( ( "CDDA_Unmount: Returning error = %d.\n", EBUSY ) );
		return ( EBUSY );
	
	}

	DebugLog ( ( "CDDA_Unmount: Closing block device.\n" ) );
	
	// Remove the mounted on flag from the device
	cddaNodePtr->blockDeviceVNodePtr->v_specflags &= ~SI_MOUNTEDON;
	
	// Close the reference on the device
	( void ) VOP_CLOSE ( cddaNodePtr->blockDeviceVNodePtr,
						FREAD, NOCRED, theProcPtr );
	
	// Release the underlying device vnode
	vrele ( cddaNodePtr->blockDeviceVNodePtr );
	
	DebugLog ( ( "CDDA_Unmount: Killing the XML file.\n" ) );
	
	// Release the xml file vnode ( we got a refcount
	// to it when we called getnewvnode )
	vrele ( xmlVNodePtr );
	
	DebugLog ( ( "CDDA_Unmount: released the XML file.\n" ) );
	
	// Release the underlying root vnode
	vrele ( rootVNodePtr );
	
	DebugLog ( ( "CDDA_Unmount: released the root vnode.\n" ) );
	
	// Reduce, Reuse, Recycle!
	vgone ( rootVNodePtr );
	
	DebugLog ( ( "CDDA_Unmount: All vnodes killed!\n" ) );
	
	// Get a pointer to the NodeInfo Array	
	nodeInfoArrayPtr = VFSTONODEINFO ( mountPtr );

	DebugLog ( ( "CDDA_Unmount: Free the nodeinfo array.\n" ) );
	
	// Free the NodeInfo Array we allocated at mount time
	FREE ( nodeInfoArrayPtr, M_TEMP );

	DebugLog ( ( "CDDA_Unmount: Free the mount point data.\n" ) );

	// Finally, free the mount-specific data we allocated at mount time
	FREE ( mountPtr->mnt_data, M_TEMP );
	
	// Point the pointer to nothing
	mountPtr->mnt_data = NULL;
		
	// Grab the lock so nothing touches our global while we do
	simple_lock ( &gCDDANumberOfInstancesLock );
	
	// Decrement number of instances of the filesystem since we are successfully
	// unmounting here
	gCDDANumberOfInstances--;
	
	// Unlock our lock
	simple_unlock ( &gCDDANumberOfInstancesLock );	
	
	DebugLog ( ( "CDDA_Unmount: Exiting, returning error = %d.\n", error ) );

	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Root - This routine is called to get a vnode pointer to the root
//				vnode of the filesystem
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Root ( struct mount * mountPtr,
			struct vnode ** vNodeHandle )
{
	
	int		error	= 0;
	SInt32	inode	= kAppleCDDARootFileID;
	
	DebugLog ( ( "CDDA_Root: Entering.\n" ) );

	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	
	error = CDDA_VGet ( mountPtr, ( void * ) &inode, vNodeHandle );
	
	DebugLog ( ( "CDDA_Root: exiting...\n" ) );
	
	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Statfs -	This routine is called to get filesystem statistics
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Statfs ( struct mount * mountPtr,
			  struct statfs * statFSPtr,
			  struct proc * theProcPtr )
{
	
	AppleCDDAMountPtr	cddaMountPtr	= NULL;
	AppleCDDANodePtr	rootCDDANodePtr = NULL;
	
	DebugLog ( ( "CDDA_Statfs: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( statFSPtr != NULL ) );
	DebugAssert ( ( theProcPtr != NULL ) );
	
	cddaMountPtr = VFSTOCDDA ( mountPtr );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	rootCDDANodePtr = VTOCDDA ( cddaMountPtr->root );
	DebugAssert ( ( rootCDDANodePtr != NULL ) );
	
	DebugAssert ( ( rootCDDANodePtr->nodeType == kAppleCDDADirectoryType ) );
	
	statFSPtr->f_flags	= 0;
	statFSPtr->f_bsize	= kPhysicalMediaBlockSize;
	statFSPtr->f_iosize = PAGE_SIZE;	// Lie to system and tell it we like 4K I/Os
	statFSPtr->f_bfree	= 0;			// No free blocks since we're a CD-ROM
	statFSPtr->f_bavail = 0;			// No available blocks since we're a CD-ROM
	statFSPtr->f_ffree	= 0;
	statFSPtr->f_files	= rootCDDANodePtr->u.directory.entryCount;
	statFSPtr->f_blocks = rootCDDANodePtr->u.directory.directorySize / kPhysicalMediaBlockSize;
	
	DebugLog ( ( "CDDA_Statfs: f_files = %ld.\n", statFSPtr->f_files ) );
	DebugLog ( ( "CDDA_Statfs: f_blocks = %ld.\n", statFSPtr->f_blocks ) );
	
	// Is the statfs structure the same as that hung off our mount point?
	if ( statFSPtr != &mountPtr->mnt_stat )
	{
		
		// No, copy the filesystem type into the other structure
		statFSPtr->f_type = mountPtr->mnt_vfc->vfc_typenum;
		
		// Copy the filesystem ID, mounted on and mounted from fields
		bcopy ( &mountPtr->mnt_stat.f_fsid, &statFSPtr->f_fsid, sizeof ( statFSPtr->f_fsid ) );
		bcopy ( mountPtr->mnt_stat.f_mntonname, statFSPtr->f_mntonname, MNAMELEN );
		bcopy ( mountPtr->mnt_stat.f_mntfromname, statFSPtr->f_mntfromname, MNAMELEN );
		
	}
	
	// Copy the filesystem name
	strncpy ( statFSPtr->f_fstypename, mountPtr->mnt_vfc->vfc_name, ( MFSNAMELEN - 1 ) );
	statFSPtr->f_fstypename[( MFSNAMELEN - 1 )] = '\0';
	
	DebugLog ( ( "CDDA_Statfs: Exiting...\n" ) );
	
	return ( 0 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_VGet - This routine is responsible for getting the desired vnode.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_VGet ( struct mount * mountPtr,
			void * ino,
			struct vnode ** vNodeHandle )
{
	
	SInt32					nodeID				= 0;
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	AppleCDDANodePtr		parentCDDANodePtr	= NULL;
	struct proc *			theProcPtr			= NULL;
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr	= NULL;
	struct vnode *			vNodePtr			= NULL;
	int						error				= 0;
	int						index				= 0;
	
	DebugLog ( ( "CDDA_VGet: Entering.\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( ino != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	
	cddaMountPtr = VFSTOCDDA ( mountPtr );
	
	DebugAssert ( ( cddaMountPtr != NULL ) );

	theProcPtr = current_proc ( );
	DebugAssert ( ( theProcPtr != NULL ) );
	
	// Check if unmount in progress
	if ( mountPtr->mnt_kern_flag & MNTK_UNMOUNT )
	{
		
		*vNodeHandle = NULL;
		return ( EPERM );
		
	}
			
	nodeID = *( SInt32 * ) ino;
	if ( nodeID == kAppleCDDARootFileID )
	{
		
		DebugLog ( ( "Root vnode asked for!\n" ) );
		
		error = vget ( cddaMountPtr->root, LK_EXCLUSIVE | LK_RETRY, theProcPtr );
		if ( error != 0 )
		{
			
			DebugLog ( ( "CDDA_VGet: exiting with error = %d after vget.\n", error ) );
			goto Exit;
			
		}
		
		// Get the root vnode pointer
		*vNodeHandle = cddaMountPtr->root;
		
	}
	
	else if ( nodeID == kAppleCDDAXMLFileID )
	{
		
		DebugLog ( ( "XML vnode asked for!\n" ) );
		error = vget ( cddaMountPtr->xmlFileVNodePtr, LK_EXCLUSIVE | LK_RETRY, theProcPtr );
		if ( error != 0 )
		{
			
			DebugLog ( ( "CDDA_VGet: exiting with error = %d after vget.\n", error ) );
			goto Exit;
			
		}
		
		*vNodeHandle = cddaMountPtr->xmlFileVNodePtr;
		
	}
	
	else
	{
		
		parentCDDANodePtr = VTOCDDA ( cddaMountPtr->root );
		
		// subtract our file offset to get to the real nodeID
		nodeID -= kOffsetForFiles;
		
		// Look in our NodeInfo array to see if a vnode has been created for this
		// track yet.
		nodeInfoArrayPtr = VFSTONODEINFO ( mountPtr );
		DebugAssert ( ( nodeInfoArrayPtr != NULL ) );
		
		
	LOOP:
		
		
		DebugLog ( ( "Locking nodeInfoLock.\n" ) );
		DebugLog ( ( "Looking for nodeID = %ld.\n", nodeID ) );
		
		error = lockmgr ( &cddaMountPtr->nodeInfoLock, LK_EXCLUSIVE, NULL, theProcPtr );
		
		index = 0;
		
		while ( index < ( parentCDDANodePtr->u.directory.entryCount - kNumberOfFakeDirEntries ) )
		{
			
			if ( nodeInfoArrayPtr->trackDescriptor.point == ( UInt8 ) nodeID )
			{
				
				// See if the vNodePtr was attached (vNodePtr is only non-NULL if the node has been created)
				if ( nodeInfoArrayPtr->vNodePtr != NULL )
				{
					
					// If the vnode was attached, the vNode was created already
					// so set the pointer to that address
					vNodePtr = nodeInfoArrayPtr->vNodePtr;
					
					DebugLog ( ( "Releasing nodeInfoLock.\n" ) );
					
					// Release the lock on our nodeInfo structure first
					error = lockmgr ( &cddaMountPtr->nodeInfoLock, LK_RELEASE, NULL, theProcPtr );
					
					// vget the vnode to up the refcount and lock it
					error = vget ( vNodePtr, LK_EXCLUSIVE, theProcPtr );
					if ( error != 0 )
					{
						
						DebugLog ( ( "CDDA_VGet: exiting with error = %d after vget.\n", error ) );
						goto LOOP;
						
					}
					
					// Stuff the vnode in
					*vNodeHandle = vNodePtr;
					
					// The specified vNode was found and successfully acquired
					goto Exit;
					
				}
				
				else
				{
					
					int		error2 = 0;
					
					DebugLog ( ( "Couldn't find the vnode...Calling CreateNewCDDAFile\n" ) );
					
					DebugLog ( ( "Releasing nodeInfoLock.\n" ) );
					
					// Now we can release our lock because we're creating a node
					error = lockmgr ( &cddaMountPtr->nodeInfoLock, LK_RELEASE, NULL, theProcPtr );
	
					// if we get here, it doesn't exist yet, so create it
					error2 = CreateNewCDDAFile ( mountPtr,
												nodeInfoArrayPtr->trackDescriptor.point + kOffsetForFiles,
												nodeInfoArrayPtr,
												theProcPtr,
												&vNodePtr );
	
					DebugLog ( ( "Locking nodeInfoLock.\n" ) );
					error = lockmgr ( &cddaMountPtr->nodeInfoLock, LK_EXCLUSIVE, NULL, theProcPtr );
					
					// Make sure we mark this vnode as being in the array now
					nodeInfoArrayPtr->vNodePtr = vNodePtr;
					
					// Now we can release our lock because we're getting out
					error = lockmgr ( &cddaMountPtr->nodeInfoLock, LK_RELEASE, NULL, theProcPtr );
					
					if ( error != 0 || error2 != 0 )
					{
						
						if ( error == 0 )
							error = error2;
						
						goto Exit;
						
					}
					
					// Stuff the vnode in
					*vNodeHandle = vNodePtr;
					
					// The specified vNode was found and successfully acquired
					goto Exit;
					
				}
				
			}
			
			index++;
			nodeInfoArrayPtr++;
			
		}
		
		DebugLog ( ( "Releasing nodeInfoLock...About to return ENOENT.\n" ) );
		
		// Now we can release our lock because we're getting out
		error = lockmgr ( &cddaMountPtr->nodeInfoLock, LK_RELEASE, NULL, theProcPtr );
		
		// If we get here, we couldn't find anything with that name. Return ENOENT
		return ( ENOENT );
		
	}
	
	
Exit:
	
	if ( *vNodeHandle == NULL )
		panic ( "*vNodeHandle == NULL" );
	
	DebugLog ( ( "CDDA_VGet: exiting...\n" ) );
	
	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_FileHandleToVNodePtr - This routine is responsible for converting
//								a file handle to a vnode pointer. It is not
//								supported because this is not a UFS or NFS
//								volume.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_FileHandleToVNodePtr ( struct mount * mountPtr,
							struct fid * fileHandlePtr,
							struct mbuf * networkAddressPtr,
							struct vnode ** vNodeHandle,
							int * exportFlagsPtr,
							struct ucred ** anonymousCredHandle )
{
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( fileHandlePtr != NULL ) );
	DebugAssert ( ( networkAddressPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	DebugAssert ( ( exportFlagsPtr != NULL ) );
	DebugAssert ( ( anonymousCredHandle != NULL ) );

	return ( EOPNOTSUPP );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_QuotaControl - This routine is responsible for handling quotas.
//						It is not supported by this filesystem.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_QuotaControl ( struct mount * mountPtr,
					int commands,
					uid_t userID,
					caddr_t arguments,
					struct proc * theProcPtr )
{
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( theProcPtr != NULL ) );
	
	return ( EOPNOTSUPP );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Synchronize -	This routine is responsible for handling flushing of
//						data that is in the dirty buffers to disk. Since this
//						is a read-only filesystem, this call is unnecessary.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Synchronize ( struct mount * mountPtr,
				   int waitForIOCompletion,
				   struct ucred * userCredPtr,
				   struct proc * theProcPtr )
{

	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( userCredPtr != NULL ) );
	DebugAssert ( ( theProcPtr != NULL ) );

	return ( 0 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_SystemControl -	This routine is responsible for FastFileSystem.
//							It is not supported because this is not a UFS or
//							NFS volume.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_SystemControl ( int * name,
					 u_int nameLength,
					 void * oldPtr,
					 size_t * oldLengthPtr,
					 void * newPtr,
					 size_t newLength,
					 struct proc * theProcPtr )
{
	
	DebugAssert ( ( name != NULL ) );
	DebugAssert ( ( oldPtr != NULL ) );
	DebugAssert ( ( oldLengthPtr != NULL ) );
	DebugAssert ( ( newPtr != NULL ) );
	DebugAssert ( ( theProcPtr != NULL ) );

	return ( EOPNOTSUPP );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_VNodePtrToFileHandle - This routine is responsible for converting
//								vnode pointer a to a file handle. It is not
//								supported because this is not a UFS or NFS
//								volume.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_VNodePtrToFileHandle ( struct vnode * vNodePtr,
							struct fid * fileHandlePtr )
{
	
	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( fileHandlePtr != NULL ) );
	
	return ( EOPNOTSUPP );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Apple_CDDA_FS_Module_Start -	This routine is responsible for
//									all the initialization that would
//									ordinarily be done as part of the
//									system startup
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
Apple_CDDA_FS_Module_Start ( int loadArgument )
{

	#pragma unused ( loadArgument )

	struct vfsconf *				newVFSConf				= NULL;
	int								error					= 0;
	int								index					= 0;
	struct vnodeopv_entry_desc *	opVectorEntryDescPtr	= NULL;
	boolean_t						funnel_state;
	
	int ( ***opv_desc_vector_p ) ( );
	int ( **opv_desc_vector ) ( );
	
	funnel_state = thread_funnel_set ( kernel_flock, TRUE );
	
	DebugLog ( ( "Apple_CDDA_FS_Module_Start: Entering...\n" ) );
	
	// Create the vfs config structure
	MALLOC ( newVFSConf, void * , sizeof ( struct vfsconf ), M_TEMP, M_WAITOK );
	
	// Zero out the vfs config structure
	bzero ( newVFSConf, sizeof ( struct vfsconf ) );
	
	// Init the global gCDDANumberOfInstancesLock
	simple_lock_init ( &gCDDANumberOfInstancesLock );

	// Set the vfs operations to point to our vfs operations
	newVFSConf->vfc_vfsops = &gCDDA_VFSOps;
	
	// Copy our filesystem's name into the structure
	strncpy ( &newVFSConf->vfc_name[0], gAppleCDDAName, MFSNAMELEN );
	
	// Fill in the rest of the structure
	newVFSConf->vfc_typenum		= maxvfsconf++; // ¥¥¥ Bad to use a system global here!!
	newVFSConf->vfc_refcount	= 0;
	newVFSConf->vfc_flags		= 0;
	newVFSConf->vfc_mountroot	= NULL;			// Can't mount as root
	newVFSConf->vfc_next		= NULL;
	
	// set the operations vector description vector pointer
	opv_desc_vector_p = gCDDA_VNodeOperationsDesc.opv_desc_vector_p;
		
	// Allocate and init the vector. Also handle backwards compatibility
	MALLOC ( *opv_desc_vector_p, PFI *, vfs_opv_numops * sizeof ( PFI ), M_TEMP,
			 M_WAITOK );
	
	// Zero the structure
	bzero ( *opv_desc_vector_p, vfs_opv_numops * sizeof ( PFI ) );
	
	// Point the structure to our descriptor
	opv_desc_vector = *opv_desc_vector_p;
	
	for ( index = 0; gCDDA_VNodeOperationsDesc.opv_desc_ops[index].opve_op; index++ )
	{
		
		opVectorEntryDescPtr = &( gCDDA_VNodeOperationsDesc.opv_desc_ops[index] );
		
		//	Sanity check:  is this operation listed in the list of operations? We check this
		//	by seeing if its offest is zero. Since the default routine should always be listed
		//	first, it should be the only one with a zero offset. Any other operation with a
		//	zero offset is probably not listed in vfs_op_descs, and so is probably an error.
		
		//	A panic here means the layer programmer has committed the all-too common bug
		//	of adding a new operation to the layer's list of vnode operations but
		//	not adding the operation to the system-wide list of supported operations.
		if ( opVectorEntryDescPtr->opve_op->vdesc_offset == 0 &&
			 opVectorEntryDescPtr->opve_op->vdesc_offset != VOFFSET ( vop_default ) )
		{

			DebugLog ( ( "Apple_CDDA_FS_Module_Start: operation %s not listed in %s.\n",
						opVectorEntryDescPtr->opve_op->vdesc_name, "vfs_op_descs" ) );
			panic ( "Apple_CDDA_FS_Module_Start: bad operation" );
			
		}
		
		// Fill in this entry
		opv_desc_vector[opVectorEntryDescPtr->opve_op->vdesc_offset] =
			opVectorEntryDescPtr->opve_impl;
		
	}
	
	// Finally, go back and replace unfilled routines with their default.  (Sigh, an O(n^3)
	// algorithm. I could make it better, but that'd be work, and n is small.)
	
	opv_desc_vector_p = gCDDA_VNodeOperationsDesc.opv_desc_vector_p;
	
	// Force every operations vector to have a default routine
	opv_desc_vector = *opv_desc_vector_p;
	
	if ( opv_desc_vector[VOFFSET( vop_default )] == NULL )
	{
	
		panic ( "Apple_CDDA_FS_Module_Start: operation vector without default routine." );
	
	}
	
	for ( index = 0; index < vfs_opv_numops; index++ )
	{
		
		if ( opv_desc_vector[index] == NULL )
		{
		
			opv_desc_vector[index] = opv_desc_vector[VOFFSET( vop_default )];
		
		}
		
	}
	
	// Ok, vnode vectors are set up, vfs vectors are set up, add it in
	error = vfsconf_add ( newVFSConf );
	if ( error != 0 )
	{
	
		DebugLog ( ( "Apple_CDDA_FS_Module_Start: Error = %d while adding vfsconf.\n", error ) );
	
	}
	
	if ( newVFSConf != NULL )
	{
		
		// It copied our stuff in on the vfsconf_add, so we can dispose of
		// our vfs config structure now
		FREE ( newVFSConf, M_TEMP );
		newVFSConf = NULL;
		
	}
	
	DebugLog ( ( "Apple_CDDA_FS_Module_Start: exiting...\n" ) );
	
	( void ) thread_funnel_set ( kernel_flock, funnel_state );
	
	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Apple_CDDA_FS_Module_Stop - This routine is responsible for stopping
//								filesystem services
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
Apple_CDDA_FS_Module_Stop ( int unloadArgument )
{
	
	int				error = 0;
	boolean_t		funnel_state;
	
	funnel_state = thread_funnel_set ( kernel_flock, TRUE );
	
	DebugLog ( ( "Apple_CDDA_FS_Module_Stop: Entering.\n" ) );
	
	// Grab the lock so nothing touches our global while we do
	simple_lock ( &gCDDANumberOfInstancesLock );
	
	// Check if there are any instances of our filesystem lying around
	if ( gCDDANumberOfInstances > 0 )
	{
		
		// Yes there are, so return an error
		error = EBUSY;
		
		// Unlock our lock
		simple_unlock ( &gCDDANumberOfInstancesLock );	
		( void ) thread_funnel_set ( kernel_flock, funnel_state );
		
		return error;
		
	}
	
	// Unlock our lock
	simple_unlock ( &gCDDANumberOfInstancesLock );	
	
	// Delete us from the vfs config table
	error = vfsconf_del ( gAppleCDDAName );
	if ( error != 0 )
	{
		
		DebugLog ( ( "Error = %d while deleting from vfsconf.\n", error ) );
		
	}
	
	// Free the memory associated with our operations vector
	FREE ( *gCDDA_VNodeOperationsDesc.opv_desc_vector_p, M_TEMP );
	
	DebugLog ( ( "Apple_CDDA_FS_Module_Stop: exiting...\n" ) );
	
	( void ) thread_funnel_set ( kernel_flock, funnel_state );
	
	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//				End				Of			File
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ