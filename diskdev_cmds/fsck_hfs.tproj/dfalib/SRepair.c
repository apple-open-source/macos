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
/*
	File:		SRepair.c

	Contains:	This file contains the Scavenger repair routines.
	
	Written by:	Bill Bruffey

	Copyright:	й 1986, 1990, 1992-1999 by Apple Computer, Inc., all rights reserved.

*/

#include "Scavenger.h"

enum {
	clearBlocks,
	addBitmapBit,
	deleteExtents
};

/* internal routine prototypes */

void	SetOffset (void *buffer, UInt16 btNodeSize, SInt16 recOffset, SInt16 vecOffset);
#define SetOffset(buffer,nodesize,offset,record)		(*(SInt16 *) ((Byte *) (buffer) + (nodesize) + (-2 * (record))) = (offset))
static	OSErr	UpdateBTreeHeader( SFCB * fcbPtr );
static	OSErr	FixBTreeHeaderReservedFields( SGlobPtr GPtr, short refNum );
static	OSErr	UpdBTM( SGlobPtr GPtr, short refNum);
static	OSErr	UpdateVolumeBitMap( SGlobPtr GPtr, Boolean preAllocateOverlappedExtents );
static	OSErr	DoMinorOrders( SGlobPtr GPtr );
static	OSErr	UpdVal( SGlobPtr GPtr, RepairOrderPtr rP );
static	int		DelFThd( SGlobPtr GPtr, UInt32 fid );
static	OSErr	FixDirThread( SGlobPtr GPtr, UInt32 did );
static	OSErr	FixOrphanedFiles ( SGlobPtr GPtr );
static	OSErr	RepairReservedBTreeFields ( SGlobPtr GPtr );
static	OSErr	RebuildBTree( SGlobPtr GPtr, SInt16 refNum );
static	OSErr	FixFinderFlags( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixLinkCount( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixBSDInfo( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	DeleteUnlinkedFile( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixOrphanedExtent( SGlobPtr GPtr );
static OSErr	FixFileSize(SGlobPtr GPtr, RepairOrderPtr p);

static	OSErr	CreateAndOpenRepairBtree( SGlobPtr GPtr, SInt16 refNum, SInt16 *newBTreeRefNum );
static	OSErr	CreateMapNodes( UInt32 firstMapNode, UInt32	numberOfNewMapNodes, UInt16 nodeSize );
extern	OSErr	FindExtentRecord( const SVCB *vcb, UInt8 forkType, UInt32 fileID, UInt32 startBlock, Boolean allowPrevious, HFSPlusExtentKey *foundKey, HFSPlusExtentRecord foundData, UInt32 *foundHint);
extern	OSErr	DeleteExtentRecord( const SVCB *vcb, UInt8 forkType, UInt32 fileID, UInt32 startBlock );
OSErr	GetFCBExtentRecord( const SVCB *vcb, const SFCB *fcb, HFSPlusExtentRecord extents );
static	OSErr	DeleteFilesOverflowExtents( SGlobPtr GPtr, SFCB *fcb  );
static	OSErr	MoveExtent( SGlobPtr GPtr, ExtentInfo *extentInfo );					//	Isolate and fix Overlapping Extents
static	Boolean	ReplaceStartBlock( SGlobPtr GPtr, ExtentRecord *extentRecord, UInt32 originalStartBlock, UInt32 newStartBlock );
static	OSErr	ScanForCatalogRecord( SGlobPtr GPtr, HFSCatalogNodeID fileID, SInt16 recordType, CatalogKey **foundCatalogKeyH, CatalogRecord **foundCatalogRecordH, UInt16 *recordSize );
static	OSErr	ScanForExtentRecord( SGlobPtr GPtr, UInt32 startBlock, HFSCatalogNodeID fileID, UInt8 forkType, ExtentKey **foundExtentKeyH, ExtentRecord **foundExtentRecordH, UInt16 *recordSize );
static	OSErr	FixOverlappingExtents( SGlobPtr GPtr );
static	void	InsertIdentifier( SGlobPtr GPtr, FileIdentifier *fileIdentifier );
static	Boolean	IdentifierExists( FileIdentifierTable **fileIdentifierTable, HFSCatalogNodeID fileID );
static	OSErr	CreateFileIdentifiers( SGlobPtr GPtr );
OSErr	CopyDiskBlocks( SGlobPtr GPtr, UInt32 startAllocationBlock, UInt32 blockCount, UInt32 newStartAllocationBlock );
static	OSErr	FixBloatedThreadRecords( SGlob *GPtr );
static	OSErr	FixMissingThreadRecords( SGlob *GPtr );
static	OSErr	FixEmbededVolDescription( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixWrapperExtents( SGlobPtr GPtr, RepairOrderPtr p );


OSErr	RepairVolume( SGlobPtr GPtr )
{
	OSErr			err;
	OSErr			unmountResult;
	Boolean			volumeMounted;
	
	SetDFAStage( kAboutToRepairStage );											//	Notify callers repair is starting...
 	err = CheckForStop( GPtr ); ReturnIfError( err );							//	Permit the user to interrupt

	volumeMounted = ( GPtr->volumeFeatures & volumeIsMountedMask );
	unmountResult = fBsyErr;

	if ( volumeMounted )														//	If the volume is mounted
	{
	//	if ( GPtr->realVCB == nil )
	//		M_DebugStr("\p realVCB should not be nil here");
			
//		unmountResult = UnmountVol( nil, GPtr->realVCB->vcbVRefNum );
		unmountResult = fBsyErr;
		err = GetVolumeFeatures( GPtr );										//	Sets up GPtr->volumeFeatures and GPtr->realVCB
		
		//	If the volume cannot be unmounted, it may be the boot volume
		if ( unmountResult != noErr )
		{
		   #if 0
			if ( GPtr->volumeFeatures & supportsTrashVolumeCacheFeatureMask )
			{
				ParamBlockRec	pb;
				ClearMemory( &pb, sizeof(ParamBlockRec) );
				pb.volumeParam.ioVRefNum = GPtr->realVCB->vcbVRefNum;
				PBHTrashVolumeCachesSync( &pb );								//	On 8.2 and later all FS caches are flushed
			}
		   #endif

			SetDFAStage( kRepairStage );										//	Stops GNE from being called, and changes behavior of MountCheck
		
			/* this sould be done outside of the engine */
		   #if 0
			err = FlushVol( NULL, GPtr->realVCB->vcbVRefNum );					//	Flush before invalidating the disk cache <HFS31> & before we mark file system busy <HFS37>

			MarkFileSystemBusy();
			
			if ( (GPtr->volumeFeatures & supportsTrashVolumeCacheFeatureMask) == 0 )
			{				
				//	Check if System 8.1 is running, we then use staticly linked cache flushing routine
				if ( (GPtr->volumeFeatures & supportsHFSPlusVolsFeatureMask) != 0 )
				{
					TrashAllFSCaches( GPtr->realVCB );
				}
				else	//	Pre HFS+ savy system software
				{
					TrashVolumeDiskCache( GPtr->realVCB );
				}
			}
		   #endif
		}
	}
	
	//
	//	Do the repair
	//
	SetDFAStage( kRepairStage );									//	Stops GNE from being called, and changes behavior of MountCheck

	err = MRepair( GPtr );
	
#if 0	
	if ( volumeMounted )											//	If the volume was mounted before the repair
	{
		if ( unmountResult != noErr )								//	If we were not able to unmount the volume
		{
			OSErr	updateErr = UpdateInMemoryStructures( GPtr );
			if ( err == noErr )
				err = updateErr;

		//	MarkFileSystemFree();
		}
		
		{
			ParamBlockRec theParam;
		//	OSErr mountErr;
			
			theParam.ioParam.ioCompletion = nil;
			theParam.ioParam.ioNamePtr = nil;
			theParam.ioParam.ioVRefNum = GPtr->DrvNum;
			
		//	mountErr = PBMountVol( &theParam );
		}
	}
#endif
	return( err );
}


/*------------------------------------------------------------------------------
Routine:	MRepair		- (Minor Repair)
Function:	Performs minor repair operations.
Input:		GPtr		-	pointer to scavenger global area
Output:		MRepair		-	function result:			
------------------------------------------------------------------------------*/

int MRepair( SGlobPtr GPtr )
{
	OSErr			err;
	SVCB		*calculatedVCB	= GPtr->calculatedVCB;
	Boolean			isHFSPlus		= GPtr->isHFSPlus;
 
#if 0
RebuildBtrees:	
	if ( rebuildErr == noErr )
	{
		if ( GPtr->EBTStat & S_RebuildBTree )
		{
			rebuildErr	= RebuildBTree( GPtr, kCalculatedExtentRefNum );
			GPtr->EBTStat &= ~S_RebuildBTree;
		}
		if ( GPtr->CBTStat & S_RebuildBTree )
		{
			rebuildErr	= RebuildBTree( GPtr, kCalculatedCatalogRefNum );
			GPtr->CBTStat &= ~S_RebuildBTree;
		}
		if ( GPtr->ABTStat & S_RebuildBTree )
		{
			rebuildErr	= RebuildBTree( GPtr, kCalculatedAttributesRefNum );
			GPtr->ABTStat &= ~S_RebuildBTree;
		}
	}
#endif	
	//  Handle repair orders.  Note that these must be done *BEFORE* the MDB is updated.
	err = DoMinorOrders( GPtr );
	ReturnIfError( err );
  	err = CheckForStop( GPtr ); ReturnIfError( err );

	/* Clear Catalog status for things repaired by DoMinorOrders */
	GPtr->CatStat &= ~(S_FileAllocation | S_Permissions | S_UnlinkedFile | S_LinkCount);

	/*
	 * Fix missing thread records
	 */
	if (GPtr->CatStat & S_MissingThread) {
		err = FixMissingThreadRecords(GPtr);
		ReturnIfError(err);
		
		GPtr->CatStat &= ~S_MissingThread;
		GPtr->CBTStat |= S_BTH;  /* leaf record count changed */
	}

	//	2210409, in System 8.1, moving file or folder would cause HFS+ thread records to be
	//	520 bytes in size.  We only shrink the threads if other repairs are needed.
	if ( GPtr->VeryMinorErrorsStat & S_BloatedThreadRecordFound )
	{
		(void) FixBloatedThreadRecords( GPtr );
		GPtr->VeryMinorErrorsStat &= ~S_BloatedThreadRecordFound;
	}

	//
	//	we will update the following data structures regardless of whether we have done
	//	major or minor repairs, so we might end up doing this multiple times. Look into this.
	//
	
	//
	//	Isolate and fix Overlapping Extents
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->VIStat & S_OverlappingExtents) != 0 )
	{
		err = FixOverlappingExtents( GPtr );						//	Isolate and fix Overlapping Extents
		ReturnIfError( err );
		
		GPtr->VIStat &= ~S_OverlappingExtents;
		GPtr->VIStat |= S_VBM;										//	Now that we changed the extents, we need to rebuild the bitmap
		InvalidateCalculatedVolumeBitMap( GPtr );					//	Invalidate our BitMap
	}
	
	//
	//	FixOrphanedFiles
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->CBTStat & S_Orphan) != 0 )
	{
		err = FixOrphanedFiles ( GPtr );							//	Orphaned file were found
		ReturnIfError( err );
	}

	//
	//	FixOrphanedExtent records
	//
	if ( (GPtr->EBTStat & S_OrphanedExtent) != 0 )					//	Orphaned extents were found
	{
		err = FixOrphanedExtent( GPtr );
		GPtr->EBTStat &= ~S_OrphanedExtent;
	//	if ( err == errRebuildBtree )
	//		goto RebuildBtrees;
		ReturnIfError( err );
	}

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	//
	//	Update the extent BTree header and bit map
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->EBTStat & S_BTH) || (GPtr->EBTStat & S_ReservedBTH) )
	{
		err = UpdateBTreeHeader( GPtr->calculatedExtentsFCB );	//	update extent file BTH

		if ( (err == noErr) && (GPtr->EBTStat & S_ReservedBTH) )
		{
			err = FixBTreeHeaderReservedFields( GPtr, kCalculatedExtentRefNum );
		}

		ReturnIfError( err );
	}


	if ( (GPtr->EBTStat & S_BTM) != 0 )
	{
		err = UpdBTM( GPtr, kCalculatedExtentRefNum );				//	update extent file BTM
		ReturnIfError( err );
	}
	
	//
	//	Update the catalog BTree header and bit map
	//

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->CBTStat & S_BTH) || (GPtr->CBTStat & S_ReservedBTH) )
	{
		err = UpdateBTreeHeader( GPtr->calculatedCatalogFCB );	//	update catalog BTH

		if ( (err == noErr) && (GPtr->CBTStat & S_ReservedBTH) )
		{
			err = FixBTreeHeaderReservedFields( GPtr, kCalculatedCatalogRefNum );
		}

		ReturnIfError( err );
	}

	if ( GPtr->CBTStat & S_BTM )
	{
		err = UpdBTM( GPtr, kCalculatedCatalogRefNum );				//	update catalog BTM
		ReturnIfError( err );
	}

	if ( (GPtr->CBTStat & S_ReservedNotZero) != 0 )
	{
		err = RepairReservedBTreeFields( GPtr );					//	update catalog fields
		ReturnIfError( err );
	}
	
	//
	//	Update the volume bit map
	//
	// Note, moved volume bit map update to end after other repairs
	// (except the MDB / VolumeHeader) have been completed
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt

	if ( (GPtr->VIStat & S_VBM) != 0 )
	{
		err = UpdateVolumeBitMap( GPtr, false );					//	update VolumeBitMap
		ReturnIfError( err );
		InvalidateCalculatedVolumeBitMap( GPtr );					//	Invalidate our BitMap
	}

	//
	//	Update the MDB / VolumeHeader
	//
	// Note, moved MDB / VolumeHeader update to end 
	// after all other repairs have been completed
	//
 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( ((GPtr->VIStat & S_MDB) != 0) || IsVCBDirty(calculatedVCB) )		//	update MDB / VolumeHeader
	{
		MarkVCBDirty(calculatedVCB);								// make sure its dirty
		calculatedVCB->vcbAttributes |= kHFSVolumeUnmountedMask;
		err = FlushAlternateVolumeControlBlock( calculatedVCB, isHFSPlus );	//	Writes real & alt blocks
		ReturnIfError( err );
	}


 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
	
	return( err );													//	all done
}



