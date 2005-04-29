/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
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

	Copyright:	© 1986, 1990, 1992-1999 by Apple Computer, Inc., all rights reserved.

*/

#include "Scavenger.h"
#include <unistd.h>
#include <sys/stat.h>

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
static 	OSErr   GetCatalogRecord(SGlobPtr GPtr, UInt32 fileID, Boolean isHFSPlus, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize); 
static 	OSErr   RepairAttributesCheckABT(SGlobPtr GPtr, Boolean isHFSPlus);
static 	OSErr   RepairAttributesCheckCBT(SGlobPtr GPtr, Boolean isHFSPlus);
static	OSErr	RepairAttributes( SGlobPtr GPtr );
static	OSErr	FixFinderFlags( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixLinkCount( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixBSDInfo( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	DeleteUnlinkedFile( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixOrphanedExtent( SGlobPtr GPtr );
static 	OSErr	FixFileSize(SGlobPtr GPtr, RepairOrderPtr p);
static  OSErr 	VolumeObjectFixVHBorMDB( Boolean * fixedIt );
static 	OSErr 	VolumeObjectRestoreWrapper( void );

extern	OSErr	FindExtentRecord( const SVCB *vcb, UInt8 forkType, UInt32 fileID, UInt32 startBlock, Boolean allowPrevious, HFSPlusExtentKey *foundKey, HFSPlusExtentRecord foundData, UInt32 *foundHint);
extern	OSErr	DeleteExtentRecord( const SVCB *vcb, UInt8 forkType, UInt32 fileID, UInt32 startBlock );
OSErr	GetFCBExtentRecord( const SVCB *vcb, const SFCB *fcb, HFSPlusExtentRecord extents );
static	OSErr	FixOverlappingExtents( SGlobPtr GPtr );
OSErr	CopyDiskBlocks( SGlobPtr GPtr, UInt32 startAllocationBlock, UInt32 blockCount, UInt32 newStartAllocationBlock );
static	OSErr	FixBloatedThreadRecords( SGlob *GPtr );
static	OSErr	FixMissingThreadRecords( SGlob *GPtr );
static	OSErr	FixEmbededVolDescription( SGlobPtr GPtr, RepairOrderPtr p );
static	OSErr	FixWrapperExtents( SGlobPtr GPtr, RepairOrderPtr p );
static  OSErr	FixIllegalNames( SGlobPtr GPtr, RepairOrderPtr roPtr );
static HFSCatalogNodeID GetObjectID( CatalogRecord * theRecPtr );
static UInt32	CreateLostAndFoundDir( SGlob *GPtr );
static int		BuildThreadRec( CatalogKey * theKeyPtr, CatalogRecord * theRecPtr, 
								Boolean isHFSPlus, Boolean isDirectory );
static int		BuildFolderRec( u_int16_t theMode, UInt32 theObjID, Boolean isHFSPlus, 
								CatalogRecord * theRecPtr );
static OSErr	FixMissingDirectory( SGlob *GPtr, UInt32 theObjID, UInt32 theParID );


OSErr	RepairVolume( SGlobPtr GPtr )
{
	OSErr			err;
	
	SetDFAStage( kAboutToRepairStage );											//	Notify callers repair is starting...
 	err = CheckForStop( GPtr ); ReturnIfError( err );							//	Permit the user to interrupt
	
	//
	//	Do the repair
	//
	SetDFAStage( kRepairStage );									//	Stops GNE from being called, and changes behavior of MountCheck

	err = MRepair( GPtr );
	
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
	SVCB			*calculatedVCB	= GPtr->calculatedVCB;
	Boolean			isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );

	if ( GPtr->CBTStat & S_RebuildBTree )
	{
		/* we currently only support rebuilding the catalog B-Tree file.  */
		/* once we do the rebuild we will force another verify since the */
		/* first verify was aborted when we determined a rebuild was necessary */
		err = RebuildCatalogBTree( GPtr );
		return( err );
	}
 
	//  Handle repair orders.  Note that these must be done *BEFORE* the MDB is updated.
	err = DoMinorOrders( GPtr );
	ReturnIfError( err );
  	err = CheckForStop( GPtr ); ReturnIfError( err );

	/* Clear Catalog status for things repaired by DoMinorOrders */
	GPtr->CatStat &= ~(S_FileAllocation | S_Permissions | S_UnlinkedFile | S_LinkCount | S_IllName);

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
		GPtr->CBTStat |= S_BTH;  									// leaf record count may change - 2913311
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

	// Check consistency of attribute btree and corresponding bits in
	// catalog btree 
	if ( (GPtr->ABTStat & S_AttributeCount) || 
	     (GPtr->ABTStat & S_SecurityCount)) 
	{
		err = RepairAttributes( GPtr );
		ReturnIfError( err );
	}
	
	// Update the attribute BTree header and bit map 
	if ( (GPtr->ABTStat & S_BTH) )
	{
		err = UpdateBTreeHeader( GPtr->calculatedAttributesFCB );	//	update attribute BTH
		ReturnIfError( err );
	}

	if ( GPtr->ABTStat & S_BTM )
	{
		err = UpdBTM( GPtr, kCalculatedAttributesRefNum );		//	update attribute BTM
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
	//	Fix missing Primary or Alternate VHB or MDB
	//

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->VIStat & S_MDB) != 0 )		//	fix MDB / VolumeHeader
	{
		Boolean		fixedIt = false;
		err = VolumeObjectFixVHBorMDB( &fixedIt );
		ReturnIfError( err );
		// don't call FlushAlternateVolumeControlBlock if we fixed it since that would 
		// mean our calculated VCB has not been completely set up.
		if ( fixedIt ) {
			GPtr->VIStat &= ~S_MDB; 
			MarkVCBClean( calculatedVCB );
		}
	}

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->VIStat & S_WMDB) != 0  )		//	fix wrapper MDB
	{
		err = VolumeObjectRestoreWrapper();
		ReturnIfError( err );
	}

	//
	//	Update the MDB / VolumeHeader
	//
	// Note, moved MDB / VolumeHeader update to end 
	// after all other repairs have been completed.
	//

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
				
	if ( (GPtr->VIStat & S_MDB) != 0 || IsVCBDirty(calculatedVCB) ) //	update MDB / VolumeHeader
	{
		MarkVCBDirty(calculatedVCB);								// make sure its dirty
		calculatedVCB->vcbAttributes |= kHFSVolumeUnmountedMask;
		err = FlushAlternateVolumeControlBlock( calculatedVCB, isHFSPlus );	//	Writes real & alt blocks
		ReturnIfError( err );
	}

 	err = CheckForStop( GPtr ); ReturnIfError( err );				//	Permit the user to interrupt
 
 	// if we had minor repairs that failed we still want to fix as much as possible
 	// so we wait until now to indicate the volume still has problems
 	if ( GPtr->minorRepairErrors )
 		err = R_RFail;
	
	return( err );													//	all done
}



