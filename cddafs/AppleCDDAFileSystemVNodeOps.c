/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

// AppleCDDAFileSystemVNodeOps.c created by CJS on Mon 10-Apr-2000

// Project Includes
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

#ifndef __APPLE_CDDA_FS_VFS_OPS_H__
#include "AppleCDDAFileSystemVFSOps.h"
#endif

// System Includes
#include <vm/vm_pageout.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/paths.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/errno.h>
#include <sys/ubc.h>
#include <vfs/vfs_support.h>

#include <miscfs/specfs/specdev.h>

extern int		strcmp __P  	( ( const char *, const char * ) );					// Kernel already includes a copy
extern int		strncmp __P  	( ( const char *, const char *, size_t length ) );	// Kernel already includes a copy
extern char *	strcpy __P  	( ( char *, const char * ) );						// Kernel already includes a copy
extern char *	strncpy __P 	( ( char *, const char *, size_t length ) );		// Kernel already includes a copy
extern char *	strchr __P  	( ( const char *, int ) );							// Kernel already includes a copy
extern int		atoi __P		( ( const char * ) );								// Kernel already includes a copy

#define RENAME_SUPPORTED 0


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Static Function Prototypes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static SInt32
AddDirectoryEntry ( UInt32 nodeID, UInt8 type, const char * name, struct uio * uio );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AddDirectoryEntry -	This routine adds a directory entry to the uio buffer
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static SInt32
AddDirectoryEntry ( UInt32 nodeID,
					UInt8 type,
					const char * name,
					struct uio * uio )
{
	
	struct dirent		directoryEntry;
	SInt32 				nameLength				= 0;
	UInt16 				directoryEntryLength	= 0;
	
	DebugAssert ( ( name != NULL ) );
	DebugAssert ( ( uio != NULL ) );

	DebugLog ( ( "fileName = %s\n", name ) );
	
	nameLength = strlen ( name );		
	DebugAssert ( ( nameLength < MAXNAMLEN + 1 ) );
	
	directoryEntry.d_fileno = nodeID;
	directoryEntry.d_reclen = sizeof ( directoryEntry );
	directoryEntry.d_type 	= type;
	directoryEntry.d_namlen = nameLength;
	directoryEntryLength	= directoryEntry.d_reclen;
	
	// Copy the string
	strncpy ( directoryEntry.d_name, name, MAXNAMLEN );
	
	// Zero the rest of the array for safe-keeping
	bzero ( &directoryEntry.d_name[nameLength], MAXNAMLEN + 1 - nameLength );
	
	if ( uio->uio_resid < directoryEntry.d_reclen )
	{
		
		// We can't copy because there isn't enough room in the buffer,
		// so set the directoryEntryLength to zero so the caller knows
		// an error occurred
		directoryEntryLength = 0;
		
	}
	
	else
	{
		
		// Move the data
		uiomove ( ( caddr_t ) &directoryEntry, sizeof ( directoryEntry ), uio );
				
	}
	