//
//	Internal Routines
//
		

/*------------------------------------------------------------------------------
Routine:	UpdateBTreeHeader - (Update BTree Header)

Function:	Replaces a BTH on disk with info from a scavenger BTCB.
			
Input:		GPtr		-	pointer to scavenger global area
			refNum		-	file refnum

Output:		UpdateBTreeHeader	-	function result:			
								0	= no error
								n 	= error
------------------------------------------------------------------------------*/

static	OSErr	UpdateBTreeHeader( SFCB * fcbPtr )
{
	OSErr err;

	M_BTreeHeaderDirty( ((BTreeControlBlockPtr) fcbPtr->fcbBtree) );	
	err = BTFlushPath( fcbPtr );
	
	return( err );

} /* End UpdateBTreeHeader */


		
/*------------------------------------------------------------------------------
Routine:	FixBTreeHeaderReservedFields

Function:	Fix reserved fields in BTree Header
			
Input:		GPtr		-	pointer to scavenger global area
			refNum		-	file refnum

Output:		0	= no error
			n 	= error
------------------------------------------------------------------------------*/

static	OSErr	FixBTreeHeaderReservedFields( SGlobPtr GPtr, short refNum )
{
	OSErr        err;
	BTHeaderRec  header;

	err = GetBTreeHeader(GPtr, ResolveFCB(refNum), &header);
	ReturnIfError( err );
	
	if ( (header.clumpSize % GPtr->calculatedVCB->vcbBlockSize) != 0 )
		header.clumpSize = GPtr->calculatedVCB->vcbBlockSize;
		
	header.reserved1	= 0;
	header.btreeType	= kHFSBTreeType;  //	control file
	header.reserved2	= 0;
	ClearMemory( header.reserved3, sizeof(header.reserved3) );

	return( err );
}


		

/*------------------------------------------------------------------------------

Routine:	UpdBTM - (Update BTree Map)

Function:	Replaces a BTM on disk with a scavenger BTM.
			
Input:		GPtr		-	pointer to scavenger global area
			refNum		-	file refnum

Output:		UpdBTM	-	function result:			
								0	= no error
								n 	= error
------------------------------------------------------------------------------*/

static	OSErr	UpdBTM( SGlobPtr GPtr, short refNum )
{
	OSErr				err;
	UInt16				recSize;
	SInt16				mapSize;
	SInt16				size;
	SInt16				recIndx;
	Ptr					p;
	Ptr					btmP;
	Ptr					sbtmP;
	UInt32				nodeNum;
	NodeRec				node;
	UInt32				fLink;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( refNum );

	//	Set up
	mapSize			= ((BTreeExtensionsRec*)calculatedBTCB->refCon)->BTCBMSize;

	//
	//	update the map records
	//
	if ( mapSize > 0 )
	{
		nodeNum	= 0;
		recIndx	= 2;
		sbtmP	= ((BTreeExtensionsRec*)calculatedBTCB->refCon)->BTCBMPtr;
		
		do
		{
			GPtr->TarBlock = nodeNum;								//	set target node number
				
			err = GetNode( calculatedBTCB, nodeNum, &node );
			ReturnIfError( err );									//	could't get map node
	
			//	Locate the map record
			recSize = GetRecordSize( calculatedBTCB, (BTNodeDescriptor *)node.buffer, recIndx );
			btmP = (Ptr)GetRecordAddress( calculatedBTCB, (BTNodeDescriptor *)node.buffer, recIndx );
			fLink	= ((NodeDescPtr)node.buffer)->fLink;
			size	= ( recSize  > mapSize ) ? mapSize : recSize;
				
			CopyMemory( sbtmP, btmP, size );						//	update it
			
			err = UpdateNode( calculatedBTCB, &node );				//	write it, and unlock buffer
			
			mapSize	-= size;										//	move to next map record
			if ( mapSize == 0 )										//	more to go?
				break;												//	no, zero remainder of record
			if ( fLink == 0 )										//	out of bitmap blocks in file?
			{
				RcdError( GPtr, E_ShortBTM );
				(void) ReleaseNode(calculatedBTCB, &node);
				return( E_ShortBTM );
			}
				
			nodeNum	= fLink;
			sbtmP	+= size;
			recIndx	= 0;
			
		} while ( mapSize > 0 );

		//	clear the unused portion of the map record
		for ( p = btmP + size ; p < btmP + recSize ; p++ )
			*p = 0; 

		err = UpdateNode( calculatedBTCB, &node );					//	Write it, and unlock buffer
	}
	
	return( noErr );												//	All done
}	//	end UpdBTM


		

/*------------------------------------------------------------------------------

Routine:	UpdateVolumeBitMap - (Update Volume Bit Map)

Function:	Replaces the VBM on disk with the scavenger VBM.
			
Input:		GPtr			-	pointer to scavenger global area

Output:		UpdateVolumeBitMap			- 	function result:
									0 = no error
									n = error
			GPtr->VIStat	-	S_VBM flag set if VBM is damaged.
------------------------------------------------------------------------------*/

static	OSErr	UpdateVolumeBitMap( SGlobPtr GPtr, Boolean preAllocateOverlappedExtents )
{
	GPtr->TarID = VBM_FNum;

	return ( CheckVolumeBitMap(GPtr, true) );
}


/*------------------------------------------------------------------------------

Routine:	DoMinorOrders

Function:	Execute minor repair orders.

Input:		GPtr	- ptr to scavenger global data

Outut:		function result:
				0 - no error
				n - error
------------------------------------------------------------------------------*/

static	OSErr	DoMinorOrders( SGlobPtr GPtr )				//	the globals
{
	RepairOrderPtr		p;
	OSErr				err	= noErr;						//	initialize to "no error"
	
	while( (p = GPtr->MinorRepairsP) && (err == noErr) )	//	loop over each repair order
	{
		GPtr->MinorRepairsP = p->link;						//	unlink next from list
		
		switch( p->type )									//	branch on repair type
		{
			case E_RtDirCnt:								//	the valence errors
			case E_RtFilCnt:								//	(of which there are several)
			case E_DirCnt:
			case E_FilCnt:
			case E_DirVal:
				err = UpdVal( GPtr, p );					//	handle a valence error
				break;
			
			case E_LockedDirName:
				err = FixFinderFlags( GPtr, p );
				break;

			case E_UnlinkedFile:
				err = DeleteUnlinkedFile( GPtr, p );
				break;

			case E_InvalidLinkCount:
				err = FixLinkCount( GPtr, p );
				break;
			
			case E_InvalidPermissions:
			case E_InvalidUID:
				err = FixBSDInfo( GPtr, p );
				break;
			
			case E_NoFile:									//	dangling file thread
				err = DelFThd( GPtr, p->parid );			//	delete the dangling thread
				break;

			//ее	E_NoFile case is never hit since VLockedChk() registers the error, 
			//ее	and returns the error causing the verification to quit.
			case E_EntryNotFound:
				GPtr->EBTStat |= S_OrphanedExtent;
				break;

			//ее	Same with E_NoDir
			case E_NoDir:									//	missing directory record
				err = FixDirThread( GPtr, p->parid );		//	fix the directory thread record
				break;
			
			case E_InvalidMDBdrAlBlSt:
				err = FixEmbededVolDescription( GPtr, p );
				break;
			
			case E_InvalidWrapperExtents:
				err = FixWrapperExtents(GPtr, p);
				break;

			case E_PEOF:
			case E_LEOF:
				err = FixFileSize(GPtr, p);
				break;
			default:										//	unknown repair type
				err = IntError( GPtr, R_IntErr );			//	treat as an internal error
				break;
		}
		
		DisposeMemory( p );								//	free the node
	}
	
	return( err );											//	return error code to our caller
}



/*------------------------------------------------------------------------------

Routine:	DelFThd - (delete file thread)

Function:	Executes the delete dangling file thread repair orders.  These are typically
			threads left after system 6 deletes an aliased file, since system 6 is not
			aware of aliases and thus will not delete the thread along with the file.

Input:		GPtr	- global data
			fid		- the thread record's key's parent-ID

Output:		0 - no error
			n - deletion failed
Modification History:
	29Oct90		KST		CBTDelete was using "k" as key which points to cache buffer.
-------------------------------------------------------------------------------*/

static	int	DelFThd( SGlobPtr GPtr, UInt32 fid )				//	the file ID
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	UInt32				hint;								//	as returned by CBTSearch
	OSErr				result;								//	status return
	UInt16				recSize;
	ExtentRecord		zeroExtents;
	
	BuildCatalogKey( fid, (const CatalogName*) nil, GPtr->isHFSPlus, &key );
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );
	
	if ( result )	return ( IntError( GPtr, result ) );
	
	if ( (record.recordType != kHFSFileThreadRecord) && (record.recordType != kHFSPlusFileThreadRecord) )	//	quit if not a file thread
		return ( IntError( GPtr, R_IntErr ) );
	
	//	Zero the record on disk
	ClearMemory( (Ptr)&zeroExtents, sizeof(ExtentRecord) );
	result	= ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &key, hint, &zeroExtents, recSize, &hint );
	if ( result )	return ( IntError( GPtr, result ) );
	
	result	= DeleteBTreeRecord( GPtr->calculatedCatalogFCB, &key );
	if ( result )	return ( IntError( GPtr, result ) );
	
	//	After deleting a record, we'll need to write back the BT header and map,
	//	to reflect the updated record count etc.
	   
	GPtr->CBTStat |= S_BTH + S_BTM;							//	set flags to write back hdr and map

	return( noErr );										//	successful return
}
	

/*------------------------------------------------------------------------------

Routine:	FixDirThread - (fix directory thread record's parent ID info)

Function:	Executes the missing directory record repair orders most likely caused by 
			disappearing folder bug.  This bug causes some folders to jump to Desktop 
			from the root window.  The catalog directory record for such a folder has 
			the Desktop folder as the parent but its thread record still the root 
			directory as its parent.

Input:		GPtr	- global data
			did		- the thread record's key's parent-ID

Output:		0 - no error
			n - deletion failed
-------------------------------------------------------------------------------*/