//
//	Internal Routines
//

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	VolumeObjectFixVHBorMDB
//
//	Function:	When the primary or alternate Volume Header Block or Master 
//				Directory Block is damaged or missing use the undamaged one to 
//				restore the other.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static OSErr VolumeObjectFixVHBorMDB( Boolean* fixedItPtr )
{
	OSErr				err;
	OSErr				err2;
	VolumeObjectPtr		myVOPtr;
	BlockDescriptor  	myPrimary;
	BlockDescriptor  	myAlternate;

	myVOPtr = GetVolumeObjectPtr( );
	myPrimary.buffer = NULL;
	myAlternate.buffer = NULL;
	err = noErr;
	
	// bail if both are OK
	if ( VolumeObjectIsHFS() ) {
		if ( (myVOPtr->flags & kVO_PriMDBOK) != 0 &&
			 (myVOPtr->flags & kVO_AltMDBOK) != 0 )
			goto ExitThisRoutine;
	}
	else {
		if ( (myVOPtr->flags & kVO_PriVHBOK) != 0 &&
			 (myVOPtr->flags & kVO_AltVHBOK) != 0 )
			goto ExitThisRoutine;
	}
			
	// it's OK if one of the primary or alternate is invalid
	err = GetVolumeObjectPrimaryBlock( &myPrimary );
	if ( !(err == noErr || err == badMDBErr || err == noMacDskErr) )
		goto ExitThisRoutine;

	// invalidate if we have not marked the primary as OK
	if ( VolumeObjectIsHFS( ) ) {
		if ( (myVOPtr->flags & kVO_PriMDBOK) == 0 )
			err = badMDBErr;
	}
	else if ( (myVOPtr->flags & kVO_PriVHBOK) == 0 ) {
		err = badMDBErr;
	}

	err2 = GetVolumeObjectAlternateBlock( &myAlternate );
	if ( !(err2 == noErr || err2 == badMDBErr || err2 == noMacDskErr) )
		goto ExitThisRoutine;

	// invalidate if we have not marked the alternate as OK
	if ( VolumeObjectIsHFS( ) ) {
		if ( (myVOPtr->flags & kVO_AltMDBOK) == 0 )
			err2 = badMDBErr;
	}
	else if ( (myVOPtr->flags & kVO_AltVHBOK) == 0 ) {
		err2 = badMDBErr;
	}
		
	// primary is OK so use it to restore alternate
	if ( err == noErr ) {
		CopyMemory( myPrimary.buffer, myAlternate.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kForceWriteBlock );		
		myAlternate.buffer = NULL;
		*fixedItPtr = true;
		if ( VolumeObjectIsHFS( ) )
			myVOPtr->flags |= kVO_AltMDBOK;
		else
			myVOPtr->flags |= kVO_AltVHBOK;
	}
	// alternate is OK so use it to restore the primary
	else if ( err2 == noErr ) {
		CopyMemory( myAlternate.buffer, myPrimary.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kForceWriteBlock );		
		myPrimary.buffer = NULL;
		*fixedItPtr = true;
		if ( VolumeObjectIsHFS( ) )
			myVOPtr->flags |= kVO_PriMDBOK;
		else
			myVOPtr->flags |= kVO_PriVHBOK;
		err = noErr;
	}
	else
		err = noMacDskErr;

ExitThisRoutine:
	if ( myPrimary.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kReleaseBlock );
	if ( myAlternate.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kReleaseBlock );

	return( err );
	
} /* VolumeObjectFixVHBorMDB */
		

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	VolumeObjectRestoreWrapper
//
//	Function:	When the primary or alternate Master Directory Block is damaged 
//				or missing use the undamaged one to restore the other.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static OSErr VolumeObjectRestoreWrapper( void )
{
	OSErr				err;
	OSErr				err2;
	VolumeObjectPtr		myVOPtr;
	BlockDescriptor  	myPrimary;
	BlockDescriptor  	myAlternate;

	myVOPtr = GetVolumeObjectPtr( );
	myPrimary.buffer = NULL;
	myAlternate.buffer = NULL;
				
	// it's OK if one of the MDB is invalid
	err = GetVolumeObjectPrimaryMDB( &myPrimary );
	if ( !(err == noErr || err == badMDBErr || err == noMacDskErr) )
		goto ExitThisRoutine;
	err2 = GetVolumeObjectAlternateMDB( &myAlternate );
	if ( !(err2 == noErr || err2 == badMDBErr || err2 == noMacDskErr) )
		goto ExitThisRoutine;

	// primary is OK so use it to restore alternate
	if ( err == noErr && (myVOPtr->flags & kVO_PriMDBOK) != 0 ) {
		CopyMemory( myPrimary.buffer, myAlternate.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kForceWriteBlock );		
		myAlternate.buffer = NULL;
		myVOPtr->flags |= kVO_AltMDBOK;
	}
	// alternate is OK so use it to restore the primary
	else if ( err2 == noErr && (myVOPtr->flags & kVO_AltMDBOK) != 0 ) {
		CopyMemory( myAlternate.buffer, myPrimary.buffer, Blk_Size );
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kForceWriteBlock );		
		myPrimary.buffer = NULL;
		myVOPtr->flags |= kVO_PriMDBOK;
		err = noErr;
	}
	else
		err = noMacDskErr;

ExitThisRoutine:
	if ( myPrimary.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myPrimary, kReleaseBlock );
	if ( myAlternate.buffer != NULL )
		(void) ReleaseVolumeBlock( myVOPtr->vcbPtr, &myAlternate, kReleaseBlock );

	return( err );
	
} /* VolumeObjectRestoreWrapper */
		

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
/*
 * TBD - we'll need to repair an invlid keyCompareType field.
 */
#if 0
	if (-->TBD<--)
		header.keyCompareType = kHFSBinaryCompare;
