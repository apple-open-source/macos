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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/paths.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/ubc.h>
#include <sys/xattr.h>
#include <vfs/vfs_support.h>
#include <string.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/IOLib.h>


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

const char gAIFFHeaderPadData[kPhysicalMediaBlockSize - sizeof(CDAIFFHeader)] = { 0 };

//-----------------------------------------------------------------------------
//	Static Function Prototypes
//-----------------------------------------------------------------------------

static SInt32
AddDirectoryEntry ( UInt32 nodeID, UInt8 type, const char * name, uio_t uio );

static inline uint64_t
__u64min(uint64_t a, uint64_t b)
{
	return (a > b ? b : a);
}

//-----------------------------------------------------------------------------
//	AddDirectoryEntry - This routine adds a directory entry to the uio buffer
//-----------------------------------------------------------------------------

static SInt32
AddDirectoryEntry ( UInt32			nodeID,
					UInt8			type,
					const char *	name,
					uio_t			uio )
{
	
	struct dirent		directoryEntry;
	SInt32				nameLength				= 0;
	UInt16				directoryEntryLength	= 0;
	
	DebugAssert ( ( name != NULL ) );
	DebugAssert ( ( uio != NULL ) );

	DebugLog ( ( "fileName = %s\n", name ) );
	
	nameLength = ( SInt32 ) strlen ( name );		
	DebugAssert ( ( nameLength < MAXNAMLEN + 1 ) );
	
	directoryEntry.d_fileno = nodeID;
	directoryEntry.d_reclen = sizeof ( directoryEntry );
	directoryEntry.d_type	= type;
	directoryEntry.d_namlen = nameLength;
	directoryEntryLength	= directoryEntry.d_reclen;
	
	// Copy the string
	strncpy ( directoryEntry.d_name, name, MAXNAMLEN );
	
	// Zero the rest of the array for safe-keeping
	bzero ( &directoryEntry.d_name[nameLength], MAXNAMLEN + 1 - nameLength );
	
	if ( uio_resid ( uio ) < directoryEntry.d_reclen )
	{
		
		// We can't copy because there isn't enough room in the buffer,
		// so set the directoryEntryLength to zero so the caller knows
		// an error occurred
		directoryEntryLength = 0;
		
	}
	
	else
	{
		
		// Move the data
		uiomove ( ( caddr_t ) &directoryEntry, ( int ) sizeof ( directoryEntry ), uio );
				
	}
	
	return directoryEntryLength;
	
}


//-----------------------------------------------------------------------------
//	CDDA_Lookup -	This routine performs a lookup
//-----------------------------------------------------------------------------