static	OSErr	FixDirThread( SGlobPtr GPtr, UInt32 did )	//	the dir ID
{
	UInt8				*dataPtr;
	UInt32				hint;							//	as returned by CBTSearch
	OSErr				result;							//	status return
	UInt16				recSize;
	CatalogName			catalogName;					//	temporary name record
	CatalogName			*keyName;						//	temporary name record
	register short 		index;							//	loop index for all records in the node
	UInt32  			curLeafNode;					//	current leaf node being checked
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	CatalogKey		 	*keyP;
	SInt16				recordType;
	UInt32				folderID;
	NodeRec				node;
	NodeDescPtr			nodeDescP;
	UInt32				newParDirID		= 0;			//	the parent ID where the dir record is really located
	Boolean				isHFSPlus		= GPtr->isHFSPlus;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( kCalculatedCatalogRefNum );
	
	
	BuildCatalogKey( did, (const CatalogName*) nil, isHFSPlus, &key );
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );
	
	if ( result )
		return( IntError( GPtr, result ) );
	if ( (record.recordType != kHFSFolderThreadRecord) && (record.recordType != kHFSPlusFolderThreadRecord) )			//	quit if not a directory thread
		return ( IntError( GPtr, R_IntErr ) );
		
	curLeafNode = calculatedBTCB->freeNodes;
	
	while ( curLeafNode )
	{
		result = GetNode( calculatedBTCB, curLeafNode, &node );
		if ( result != noErr ) return( IntError( GPtr, result ) );
		
		nodeDescP = node.buffer;

		// loop on number of records in node
		for ( index = 0 ; index < nodeDescP->numRecords ; index++ )
		{
			GetRecordByIndex( calculatedBTCB, (NodeDescPtr)nodeDescP, index, (BTreeKey **)&keyP, &dataPtr, &recSize );
			
			recordType	= ((CatalogRecord *)dataPtr)->recordType;
			folderID	= recordType == kHFSPlusFolderRecord ? ((HFSPlusCatalogFolder *)dataPtr)->folderID : ((HFSCatalogFolder *)dataPtr)->folderID;
			
			// did we locate a directory record whode dirID match the the thread's key's parent dir ID?
			if ( (folderID == did) && ( recordType == kHFSPlusFolderRecord || recordType == kHFSFolderRecord ) )
			{
				newParDirID	= recordType == kHFSPlusFolderRecord ? keyP->hfsPlus.parentID : keyP->hfs.parentID;
				keyName		= recordType == kHFSPlusFolderRecord ? (CatalogName *)&keyP->hfsPlus.nodeName : (CatalogName *)&keyP->hfs.nodeName;
				CopyCatalogName( keyName, &catalogName, isHFSPlus );
				break;
			}
		}

		if ( newParDirID ) {
			(void) ReleaseNode(calculatedBTCB, &node);
			break;
		}

		curLeafNode = nodeDescP->fLink;	 // sibling of this leaf node
		
		(void) ReleaseNode(calculatedBTCB, &node);
	}
		
	if ( newParDirID == 0 )
	{
		return ( IntError( GPtr, R_IntErr ) ); // ее  Try fixing by creating a new directory record?
	}
	else
	{
		(void) SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );

		if ( isHFSPlus )
		{
			HFSPlusCatalogThread	*largeCatalogThreadP	= (HFSPlusCatalogThread *) &record;
			
			largeCatalogThreadP->parentID = newParDirID;
			CopyCatalogName( &catalogName, (CatalogName *) &largeCatalogThreadP->nodeName, isHFSPlus );
		}
		else
		{
			HFSCatalogThread	*smallCatalogThreadP	= (HFSCatalogThread *) &record;
			
			smallCatalogThreadP->parentID = newParDirID;
			CopyCatalogName( &catalogName, (CatalogName *)&smallCatalogThreadP->nodeName, isHFSPlus );
		}
		
		result = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recSize, &hint );
	}

	return( noErr );										//	successful return
}
	

/*------------------------------------------------------------------------------

Routine:	UpdVal - (Update Valence)

Function:	Replaces out of date valences with correct vals computed during scavenge.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:		UpdVal			- 	function result:
									0 = no error
									n = error
------------------------------------------------------------------------------*/

static	OSErr	UpdVal( SGlobPtr GPtr, RepairOrderPtr p )					//	the valence repair order
{
	OSErr				result;						//	status return
	UInt32				hint;						//	as returned by CBTSearch
	UInt16				recSize;
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	SVCB			*calculatedVCB = GPtr->calculatedVCB;
	
	switch( p->type )
	{
		case E_RtDirCnt: //	invalid count of Dirs in Root
			if ( (UInt16)p->incorrect != calculatedVCB->vcbNmRtDirs )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbNmRtDirs = (UInt16)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
			
		case E_RtFilCnt:
			if ( (UInt16)p->incorrect != calculatedVCB->vcbNmFls )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbNmFls = (UInt16)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
			
		case E_DirCnt:
			if ( (UInt32)p->incorrect != calculatedVCB->vcbFolderCount )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbFolderCount = (UInt32)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
			
		case E_FilCnt:
			if ( (UInt32)p->incorrect != calculatedVCB->vcbFileCount )
				return ( IntError( GPtr, R_IntErr ) );
			calculatedVCB->vcbFileCount = (UInt32)p->correct;
			GPtr->VIStat |= S_MDB;
			break;
	
		case E_DirVal:
			BuildCatalogKey( p->parid, (CatalogName *)&p->name, GPtr->isHFSPlus, &key );
			result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint,
					&foundKey, &record, &recSize, &hint );
			if ( result )
				return ( IntError( GPtr, result ) );
				
			if ( record.recordType == kHFSPlusFolderRecord )
			{
				if ( (UInt32)p->incorrect != record.hfsPlusFolder.valence)
					return ( IntError( GPtr, R_IntErr ) );
				record.hfsPlusFolder.valence = (UInt32)p->correct;
			}
			else
			{
				if ( (UInt16)p->incorrect != record.hfsFolder.valence )
					return ( IntError( GPtr, R_IntErr ) );
				record.hfsFolder.valence = (UInt16)p->correct;
			}
				
			result = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &key, hint,\
						&record, recSize, &hint );
			if ( result )
				return ( IntError( GPtr, result ) );
			break;
	}
		
	return( noErr );														//	no error
}

/*------------------------------------------------------------------------------

Routine:	FixFinderFlags

Function:	Changes some of the Finder flag bits for directories.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:		FixFinderFlags	- 	function result:
									0 = no error
									n = error
------------------------------------------------------------------------------*/

static	OSErr	FixFinderFlags( SGlobPtr GPtr, RepairOrderPtr p )				//	the repair order
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	UInt32				hint;												//	as returned by CBTSearch
	OSErr				result;												//	status return
	UInt16				recSize;
	
	BuildCatalogKey( p->parid, (CatalogName *)&p->name, GPtr->isHFSPlus, &key );

	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );
	if ( result )
		return ( IntError( GPtr, result ) );

	if ( record.recordType == kHFSPlusFolderRecord )
	{
		HFSPlusCatalogFolder	*largeCatalogFolderP	= (HFSPlusCatalogFolder *) &record;	
		if ( (UInt16) p->incorrect != largeCatalogFolderP->userInfo.frFlags )
		{
			//	Another repar order may have affected the flags
			if ( p->correct < p->incorrect )
				largeCatalogFolderP->userInfo.frFlags &= ~((UInt16)p->maskBit);
			else
				largeCatalogFolderP->userInfo.frFlags |= (UInt16)p->maskBit;
		}
		else
		{
			largeCatalogFolderP->userInfo.frFlags = (UInt16)p->correct;
		}
	//	largeCatalogFolderP->contentModDate = timeStamp;
	}
	else
	{
		HFSCatalogFolder	*smallCatalogFolderP	= (HFSCatalogFolder *) &record;	
		if ( p->incorrect != smallCatalogFolderP->userInfo.frFlags )		//	do we know what we're doing?
		{
			//	Another repar order may have affected the flags
			if ( p->correct < p->incorrect )
				smallCatalogFolderP->userInfo.frFlags &= ~((UInt16)p->maskBit);
			else
				smallCatalogFolderP->userInfo.frFlags |= (UInt16)p->maskBit;
		}
		else
		{
			smallCatalogFolderP->userInfo.frFlags = (UInt16)p->correct;
		}
		
	//	smallCatalogFolderP->modifyDate = timeStamp;						// also update the modify date! -DJB
	}

	result = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recSize, &hint );	//	write the node back to the file
	if ( result )
		return( IntError( GPtr, result ) );
		
	return( noErr );														//	no error
}


/*------------------------------------------------------------------------------
FixLinkCount:  Adjust a data node link count (BSD hard link)
               (HFS Plus volumes only)
------------------------------------------------------------------------------*/
static OSErr
FixLinkCount(SGlobPtr GPtr, RepairOrderPtr p)
{
	SFCB *fcb;
	CatalogRecord rec;
	HFSPlusCatalogKey * keyp;
	FSBufferDescriptor btRecord;
	BTreeIterator btIterator;
	size_t len;
	OSErr result;
	UInt16 recSize;

	if (!GPtr->isHFSPlus)
		return (0);
	fcb = GPtr->calculatedCatalogFCB;

	ClearMemory(&btIterator, sizeof(btIterator));
	btIterator.hint.nodeNum = p->hint;
	keyp = (HFSPlusCatalogKey*)&btIterator.key;
	keyp->parentID = p->parid;
	/* name was stored in UTF-8 */
	(void) utf_decodestr(&p->name[1], p->name[0], keyp->nodeName.unicode, &len);
	keyp->nodeName.length = len / 2;
	keyp->keyLength = kHFSPlusCatalogKeyMinimumLength + len;

	btRecord.bufferAddress = &rec;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(rec);
	
	result = BTSearchRecord(fcb, &btIterator, kInvalidMRUCacheKey,
			&btRecord, &recSize, &btIterator);
	if (result)
		return (IntError(GPtr, result));

	if (rec.recordType != kHFSPlusFileRecord)
		return (noErr);

	if ((UInt32)p->correct != rec.hfsPlusFile.bsdInfo.special.linkCount) {
		if (GPtr->logLevel >= kDebugLog)
		    printf("\t%s: fixing link count from %d to %d\n",
		        &p->name[1], rec.hfsPlusFile.bsdInfo.special.linkCount, (int)p->correct);

		rec.hfsPlusFile.bsdInfo.special.linkCount = (UInt32)p->correct;
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
		if (result)
			return (IntError(GPtr, result));
	}
		
	return (noErr);
}


/*------------------------------------------------------------------------------
FixBSDInfo:  Reset or repair BSD info
                 (HFS Plus volumes only)
------------------------------------------------------------------------------*/
static OSErr
FixBSDInfo(SGlobPtr GPtr, RepairOrderPtr p)
{
	SFCB *fcb;
	CatalogRecord rec;
	FSBufferDescriptor btRecord;
	BTreeIterator btIterator;
	OSErr result;
	UInt16 recSize;
	size_t namelen;
	unsigned char filename[256];

	if (!GPtr->isHFSPlus)
		return (0);
	fcb = GPtr->calculatedCatalogFCB;

	ClearMemory(&btIterator, sizeof(btIterator));
	btIterator.hint.nodeNum = p->hint;
	BuildCatalogKey(p->parid, (CatalogName *)&p->name, true,
		(CatalogKey*)&btIterator.key);
	btRecord.bufferAddress = &rec;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(rec);
	
	result = BTSearchRecord(fcb, &btIterator, kInvalidMRUCacheKey,
			&btRecord, &recSize, &btIterator);
	if (result)
		return (IntError(GPtr, result));

	if (rec.recordType != kHFSPlusFileRecord &&
	    rec.recordType != kHFSPlusFolderRecord)
		return (noErr);

	utf_encodestr(((HFSUniStr255 *)&p->name)->unicode,
		((HFSUniStr255 *)&p->name)->length << 1, filename, &namelen);
	filename[namelen] = '\0';

	if (p->type == E_InvalidPermissions &&
	    ((UInt16)p->incorrect == rec.hfsPlusFile.bsdInfo.fileMode)) {
		if (GPtr->logLevel >= kDebugLog)
		    printf("\t\"%s\": fixing mode from %07o to %07o\n",
			   filename, (int)p->incorrect, (int)p->correct);

		rec.hfsPlusFile.bsdInfo.fileMode = (UInt16)p->correct;
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
	}

	if (p->type == E_InvalidUID) {
		if ((UInt32)p->incorrect == rec.hfsPlusFile.bsdInfo.ownerID) {
			if (GPtr->logLevel >= kDebugLog) {
				printf("\t\"%s\": replacing UID %d with %d\n",
				filename, (int)p->incorrect, (int)p->correct);
			}
			rec.hfsPlusFile.bsdInfo.ownerID = (UInt32)p->correct;
		}
		/* Fix group ID if neccessary */
		if ((UInt32)p->incorrect == rec.hfsPlusFile.bsdInfo.groupID)
			rec.hfsPlusFile.bsdInfo.groupID = (UInt32)p->correct;
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
	}

	if (result)
		return (IntError(GPtr, result));
	else	
		return (noErr);
}