#endif
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
	SInt32				mapSize;
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

			//¥¥	E_NoFile case is never hit since VLockedChk() registers the error, 
			//¥¥	and returns the error causing the verification to quit.
			case E_EntryNotFound:
				GPtr->EBTStat |= S_OrphanedExtent;
				break;

			//¥¥	Same with E_NoDir
			case E_NoDir:									//	missing directory record
				err = FixDirThread( GPtr, p->parid );		//	fix the directory thread record
				break;
			
			case E_InvalidMDBdrAlBlSt:
				err = FixEmbededVolDescription( GPtr, p );
				break;
			
			case E_InvalidWrapperExtents:
				err = FixWrapperExtents(GPtr, p);
				break;

            case E_IllegalName:
                err = FixIllegalNames( GPtr, p );
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
	Boolean				isHFSPlus;
	ExtentRecord		zeroExtents;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	
	BuildCatalogKey( fid, (const CatalogName*) nil, isHFSPlus, &key );
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
	Boolean				isHFSPlus;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( kCalculatedCatalogRefNum );

	isHFSPlus = VolumeObjectIsHFSPlus( );

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
		return ( IntError( GPtr, R_IntErr ) ); // ¥¥  Try fixing by creating a new directory record?
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
	Boolean				isHFSPlus;
	UInt32				hint;						//	as returned by CBTSearch
	UInt16				recSize;
	CatalogRecord		record;
	CatalogKey			foundKey;
	CatalogKey			key;
	SVCB				*calculatedVCB = GPtr->calculatedVCB;

	isHFSPlus = VolumeObjectIsHFSPlus( );

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
			BuildCatalogKey( p->parid, (CatalogName *)&p->name, isHFSPlus, &key );
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
	Boolean				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );

	BuildCatalogKey( p->parid, (CatalogName *)&p->name, isHFSPlus, &key );

	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recSize, &hint );
	if ( result )
		return ( IntError( GPtr, result ) );

	if ( record.recordType == kHFSPlusFolderRecord )
	{
		HFSPlusCatalogFolder	*largeCatalogFolderP	= (HFSPlusCatalogFolder *) &record;	
		if ( (UInt16) p->incorrect != SWAP_BE16(largeCatalogFolderP->userInfo.frFlags) )
		{
			//	Another repar order may have affected the flags
			if ( p->correct < p->incorrect )
				largeCatalogFolderP->userInfo.frFlags &= SWAP_BE16(~((UInt16)p->maskBit));
			else
				largeCatalogFolderP->userInfo.frFlags |= SWAP_BE16((UInt16)p->maskBit);
		}
		else
		{
			largeCatalogFolderP->userInfo.frFlags = SWAP_BE16((UInt16)p->correct);
		}
	//	largeCatalogFolderP->contentModDate = timeStamp;
	}
	else
	{
		HFSCatalogFolder	*smallCatalogFolderP	= (HFSCatalogFolder *) &record;	
		if ( p->incorrect != SWAP_BE16(smallCatalogFolderP->userInfo.frFlags) )		//	do we know what we're doing?
		{
			//	Another repar order may have affected the flags
			if ( p->correct < p->incorrect )
				smallCatalogFolderP->userInfo.frFlags &= SWAP_BE16(~((UInt16)p->maskBit));
			else
				smallCatalogFolderP->userInfo.frFlags |= SWAP_BE16((UInt16)p->maskBit);
		}
		else
		{
			smallCatalogFolderP->userInfo.frFlags = SWAP_BE16((UInt16)p->correct);
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
	Boolean				isHFSPlus;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
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
FixIllegalNames:  Fix catalog enties that have illegal names.
    RepairOrder.name[] holds the old (illegal) name followed by the new name.
    The new name has been checked to make sure it is unique within the target
    directory.  The names will look like this:
        2 byte length of old name (in unicode characters not bytes)
        unicode characters for old name
        2 byte length of new name (in unicode characters not bytes)
        unicode characters for new name
------------------------------------------------------------------------------*/
static OSErr
FixIllegalNames( SGlobPtr GPtr, RepairOrderPtr roPtr )
{
	OSErr 				result;
	Boolean				isHFSPlus;
	Boolean				isDirectory;
	UInt16				recSize;
	SFCB *				fcbPtr;
    CatalogName *		oldNamePtr;
    CatalogName *		newNamePtr;
	UInt32				hint;				
	CatalogRecord		record;
	CatalogKey			key;
	CatalogKey			newKey;

	isHFSPlus = VolumeObjectIsHFSPlus( );
 	fcbPtr = GPtr->calculatedCatalogFCB;
    
	oldNamePtr = (CatalogName *) &roPtr->name;
    if ( isHFSPlus )
    {
        int					myLength;
        u_int16_t *			myPtr = (u_int16_t *) oldNamePtr;
        myLength = *myPtr; // get length of old name
        myPtr += (1 + myLength);  // bump past length of old name and old name
        newNamePtr = (CatalogName *) myPtr;
    }
    else
    {
        int					myLength;
        u_char *			myPtr = (u_char *) oldNamePtr;
        myLength = *myPtr; // get length of old name
        myPtr += (1 + myLength);  // bump past length of old name and old name
        newNamePtr = (CatalogName *) myPtr;
    }

	// make sure new name isn't already there
	BuildCatalogKey( roPtr->parid, newNamePtr, isHFSPlus, &newKey );
	result = SearchBTreeRecord( fcbPtr, &newKey, kNoHint, NULL, &record, &recSize, NULL );
	if ( result == noErr ) {
		if ( GPtr->logLevel >= kDebugLog ) {
        	printf( "\treplacement name already exists \n" );
			printf( "\tduplicate name is 0x" );
			PrintName( newNamePtr->ustr.length, (UInt8 *) &newNamePtr->ustr.unicode, true );
		}
        goto ErrorExit;
    }
    
    // get catalog record for object with the illegal name.  We will restore this
    // info with our new (valid) name. 
	BuildCatalogKey( roPtr->parid, oldNamePtr, isHFSPlus, &key );
	result = SearchBTreeRecord( fcbPtr, &key, kNoHint, NULL, &record, &recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
    }
 
    result	= DeleteBTreeRecord( fcbPtr, &key );
	if ( result != noErr ) {
        goto ErrorExit;
    }
 
    // insert record back into the catalog using the new name
    result	= InsertBTreeRecord( fcbPtr, &newKey, &record, recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
    }

	isDirectory = false;
    switch( record.recordType )
    {
        case kHFSFolderRecord:
        case kHFSPlusFolderRecord:	
			isDirectory = true;	 break;
    }
 
    // now we need to remove the old thread record and create a new one with
    // our new name.
	BuildCatalogKey( GetObjectID( &record ), NULL, isHFSPlus, &key );
	result = SearchBTreeRecord( fcbPtr, &key, kNoHint, NULL, &record, &recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
    }
 
    result	= DeleteBTreeRecord( fcbPtr, &key );
	if ( result != noErr ) {
        goto ErrorExit;
    }

    // insert thread record with new name as thread data
	recSize = BuildThreadRec( &newKey, &record, isHFSPlus, isDirectory );
 	result = InsertBTreeRecord( fcbPtr, &key, &record, recSize, &hint );
	if ( result != noErr ) {
        goto ErrorExit;
	}

    return( noErr );
 
ErrorExit:
	GPtr->minorRepairErrors = true;
    if ( GPtr->logLevel >= kDebugLog )
        printf( "\t%s - repair failed for type 0x%02X %d \n", __FUNCTION__, roPtr->type, roPtr->type );
    return( noErr );  // errors in this routine should not be fatal
    
} /* FixIllegalNames */


/*------------------------------------------------------------------------------
FixBSDInfo:  Reset or repair BSD info
                 (HFS Plus volumes only)
------------------------------------------------------------------------------*/
static OSErr
FixBSDInfo(SGlobPtr GPtr, RepairOrderPtr p)
{
	SFCB 						*fcb;
	CatalogRecord 				rec;
	FSBufferDescriptor 			btRecord;
	BTreeIterator 				btIterator;
	Boolean						isHFSPlus;
	OSErr 						result;
	UInt16 						recSize;
	size_t 						namelen;
	unsigned char 				filename[256];

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
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
	CatalogName 		name;
	CatalogName 		*cNameP;
	Boolean				isHFSPlus;
	size_t 				len;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
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
	Boolean	isHFSPlus;
	Boolean replace;
	OSErr result;
	UInt16 recSize;
	UInt64 bytes;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	if (!isHFSPlus)
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

Function:	If the "mdb->drAlBlSt" field has been modified, i.e. Norton Disk Doctor
			3.5 tried to "Fix" an HFS+ volume, it reduces the value in the 
			"mdb->drAlBlSt" field.  If this field is changed, the file system can 
			no longer find the VolumeHeader or AltVolumeHeader.
			
Input:		GPtr			-	pointer to scavenger global area
			p				- 	pointer to the repair order

Output:		FixMDBdrAlBlSt	- 	function result:
									0 = no error
									n = error
------------------------------------------------------------------------------*/

static	OSErr	FixEmbededVolDescription( SGlobPtr GPtr, RepairOrderPtr p )
{
	OSErr					err;
	HFSMasterDirectoryBlock	*mdb;
	EmbededVolDescription	*desc;
	SVCB					*vcb = GPtr->calculatedVCB;
	BlockDescriptor  		block;

	desc = (EmbededVolDescription *) &(p->name);
	block.buffer = NULL;
	
	/* Fix the Alternate MDB */
	err = GetVolumeObjectAlternateMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock *) block.buffer;
	
	mdb->drAlBlSt			= desc->drAlBlSt;
	mdb->drEmbedSigWord		= desc->drEmbedSigWord;
	mdb->drEmbedExtent.startBlock	= desc->drEmbedExtent.startBlock;
	mdb->drEmbedExtent.blockCount	= desc->drEmbedExtent.blockCount;
	
	err = ReleaseVolumeBlock( vcb, &block, kForceWriteBlock );
	block.buffer = NULL;
	if ( err != noErr )
		goto ExitThisRoutine;
	
	/* Fix the MDB */
	err = GetVolumeObjectPrimaryMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock *) block.buffer;
	
	mdb->drAlBlSt			= desc->drAlBlSt;
	mdb->drEmbedSigWord		= desc->drEmbedSigWord;
	mdb->drEmbedExtent.startBlock	= desc->drEmbedExtent.startBlock;
	mdb->drEmbedExtent.blockCount	= desc->drEmbedExtent.blockCount;
	err = ReleaseVolumeBlock( vcb, &block, kForceWriteBlock );
	block.buffer = NULL;
	
ExitThisRoutine:
	if ( block.buffer != NULL )
		err = ReleaseVolumeBlock( vcb, &block, kReleaseBlock );
	
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

	OSErr						err;
	HFSMasterDirectoryBlock		*mdb;
	SVCB						*vcb = GPtr->calculatedVCB;
	BlockDescriptor  			block;

	/* Get the Alternate MDB */
	block.buffer = NULL;
	err = GetVolumeObjectAlternateMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock	*) block.buffer;

	/* Fix the wrapper catalog's first (and only) extent */
	mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock +
	                                mdb->drXTExtRec[0].blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	block.buffer = NULL;
	if ( err != noErr )
		goto ExitThisRoutine;
	
	/* Fix the MDB */
	err = GetVolumeObjectPrimaryMDB( &block );
	if ( err != noErr )
		goto ExitThisRoutine;
	mdb = (HFSMasterDirectoryBlock	*) block.buffer;
	
	mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock +
	                                mdb->drXTExtRec[0].blockCount;
	
	err = ReleaseVolumeBlock(vcb, &block, kForceWriteBlock);
	block.buffer = NULL;

ExitThisRoutine:
	if ( block.buffer != NULL )
		(void) ReleaseVolumeBlock( vcb, &block, kReleaseBlock );
	
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
	Boolean			isHFSPlus;
	SVCB			*calculatedVCB		= GPtr->calculatedVCB;
	UInt32			**dataHandle		= GPtr->validFilesList;
	SFCB *			fcb = GPtr->calculatedExtentsFCB;

	//	Set Up
	isHFSPlus = VolumeObjectIsHFSPlus( );
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
					{	//¥¥ can't we just delete btree record?
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
					{	//¥¥ can't we just delete btree record?
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
	SInt16				selCode				= 0x8001;
	Boolean				isHFSPlus;
	BTreeControlBlock	*btcb				= GetBTreeControlBlock( kCalculatedCatalogRefNum );

	isHFSPlus = VolumeObjectIsHFSPlus( );
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

				err = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, 
										 &foundKey, &threadRecord, &recordSize, &hint2 );
				if ( err != noErr )
				{
					//	For missing thread records, just create the thread
					if ( err == btNotFound )
					{
						//	Create the missing thread record.
						Boolean		isDirectory;
						
						isDirectory = false;
						switch( recordType )
						{
							case kHFSFolderRecord:
							case kHFSPlusFolderRecord:	
								isDirectory = true;		
								break;
						}

						//-- Fill out the data for the new file thread
						recordSize = BuildThreadRec( &foundKey, &threadRecord, isHFSPlus, 
													 isDirectory );
						err = InsertBTreeRecord( GPtr->calculatedCatalogFCB, &key,
												 &threadRecord, recordSize, &threadHint );
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

/* Function:	GetCatalogRecord
 *
 * Description:
 * This function returns a catalog file/folder record for a given
 * file/folder ID from the catalog BTree.
 *
 * Input:	1. GPtr - pointer to global scavenger area
 *        	2. fileID - file ID to search the file/folder record  
 *      	3. isHFSPlus - boolean value to indicate if volume is HFSPlus 
 *
 * Output:	1. catKey - catalog key
 *        	2. catRecord - catalog record for given ID
 *         	3. recordSize - size of catalog record return back 
 *
 * Return value:	
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr GetCatalogRecord(SGlobPtr GPtr, UInt32 fileID, Boolean isHFSPlus, CatalogKey *catKey, CatalogRecord *catRecord, UInt16 *recordSize) 
{
	OSErr err = noErr;
	CatalogKey catThreadKey;
	CatalogName catalogName;
	UInt32 hint;

	/* Look up for catalog thread record for the file that owns attribute */
	BuildCatalogKey(fileID, NULL, isHFSPlus, &catThreadKey);
	err = SearchBTreeRecord(GPtr->calculatedCatalogFCB, &catThreadKey, kNoHint, catKey, catRecord, recordSize, &hint);
	if (err) {
#if DEBUG_XATTR
		printf ("%s: No matching catalog thread record found\n", __FUNCTION__);
#endif
		goto out;
	}

#if DEBUG_XATTR
	printf ("%s(%s,%d):1 recordType=%x, flags=%x\n", __FUNCTION__, __FILE__, __LINE__, 
				catRecord->hfsPlusFile.recordType, 
				catRecord->hfsPlusFile.flags);
#endif

	/* We were expecting a thread record.  The recordType says it is a file 
	 * record or folder record.  Return error.
	 */
	if ((catRecord->hfsPlusFile.recordType == kHFSPlusFolderRecord) ||
	    (catRecord->hfsPlusFile.recordType == kHFSPlusFileRecord)) {
		err = fsBTRecordNotFoundErr; 
		goto out;
	}
	
	/* It is either a file thread record or folder thread record.
	 * Look up for catalog record for the file that owns attribute */
	CopyCatalogName((CatalogName *)&(catRecord->hfsPlusThread.nodeName), &catalogName, isHFSPlus);
	BuildCatalogKey(catRecord->hfsPlusThread.parentID, &catalogName, isHFSPlus, catKey); 
	err = SearchBTreeRecord(GPtr->calculatedCatalogFCB, catKey, kNoHint, catKey, catRecord, recordSize, &hint);
	if (err) {
#if DEBUG_XATTR	
		printf ("%s: No matching catalog record found\n", __FUNCTION__);
#endif
		goto out;
	}
	
#if DEBUG_XATTR
	printf ("%s(%s,%d):2 recordType=%x, flags=%x\n", __FUNCTION__, __FILE__, __LINE__, 
				catRecord->hfsPlusFile.recordType, 
				catRecord->hfsPlusFile.flags);
#endif

out:
	return err;
}

/* Function:	RepairAttributesCheckABT
 *
 * Description:
 * This function is called from RepairAttributes (to repair extended 
 * attributes) during repair stage of fsck_hfs.
 *
 * 1. Make full pass through attribute BTree.
 * 2. For every unique fileID, lookup its catalog record in Catalog BTree.
 * 3. If found, check the attributes/security bit in catalog record.
 *              If not set correctly, set it and replace the catalog record.
 * 4. If not found, return error 
 * 
 * Input:	1. GPtr - pointer to global scavenger area
 *      	2. isHFSPlus - boolean value to indicate if volume is HFSPlus 
 *
 * Output:	err - Function result 
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr RepairAttributesCheckABT(SGlobPtr GPtr, Boolean isHFSPlus) 
{
	OSErr err = noErr;
	UInt16 selCode;		/* select access pattern for BTree */
	UInt32 hint;

	HFSPlusAttrRecord attrRecord;
	HFSPlusAttrKey attrKey;
	UInt16 attrRecordSize;
	CatalogRecord catRecord; 
	CatalogKey catKey;
	UInt16 catRecordSize;

	lastAttrID lastID;	/* fileID for the last attribute searched */
	Boolean didRecordChange = false; 	/* whether catalog record was changed after checks */

#if DEBUG_XATTR
	char attrName[XATTR_MAXNAMELEN];
	size_t len;
#endif

	lastID.fileID = 0;
	lastID.hasSecurity = false;
	
	selCode = 0x8001;	/* Get first record from BTree */
	err = GetBTreeRecord(GPtr->calculatedAttributesFCB, selCode, &attrKey, &attrRecord, &attrRecordSize, &hint);
	if (err != noErr) goto out; 

	selCode = 1;	/* Get next record */
	do {
#if DEBUG_XATTR
		/* Convert unicode attribute name to char for ACL check */
		(void) utf_encodestr(attrKey.attrName, attrKey.attrNameLen * 2, attrName, &len);
		attrName[len] = '\0';
		printf ("%s(%s,%d): Found attrName=%s for fileID=%d\n", __FUNCTION__, __FILE__, __LINE__, attrName, attrKey.fileID);
#endif
	
		if (attrKey.fileID != lastID.fileID) {
			/* We found an attribute with new file ID */
			
			/* Replace the previous catalog record only if we updated the flags */
			if (didRecordChange == true) {
				err = ReplaceBTreeRecord(GPtr->calculatedCatalogFCB, &catKey , kNoHint, &catRecord, catRecordSize, &hint);
				if (err) {
#if DEBUG_XATTR
					printf ("%s: Error in replacing Catalog Record\n", __FUNCTION__);
#endif		
					goto out;
				}
			}
			
			didRecordChange = false; /* reset to indicate new record has not changed */

			/* Get the catalog record for the new fileID */
			err = GetCatalogRecord(GPtr, attrKey.fileID, isHFSPlus, &catKey, &catRecord, &catRecordSize);
			if (err) {
				/* No catalog record was found for this fileID. */
#if DEBUG_XATTR
				printf ("%s: No matching catalog record found\n", __FUNCTION__);
#endif
				/* 3984119 - Do not delete extended attributes for file IDs less
	 			 * kHFSFirstUserCatalogNodeID but not equal to kHFSRootFolderID 
	 			 * in prime modulus checksum.  These file IDs do not have 
	 			 * any catalog record
	 			 */
				if ((attrKey.fileID < kHFSFirstUserCatalogNodeID) && 
	    			    (attrKey.fileID != kHFSRootFolderID)) { 
#if DEBUG_XATTR
					printf ("%s: Ignore catalog check for fileID=%d for attribute=%s\n", __FUNCTION__, attrKey.fileID, attrName); 
#endif
					goto getnext;
				}

				/* Delete this orphan extended attribute */
				err = DeleteBTreeRecord(GPtr->calculatedAttributesFCB, &attrKey);
				if (err) {
#if DEBUG_XATTR
					printf ("%s: Error in deleting attribute record\n", __FUNCTION__);
#endif
					goto out;
				}
#if DEBUG_XATTR
				printf ("%s: Deleted attribute=%s for fileID=%d\n", __FUNCTION__, attrName, attrKey.fileID);
#endif
				/* set flags to write back header and map */
				GPtr->ABTStat |= S_BTH + S_BTM;	
				goto getnext;
			} 

			lastID.fileID = attrKey.fileID;	/* set last fileID to the new ID */
			lastID.hasSecurity = false; /* reset to indicate new fileID does not have security */
				
			/* Check the Attribute bit */
			if (!(catRecord.hfsPlusFile.flags & kHFSHasAttributesMask)) {
				/* kHFSHasAttributeBit should be set */
				catRecord.hfsPlusFile.flags |= kHFSHasAttributesMask;
				didRecordChange = true;
			}

			/* Check if attribute is ACL */
			if (!bcmp(attrKey.attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
				lastID.hasSecurity = true;
				/* Check the security bit */
				if (!(catRecord.hfsPlusFile.flags & kHFSHasSecurityMask)) {
					/* kHFSHasSecurityBit should be set */
					catRecord.hfsPlusFile.flags |= kHFSHasSecurityMask;
					didRecordChange = true;
				}
			}
		} else {
			/* We have seen attribute for fileID in past */

			/* If last time we saw this fileID did not have an ACL and this 
			 * extended attribute is an ACL, ONLY check consistency of 
			 * security bit from Catalog record 
			 */
			if ((lastID.hasSecurity == false) && !bcmp(attrKey.attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
				lastID.hasSecurity = true;
				/* Check the security bit */
				if (!(catRecord.hfsPlusFile.flags & kHFSHasSecurityMask)) {
					/* kHFSHasSecurityBit should be set */
					catRecord.hfsPlusFile.flags |= kHFSHasSecurityMask;
					didRecordChange = true;
				}
			}
		}
		
getnext:
		/* Access the next record */
		err = GetBTreeRecord(GPtr->calculatedAttributesFCB, selCode, &attrKey, &attrRecord, &attrRecordSize, &hint);
	} while (err == noErr);

	err = noErr;

	/* Replace the catalog record for last extended attribute in the attributes BTree
	 * only if we updated the flags
	 */
	if (didRecordChange == true) {
		err = ReplaceBTreeRecord(GPtr->calculatedCatalogFCB, &catKey , kNoHint, &catRecord, catRecordSize, &hint);
		if (err) {
#if DEBUG_XATTR
			printf ("%s: Error in replacing Catalog Record\n", __FUNCTION__);
#endif		
			goto out;
		}
	}

out:
	return err;
}

/* Function:	RepairAttributesCheckCBT
 *
 * Description:
 * This function is called from RepairAttributes (to repair extended 
 * attributes) during repair stage of fsck_hfs.
 *
 * NOTE: The case where attribute exists and bit is not set is being taken care in 
 * RepairAttributesCheckABT.  This function determines relationship from catalog
 * Btree to attribute Btree, and not vice-versa. 

 * 1. Make full pass through catalog BTree.
 * 2. For every fileID, if the attributes/security bit is set, 
 *    lookup all the extended attributes in the attributes BTree.
 * 3. If found, check that if bits are set correctly.
 * 4. If not found, clear the bits.
 * 
 * Input:	1. GPtr - pointer to global scavenger area
 *      	2. isHFSPlus - boolean value to indicate if volume is HFSPlus 
 *
 * Output:	err - Function result 
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr RepairAttributesCheckCBT(SGlobPtr GPtr, Boolean isHFSPlus)
{
	OSErr err = noErr;
	UInt16 selCode;		/* select access pattern for BTree */
	UInt16 recordSize;
	UInt32 hint;

	HFSPlusAttrKey *attrKey;
	CatalogRecord catRecord; 
	CatalogKey catKey;

	Boolean didRecordChange = false; 	/* whether catalog record was changed after checks */

	BTreeIterator iterator;

	UInt32 curFileID;
	Boolean curRecordHasAttributes = false;
	Boolean curRecordHasSecurity = false;

	selCode = 0x8001;	/* Get first record from BTree */
	err = GetBTreeRecord(GPtr->calculatedCatalogFCB, selCode, &catKey, &catRecord, &recordSize, &hint);
	if ( err != noErr ) goto out;

	selCode = 1;	/* Get next record */
	do {
		/* Check only file record and folder record, else skip to next record */
		if ( (catRecord.hfsPlusFile.recordType != kHFSPlusFileRecord) && 
		     (catRecord.hfsPlusFile.recordType != kHFSPlusFolderRecord)) {
			goto getnext;
		}

		/* Check if catalog record has attribute and/or security bit set, else
		 * skip to next record 
		 */
		if ( ((catRecord.hfsPlusFile.flags & kHFSHasAttributesMask) == 0) && 
		   	 ((catRecord.hfsPlusFile.flags & kHFSHasSecurityMask) == 0) ) {
			 goto getnext;
		}

		/* Initialize some flags */
		didRecordChange = false;
		curRecordHasSecurity = false;
		curRecordHasAttributes = false;

		/* Access all extended attributes for this fileID */ 
		curFileID = catRecord.hfsPlusFile.fileID;

		/* Initialize the iterator and attribute key */
		ClearMemory(&iterator, sizeof(BTreeIterator));
		attrKey = (HFSPlusAttrKey *)&iterator.key;
		attrKey->keyLength = kHFSPlusAttrKeyMinimumLength;
		attrKey->fileID = curFileID;

		/* Search for attribute with NULL name.  This will place the iterator at correct fileID location in BTree */	
		err = BTSearchRecord(GPtr->calculatedAttributesFCB, &iterator, kInvalidMRUCacheKey, NULL, NULL, &iterator);
		if (err && (err != btNotFound)) {
#if DEBUG_XATTR
			printf ("%s: No matching attribute record found\n", __FUNCTION__);
#endif
			goto out;
		}

		/* Iterate over to all extended attributes for given fileID */
		err = BTIterateRecord(GPtr->calculatedAttributesFCB, kBTreeNextRecord, &iterator, NULL, NULL);

		/* Check only if we did _find_ an attribute record for the current fileID */
		while ((err == noErr) && (attrKey->fileID == curFileID)) {
			/* Current record should have attribute bit set */
			curRecordHasAttributes = true;

			/* Current record should have security bit set */
			if (!bcmp(attrKey->attrName, GPtr->securityAttrName, GPtr->securityAttrLen)) {
				curRecordHasSecurity = true;
			}

			/* Get the next record */
			err = BTIterateRecord(GPtr->calculatedAttributesFCB, kBTreeNextRecord, &iterator, NULL, NULL);
		}

		/* Determine if we need to update the catalog record */
		if ((curRecordHasAttributes == false) && (catRecord.hfsPlusFile.flags & kHFSHasAttributesMask)) {
			/* If no attribute exists and attributes bit is set, clear it */
			catRecord.hfsPlusFile.flags &= ~kHFSHasAttributesMask;		
			didRecordChange = true;
		}
				
		if ((curRecordHasSecurity == false) && (catRecord.hfsPlusFile.flags & kHFSHasSecurityMask)) {
			/* If no security attribute exists and security bit is set, clear it */
			catRecord.hfsPlusFile.flags &= ~kHFSHasSecurityMask;		
			didRecordChange = true;
		}
			 
		/* If there was any change in catalog record, write it back to disk */
		if (didRecordChange == true) {
			err = ReplaceBTreeRecord( GPtr->calculatedCatalogFCB, &catKey , kNoHint, &catRecord, recordSize, &hint );
			if (err) {
#if DEBUG_XATTR
				printf ("%s: Error writing catalog record\n", __FUNCTION__);
#endif
				goto out;
			}
		}

getnext:
		/* Access the next record */
		err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &catKey, &catRecord, &recordSize, &hint );
	} while (err == noErr); 

	err = noErr;

out:
	return err;
}

/* Function:	RepairAttributes
 *
 * Description:
 * This function fixes the extended attributes consistency by
 * calling two functions:
 * 1. RepairAttributesCheckABT:  Traverses attributes Btree and
 * checks if each attribute has correct bits set in its corresponding 
 * catalog record.
 * 2. RepairAttributesCheckCBT:  Traverses catalog Btree and checks
 * if each catalog record that has attribute/security bit set have
 * corresponding extended attributes. 
 * 
 * Input:	1. GPtr - pointer to global scavenger area
 *
 * Output:	err - Function result 
 * 		      zero means success
 *		      non-zero means failure
 */
static OSErr RepairAttributes(SGlobPtr GPtr)
{
	OSErr err = noErr;
	Boolean isHFSPlus;
	
	/* Check if the volume is HFS Plus volume */
	isHFSPlus = VolumeObjectIsHFSPlus();
	if (!isHFSPlus) {
		goto out;
	}

	/* Traverse Attributes BTree and access required records in Catalog BTree */
	err = RepairAttributesCheckABT(GPtr, isHFSPlus);
	if (err) {
		goto out;
	}

	/* Traverse Catalog BTree and access required records in Attributes BTree */
	err = RepairAttributesCheckCBT(GPtr, isHFSPlus);
	if (err) {
		goto out;
	}

out:
	return err;
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
	OSErr			err = R_RFail;
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
	struct MissingThread *	mtp;
	FSBufferDescriptor    	btRecord;
	BTreeIterator         	iterator;
	OSStatus           		result;
	UInt16              	dataSize;
	Boolean					headsUp;
	UInt32					lostAndFoundDirID;

	lostAndFoundDirID = 0;
	headsUp = false;
	for (mtp = GPtr->missingThreadList; mtp != NULL; mtp = mtp->link) {
		if ( mtp->threadID == 0 )
			continue;

		// if the thread record information in the MissingThread struct is not there
		// then we have a missing directory in addition to a missing thread record 
		// for that directory.  We will recreate the missing directory in our 
		// lost+found directory.
		if ( mtp->thread.parentID == 0 ) {
			char 	myString[32];
			if ( lostAndFoundDirID == 0 )
				lostAndFoundDirID = CreateLostAndFoundDir( GPtr );
			if ( lostAndFoundDirID == 0 ) {
				if ( GPtr->logLevel >= kDebugLog )
					printf( "\tCould not create lost+found directory \n" );
				return( R_RFail );
			}
			sprintf( myString, "%ld", (long)mtp->threadID );
			PrintError( GPtr, E_NoDir, 1, myString );
			result = FixMissingDirectory( GPtr, mtp->threadID, lostAndFoundDirID );
			if ( result != 0 ) {
				if ( GPtr->logLevel >= kDebugLog )
					printf( "\tCould not recreate a missing directory \n" );
				return( R_RFail );
			}
			else
				headsUp = true;
			continue;
		}

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
	if ( headsUp )
		PrintStatus( GPtr, M_Look, 0 );

	return (0);
}


static OSErr
FixMissingDirectory( SGlob *GPtr, UInt32 theObjID, UInt32 theParID )
{
	Boolean				isHFSPlus;
	UInt16				recSize;
	OSErr				result;		
	int					nameLen;
	UInt32				hint;		
	UInt32				myItemsCount;		
	char 				myString[ 32 ];
	CatalogName			myName;
	CatalogRecord		catRec;
	CatalogKey			myKey, myThreadKey;

	isHFSPlus = VolumeObjectIsHFSPlus( );
	
	// we will use the object ID of the missing directory as the name since we have
	// no way to find the original name and this should make it unique within our
	// lost+found directory.
	sprintf( myString, "%ld", (long)theObjID );
	nameLen = strlen( myString );

    if ( isHFSPlus )
    {
        int		i;
        myName.ustr.length = nameLen;
        for ( i = 0; i < myName.ustr.length; i++ )
            myName.ustr.unicode[ i ] = (u_int16_t) myString[ i ];
    }
    else
    {
        myName.pstr[0] = nameLen;
        memcpy( &myName.pstr[1], &myString[0], nameLen );
    }

	// make sure the name is not already used 
	BuildCatalogKey( theParID, &myName, isHFSPlus, &myKey );
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &myKey, kNoHint, 
								NULL, &catRec, &recSize, &hint );
	if ( result == noErr )
		return( R_IntErr );
	
    // insert new directory and thread record into the catalog
	recSize = BuildThreadRec( &myKey, &catRec, isHFSPlus, true );
	BuildCatalogKey( theObjID, NULL, isHFSPlus, &myThreadKey );
	result	= InsertBTreeRecord( GPtr->calculatedCatalogFCB, &myThreadKey, &catRec, recSize, &hint );
	if ( result != noErr )
		return( result );

	// need to look up all objects in the directory so we can set the valance
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &myThreadKey, kNoHint, 
								NULL, &catRec, &recSize, &hint );
	if ( result != noErr )
		return( result );

	myItemsCount = 0;
	for ( ;; ) {
		CatalogKey		foundKey;
		result = GetBTreeRecord( GPtr->calculatedCatalogFCB, 1, &foundKey, &catRec, &recSize, &hint );
		if ( result != noErr )
			break;
		if ( isHFSPlus )
			if ( foundKey.hfsPlus.parentID != theObjID )
				break;
		else
			if ( foundKey.hfs.parentID != theObjID )
				break;
		if ( catRec.recordType == kHFSPlusFolderRecord || catRec.recordType == kHFSPlusFileRecord ||
			 catRec.recordType == kHFSFolderRecord || catRec.recordType == kHFSFileRecord ) {
			myItemsCount++;
		}
	}

	recSize = BuildFolderRec( 01777, theObjID, isHFSPlus, &catRec );
	if ( isHFSPlus )
		catRec.hfsPlusFolder.valence = myItemsCount;
	else
		catRec.hfsFolder.valence = myItemsCount;
	result	= InsertBTreeRecord( GPtr->calculatedCatalogFCB, &myKey, &catRec, recSize, &hint );
	if ( result != noErr )
		return( result );
	
	/* update parent directory to reflect addition of new directory */
	result = UpdateFolderCount( GPtr->calculatedVCB, theParID, NULL, 
								((isHFSPlus) ? kHFSPlusFolderRecord : kHFSFolderRecord), 
								kNoHint, 1 );

	/* update our header node on disk from our BTreeControlBlock */
	UpdateBTreeHeader( GPtr->calculatedCatalogFCB );
		
	return( result );
	
} /* FixMissingDirectory */


static HFSCatalogNodeID 
GetObjectID( CatalogRecord * theRecPtr )
{
    HFSCatalogNodeID	myObjID;
    
	switch ( theRecPtr->recordType ) {
	case kHFSPlusFolderRecord:
        myObjID = theRecPtr->hfsPlusFolder.folderID;
		break;
	case kHFSPlusFileRecord:
        myObjID = theRecPtr->hfsPlusFile.fileID;
		break;
	case kHFSFolderRecord:
        myObjID = theRecPtr->hfsFolder.folderID;
		break;
	case kHFSFileRecord:
        myObjID = theRecPtr->hfsFile.fileID;
		break;
	default:
        myObjID = 0;
	}
	
    return( myObjID );
    
} /* GetObjectID */


/*
 *	Create a lost and found directory at the root of the volume we are checking.
 *
 *  Returns directory ID of the new (or existing) lost+found directory or 0 on error.
 */

static UInt32
CreateLostAndFoundDir( SGlob *GPtr )
{
	Boolean				isHFSPlus;
	UInt16				recSize;
	UInt16				myMode;
	int					result;		
	int					nameLen;
	UInt32				hint;		
	UInt32				nextCNID;		
	SFCB *				fcbPtr;
	u_char				lostAndFoundDirName[] = "lost+found";
	CatalogKey			myKey;
	CatalogName			myName;
	CatalogRecord		catRec;
	
	isHFSPlus = VolumeObjectIsHFSPlus( );
  	fcbPtr = GPtr->calculatedCatalogFCB;
	nameLen = strlen( lostAndFoundDirName );

    if ( isHFSPlus )
    {
        int		i;
        myName.ustr.length = nameLen;
        for ( i = 0; i < myName.ustr.length; i++ )
            myName.ustr.unicode[ i ] = (u_int16_t) lostAndFoundDirName[ i ];
    }
    else
    {
        myName.pstr[0] = nameLen;
        memcpy( &myName.pstr[1], &lostAndFoundDirName[0], nameLen );
    }

	// see if we already have a lost and found directory
	BuildCatalogKey( kHFSRootFolderID, &myName, isHFSPlus, &myKey );
	result = SearchBTreeRecord( fcbPtr, &myKey, kNoHint, NULL, &catRec, &recSize, &hint );
	if ( result == noErr ) {
		if ( isHFSPlus ) {
			if ( catRec.recordType == kHFSPlusFolderRecord )
				return( catRec.hfsPlusFolder.folderID ); 
		}
		else if ( catRec.recordType == kHFSFolderRecord )
			return( catRec.hfsFolder.folderID ); 	
        return( 0 );  // something already there but not a directory
	}
  
    // insert new directory and thread record into the catalog
	nextCNID = GPtr->calculatedVCB->vcbNextCatalogID;
	if ( !isHFSPlus && nextCNID == 0xFFFFFFFF )
		return( 0 );

	recSize = BuildThreadRec( &myKey, &catRec, isHFSPlus, true );
	for (;;) {
		CatalogKey			key;
		
		BuildCatalogKey( nextCNID, NULL, isHFSPlus, &key );
		result	= InsertBTreeRecord( fcbPtr, &key, &catRec, recSize, &hint );
		if ( result == fsBTDuplicateRecordErr && isHFSPlus ) {
			/*
			 * Allow CNIDs on HFS Plus volumes to wrap around
			 */
			++nextCNID;
			if ( nextCNID < kHFSFirstUserCatalogNodeID ) {
				GPtr->calculatedVCB->vcbAttributes |= kHFSCatalogNodeIDsReusedMask;
				MarkVCBDirty( GPtr->calculatedVCB );
				nextCNID = kHFSFirstUserCatalogNodeID;
			}
			continue;
		}
		break;
	}
	if ( result != 0 )
		return( 0 ); 	
	
	myMode = ( GPtr->lostAndFoundMode == 0 ) ? 01777 : GPtr->lostAndFoundMode;
	recSize = BuildFolderRec( myMode, nextCNID, isHFSPlus, &catRec );
    result	= InsertBTreeRecord( fcbPtr, &myKey, &catRec, recSize, &hint );
	if ( result != 0 )
		return( 0 );

	/* Update volume header */
	GPtr->calculatedVCB->vcbNextCatalogID = nextCNID + 1;
	if ( GPtr->calculatedVCB->vcbNextCatalogID < kHFSFirstUserCatalogNodeID ) {
		GPtr->calculatedVCB->vcbAttributes |= kHFSCatalogNodeIDsReusedMask;
		GPtr->calculatedVCB->vcbNextCatalogID = kHFSFirstUserCatalogNodeID;
	}
	MarkVCBDirty( GPtr->calculatedVCB );
	
	/* update parent directory to reflect addition of new directory */
	result = UpdateFolderCount( GPtr->calculatedVCB, kHFSRootFolderID, NULL, kHFSPlusFolderRecord, kNoHint, 1 );

	/* update our header node on disk from our BTreeControlBlock */
	UpdateBTreeHeader( GPtr->calculatedCatalogFCB );

	return( nextCNID );

} /* CreateLostAndFoundDir */


/*
 * Build a catalog node folder record with the given input.
 */
static int
BuildFolderRec( u_int16_t theMode, UInt32 theObjID, Boolean isHFSPlus, CatalogRecord * theRecPtr )
{
	UInt16				recSize;
	UInt32 				createTime;
	
	ClearMemory( (Ptr)theRecPtr, sizeof(*theRecPtr) );
	
	if ( isHFSPlus ) {
		createTime = GetTimeUTC();
		theRecPtr->hfsPlusFolder.recordType = kHFSPlusFolderRecord;
		theRecPtr->hfsPlusFolder.folderID = theObjID;
		theRecPtr->hfsPlusFolder.createDate = createTime;
		theRecPtr->hfsPlusFolder.contentModDate = createTime;
		theRecPtr->hfsPlusFolder.attributeModDate = createTime;
		theRecPtr->hfsPlusFolder.backupDate = createTime;
		theRecPtr->hfsPlusFolder.bsdInfo.ownerID = getuid( );
		theRecPtr->hfsPlusFolder.bsdInfo.groupID = getgid( );
		theRecPtr->hfsPlusFolder.bsdInfo.fileMode = S_IFDIR;
		theRecPtr->hfsPlusFolder.bsdInfo.fileMode |= theMode;
		recSize= sizeof(HFSPlusCatalogFolder);
	}
	else {
		createTime = GetTimeLocal( true );
		theRecPtr->hfsFolder.recordType = kHFSFolderRecord;
		theRecPtr->hfsFolder.folderID = theObjID;
		theRecPtr->hfsFolder.createDate = createTime;
		theRecPtr->hfsFolder.modifyDate = createTime;
		theRecPtr->hfsFolder.backupDate = createTime;
		recSize= sizeof(HFSCatalogFolder);
	}

	return( recSize );
	
} /* BuildFolderRec */


/*
 * Build a catalog node thread record from a catalog key
 * and return the size of the record.
 */
static int
BuildThreadRec( CatalogKey * theKeyPtr, CatalogRecord * theRecPtr, 
				Boolean isHFSPlus, Boolean isDirectory )
{
	int size = 0;

	if ( isHFSPlus ) {
		HFSPlusCatalogKey *key = (HFSPlusCatalogKey *)theKeyPtr;
		HFSPlusCatalogThread *rec = (HFSPlusCatalogThread *)theRecPtr;

		size = sizeof(HFSPlusCatalogThread);
		if ( isDirectory )
			rec->recordType = kHFSPlusFolderThreadRecord;
		else
			rec->recordType = kHFSPlusFileThreadRecord;
		rec->reserved = 0;
		rec->parentID = key->parentID;			
		bcopy(&key->nodeName, &rec->nodeName,
			sizeof(UniChar) * (key->nodeName.length + 1));

		/* HFS Plus has varaible sized thread records */
		size -= (sizeof(rec->nodeName.unicode) -
			  (rec->nodeName.length * sizeof(UniChar)));
	} 
	else /* HFS standard */ {
		HFSCatalogKey *key = (HFSCatalogKey *)theKeyPtr;
		HFSCatalogThread *rec = (HFSCatalogThread *)theRecPtr;

		size = sizeof(HFSCatalogThread);
		bzero(rec, size);
		if ( isDirectory )
			rec->recordType = kHFSFolderThreadRecord;
		else
			rec->recordType = kHFSFileThreadRecord;
		rec->parentID = key->parentID;
		bcopy(key->nodeName, rec->nodeName, key->nodeName[0]+1);
	}
	
	return (size);
	
} /* BuildThreadRec */

