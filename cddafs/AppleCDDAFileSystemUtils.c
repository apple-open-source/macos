/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 
// AppleCDDAFileSystemUtils.c created by CJS on Sun 14-May-2000

#ifndef __APPLE_CDDA_FS_UTILS_H__
#include "AppleCDDAFileSystemUtils.h"
#endif

#ifndef __APPLE_CDDA_FS_DEBUG_H__
#include "AppleCDDAFileSystemDebug.h"
#endif

#ifndef __APPLE_CDDA_FS_DEFINES_H__
#include "AppleCDDAFileSystemDefines.h"
#endif

#ifndef __AIFF_SUPPORT_H__
#include "AIFFSupport.h"
#endif

#ifndef __APPLE_CDDA_FS_VFS_OPS_H__
#include "AppleCDDAFileSystemVFSOps.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/attr.h>
#include <sys/time.h>
#include <sys/ubc.h>

#include <miscfs/specfs/specdev.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Static Function Prototypes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


static int 		BuildTrackName 					( struct mount * mountPtr, AppleCDDANodeInfoPtr nodeInfoPtr );
static UInt32	CalculateNumberOfDescriptors 	( const QTOCDataFormat10Ptr TOCDataPtr );
static UInt32	CalculateLBA 					( SubQTOCInfoPtr trackDescriptorPtr );
static int		FindName						( struct mount * mountPtr, UInt8 trackNumber, char ** name, UInt8 * nameSize );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateNewCDDANode -	This routine is responsible for creating new nodes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CreateNewCDDANode ( struct mount * mountPtr,
					UInt32 nodeID,
					struct proc * procPtr,
					struct vnode ** vNodeHandle )
{
	
	int 					result			= 0;
	AppleCDDANodePtr		cddaNodePtr		= NULL;
	struct vnode *			vNodePtr		= NULL;
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( procPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );

	// Allocate the cddaNode
	MALLOC ( cddaNodePtr, AppleCDDANodePtr, sizeof ( AppleCDDANode ), M_TEMP, M_WAITOK );
		
	// Zero the structure
	bzero ( cddaNodePtr, sizeof ( *cddaNodePtr ) );
	
	// Init the node lock
	lockinit ( &cddaNodePtr->lock, PINOD, "cddanode", 0, 0 );

	// Set the nodeID
	cddaNodePtr->nodeID = nodeID;

	// Note that getnewvnode ( ) returns the vnode with a refcount of +1;
	// this routine returns the newly created vnode with this positive refcount.
	result = getnewvnode ( VT_CDDA, mountPtr, gCDDA_VNodeOp_p, &vNodePtr );
	if ( result != 0 )
	{
	
		DebugLog ( ( "getnewvnode failed with error code %d\n", result ) );
		goto FREE_CDDA_NODE_ERROR;
		
	}
		
	// Link the vnode and cddaNode together
	vNodePtr->v_data 		= cddaNodePtr;
	cddaNodePtr->vNodePtr 	= vNodePtr;
	
	// Stuff the vnode in
	*vNodeHandle = vNodePtr;
	
	return result;
	
	
FREE_CDDA_NODE_ERROR:
	
	
	// Free the allocated memory
	FREE ( ( caddr_t ) cddaNodePtr, M_TEMP );
	cddaNodePtr = NULL;
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	DisposeCDDANode -	This routine is responsible for cleaning up cdda nodes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
DisposeCDDANode ( struct vnode * vNodePtr,
				  struct proc * theProcPtr )
{
	
	AppleCDDANodePtr	cddaNodePtr		= NULL;
	AppleCDDAMountPtr	cddaMountPtr	= NULL;
	
	DebugAssert ( ( vNodePtr != NULL ) );
	
	cddaNodePtr = VTOCDDA ( vNodePtr );
	cddaMountPtr = VFSTOCDDA ( vNodePtr->v_mount );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	
	if ( cddaNodePtr != NULL )
	{
		
		// Check if it's a directory or a file
		if ( cddaNodePtr->nodeType == kAppleCDDADirectoryType )
		{
			
			// Just get rid of the name we allocated for the directory
			FREE ( cddaNodePtr->u.directory.name, M_TEMP );
			cddaNodePtr->u.directory.name = NULL;
			
		}
		
		else if ( cddaNodePtr->nodeType == kAppleCDDATrackType )
		{
			
			if ( cddaNodePtr->u.file.nodeInfoPtr != NULL )
			{
				
				// Remove this vnode from our list
				cddaNodePtr->u.file.nodeInfoPtr->vNodePtr 	= NULL;
				cddaNodePtr->u.file.nodeInfoPtr 			= NULL;
				
			}
			
		}
		
		else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
		{
			
			FREE ( cddaNodePtr->u.xmlFile.fileDataPtr, M_TEMP );
			cddaNodePtr->u.xmlFile.fileDataPtr 	= NULL;
			cddaNodePtr->u.xmlFile.fileSize 	= 0;
			
		}
		
		// Free memory associated with our filesystem's internal data
		FREE ( vNodePtr->v_data, M_TEMP );
		vNodePtr->v_data = NULL;
	
	}
		
	return ( 0 );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateNewCDDAFile -	This routine is responsible for creating new
//						files
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CreateNewCDDAFile ( struct mount * mountPtr,
					UInt32 nodeID,
					AppleCDDANodeInfoPtr nodeInfoPtr,
					struct proc * procPtr,
					struct vnode ** vNodeHandle )
{
	