/*------------------------------------------------------------------------------
DeleteUnlinkedFile:  Delete orphaned data node (BSD unlinked file)
		     Also used to delete empty "HFS+ Private Data" directories
                     (HFS Plus volumes only)
------------------------------------------------------------------------------*/
static OSErr
DeleteUnlinkedFile(SGlobPtr GPtr, RepairOrderPtr p)
{
	CatalogName name;
	CatalogName *cNameP;
	size_t len;

	if (!GPtr->isHFSPlus)
		return (0);

	if (p->name[0] > 0) {
		/* name was stored in UTF-8 */
		(void) utf_decodestr(&p->name[1], p->name[0], name.ustr.unicode, &len);
		name.ustr.length = len / 2;
		cNameP = &name;
	} else {
		cNameP = NULL;
	}

	(void) DeleteCatalogNode(GPtr->calculatedVCB, p->parid, cNameP, p->hint);

	GPtr->VIStat |= S_MDB;
	GPtr->VIStat |= S_VBM;

	return (noErr);
}

/*
 * Fix a file's PEOF or LEOF (truncate)
 * (HFS Plus volumes only)
 */
static OSErr
FixFileSize(SGlobPtr GPtr, RepairOrderPtr p)
{
	SFCB *fcb;
	CatalogRecord rec;
	HFSPlusCatalogKey * keyp;
	FSBufferDescriptor btRecord;
	BTreeIterator btIterator;
	size_t len;
	Boolean replace;
	OSErr result;
	UInt16 recSize;
	UInt64 bytes;

	if (!GPtr->isHFSPlus)
		return (0);
	fcb = GPtr->calculatedCatalogFCB;
	replace = false;

	ClearMemory(&btIterator, sizeof(btIterator));
	btIterator.hint.nodeNum = p->hint;
	keyp = (HFSPlusCatalogKey*)&btIterator.key;
	keyp->parentID = p->parid;

	/* name was stored in UTF-8 */
	(void) utf_decodestr(&p->name[1], p->name[0], keyp->nodeName.unicode, &len);
	keyp->nodeName.length = len / 2;
	keyp->keyLength = kHFSPlusCatalogKeyMinimumLength + len;

	btRecord.bufferAddress = &rec;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(rec);
	
	result = BTSearchRecord(fcb, &btIterator, kInvalidMRUCacheKey,
			&btRecord, &recSize, &btIterator);
	if (result)
		return (IntError(GPtr, result));

	if (rec.recordType != kHFSPlusFileRecord)
		return (noErr);

	if (p->type == E_PEOF) {
		bytes = p->correct * (UInt64)GPtr->calculatedVCB->vcbBlockSize;
		if ((p->forkType == kRsrcFork) &&
		    ((UInt32)p->incorrect == rec.hfsPlusFile.resourceFork.totalBlocks)) {

			rec.hfsPlusFile.resourceFork.totalBlocks = (UInt32)p->correct;
			replace = true;
			/*
			 * Make sure our new block count is large
			 * enough to cover the current LEOF.  If
			 * its not we must truncate the fork.
			 */
			if (rec.hfsPlusFile.resourceFork.logicalSize > bytes) {
				rec.hfsPlusFile.resourceFork.logicalSize = bytes;
			}
		} else if ((p->forkType == kDataFork) &&
		           ((UInt32)p->incorrect == rec.hfsPlusFile.dataFork.totalBlocks)) {

			rec.hfsPlusFile.dataFork.totalBlocks = (UInt32)p->correct;
			replace = true;
			/*
			 * Make sure our new block count is large
			 * enough to cover the current LEOF.  If
			 * its not we must truncate the fork.
			 */
			if (rec.hfsPlusFile.dataFork.logicalSize > bytes) {
				rec.hfsPlusFile.dataFork.logicalSize = bytes;
			}
		}
	} else /* E_LEOF */ {
		if ((p->forkType == kRsrcFork) &&
		    (p->incorrect == rec.hfsPlusFile.resourceFork.logicalSize)) {

			rec.hfsPlusFile.resourceFork.logicalSize = p->correct;
			replace = true;

		} else if ((p->forkType == kDataFork) &&
		           (p->incorrect == rec.hfsPlusFile.dataFork.logicalSize)) {

			rec.hfsPlusFile.dataFork.logicalSize = p->correct;
			replace = true;
		}
	}

	if (replace) {
		result = BTReplaceRecord(fcb, &btIterator, &btRecord, recSize);
		if (result)
			return (IntError(GPtr, result));
	}
		
	return (noErr);
}

/*------------------------------------------------------------------------------

Routine:	FixEmbededVolDescription

Function:	If the "mdb->drAlBlSt" field has been modified, i.e. Norton Disk Doctor 3.5 tried to "Fix" an HFS+ volume, it
			reduces the value in the "mdb->drAlBlSt" field.  If this field is changed, the file system can no longer find
			the VolumeHeader or AltVolumeHeader.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:		FixMDBdrAlBlSt	- 	function result:
									0 = no error
									n = error
------------------------------------------------------------------------------*/

static	OSErr	FixEmbededVolDescription( SGlobPtr GPtr, RepairOrderPtr p )
{
	OSErr			err;
	UInt64			totalSectors;
	UInt32			sectorSize;
	HFSMasterDirectoryBlock	*mdb;
	EmbededVolDescription	*desc;
	SVCB			*vcb = GPtr->calculatedVCB;
	BlockDescriptor  block;

	desc = (EmbededVolDescription *) &(p->name);
	
	/* Fix the Alternate MDB */
	err = GetDeviceSize( vcb->vcbDriveNumber, &totalSectors, &sectorSize );
	ReturnIfError( err );
	
	err = GetVolumeBlock(vcb, totalSectors - 2, kGetBlock, &block);
	ReturnIfError( err );
	mdb = (HFSMasterDirectoryBlock *) block.buffer;
	
	mdb->drAlBlSt			= desc->drAlBlSt;
	mdb->drEmbedSigWord		= desc->drEmbedSigWord;
	mdb->drEmbedExtent.startBlock	= desc->drEmbedExtent.startBlock;
	mdb->drEmbedExtent.blockCount	= desc->drEmbedExtent.blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	ReturnIfError( err );
	
	/* Fix the MDB */
	err = GetVolumeBlock(vcb, 2, kGetBlock, &block);
	ReturnIfError( err );
	mdb = (HFSMasterDirectoryBlock *) block.buffer;
	
	mdb->drAlBlSt			= desc->drAlBlSt;
	mdb->drEmbedSigWord		= desc->drEmbedSigWord;
	mdb->drEmbedExtent.startBlock	= desc->drEmbedExtent.startBlock;
	mdb->drEmbedExtent.blockCount	= desc->drEmbedExtent.blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	
	return( err );
}




/*------------------------------------------------------------------------------

Routine:	FixWrapperExtents

Function:	When Norton Disk Doctor 2.0 tries to repair an HFS Plus volume, it
			assumes that the first catalog extent must be a fixed number of
			allocation blocks after the start of the first extents extent (in the
			wrapper).  In reality, the first catalog extent should start immediately
			after the first extents extent.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:
			0 = no error
			n = error
------------------------------------------------------------------------------*/

static	OSErr	FixWrapperExtents( SGlobPtr GPtr, RepairOrderPtr p )
{
#pragma unused (p)

	OSErr			err;
	UInt64			totalSectors;
	UInt32			sectorSize;
	HFSMasterDirectoryBlock	*mdb;
	SVCB			*vcb = GPtr->calculatedVCB;
	BlockDescriptor  block;

	/* Get the Alternate MDB */
	err = GetDeviceSize( vcb->vcbDriveNumber, &totalSectors, &sectorSize );
	ReturnIfError( err );
	
	err = GetVolumeBlock(vcb, totalSectors - 2, kGetBlock, &block);
	ReturnIfError( err );
	mdb = (HFSMasterDirectoryBlock	*) block.buffer;

	/* Fix the wrapper catalog's first (and only) extent */
	mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock +
	                                mdb->drXTExtRec[0].blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	ReturnIfError( err );

	
	/* Fix the MDB */
	err = GetVolumeBlock(vcb, 2, kGetBlock, &block);
	ReturnIfError( err );
	mdb = (HFSMasterDirectoryBlock	*) block.buffer;
	
	mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock +
	                                mdb->drXTExtRec[0].blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	
	return( err );
}


//
//	Entries in the extents BTree which do not have a corresponding catalog entry get fixed here
//	This routine will run slowly if the extents file is large because we require a Catalog BTree
//	search for each extent record.
//
static	OSErr	FixOrphanedExtent( SGlobPtr GPtr )
{
#if 0
	OSErr				err;
	UInt32				hint;
	UInt32				recordSize;
	UInt32				maxRecords;
	UInt32				numberOfFilesInList;
	ExtentKey			*extentKeyPtr;
	ExtentRecord		*extentDataPtr;
	ExtentRecord		extents;
	ExtentRecord		zeroExtents;
	CatalogKey			foundExtentKey;
	CatalogRecord		catalogData;
	CatalogRecord		threadData;
	HFSCatalogNodeID	fileID;
	BTScanState			scanState;

	HFSCatalogNodeID	lastFileID			= -1;
	UInt32			recordsFound		= 0;
	Boolean			mustRebuildBTree	= false;
	Boolean			isHFSPlus			= GPtr->isHFSPlus;
	SVCB			*calculatedVCB		= GPtr->calculatedVCB;
	UInt32			**dataHandle		= GPtr->validFilesList;
	SFCB *			fcb = GPtr->calculatedExtentsFCB;

	//	Set Up
	//
	//	Use the BTree scanner since we use MountCheck to find orphaned extents, and MountCheck uses the scanner
	err = BTScanInitialize( fcb, 0, 0, 0, gFSBufferPtr, gFSBufferSize, &scanState );
	if ( err != noErr )	return( badMDBErr );

	ClearMemory( (Ptr)&zeroExtents, sizeof(ExtentRecord) );

	if ( isHFSPlus )
	{
		maxRecords = fcb->fcbLogicalSize / sizeof(HFSPlusExtentRecord);
	}
	else
	{
		maxRecords = fcb->fcbLogicalSize / sizeof(HFSExtentRecord);
		numberOfFilesInList = GetHandleSize((Handle) dataHandle) / sizeof(UInt32);
		qsort( *dataHandle, numberOfFilesInList, sizeof (UInt32), cmpLongs );	// Sort the list of found file IDs
	}


	while ( recordsFound < maxRecords )
	{
		err = BTScanNextRecord( &scanState, false, (void **) &extentKeyPtr, (void **) &extentDataPtr, &recordSize );

		if ( err != noErr )
		{
			if ( err == btNotFound )
				err	= noErr;
			break;
		}

		++recordsFound;
		fileID = (isHFSPlus == true) ? extentKeyPtr->hfsPlus.fileID : extentKeyPtr->hfs.fileID;
		
		if ( (fileID > kHFSBadBlockFileID) && (lastFileID != fileID) )	// Keep us out of reserved file trouble
		{
			lastFileID	= fileID;
			
			if ( isHFSPlus )
			{
				err = LocateCatalogThread( calculatedVCB, fileID, &threadData, (UInt16*)&recordSize, &hint );	//	This call returns nodeName as either Str31 or HFSUniStr255, no need to call PrepareInputName()
				
				if ( err == noErr )									//	Thread is found, just verify actual record exists.
				{
					err = LocateCatalogNode( calculatedVCB, threadData.hfsPlusThread.parentID, (const CatalogName *) &(threadData.hfsPlusThread.nodeName), kNoHint, &foundExtentKey, &catalogData, &hint );
				}
				else if ( err == cmNotFound )
				{
					err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, extentKeyPtr, kNoHint, &foundExtentKey, &extents, (UInt16*)&recordSize, &hint );
					if ( err == noErr )
					{	//ее can't we just delete btree record?
						err = ReplaceBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey, hint, &zeroExtents, recordSize, &hint );
						err	= DeleteBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey );	//	Delete the orphaned extent
					}
				}
					
				if ( err != noErr )
					mustRebuildBTree	= true;						//	if we have errors here we should rebuild the extents btree
			}
			else
			{
				if ( ! bsearch( &fileID, *dataHandle, numberOfFilesInList, sizeof(UInt32), cmpLongs ) )
				{
					err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, extentKeyPtr, kNoHint, &foundExtentKey, &extents, (UInt16*)&recordSize, &hint );
					if ( err == noErr )
					{	//ее can't we just delete btree record?
						err = ReplaceBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey, hint, &zeroExtents, recordSize, &hint );
						err = DeleteBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey );	//	Delete the orphaned extent
					}
					
					if ( err != noErr )
						mustRebuildBTree	= true;						//	if we have errors here we should rebuild the extents btree
				}
			}
		}
	}

	if ( mustRebuildBTree == true )
	{
		GPtr->EBTStat |= S_RebuildBTree;
		err	= errRebuildBtree;
	}

	return( err );
#else
	return (0);