int
CDDA_Lookup ( struct vnop_lookup_args * lookupArgsPtr )
/*
struct vnop_lookup_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_dvp;
	vnode_t *a_vpp;
	struct componentname *a_cnp;
	vfs_context_t a_context;
};
*/
{
	
	struct mount *				mountPtr			= NULL;
	struct componentname *		compNamePtr			= NULL;
	vnode_t *					vNodeHandle			= NULL;
	vnode_t						parentVNodePtr		= NULLVP;
	AppleCDDANodePtr			parentCDDANodePtr	= NULL;
	AppleCDDAMountPtr			cddaMountPtr		= NULL;
	int							error				= 0;
	int							flags				= 0;
	
	DebugLog ( ( "CDDA_Lookup: Entering.\n" ) );
	
	DebugAssert ( ( lookupArgsPtr != NULL ) );
	
	compNamePtr		= lookupArgsPtr->a_cnp;
	vNodeHandle		= lookupArgsPtr->a_vpp;
	parentVNodePtr	= lookupArgsPtr->a_dvp;
	mountPtr		= vnode_mount ( parentVNodePtr );
	
	DebugAssert ( ( compNamePtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	DebugAssert ( ( parentVNodePtr != NULL ) );
	
	parentCDDANodePtr	= VTOCDDA ( parentVNodePtr );
	cddaMountPtr		= VFSTOCDDA ( mountPtr );
	
	DebugAssert ( ( parentCDDANodePtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	*vNodeHandle	= NULL;
	flags			= compNamePtr->cn_flags;
	
	if ( compNamePtr->cn_namelen > NAME_MAX )
	{
		
		error = ENAMETOOLONG;
		goto Exit;
		
	}
	
	// Check if process wants to create, delete or rename anything
	if ( compNamePtr->cn_nameiop == CREATE ||
		 compNamePtr->cn_nameiop == RENAME ||
		 compNamePtr->cn_nameiop == DELETE )
	{
		
		DebugLog ( ( "Can't CREATE, RENAME or DELETE %s, returning EROFS\n", compNamePtr->cn_nameptr ) );
		error = EROFS;
		goto Exit;
		
	}
	
	// Determine if we're looking for a resource fork.
	// NB: this could cause a read off the end of the component name buffer in some rare cases.
	if ( ( flags & ISLASTCN ) == 0 && bcmp ( &compNamePtr->cn_nameptr[compNamePtr->cn_namelen],
											 _PATH_RSRCFORKSPEC,
											 sizeof ( _PATH_RSRCFORKSPEC ) - 1 ) == 0 )
	{
		
		DebugLog ( ( "No resource forks available, return ENOTDIR.\n" ) );
		compNamePtr->cn_consume = ( uint32_t ) sizeof ( _PATH_RSRCFORKSPEC ) - 1;
		error = ENOTDIR;
		goto Exit;
		
	}
	
	DebugLog ( ( "Looking for name = %s.\n", compNamePtr->cn_nameptr ) );
	
	// first check for "." and ".TOC.plist"
	if ( compNamePtr->cn_nameptr[0] == '.' )
	{
		
		if ( compNamePtr->cn_namelen == 1 )
		{
			
			DebugLog ( ( ". was requested\n" ) );
			
			error = CDDA_VGetInternal ( mountPtr, kAppleCDDARootFileID, parentVNodePtr, compNamePtr, vNodeHandle );
			goto Exit;
			
		}
		
		else if ( ( compNamePtr->cn_namelen == 10 ) && ( !strncmp ( &compNamePtr->cn_nameptr[1], "TOC.plist", 9 ) ) )
		{
			
			DebugLog ( ( ".TOC.plist was requested\n" ) );
			
			error = CDDA_VGetInternal ( mountPtr, kAppleCDDAXMLFileID, parentVNodePtr, compNamePtr, vNodeHandle );
			goto Exit;
			
		}
		
		else
		{
			
			// Not going to find anything prefixed with "." other than the above.
			error = ENOENT;
			goto Exit;
			
		}
		
	}
	
	// At this point, we better be fetching a file which ends in ".aiff".
	if ( strncmp ( &compNamePtr->cn_nameptr[compNamePtr->cn_namelen - 5], ".aiff", 5 ) != 0 )
	{
		
		error = ENOENT;
		goto Exit;
		
	}
	
	// Find out which inode they want. The first two bytes will tell us the track number which
	// we can convert to an inode number by adding the kOffsetForFiles constant. Beware lame string
	// parsing ahead...
	{
		
		ino64_t inode = 0;
		
		if ( compNamePtr->cn_nameptr[1] == ' ' )
		{
			
			// It's asking about track 1-9.
			inode = ( ino64_t ) ( compNamePtr->cn_nameptr[0] - '0' );
			
		}
		
		else if ( compNamePtr->cn_nameptr[2] == ' ' )
		{
			
			// It's asking about track 10-99.
			inode = ( ino64_t ) ( ( ( compNamePtr->cn_nameptr[0] - '0' ) * 10 ) + ( compNamePtr->cn_nameptr[1] - '0' ) );
			
		}
		
		DebugLog ( ( "Track %lld was requested\n", inode ) );
		
		// Add the offset for a CD Track...
		inode += kOffsetForFiles;
		
		// Call the internal vget routine. Make sure to pass the parentVNode and compNamePtr so they
		// can be passed to the CreateXXX routines if a vnode needs to be created.
		error = CDDA_VGetInternal ( mountPtr, inode, parentVNodePtr, compNamePtr, vNodeHandle );
		goto Exit;
		
	}
	
	
Exit:
	
	
	return ( error );
	
}


//-----------------------------------------------------------------------------
//	CDDA_Open - This routine opens a file
//-----------------------------------------------------------------------------

int
CDDA_Open ( struct vnop_open_args * openArgsPtr )
/*
struct vnop_open_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	int a_mode;
	vfs_context_t a_context;
};
*/
{

	vnode_t		vNodePtr	= NULLVP;
	int			error		= 0;
	
	DebugLog ( ( "CDDA_Open: Entering.\n" ) );

	DebugAssert ( ( openArgsPtr != NULL ) );
	
	vNodePtr = openArgsPtr->a_vp;
	DebugAssert ( ( vNodePtr != NULL ) );
		
	// Set the vNodeOperationType to tell the user process if we are going to open a
	// file or a directory
	if ( ! vnode_isreg ( vNodePtr ) && ! vnode_isdir ( vNodePtr ) )
	{
	
		// This should never happen but just in case
		DebugLog ( ( "Error = %d, wrong vnode type.\n", ENOTSUP ) );
		error = ENOTSUP;
		goto ERROR;
		
	}
	
	// Turn off speculative read-ahead for our vnodes. The cluster
	// code can't possibly do the right thing when we have possible
	// loss of streaming on CD media.
	vnode_setnoreadahead ( vNodePtr );
	
	
ERROR:


	DebugLog ( ( "CDDA_Open: exiting with error = %d.\n", error ) );

	return ( error );

}


//-----------------------------------------------------------------------------
//	CDDA_Close -	This routine closes a file. Since we are a read-only
//					filesystem, we don't have any cleaning up to do.
//-----------------------------------------------------------------------------

int
CDDA_Close ( struct vnop_close_args * closeArgsPtr )
/*
struct vnop_close_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	int a_fflag;
	vfs_context_t a_context;
};
*/
{
#pragma unused (closeArgsPtr)
	return ( 0 );
}


//-----------------------------------------------------------------------------
//	CDDA_Read - This routine reads from a file
//-----------------------------------------------------------------------------

int
CDDA_Read ( struct vnop_read_args * readArgsPtr )
/*
struct vnop_read_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	uio_t a_uio;
	int a_ioflag;
	vfs_context_t a_context;
};
*/
{
	
	vnode_t				vNodePtr		= NULLVP;
	uio_t				uio				= NULL;
	AppleCDDANodePtr	cddaNodePtr		= NULL;
	int					error			= 0;
	
	DebugLog ( ( "CDDA_Read: Entering.\n" ) );
	
	DebugAssert ( ( readArgsPtr ) );
	
	vNodePtr	= readArgsPtr->a_vp;
	uio			= readArgsPtr->a_uio;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( uio != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	// Check to make sure we're operating on a regular file
	if ( ! vnode_isreg ( vNodePtr ) )
	{

		DebugLog ( ( "CDDA_Read: not a file, exiting with error = %d.\n", EISDIR ) );
		return ( EISDIR );

	}
	
	// Check to make sure they asked for data
	if ( uio_resid ( uio ) == 0 )
	{
		
		DebugLog ( ( "CDDA_Read: uio_resid = 0, no data requested" ) );
		return ( 0 );
		
	}
	
	// Can't read from a negative offset
	if ( uio_offset ( uio ) < 0 )
	{
		
		DebugLog ( ( "CDDA_Read: Can't read from a negative offset..." ) );
		return ( EINVAL );
		
	}

	if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
	{
		
		off_t		offset			= uio_offset ( uio );
		UInt32		amountToCopy	= 0;
		UInt32		numBytes		= 0;
		
		numBytes = cddaNodePtr->u.xmlFile.fileSize;
		
		// Check to make sure we don't read past EOF
		if ( uio_offset ( uio ) > numBytes )
		{
			
			DebugLog ( ( "CDDA_Read: Can't read past end of file..." ) );
			return ( 0 );
			
		}
		
		amountToCopy = ( UInt32 ) __u64min ( uio_resid ( uio ), numBytes - offset );
		
		uiomove ( ( caddr_t ) &cddaNodePtr->u.xmlFile.fileDataPtr[offset],
				  amountToCopy,
				  uio );
		
		return ( 0 );
		
	}
	
	else if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
	{
		
		UInt32			headerSize		= 0;
		UInt32			count			= 0;
		UInt64			blockNumber		= 0;
		off_t			offset			= 0;
		off_t			sectorOffset	= 0;
		buf_t			bufPtr			= NULL;
		
		offset	= uio_offset ( uio );
		
		// Check to make sure we don't read past EOF
		if ( offset > cddaNodePtr->u.file.nodeInfoPtr->numBytes )
		{
			
			DebugLog ( ( "CDDA_Read: Can't read past end of file..." ) );
			return ( 0 );
			
		}
		
		headerSize = ( UInt32 ) sizeof ( cddaNodePtr->u.file.aiffHeader );
		
		// Copy any part of the header that we need to copy.
		if ( offset < headerSize )
		{
			
			UInt32	amountToCopy = 0;
			UInt8 *	bytes		 = NULL;
			
			bytes = ( UInt8 * ) &cddaNodePtr->u.file.aiffHeader;
			
			amountToCopy = ( UInt32 ) __u64min ( uio_resid ( uio ), headerSize - offset );
			
			uiomove ( ( caddr_t ) &bytes[offset],
				  amountToCopy,
				  uio );
			
			offset += amountToCopy;
			
		}

		// Copy any part of the header pad that we need to copy.
		if ( ( uio_resid ( uio ) > 0  ) &&
			 ( offset < kPhysicalMediaBlockSize ) )
		{
			
			UInt32	amountToCopy = 0;

			amountToCopy = ( UInt32 ) __u64min ( uio_resid ( uio ), kPhysicalMediaBlockSize - offset );

			uiomove ( ( caddr_t ) &gAIFFHeaderPadData[offset - headerSize],
				  amountToCopy,
				  uio );
			
			offset += amountToCopy;
			
		}
		
		if ( ( uio_resid ( uio ) > 0  ) &&
			 ( uio_offset ( uio ) < cddaNodePtr->u.file.nodeInfoPtr->numBytes ) )
		{
			
			// Adjust offset by the header size so we have a true offset into the media.
			offset -= kPhysicalMediaBlockSize;
			sectorOffset = offset % kPhysicalMediaBlockSize;
			blockNumber = ( offset / kPhysicalMediaBlockSize ) + cddaNodePtr->u.file.nodeInfoPtr->LBA;
			
			// Part 1
			// We do the read in 3 parts. First, we read one sector (picks up any buffer cache entry from a
			// previous Part 3 if it exists).
			{
				
				// Clip to requested transfer count and end of file.
				count = ( UInt32 ) __u64min ( uio_resid ( uio ), ( kPhysicalMediaBlockSize - sectorOffset ) );
				count = ( UInt32 ) __u64min ( count, cddaNodePtr->u.file.nodeInfoPtr->numBytes - uio_offset ( uio ) );
				
				// Read the one sector
				error = ( int ) buf_meta_bread (
									cddaNodePtr->blockDeviceVNodePtr,
									blockNumber,
									kPhysicalMediaBlockSize,
									NOCRED,
									&bufPtr );
				
				if ( error != 0 )
				{
					
					buf_brelse ( bufPtr );
					return ( error );
					
				}
				
				// Move the data from the block into the buffer
				uiomove ( ( caddr_t ) ( ( char * ) buf_dataptr ( bufPtr ) + sectorOffset ), count, uio );
				
				// Make sure we mark this bp invalid as we don't need to keep it around anymore
				buf_markinvalid ( bufPtr );
				
				// Release this buffer back into the buffer pool. 
				buf_brelse ( bufPtr );
				
				// Update offset
				blockNumber++;
				
			}
			
			// Part 2
			// Now we execute the second part of the read. This will be the largest chunk of the read.
			// We will read multiple disc blocks up to MAXBSIZE bytes in a loop until we hit a chunk which
			// is less than one block size. That will be read in the third part.
			
			while ( ( uio_resid ( uio ) > kPhysicalMediaBlockSize ) &&
					( uio_offset ( uio ) < cddaNodePtr->u.file.nodeInfoPtr->numBytes ) )
			{
				
				UInt64		blocksToRead = 0;
				
				// Read in as close to MAXBSIZE chunks as possible
				if ( uio_resid ( uio ) > kMaxBytesPerRead )
				{
					blocksToRead	= kMaxBlocksPerRead;
					count			= kMaxBytesPerRead;
				}
				
				else
				{
					blocksToRead	= uio_resid ( uio ) / kPhysicalMediaBlockSize;
					count			= ( UInt32 ) ( blocksToRead * kPhysicalMediaBlockSize );
				}
				
				// Read kMaxBlocksPerRead blocks and put them in the cache.
				error = ( int ) buf_meta_bread (
									cddaNodePtr->blockDeviceVNodePtr,
									blockNumber,
									count,
									NOCRED,
									&bufPtr );
				
				if ( error != 0 )
				{
					
					buf_brelse ( bufPtr );
					return ( error );
					
				}
				
				count = ( UInt32 ) __u64min ( count, cddaNodePtr->u.file.nodeInfoPtr->numBytes - uio_offset ( uio ) );

				// Move the data from the block into the buffer
				uiomove ( ( caddr_t ) buf_dataptr ( bufPtr ), count, uio );
				
				// Make sure we mark any intermediate buffers as invalid as we don't need
				// to keep them.
				buf_markinvalid ( bufPtr );
				
				// Release this buffer back into the buffer pool. 
				buf_brelse ( bufPtr );
				
				// Update offset
				blockNumber += blocksToRead;
				
			}
			
			// Part 3
			// Now that we have read everything, we read the tail end which is a partial sector.
			// Sometimes we don't need to execute this step since there isn't a tail.
			if ( ( uio_resid ( uio ) > 0  ) &&
				 ( uio_offset ( uio ) < cddaNodePtr->u.file.nodeInfoPtr->numBytes ) )
			{
				
				count = ( UInt32 ) __u64min ( uio_resid ( uio ), cddaNodePtr->u.file.nodeInfoPtr->numBytes - uio_offset ( uio ) );
				
				// Read the one sector
				error = ( int ) buf_meta_bread (
									cddaNodePtr->blockDeviceVNodePtr,
									blockNumber,
									kPhysicalMediaBlockSize,
									NOCRED,
									&bufPtr );
				
				if ( error != 0 )
				{
					
					buf_brelse ( bufPtr );
					return ( error );
					
				}
				
				// Move the data from the block into the buffer
				uiomove ( ( caddr_t ) buf_dataptr ( bufPtr ), count, uio );
				
				// Make sure we mark any intermediate buffers as invalid as we don't need
				// to keep them.
				buf_markinvalid ( bufPtr );
				
				// Release this buffer back into the buffer pool. 
				buf_brelse ( bufPtr );
				
			}
			
		}
		
	}
	
	DebugLog ( ( "CDDA_Read: exiting.\n" ) );
	
	return ( error );
	
}


//-----------------------------------------------------------------------------
//	CDDA_ReadDir -	This routine reads the contents of a directory
//-----------------------------------------------------------------------------

int
CDDA_ReadDir ( struct vnop_readdir_args * readDirArgsPtr )
/*
struct vnop_readdir_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	uio_t a_uio;
	int a_flags;
	int *a_eofflag;
	int *a_numdirent;
	vfs_context_t a_context;
};
*/
{

	vnode_t					vNodePtr			= NULLVP;
	AppleCDDANodePtr		cddaNodePtr			= NULL;
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	AppleCDDANodeInfoPtr	nodeInfoArrayPtr	= NULL;
	uio_t					uio					= NULL;
	UInt32					index				= 0;
	int						error				= 0;
	SInt32					offsetValue			= 0;
	UInt32					direntSize			= 0;

	DebugLog ( ( "CDDA_ReadDir: Entering.\n" ) );

	DebugAssert ( ( readDirArgsPtr != NULL ) );

	vNodePtr	= readDirArgsPtr->a_vp;
	uio			= readDirArgsPtr->a_uio;

	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( uio != NULL ) );

	cddaNodePtr = VTOCDDA ( vNodePtr );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	if ( readDirArgsPtr->a_flags & ( VNODE_READDIR_EXTENDED | VNODE_READDIR_REQSEEKOFF ) )
		return ( EINVAL );
	
	// First make sure it is a directory we are dealing with
	if ( ! vnode_isdir ( vNodePtr ) )
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
	if ( uio_iovcnt ( uio ) > 1 )
	{
		
		DebugLog ( ( "More than one buffer, exiting with error = %d.\n", EINVAL ) );
		return ( EINVAL );
		
	}
	
	// Make sure we don't return partial entries
	if ( ( uint32_t ) uio_resid ( uio ) < sizeof ( struct dirent ) )
	{
		
		DebugLog ( ( "resid < dirent size, exiting with error = %d.\n", EINVAL ) );
		return ( EINVAL );
		
	}
	
	direntSize	= ( UInt32 ) sizeof ( struct dirent );
	
	// Synthesize '.', "..", and ".TOC.plist"
	if ( uio_offset ( uio ) == 0 )
	{
		
		offsetValue = AddDirectoryEntry ( cddaNodePtr->nodeID, DT_DIR, ".", uio );
		if ( offsetValue == 0 )
		{
			
			DebugLog ( ( "offsetValue is zero, exiting with error = %d.\n", 0 ) );
			return 0;
			
		}
		
	}
	
	if ( uio_offset ( uio ) == direntSize )
	{
		
		offsetValue = AddDirectoryEntry ( cddaNodePtr->nodeID, DT_DIR, "..", uio );
		if ( offsetValue == 0 )
		{
			
			DebugLog ( ( "offsetValue is zero, exiting with error = %d.\n", 0 ) );
			return 0;
			
		}
		
	}
	
	if ( uio_offset ( uio ) == direntSize * kAppleCDDARootFileID )
	{
		
		offsetValue += AddDirectoryEntry ( kAppleCDDAXMLFileID, kAppleCDDAXMLFileType, ".TOC.plist", uio );
		if ( offsetValue == 0 )
		{
			
			DebugLog ( ( "offsetValue is zero, exiting with error = %d.\n", 0 ) );
			return 0;
			
		}
		
	}
	
	nodeInfoArrayPtr	= VFSTONODEINFO ( vnode_mount ( vNodePtr ) );
	cddaMountPtr		= VFSTOCDDA ( vnode_mount ( vNodePtr ) );
	
	DebugAssert ( ( nodeInfoArrayPtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	DebugLog ( ( "cddaMountPtr->numTracks = %ld.\n", cddaMountPtr->numTracks ) );
	DebugLog ( ( "buffer size needed = %ld.\n", direntSize * ( cddaMountPtr->numTracks + kNumberOfFakeDirEntries ) ) );
	
	// OK, so much for the fakes.  Now for the "real thing"
	// Loop over all the names in the NameArray to produce directory entries
	for ( index = 0; index < cddaMountPtr->numTracks; index++, nodeInfoArrayPtr++ )
	{
		
		DebugLog ( ( "uio_offset ( uio ) = %ld.\n", uio_offset ( uio ) ) );
		DebugLog ( ( "uio_resid ( uio ) = %ld.\n", uio_resid ( uio ) ) );
		
		if ( uio_offset ( uio ) == direntSize * ( index + kNumberOfFakeDirEntries ) )
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
		
		DebugLog ( ( "eofflag = %d.\n", ( uio_offset ( uio ) >= direntSize * ( cddaMountPtr->numTracks + kNumberOfFakeDirEntries ) ) ? 1 : 0 ) );
		
		// If we ran all the way through the list, there are no more
		*readDirArgsPtr->a_eofflag = ( uio_offset ( uio ) >= direntSize * ( cddaMountPtr->numTracks + kNumberOfFakeDirEntries ) ) ? 1 : 0;
		error = 0;
		
	}
	
	DebugLog ( ( "CDDA_ReadDir: exiting with error = %d.\n", error ) );
	
	return ( error );
	
}


//-----------------------------------------------------------------------------
//	CDDA_PageIn -	This routine handles VM PageIn requests
//-----------------------------------------------------------------------------

int
CDDA_PageIn ( struct vnop_pagein_args * pageInArgsPtr )
/*
struct vnop_pagein_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	upl_t a_pl;
	vm_offset_t a_pl_offset;
	off_t a_f_offset;
	size_t a_size;
	int a_flags;
	vfs_context_t a_context;
};
*/
{
	
	vnode_t				vNodePtr		= NULLVP;
	AppleCDDANodePtr	cddaNodePtr		= NULL;
	int					error			= 0;
	int					nocommit		= 0;
	UInt32				numBytes		= 0;
	
	DebugLog ( ( "CDDA_PageIn: Entering.\n" ) );
	
	DebugAssert ( ( pageInArgsPtr != NULL ) );
	
	vNodePtr = pageInArgsPtr->a_vp;
	nocommit = pageInArgsPtr->a_flags & UPL_NOCOMMIT;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
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
								  ( upl_size_t ) pageInArgsPtr->a_size,
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
								  ( upl_size_t ) pageInArgsPtr->a_size,
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
								  ( upl_size_t ) pageInArgsPtr->a_size,
								  UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY );
			
		}
		
		return ( error );
		
	}
	
	// Workaround for faked ".TOC.plist" file
	if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
	{
				
		kern_return_t		kret			= 0;
		vm_offset_t			vmOffsetPtr		= 0;
		UInt32				amountToCopy	= 0;
					
		// Map the physical page into the kernel address space
		kret = ubc_upl_map ( pageInArgsPtr->a_pl, &vmOffsetPtr );
		
		// If we got an error or the vmOffsetPtr is zero, panic for now
		if ( kret != KERN_SUCCESS || vmOffsetPtr == 0 )
		{
		
			panic ( "CDDA_PageIn: error mapping buffer into kernel space!" );
		
		}
		
		// Zero fill the page
		bzero ( ( caddr_t )( vmOffsetPtr + pageInArgsPtr->a_pl_offset ), PAGE_SIZE );
		
		amountToCopy = ( UInt32 ) __u64min ( PAGE_SIZE, numBytes - pageInArgsPtr->a_f_offset );
		
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
			ubc_upl_commit_range (	pageInArgsPtr->a_pl,
									pageInArgsPtr->a_pl_offset,
									PAGE_SIZE,
									UPL_COMMIT_FREE_ON_EMPTY | UPL_COMMIT_CLEAR_DIRTY );
			
		}
		
		return 0;
			
	}
	
	else if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
	{
		
		UInt32			headerSize		= 0;
		UInt64			blockNumber		= 0;
		UInt32			count			= 0;
		off_t			offset			= 0;
		off_t			sectorOffset	= 0;
		off_t			residual		= 0;
		kern_return_t	kret			= 0;
		vm_offset_t		vmOffsetPtr		= 0;
		buf_t			bufPtr			= NULL;
		
		residual	= pageInArgsPtr->a_size;
		offset		= pageInArgsPtr->a_f_offset;
		
		// Check to make sure we don't read past EOF
		if ( offset > cddaNodePtr->u.file.nodeInfoPtr->numBytes )
		{
			
			DebugLog ( ( "CDDA_PageIn: Can't read past end of file..." ) );
			return ( 0 );
			
		}
		
		headerSize = ( UInt32 ) sizeof ( cddaNodePtr->u.file.aiffHeader );

		// Map the physical pages into the kernel address space
		kret = ubc_upl_map ( pageInArgsPtr->a_pl, &vmOffsetPtr );
		
		// If we got an error or the vmOffsetPtr is zero, panic for now
		if ( kret != KERN_SUCCESS || vmOffsetPtr == 0 )
		{
		
			panic ( "CDDA_PageIn: error mapping buffer into kernel space!" );
		
		}
		
		// Account for the offset into the UPL.
		vmOffsetPtr += pageInArgsPtr->a_pl_offset;
		
		// Copy any part of the header that we need to copy.
		if ( offset < headerSize )
		{
			
			UInt32		amountToCopy	= 0;
			UInt8 *		bytes			= NULL;
			
			amountToCopy = ( UInt32 ) __u64min ( pageInArgsPtr->a_size, headerSize - offset );
			
			bytes = ( UInt8 * ) &cddaNodePtr->u.file.aiffHeader;
			
			// Copy the header data
			bcopy ( &bytes[offset],
					( void * ) vmOffsetPtr,
					amountToCopy );
			
			offset += amountToCopy;
			residual -= amountToCopy;
			vmOffsetPtr += amountToCopy;
			
		}

		// Copy any part of the header pad that we need to copy.
		if ( ( residual > 0 ) &&
			 ( offset < kPhysicalMediaBlockSize ) )
		{
			
			UInt32	amountToCopy = 0;
			
			amountToCopy = ( UInt32 ) __u64min ( residual, kPhysicalMediaBlockSize - offset );
						
			// Copy the header pad data (all zeroes).
			bcopy ( &gAIFFHeaderPadData[offset - headerSize],
					( void * ) vmOffsetPtr,
					amountToCopy );
			
			offset += amountToCopy;
			residual -= amountToCopy;
			vmOffsetPtr += amountToCopy;
			
		}
		
		if ( ( residual > 0 ) &&
			 ( offset < cddaNodePtr->u.file.nodeInfoPtr->numBytes ) )
		{
			
			// Adjust offset by the size of header + header pad so we have a true offset into the media.
			offset -= kPhysicalMediaBlockSize;
			sectorOffset = offset % kPhysicalMediaBlockSize;
			blockNumber = ( offset / kPhysicalMediaBlockSize ) + cddaNodePtr->u.file.nodeInfoPtr->LBA;
			
			// Part 1
			// We do the read in 3 parts. First, we read one sector (picks up any buffer cache entry from a
			// previous Part 3 if it exists).
			{
				
				// Clip to requested transfer count and end of file.
				count = ( UInt32 ) __u64min ( residual, ( kPhysicalMediaBlockSize - sectorOffset ) );
				count = ( UInt32 ) __u64min ( count, cddaNodePtr->u.file.nodeInfoPtr->numBytes - offset );
				
				// Read the one sector
				error = ( int ) buf_meta_bread (
									cddaNodePtr->blockDeviceVNodePtr,
									blockNumber,
									kPhysicalMediaBlockSize,
									NOCRED,
									&bufPtr );
				
				if ( error != 0 )
				{
					
					buf_brelse ( bufPtr );
					return ( error );
					
				}

				// Copy the data
				bcopy ( ( void * ) ( ( char * ) buf_dataptr ( bufPtr ) + sectorOffset ),
						( void * ) vmOffsetPtr,
						count );
				
				// Increment/decrement counters
				offset		+= count;
				residual	-= count;
				vmOffsetPtr += count;
				
				// Make sure we mark this bp invalid as we don't need to keep it around anymore
				buf_markinvalid ( bufPtr );
				
				// Release this buffer back into the buffer pool. 
				buf_brelse ( bufPtr );
				
				// Update offset
				blockNumber++;
				
			}
			
			// Part 2
			// Now we execute the second part of the read. This will be the largest chunk of the read.
			// We will read multiple disc blocks up to MAXBSIZE bytes in a loop until we hit a chunk which
			// is less than one block size. That will be read in the third part.
			
			while ( ( residual > kPhysicalMediaBlockSize ) &&
					( offset < cddaNodePtr->u.file.nodeInfoPtr->numBytes ) )
			{
				
				UInt64		blocksToRead = 0;
				
				// Read in as close to MAXBSIZE chunks as possible
				if ( residual > kMaxBytesPerRead )
				{
					blocksToRead	= kMaxBlocksPerRead;
					count			= kMaxBytesPerRead;
				}
				
				else
				{
					blocksToRead	= residual / kPhysicalMediaBlockSize;
					count			= ( UInt32 ) ( blocksToRead * kPhysicalMediaBlockSize );
				}
				
				// read kMaxBlocksPerRead blocks and put them in the cache.
				error = ( int ) buf_meta_bread (
									cddaNodePtr->blockDeviceVNodePtr,
									blockNumber,
									count,
									NOCRED,
									&bufPtr );
				
				if ( error != 0 )
				{
					
					buf_brelse ( bufPtr );
					return ( error );
					
				}
				
				count = ( UInt32 ) __u64min ( count, cddaNodePtr->u.file.nodeInfoPtr->numBytes - offset );
				
				// Copy the data
				bcopy ( ( void * ) buf_dataptr ( bufPtr ), ( void * ) vmOffsetPtr, count );
				
				// Increment/decrement counters
				offset		+= count;
				residual	-= count;
				vmOffsetPtr += count;
				
				// Make sure we mark any intermediate buffers as invalid as we don't need
				// to keep them.
				buf_markinvalid ( bufPtr );
				
				// Release this buffer back into the buffer pool. 
				buf_brelse ( bufPtr );
				
				// Update offset
				blockNumber += blocksToRead;
				
			}
			
			// Part 3
			// Now that we have read everything, we read the tail end which is a partial sector.
			// Sometimes we don't need to execute this step since there isn't a tail.
			if ( ( residual > 0	 ) &&
				 ( offset < cddaNodePtr->u.file.nodeInfoPtr->numBytes ) )
			{
				
				count = ( UInt32 ) __u64min ( residual, cddaNodePtr->u.file.nodeInfoPtr->numBytes - offset );
				
				// Read the one sector
				error = ( int ) buf_meta_bread (
									cddaNodePtr->blockDeviceVNodePtr,
									blockNumber,
									kPhysicalMediaBlockSize,
									NOCRED,
									&bufPtr );
				
				if ( error != 0 )
				{
					
					buf_brelse ( bufPtr );
					return ( error );
					
				}

				// Copy the data
				bcopy ( ( void * ) buf_dataptr ( bufPtr ), ( void * ) vmOffsetPtr, count );
				
				// Increment/decrement counters
				offset		+= count;
				residual	-= count;
				vmOffsetPtr += count;
				
				// Make sure we mark any intermediate buffers as invalid as we don't need
				// to keep them.
				buf_markinvalid ( bufPtr );
				
				// Release this buffer back into the buffer pool. 
				buf_brelse ( bufPtr );
				
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
			ubc_upl_commit_range (	pageInArgsPtr->a_pl,
									pageInArgsPtr->a_pl_offset,
									( upl_size_t ) pageInArgsPtr->a_size,
									UPL_COMMIT_FREE_ON_EMPTY | UPL_COMMIT_CLEAR_DIRTY );
			
		}
		
	}
	
	DebugLog ( ( "CDDA_PageIn: exiting...\n" ) );
	
	return ( error );
	
}


//-----------------------------------------------------------------------------
//	CDDA_GetAttributes - This routine gets the attributes for a folder/file
//-----------------------------------------------------------------------------

int
CDDA_GetAttributes ( struct vnop_getattr_args * getAttrArgsPtr )
/*
struct vnop_getattr_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	struct vnode_attr *a_vap;
	vfs_context_t a_context;
};
*/
{
	
	vnode_t					vNodePtr		= NULLVP;
	mount_t					mountPtr		= NULL;
	struct vnode_attr *		attributesPtr	= NULL;
	AppleCDDANodePtr		cddaNodePtr		= NULL;
	AppleCDDAMountPtr		cddaMountPtr	= NULL;
	struct timespec			nullTime		= { 0, 0 };
	
	DebugLog ( ( "CDDA_GetAttributes: Entering.\n" ) );
	
	DebugAssert ( ( getAttrArgsPtr != NULL ) );
	
	vNodePtr		= getAttrArgsPtr->a_vp;
	mountPtr		= vnode_mount ( vNodePtr );
	attributesPtr	= getAttrArgsPtr->a_vap;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	DebugAssert ( ( attributesPtr != NULL ) );
	
	cddaMountPtr	= VFSTOCDDA ( mountPtr );
	cddaNodePtr		= VTOCDDA ( vNodePtr );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	DebugLog ( ( "nodeID = %ld.\n", cddaNodePtr->nodeID ) );
	
	// Special case root since we know how to get its name
	if ( cddaNodePtr->nodeType == kAppleCDDADirectoryType )
	{
		
		// Set the nodeID. Force root to be 2.
		VATTR_RETURN ( attributesPtr, va_fileid, kAppleCDDARootFileID );
		
	}
	
	// Special case the XMLFileNode
	else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
	{
		
		// Set the nodeID. Force the XMLFileNode to be 3.
		VATTR_RETURN ( attributesPtr, va_fileid, kAppleCDDAXMLFileID );
		
	}
	
	else
	{
		
		// Set the nodeID.		
		VATTR_RETURN ( attributesPtr, va_fileid, cddaNodePtr->nodeID );
		
	}
	
	if ( cddaNodePtr->nodeType == kAppleCDDADirectoryType )
	{
		
		// If this is the root directory and they want the parent, force it to 1
		VATTR_RETURN ( attributesPtr, va_parentid, 1 );
		
	}
	
	else
	{
		
		// Every other object has the root as its parent (flat filesystem)
		VATTR_RETURN ( attributesPtr, va_parentid, kAppleCDDARootFileID );
		
	}
	
	VATTR_RETURN ( attributesPtr, va_type, vnode_vtype ( vNodePtr ) );	// Set the VNode type (e.g. VREG, VDIR)
	VATTR_RETURN ( attributesPtr, va_iosize, kPhysicalMediaBlockSize );	// Set preferred block size for I/O requests
	
	// Set all the time fields
	VATTR_RETURN ( attributesPtr, va_create_time, cddaMountPtr->mountTime );	// Creation time
	VATTR_RETURN ( attributesPtr, va_modify_time, cddaMountPtr->mountTime );	// Last modification time
	VATTR_RETURN ( attributesPtr, va_change_time, nullTime );					// Last change time
	VATTR_RETURN ( attributesPtr, va_access_time, nullTime );					// Last accessed time
	VATTR_RETURN ( attributesPtr, va_backup_time, nullTime );					// Backup time
	
	// These fields are the same
	VATTR_RETURN ( attributesPtr, va_fsid, vfs_statfs ( mountPtr )->f_fsid.val[0] );
	VATTR_RETURN ( attributesPtr, va_uid, kUnknownUserID );		// "unknown"
	VATTR_RETURN ( attributesPtr, va_gid, kUnknownGroupID );	// "unknown"
	VATTR_RETURN ( attributesPtr, va_filerev, 0 );
	VATTR_RETURN ( attributesPtr, va_gen, 0 );
	VATTR_RETURN ( attributesPtr, va_flags, 0 );
	VATTR_RETURN ( attributesPtr, va_rdev, 0 );
	VATTR_RETURN ( attributesPtr, va_encoding, 0 );
	
	// Set some common mode flags.
	// Read is ok for user, group, other.
	VATTR_RETURN ( attributesPtr, va_mode, S_IRUSR | S_IRGRP | S_IROTH );
	
	// If it's the root, set some flags for it.
	if ( vnode_isvroot ( vNodePtr ) )
	{
		
		attributesPtr->va_mode		|= S_IFDIR;							// It's a directory
		attributesPtr->va_mode		|= S_IXUSR | S_IXGRP | S_IXOTH;		// Execute is ok for user, group, other
		
		// Number of file refs: "." and ".."
		VATTR_RETURN ( attributesPtr, va_nlink, kAppleCDDANumberOfRootDirReferences );		
		VATTR_RETURN ( attributesPtr, va_nchildren, cddaNodePtr->u.directory.entryCount - kAppleCDDANumberOfRootDirReferences );
		
		// Number of Tracks + ".", "..", and ".TOC.plist"
		VATTR_RETURN ( attributesPtr, va_data_size, ( cddaNodePtr->u.directory.entryCount ) * sizeof ( struct dirent ) );
		
	}
	
	// If it isn't the root vnode, it's a file.
	else
	{
		
		// It's a file...
		attributesPtr->va_mode |= DEFFILEMODE;
		
		// Just the file itself
		VATTR_RETURN ( attributesPtr, va_nlink, kAppleCDDANumberOfFileReferences );
		
		// Is it a track?
		if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
		{
			
			// Set file size in bytes
			VATTR_RETURN ( attributesPtr, va_data_size, cddaNodePtr->u.file.nodeInfoPtr->numBytes );
			VATTR_RETURN ( attributesPtr, va_data_alloc, cddaNodePtr->u.file.nodeInfoPtr->numBytes );
			
		}
		
		// Is it the ".TOC.plist" file?
		else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
		{
			
			// Set file size in bytes.
			VATTR_RETURN ( attributesPtr, va_data_size, cddaNodePtr->u.xmlFile.fileSize );
			VATTR_RETURN ( attributesPtr, va_data_alloc, cddaNodePtr->u.xmlFile.fileSize );
			
		}
		
	}
	
	DebugLog ( ( "CDDA_GetAttributes: exiting...\n" ) );
	
	return ( 0 );
	
}


//-----------------------------------------------------------------------------
//	CDDA_Inactive - This routine simply unlocks a vnode.
//-----------------------------------------------------------------------------

int
CDDA_Inactive ( struct vnop_inactive_args * inactiveArgsPtr )
/*
struct vnop_inactive_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	vfs_context_t a_context;
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


//-----------------------------------------------------------------------------
//	CDDA_Remove -	This routine removes a file from the name space. Since we
//					are a read-only volume, we release any locks if appropriate
//					and return EROFS
//-----------------------------------------------------------------------------

int 
CDDA_Remove ( struct vnop_remove_args * removeArgsPtr )
/*
struct vnop_remove_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_dvp;
	vnode_t a_vp;
	struct componentname *a_cnp;
	int a_flags;
	vfs_context_t a_context;
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


//-----------------------------------------------------------------------------
//	CDDA_RmDir -	This routine removes a directory from the name space.
//					Since we are a read-only volume, we release any locks
//					and return EROFS
//-----------------------------------------------------------------------------

int 
CDDA_RmDir ( struct vnop_rmdir_args * removeDirArgsPtr )
/*
struct vnop_rmdir_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_dvp;
	vnode_t a_vp;
	struct componentname *a_cnp;
	vfs_context_t a_context;
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


//-----------------------------------------------------------------------------
//	CDDA_Reclaim - This routine reclaims a vnode for use by the system.
//
//	Remove the vnode from our node info array, drop the fs reference,
//	free our private data.
//-----------------------------------------------------------------------------

int
CDDA_Reclaim ( struct vnop_reclaim_args * reclaimArgsPtr )
/*
struct vnop_reclaim_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	vfs_context_t a_context;
};
*/
{
	
	vnode_t					vNodePtr			= NULLVP;
	AppleCDDANodePtr		cddaNodePtr			= NULL;
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	AppleCDDANodeInfoPtr	nodeInfoPtr			= NULL;
	int						error				= 0;
	
	DebugLog ( ( "CDDA_Reclaim: Entering.\n" ) );
	
	DebugAssert ( ( reclaimArgsPtr != NULL ) );
	
	vNodePtr = reclaimArgsPtr->a_vp;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	cddaMountPtr = VFSTOCDDA ( vnode_mount ( vNodePtr ) );
	
	lck_mtx_lock ( cddaMountPtr->cddaMountLock );
	
	if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
	{
		
		nodeInfoPtr = CDDATONODEINFO ( cddaNodePtr );
		nodeInfoPtr->vNodePtr = NULL;
		
	}
	
	else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
	{
		
		cddaMountPtr->xmlFileVNodePtr = NULL;
		
	}
	
	lck_mtx_unlock ( cddaMountPtr->cddaMountLock );
	
	// Release our reference on this node
	vnode_removefsref ( vNodePtr );
	
	error = DisposeCDDANode ( vNodePtr );
	
	DebugLog ( ( "CDDA_Reclaim: exiting...\n" ) );
	
	return ( error );

}



//-----------------------------------------------------------------------------
//	CDDA_BlockToOffset -	This routine converts logical block number to file
//							offset
//-----------------------------------------------------------------------------

int
CDDA_BlockToOffset ( struct vnop_blktooff_args * blockToOffsetArgsPtr )
/*
struct vnop_blktooff_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	daddr64_t a_lblkno;
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


//-----------------------------------------------------------------------------
//	CDDA_OffsetToBlock -	This routine converts a file offset to a logical
//							block number
//-----------------------------------------------------------------------------

int
CDDA_OffsetToBlock ( struct vnop_offtoblk_args * offsetToBlockArgsPtr )
/*
struct vnop_offtoblk_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	off_t a_offset;
	daddr64_t *a_lblkno;
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


//-----------------------------------------------------------------------------
//	CDDA_Pathconf - Return POSIX pathconf information applicable to
//					special devices
//-----------------------------------------------------------------------------

int
CDDA_Pathconf ( struct vnop_pathconf_args * pathConfArgsPtr )
/*
struct vnop_pathconf_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	int a_name;
	register_t *a_retval;
	vfs_context_t a_context;
};
*/
{
	
	int returnValue = 0;
	
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

//-----------------------------------------------------------------------------
//	CDDA_GetXAttr - Handles extended attribute reads.
//					In this case, just for FinderInfo.
//-----------------------------------------------------------------------------

int
CDDA_GetXAttr ( struct vnop_getxattr_args * getXAttrArgsPtr )
/*
struct vnop_getxattr_args {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	char * a_name;
	uio_t a_uio;
	size_t
	*a_size;
	int a_options;
	vfs_context_t a_context;
};
*/
{
	
	AppleCDDANodePtr	cddaNodePtr		= NULL;
	FinderInfo *		finderInfoPtr	= NULL;
	char				buf[32]			= { 0 };
	
	DebugLog ( ( "CDDA_GetXAttr: Entering.\n" ) );
	DebugAssert ( ( getXAttrArgsPtr != NULL ) );
	
	if ( strncmp ( getXAttrArgsPtr->a_name, XATTR_FINDERINFO_NAME, sizeof ( XATTR_FINDERINFO_NAME ) ) != 0 )
	{
		return ( ENOATTR );
	}
	
	cddaNodePtr		= VTOCDDA ( getXAttrArgsPtr->a_vp );
	finderInfoPtr	= ( FinderInfo * ) buf;
	
	if ( !vnode_isvroot ( getXAttrArgsPtr->a_vp ) )
	{
		
		if ( cddaNodePtr->nodeID == kAppleCDDAXMLFileID )
		{
			
			DebugLog ( ( "kFinderInfoInvisibleMask\n" ) );
			// Make the XML file invisible
			finderInfoPtr->finderFlags = kFinderInfoInvisibleMask;
			
		}
		
		else
		{
			finderInfoPtr->finderFlags = kFinderInfoNoFileExtensionMask;
		}
		
		finderInfoPtr->location.v	= -1;
		finderInfoPtr->location.h	= -1;
		
		if ( vnode_isreg ( getXAttrArgsPtr->a_vp ) && ( cddaNodePtr->nodeID != kAppleCDDAXMLFileID ) )
		{
			
			DebugLog ( ( "fileType, creator\n" ) );
			finderInfoPtr->fileType		= VFSTOCDDA ( vnode_mount ( cddaNodePtr->vNodePtr ) )->fileType;
			finderInfoPtr->fileCreator	= VFSTOCDDA ( vnode_mount ( cddaNodePtr->vNodePtr ) )->fileCreator;
			
		}
		
		// Swap FinderInfo into big endian. FinderInfo must always be big endian when passed
		// back and forth across the kernel using getattrlist/getxattr.
		finderInfoPtr->fileType 	= OSSwapHostToBigInt32 ( finderInfoPtr->fileType );
		finderInfoPtr->fileCreator 	= OSSwapHostToBigInt32 ( finderInfoPtr->fileCreator );
		finderInfoPtr->finderFlags 	= OSSwapHostToBigInt16 ( finderInfoPtr->finderFlags );
		finderInfoPtr->location.v 	= OSSwapHostToBigInt16 ( finderInfoPtr->location.v );
		finderInfoPtr->location.h 	= OSSwapHostToBigInt16 ( finderInfoPtr->location.h );
		
	}
	
	return ( uiomove ( ( caddr_t ) buf, ( int ) sizeof ( buf ), getXAttrArgsPtr->a_uio ) );
	
}


//-----------------------------------------------------------------------------
//	Other macro'd function definitions
//-----------------------------------------------------------------------------

int ( **gCDDA_VNodeOp_p )( void * );
typedef int (*VNOPFUNC) ( void * );


#if 0
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	VNode Operation Vector Entry Description (Dispatch Table)
//-----------------------------------------------------------------------------

struct vnodeopv_entry_desc gCDDA_VNodeOperationEntries[] =
{
	{ &vnop_default_desc,		( VNOPFUNC ) vn_default_error },
	{ &vnop_lookup_desc,		( VNOPFUNC ) CDDA_Lookup },				// lookup
	{ &vnop_open_desc,			( VNOPFUNC ) CDDA_Open },				// open
	{ &vnop_close_desc,			( VNOPFUNC ) CDDA_Close },				// close
	{ &vnop_getattr_desc,		( VNOPFUNC ) CDDA_GetAttributes },		// getattr
	{ &vnop_setattr_desc,		( VNOPFUNC ) nop_setattr },				// setattr
	{ &vnop_read_desc,			( VNOPFUNC ) CDDA_Read },				// read
	{ &vnop_fsync_desc,			( VNOPFUNC ) nop_fsync },				// fsync
	{ &vnop_remove_desc,		( VNOPFUNC ) CDDA_Remove },				// remove
	{ &vnop_rmdir_desc,			( VNOPFUNC ) CDDA_RmDir },				// rmdir
	{ &vnop_readdir_desc,		( VNOPFUNC ) CDDA_ReadDir },			// readdir
	{ &vnop_inactive_desc,		( VNOPFUNC ) CDDA_Inactive },			// inactive
	{ &vnop_reclaim_desc,		( VNOPFUNC ) CDDA_Reclaim },			// reclaim
	{ &vnop_pathconf_desc,		( VNOPFUNC ) CDDA_Pathconf },			// pathconf
	{ &vnop_pagein_desc,		( VNOPFUNC ) CDDA_PageIn },				// pagein
	{ &vnop_blktooff_desc,		( VNOPFUNC ) CDDA_BlockToOffset },		// blktoff
	{ &vnop_offtoblk_desc,		( VNOPFUNC ) CDDA_OffsetToBlock },		// offtoblk
	{ &vnop_getxattr_desc,		( VNOPFUNC ) CDDA_GetXAttr },			// getxattr
	{ NULL,						( VNOPFUNC ) NULL }
};


struct vnodeopv_desc gCDDA_VNodeOperationsDesc =
{
	&gCDDA_VNodeOp_p,
	gCDDA_VNodeOperationEntries
};


//-----------------------------------------------------------------------------
//				End				Of			File
//-----------------------------------------------------------------------------