	int						result				= 0;
	struct vnode *			vNodePtr			= NULL;
	AppleCDDANodePtr		cddaNodePtr			= NULL;
	AppleCDDANodePtr		parentCDDANodePtr	= NULL;
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( nodeInfoPtr != NULL ) );
	DebugAssert ( ( procPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );
	
	result = CreateNewCDDANode ( mountPtr, nodeID, procPtr, &vNodePtr );
	if ( result != 0 )
	{
		
		DebugLog ( ( "Error = %d returned from CreatNewCDDANode\n", result ) );
		return result;
		
	}
	
	cddaNodePtr 		= VTOCDDA ( vNodePtr );
	cddaMountPtr		= VFSTOCDDA ( mountPtr );
	parentCDDANodePtr	= VTOCDDA ( cddaMountPtr->root );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	DebugAssert ( ( parentCDDANodePtr != NULL ) );

	// Initialize the relevant vnode fields
	vNodePtr->v_type = VREG;
	
	// Fill in the miscellaneous fields for the cddaNode
	cddaNodePtr->u.file.aiffHeader		= BuildCDAIFFHeader ( nodeInfoPtr->numBytes );
	cddaNodePtr->nodeType 				= kAppleCDDATrackType;
	cddaNodePtr->blockDeviceVNodePtr	= parentCDDANodePtr->blockDeviceVNodePtr;
	
	// Set the back pointer
	cddaNodePtr->u.file.nodeInfoPtr	= nodeInfoPtr;
	
	DebugLog ( ( "LBA of %d = %ld.\n", cddaNodePtr->nodeID, nodeInfoPtr->LBA ) );
	
	// Init the UBC info
	if ( UBCINFOMISSING ( vNodePtr ) || UBCINFORECLAIMED ( vNodePtr ) )
	{
		
		ubc_info_init ( vNodePtr );
		
	}

	// stuff the vNode in
	*vNodeHandle = vNodePtr;
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateNewXMLFile -	This routine is responsible for creating the ".cdid"
//						file which has the unique disc ID string
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CreateNewXMLFile ( 	struct mount * mountPtr,
					struct proc * procPtr,
					UInt32 xmlFileSize,
					UInt8 * xmlData,
					struct vnode ** vNodeHandle )
{
	
	int						result				= 0;
	struct vnode *			vNodePtr			= NULL;
	AppleCDDANodePtr		cddaNodePtr			= NULL;
	AppleCDDANodePtr		parentCDDANodePtr	= NULL;
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( procPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );

	result = CreateNewCDDANode ( mountPtr, kAppleCDDAXMLFileID, procPtr, &vNodePtr );
	if ( result != 0 )
	{
		
		DebugLog ( ( "Error = %d returned from CreatNewCDDANode\n", result ) );
		return result;
		
	}
	
	cddaNodePtr 		= VTOCDDA ( vNodePtr );
	cddaMountPtr		= VFSTOCDDA ( mountPtr );
	parentCDDANodePtr	= VTOCDDA ( cddaMountPtr->root );
	
	DebugAssert ( ( cddaNodePtr != NULL ) );
	DebugAssert ( ( cddaMountPtr != NULL ) );
	DebugAssert ( ( parentCDDANodePtr != NULL ) );
	
	// Initialize the relevant vnode fields
	vNodePtr->v_type = VREG;
	
	// Fill in the miscellaneous fields for the cddaNode
	cddaNodePtr->nodeType 				= kAppleCDDAXMLFileType;
	cddaNodePtr->blockDeviceVNodePtr	= parentCDDANodePtr->blockDeviceVNodePtr;
	
	// Point the xmlData to the correct place
	cddaNodePtr->u.xmlFile.fileDataPtr 	= xmlData;
	cddaNodePtr->u.xmlFile.fileSize 	= xmlFileSize;
	
	#if 0
	{
		UInt32	count;
		// Let's see if we got the right data mapped in
		for ( count = 0; count < xmlFileSize; count = count + 8 )
		{
			
			DebugLog ( ( "%x:%x:%x:%x %x:%x:%x:%x\n",
						xmlData[count],
						xmlData[count+1],
						xmlData[count+2],
						xmlData[count+3],
						xmlData[count+4],
						xmlData[count+5],
						xmlData[count+6],
						xmlData[count+7] ) );
			
		}
		
		DebugLog ( ( "\n" ) );
		
	}
	#endif
	
	// Init the UBC info
	if ( UBCINFOMISSING ( vNodePtr ) || UBCINFORECLAIMED ( vNodePtr ) )
	{
		
		ubc_info_init ( vNodePtr );
		
	}

	// stuff the vNode in
	*vNodeHandle = vNodePtr;
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateNewCDDADirectory -	This routine is responsible for creating new
//								directories
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CreateNewCDDADirectory ( struct mount * mountPtr,
						 const char * name,
						 UInt32 nodeID,
						 struct proc * procPtr,
						 struct vnode ** vNodeHandle )
{
	
	int						result			= 0;
	struct vnode *			vNodePtr		= NULL;
	AppleCDDANodePtr		cddaNodePtr		= NULL;
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( procPtr != NULL ) );
	DebugAssert ( ( vNodeHandle != NULL ) );

	result = CreateNewCDDANode ( mountPtr, nodeID, procPtr, &vNodePtr );
	if ( result != 0 )
	{
		
		DebugLog ( ( "Error = %d returned from CreatNewCDDANode\n", result ) );
		return result;
		
	}
		
	cddaNodePtr = VTOCDDA ( vNodePtr );
	DebugAssert ( ( cddaNodePtr != NULL ) );
	
	// Initialize the relevant vnode fields
	vNodePtr->v_type = VDIR;
	
	// Set up the directory-specific fields
	cddaNodePtr->nodeType 					= kAppleCDDADirectoryType;
	cddaNodePtr->u.directory.directorySize	= 0;			
	cddaNodePtr->u.directory.entryCount 	= kNumberOfFakeDirEntries; 		// ".", "..", and ".TOC.plist"
	
	name = name + 8;
	
	// Allocate memory for the directory name
	MALLOC ( cddaNodePtr->u.directory.name, char *, strlen ( name ) + 1, M_TEMP, M_WAITOK );
	
	// Copy the name over
	strcpy ( cddaNodePtr->u.directory.name, name );
	
	// stuff the vNode in
	*vNodeHandle = vNodePtr;
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	IsAudioTrack -	Checks the arguments passed in to find out if specified
//					track is audio or not
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

boolean_t
IsAudioTrack ( const SubQTOCInfoPtr trackDescriptorPtr )
{
	
	DebugAssert ( ( trackDescriptorPtr != NULL ) );

	// Check to make sure the point is between 1 and 99 (inclusive)
	if ( trackDescriptorPtr->point < 100 && trackDescriptorPtr->point > 0 )
	{
		
		// Do we have digital data?
		if ( ( trackDescriptorPtr->control & kDigitalDataMask ) == 0 )
		{
			
			// Found an audio track
			return TRUE;
			
		}
		
	}
	
	return FALSE;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CalculateSize -	Calculate the file size based on number of frames
//					(i.e. blocks) in the track
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
CalculateSize ( const QTOCDataFormat10Ptr TOCDataPtr,
				UInt32 trackDescriptorOffset,
				UInt32 currentA2Offset )
{

	UInt32				size					= 0;
	UInt32				offset					= 0;
	UInt32				numberOfDescriptors		= 0;
	UInt32				nextOffset				= 0;
	SubQTOCInfoPtr		trackDescriptorPtr		= NULL;
	SubQTOCInfoPtr		nextTrackDescriptorPtr	= NULL;
	
	DebugLog ( ( "CalculateSize: Entering...\n" ) );
	
	DebugAssert ( ( TOCDataPtr != NULL ) );
	
	// Find the number of descriptors
	numberOfDescriptors = CalculateNumberOfDescriptors ( TOCDataPtr );
	
	// Get the correct track descriptor
	trackDescriptorPtr = &TOCDataPtr->trackDescriptors[trackDescriptorOffset];
	
	// Are we past the total number of descriptors in TOC?
	if ( trackDescriptorOffset + 1 >= numberOfDescriptors )
	{
	
		// yes, so set the descriptor to the last leadout descriptor we hit 
		nextTrackDescriptorPtr = &TOCDataPtr->trackDescriptors[currentA2Offset];
	
	}
	
	else
	{

		// no, so set the descriptor to the next entry in the TOC
		nextTrackDescriptorPtr = &TOCDataPtr->trackDescriptors[trackDescriptorOffset + 1];
		
		// Are we past the end of the session?
		if ( trackDescriptorPtr->sessionNumber != nextTrackDescriptorPtr->sessionNumber ||
			 !IsAudioTrack ( nextTrackDescriptorPtr ) )
		{
			
			// yes, so set the descriptor to the last leadout descriptor we hit 
			nextTrackDescriptorPtr = &TOCDataPtr->trackDescriptors[currentA2Offset];
		
		}
		
	}
	
	// Calculate the LBAs for both tracks
	offset 		= CalculateLBA ( trackDescriptorPtr );
	nextOffset 	= CalculateLBA ( nextTrackDescriptorPtr );
	
	// Multiply number of blocks by block size and add the header
	size = ( ( nextOffset - offset ) * kPhysicalMediaBlockSize ) + sizeof ( CDAIFFHeader );
	
	DebugLog ( ( "CalculateSize: size = %ld.\n", size ) );
	
	DebugLog ( ( "CalculateSize: exiting...\n" ) );
	
	return size;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FindName - 	Parses the names data that gets passed in to the
//				filesystem, looking for the specified track's name.
//				All names look like the following packed structure:
//
//	| 	1 byte 		| 		1 byte 		| 	number of bytes in 2nd byte		|
//		Track #	 		size of String			String for Name
//
//	Track # of zero corresponds to the album name (the mount point name).
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


int
FindName ( struct mount * mountPtr, UInt8 trackNumber, char ** name, UInt8 * nameSize )
{
	
	AppleCDDAMountPtr		cddaMountPtr		= NULL;
	UInt8 *					ptr					= NULL;
	UInt8					length				= 0;
	
	DebugLog ( ( "FindName: entering\n" ) );
	DebugLog ( ( "trackNumber = %d\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( name != NULL ) );
	DebugAssert ( ( nameSize != NULL ) );
	
	cddaMountPtr = VFSTOCDDA ( mountPtr );
	DebugLog ( ( "cddaMountPtr->nameDataSize = %ld\n", cddaMountPtr->nameDataSize ) );
	
	ptr = cddaMountPtr->nameData;
	
	if ( ptr == NULL )
	{
		
		DebugLog ( ( "cddaMountPtr->nameData is NULL" ) );
		return ENOENT;
		
	}
	
	do
	{
				
		DebugLog ( ( "*ptr = %d\n", *ptr ) );
		
		if ( *ptr == trackNumber )
		{
			
			char	mylocalname[512];
			
			DebugLog ( ( "Found track = %d\n", trackNumber ) );
			
			*nameSize 	= ptr[1];
			*name 		= &ptr[2];
			
			bcopy ( &ptr[2], mylocalname, *nameSize );
			mylocalname[*nameSize] = 0;
			
			DebugLog ( ( "NameSize = %d\n", *nameSize ) );
			DebugLog ( ( "Name = %s\n", mylocalname ) );
			
			break;
			
		}
		
		else
		{
			
			length = ptr[1];
			
			DebugLog ( ( "Didn't find it, keep looking\n" ) );
			ptr = &ptr[length + 2];
			
		}
		
	} while ( ptr < ( cddaMountPtr->nameData + cddaMountPtr->nameDataSize ) );
	
	DebugLog ( ( "FindName: exiting\n" ) );
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	ParseTOC - 	Parses the TOC to find audio tracks. It figures out which
//				tracks are audio and what their offsets are and fills in the
//				cddaNode structures associated with each vnode
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SInt32
ParseTOC ( struct mount * mountPtr,
		   UInt32 numTracks,
		   UInt32 xmlFileSize,
		   UInt8 * xmlData,
		   struct proc * theProc )
{

	QTOCDataFormat10Ptr			TOCDataPtr			= NULL;
	SubQTOCInfoPtr				trackDescriptorPtr	= NULL;
	AppleCDDAMountPtr			cddaMountPtr		= NULL;
	AppleCDDANodeInfoPtr		nodeInfoPtr			= NULL;
	AppleCDDADirectoryNodePtr	rootDirNodePtr		= NULL;
	OSStatus					error				= 0;
	UInt16						numberOfDescriptors = 0;
	char *						ioBSDNamePtr		= NULL;
	UInt32						currentA2Offset		= 0;
	UInt32						currentOffset		= 0;
	
	DebugLog ( ( "ParseTOC: Entering...\n" ) );
	
	DebugAssert ( ( mountPtr != NULL ) );
	DebugAssert ( ( theProc != NULL ) );

	cddaMountPtr 	= VFSTOCDDA ( mountPtr );
	rootDirNodePtr	= &( ( VTOCDDA ( cddaMountPtr->root ) )->u.directory );
	ioBSDNamePtr	= mountPtr->mnt_stat.f_mntfromname;
	
	DebugAssert ( ( cddaMountPtr != NULL ) );
	DebugAssert ( ( rootDirNodePtr != NULL ) );
	DebugAssert ( ( ioBSDNamePtr != NULL ) );

	// Get the data from the registry entry
	TOCDataPtr = CreateBufferFromIORegistry ( mountPtr );
	
	if ( TOCDataPtr != NULL )
	{
		
		// Generate the xml TOC file for this disc
		error = CreateNewXMLFile ( 	mountPtr, theProc, xmlFileSize, xmlData,
									&cddaMountPtr->xmlFileVNodePtr );
		if ( error != 0 )
		{
			
			DebugLog ( ( "Error = %d creating xml file.\n", error ) );
			goto Exit;
			
		}
		
		// calculate number of track descriptors
		numberOfDescriptors = CalculateNumberOfDescriptors ( TOCDataPtr );
		DebugLog ( ( "Number of descriptors = %d\n", numberOfDescriptors ) );

		if ( numberOfDescriptors <= 0 )
		{
			
			AppleCDDANodePtr	cddaNodePtr;
			
			// This is bad...no track descriptors, time to bail
			error = EINVAL;
			
			cddaNodePtr = VTOCDDA ( cddaMountPtr->xmlFileVNodePtr );
			
			// Don't forget to release memory for xmlFile			
			FREE ( cddaNodePtr->u.xmlFile.fileDataPtr, M_TEMP );
			cddaNodePtr->u.xmlFile.fileDataPtr 	= NULL;
			cddaNodePtr->u.xmlFile.fileSize 	= 0;

			goto Exit;
			
		}
			
		trackDescriptorPtr = TOCDataPtr->trackDescriptors;

		while ( numberOfDescriptors > 0 && rootDirNodePtr->entryCount < ( numTracks + kNumberOfFakeDirEntries ) )
		{
						
			if ( trackDescriptorPtr->point == 0xA2 )
			{
				
				// Set the a2 offset when we find an a2 point
				currentA2Offset = currentOffset;
			
			}
			
			// Is this an audio track?
			if ( IsAudioTrack ( trackDescriptorPtr ) )
			{
				
				// Make this easier to read by getting a pointer to the nodeInfo
				nodeInfoPtr = &cddaMountPtr->nodeInfoArrayPtr[rootDirNodePtr->entryCount - kNumberOfFakeDirEntries];
				
				// Copy this trackDescriptor into the AppleCDDANodeInfo array
				nodeInfoPtr->trackDescriptor = *trackDescriptorPtr;				
				
				// Get the LogicalBlockAddress and number of bytes in the track
				nodeInfoPtr->LBA 		= CalculateLBA ( trackDescriptorPtr );
				nodeInfoPtr->numBytes 	= CalculateSize ( TOCDataPtr, currentOffset, currentA2Offset );
				
				// Add this node's size to the root directory's directorySize field
				rootDirNodePtr->directorySize += nodeInfoPtr->numBytes;
				
				// Increment the number of audio tracks
				rootDirNodePtr->entryCount++;
				
				( void ) BuildTrackName ( mountPtr, nodeInfoPtr );
				
				DebugLog ( ( "LBA of %d = %ld.\n", trackDescriptorPtr->point, nodeInfoPtr->LBA ) );
				
			}
			
			// Advance the pointers and decrement the count
			trackDescriptorPtr++;
			numberOfDescriptors--;
			currentOffset++;
			
		}

		if ( numberOfDescriptors != 0 && rootDirNodePtr->entryCount == ( numTracks + kNumberOfFakeDirEntries ) )
		{
			
			// Oops...the parsing routine in userland must've screwed up
			DebugLog ( ( "ParseTOC: userland utility sent wrong number of audio tracks in at mount time.\n" ) );
			
		}
		
	}
	
	else
	{
	
		// Couldn't parse the TOC, so return an error
		return ENOMEM;
		
	}

	
Exit:
	
	if ( TOCDataPtr != NULL )
	{
		
		// Free the buffer allocated earlier
		DisposeBufferFromIORegistry ( TOCDataPtr );
	
	}
	
	DebugLog ( ( "ParseTOC: exiting...\n" ) );
	
	return error;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	BuildTrackName -	This routine is responsible for building a track
//						name based on its number
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
BuildTrackName ( struct mount * mountPtr, AppleCDDANodeInfoPtr nodeInfoPtr )
{
	
	UInt8		trackNumber	= 0;
	char *		name		= NULL;
	UInt8		nameSize	= 0;
	int			error		= 0;
	
	DebugLog ( ( "BuildTrackName: entering.\n" ) );
	
	DebugAssert ( ( nodeInfoPtr != NULL ) );
	
	// Get the track number for which to find the name
	trackNumber = nodeInfoPtr->trackDescriptor.point;
	
	// Find the name
	error = FindName ( mountPtr, trackNumber, &name, &nameSize );
	if ( error != 0 )
	{
		
		DebugLog ( ( "cddafs : FindName returned error = %d\n", error ) );
		// Buffer copied in was formatted incorrectly. We'll just use
		// track numbers on this CD.
		
	}
	
	// Set the size of the name
	nodeInfoPtr->nameSize = nameSize;
	
	// If we got here, then we have a valid track and the nodeInfoArrayPtr points to our
	// offset into the array. So, MALLOC the name here
	MALLOC ( nodeInfoPtr->name, char *, nodeInfoPtr->nameSize + 1, M_TEMP, M_WAITOK ); 
	
	// Copy the name
	bcopy ( name, &nodeInfoPtr->name[0], nameSize );
	
	// Don't forget NULL byte
	nodeInfoPtr->name[nameSize] = 0;
	
	DebugLog ( ( "BuildTrackName: fileName = %s\n", nodeInfoPtr->name ) );
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CalculateNumberOfDescriptors -	Calculate the number of SubQTOCInfo entries
//									in the given TOC
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
CalculateNumberOfDescriptors ( const QTOCDataFormat10Ptr TOCDataPtr )
{
	
	UInt32		numberOfDescriptors = 0;
	UInt32		length				= 0;
	
	DebugLog ( ( "CalculateNumberOfDescriptors: Entering...\n" ) );
	
	DebugAssert ( ( TOCDataPtr != NULL ) );

	// Get the length of the TOC
	length = TOCDataPtr->TOCDataLength;

	// Remove the first and last session numbers so all we are left with are track descriptors
	length -= ( sizeof ( TOCDataPtr->firstSessionNumber ) +
				sizeof ( TOCDataPtr->lastSessionNumber ) );
	
	// Divide the length by the size of a single track descriptor to get total number
	numberOfDescriptors = length / ( sizeof ( SubQTOCInfo ) );
		
	DebugLog ( ( "CalculateNumberOfDescriptors: exiting...\n" ) );
	
	return numberOfDescriptors;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CalculateLBA -	Convert the frames offset of the file (from the TOC) to
//					the LBA of the device
//
// NB: this is a workaround because the first 2 seconds of a CD are unreadable
// and defined as off-limits. So we convert our absolute MSF to an actual
// logical block which can be addressed through the BSD layer.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
CalculateLBA ( SubQTOCInfoPtr trackDescriptorPtr )
{
	
	DebugAssert ( ( trackDescriptorPtr != NULL ) );

	// Simply convert MSF to LBA
	return ( ( trackDescriptorPtr->PMSF.startPosition.minutes * kSecondsPerMinute + 
	  		   trackDescriptorPtr->PMSF.startPosition.seconds ) * kFramesPerSecond +
	  		   trackDescriptorPtr->PMSF.startPosition.frames - kMSFToLBA );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CalculateAttributeBlockSize - 	Calculates the size of the attribute
//									block
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int
CalculateAttributeBlockSize ( struct attrlist * attrlist )
{
	
	int				size;
	attrgroup_t		a;
	
	DebugAssert ( ( attrlist->commonattr & ~ATTR_CMN_VALIDMASK ) == 0 );
	DebugAssert ( ( attrlist->volattr & ~ATTR_VOL_VALIDMASK ) == 0 );
	DebugAssert ( ( attrlist->dirattr & ~ATTR_DIR_VALIDMASK ) == 0 );
	DebugAssert ( ( attrlist->fileattr & ~ATTR_FILE_VALIDMASK ) == 0 );
	DebugAssert ( ( attrlist->forkattr & ~ATTR_FORK_VALIDMASK ) == 0 );
	
	size = 0;
	
	if ( ( a = attrlist->commonattr ) != 0 )
	{
		
		DebugLog ( ( "Common attributes wanted\n" ) );
		
		if ( a & ATTR_CMN_NAME ) 			size += sizeof ( struct attrreference );
		if ( a & ATTR_CMN_DEVID ) 			size += sizeof ( dev_t );
		if ( a & ATTR_CMN_FSID ) 			size += sizeof ( fsid_t );
		if ( a & ATTR_CMN_OBJTYPE ) 		size += sizeof ( fsobj_type_t );
		if ( a & ATTR_CMN_OBJTAG ) 			size += sizeof ( fsobj_tag_t );
		if ( a & ATTR_CMN_OBJID ) 			size += sizeof ( fsobj_id_t );
		if ( a & ATTR_CMN_OBJPERMANENTID ) 	size += sizeof ( fsobj_id_t );
		if ( a & ATTR_CMN_PAROBJID ) 		size += sizeof( fsobj_id_t );
		if ( a & ATTR_CMN_SCRIPT ) 			size += sizeof ( text_encoding_t );
		if ( a & ATTR_CMN_CRTIME ) 			size += sizeof ( struct timespec );
		if ( a & ATTR_CMN_MODTIME ) 		size += sizeof ( struct timespec );
		if ( a & ATTR_CMN_CHGTIME ) 		size += sizeof ( struct timespec );
		if ( a & ATTR_CMN_ACCTIME ) 		size += sizeof ( struct timespec );
		if ( a & ATTR_CMN_BKUPTIME ) 		size += sizeof ( struct timespec );
		if ( a & ATTR_CMN_FNDRINFO ) 		size += 32 * sizeof ( UInt8 );
		if ( a & ATTR_CMN_OWNERID ) 		size += sizeof ( uid_t );
		if ( a & ATTR_CMN_GRPID ) 			size += sizeof ( gid_t );
		if ( a & ATTR_CMN_ACCESSMASK ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_CMN_NAMEDATTRCOUNT ) 	size += sizeof ( UInt32 );
		if ( a & ATTR_CMN_NAMEDATTRLIST ) 	size += sizeof ( struct attrreference );
		if ( a & ATTR_CMN_FLAGS ) 			size += sizeof ( UInt32 );
		if ( a & ATTR_CMN_USERACCESS ) 		size += sizeof ( UInt32 );
		
	}
	
	if ( ( a = attrlist ->volattr ) != 0 )
	{
		
		DebugLog ( ( "Volume attributes wanted\n" ) );
		
		if ( a & ATTR_VOL_FSTYPE )			size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_SIGNATURE ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_VCBFSID )			size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_SIZE ) 			size += sizeof ( off_t );
		if ( a & ATTR_VOL_SPACEFREE ) 		size += sizeof ( off_t );
		if ( a & ATTR_VOL_SPACEAVAIL ) 		size += sizeof ( off_t );
		if ( a & ATTR_VOL_MINALLOCATION ) 	size += sizeof ( off_t );
		if ( a & ATTR_VOL_ALLOCATIONCLUMP ) size += sizeof ( off_t );
		if ( a & ATTR_VOL_IOBLOCKSIZE ) 	size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_OBJCOUNT ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_FILECOUNT ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_DIRCOUNT ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_MAXOBJCOUNT ) 	size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_MOUNTPOINT ) 		size += sizeof ( struct attrreference );
		if ( a & ATTR_VOL_NAME ) 			size += sizeof ( struct attrreference );
		if ( a & ATTR_VOL_MOUNTFLAGS ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_VOL_MOUNTEDDEVICE ) 	size += sizeof ( struct attrreference );
		if ( a & ATTR_VOL_ENCODINGSUSED ) 	size += sizeof ( UInt64 );
		if ( a & ATTR_VOL_CAPABILITIES ) 	size += sizeof ( vol_capabilities_attr_t );
		if ( a & ATTR_VOL_ATTRIBUTES ) 		size += sizeof ( vol_attributes_attr_t );
		
	}
	
	if ( ( a = attrlist->dirattr ) != 0 )
	{
		
		DebugLog ( ( "Directory attributes wanted\n" ) );
		
		if ( a & ATTR_DIR_LINKCOUNT ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_DIR_ENTRYCOUNT ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_DIR_MOUNTSTATUS )		size += sizeof ( UInt32 );
		
	}
	
	if ( ( a = attrlist->fileattr ) != 0 )
	{
		
		DebugLog ( ( "File attributes wanted\n" ) );
		
		if ( a & ATTR_FILE_LINKCOUNT ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_FILE_TOTALSIZE ) 		size += sizeof ( off_t );
		if ( a & ATTR_FILE_ALLOCSIZE ) 		size += sizeof ( off_t );
		if ( a & ATTR_FILE_IOBLOCKSIZE ) 	size += sizeof ( size_t );
		if ( a & ATTR_FILE_CLUMPSIZE ) 		size += sizeof ( off_t );
		if ( a & ATTR_FILE_DEVTYPE ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_FILE_FILETYPE ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_FILE_FORKCOUNT ) 		size += sizeof ( UInt32 );
		if ( a & ATTR_FILE_FORKLIST ) 		size += sizeof ( struct attrreference );
		if ( a & ATTR_FILE_DATALENGTH ) 	size += sizeof ( off_t );
		if ( a & ATTR_FILE_DATAALLOCSIZE ) 	size += sizeof ( off_t );
		if ( a & ATTR_FILE_DATAEXTENTS ) 	size += sizeof ( extentrecord );
		if ( a & ATTR_FILE_RSRCLENGTH ) 	size += sizeof ( off_t );
		if ( a & ATTR_FILE_RSRCALLOCSIZE ) 	size += sizeof ( off_t );
		if ( a & ATTR_FILE_RSRCEXTENTS )	size += sizeof ( extentrecord );
		
	}
	
	return size;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	PackVolumeAttributes - 	Packs the volume attributes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
PackVolumeAttributes ( 	struct attrlist * attrListPtr,
						AppleCDDANodePtr cddaNodePtr,
						void ** attrbufHandle,
						void ** varbufHandle )
{
	
	void *				attrbufptr;
	void *				varbufptr;
	attrgroup_t 		a;
	UInt32 				attrlength;
	
	DebugLog ( ( "PackVolumeAttributes called\n" ) );
	
	attrbufptr 	= *attrbufHandle;
	varbufptr 	= *varbufHandle;
	
	if ( ( a = attrListPtr->commonattr ) != 0 )
	{
		
		if ( a & ATTR_CMN_NAME )
		{
			
			DebugLog ( ( "ATTR_CMN_NAME : %s\n", cddaNodePtr->u.directory.name ) );
			
			attrlength = strlen ( cddaNodePtr->u.directory.name ) + 1;
			( ( struct attrreference * ) attrbufptr )->attr_dataoffset = ( UInt8 * ) varbufptr - ( UInt8 * ) attrbufptr;
			( ( struct attrreference *) attrbufptr )->attr_length = attrlength;
			( void ) strncpy ( ( UInt8 * ) varbufptr, cddaNodePtr->u.directory.name, attrlength );

			/* Advance beyond the space just allocated and round up to the next 4-byte boundary: */
			( UInt8 * ) varbufptr += attrlength + ( ( 4 - ( attrlength & 3 ) ) & 3 );
			++( ( struct attrreference * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_DEVID )
		{
			
			DebugLog ( ( "ATTR_CMN_DEVID : %ld\n", *( dev_t * ) &( CDDATOV ( cddaNodePtr )->v_mount->mnt_stat.f_fsid ) ) );
			*( ( dev_t * ) attrbufptr )++ = *( dev_t * ) &( CDDATOV ( cddaNodePtr )->v_mount->mnt_stat.f_fsid );
			
		}
		
		if ( a & ATTR_CMN_FSID )
		{
			
			DebugLog ( ( "ATTR_CMN_FSID : %d\n", CDDATOV ( cddaNodePtr )->v_mount->mnt_stat.f_fsid ) );
			*( ( fsid_t * ) attrbufptr )++ = CDDATOV ( cddaNodePtr )->v_mount->mnt_stat.f_fsid;
			
		}
		
		if ( a & ATTR_CMN_OBJTYPE )
		{
			
			DebugLog ( ( "ATTR_CMN_OBJTYPE : %d\n", 0 ) );
			*( ( fsobj_type_t * ) attrbufptr )++ = 0;
			
		}
		
		if ( a & ATTR_CMN_OBJTAG )
		{
			
			DebugLog ( ( "ATTR_CMN_OBJTAG : %d\n", VT_CDDA ) );
			*( ( fsobj_tag_t * ) attrbufptr )++ = VT_CDDA;
			
		}
		
		if ( a & ATTR_CMN_OBJID )
		{
			
			DebugLog ( ( "ATTR_CMN_OBJID : fid_objno = %d, fid_generation = %d\n", 0, 0 ) );
			
			( ( fsobj_id_t * ) attrbufptr )->fid_objno = 0;
			( ( fsobj_id_t * ) attrbufptr )->fid_generation = 0;
			++( ( fsobj_id_t * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_OBJPERMANENTID )
		{
			
			DebugLog ( ( "ATTR_CMN_OBJPERMANENTID : fid_objno = %d, fid_generation = %d\n", 0, 0 ) );
			
			( ( fsobj_id_t * ) attrbufptr )->fid_objno = 0;
			( ( fsobj_id_t * ) attrbufptr )->fid_generation = 0;
			++( ( fsobj_id_t * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_PAROBJID )
		{
			
			DebugLog ( ( "ATTR_CMN_PAROBJID : fid_objno = %d, fid_generation = %d\n", 0, 0 ) );
			
			( ( fsobj_id_t * ) attrbufptr )->fid_objno = 0;
			( ( fsobj_id_t * ) attrbufptr )->fid_generation = 0;
			++( ( fsobj_id_t * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_SCRIPT )
		{
			
			DebugLog ( ( "ATTR_CMN_SCRIPT\n" ) );
			*( ( text_encoding_t * ) attrbufptr )++ = 0;
			
		}
		
		if ( a & ATTR_CMN_CRTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_CRTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = VFSTOCDDA ( cddaNodePtr->vNodePtr->v_mount )->mountTime;
			
		}
		
		if ( a & ATTR_CMN_MODTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_MODTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = VFSTOCDDA ( cddaNodePtr->vNodePtr->v_mount )->mountTime;
			
		}
		
		if ( a & ATTR_CMN_CHGTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_CHGTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = cddaNodePtr->lastModTime;
			
		}
		
		if ( a & ATTR_CMN_ACCTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_ACCTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = cddaNodePtr->accessTime;
			
		}
		
		if ( a & ATTR_CMN_BKUPTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_BKUPTIME\n" ) );
			( ( struct timespec * ) attrbufptr )->tv_sec = 0;
			( ( struct timespec * ) attrbufptr )->tv_nsec = 0;
			++( ( struct timespec * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_FNDRINFO )
		{
			
			DebugLog ( ( "ATTR_CMN_FNDRINFO\n" ) );
			bzero ( attrbufptr, 32 * sizeof ( UInt8 ) );
			( UInt8 * ) attrbufptr += 32 * sizeof ( UInt8 );
			
		}
		
		if ( a & ATTR_CMN_OWNERID )
		{
			
			DebugLog ( ( "ATTR_CMN_OWNERID\n" ) );
			*( ( uid_t * ) attrbufptr )++ = kUnknownUserID;
			
		}
		
		if ( a & ATTR_CMN_GRPID )
		{
			
			DebugLog ( ( "ATTR_CMN_GRPID\n" ) );
			*( ( gid_t * ) attrbufptr )++ = kUnknownGroupID;
			
		}
		
		if ( a & ATTR_CMN_ACCESSMASK )
		{
			
			UInt32	access = S_IRUSR | S_IRGRP | S_IROTH;
			
			if ( cddaNodePtr->vNodePtr->v_flag & VROOT )
				access |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
			
			DebugLog ( ( "ATTR_CMN_ACCESSMASK\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = access;
			
		}
		
		if ( a & ATTR_CMN_FLAGS )
		{
			
			DebugLog ( ( "ATTR_CMN_FLAGS\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = 0;
			
		}
		
		if ( a & ATTR_CMN_USERACCESS )
		{
			
			UInt32	permissions = R_OK;
			
			DebugLog ( ( "ATTR_CMN_USERACCESS\n" ) );
			
			if ( cddaNodePtr->vNodePtr->v_flag & VROOT )
				permissions |= X_OK;
			
			*( ( UInt32 * ) attrbufptr )++ = permissions;
			
		}
		
	}
	
	if ( ( a = attrListPtr->volattr ) != 0 )
	{
		
		off_t	blocksize = ( off_t ) kPhysicalMediaBlockSize;
		
		if ( a & ATTR_VOL_FSTYPE )
		{
			
			DebugLog ( ( "ATTR_VOL_FSTYPE : %d\n", cddaNodePtr->vNodePtr->v_mount->mnt_vfc->vfc_typenum ) );
			*( ( UInt32 * ) attrbufptr )++ = cddaNodePtr->vNodePtr->v_mount->mnt_vfc->vfc_typenum;
			
		}
		
		if ( a & ATTR_VOL_SIGNATURE )
		{
			
			DebugLog ( ( "ATTR_VOL_SIGNATURE : 0x%04x\n", kAppleCDDAFileSystemVolumeSignature ) );
			*( ( UInt32 * ) attrbufptr )++ = ( UInt32 ) kAppleCDDAFileSystemVolumeSignature;
			
		}
		
		if ( a & ATTR_VOL_SIZE )
		{
			
			DebugLog ( ( "ATTR_VOL_SIZE : %d\n", cddaNodePtr->u.directory.directorySize ) );
			*( ( off_t * ) attrbufptr )++ = cddaNodePtr->u.directory.directorySize;
			
		}
		
		if ( a & ATTR_VOL_SPACEFREE )
		{
			
			DebugLog ( ( "ATTR_VOL_SPACEFREE : %d\n", 0 ) );
			*( ( off_t * ) attrbufptr )++ = 0;
			
		}
		
		if ( a & ATTR_VOL_SPACEAVAIL )
		{
			
			DebugLog ( ( "ATTR_VOL_SPACEAVAIL : %d\n", 0 ) );
			*( ( off_t * ) attrbufptr )++ = 0;
			
		}
		
		if ( a & ATTR_VOL_MINALLOCATION )
		{
			
			DebugLog ( ( "ATTR_VOL_MINALLOCATION : %d\n", blocksize ) );
			*( ( off_t *) attrbufptr )++ = blocksize;
			
		}
		
		if ( a & ATTR_VOL_ALLOCATIONCLUMP )
		{
			
			DebugLog ( ( "ATTR_VOL_ALLOCATIONCLUMP : %d\n", blocksize ) );
			*( ( off_t *) attrbufptr )++ = blocksize;
			
		}
			
		if ( a & ATTR_VOL_IOBLOCKSIZE )
		{
			
			DebugLog ( ( "ATTR_VOL_IOBLOCKSIZE : %d\n", blocksize ) );
			*( ( off_t *) attrbufptr )++ = blocksize;
			
		}
			
		if ( a & ATTR_VOL_OBJCOUNT )
		{
			
			DebugLog ( ( "ATTR_VOL_OBJCOUNT : %d\n", 0 ) );
			*( ( UInt32 * ) attrbufptr )++ = 0;
			
		}
		
		if ( a & ATTR_VOL_FILECOUNT )
		{
			
			DebugLog ( ( "ATTR_VOL_FILECOUNT : %d\n", 0 ) );
			*( ( UInt32 * ) attrbufptr )++ = cddaNodePtr->u.directory.entryCount - kNumberOfFakeDirEntries + 1;
			
		}
		
		if ( a & ATTR_VOL_DIRCOUNT )
		{
			
			DebugLog ( ( "ATTR_VOL_DIRCOUNT : %d\n", 0 ) );
			*( ( UInt32 * ) attrbufptr )++ = 1;
			
		}
		
		if ( a & ATTR_VOL_MAXOBJCOUNT )
		{
			
			DebugLog ( ( "ATTR_VOL_MAXOBJCOUNT : %d\n", 0xFFFFFFFF ) );
			*( ( UInt32 * ) attrbufptr )++ = 0xFFFFFFFF;
			
		}
		
		if ( a & ATTR_VOL_MOUNTPOINT )
		{
			
			DebugLog ( ( "ATTR_VOL_MOUNTPOINT : %s\n", cddaNodePtr->vNodePtr->v_mount->mnt_stat.f_mntonname ) );
			( ( struct attrreference * ) attrbufptr )->attr_dataoffset = ( UInt8 * ) varbufptr - ( UInt8 * ) attrbufptr;
			( ( struct attrreference * ) attrbufptr )->attr_length = strlen ( cddaNodePtr->vNodePtr->v_mount->mnt_stat.f_mntonname ) + 1;
			attrlength = ( ( struct attrreference * ) attrbufptr )->attr_length;
			
			// round up to the next 4-byte boundary:
			attrlength = attrlength + ( ( 4 - ( attrlength & 3 ) ) & 3 );
			( void ) bcopy ( cddaNodePtr->vNodePtr->v_mount->mnt_stat.f_mntonname, varbufptr, attrlength );
			
			// Advance beyond the space just allocated:
			( UInt8 * ) varbufptr += attrlength;
			++( ( struct attrreference * ) attrbufptr );
			
		}
		
		if ( a & ATTR_VOL_NAME )
		{
			
			DebugLog ( ( "ATTR_VOL_NAME : %s\n", cddaNodePtr->u.directory.name ) );
			attrlength = strlen ( cddaNodePtr->u.directory.name ) + 1;
			( ( struct attrreference * ) attrbufptr )->attr_dataoffset = ( UInt8 * ) varbufptr - ( UInt8 * ) attrbufptr;
			( ( struct attrreference * ) attrbufptr )->attr_length = attrlength;
			( void ) strncpy ( ( UInt8 * ) varbufptr, cddaNodePtr->u.directory.name, attrlength );
			
			// Advance beyond the space just allocated and round up to the next 4-byte boundary:
			( UInt8 * ) varbufptr += attrlength + ( ( 4 - ( attrlength & 3 ) ) & 3 );
			++( ( struct attrreference * ) attrbufptr );
			
		}
		
		if ( a & ATTR_VOL_MOUNTFLAGS )
		{
			
			DebugLog ( ( "ATTR_VOL_MOUNTFLAGS : %d\n", cddaNodePtr->vNodePtr->v_mount->mnt_flag ) );
			*( ( UInt32 * ) attrbufptr )++ = ( UInt32 ) cddaNodePtr->vNodePtr->v_mount->mnt_flag;
			
		}
		
		if ( a & ATTR_VOL_MOUNTEDDEVICE )
		{
			
			DebugLog ( ( "ATTR_VOL_MOUNTEDDEVICE : %s\n", cddaNodePtr->vNodePtr->v_mount->mnt_stat.f_mntfromname ) );
			
			( ( struct attrreference * ) attrbufptr )->attr_dataoffset = ( UInt8 * ) varbufptr - ( UInt8 * ) attrbufptr;
			( ( struct attrreference * ) attrbufptr )->attr_length = strlen ( cddaNodePtr->vNodePtr->v_mount->mnt_stat.f_mntfromname ) + 1;
			attrlength = ( ( struct attrreference * ) attrbufptr )->attr_length;
			
			// round up to the next 4-byte boundary:
			attrlength = attrlength + ( ( 4 - ( attrlength & 3 ) ) & 3 );
			( void ) bcopy ( cddaNodePtr->vNodePtr->v_mount->mnt_stat.f_mntfromname, varbufptr, attrlength );
			
			// Advance beyond the space just allocated:
			( UInt8 * ) varbufptr += attrlength;
			++( ( struct attrreference * ) attrbufptr );
			
		}
		
		if ( a & ATTR_VOL_ENCODINGSUSED )
		{
			
			DebugLog ( ( "ATTR_VOL_ENCODINGSUSED : %d\n", 0 ) );
			*( ( UInt64 * ) attrbufptr )++ = ( UInt64 ) 0;
			
		}
		
		if ( a & ATTR_VOL_CAPABILITIES )
		{
			
			DebugLog ( ( "ATTR_VOL_CAPABILITIES\n" ) );
			
			vol_capabilities_attr_t *	capabilities = ( vol_capabilities_attr_t * ) attrbufptr;
			
			// We understand the following.
			capabilities->valid[VOL_CAPABILITIES_FORMAT] 			= VOL_CAP_FMT_PERSISTENTOBJECTIDS |
																	  VOL_CAP_FMT_SYMBOLICLINKS |
																	  VOL_CAP_FMT_HARDLINKS |
																	  VOL_CAP_FMT_JOURNAL |
																	  VOL_CAP_FMT_JOURNAL_ACTIVE |
																	  VOL_CAP_FMT_NO_ROOT_TIMES |
																	  VOL_CAP_FMT_SPARSE_FILES |
																	  VOL_CAP_FMT_ZERO_RUNS |
																	  VOL_CAP_FMT_CASE_SENSITIVE |
																	  VOL_CAP_FMT_CASE_PRESERVING |
																	  VOL_CAP_FMT_FAST_STATFS;

			// We understand the following interfaces.
			capabilities->valid[VOL_CAPABILITIES_INTERFACES] 		= VOL_CAP_INT_SEARCHFS |
																	  VOL_CAP_INT_ATTRLIST |
																	  VOL_CAP_INT_NFSEXPORT |
																	  VOL_CAP_INT_READDIRATTR |
																	  VOL_CAP_INT_EXCHANGEDATA |
																	  VOL_CAP_INT_COPYFILE |
																	  VOL_CAP_INT_ALLOCATE |
																	  VOL_CAP_INT_VOL_RENAME |
																	  VOL_CAP_INT_ADVLOCK |
																	  VOL_CAP_INT_FLOCK;

			// We only support these bits of the above recognized things.
			capabilities->capabilities[VOL_CAPABILITIES_FORMAT] 	= VOL_CAP_FMT_PERSISTENTOBJECTIDS |
																	  VOL_CAP_FMT_FAST_STATFS |
																	  VOL_CAP_FMT_NO_ROOT_TIMES;
			
			// We only support this one thing of the above recognized ones.
			capabilities->capabilities[VOL_CAPABILITIES_INTERFACES] = VOL_CAP_INT_ATTRLIST;
			
			// Reserved. Zero them.
			capabilities->capabilities[VOL_CAPABILITIES_RESERVED1] 	= 0;
			capabilities->capabilities[VOL_CAPABILITIES_RESERVED2] 	= 0;
			capabilities->valid[VOL_CAPABILITIES_RESERVED1] 		= 0;
			capabilities->valid[VOL_CAPABILITIES_RESERVED2] 		= 0;
			
			++( ( vol_capabilities_attr_t * ) attrbufptr );
			
		}
		
		if ( a & ATTR_VOL_VCBFSID )
		{
			
			DebugLog ( ( "ATTR_VOL_VCBFSID : 0x%04x\n", kAppleCDDAFileSystemVCBFSID ) );
			*( ( UInt32 * ) attrbufptr )++ = ( UInt32 ) kAppleCDDAFileSystemVCBFSID;
			
		}
		
		if ( a & ATTR_VOL_ATTRIBUTES )
		{
			
			DebugLog ( ( "ATTR_VOL_ATTRIBUTES\n" ) );
			
			( ( vol_attributes_attr_t * ) attrbufptr )->validattr.commonattr 	= kAppleCDDACommonAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->validattr.volattr 		= kAppleCDDAVolumeAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->validattr.dirattr 		= kAppleCDDADirectoryAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->validattr.fileattr 		= kAppleCDDAFileAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->validattr.forkattr 		= kAppleCDDAForkAttributesValidMask;
			
			( ( vol_attributes_attr_t * ) attrbufptr )->nativeattr.commonattr 	= kAppleCDDACommonAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->nativeattr.volattr 		= kAppleCDDAVolumeAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->nativeattr.dirattr 		= kAppleCDDADirectoryAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->nativeattr.fileattr 	= kAppleCDDAFileAttributesValidMask;
			( ( vol_attributes_attr_t * ) attrbufptr )->nativeattr.forkattr 	= kAppleCDDAForkAttributesValidMask;
			
			++( ( vol_attributes_attr_t * ) attrbufptr );
			
		}
		
	}
	
	*attrbufHandle 	= attrbufptr;
	*varbufHandle 	= varbufptr;
	
	DebugLog ( ( "PackVolumeAttributes exiting\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	PackCommonAttributes - 	Packs the common attributes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
PackCommonAttributes (  struct attrlist * attrListPtr,
						AppleCDDANodePtr cddaNodePtr,
						void ** attrbufHandle,
						void ** varbufHandle )
{
	
	void *			attrbufptr;
	void *			varbufptr;
	attrgroup_t 	a;
	UInt32 			attrlength;
	
	DebugLog ( ( "PackCommonAttributes called\n" ) );
	
	attrbufptr 	= *attrbufHandle;
	varbufptr 	= *varbufHandle;
	
	if ( ( a = attrListPtr->commonattr ) != 0 )
	{
		
		if ( a & ATTR_CMN_NAME )
		{
			
			// special case root since we know how to get it's name
			if ( cddaNodePtr->nodeType == kAppleCDDADirectoryType )
			{
				
				DebugLog ( ( "ATTR_CMN_NAME : %s\n", cddaNodePtr->u.directory.name ) );
				
				attrlength = strlen ( cddaNodePtr->u.directory.name ) + 1;
				( void ) strncpy ( ( UInt8 * ) varbufptr, cddaNodePtr->u.directory.name, attrlength );
				
			}
			
			// Special case the XMLFileNode
			else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
			{
				
				DebugLog ( ( "ATTR_CMN_NAME : %s\n", ".TOC.plist" ) );
				
				attrlength = strlen ( ".TOC.plist" ) + 1;
				( void ) strncpy ( ( UInt8 * ) varbufptr, ".TOC.plist", attrlength );
				
			}
			
			else
			{
				
				DebugLog ( ( "ATTR_CMN_NAME : %s\n", cddaNodePtr->u.file.nodeInfoPtr->name ) );
				
				attrlength = cddaNodePtr->u.file.nodeInfoPtr->nameSize + 1;
				( void ) strncpy ( ( UInt8 * ) varbufptr, cddaNodePtr->u.file.nodeInfoPtr->name, attrlength );
				
			}
			
			( ( struct attrreference * ) attrbufptr )->attr_dataoffset = ( UInt8 * ) varbufptr - ( UInt8 * ) attrbufptr;
			( ( struct attrreference * ) attrbufptr )->attr_length = attrlength;
			
			// Advance beyond the space just allocated and round up to the next 4-byte boundary:
			( UInt8 * ) varbufptr += attrlength + ( ( 4 - ( attrlength & 3 ) ) & 3 );
			++( ( struct attrreference * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_DEVID )
		{
			
			DebugLog ( ( "ATTR_CMN_DEVID : %ld\n", *( dev_t * ) &( CDDATOV ( cddaNodePtr )->v_mount->mnt_stat.f_fsid ) ) );
			*( ( dev_t * ) attrbufptr )++ = *( dev_t * ) &( CDDATOV ( cddaNodePtr )->v_mount->mnt_stat.f_fsid );
			//*( ( dev_t * ) attrbufptr )++ = cddaNodePtr->blockDeviceVNodePtr->v_specinfo->si_rdev;
			
		}
		
		if ( a & ATTR_CMN_FSID )
		{
			
			DebugLog ( ( "ATTR_CMN_FSID\n" ) );
			*( ( fsid_t * ) attrbufptr )++ = CDDATOV ( cddaNodePtr )->v_mount->mnt_stat.f_fsid;
			
		}
		
		if ( a & ATTR_CMN_OBJTYPE )
		{
			
			DebugLog ( ( "ATTR_CMN_OBJTYPE\n" ) );
			*( ( fsobj_type_t * ) attrbufptr )++ = CDDATOV ( cddaNodePtr )->v_type;
			
		}
		
		if ( a & ATTR_CMN_OBJTAG )
		{
			
			DebugLog ( ( "ATTR_CMN_OBJTAG\n" ) );
			*( ( fsobj_tag_t * ) attrbufptr )++ = CDDATOV ( cddaNodePtr )->v_tag;
			
		}
		
		if ( a & ATTR_CMN_OBJID )
		{
						
			// special case root since we know how to get it's name
			if ( cddaNodePtr->nodeType == kAppleCDDADirectoryType )
			{
				
				DebugLog ( ( "ATTR_CMN_OBJID: kAppleCDDARootFileID\n" ) );
				
				// force root to be 2
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = kAppleCDDARootFileID;	
				
			}
			
			// Special case the XMLFileNode
			else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
			{
				
				DebugLog ( ( "ATTR_CMN_OBJID: kAppleCDDAXMLFileID\n" ) );
				
				// force the XMLFileNode to be 3
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = kAppleCDDAXMLFileID;
				
			}
			
			else
			{
				
				DebugLog ( ( "ATTR_CMN_OBJID: nodeID = %ld\n", cddaNodePtr->nodeID ) );
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = cddaNodePtr->nodeID;				
				
			}
			
			( ( fsobj_id_t * ) attrbufptr )->fid_generation = 0;
			++( ( fsobj_id_t * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_OBJPERMANENTID )
		{
			
			// special case root
			if ( cddaNodePtr->nodeType == kAppleCDDADirectoryType )
			{
				
				DebugLog ( ( "ATTR_CMN_OBJPERMANENTID: kAppleCDDARootFileID\n" ) );
				
				// force root to be 2
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = kAppleCDDARootFileID;	
				
			}
			
			// Special case the XMLFileNode
			else if ( cddaNodePtr->nodeType == kAppleCDDAXMLFileType )
			{
				
				DebugLog ( ( "ATTR_CMN_OBJPERMANENTID: kAppleCDDAXMLFileID\n" ) );
				
				// force the XMLFileNode to be 3
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = kAppleCDDAXMLFileID;
				
			}
			
			else
			{
				
				DebugLog ( ( "ATTR_CMN_OBJPERMANENTID: nodeID = %ld\n", cddaNodePtr->nodeID ) );
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = cddaNodePtr->nodeID;
				
			}
			
			( ( fsobj_id_t * ) attrbufptr )->fid_generation = 0;
			++( ( fsobj_id_t * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_PAROBJID )
		{
			
			// What node are they asking for? 
			if ( cddaNodePtr->nodeID == kAppleCDDARootFileID )
			{
				
				DebugLog ( ( "ATTR_CMN_PAROBJID: 1\n" ) );
				
				// If this is the root directory and they want the parent, force it to 1
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = 1;
				
			}
			
			else
			{
				
				DebugLog ( ( "ATTR_CMN_PAROBJID: kAppleCDDARootFileID\n" ) );
				
				// Every other object has the root as its parent (flat filesystem)
				( ( fsobj_id_t * ) attrbufptr )->fid_objno = kAppleCDDARootFileID;
				
			}
			
			( ( fsobj_id_t * ) attrbufptr )->fid_generation = 0;
			++( ( fsobj_id_t * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_SCRIPT )
		{
			
			DebugLog ( ( "ATTR_CMN_SCRIPT\n" ) );
			*( ( text_encoding_t * ) attrbufptr )++ = 0;
		
		}
		
		if ( a & ATTR_CMN_CRTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_CRTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = VFSTOCDDA ( cddaNodePtr->vNodePtr->v_mount )->mountTime;
		
		}
		
		if ( a & ATTR_CMN_MODTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_MODTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = VFSTOCDDA ( cddaNodePtr->vNodePtr->v_mount )->mountTime;
		
		}
		
		if ( a & ATTR_CMN_CHGTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_CHGTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = cddaNodePtr->lastModTime;
			
		}
			
		if ( a & ATTR_CMN_ACCTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_ACCTIME\n" ) );
			*( ( struct timespec * ) attrbufptr )++ = cddaNodePtr->accessTime;
			
		}
		
		if ( a & ATTR_CMN_BKUPTIME )
		{
			
			DebugLog ( ( "ATTR_CMN_BKUPTIME\n" ) );
			( ( struct timespec * ) attrbufptr )->tv_sec = 0;
			( ( struct timespec * ) attrbufptr )->tv_nsec = 0;
			++( ( struct timespec * ) attrbufptr );
			
		}
		
		if ( a & ATTR_CMN_FNDRINFO )
		{
			
			FinderInfo		finderInfo = { 0 };
			
			DebugLog ( ( "ATTR_CMN_FNDRINFO\n" ) );
			
			if ( cddaNodePtr->nodeID == kAppleCDDAXMLFileID )
			{
				
				DebugLog ( ( "kFinderInfoInvisibleMask\n" ) );
				// Make the XML file invisible
				finderInfo.finderFlags = kFinderInfoInvisibleMask;
				
			}
			
			else
			{
				finderInfo.finderFlags = kFinderInfoNoFileExtensionMask;
			}
			
			finderInfo.location.v 	= -1;
			finderInfo.location.h 	= -1;
			
			if ( ( CDDATOV ( cddaNodePtr )->v_type == VREG ) &&
				 ( cddaNodePtr->nodeID != kAppleCDDAXMLFileID ) )
			{
				
				DebugLog ( ( "fileType, creator\n" ) );
				finderInfo.fileType 	= VFSTOCDDA ( cddaNodePtr->vNodePtr->v_mount )->fileType;
				finderInfo.fileCreator 	= VFSTOCDDA ( cddaNodePtr->vNodePtr->v_mount )->fileCreator;
				
			}
			
			bcopy ( &finderInfo, attrbufptr, sizeof ( finderInfo ) );
			( UInt8 * ) attrbufptr += sizeof ( finderInfo );
			
			bzero ( attrbufptr, kExtendedFinderInfoSize );
			( UInt8 * ) attrbufptr += kExtendedFinderInfoSize;
			
		}
		
		if ( a & ATTR_CMN_OWNERID )
		{
			
			DebugLog ( ( "ATTR_CMN_OWNERID\n" ) );
			*( ( uid_t * ) attrbufptr )++ = kUnknownUserID;
			
		}
		
		if ( a & ATTR_CMN_GRPID )
		{
			
			DebugLog ( ( "ATTR_CMN_GRPID\n" ) );
			*( ( gid_t * ) attrbufptr )++ = kUnknownGroupID;
			
		}
		
		if ( a & ATTR_CMN_ACCESSMASK )
		{
			
			UInt32	access = S_IRUSR | S_IRGRP | S_IROTH;
			
			if ( cddaNodePtr->vNodePtr->v_flag & VROOT )
				access |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
			
			DebugLog ( ( "ATTR_CMN_ACCESSMASK\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = access;
			
		}
		
		if ( a & ATTR_CMN_FLAGS )
		{
			
			DebugLog ( ( "ATTR_CMN_FLAGS\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = 0;		// ???
			
		}
		
		if ( a & ATTR_CMN_USERACCESS )
		{
			
			UInt32	permissions = R_OK;
			
			DebugLog ( ( "ATTR_CMN_USERACCESS\n" ) );
			
			if ( cddaNodePtr->vNodePtr->v_flag & VROOT )
				permissions |= X_OK;
			
			*( ( UInt32 * ) attrbufptr )++ = permissions;
			
		}
		
	}
	
	*attrbufHandle 	= attrbufptr;
	*varbufHandle 	= varbufptr;
	
	DebugLog ( ( "PackCommonAttributes exiting\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	PackDirectoryAttributes - 	Packs the directory attributes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
PackDirectoryAttributes ( struct attrlist * attrListPtr,
						  AppleCDDANodePtr cddaNodePtr,
						  void ** attrbufHandle,
						  void ** varbufHandle )
{
	
	void *			attrbufptr;
	attrgroup_t 	a;
	
	DebugLog ( ( "PackDirectoryAttributes called\n" ) );
	
	attrbufptr 	= *attrbufHandle;
	
	a = attrListPtr->dirattr;
	if ( ( cddaNodePtr->vNodePtr->v_type == VDIR ) && ( a != 0 ) )
	{
		
		if ( a & ATTR_DIR_LINKCOUNT )
		{
			
			DebugLog ( ( "ATTR_DIR_LINKCOUNT\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = 0; // ???
			
		}
		
		if ( a & ATTR_DIR_ENTRYCOUNT )
		{
			
			DebugLog ( ( "ATTR_DIR_ENTRYCOUNT: %ld\n", cddaNodePtr->u.directory.entryCount - 2 ) );
			
			// exclude '.' and '..' from total count
			*( ( UInt32 * ) attrbufptr )++ = cddaNodePtr->u.directory.entryCount - 2;
			
		}
		
		if ( a & ATTR_DIR_MOUNTSTATUS )
		{
			
			DebugLog ( ( "ATTR_DIR_MOUNTSTATUS: %d\n", CDDATOV ( cddaNodePtr )->v_mountedhere ) );
			
			if ( CDDATOV ( cddaNodePtr )->v_mountedhere )
			{
				*( ( UInt32 * ) attrbufptr )++ = DIR_MNTSTATUS_MNTPOINT;
			}
			
			else
			{
				*( ( UInt32 * ) attrbufptr )++ = 0;
			}
			
		}
		
	}
	
	*attrbufHandle = attrbufptr;
	
	DebugLog ( ( "PackDirectoryAttributes exiting\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	PackFileAttributes - Packs the file attributes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
PackFileAttributes ( struct attrlist * 	attrListPtr,
					 AppleCDDANodePtr cddaNodePtr,
					 void ** attrbufHandle,
					 void ** varbufHandle )
{
	
	void *			attrbufptr 	= *attrbufHandle;
	void *			varbufptr 	= *varbufHandle;
	attrgroup_t 	a 			= attrListPtr->fileattr;
	
	DebugLog ( ( "PackFileAttributes called\n" ) );
	
	if ( ( CDDATOV ( cddaNodePtr )->v_type == VREG ) && ( a != 0 ) )
	{
		
		if ( a & ATTR_FILE_LINKCOUNT )
		{
			
			DebugLog ( ( "ATTR_FILE_LINKCOUNT\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = 0; 			// ???
			
		}
		
		if ( a & ATTR_FILE_TOTALSIZE )
		{
			
			DebugLog ( ( "ATTR_FILE_TOTALSIZE\n" ) );
			if ( cddaNodePtr->nodeID == kAppleCDDAXMLFileID )
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.xmlFile.fileSize;
			else
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.file.nodeInfoPtr->numBytes;
			
		}
		
		if ( a & ATTR_FILE_ALLOCSIZE )
		{
			
			DebugLog ( ( "ATTR_FILE_ALLOCSIZE\n" ) );
			if ( cddaNodePtr->nodeID == kAppleCDDAXMLFileID )
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.xmlFile.fileSize;
			else
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.file.nodeInfoPtr->numBytes;
			
		}
		
		if ( a & ATTR_FILE_IOBLOCKSIZE )
		{
			
			DebugLog ( ( "ATTR_FILE_IOBLOCKSIZE\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = kPhysicalMediaBlockSize;
			
		}
		
		if ( a & ATTR_FILE_CLUMPSIZE )
		{
			
			DebugLog ( ( "ATTR_FILE_CLUMPSIZE\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = kPhysicalMediaBlockSize;
			
		}
		
		if ( a & ATTR_FILE_DEVTYPE )
		{
			
			DebugLog ( ( "ATTR_FILE_DEVTYPE\n" ) );
			*( ( UInt32 * ) attrbufptr )++ = ( UInt32 ) 0; 	// ???
			
		}
		
		if ( a & ATTR_FILE_DATALENGTH )
		{
			
			DebugLog ( ( "ATTR_FILE_DATALENGTH\n" ) );
			
			if ( cddaNodePtr->nodeID == kAppleCDDAXMLFileID )
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.xmlFile.fileSize;
			else
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.file.nodeInfoPtr->numBytes;
			
		}	
		
		if ( a & ATTR_FILE_DATAALLOCSIZE )
		{
			
			DebugLog ( ( "ATTR_FILE_DATAALLOCSIZE\n" ) );
			if ( cddaNodePtr->nodeID == kAppleCDDAXMLFileID )
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.xmlFile.fileSize;
			else
				*( ( off_t * ) attrbufptr )++ = ( off_t ) cddaNodePtr->u.file.nodeInfoPtr->numBytes;
			
		}
		
		if ( a & ATTR_FILE_RSRCLENGTH )
		{
			
			DebugLog ( ( "ATTR_FILE_RSRCLENGTH\n" ) );
			*( ( off_t * ) attrbufptr )++ = ( off_t ) 0;
			
		}	
		
		if ( a & ATTR_FILE_RSRCALLOCSIZE )
		{
			
			DebugLog ( ( "ATTR_FILE_RSRCALLOCSIZE\n" ) );
			*( ( off_t * ) attrbufptr )++ = ( off_t ) 0;
			DebugLog ( ( "ATTR_FILE_RSRCALLOCSIZE done\n" ) );
			
		}
		
	}
	
	*attrbufHandle 	= attrbufptr;
	*varbufHandle 	= varbufptr;
	
	DebugLog ( ( "PackFileAttributes exiting\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	PackAttributesBlock - Packs the attributes block
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
PackAttributesBlock ( struct attrlist * attrListPtr,
					  struct vnode * vNodePtr,
					  void ** attrbufHandle,
					  void ** varbufHandle )
{
	
	AppleCDDANodePtr	cddaNodePtr = VTOCDDA ( vNodePtr );
	
	DebugLog ( ( "PackAttributesBlock called\n" ) );
	
	if ( attrListPtr->volattr != 0 )
	{
		
		DebugLog ( ( "PackAttributesBlock: volume attributes requested.\n" ) );
		PackVolumeAttributes ( attrListPtr, cddaNodePtr, attrbufHandle, varbufHandle );
		
	}
	
	else
	{
		
		DebugLog ( ( "PackAttributesBlock: NO volume attributes requested.\n" ) );
		PackCommonAttributes ( attrListPtr, cddaNodePtr, attrbufHandle, varbufHandle );
		
		switch ( vNodePtr->v_type )
		{
			
			case VDIR:
				DebugLog ( ( "PackAttributesBlock: directory attributes requested.\n" ) );
				PackDirectoryAttributes ( attrListPtr, cddaNodePtr, attrbufHandle, varbufHandle );
				break;
				
			case VREG:
				DebugLog ( ( "PackAttributesBlock: file attributes requested.\n" ) );
				PackFileAttributes ( attrListPtr, cddaNodePtr, attrbufHandle, varbufHandle );
				break;
				
			default:
				break;
			
		}
		
	}
	
	DebugLog ( ( "PackAttributesBlock exiting\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//				End				Of			File
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