#endif
}


//
//	File records, which have the kHFSThreadExistsMask set, but do not have a corresponding
//	thread, or threads which do not have corresponding records get fixed here.
//
static	OSErr	FixOrphanedFiles ( SGlobPtr GPtr )
{
	CatalogKey			key;
	CatalogKey			foundKey;
	CatalogKey			tempKey;
	CatalogRecord		record;
	CatalogRecord		threadRecord;
	CatalogRecord		record2;
	HFSCatalogNodeID	parentID;
	HFSCatalogNodeID	cNodeID = 0;
	BTreeIterator		savedIterator;
	UInt32				hint;
	UInt32				hint2;
	UInt32				threadHint;
	OSErr				err;
	UInt16				recordSize;
	SInt16				recordType;
	SInt16				threadRecordType = 0;
	SInt16				selCode				= 0x8001;
	Boolean				isHFSPlus			= GPtr->isHFSPlus;
	BTreeControlBlock	*btcb				= GetBTreeControlBlock( kCalculatedCatalogRefNum );
	
	CopyMemory( &btcb->lastIterator, &savedIterator, sizeof(BTreeIterator) );

	do
	{
		//	Save/Restore Iterator around calls to GetBTreeRecord
		CopyMemory( &savedIterator, &btcb->lastIterator, sizeof(BTreeIterator) );
		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
		if ( err != noErr ) break;
		CopyMemory( &btcb->lastIterator, &savedIterator, sizeof(BTreeIterator) );
	
		selCode		= 1;														//	 kNextRecord			
		recordType	= record.recordType;
		
		switch( recordType )
		{
			case kHFSFileRecord:
				//	If the small file is not supposed to have a thread, just break
				if ( ( record.hfsFile.flags & kHFSThreadExistsMask ) == 0 )
					break;
			
			case kHFSFolderRecord:
			case kHFSPlusFolderRecord:
			case kHFSPlusFileRecord:
				
				//	Locate the thread associated with this record
				
				(void) CheckForStop( GPtr );										//	rotate cursor

				parentID	= isHFSPlus == true ? foundKey.hfsPlus.parentID : foundKey.hfs.parentID;
				threadHint	= hint;
				
				switch( recordType )
				{
					case kHFSFolderRecord:		cNodeID		= record.hfsFolder.folderID;		break;
					case kHFSFileRecord:		cNodeID		= record.hfsFile.fileID;			break;
					case kHFSPlusFolderRecord:	cNodeID		= record.hfsPlusFolder.folderID;	break;
					case kHFSPlusFileRecord:	cNodeID		= record.hfsPlusFile.fileID;		break;
				}

				//-- Build the key for the file thread
				BuildCatalogKey( cNodeID, nil, isHFSPlus, &key );

				err = SearchBTreeRecord ( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &threadRecord, &recordSize, &hint2 );

				if ( err != noErr )
				{
					//	For missing thread records, just create the thread
					if ( err == btNotFound )
					{
						//	Create the missing thread record.
						switch( recordType )
						{
							case kHFSFolderRecord:		threadRecordType	= kHFSFolderThreadRecord;		break;
							case kHFSFileRecord:		threadRecordType	= kHFSFileThreadRecord;			break;
							case kHFSPlusFolderRecord:	threadRecordType	= kHFSPlusFolderThreadRecord;	break;
							case kHFSPlusFileRecord:	threadRecordType	= kHFSPlusFileThreadRecord;		break;
						}

						//-- Fill out the data for the new file thread
						
						if ( isHFSPlus )
						{
							HFSPlusCatalogThread		threadData;
							UInt16 recSize;
							
							ClearMemory( (Ptr)&threadData, sizeof(HFSPlusCatalogThread) );
							threadData.recordType	= threadRecordType;
							threadData.parentID		= parentID;
							CopyCatalogName( (CatalogName *)&foundKey.hfsPlus.nodeName,
										(CatalogName *)&threadData.nodeName, isHFSPlus );
							recSize = 10 + (foundKey.hfsPlus.nodeName.length * 2);
							err = InsertBTreeRecord( GPtr->calculatedCatalogFCB, &key,
										&threadData, recSize, &threadHint );

						}
						else
						{
							HFSCatalogThread		threadData;
							
							ClearMemory( (Ptr)&threadData, sizeof(HFSCatalogThread) );
							threadData.recordType	= threadRecordType;
							threadData.parentID		= parentID;
							CopyCatalogName( (CatalogName *)&foundKey.hfs.nodeName, (CatalogName *)&threadData.nodeName, isHFSPlus );
							err = InsertBTreeRecord( GPtr->calculatedCatalogFCB, &key, &threadData, sizeof(HFSCatalogThread), &threadHint );
						}
					}
					else
					{
						break;
					}
				}
			
				break;
			
			
			case kHFSFolderThreadRecord:
			case kHFSFileThreadRecord:
			case kHFSPlusFolderThreadRecord:
			case kHFSPlusFileThreadRecord:
				
				//	Find the catalog record, if it does not exist, delete the existing thread.
				if ( isHFSPlus )
					BuildCatalogKey( record.hfsPlusThread.parentID, (const CatalogName *)&record.hfsPlusThread.nodeName, isHFSPlus, &key );
				else
					BuildCatalogKey( record.hfsThread.parentID, (const CatalogName *)&record.hfsThread.nodeName, isHFSPlus, &key );
				
				err = SearchBTreeRecord ( GPtr->calculatedCatalogFCB, &key, kNoHint, &tempKey, &record2, &recordSize, &hint2 );
				if ( err != noErr )
				{
					err = DeleteBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey );
				}
				
				break;
				
			default:
				M_DebugStr("\p Unknown record type");
				break;

		}
	} while ( err == noErr );

	if ( err == btNotFound )
		err = noErr;				 						//	all done, no more catalog records

//	if (err == noErr)
//		err = BTFlushPath( GPtr->calculatedCatalogFCB );

	return( err );
}