	return directoryEntryLength;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Lookup -	This routine performs a lookup
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Lookup ( struct vop_lookup_args * lookupArgsPtr )
/*
struct vop_lookup_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
};
*/
{
	
	struct componentname *		compNamePtr 		= NULL;
	struct vnode **				vNodeHandle 		= NULL;
	struct vnode *				parentVNodePtr 		= NULL;
	struct vnode *				vNodePtr 			= NULL;
	struct proc *				theProcPtr 			= NULL;
	AppleCDDANodePtr			parentCDDANodePtr	= NULL;
	AppleCDDAMountPtr			cddaMountPtr		= NULL;
	AppleCDDANodeInfoPtr		nodeInfoArrayPtr	= NULL;
	UInt32						index				= 0;
	int 						error				= 0;
	int 						flags				= 0;
	int							lockParent			= 0;
	
	DebugLog ( ( "CDDA_Lookup: Entering.\n" ) );
	
	DebugAssert ( ( lookupArgsPtr != NULL ) );
	
	compNamePtr 	= lookupArgsPtr->a_cnp;
	vNodeHandle 	= lookupArgsPtr->a_vpp;
	parentVNodePtr 	= lookupArgsPtr->a_dvp;
	
	DebugAssert ( ( compNamePtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	DebugAssert ( ( parentVNodePtr != NULL ) );
	
	theProcPtr 			= compNamePtr->cn_proc;
	parentCDDANodePtr	= VTOCDDA ( parentVNodePtr );
	cddaMountPtr 		= VFSTOCDDA ( parentVNodePtr->v_mount );
	
	DebugAssert ( ( theProcPtr != NULL ) );
	DebugAssert ( ( parentCDDANodePtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	*vNodeHandle 	= NULL;
	flags			= compNamePtr->cn_flags;
	lockParent		= flags & LOCKPARENT;


#if RENAME_SUPPORTED
	
	// Check if process wants to create, delete or rename anything
	if ( compNamePtr->cn_nameiop == CREATE )
	{
		
		DebugLog ( ( "Can't CREATE %s, returning EROFS\n", compNamePtr->cn_nameptr ) );
		error = EROFS;
		goto Exit;
		
	}
	
	// Check if process wants to create, delete or rename anything
	if ( compNamePtr->cn_nameiop == RENAME )
	{
		
		DebugLog ( ( "Can't RENAME %s, returning EJUSTRETURN\n", compNamePtr->cn_nameptr ) );
		error = EJUSTRETURN;
		goto Exit;
		
	}

	// Check if process wants to create, delete or rename anything
	if ( compNamePtr->cn_nameiop == DELETE )
	{
		
		DebugLog ( ( "Can't DELETE %s\n", compNamePtr->cn_nameptr ) );
		
	}
	
#else /* if ( RENAME_SUPPORTED = 0 ) */
	
	// Check if process wants to create, delete or rename anything
	if ( compNamePtr->cn_nameiop == CREATE ||
		 compNamePtr->cn_nameiop == RENAME ||
		 compNamePtr->cn_nameiop == DELETE )
	{
		
		DebugLog ( ( "Can't CREATE %s, returning EROFS\n", compNamePtr->cn_nameptr ) );
		error = EROFS;
		goto Exit;
		
	}
	
#endif
	
	
	// Determine if we're looking for a resource fork.
	// note: this could cause a read off the end of the component name buffer in some rare cases.
	if ( ( flags & ISLASTCN ) == 0 && bcmp ( &compNamePtr->cn_nameptr[compNamePtr->cn_namelen],
											 _PATH_RSRCFORKSPEC,
											 sizeof ( _PATH_RSRCFORKSPEC ) - 1 ) == 0 )
	{
		
		DebugLog ( ( "No resource forks available, return ENOTDIR.\n" ) );
		compNamePtr->cn_consume = sizeof ( _PATH_RSRCFORKSPEC ) - 1;
		error = ENOTDIR;
		goto Exit;
		
	}
	
	if ( parentVNodePtr->v_type != VDIR )
	{
		
		DebugLog ( ( "parentVNodePtr->v_type != VDIR, returning ENOTDIR\n" ) );
		error = ENOTDIR;
		goto Exit;
		
	}
	
	DebugLog ( ( "Looking for name = %s.\n", compNamePtr->cn_nameptr ) );
	
	// first check for ".", "..", and ".TOC.plist"
	if ( compNamePtr->cn_nameptr[0] == '.' )
	{
		
		if ( compNamePtr->cn_namelen == 1 )
		{
			
			DebugLog ( ( ". was requested\n" ) );
			
			// "." requested
			vNodePtr = parentVNodePtr;
			VREF ( vNodePtr );
			
			error = 0;
			*vNodeHandle = vNodePtr;
			
			goto Exit;
			
		}
		
		else if ( ( compNamePtr->cn_namelen == 2 ) && ( compNamePtr->cn_nameptr[1] == '.' ) )
		{
			
			panic ( "CDDA_Lookup: namei asked for .. when it shouldn't have" );			
			
		}
		
		else if ( ( compNamePtr->cn_namelen == 10 ) && ( !strncmp ( &compNamePtr->cn_nameptr[1], "TOC.plist", 9 ) ) )
		{
			
			DebugLog ( ( ".TOC.plist was requested\n" ) );
			
			// ".TOC.plist" requested, lock it and return
			error = vget ( cddaMountPtr->xmlFileVNodePtr, LK_EXCLUSIVE, theProcPtr );
			if ( error != 0 )
			{
				
				DebugLog ( ( "CDDA_Lookup: exiting with error = %d after vget.\n", error ) );
				goto Exit;
				
			}
						
			if ( !lockParent || !( flags & ISLASTCN ) )
			{
				
				// Unlock the parent if the last component name
				VOP_UNLOCK ( parentVNodePtr, 0, theProcPtr );
				
			}
			
			if ( ! ( flags & LOCKLEAF ) )
			{
				
				// Unlock the vnode since they didn't ask for it locked
				VOP_UNLOCK ( cddaMountPtr->xmlFileVNodePtr, 0, theProcPtr );
				
			}
			
			// Stuff the vnode in
			*vNodeHandle = cddaMountPtr->xmlFileVNodePtr;
			
			goto Exit;
			
		}
		
	}
	
	// Now check for the name of the root directory (usually "Audio CD", but could be anything)
	if ( !strncmp ( &compNamePtr->cn_nameptr[1],
		 parentCDDANodePtr->u.directory.name,
		 strlen ( parentCDDANodePtr->u.directory.name ) ) )
	{
		
		DebugLog ( ( "The root directory was requested by name = %s\n", parentCDDANodePtr->u.directory.name ) );
		
		// "." requested
		vNodePtr = parentVNodePtr;
		VREF ( vNodePtr );
		
		error = 0;
		*vNodeHandle = vNodePtr;
		
		goto Exit;
		
	}
	
	// Look in our NodeInfo array to see if a vnode has been created for this
	// track yet.
	nodeInfoArrayPtr = VFSTONODEINFO ( parentVNodePtr->v_mount );
	DebugAssert ( ( nodeInfoArrayPtr != NULL ) );
	
	// Not '.' or "..", so it must be a track we're looking up
	// Look for entries by name (making sure the entry's length matches the
	// compNamePtr's namelen
	
	
LOOP:
	
	
	DebugLog ( ( "Locking nodeInfoLock.\n" ) );
	
	error = lockmgr ( &cddaMountPtr->nodeInfoLock, LK_EXCLUSIVE, NULL, theProcPtr );
	
	index = 0;
	
	while ( index < ( parentCDDANodePtr->u.directory.entryCount - kNumberOfFakeDirEntries ) )
	{
				
		if ( nodeInfoArrayPtr->nameSize == compNamePtr->cn_namelen &&
			 !strcmp ( nodeInfoArrayPtr->name, compNamePtr->cn_nameptr ) )
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

					DebugLog ( ( "CDDA_Lookup: exiting with error = %d after vget.\n", error ) );
					goto LOOP;
				
				}
								
				if ( !lockParent || !( flags & ISLASTCN ) )
				{
					
					// Unlock the parent if the last component name
					VOP_UNLOCK ( parentVNodePtr, 0, theProcPtr );
					
				}
				
				if ( ! ( flags & LOCKLEAF ) )
				{
					
					// Unlock the vnode since they didn't ask for it locked
					VOP_UNLOCK ( vNodePtr, 0, theProcPtr );
				
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
				error2 = CreateNewCDDAFile ( parentVNodePtr->v_mount,
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
					goto Exit;
				
				if ( !lockParent || !( flags & ISLASTCN ) )
				{
					
					// Unlock the parent if the last component name
					VOP_UNLOCK ( parentVNodePtr, 0, theProcPtr );
					
				}

				if ( ! ( flags & LOCKLEAF ) )
				{
					
					// Unlock the vnode since they didn't ask for it locked
					VOP_UNLOCK ( vNodePtr, 0, theProcPtr );
				
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
	
	
	
Exit:
	
	
	DebugLog ( ( "CDDA_Lookup: exiting from Exit with error = %d.\n", error ) );
	
	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Open -	This routine opens a file
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Open ( struct vop_open_args * openArgsPtr )
/*
struct vop_open_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
*/
{

	struct vnode *		vNodePtr 	= NULL;
	int 				error		= 0;
	
	DebugLog ( ( "CDDA_Open: Entering.\n" ) );

	DebugAssert ( ( openArgsPtr != NULL ) );
	
	vNodePtr = openArgsPtr->a_vp;
	DebugAssert ( ( vNodePtr != NULL ) );
		
	// Set the vNodeOperationType to tell the user process if we are going to open a
	// file or a directory
	if ( vNodePtr->v_type != VREG && vNodePtr->v_type != VDIR )
	{
	
		// This should never happen but just in case
		DebugLog ( ( "Error = %d, wrong v_type.\n", ENOTSUP ) );
		error = ENOTSUP;
		goto ERROR;
		
	}
	
	// Turn off speculative read-ahead for our vnodes. The cluster
	// code can't possibly do the right thing when we have possible
	// loss of streaming on CD media.
	vNodePtr->v_flag |= VRAOFF;
	
	
ERROR:


	DebugLog ( ( "CDDA_Open: exiting with error = %d.\n", error ) );

	return ( error );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Close -	This routine closes a file. Since we are a read-only
//					filesystem, we don't have any cleaning up to do.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Close ( struct vop_close_args * closeArgsPtr )
/*
struct vop_close_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflag;
	struct ucred *a_cred;
	struct proc *a_p;
};
*/
{
	
	DebugLog ( ( "CDDA_Close: Entering.\n" ) );

#if DEBUG
	DebugAssert ( ( closeArgsPtr != NULL ) );
#else
	#pragma unused ( closeArgsPtr )
#endif
	
	DebugLog ( ( "CDDA_Close: exiting...\n" ) );
	
	return ( 0 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Read -	This routine reads from a file
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Read ( struct vop_read_args * readArgsPtr )
/*
struct vop_read_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
*/
{
	
	register struct vnode * 	vNodePtr 			= NULL;
	register struct uio *		uio 				= NULL;
	AppleCDDANodePtr			cddaNodePtr			= NULL;
	int							error				= 0;
	
	DebugLog ( ( "CDDA_Read: Entering.\n" ) );
	
	DebugAssert ( ( readArgsPtr ) );
	
	vNodePtr	= readArgsPtr->a_vp;
	uio			= readArgsPtr->a_uio;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( uio != NULL ) );
	DebugAssert ( ( UBCINFOEXISTS ( vNodePtr ) != 0 ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	cddaNodePtr->flags |= kAppleCDDAAccessedMask;
	
	// Check to make sure we're operating on a regular file
	if ( vNodePtr->v_type != VREG )
	{

		DebugLog ( ( "CDDA_Read: not a file, exiting with error = %d.\n", EISDIR ) );
		return ( EISDIR );

	}
	
	// Check to make sure they asked for data
	if ( uio->uio_resid == 0 )
	{
		
		DebugLog ( ( "CDDA_Read: uio_resid = 0, no data requested" ) );
		return ( 0 );
		
	}
	
	// Can't read from a negative offset
	if ( uio->uio_offset < 0 )
	{
		
		DebugLog ( ( "CDDA_Read: Can't read from a negative offset..." ) );
		return ( EINVAL );
		
	}

	if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
	{
		
		off_t		offset			= uio->uio_offset;
		UInt32		amountToCopy	= 0;
		UInt32		numBytes		= 0;
		
		numBytes = cddaNodePtr->u.xmlFile.fileSize;
		
		// Check to make sure we don't read past EOF
		if ( uio->uio_offset > numBytes )
		{
			
			DebugLog ( ( "CDDA_Read: Can't read past end of file..." ) );
			return ( 0 );
			
		}
		
		amountToCopy = ulmin ( uio->uio_resid, numBytes - offset );
		
		uiomove ( ( caddr_t ) &cddaNodePtr->u.xmlFile.fileDataPtr[offset],
				  amountToCopy,
				  uio );
		
		return ( 0 );
		
	}
	
	else if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
	{
		
		UInt32			headerSize		= 0;
		UInt32			count			= 0;
		UInt32			blockNumber 	= 0;
		off_t			offset			= 0;
		off_t			sectorOffset	= 0;
		struct buf *	bufPtr			= NULL;
		
		offset	= uio->uio_offset;
		
		// Check to make sure we don't read past EOF
		if ( offset > cddaNodePtr->u.file.nodeInfoPtr->numBytes )
		{
			
			DebugLog ( ( "CDDA_Read: Can't read past end of file..." ) );
			return ( 0 );
			
		}
		
		headerSize = sizeof ( cddaNodePtr->u.file.aiffHeader );
		
		// Copy any part of the header that we need to copy.
		if ( offset < headerSize )
		{
			
			UInt32	amountToCopy = 0;
			
			amountToCopy = ulmin ( uio->uio_resid, headerSize - offset );
			
			uiomove ( ( caddr_t ) &cddaNodePtr->u.file.aiffHeader.u.alignedHeader.filler[offset],
				  amountToCopy,
				  uio );
			
			offset += amountToCopy;
			
		}
		
		if ( uio->uio_resid > 0 )
		{
			
			// Adjust offset by the header size so we have a true offset into the media.
			offset -= headerSize;
			sectorOffset = offset % kPhysicalMediaBlockSize;
			blockNumber = ( offset / kPhysicalMediaBlockSize ) + cddaNodePtr->u.file.nodeInfoPtr->LBA;
			
			// Part 1
			// We do the read in 3 parts. First is the portion which is not part of a full 2352 byte block.
			// In some cases, we actually are sector aligned and we skip this part. A lot of times we'll find
			// this sector incore as well, since it was part of third read for the previous I/O.
			if ( sectorOffset != 0 )
			{
				
				// Clip to requested transfer count.
				count = ulmin ( uio->uio_resid, ( kPhysicalMediaBlockSize - sectorOffset ) );
				
				// Read the one sector
				error = bread ( cddaNodePtr->blockDeviceVNodePtr,
								blockNumber,
								kPhysicalMediaBlockSize,
								NOCRED,
								&bufPtr );
				
				if ( error != 0 )
				{
					
					brelse ( bufPtr );
					return ( error );
					
				}
				
				// Move the data from the block into the buffer
				uiomove ( bufPtr->b_data + sectorOffset, count, uio );
				
				// Make sure we mark this bp invalid as we don't need to keep it around anymore
				SET ( bufPtr->b_flags, B_INVAL );
				
				// Release this buffer back into the buffer pool. 
				brelse ( bufPtr );
				
				// Update offset
				blockNumber++;
				
			}
			
			// Part 2
			// Now we execute the second part of the read. This will be the largest chunk of the read.
			// We will read multiple disc blocks up to MAXBSIZE bytes in a loop until we hit a chunk which
			// is less than one block size. That will be read in the third part.
			
			while ( uio->uio_resid > kPhysicalMediaBlockSize )
			{
				
				UInt32		blocksToRead = 0;
				
				// Read in as close to MAXBSIZE chunks as possible
				if ( uio->uio_resid > kMaxBytesPerRead )
				{
					blocksToRead	= kMaxBlocksPerRead;
					count			= kMaxBytesPerRead;
				}
				
				else
				{
					blocksToRead	= uio->uio_resid / kPhysicalMediaBlockSize;
					count			= blocksToRead * kPhysicalMediaBlockSize;
				}
				
				// bread kMaxBlocksPerRead blocks and put them in the cache.
				error = bread ( cddaNodePtr->blockDeviceVNodePtr,
								blockNumber,
								count,
								NOCRED,
								&bufPtr );
				
				if ( error != 0 )
				{
					
					brelse ( bufPtr );
					return ( error );
					
				}
				
				// Move the data from the block into the buffer
				uiomove ( bufPtr->b_data, count, uio );
				
				// Make sure we mark any intermediate buffers as invalid as we don't need
				// to keep them.
				SET ( bufPtr->b_flags, B_INVAL );
				
				// Release this buffer back into the buffer pool. 
				brelse ( bufPtr );
				
				// Update offset
				blockNumber += blocksToRead;
				
			}
			
			// Part 3
			// Now that we have read everything, we read the tail end which is a partial sector.
			// Sometimes we don't need to execute this step since there isn't a tail.
			if ( uio->uio_resid > 0 )
			{
				
				count = uio->uio_resid;
				
				// Read the one sector
				error = bread ( cddaNodePtr->blockDeviceVNodePtr,
								blockNumber,
								kPhysicalMediaBlockSize,
								NOCRED,
								&bufPtr );
				
				if ( error != 0 )
				{
					
					brelse ( bufPtr );
					return ( error );
					
				}
				
				// Move the data from the block into the buffer
				uiomove ( bufPtr->b_data, count, uio );
				
				// Release this buffer back into the buffer pool. 
				brelse ( bufPtr );
				
			}
			
		}
		
	}
	
	DebugLog ( ( "CDDA_Read: exiting.\n" ) );
	
	return ( error );
		
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_ReadDir -	This routine reads the contents of a directory
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_ReadDir ( struct vop_readdir_args * readDirArgsPtr )
/*
struct vop_readdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
	int *a_eofflag;
	int *a_ncookies;
	u_long **a_cookies;
};
*/
{

	struct vnode * 			vNodePtr 			= NULL;
	AppleCDDANodePtr	 	cddaNodePtr			= NULL;
	AppleCDDAMountPtr	 	cddaMountPtr		= NULL;
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr	= NULL;
	struct proc *			theProcPtr			= NULL;
	struct uio *			uio 				= NULL;
	UInt32					index				= 0;
	int 					error				= 0;
	SInt32					offsetValue			= 0;
	UInt32					direntSize			= 0;

	DebugLog ( ( "CDDA_ReadDir: Entering.\n" ) );

	DebugAssert ( ( readDirArgsPtr != NULL ) );

	vNodePtr 	= readDirArgsPtr->a_vp;
	uio 		= readDirArgsPtr->a_uio;

	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( uio != NULL ) );

	// Get the current proc
	theProcPtr = current_proc ( );
	cddaNodePtr = VTOCDDA ( vNodePtr );
	
	DebugAssert ( ( theProcPtr != NULL ) );
	DebugAssert ( ( cddaNodePtr != NULL ) );

	// We don't allow exporting CDDA mounts, and currently local requests do
	// not need cookies.
	if ( readDirArgsPtr->a_ncookies != NULL || readDirArgsPtr->a_cookies != NULL )
	{
		
		DebugLog ( ( "No cookie exporting, exiting with error = %d.\n", EINVAL ) );
		return EINVAL;

	}
	
	// First make sure it is a directory we are dealing with
	if ( vNodePtr->v_type != VDIR )
	{

		DebugLog ( ( "CDDA_ReadDir: not a directory, exiting with error = %d.\n", ENOTDIR ) );
		return ( ENOTDIR );

	}
	
	if ( cddaNodePtr->nodeID != kAppleCDDARootFileID )
	{

		DebugLog ( ( "CDDA_ReadDir: not root directory, exiting with error = %d.\n", EINVAL ) );
		return ( EINVAL );

	}
	
	// Make sure it's all one big buffer
	if ( uio->uio_iovcnt > 1 )
	{

		DebugLog ( ( "More than one buffer, exiting with error = %d.\n", EINVAL ) );
		return ( EINVAL );

	}

	// Make sure we don't return partial entries
	if ( uio->uio_resid < sizeof ( struct dirent ) )
	{

		DebugLog ( ( "resid < dirent size, exiting with error = %d.\n", EINVAL ) );
		return ( EINVAL );

	}
	
	direntSize 	= sizeof ( struct dirent );
	
	// Synthesize '.', "..", and ".TOC.plist"
	if ( uio->uio_offset == 0 )
	{
	
		offsetValue = AddDirectoryEntry ( cddaNodePtr->nodeID, DT_DIR, ".", uio );
		if ( offsetValue == 0 )
		{
		
			DebugLog ( ( "offsetValue is zero, exiting with error = %d.\n", 0 ) );
			return 0;
		
		}
			
	}
		
	if ( uio->uio_offset == direntSize )
	{
		
		offsetValue = AddDirectoryEntry ( cddaNodePtr->nodeID, DT_DIR, "..", uio );
		if ( offsetValue == 0 )
		{
		
			DebugLog ( ( "offsetValue is zero, exiting with error = %d.\n", 0 ) );
			return 0;
		
		}
		
	}
		
	
	if ( uio->uio_offset == direntSize * kAppleCDDARootFileID )
	{
		
		offsetValue += AddDirectoryEntry ( kAppleCDDAXMLFileID, kAppleCDDAXMLFileType, ".TOC.plist", uio );
		if ( offsetValue == 0 )
		{
		
			DebugLog ( ( "offsetValue is zero, exiting with error = %d.\n", 0 ) );
			return 0;
		
		}
		
	}
	
	nodeInfoArrayPtr	= VFSTONODEINFO ( vNodePtr->v_mount );
	cddaMountPtr		= VFSTOCDDA ( vNodePtr->v_mount );

	DebugAssert ( ( nodeInfoArrayPtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	DebugLog ( ( "cddaMountPtr->numTracks = %ld.\n", cddaMountPtr->numTracks ) );
	DebugLog ( ( "buffer size needed = %ld.\n", direntSize * ( cddaMountPtr->numTracks + kNumberOfFakeDirEntries ) ) );
	
	// OK, so much for the fakes.  Now for the "real thing"
	// Loop over all the names in the NameArray to produce directory entries
	for ( index = 0; index < cddaMountPtr->numTracks; index++, nodeInfoArrayPtr++ )
	{
		
		DebugLog ( ( "uio->uio_offset = %ld.\n", uio->uio_offset ) );
		DebugLog ( ( "uio->uio_resid = %ld.\n", uio->uio_resid ) );
		
		if ( uio->uio_offset == direntSize * ( index + kNumberOfFakeDirEntries ) )
		{
			
			DebugLog ( ( "index = %ld.\n", index ) );
								
			// Return this entry
			offsetValue = AddDirectoryEntry ( nodeInfoArrayPtr->trackDescriptor.point,
											  kAppleCDDATrackType,
											  nodeInfoArrayPtr->name,
											  uio );
			
			if ( offsetValue == 0 )
			{
			
				DebugLog ( ( "offsetValue is zero, exiting with error = %d.\n", 0 ) );
				return 0;
			
			}
						
		}
				
	}
		
	if ( readDirArgsPtr->a_eofflag )
	{
		
		DebugLog ( ( "eofflag = %d.\n", ( uio->uio_offset >= direntSize * ( cddaMountPtr->numTracks + kNumberOfFakeDirEntries ) ) ? 1 : 0 ) );
		
		// If we ran all the way through the list, there are no more
		*readDirArgsPtr->a_eofflag = ( uio->uio_offset >= direntSize * ( cddaMountPtr->numTracks + kNumberOfFakeDirEntries ) ) ? 1 : 0;
		error = 0;
		
	}
	
	DebugLog ( ( "CDDA_ReadDir: exiting with error = %d.\n", error ) );
	
	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_PageIn -	This routine handles VM PageIn requests
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_PageIn ( struct vop_pagein_args * pageInArgsPtr )
/*
struct vop_pagein_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	upl_t a_pl;
	vm_offset_t a_pl_offset;
	off_t a_f_offset;
	size_t a_size;
	struct ucred *a_cred;
	int a_flags;
};
*/
{
	
	register struct vnode *		vNodePtr		= NULL;
	AppleCDDANodePtr			cddaNodePtr		= NULL;
	int 		   				error			= 0;
	int							nocommit 		= 0;
	UInt32						numBytes		= 0;
	
	DebugLog ( ( "CDDA_PageIn: Entering.\n" ) );
	
	DebugAssert ( ( pageInArgsPtr != NULL ) );
	
	vNodePtr = pageInArgsPtr->a_vp;
	nocommit = pageInArgsPtr->a_flags & UPL_NOCOMMIT;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	if ( vNodePtr->v_type != VREG )
		panic ( "CDDA_PageIn: vNodePtr not UBC type.\n" );
	
	if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
	{
		
		numBytes = cddaNodePtr->u.xmlFile.fileSize;
				
	}
	
	else if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
	{
	
		numBytes = cddaNodePtr->u.file.nodeInfoPtr->numBytes;
	
	}

	// If they didn't ask for any data, then we are done
	if ( pageInArgsPtr->a_size == 0 )
	{
		
		if ( !nocommit )
		{
			
			ubc_upl_abort_range ( pageInArgsPtr->a_pl,
								  pageInArgsPtr->a_pl_offset,
								  pageInArgsPtr->a_size,
								  UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY );
			
		}
		
		return ( error );
		
	}
	
	// Make sure we aren't reading from a negative offset
	if ( pageInArgsPtr->a_f_offset < 0 )
	{
		
		if ( !nocommit )
		{
			
			ubc_upl_abort_range ( pageInArgsPtr->a_pl,
								  pageInArgsPtr->a_pl_offset,
								  pageInArgsPtr->a_size,
								  UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY );
			
		}
		
		error = EINVAL;
		DebugLog ( ( "CDDA_PageIn: trying to page in from a negative offset.\n" ) );
		
		return ( error );
		
	}
	
	// Check to make sure we don't read past EOF
	if ( pageInArgsPtr->a_f_offset > numBytes )
	{
		
		if ( !nocommit )
		{
			
			ubc_upl_abort_range ( pageInArgsPtr->a_pl,
								  pageInArgsPtr->a_pl_offset,
								  pageInArgsPtr->a_size,
								  UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY );
			
		}
		
		return ( error );
		
	}
	
	// Workaround for faked ".TOC.plist" file
	if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
	{
				
		kern_return_t		kret			= 0;
		vm_offset_t			vmOffsetPtr		= 0;
		off_t				amountToCopy	= 0;
					
		// Map the physical page into the kernel address space
		kret = ubc_upl_map ( pageInArgsPtr->a_pl, &vmOffsetPtr );
		
		// If we got an error or the vmOffsetPtr is zero, panic for now
		if ( kret != KERN_SUCCESS || vmOffsetPtr == 0 )
		{
		
			panic ( "CDDA_PageIn: error mapping buffer into kernel space!" );
		
		}
		
		// Zero fill the page
		bzero ( ( caddr_t )( vmOffsetPtr + pageInArgsPtr->a_pl_offset ), PAGE_SIZE );
		
		amountToCopy = ulmin ( PAGE_SIZE, numBytes - pageInArgsPtr->a_f_offset );
		
		// Copy the file data
		bcopy ( &cddaNodePtr->u.xmlFile.fileDataPtr[pageInArgsPtr->a_f_offset],
				( void * ) vmOffsetPtr,
				amountToCopy );
		
		// Unmap the physical page from the kernel address space
		kret = ubc_upl_unmap ( pageInArgsPtr->a_pl );
		
		// If we got an error, panic for now
		if ( kret != KERN_SUCCESS )
		{
		
			panic ( "CDDA_PageIn: error unmapping buffer from kernel space!" );
		
		}
		
		if ( !nocommit )
		{
			
			// Commit the page to the vm subsystem
			ubc_upl_commit_range ( 	pageInArgsPtr->a_pl,
									pageInArgsPtr->a_pl_offset,
									PAGE_SIZE,
									UPL_COMMIT_FREE_ON_EMPTY | UPL_COMMIT_CLEAR_DIRTY );
			
		}
		
		return 0;
			
	}
	
	else if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
	{
		
		UInt32			headerSize		= 0;
		UInt32			blockNumber 	= 0;
		UInt32			count			= 0;
		off_t			offset			= 0;
		off_t			sectorOffset	= 0;
		off_t			residual		= 0;
		kern_return_t	kret			= 0;
		vm_offset_t		vmOffsetPtr		= 0;
		struct buf *	bufPtr			= NULL;
		
		residual	= pageInArgsPtr->a_size;
		offset		= pageInArgsPtr->a_f_offset;
		
		// Check to make sure we don't read past EOF
		if ( offset > cddaNodePtr->u.file.nodeInfoPtr->numBytes )
		{
			
			DebugLog ( ( "CDDA_PageIn: Can't read past end of file..." ) );
			return ( 0 );
			
		}
		
		headerSize = sizeof ( cddaNodePtr->u.file.aiffHeader );

		// Map the physical pages into the kernel address space
		kret = ubc_upl_map ( pageInArgsPtr->a_pl, &vmOffsetPtr );
		
		// If we got an error or the vmOffsetPtr is zero, panic for now
		if ( kret != KERN_SUCCESS || vmOffsetPtr == 0 )
		{
		
			panic ( "CDDA_PageIn: error mapping buffer into kernel space!" );
		
		}
		
		// Copy any part of the header that we need to copy.
		if ( offset < headerSize )
		{
			
			off_t	amountToCopy	= 0;
			
			amountToCopy = ulmin ( pageInArgsPtr->a_size, headerSize - offset );			
			
			// Copy the header data
			bcopy ( &cddaNodePtr->u.file.aiffHeader.u.alignedHeader.filler[pageInArgsPtr->a_f_offset],
					( void * ) vmOffsetPtr,
					amountToCopy );
			
			offset += amountToCopy;
			residual -= amountToCopy;
			
		}
		
		if ( residual > 0 )
		{
			
			// Adjust offset by the header size so we have a true offset into the media.
			offset -= headerSize;
			sectorOffset = offset % kPhysicalMediaBlockSize;
			blockNumber = ( offset / kPhysicalMediaBlockSize ) + cddaNodePtr->u.file.nodeInfoPtr->LBA;
			
			// Part 1
			// We do the read in 3 parts. First is the portion which is not part of a full 2352 byte block.
			// In some cases, we actually are sector aligned and we skip this part. A lot of times we'll find
			// this sector incore as well, since it was part of third read for the previous I/O.
			if ( sectorOffset != 0 )
			{
				
				// Clip to requested transfer count.
				count = ulmin ( residual, ( kPhysicalMediaBlockSize - sectorOffset ) );
				
				// Read the one sector
				error = bread ( cddaNodePtr->blockDeviceVNodePtr,
								blockNumber,
								kPhysicalMediaBlockSize,
								NOCRED,
								&bufPtr );
				
				if ( error != 0 )
				{
					
					brelse ( bufPtr );
					return ( error );
					
				}
				
				// Copy the data
				bcopy ( bufPtr->b_data + sectorOffset, ( void * ) vmOffsetPtr + offset + headerSize, count );
				
				// Increment/decrement counters
				offset		+= count;
				residual	-= count;
				
				// Make sure we mark this bp invalid as we don't need to keep it around anymore
				SET ( bufPtr->b_flags, B_INVAL );
				
				// Release this buffer back into the buffer pool. 
				brelse ( bufPtr );
				
				// Update offset
				blockNumber++;
				
			}
			
			// Part 2
			// Now we execute the second part of the read. This will be the largest chunk of the read.
			// We will read multiple disc blocks up to MAXBSIZE bytes in a loop until we hit a chunk which
			// is less than one block size. That will be read in the third part.
			
			while ( residual > kPhysicalMediaBlockSize )
			{
				
				UInt32		blocksToRead = 0;
				
				// Read in as close to MAXBSIZE chunks as possible
				if ( residual > kMaxBytesPerRead )
				{
					blocksToRead	= kMaxBlocksPerRead;
					count			= kMaxBytesPerRead;
				}
				
				else
				{
					blocksToRead	= residual / kPhysicalMediaBlockSize;
					count			= blocksToRead * kPhysicalMediaBlockSize;
				}
				
				// bread kMaxBlocksPerRead blocks and put them in the cache.
				error = bread ( cddaNodePtr->blockDeviceVNodePtr,
								blockNumber,
								count,
								NOCRED,
								&bufPtr );
				
				if ( error != 0 )
				{
					
					brelse ( bufPtr );
					return ( error );
					
				}

				// Copy the data
				bcopy ( bufPtr->b_data, ( void * ) vmOffsetPtr + offset + headerSize, count );
				
				// Increment/decrement counters
				offset		+= count;
				residual	-= count;
				
				// Make sure we mark any intermediate buffers as invalid as we don't need
				// to keep them.
				SET ( bufPtr->b_flags, B_INVAL );
				
				// Release this buffer back into the buffer pool. 
				brelse ( bufPtr );
				
				// Update offset
				blockNumber += blocksToRead;
				
			}
			
			// Part 3
			// Now that we have read everything, we read the tail end which is a partial sector.
			// Sometimes we don't need to execute this step since there isn't a tail.
			if ( residual > 0 )
			{
				
				count = residual;
				
				// Read the one sector
				error = bread ( cddaNodePtr->blockDeviceVNodePtr,
								blockNumber,
								kPhysicalMediaBlockSize,
								NOCRED,
								&bufPtr );
				
				if ( error != 0 )
				{
					
					brelse ( bufPtr );
					return ( error );
					
				}
				
				// Copy the data
				bcopy ( bufPtr->b_data, ( void * ) vmOffsetPtr + offset + headerSize, count );
				
				// Increment/decrement counters
				offset		+= count;
				residual	-= count;
				
				// Release this buffer back into the buffer pool. 
				brelse ( bufPtr );
				
			}
			
		}
		
		// Unmap the physical page from the kernel address space
		kret = ubc_upl_unmap ( pageInArgsPtr->a_pl );
		
		// If we got an error, panic for now
		if ( kret != KERN_SUCCESS )
		{
			
			panic ( "CDDA_PageIn: error unmapping buffer from kernel space!" );
			
		}
		
		if ( !nocommit )
		{
			
			// Commit the page to the vm subsystem
			ubc_upl_commit_range ( 	pageInArgsPtr->a_pl,
									pageInArgsPtr->a_pl_offset,
									pageInArgsPtr->a_size,
									UPL_COMMIT_FREE_ON_EMPTY | UPL_COMMIT_CLEAR_DIRTY );
			
		}
		
	}
	
	DebugLog ( ( "CDDA_PageIn: exiting...\n" ) );
	
	return ( error );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_GetAttributes - This routine gets the attributes for a folder/file
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_GetAttributes ( struct vop_getattr_args * getAttrArgsPtr )
/*
struct vop_getattr_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct proc *a_p;
};
*/
{
	
	struct vnode *			vNodePtr 		= NULL;
	struct vattr *			attributesPtr 	= NULL;
	AppleCDDANodePtr		cddaNodePtr		= NULL;
	AppleCDDAMountPtr		cddaMountPtr	= NULL;
	
	DebugLog ( ( "CDDA_GetAttributes: Entering.\n" ) );
	
	DebugAssert ( ( getAttrArgsPtr != NULL ) );
	
	vNodePtr 		= getAttrArgsPtr->a_vp;
	attributesPtr 	= getAttrArgsPtr->a_vap;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( attributesPtr != NULL ) );
	
	cddaMountPtr	= VFSTOCDDA ( vNodePtr->v_mount );
	cddaNodePtr		= VTOCDDA ( vNodePtr );

	DebugAssert ( ( cddaNodePtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );

	DebugLog ( ( "nodeID = %ld.\n", cddaNodePtr->nodeID ) );
	
	// Set filesystem ID since we called vfs_getnewfsid() on mount
	attributesPtr->va_fsid		= vNodePtr->v_mount->mnt_stat.f_fsid.val[0];
	
	attributesPtr->va_fileid 	= cddaNodePtr->nodeID;		// Set the nodeID
	attributesPtr->va_type		= vNodePtr->v_type;			// Set the VNode type (e.g. VREG, VDIR)
	attributesPtr->va_blocksize = kPhysicalMediaBlockSize;	// Set preferred block size for I/O requests
	
	// Set all the time fields
	attributesPtr->va_atime		= cddaMountPtr->mountTime;	// Last accessed time
	attributesPtr->va_mtime 	= cddaMountPtr->mountTime;	// Last modification time
	attributesPtr->va_ctime 	= cddaMountPtr->mountTime;	// Last change time
	
	// These fields are the same
	attributesPtr->va_uid 		= kUnknownUserID;	// "unknown"
	attributesPtr->va_gid 		= kUnknownGroupID;	// "unknown"
	attributesPtr->va_gen 		= 0;
	attributesPtr->va_flags 	= 0;
	attributesPtr->va_rdev 		= NULL;
	
	// Set some common mode flags
	attributesPtr->va_mode		= S_IRUSR | S_IRGRP | S_IROTH;	// Read is ok for user, group, other
	
	// If it's the root, set some flags for it.
	if ( vNodePtr->v_flag & VROOT )
	{
		
		attributesPtr->va_mode 		|= S_IFDIR;									// It's a directory
		attributesPtr->va_mode		|= S_IXUSR | S_IXGRP | S_IXOTH;				// Execute is ok for user, group, other
		attributesPtr->va_nlink 	= kAppleCDDANumberOfRootDirReferences;		// Number of file refs: "." and ".."
		
		// Number of Tracks + ".", "..", and ".TOC.plist"
		attributesPtr->va_bytes	= attributesPtr->va_size = ( cddaNodePtr->u.directory.entryCount ) * sizeof ( struct dirent );
		
	}

	// If it isn't the root vnode, it's a file
	else
	{
		
		attributesPtr->va_mode 		|= S_IFREG;	// It's a file...
		attributesPtr->va_nlink 	= kAppleCDDANumberOfFileReferences;	// Just the file itself
		
		// Is it a track?
		if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
		{
			
			// Set file size in bytes
			attributesPtr->va_bytes	= attributesPtr->va_size = cddaNodePtr->u.file.nodeInfoPtr->numBytes;
			
		}
		
		// Is it the ".TOC.plist" file?
		else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
		{
			
			// Set file size in bytes
			attributesPtr->va_bytes	= attributesPtr->va_size = cddaNodePtr->u.xmlFile.fileSize;
			
		}
		
	}
	
	DebugLog ( ( "CDDA_GetAttributes: exiting...\n" ) );
	
	return ( 0 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_GetAttributesList - 	This routine gets the attribute list for the
//								specified vnode.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_GetAttributesList ( struct vop_getattrlist_args * attrListArgsPtr )
/*
struct vop_getattrlist_args {
	struct vnode *a_vp;
	struct attrlist *a_alist
	struct uio *a_uio;
	struct ucred *a_cred;
	struct proc *a_p;
};
*/
{
	
	struct vnode *		vNodePtr 			= NULL;
	AppleCDDANodePtr	cddaNodePtr			= NULL;
	struct attrlist *	attributesListPtr	= NULL;
	int 				error = 0;
	UInt32 				fixedblocksize;
	UInt32 				attrblocksize;
	UInt32 				attrbufsize;
	void *				attrbufptr;
	void *				attrptr;
	void *				varptr;
	
	DebugLog ( ( "CDDA_GetAttributesList: Entering.\n" ) );
	
	DebugAssert ( ( attrListArgsPtr != NULL ) );
	
	vNodePtr 			= attrListArgsPtr->a_vp;
	attributesListPtr 	= attrListArgsPtr->a_alist;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( attributesListPtr != NULL ) );
	
	cddaNodePtr	= VTOCDDA ( vNodePtr );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	DebugAssert ( ( ap->a_uio->uio_rw == UIO_READ ) );
	
	// Make sure the caller isn't asking for an attribute we don't support.
	if ( ( attributesListPtr->bitmapcount != 5 ) ||
		 ( ( attributesListPtr->commonattr & ~kAppleCDDACommonAttributesValidMask ) != 0 ) ||
		 ( ( attributesListPtr->volattr & ~kAppleCDDAVolumeAttributesValidMask ) != 0 ) ||
		 ( ( attributesListPtr->dirattr & ~kAppleCDDADirectoryAttributesValidMask ) != 0 ) ||
		 ( ( attributesListPtr->fileattr & ~kAppleCDDAFileAttributesValidMask ) != 0 ) ||
		 ( ( attributesListPtr->forkattr & ~kAppleCDDAForkAttributesValidMask ) != 0) )
	{
		
		DebugLog ( ( "CDDA_GetAttributesList: bad attrlist\n" ) );
		return EOPNOTSUPP;
		
	}
	
	// Requesting volume information requires setting the ATTR_VOL_INFO bit
	// and volume info requests are mutually exclusive with all other info requests:
	if ( ( attributesListPtr->volattr != 0 ) && ( ( ( attributesListPtr->volattr & ATTR_VOL_INFO ) == 0 ) ||
		 ( attributesListPtr->dirattr != 0 ) || ( attributesListPtr->fileattr != 0 ) || ( attributesListPtr->forkattr != 0 ) ) )
	{
		
		DebugLog ( ( "CDDA_GetAttributesList: conflicting information requested\n" ) );
		return EINVAL;
		
	}
	
	fixedblocksize = CalculateAttributeBlockSize ( attributesListPtr );
	
	// UInt32 for length longword
	attrblocksize = fixedblocksize + ( sizeof ( UInt32 ) );
	
	if ( attributesListPtr->commonattr & ATTR_CMN_NAME )
		attrblocksize += kCDDAMaxFileNameBytes + 1;
		
	if ( attributesListPtr->volattr & ATTR_VOL_MOUNTPOINT )
		attrblocksize += PATH_MAX;
		
	if ( attributesListPtr->volattr & ATTR_VOL_NAME )
		attrblocksize += kCDDAMaxFileNameBytes + 1;
	
	attrbufsize = MIN ( attrListArgsPtr->a_uio->uio_resid, attrblocksize );
	
	DebugLog ( ( "CDDA_GetAttributesList: allocating Ox%X byte buffer (Ox%X + Ox%X) for attributes...\n",
				  attrblocksize, fixedblocksize, attrblocksize - fixedblocksize ) );
	
	MALLOC ( attrbufptr, void *, attrblocksize, M_TEMP, M_WAITOK );
	attrptr = attrbufptr;
	
	// Set buffer length in case of errors
	*( ( UInt32 * ) attrptr ) = 0;
	
	// Reserve space for length field
	++( ( UInt32 * ) attrptr );
	
	// Point to variable-length storage
	varptr = ( ( char * ) attrptr ) + fixedblocksize;
	
	DebugLog ( ( "CDDA_GetAttributesList: attrptr = 0x%08X, varptr = 0x%08X...\n", ( u_int ) attrptr, ( u_int ) varptr ) );
	
	PackAttributesBlock ( attributesListPtr, vNodePtr, &attrptr, &varptr );

	DebugLog ( ( "CDDA_GetAttributesList: calculating MIN\n" ) );
	
	// Don't copy out more data than was generated
	attrbufsize = MIN ( attrbufsize, ( u_int ) varptr - ( u_int ) attrbufptr );
	
	DebugLog ( ( "CDDA_GetAttributesList: setting attrbufsize = %ld\n", attrbufsize ) );
	
	// Set actual buffer length for return to caller
	*( ( UInt32 * ) attrbufptr ) = attrbufsize;
	
	DebugLog ( ( "CDDA_GetAttributesList: copying %ld bytes to user address 0x%08X.\n",
				 ( UInt32 ) attrbufsize, ( u_int ) attrListArgsPtr->a_uio->uio_iov->iov_base ) );
	
	error = uiomove ( ( caddr_t ) attrbufptr, attrbufsize, attrListArgsPtr->a_uio );
	if ( error != 0 )
	{
		DebugLog ( ( "CDDA_GetAttributesList: error %d on uiomove.\n", error ) );
	}
	
	DebugLog ( ( "CDDA_GetAttributesList: calling FREE\n" ) );
	FREE ( attrbufptr, M_TEMP );
	
	return error;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Inactive - This routine simply unlocks a vnode.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Inactive ( struct vop_inactive_args * inactiveArgsPtr )
/*
struct vop_inactive_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
};
*/
{
	
	DebugLog ( ( "CDDA_Inactive: Entering.\n" ) );
	
	DebugAssert ( ( inactiveArgsPtr != NULL ) );
	
	// We don't do anything special, so call VFS function to handle it
	( void ) nop_inactive ( inactiveArgsPtr );
		
	DebugLog ( ( "CDDA_Inactive: exiting...\n" ) );
	
	return ( 0 );
	
}


#if RENAME_SUPPORTED
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Rename - This routine renames a file in the filesystem.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Rename ( struct vop_rename_args * renameArgsPtr )
/*
struct vop_rename_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_fdvp;
	struct vnode *a_fvp;
	struct componentname *a_fcnp;
	struct vnode *a_tdvp;
	struct vnode *a_tvp;
	struct componentname *a_tcnp;
};
*/
{

	struct vnode *			targetVNodePtr 			= NULL;
	struct vnode *			targetParentVNodePtr 	= NULL;
	struct vnode *			sourceVNodePtr 			= NULL;
	struct vnode *			sourceParentVNodePtr 	= NULL;
	struct componentname *	targetComponentNamePtr 	= NULL;
	struct componentname *	sourceComponentNamePtr 	= NULL;
	struct proc	*			procPtr					= NULL;
	AppleCDDAMountPtr		cddaMountPtr			= NULL;
	AppleCDDANodePtr		sourceCDDANodePtr		= NULL;
	AppleCDDANodePtr		sourceParentCDDANodePtr	= NULL;
	AppleCDDANodeInfoPtr	nodeInfoPtr				= NULL;
	int						retval					= 0;
	struct timeval			tv;
	struct timespec			timespec;
	
	DebugLog ( ( "CDDA_Rename: Entering.\n" ) );	
	DebugAssert ( ( renameArgsPtr != NULL ) );
	
	targetVNodePtr 			= renameArgsPtr->a_tvp;
	targetParentVNodePtr 	= renameArgsPtr->a_tdvp;
	sourceVNodePtr 			= renameArgsPtr->a_fvp;
	sourceParentVNodePtr 	= renameArgsPtr->a_fdvp;
	targetComponentNamePtr 	= renameArgsPtr->a_tcnp;
	sourceComponentNamePtr 	= renameArgsPtr->a_fcnp;
	
	DebugAssert ( ( targetVNodePtr != NULL ) );
	DebugAssert ( ( targetParentVNodePtr != NULL ) );
	DebugAssert ( ( sourceVNodePtr != NULL ) );
	DebugAssert ( ( sourceParentVNodePtr != NULL ) );
	DebugAssert ( ( targetComponentNamePtr != NULL ) );
	DebugAssert ( ( sourceComponentNamePtr != NULL ) );
	
	procPtr = sourceComponentNamePtr->cn_proc;
	
	DebugAssert ( ( procPtr != NULL ) );
	
    if ( ( ( targetComponentNamePtr->cn_flags & HASBUF ) == 0 ) ||
		 ( ( sourceComponentNamePtr->cn_flags & HASBUF ) == 0 ) )
        panic ( "CDDA_Rename: no name" );
	
	// If targetVNodePtr is not NULL, we bail.
	// We only support in place renaming.
	if ( targetVNodePtr != NULL )
	{
		
		DebugLog ( ( "CDDA_Rename: sourceVNodePtr != targetVNodePtr.\n" ) );
		retval = EINVAL;
		goto Exit;
		
	}
		
	sourceCDDANodePtr 		= VTOCDDA ( sourceVNodePtr );
	sourceParentCDDANodePtr	= VTOCDDA ( sourceParentVNodePtr );
	
	// Be sure we are not renaming ".", "..", or an alias of ".". This
	// leads to a crippled directory tree.	It's pretty tough to do a
	// "ls" or "pwd" with the "." directory entry missing, and "cd .."
	// doesn't work if the ".." entry is missing.
	if ( sourceCDDANodePtr->nodeType == kAppleCDDADirectoryType )
	{
		
		DebugLog ( ( "CDDA_Rename: sourceCDDANodePtr->nodeType == kAppleCDDADirectoryType.\n" ) );
		retval = EINVAL;
		goto Exit;
		
	}
	
	if ( sourceCDDANodePtr->nodeType == kAppleCDDAXMLFileType )
	{
		
		DebugLog ( ( "CDDA_Rename: sourceCDDANodePtr->nodeType == kAppleCDDAXMLFileType.\n" ) );
		retval = EPERM;
		goto Exit;
		
	}
	
	// Verify the name makes sense (has .aiff at the end and
	// has the track number at the beginning)
	if ( targetComponentNamePtr->cn_nameptr )
	{
		
		char		buffer[3];
		char *		ptr;
		char *		ptr2;
		UInt32		size;
		UInt8		value;
		
		ptr = strchr ( targetComponentNamePtr->cn_nameptr, ' ' );
		if ( ptr == NULL )
		{
			
			DebugLog ( ( "CDDA_Rename: Need space between number and name.\n" ) );
			retval = EINVAL;
			goto Exit;
			
		}
		
		size = ( UInt32 ) ptr - ( UInt32 ) targetComponentNamePtr->cn_nameptr;
		if ( size < 1 )
		{
			
			DebugLog ( ( "CDDA_Rename: Amount of data to copy is nothing, bail\n" ) );
			retval = EINVAL;
			goto Exit;
			
		}
			
		strncpy ( buffer, targetComponentNamePtr->cn_nameptr, size );
		buffer[size] = 0;
		value = atoi ( buffer );
		
		if ( value != sourceCDDANodePtr->u.file.nodeInfoPtr->trackDescriptor.point )
		{
			
			DebugLog ( ( "CDDA_Rename: Name is invalid\n" ) );
			retval = EINVAL;
			goto Exit;
			
		}
		
		// Now check for .aiff
		ptr = strchr ( targetComponentNamePtr->cn_nameptr, '.' );
		if ( ptr == NULL )
		{
			
			DebugLog ( ( "CDDA_Rename: No .aiff at the end\n" ) );
			retval = EINVAL;
			goto Exit;
			
		}
		
		// make sure we go to the last . in the name (so tedious)...
		ptr2 = strchr ( ptr + 1, '.' );		
		while ( ptr2 != NULL )
		{
			
			DebugLog ( ( "CDDA_Rename: In while loop\n" ) );
			
			ptr = ptr2;
			ptr2 = strchr ( ptr2 + 1, '.' );
			
		}
		
		size = ( UInt32 ) ptr - ( UInt32 ) targetComponentNamePtr->cn_nameptr + strlen ( ".aiff" );
		if ( size != strlen ( targetComponentNamePtr->cn_nameptr ) )
		{
			
			DebugLog ( ( "CDDA_Rename: .aiff is not at the ending\n" ) );
			retval = EINVAL;
			goto Exit;
			
		}
		
	}
	
	// remove the existing entry from the namei cache:
	cache_purge ( sourceVNodePtr );
	DebugLog ( ( "CDDA_Rename: finished cach_purge.\n" ) );
	
	// Change the name in the NodeInfoArray
	nodeInfoPtr = sourceCDDANodePtr->u.file.nodeInfoPtr;
	FREE ( nodeInfoPtr->name, M_TEMP );
	DebugLog ( ( "CDDA_Rename: FREE'd name.\n" ) );
	
	MALLOC ( nodeInfoPtr->name, char *, strlen ( targetComponentNamePtr->cn_nameptr ) + 1, M_TEMP, M_WAITOK );
	strcpy ( nodeInfoPtr->name, targetComponentNamePtr->cn_nameptr );
	nodeInfoPtr->nameSize = strlen ( targetComponentNamePtr->cn_nameptr );
	DebugLog ( ( "CDDA_Rename: finished strcpy of name.\n" ) );
	
	// Timestamp the parent directory.
	tv = time;
	
	TIMEVAL_TO_TIMESPEC ( &tv, &timespec );
	cddaMountPtr = VFSTOCDDA ( sourceVNodePtr->v_mount );
	cddaMountPtr->mountTime	= timespec;
	
	
Exit:
	
	
	if ( targetVNodePtr != NULL )
	{
		
		vput ( targetVNodePtr );
		DebugLog ( ( "CDDA_Rename: vput of targetVNodePtr done.\n" ) );
		
	}
	
	vput ( targetParentVNodePtr );
	DebugLog ( ( "CDDA_Rename: vput of targetParentVNodePtr done.\n" ) );
	
	vrele ( sourceParentVNodePtr );
	DebugLog ( ( "CDDA_Rename: vrele of sourceParentVNodePtr done.\n" ) );
	
	vrele ( sourceVNodePtr );
	DebugLog ( ( "CDDA_Rename: vrele of sourceVNodePtr done.\n" ) );
	
	return retval;
	
}
#endif	/* RENAME_SUPPORTED */

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Remove - 	This routine removes a file from the name space. Since we
//					are a read-only volume, we release any locks if appropriate
//					and return EROFS
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int 
CDDA_Remove ( struct vop_remove_args * removeArgsPtr )
/*
struct vop_remove_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
*/
{
		
	DebugLog ( ( "CDDA_Remove: Entering.\n" ) );

	DebugAssert ( ( removeArgsPtr != NULL ) );
	
	// We don't do anything special, so call VFS function to handle it
	( void ) nop_remove ( removeArgsPtr );
		
	DebugLog ( ( "CDDA_Remove: exiting...\n" ) );
	
	// Return the read-only filesystem error
	return ( EROFS );
	
}   


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_RmDir - 	This routine removes a directory from the name space.
//					Since we are a read-only volume, we release any locks
//					and return EROFS
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int 
CDDA_RmDir ( struct vop_rmdir_args * removeDirArgsPtr )
/*
struct vop_rmdir_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
*/
{

	DebugLog ( ( "CDDA_RmDir: Entering.\n" ) );

	DebugAssert ( ( removeDirArgsPtr != NULL ) );

	// Call nop_rmdir to release locks
	( void ) nop_rmdir ( removeDirArgsPtr );
	
	DebugLog ( ( "CDDA_RmDir: exiting...\n" ) );
	
	// Return the read-only filesystem error
	return ( EROFS );
	
}   


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Reclaim - This routine reclaims a vnode for use by the system.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Reclaim ( struct vop_reclaim_args * reclaimArgsPtr )
/*
struct vop_reclaim_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct proc *a_p;
};
*/
{
	
	int		error 	= 0;
	
	DebugLog ( ( "CDDA_Reclaim: Entering.\n" ) );

	DebugAssert ( ( reclaimArgsPtr != NULL ) );

	error = DisposeCDDANode ( reclaimArgsPtr->a_vp, reclaimArgsPtr->a_p );	

	DebugLog ( ( "CDDA_Reclaim: exiting...\n" ) );
	
	return ( error );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Access - This routine checks the access attributes for a node
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Access ( struct vop_access_args * accessArgsPtr )
/*
struct vop_access_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct proc *a_p;
};
*/
{
	
	AppleCDDANodePtr	cddaNodePtr		= NULL;
	struct ucred * 		cred			= NULL;
	mode_t				mode			= 0;
	int					error			= 0;
	
	DebugLog ( ( "CDDA_Access: Entering.\n" ) );
	
	DebugAssert ( ( accessArgsPtr != NULL ) );
	
	cred 		= accessArgsPtr->a_cred;
	mode 		= accessArgsPtr->a_mode;	
	cddaNodePtr = VTOCDDA ( accessArgsPtr->a_vp );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	// Disallow writes on read-only filesystem
	if ( mode & VWRITE )
	{
		
		if ( accessArgsPtr->a_vp->v_mount->mnt_flag & MNT_RDONLY )
		{
			
			// They tried to write to a read-only mountpoint, return EROFS
			return ( EROFS );
		
		}
		
		else
		{

			// Should never get here
			DebugLog ( ( "Error: this filesystem is read-only, but isn't marked read-only" ) );
			panic ( "Read-Only filesystem" );
		
		}
		
	}
	
	// user id 0 (super-user) always gets access
	if ( cred->cr_uid == 0 )
	{

		error = 0;

	}
	
	// everyone else gets access too!
	error = 0;
	
	
	DebugLog ( ( "CDDA_Access: exiting...\n" ) );
	
	return ( error );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_BlockToOffset - 	This routine converts logical block number to file
//							offset
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_BlockToOffset ( struct vop_blktooff_args * blockToOffsetArgsPtr )
/*
struct vop_blktooff_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	daddr_t a_lblkno;
	off_t *a_offset;
};
*/
{	

	DebugLog ( ( "CDDA_BlockToOffset: Entering.\n" ) );

	DebugAssert ( ( blockToOffsetArgsPtr != NULL ) );
	
	if ( blockToOffsetArgsPtr->a_vp == NULL )
	{

		DebugLog ( ( "CDDA_BlockToOffset: incoming vnode is NULL.\n" ) );
		return ( EINVAL );

	}
	
	*( blockToOffsetArgsPtr->a_offset ) =
			( off_t ) ( blockToOffsetArgsPtr->a_lblkno * PAGE_SIZE );
	
	DebugLog ( ( "CDDA_BlockToOffset: exiting...\n" ) );
	
	return ( 0 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_OffsetToBlock - 	This routine converts a file offset to a logical
//							block number
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_OffsetToBlock ( struct vop_offtoblk_args * offsetToBlockArgsPtr )
/*
struct vop_offtoblk_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_offset;
	daddr_t *a_lblkno;
};
*/
{

	DebugLog ( ( "CDDA_OffsetToBlock: Entering.\n" ) );

	DebugAssert ( ( offsetToBlockArgsPtr != NULL ) );

	if ( offsetToBlockArgsPtr->a_vp == NULL )
	{

		DebugLog ( ( "CDDA_OffsetToBlock: incoming vnode is NULL.\n" ) );
		return ( EINVAL );

	}
	
	*( offsetToBlockArgsPtr->a_lblkno ) = ( offsetToBlockArgsPtr->a_offset / PAGE_SIZE );

	DebugLog ( ( "CDDA_OffsetToBlock: exiting...\n" ) );

	return ( 0 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Pathconf - Return POSIX pathconf information applicable to
//					special devices
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Pathconf ( struct vop_pathconf_args * pathConfArgsPtr )
/*
struct vop_pathconf_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_name;
	register_t *a_retval;
};
*/
{
	
	int returnValue	= 0;
	
	DebugLog ( ( "CDDA_Pathconf: Entering.\n" ) );

	DebugAssert ( ( pathConfArgsPtr != NULL ) );

	switch ( pathConfArgsPtr->a_name )
	{
		
		case _PC_LINK_MAX:
			*pathConfArgsPtr->a_retval = 1;
			break;
			
		case _PC_NAME_MAX:
			*pathConfArgsPtr->a_retval = NAME_MAX;
			break;
		
		case _PC_PATH_MAX:
			*pathConfArgsPtr->a_retval = PATH_MAX;
			break;
					
		case _PC_CHOWN_RESTRICTED:
			*pathConfArgsPtr->a_retval = 1;
			break;

		case _PC_NO_TRUNC:
			*pathConfArgsPtr->a_retval = 0;
			break;
			
		default:
			returnValue = EINVAL;
			break;
			
	}

	DebugLog ( ( "CDDA_Pathconf: exiting with returnValue = %d.\n", returnValue ) );
	
	return ( returnValue );
		
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Lock -  Locks a vnode
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Lock ( struct vop_lock_args * lockArgsPtr )
/*
struct vop_lock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
	struct proc *a_p;
};
*/
{

	struct vnode * 		vNodePtr 	= NULL;
	AppleCDDANodePtr	cddaNodePtr = NULL;
	int					error		= 0;
		
	DebugLog ( ( "CDDA_Lock: Entering.\n" ) );
	
	DebugAssert ( ( lockArgsPtr != NULL ) );

	vNodePtr = lockArgsPtr->a_vp;
	DebugAssert ( ( vNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	// Pass it along to the lockmgr to do the locking
	error = lockmgr ( &cddaNodePtr->lock, lockArgsPtr->a_flags,
					   &vNodePtr->v_interlock, lockArgsPtr->a_p );
	
	if ( error != 0 )
	{
		
		if ( ( lockArgsPtr->a_flags & LK_NOWAIT ) == 0 )
		{
			
			DebugLog ( ( "CDDA_Lock: error %d trying to lock vnode (flags = 0x%08X).\n", error, lockArgsPtr->a_flags ) );
			
		}
		
	}

	DebugLog ( ( "CDDA_Lock: exiting...\n" ) );
	
	return ( error );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Unlock -  Unlocks a vnode
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Unlock ( struct vop_unlock_args * unlockArgsPtr )
/*
struct vop_unlock_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
	struct proc *a_p;
};
*/
{

	struct vnode * 		vNodePtr 	= NULL;
	AppleCDDANodePtr	cddaNodePtr = NULL;
	int					error		= 0;
	
	DebugLog ( ( "CDDA_Unlock: Entering.\n" ) );
	
	DebugAssert ( ( unlockArgsPtr != NULL ) );

	vNodePtr = unlockArgsPtr->a_vp;
	DebugAssert ( ( vNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	// Pass it along to the lockmgr to do the unlocking
	error = lockmgr ( &cddaNodePtr->lock, unlockArgsPtr->a_flags | LK_RELEASE,
					   &vNodePtr->v_interlock, unlockArgsPtr->a_p );
	
	if ( error != 0 )
	{
		
		DebugLog ( ( "CDDA_Unlock: error %d trying to unlock vnode (flags = 0x%08X).\n", error, unlockArgsPtr->a_flags ) );
		
	}

	DebugLog ( ( "CDDA_Unlock: exiting...\n" ) );
	
	return ( error );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_IsLocked -  Gets lock status on a vnode
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_IsLocked ( struct vop_islocked_args * isLockedArgsPtr )
/*
struct vop_islocked_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
*/
{
	
	struct vnode * 		vNodePtr 	= NULL;
	AppleCDDANodePtr	cddaNodePtr = NULL;
	int					lockStatus	= 0;

	DebugLog ( ( "CDDA_IsLocked: Entering.\n" ) );
		
	DebugAssert ( ( isLockedArgsPtr != NULL ) );

	vNodePtr = isLockedArgsPtr->a_vp;
	DebugAssert ( ( vNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	// Pass along to the lockmgr to get the lock status
	lockStatus = lockstatus ( &cddaNodePtr->lock );
	
	DebugLog ( ( "CDDA_IsLocked: exiting...\n" ) );
	
	return ( lockStatus );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_Print -  Print out the contents of a CDDA vnode.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_Print ( struct vop_print_args * printArgsPtr )
/*
struct vop_print_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
*/
{

#if DEBUG
	DebugAssert ( ( printArgsPtr != NULL ) );
#else
	#pragma unused ( printArgsPtr )
#endif

	DebugLog ( ( "CDDA_Print: Entering.\n" ) );
	DebugLog ( ( "CDDA_Print: exiting...\n" ) );
	
	return ( 0 );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDDA_VFree -  Does nothing.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CDDA_VFree ( struct vop_vfree_args * vFreeArgsPtr )
{

#if DEBUG
	DebugAssert ( ( vFreeArgsPtr != NULL ) );
#else
	#pragma unused ( vFreeArgsPtr )
#endif

	DebugLog ( ( "CDDA_VFree: Entering.\n" ) );
	DebugLog ( ( "CDDA_VFree: exiting...\n" ) );
	
	return ( 0 );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Other macro'd function definitions
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int ( **gCDDA_VNodeOp_p )( void * );
#define VOPFUNC int ( * )( void * )


#if 0
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	VNode Operation Vector Entry Description (Dispatch Table)
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

struct vnodeopv_entry_desc gCDDA_VNodeOperationEntries[] =
{
	{ &vop_default_desc, 		( VOPFUNC ) vn_default_error },
	{ &vop_lookup_desc, 		( VOPFUNC ) CDDA_Lookup },				// lookup
	{ &vop_create_desc, 		( VOPFUNC ) err_create },				// create
	{ &vop_mknod_desc, 			( VOPFUNC ) err_mknod },				// mknod
	{ &vop_open_desc, 			( VOPFUNC ) CDDA_Open },				// open
	{ &vop_close_desc, 			( VOPFUNC ) CDDA_Close },				// close
	{ &vop_access_desc, 		( VOPFUNC ) CDDA_Access },				// access
	{ &vop_getattr_desc, 		( VOPFUNC ) CDDA_GetAttributes },		// getattr
	{ &vop_setattr_desc, 		( VOPFUNC ) nop_setattr },				// setattr
	{ &vop_read_desc, 			( VOPFUNC ) CDDA_Read },				// read
	{ &vop_write_desc, 			( VOPFUNC ) err_write },				// write
	{ &vop_ioctl_desc, 			( VOPFUNC ) err_ioctl },				// ioctl
	{ &vop_select_desc, 		( VOPFUNC ) err_select },				// select
	{ &vop_exchange_desc, 		( VOPFUNC ) err_exchange },				// exchange
	{ &vop_mmap_desc, 			( VOPFUNC ) err_mmap },					// mmap
	{ &vop_fsync_desc, 			( VOPFUNC ) nop_fsync },				// fsync
	{ &vop_seek_desc, 			( VOPFUNC ) err_seek },					// seek
	{ &vop_remove_desc, 		( VOPFUNC ) CDDA_Remove },				// remove
	{ &vop_link_desc, 			( VOPFUNC ) err_link },					// link
#if RENAME_SUPPORTED
	{ &vop_rename_desc, 		( VOPFUNC ) CDDA_Rename },				// rename
#else
	{ &vop_rename_desc, 		( VOPFUNC ) err_rename },				// rename	
#endif /* RENAME_SUPPORTED */
	{ &vop_mkdir_desc, 			( VOPFUNC ) err_mkdir },				// mkdir
	{ &vop_rmdir_desc, 			( VOPFUNC ) CDDA_RmDir },				// rmdir
	{ &vop_getattrlist_desc, 	( VOPFUNC ) CDDA_GetAttributesList },	// getattrlist
	{ &vop_setattrlist_desc, 	( VOPFUNC ) err_setattrlist },			// setattrlist
	{ &vop_symlink_desc, 		( VOPFUNC ) err_symlink },				// symlink
	{ &vop_readdir_desc, 		( VOPFUNC ) CDDA_ReadDir },				// readdir
	{ &vop_readdirattr_desc, 	( VOPFUNC ) err_readdirattr },			// readdirattr
	{ &vop_readlink_desc, 		( VOPFUNC ) err_readlink },				// readlink
	{ &vop_abortop_desc, 		( VOPFUNC ) err_abortop },				// abortop
	{ &vop_inactive_desc, 		( VOPFUNC ) CDDA_Inactive },			// inactive
	{ &vop_reclaim_desc, 		( VOPFUNC ) CDDA_Reclaim },				// reclaim
	{ &vop_lock_desc, 			( VOPFUNC ) CDDA_Lock },				// lock
	{ &vop_unlock_desc, 		( VOPFUNC ) CDDA_Unlock },				// unlock
	{ &vop_bmap_desc, 			( VOPFUNC ) err_bmap },					// bmap
	{ &vop_strategy_desc, 		( VOPFUNC ) err_strategy },				// strategy
	{ &vop_print_desc, 			( VOPFUNC ) CDDA_Print },				// print
	{ &vop_islocked_desc, 		( VOPFUNC ) CDDA_IsLocked },			// islocked
	{ &vop_pathconf_desc, 		( VOPFUNC ) CDDA_Pathconf },			// pathconf
	{ &vop_advlock_desc, 		( VOPFUNC ) err_advlock },				// advlock
	{ &vop_reallocblks_desc,	( VOPFUNC ) err_reallocblks },			// reallocblks
	{ &vop_truncate_desc, 		( VOPFUNC ) err_truncate },				// truncate
	{ &vop_allocate_desc, 		( VOPFUNC ) err_allocate },				// allocate
	{ &vop_update_desc, 		( VOPFUNC ) nop_update },				// update
	{ &vop_bwrite_desc, 		( VOPFUNC ) err_bwrite },				// bwrite
	{ &vop_pagein_desc, 		( VOPFUNC ) CDDA_PageIn },				// pagein
	{ &vop_pageout_desc, 		( VOPFUNC ) err_pageout },				// pageout
	{ &vop_blktooff_desc, 		( VOPFUNC ) CDDA_BlockToOffset },		// blktoff
	{ &vop_offtoblk_desc,		( VOPFUNC ) CDDA_OffsetToBlock },		// offtoblk
	{ &vop_cmap_desc,			( VOPFUNC ) err_cmap },					// cmap
	{ NULL, 					( VOPFUNC ) NULL }
};


struct vnodeopv_desc gCDDA_VNodeOperationsDesc =
{
	&gCDDA_VNodeOp_p,
	gCDDA_VNodeOperationEntries
};


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//				End				Of			File
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