static	OSErr	RepairReservedBTreeFields ( SGlobPtr GPtr )
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	UInt16 				recordSize;
	SInt16				selCode;
	UInt32				hint;
	UInt32				*reserved;
	OSErr				err;

	selCode = 0x8001;															//	 start with 1st record			

	err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	if ( err != noErr ) goto EXIT;

	selCode = 1;																//	 get next record from now on		
	
	do
	{
		switch( record.recordType )
		{
			case kHFSPlusFolderRecord:
				if ( record.hfsPlusFolder.flags != 0 )
				{
					record.hfsPlusFolder.flags = 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;
				
			case kHFSPlusFileRecord:
				//	Note: bit 7 (mask 0x80) of flags is unused in HFS or HFS Plus.  However, Inside Macintosh: Files
				//	describes it as meaning the file record is in use.  Some non-Apple implementations end up setting
				//	this bit, so we just ignore it.
				if ( ( record.hfsPlusFile.flags  & (UInt16) ~(0X83) ) != 0 )
				{
					record.hfsPlusFile.flags &= 0X83;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;

			case kHFSFolderRecord:
				if ( record.hfsFolder.flags != 0 )
				{
					record.hfsFolder.flags = 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;

			case kHFSFolderThreadRecord:
			case kHFSFileThreadRecord:
				reserved = (UInt32*) &(record.hfsThread.reserved);
				if ( reserved[0] || reserved[1] )
				{
					reserved[0]	= 0;
					reserved[1]	= 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;

			case kHFSFileRecord:
				//	Note: bit 7 (mask 0x80) of flags is unused in HFS or HFS Plus.  However, Inside Macintosh: Files
				//	describes it as meaning the file record is in use.  Some non-Apple implementations end up setting
				//	this bit, so we just ignore it.
				if ( 	( ( record.hfsFile.flags  & (UInt8) ~(0X83) ) != 0 )
					||	( record.hfsFile.dataStartBlock != 0 )	
					||	( record.hfsFile.rsrcStartBlock != 0 )	
					||	( record.hfsFile.reserved != 0 )			)
				{
					record.hfsFile.flags			&= 0X83;
					record.hfsFile.dataStartBlock	= 0;
					record.hfsFile.rsrcStartBlock	= 0;
					record.hfsFile.reserved		= 0;
					err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
				}
				break;
				
			default:
				break;
		}

		if ( err != noErr ) goto EXIT;
		
		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	
	} while ( err == noErr );

	if ( err == btNotFound )
		err = noErr;				 						//	all done, no more catalog records

EXIT:
	return( err );
}


//	Traverse the leaf nodes forward until a bad node is found then...
//	Traverse the leaf nodes backwards untill a bad node is found, all the while...
//	Inserting each record into a new BTree.  Once we are done call ExchangeFileIDs,
//	and update the entries in the VH.
static	OSErr	RebuildBTree( SGlobPtr GPtr, SInt16 refNum )
{
#if 0
	CatalogRecord		record;
	CatalogKey			foundKey;
	UInt16 				recordSize;
	UInt16				i;
	SInt16				newBTreeRefNum;
	UInt32				returnedHint;
	UInt32				hint;
	UInt32				blocksUsed;
	OSErr				err;
	Boolean				hasOverflowExtents;
	SInt16				selCode				= 0x8001;										//	 Start with 1st record
	Boolean				isHFSPlus			= GPtr->isHFSPlus;
	SFCB *				fcbPtr;	
	SFCB *				repairFCB			= GPtr->calculatedRepairFCB;
	SVCB			*vcb				= GPtr->calculatedVCB;


		
	
	fcbPtr = ResolveFCB( refNum );

	//	Mark the volume inconsistant
	//	If any errors, ie loss of power, occur before we update the bitmap, MountCheck will clean them up
	vcb->vcbAtrb	&= ~kHFSVolumeUnmountedMask;
	MarkVCBDirty( vcb );
	err = FlushVolumeControlBlock( vcb );
	ReturnIfError( err );											//	If we can't write to our id block, we are in real trouble

	//	Create our "Repair" BTree
	err = CreateAndOpenRepairBtree( GPtr, refNum, &newBTreeRefNum );
	ReturnIfError( err );

	//
	//-	First traverse the tree forwards inserting each record into the new BTree
	//

	err = GetBTreeRecord( refNum, selCode, &foundKey, &record, &recordSize, &hint );
	if ( err == noErr )
	{
		selCode = 1;																//	 Get next record from now on		
	
		do
		{
			if ( ++i > 10 ) { (void) CheckForStop( GPtr ); i = 0; }					//	Spin the cursor every 10 entries
	
			err = InsertBTreeRecord( newBTreeRefNum, &foundKey, &record, recordSize, &returnedHint );
			ExitOnError( err );
			
			err = GetBTreeRecord( refNum, selCode, &foundKey, &record, &recordSize, &hint );
		} while ( err == noErr );
	}
	
	//
	//- Now	traverse the tree backwards inserting each record into the new BTree
	//
	if ( GPtr->RepLevel >= repairLevelWillCauseDataLoss )
	{
		if ( err == btNotFound )
		{
			selCode = 0x7FFF;														//	 Start with last record			
		
			err = GetBTreeRecord( refNum, selCode, &foundKey, &record, &recordSize, &hint );
			ExitOnError( err );														//	If the last node is damaged, just bail
		
			selCode = -1;															//	Get previous record from now on		
		
			do
			{
				if ( ++i > 10 ) { (void) CheckForStop( GPtr ); i = 0; }				//	Spin the cursor every 10 entries
	
				err = InsertBTreeRecord( newBTreeRefNum, &foundKey, &record, recordSize, &returnedHint );
				
				if ( err == noErr )
					err = GetBTreeRecord( refNum, selCode, &foundKey, &record, &recordSize, &hint );
	
			} while ( err == noErr );
		}
		
		if ( err == btExists )		//	If the backward links overflow with what we have already inserted.
			err = noErr;
	
		//	clean up error here

	}

	if ( (err != noErr) && (err != btNotFound) )		//	Exit if we have an unexpected error
		ExitOnError( err );
	
	//
	//	Delete the corrupted BTree file
	//	Delete overflow extents, and bitmap bits
	//
	//	Delete old BTree extents from extents overflow file  ( We will be rebuilding the BitMap  GPtr->VIStat |= S_VBM )
	(void) ProcessFileExtents( GPtr, fcbPtr, 0, deleteExtents, (fcbPtr->fcbFileID == kHFSExtentsFileID), &hasOverflowExtents, &blocksUsed );
	(void) DeleteFilesOverflowExtents( GPtr, fcbPtr );
	if ( hasOverflowExtents == true )
	{
		//	Flush the extents BTree
		M_BTreeHeaderDirty( ((BTreeControlBlockPtr) GPtr->calculatedExtentsFCB->fcbBTCBPtr) );	
		(void) BTFlushPath( GPtr->calculatedExtentsFCB );						//	Flush the header out to disk.
	}
	
	//
	//	Set our globals to point to the new BTree file
	//
	GPtr->calculatedRepairBTCB->fileRefNum	= refNum;
	CopyMemory( GPtr->calculatedRepairBTCB, fcbPtr->fcbBtree, sizeof( BTreeControlBlock ) );
//	CopyMemory( GPtr->extendedRepairFCB, GetExtendedFCB(fcbPtr), sizeof( ExtendedFCB ) );

	//	Copy the FCB
//	CopyMemory( fcbPtr->fcbCName, repairFCB->fcbCName, sizeof(fcbPtr->fcbCName) );
	repairFCB->fcbFileID = fcbPtr->fcbFileID;
	repairFCB->fcbBtree = fcbPtr->fcbBtree;
	CopyMemory( repairFCB, fcbPtr, sizeof( SFCB ) );
	
	//	Write the new BTree file to disk.
	err = UpdateBTreeHeader( GPtr, refNum );	//	update catalog BTH
//	err = BTFlushPath( ResolveFCB(newBTreeRefNum));
	ExitOnError( err );

	//
	//	Update the VH MDB to point to our new BTree
	//
	MarkVCBDirty( GPtr->calculatedVCB );
	err = FlushAlternateVolumeControlBlock( GPtr->calculatedVCB, isHFSPlus );	//	Writes real & alt blocks
	ExitOnError( err );


ErrorExit:
	if ( err )
		(void) BTClosePath( repairFCB );

	return( err );
#else
	return (0);
#endif
}


static	OSErr	CreateAndOpenRepairBtree( SGlobPtr GPtr, SInt16 refNum, SInt16 *newBTreeRefNum )
{
	OSErr				err = 0;
#if 0
	UInt16				nodeSize;
	SInt32				clumpSize;
	UInt32				mapNodes;
	UInt32				actualSectorsAdded;
	UInt32				blocksUsed;
	Boolean				readFromDisk;
	Boolean				hasOverflowExtents;
	BTreeControlBlock	*corruptedBTCB;
	void*				buffer;
	SFCB				*corruptedFCB;
	SFCB				*fcb;
	UInt16				recordCount = 0;
	SVCB			*vcb = GPtr->calculatedVCB;
	BTreeControlBlock	*btcb = GPtr->calculatedRepairBTCB;	//	calculatedRepairBTCB
	HFSCatalogNodeID	fileID = vcb->vcbNextCatalogID + 1;		//ее Verify fileID is not in use


	//
	//	Clear all the Repair structures in case they are being reused
	//
	ClearMemory( (Ptr) GPtr->calculatedRepairFCB, sizeof(SFCB) );
//	ClearMemory( (Ptr) GPtr->extendedRepairFCB, sizeof(ExtendedFCB) );
	ClearMemory( (Ptr) GPtr->calculatedRepairBTCB, sizeof(BTreeControlBlock) );

	//ее	We don't know the state of our calculated btcb structures since the BTree was damaged,
	//ее	so we may want to read the btree header from disk.
	//	HeaderRec				*header;
	//	err = GetBTreeHeader( GPtr, kCalculatedExtentRefNum, &header );

	if ( refNum == kCalculatedExtentRefNum )
	{
		WriteMsg( GPtr, M_RebuildingExtentsBTree, kStatusMessage );
		corruptedBTCB	= GPtr->calculatedExtentsBTCB;
		corruptedFCB	= GPtr->calculatedExtentsFCB;
		clumpSize	= vcb->vcbExtentsFile->fcbClumpSize;
		GPtr->EBTStat	|= S_BTH;
		GPtr->TarID	= kHFSExtentsFileID;
	}
	else if ( refNum == kCalculatedCatalogRefNum )
	{
		WriteMsg( GPtr, M_RebuildingCatalogBTree, kStatusMessage );
		corruptedBTCB	= GPtr->calculatedCatalogBTCB;
		corruptedFCB	= GPtr->calculatedCatalogFCB;
		clumpSize	= vcb->vcbCatalogFile->fcbClumpSize;
		GPtr->CBTStat	|= S_BTH;
		GPtr->TarID	= kHFSCatalogFileID;
		recordCount	= 0;
	}
	else if ( refNum == kCalculatedAttributesRefNum )
	{
		WriteMsg( GPtr, M_RebuildingAttributesBTree, kStatusMessage );
		corruptedBTCB	= GPtr->calculatedAttributesBTCB;
		corruptedFCB	= GPtr->calculatedAttributesFCB;
		clumpSize	= vcb->vcbAttributesFile->fcbClumpSize;
		GPtr->TarID	= kHFSAttributesFileID;
	}
	else
	{
		return( notBTree );
	}

	GPtr->VIStat |= ( S_VBM + S_MDB );									//	Force bitmap and MDB/VH to be updated
	InvalidateCalculatedVolumeBitMap( GPtr );							//	Invalidate our BitMap

	//
	//	Create and open new BTree
	//
	
	//	Create Calculated Repair FCB
	fcb = GPtr->calculatedRepairFCB;
	SetupFCB( vcb, kCalculatedRepairRefNum, fileID, clumpSize );
	fcb->fcbBtree = nil;
	fcb->fcbLogicalSize = 0;
	fcb->fcbPhysicalSize = 0;


	//	Make sure the new BTree is the same size as the corrupted BTree
	err = ExtendFileC ( vcb, fcb, (corruptedFCB->fcbLogicalSize) >> kSectorShift, 0, &actualSectorsAdded );
	ReturnIfError( err );
	
	fcb->fcbLogicalSize = fcb->fcbPhysicalSize;		// new B-tree looks at fcbLogicalSize
	*newBTreeRefNum	= kCalculatedRepairRefNum;

	//	Verify that new BTree fits in our extent, (no overflow extents)
	err	= ProcessFileExtents( GPtr, fcb, 0, clearBlocks, (refNum == kCalculatedExtentRefNum), &hasOverflowExtents, &blocksUsed );
	if ( err || hasOverflowExtents )
	{
		if ( hasOverflowExtents )
		{
			err	= E_DiskFull;
			GPtr->TarBlock	= blocksUsed;
			RcdError( GPtr, err );
		}
		(void) ProcessFileExtents( GPtr, fcb, 0, deleteExtents, (refNum == kCalculatedExtentRefNum), &hasOverflowExtents, &blocksUsed );
		return( err );
	}

	err = ZeroFileBlocks( vcb, fcb, 0, (fcb->fcbLogicalSize) >> kSectorShift);


	//
	//	Initialize the b-tree.  Write out the header.
	//
	nodeSize	= corruptedBTCB->nodeSize;
	err = GetCacheBlock( kCalculatedRepairRefNum, 0, nodeSize, gbDefault, (void**)&buffer, &readFromDisk );
	ReturnIfError( err );
	
	InitBTreeHeader( fcb->fcbLogicalSize, fcb->fcbClumpSize, nodeSize, recordCount, corruptedBTCB->maxKeyLength, corruptedBTCB->attributes, &mapNodes, (void*)buffer );
	
	err = ReleaseCacheBlock( buffer, rbWriteMask );
	ReturnIfError( err );
		
	if ( mapNodes > 0 )										// do we have any map nodes?
	{
		err = CreateMapNodes( recordCount+1, mapNodes, nodeSize );				// write map nodes to disk
		ReturnIfError( err );
	}

	// Finally, prepare for using the B-tree		
	err = BTOpenPath(GPtr->calculatedRepairFCB,
			corruptedBTCB->keyCompareProc,
			GetBlockProc,
			ReleaseBlockProc,
			SetEndOfForkProc,
			SetBlockSizeProc);
	if ( err != noErr )
	{
		M_DebugStr("\pCould not Open B-tree");
		(void) BTClosePath( fcb );
		return( err );
	}
	
	//	Move the new btcb into our space, calculatedRepairBTCB
	CopyMemory( fcb->fcbBtree , btcb, sizeof(BTreeControlBlock) );
	fcb->fcbBtree	= (Ptr) btcb;
	DisposeMemory( fcb->fcbBtree );
#endif	
	return( err );
}



OSErr	ProcessFileExtents( SGlobPtr GPtr, SFCB *fcb, UInt8 forkType, UInt16 flags, Boolean isExtentsBTree, Boolean *hasOverflowExtents, UInt32 *blocksUsed  )
{
	OSErr				err					= noErr;
#if 0
	UInt32				extentBlockCount;
	UInt32				extentStartBlock;
	UInt32				hint;
	SInt16				i;
	HFSPlusExtentKey	key;
	HFSPlusExtentRecord	extents;
	Boolean				done				= false;
	SVCB			*vcb				= GPtr->calculatedVCB;
	UInt32				fileNumber			= fcb->fcbFileID;
	UInt32				blockCount			= 0;
	OSErr				err					= noErr;

	
	*hasOverflowExtents = false;
	extentBlockCount = 0;							//	default
	
	err = GetFCBExtentRecord( vcb, fcb, extents );			//	Gets extents in a HFSPlusExtentRecord

	while ( (done == false) && (err == noErr) )
	{
//		err = ChkExtRec( GPtr, extents );					//	checkout the extent record first
//		if ( err != noErr )		break;

		for ( i=0 ; i<GPtr->numExtents ; i++ )				//	now checkout the extents
		{
			extentBlockCount = extents[i].blockCount;
			extentStartBlock = extents[i].startBlock;
		
			if ( extentBlockCount == 0 )
				break;
			
	
			if ( flags == addBitmapBit )
			{
				err = AllocExt( GPtr, extentStartBlock, extentBlockCount );
				if ( err != noErr )
				{
					M_DebugStr("\p Problem Allocating Extents");
					break;
				}
			}
	
			blockCount += extentBlockCount;
		}
		
		
		if ( (err != noErr) || isExtentsBTree )				//	Extents file has no extents
			break;


		err = FindExtentRecord( vcb, forkType, fileNumber, blockCount, false, &key, extents, &hint );
		if ( err == noErr )
		{
			*hasOverflowExtents	= true;
		}
		else if ( err == btNotFound )
		{
			err		= noErr;								//	 no more extent records
			done	= true;
			break;
		}
		else if ( err != noErr )
		{
			err = IntError( GPtr, err );
			break;
		}
		
		if ( flags == deleteExtents )
		{
			err = DeleteExtentRecord( vcb, forkType, fileNumber, blockCount );
			if ( err != noErr ) break;
			
			vcb->vcbFreeBlocks += extentBlockCount;
			MarkVCBDirty( vcb );
		}
	}
	
	*blocksUsed = blockCount;
#endif	
	return( err );
}



static	OSErr	DeleteFilesOverflowExtents( SGlobPtr GPtr, SFCB *fcb  )
{
#if 0
	BTScanState			scanState;
	ExtentKey			*extentKeyPtr;
	ExtentRecord		*extentDataPtr;
	ExtentRecord		zeroExtents;
	UInt32				maxRecords;
	UInt32				recordSize;
	UInt32				hint;
	OSErr				err;
	ExtentRecord		extents;
	ExtentKey		foundExtentKey;
	UInt32			recordsFound = 0;
	SVCB			*vcb = GPtr->calculatedVCB;
	Boolean			isHFSPlus			= GPtr->isHFSPlus;

	ClearMemory( (Ptr)&zeroExtents, sizeof(ExtentRecord) );
	maxRecords = (fcb->fcbLogicalSize) / (isHFSPlus ? sizeof(HFSPlusExtentRecord) : sizeof(HFSExtentRecord));

	err = BTScanInitialize( GPtr->calculatedExtentsFCB, 0, 0, 0, gFSBufferPtr, gFSBufferSize, &scanState );
	if ( err != noErr )	return( badMDBErr );

	// visit all the leaf node data records in the extents B*-Tree
	while ( recordsFound < maxRecords )
	{
		err = BTScanNextRecord( &scanState, false, (void **) &extentKeyPtr, (void **) &extentDataPtr, &recordSize );

		if ( err != noErr )	break;

		++recordsFound;

		if ( isHFSPlus && (fcb->fcbFileID == extentKeyPtr->hfsPlus.fileID) )
		{
			err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, extentKeyPtr, kNoHint, &foundExtentKey, &extents, (UInt16*)&recordSize, &hint );
			err = ReplaceBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey, hint, &zeroExtents, recordSize, &hint );
//			err = DeleteExtentRecord( vcb, extentKeyPtr->hfsPlus.forkType, fcb->fcbFileID, extentKeyPtr->hfsPlus.startBlock );
		}
		else if ( !isHFSPlus && (fcb->fcbFileID == extentKeyPtr->hfs.fileID) )
		{
			err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, extentKeyPtr, kNoHint, &foundExtentKey, &extents, (UInt16*)&recordSize, &hint );
			err = ReplaceBTreeRecord( GPtr->calculatedExtentsFCB, &foundExtentKey, hint, &zeroExtents, recordSize, &hint );
		}
	}
	
	if ( err == btNotFound )
		err = noErr;
		
	return( err );
#else
	return (0);
#endif
}


static	OSErr	CreateMapNodes( UInt32 firstMapNode, UInt32	numberOfNewMapNodes, UInt16 nodeSize )
{
	void				*buffer;
	UInt32				mapRecordBytes;
	UInt32				i;
	UInt32				mapNodeNum		= firstMapNode;
	OSErr				err				= noErr;
	SFCB* fcb = ResolveFCB(kCalculatedRepairRefNum);
	BlockDescriptor  block;

	SetFileBlockSize(fcb, nodeSize);	

	for ( i = 0 ; i < numberOfNewMapNodes ; i++  )
	{
		err = GetFileBlock(ResolveFCB(kCalculatedRepairRefNum), mapNodeNum, kGetBlock, &block);
		ReturnIfError( err );
		buffer = (void *) block.buffer;
		ClearMemory( buffer, nodeSize );							// start with clean node

		((NodeDescPtr)buffer)->numRecords	= 1;
		((NodeDescPtr)buffer)->kind			= kBTMapNode;
		
		// set free space offset
//		*(UInt16 *)((Ptr)buffer + nodeSize - 4) = nodeSize - 6;
		mapRecordBytes = nodeSize - sizeof(BTNodeDescriptor) - 2*sizeof(SInt16) - 2;		// must belong word aligned (hence the extra -2)
	
		SetOffset( buffer, nodeSize, sizeof(BTNodeDescriptor), 1);							// set offset to map record (1st)
		SetOffset( buffer, nodeSize, sizeof(BTNodeDescriptor) + mapRecordBytes, 2);		// set offset to free space (2nd)

		if ( (i+1) < numberOfNewMapNodes )
			((BTNodeDescriptor*)buffer)->fLink = mapNodeNum+1;	// point to next map node
		else
			((BTNodeDescriptor*)buffer)->fLink = 0;					// this is the last map node
			
		err = ReleaseFileBlock (fcb, &block, kForceWriteBlock);
		ReturnIfError( err );	

		++mapNodeNum;
	}
	
	//ее	Mark off the new map nodes
	
	return( err );
}


/*------------------------------------------------------------------------------

Function:	cmpLongs

Function:	compares two longs.
			
Input:		*a:  pointer to first number
			*b:  pointer to second number

Output:		<0 if *a < *b
			0 if a == b
			>0 if a > b
------------------------------------------------------------------------------*/

int cmpLongs ( const void *a, const void *b )
{
	return( *(long*)a - *(long*)b );
}


//
//	Isolate and fix Overlapping Extents
//
static	OSErr	FixOverlappingExtents( SGlobPtr GPtr )
{
	OSErr			err = noErr;
#if 0
	UInt32			i;
	UInt32			numInitialExtents;
	ExtentInfo		*extentInfo;
	ExtentsTable	**extentsTableH		= GPtr->overlappedExtents;
	
	if ( extentsTableH == nil )
		return( -1 );
	
	numInitialExtents	= (**extentsTableH).count;
	InvalidateCalculatedVolumeBitMap( GPtr );
	err = UpdateVolumeBitMap( GPtr, true );							//	Update VolumeBitMap, while first allocating the known overlapped extents
	
	//	Preflight to make sure none of the HFS files are being overlapped
	for ( i = 0 ; i < (**extentsTableH).count; i++ )
	{
		if ( (**extentsTableH).extentInfo[i].fileNumber < kHFSFirstUserCatalogNodeID )
		{
			char fileNum[32];

			GPtr->TarBlock	= (**extentsTableH).extentInfo[i].fileNumber;
			
			sprintf(fileNum, "%ld", GPtr->TarBlock);
			PrintError(GPtr, E_InternalFileOverlap, 1, fileNum);
			return( E_InternalFileOverlap );
		}
	}
		
	//	We now have a complete list off all the overlapping extents
	//	Duplicate numInitialExtents extents, and change its extent info to point to the copy.

	for ( i = 0 ; (i < numInitialExtents) && (err == noErr) ; i++ )
	{
		extentInfo	= &((**extentsTableH).extentInfo[i]);
		
		err	= MoveExtent( GPtr, extentInfo );
	}
	
	InvalidateCalculatedVolumeBitMap( GPtr );					//	Invalidate our BitMap, since we modified it
	
	if ( err == noErr )
		err = CreateFileIdentifiers( GPtr );
#endif
	return( err );
}


//
//	Move the extent somewhere else
//
static	OSErr	MoveExtent( SGlobPtr GPtr, ExtentInfo *extentInfo )					//	Isolate and fix Overlapping Extents
{
	OSErr				err = 0;
#if 0
	Ptr					byteP;
	UInt16				bitPos;
	unsigned char		mask;
	SInt32				bufferNumber;
	UInt32				startBuffer;
	UInt32				startBlock;
	UInt16 				recordSize;
	UInt32				hint;
	UInt32				i;
	UInt32				bitsInBuffer;
	UInt32				blockCount		= 0;
	VolumeBitMapHeader	*volumeBitMap	= GPtr->volumeBitMapPtr;
	Boolean				isHFSPlus		= GPtr->isHFSPlus;

	//	Set up

	GPtr->TarID = VBM_FNum;	//	target file = volume bit map
	startBlock = startBuffer = 0;
	
	bitsInBuffer	= volumeBitMap->bufferSize * 8;
	
	for ( bufferNumber=0 ; bufferNumber < volumeBitMap->numberOfBuffers ; bufferNumber++ )
	{
		BitMapRec	*bufferInfo	= &(volumeBitMap->bitMapRecord[bufferNumber]);
		
		//	This code must cycle through the BitMap buffers and if they have already been processed:
		
		//	if this buffer is completely empty
		if ( (bufferInfo->processed) && (bufferInfo->count == 0) )
		{
			ClearMemory ( volumeBitMap->buffer, volumeBitMap->bufferSize );		//	start with an empty bitmap buffer
		}
		//	if this buffer is completely full
		else if ( (bufferInfo->processed) && (bufferInfo->count ==  volumeBitMap->bufferSize * 8) )
		{
			memset( volumeBitMap->buffer, 0xFF, volumeBitMap->bufferSize );
		}
		//	or else we have to recreate the bitmap buffer
		else
		{
			err = CreateVolumeBitMapBuffer( GPtr, bufferNumber, false );		//	no need to pre allocate overlapped extents
			ReturnIfError( err );
		}
		
		//
		//	Now that the bit map buffer is created, search it for contiguous free bits
		//
		
		if ( bufferNumber == volumeBitMap->numberOfBuffers-1 )
			bitsInBuffer	= (volumeBitMap->bitMapSizeInBytes - (volumeBitMap->bufferSize * bufferNumber)) * 8;

		byteP	= volumeBitMap->buffer;											//	Initialize byteP to point to start of buffer
		
		for ( i = 0; i < bitsInBuffer; i++ )
		{
			bitPos	= ( i % 8 );
			mask	= ( 0x80 >> bitPos );
			
			
			if ( (*byteP & mask) != 0 )
			{
				startBlock	= -1;
			}
			else
			{
				if ( startBlock == -1 )
				{
					startBuffer	= bufferNumber;
					startBlock	= i;
					blockCount	= 1;
				}
				else
				{
					blockCount++;
				}
				
				if ( blockCount >= extentInfo->blockCount )
					break;
			}
	

			if ( bitPos == 7 )													//	Advance to the next byte
				byteP++;
		}
		
		if ( blockCount >= extentInfo->blockCount )
			break;
	}
		
	
	//
	//	If we found enough free space in our bitmap
	//	-	Search for the original extent
	//	-	Replace the extents start block with our new start block
	//	-	Mark the buffers as   bufferInfo->processed	= false;
	//
	if ( blockCount < extentInfo->blockCount )
	{
		err				= E_DiskFull;
		GPtr->TarBlock	= kHFSAllocationFileID;
		RcdError( GPtr, err );
		return( err );
	}
	else
	{
		ExtentKey			*extentKeyP;
		ExtentRecord		*extentRecordP;

		UInt32				newStartBlock	= startBlock + ( startBuffer * volumeBitMap->bufferSize * 8 );
		
		//	Do the brute force scan for our catalog record.
		err = ScanForExtentRecord( GPtr, extentInfo->startBlock, extentInfo->fileNumber, extentInfo->forkType, &extentKeyP, &extentRecordP, &recordSize );

		//	If the questioned extent is represented in the extents btree
		if ( err == noErr )
		{
			(void) ReplaceStartBlock( GPtr, extentRecordP, extentInfo->startBlock, newStartBlock );
			
			err = ReplaceBTreeRecord( GPtr->calculatedExtentsFCB, extentKeyP, hint, extentRecordP, recordSize, &hint );
			ReturnIfError( err );
		}
		else		//	The questioned extent must be in the catalog btree
		{
			CatalogRecord		catalogRecord;
			CatalogKey			catalogKey;
			CatalogKey			foundCatalogKey;


			if ( err != btNotFound )
				return( err );
				
			//	The extent must be represented in the catalog file.
			
			BuildCatalogKey( extentInfo->fileNumber, (const CatalogName*) nil, isHFSPlus, &catalogKey );
			err = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &catalogKey, kNoHint, &foundCatalogKey, &catalogRecord, &recordSize, &hint );
			
			//	If this file does have a thread
			if ( err == noErr )
			{
				extentInfo->hasThread	= true;
				
				if ( (catalogRecord.recordType != kHFSFileThreadRecord) && (catalogRecord.recordType != kHFSPlusFileThreadRecord) )	//	Quit if not a file thread
					return ( IntError( GPtr, R_IntErr ) );
				
				if ( isHFSPlus == true )
				{
					BuildCatalogKey( catalogRecord.hfsPlusThread.parentID, (const CatalogName*) &(catalogRecord.hfsPlusThread.nodeName), isHFSPlus, &catalogKey );
					err = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &catalogKey, kNoHint, &foundCatalogKey, &catalogRecord, &recordSize, &hint );
					ReturnIfError( err );
					
					if ( extentInfo->forkType == 0x00 )
						(void) ReplaceStartBlock( GPtr, (ExtentRecord *) &(catalogRecord.hfsPlusFile.dataFork.extents), extentInfo->startBlock, newStartBlock );
					else
						(void) ReplaceStartBlock( GPtr, (ExtentRecord *) &(catalogRecord.hfsPlusFile.resourceFork.extents), extentInfo->startBlock, newStartBlock );
				}
				else
				{
					BuildCatalogKey( catalogRecord.hfsThread.parentID, (const CatalogName*) &(catalogRecord.hfsThread.nodeName), isHFSPlus, &catalogKey );
					err = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &catalogKey, kNoHint, &foundCatalogKey, &catalogRecord, &recordSize, &hint );
					ReturnIfError( err );
					
					if ( extentInfo->forkType == 0x00 )
						(void) ReplaceStartBlock( GPtr, (ExtentRecord *) &(catalogRecord.hfsFile.dataExtents), extentInfo->startBlock, newStartBlock );
					else
						(void) ReplaceStartBlock( GPtr, (ExtentRecord *) &(catalogRecord.hfsFile.rsrcExtents), extentInfo->startBlock, newStartBlock );
				}

				err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundCatalogKey, hint, &catalogRecord, recordSize, &hint );
				ReturnIfError( err );
			}
			else		//	The questioned extent has no thread record
			{
				CatalogRecord		*foundCatalogRecordP;
				CatalogKey			*foundCatalogKeyP;
				
				if ( isHFSPlus == true )
					return ( IntError( GPtr, R_IntErr ) );			//	All HFS+ files need threads
					
				//	Do the brute force scan for our catalog record.
				err = ScanForCatalogRecord( GPtr, extentInfo->fileNumber, kHFSFileRecord, &foundCatalogKeyP, &foundCatalogRecordP, &recordSize );
				ReturnIfError( err );
				
				if ( extentInfo->forkType == 0x00 )
					(void) ReplaceStartBlock( GPtr, (ExtentRecord *) &(foundCatalogRecordP->hfsFile.dataExtents), extentInfo->startBlock, newStartBlock );
				else
					(void) ReplaceStartBlock( GPtr, (ExtentRecord *) &(foundCatalogRecordP->hfsFile.rsrcExtents), extentInfo->startBlock, newStartBlock );

				err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, foundCatalogKeyP, kNoHint, foundCatalogRecordP, recordSize, &hint );
				ReturnIfError( err );
			}
			
			err = CopyDiskBlocks( GPtr, extentInfo->startBlock, extentInfo->blockCount, newStartBlock );
			ReturnIfError( err );
		}
	}
#endif		
	return( err );
}



static	Boolean	ReplaceStartBlock( SGlobPtr GPtr, ExtentRecord *extentRecord, UInt32 originalStartBlock, UInt32 newStartBlock )
{
	SInt16			i;
	Boolean			isHFSPlus		= GPtr->isHFSPlus;

	for ( i = 0 ; i < GPtr->numExtents ; i++ )
	{
		if ( isHFSPlus == true )
		{
			if ( extentRecord->hfsPlus[i].startBlock	== originalStartBlock )
			{
				extentRecord->hfsPlus[i].startBlock	= newStartBlock;
				return( true );
			}
		}
		else
		{
			if ( extentRecord->hfs[i].startBlock	== originalStartBlock )
			{
				extentRecord->hfs[i].startBlock	= newStartBlock;
				return( true );
			}
		}
	}
	return( false );
}


#if 0
OSErr	CopyDiskBlocks( SGlobPtr GPtr, UInt32 startAllocationBlock, UInt32 blockCount, UInt32 newStartAllocationBlock )
{
	OSErr		err = noErr;
	UInt32		diskBlock;
	SInt32		i;
	UInt32		sectorsInBuffer;
	UInt32		numberOfBuffersToWrite;
	SVCB		*vcb;
	UInt32		sectorsPerBlock;
	UInt32	ioReqCount;
	UInt32	actBytes;
	int	drive;
	
	gFSBufferInUse = true;
	ClearMemory( gFSBufferPtr, gFSBufferSize );

	vcb = GPtr->calculatedVCB;
	sectorsPerBlock = vcb->vcbBlockSize / Blk_Size;
	drive = vcb->vcbDriveNumber;
	ioReqCount = gFSBufferSize;
	sectorsInBuffer = gFSBufferSize / Blk_Size;
	numberOfBuffersToWrite	= ( blockCount * sectorsPerBlock + sectorsInBuffer -1 ) / ( sectorsInBuffer );


	for ( i = 0 ; i < numberOfBuffersToWrite ; i++ )
	{
		if ( i == numberOfBuffersToWrite - 1 )					//	Last buffer
			ioReqCount = ( (blockCount * sectorsPerBlock) % sectorsInBuffer) * Blk_Size;
		
		/* Read up to one buffer full */
		diskBlock = sectorsPerBlock * startAllocationBlock + vcb->vcbAlBlSt + ( i * sectorsInBuffer );

		err = DeviceRead(vcb->vcbDriverReadRef, drive, gFSBufferPtr, diskBlock << Log2BlkLo, ioReqCount, &actBytes);
		ReturnIfError( err );
		
		/* Write up to one buffer full */
		diskBlock = sectorsPerBlock * newStartAllocationBlock + vcb->vcbAlBlSt + ( i * sectorsInBuffer );

		err = DeviceWrite(vcb->vcbDriverWriteRef, drive, gFSBufferPtr, diskBlock << Log2BlkLo, ioReqCount, &actBytes);
		ReturnIfError( err );
	}
		
	gFSBufferInUse = false;	// Mark the buffer free	

	return( err );
}
#endif


static OSErr
ScanForCatalogRecord( SGlobPtr GPtr, HFSCatalogNodeID fileID, SInt16 recordType,
			CatalogKey **foundCatalogKeyH, CatalogRecord **foundCatalogRecordH, UInt16 *recordSize )
{
	OSErr			err;
	UInt32			maxRecords;
	UInt32			recordsFound;
	SFCB *			fcb;
	CatalogRecord		catalogRecord;
	UInt16			operation;
	BTreeIterator		btreeIterator;
	FSBufferDescriptor	btRecord;
	UInt16			btRecordSize;


	if ( GPtr->isHFSPlus == true )			//	This routine is only used on HFS volumes for now.
		return( -1 );

	fcb = GPtr->calculatedCatalogFCB;
	operation = kBTreeFirstRecord;
	recordsFound = 0;
	maxRecords = (fcb->fcbLogicalSize) / sizeof(HFSCatalogFolder);

	btRecord.bufferAddress = &catalogRecord;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(catalogRecord);

	while ( recordsFound < maxRecords )
	{
		err = BTIterateRecord(fcb, operation, &btreeIterator, &btRecord, &btRecordSize);
		ReturnIfError( err );

		++recordsFound;
		if (operation == kBTreeFirstRecord)
			operation = kBTreeNextRecord;

		if ( (catalogRecord.recordType == kHFSFileRecord) && (catalogRecord.hfsFile.fileID == fileID) )
		{
			*recordSize = btRecordSize;
			return( noErr );
		}
	}
	
	return( btNotFound );
}


static OSErr ScanForExtentRecord( SGlobPtr GPtr, UInt32 startBlock, HFSCatalogNodeID fileID, UInt8 forkType,
	ExtentKey **foundExtentKeyH, ExtentRecord **foundExtentRecordH, UInt16 *recordSize )
{
	OSErr			err;
	UInt32			maxRecords;
	UInt32			i;
	UInt32			recordsFound = 0;
	Boolean			isHFSPlus = GPtr->isHFSPlus;
	SFCB *fcb = GPtr->calculatedExtentsFCB;
	HFSPlusExtentKey	*extentKeyPtr;
	HFSPlusExtentRecord	extentRecord;
	UInt16			operation;
	BTreeIterator		btreeIterator;
	FSBufferDescriptor	btRecord;
	UInt16			btRecordSize;
	
	operation = kBTreeFirstRecord;
	maxRecords = (fcb->fcbLogicalSize) / (isHFSPlus ? sizeof(HFSPlusExtentRecord) : sizeof(HFSExtentRecord));

	extentKeyPtr = (HFSPlusExtentKey*) &btreeIterator.key;

	btRecord.bufferAddress = &extentRecord;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(extentRecord);

	while ( recordsFound < maxRecords )
	{
		err = BTIterateRecord(fcb, operation, &btreeIterator, &btRecord, &btRecordSize);
		ReturnIfError( err );

		++recordsFound;
		if (operation == kBTreeFirstRecord)
			operation = kBTreeNextRecord;

		if ( isHFSPlus ) 
		{
			if ( (extentKeyPtr->fileID != fileID) ||
			     (extentKeyPtr->forkType != forkType) )
				continue;
		}
		else /* standard HFS */
		{
			if ( (((HFSExtentKey*) extentKeyPtr)->fileID != fileID) ||
			     (((HFSExtentKey*) extentKeyPtr)->forkType != forkType) )
				continue;

			/* convert in place to an hfs plus extent record */
			ConvertToHFSPlusExtent(*(HFSExtentRecord*) &extentRecord, extentRecord);
		}

		for ( i=0 ; i < GPtr->numExtents ; i++ )
		{
			if ( extentRecord[i].startBlock == startBlock )
			{
				*recordSize = btRecordSize;
				return( noErr );
			}
		}
	}
	
	return( btNotFound );
}



static	OSErr	CreateFileIdentifiers( SGlobPtr GPtr )
{
	UInt32				i;
	ExtentInfo			*extentInfo;
	FileIdentifier		fileIdentifier;
	CatalogRecord		*foundCatalogRecordP;
	CatalogKey			*foundCatalogKeyP;
	UInt16 				recordSize;
	OSErr				err;
	Boolean				isHFSPlus			= GPtr->isHFSPlus;
	ExtentsTable		**extentsTableH		= GPtr->overlappedExtents;
	
	ClearMemory( &fileIdentifier, sizeof(FileIdentifier) );

	for ( i = 0 ; i < (**extentsTableH).count; i++ )
	{
		extentInfo	= &((**extentsTableH).extentInfo[i]);
		
		if ( IdentifierExists( GPtr->fileIdentifierTable, extentInfo->fileNumber ) == false )
		{
			if ( isHFSPlus || extentInfo->hasThread )
			{
					fileIdentifier.hasThread	= true;
			}
			else	//	Files on HFS volumes which may not have threads
			{
				err = ScanForCatalogRecord( GPtr, extentInfo->fileNumber, kHFSFileRecord, &foundCatalogKeyP, &foundCatalogRecordP, &recordSize );
				ReturnIfError( err );
				
				fileIdentifier.hasThread	= false;
				fileIdentifier.parID		= foundCatalogKeyP->hfs.parentID;
				CopyMemory( foundCatalogKeyP->hfs.nodeName, fileIdentifier.name, foundCatalogKeyP->hfs.nodeName[0]+1 );

			}
			
			fileIdentifier.fileID	= extentInfo->fileNumber;
			InsertIdentifier( GPtr, &fileIdentifier );
		}
	}
	
	return( noErr );
}


static	void	InsertIdentifier( SGlobPtr GPtr, FileIdentifier *fileIdentifier )
{
	UInt32					newHandleSize;
	FileIdentifierTable		**fileIdentifierTable;

	if ( GPtr->fileIdentifierTable == nil )
	{
		GPtr->fileIdentifierTable	= (FileIdentifierTable **) NewHandleClear( sizeof(FileIdentifierTable) );
		fileIdentifierTable			= GPtr->fileIdentifierTable;
	}
	else
	{
		//	Grow the FileIdentifierTable for a new entry.
		fileIdentifierTable	= GPtr->fileIdentifierTable;
		newHandleSize		= ( sizeof(FileIdentifier) ) + ( GetHandleSize( (Handle)fileIdentifierTable ) );
		SetHandleSize( (Handle)fileIdentifierTable, newHandleSize );
	}

	CopyMemory( fileIdentifier, &((**fileIdentifierTable).fileIdentifier[(**fileIdentifierTable).count]), sizeof(FileIdentifier) );

	//	Update the extent table count
	(**fileIdentifierTable).count++;
}


static	Boolean	IdentifierExists( FileIdentifierTable **fileIdentifierTable, HFSCatalogNodeID fileID )
{
	UInt32		i;
	
	if ( fileIdentifierTable == nil )
		return( false );
	
	for ( i = 0 ; i < (**fileIdentifierTable).count ; i++ )
	{
		if ( (**fileIdentifierTable).fileIdentifier[i].fileID == fileID )
			return( true );
	}
	
	return( false );
}

//	2210409, in System 8.1, moving file or folder would cause HFS+ thread records to be
//	520 bytes in size.  We only shrink the threads if other repairs are needed.
static	OSErr	FixBloatedThreadRecords( SGlob *GPtr )
{
	CatalogRecord		record;
	CatalogKey			foundKey;
	UInt32				hint;
	UInt16 				recordSize;
	SInt16				i = 0;
	OSErr				err;
	SInt16				selCode				= 0x8001;										//	 Start with 1st record

	err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	ReturnIfError( err );

	selCode = 1;																//	 Get next record from now on		

	do
	{
		if ( ++i > 10 ) { (void) CheckForStop( GPtr ); i = 0; }					//	Spin the cursor every 10 entries
		
		if (  (recordSize == sizeof(HFSPlusCatalogThread)) && ((record.recordType == kHFSPlusFolderThreadRecord) || (record.recordType == kHFSPlusFileThreadRecord)) )
		{
			// HFS Plus has varaible sized threads so adjust to actual length
			recordSize -= ( sizeof(record.hfsPlusThread.nodeName.unicode) - (record.hfsPlusThread.nodeName.length * sizeof(UniChar)) );

			err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &foundKey, hint, &record, recordSize, &hint );
			ReturnIfError( err );
		}

		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
	} while ( err == noErr );

	if ( err == btNotFound )
		err = noErr;
		
	return( err );
}


static OSErr
FixMissingThreadRecords( SGlob *GPtr )
{
	struct MissingThread *mtp;
	FSBufferDescriptor    btRecord;
	BTreeIterator         iterator;
	OSStatus              result;
	UInt16                dataSize;

	for (mtp = GPtr->missingThreadList; mtp != NULL; mtp = mtp->link) {
		if (mtp->threadID == 0 || mtp->thread.parentID == 0)
			continue;

		dataSize = 10 + (mtp->thread.nodeName.length * 2);
		btRecord.bufferAddress = (void *)&mtp->thread;
		btRecord.itemSize = dataSize;
		btRecord.itemCount = 1;
		iterator.hint.nodeNum = 0;
		BuildCatalogKey(mtp->threadID, NULL, true, (CatalogKey*)&iterator.key);

		result = BTInsertRecord(GPtr->calculatedCatalogFCB, &iterator, &btRecord, dataSize);
		if (result)
			return (IntError(GPtr, R_IntErr));
		mtp->threadID = 0;
	}

	return (0);
}

