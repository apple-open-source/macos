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
	File:		SVerify2.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.
*/

#include "BTree.h"
#include "BTreePrivate.h"

#include "Scavenger.h"

//	Prototypes for internal subroutines
static int BTKeyChk( SGlobPtr GPtr, NodeDescPtr nodeP, BTreeControlBlock *btcb );

		

/*------------------------------------------------------------------------------

Routine:	ChkExtRec (Check Extent Record)

Function:	Checks out a generic extent record.
			
Input:		GPtr		-	pointer to scavenger global area.
			extP		-	pointer to extent data record.
			
Output:		ChkExtRec	-	function result:			
								0 = no error
								n = error
------------------------------------------------------------------------------*/
OSErr ChkExtRec ( SGlobPtr GPtr, const void *extents )
{
	short		i;
	UInt32		numABlks;
	UInt32		maxNABlks;
	UInt32		extentBlockCount;
	UInt32		extentStartBlock;

	maxNABlks = GPtr->calculatedVCB->vcbTotalBlocks;
	numABlks = 1;

	for ( i=0 ; i<GPtr->numExtents ; i++ )
	{
		if ( GPtr->isHFSPlus )
		{
			extentBlockCount = ((HFSPlusExtentDescriptor *)extents)[i].blockCount;
			extentStartBlock = ((HFSPlusExtentDescriptor *)extents)[i].startBlock;
		}
		else
		{
			extentBlockCount = ((HFSExtentDescriptor *)extents)[i].blockCount;
			extentStartBlock = ((HFSExtentDescriptor *)extents)[i].startBlock;
		}
		
		if ( extentStartBlock >= maxNABlks )
		{
			RcdError( GPtr, E_ExtEnt );
			return( E_ExtEnt );
		}
		if ( extentBlockCount >= maxNABlks )
		{
			RcdError( GPtr, E_ExtEnt );
			return( E_ExtEnt );
		}			
		if ( numABlks == 0 )
		{
			if ( extentBlockCount != 0 )
			{
				RcdError( GPtr, E_ExtEnt );
				return( E_ExtEnt );
			}
		}
		numABlks = extentBlockCount;
	}
	
	return( noErr );
	
}


/*------------------------------------------------------------------------------

Routine:	BTCheck - (BTree Check)

Function:	Checks out the internal structure of a Btree file.  The BTree 
		structure is enunumerated top down starting from the root node.
			
Input:		GPtr		-	pointer to scavenger global area
			realRefNum		-	file refnum

Output:		BTCheck	-	function result:			
		0	= no error
		n 	= error code
------------------------------------------------------------------------------*/

int
BTCheck(SGlobPtr GPtr, short refNum, CheckLeafRecordProcPtr checkLeafRecord)
{
	OSErr			result;
	short			i;
	short			keyLen;
	UInt32			nodeNum;
	short			numRecs;
	short			index;
	UInt16			recSize;
	UInt8			parKey[ kMaxKeyLength + 2 + 2 ];
	UInt8			*dataPtr;
	STPR			*tprP;
	STPR			*parentP;
	KeyPtr			keyPtr;
	BTHeaderRec		*header;
	NodeRec			node;
	NodeDescPtr		nodeDescP;
	UInt16			*statusFlag;
	UInt32			leafRecords = 0;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( refNum );

	//	Set up
	if ( refNum == kCalculatedCatalogRefNum )
		statusFlag	= &(GPtr->CBTStat);
	else if ( refNum == kCalculatedExtentRefNum )
		statusFlag	= &(GPtr->EBTStat);
	else
		statusFlag	= &(GPtr->ABTStat);

	GPtr->TarBlock = 0;
	node.buffer = NULL;

	/*
	 * Check out BTree header node 
	 */
	result = GetNode( calculatedBTCB, kHeaderNodeNum, &node );
	if ( result != noErr )
	{
		if ( result == fsBTInvalidNodeErr )	/* CheckNode failed */
		{
			RcdError( GPtr, E_BadNode );
			result	= E_BadNode;
		}
		node.buffer = NULL;
		goto exit;
	}

	nodeDescP = node.buffer;

	result = AllocBTN( GPtr, refNum, 0 );
	if (result) goto exit;	/* node already allocated */
	
	if ( nodeDescP->kind != kBTHeaderNode )
	{
		RcdError( GPtr, E_BadHdrN );
		result = E_BadHdrN;
		goto exit;
	}	
	if ( nodeDescP->numRecords != Num_HRecs )
	{
		RcdError( GPtr, E_BadHdrN );
		result = E_BadHdrN;
		goto exit;
	}	
	if ( nodeDescP->height != 0 )
	{
		RcdError( GPtr, E_NHeight );
		result = E_NHeight;
		goto exit;
	}

	/*		
	 * check out BTree Header record
	 */
	header = (BTHeaderRec*) ((Byte*)nodeDescP + sizeof(BTNodeDescriptor));
	recSize = GetRecordSize( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, 0 );	
	
	if ( recSize != sizeof(BTHeaderRec) )
	{
		RcdError( GPtr, E_LenBTH );
		result = E_LenBTH;
		goto exit;
	}
	if ( header->treeDepth > BTMaxDepth )
	{
		RcdError( GPtr, E_BTDepth );
		result = E_BTDepth;
		goto exit;
	}
	calculatedBTCB->treeDepth = header->treeDepth;
	
	if ((header->rootNode < 0) || (header->rootNode >= calculatedBTCB->totalNodes))
	{
		RcdError( GPtr, E_BTRoot );
		result = E_BTRoot;
		goto exit;
	}
	calculatedBTCB->rootNode = header->rootNode;

	if ( (calculatedBTCB->treeDepth == 0) || (calculatedBTCB->rootNode == 0) )
	{
		if ( calculatedBTCB->treeDepth == calculatedBTCB->rootNode )
		{
			goto exit;	/* empty BTree */
		}
		else
		{
			RcdError( GPtr, E_BTDepth );
			result = E_BTDepth;;	/* depth doesn't agree with root */
			goto exit;
		}
	}		
		
	/*
	 * Set up tree path record for root level
	 */
 	GPtr->BTLevel	= 1;
	tprP		= &(*GPtr->BTPTPtr)[0];
	tprP->TPRNodeN	= calculatedBTCB->rootNode;
	tprP->TPRRIndx	= -1;
	tprP->TPRLtSib	= 0;
	tprP->TPRRtSib	= 0;
	parKey[0]	= 0;
		
	/*
	 * Now enumerate the entire BTree
	 */
	while ( GPtr->BTLevel > 0 )
	{
		tprP	= &(*GPtr->BTPTPtr)[GPtr->BTLevel -1];
		nodeNum	= tprP->TPRNodeN;
		index	= tprP->TPRRIndx;

		GPtr->TarBlock = nodeNum;

		(void) ReleaseNode(calculatedBTCB, &node);
		result = GetNode( calculatedBTCB, nodeNum, &node );
		if ( result != noErr )
		{
			if ( result == fsBTInvalidNodeErr )	/* CheckNode failed */
			{
				RcdError( GPtr, E_BadNode );
				result	= E_BadNode;
			}
			node.buffer = NULL;
			goto exit;
		}
		nodeDescP = node.buffer;
			
		/*
		 * Check out and allocate the node if its the first time its been seen
		 */		
		if ( index < 0 )
		{
			result = AllocBTN( GPtr, refNum, nodeNum );
			if (result) goto exit;	/* node already allocated? */
				
			result = BTKeyChk( GPtr, nodeDescP, calculatedBTCB );
			if (result) goto exit;
				
			if ( nodeDescP->bLink != tprP->TPRLtSib )
			{
				RcdError( GPtr, E_SibLk );
				result = E_SibLk;
				goto exit;
			}	
			if ( tprP->TPRRtSib == -1 )
			{
				tprP->TPRRtSib = nodeNum;	/* set Rt sibling for later verification */		
			}
			else
			{
				if ( nodeDescP->fLink != tprP->TPRRtSib )
				{				
					RcdError( GPtr, E_SibLk );
					result = E_SibLk;
					goto exit;
				}
			}
			
			if ( (nodeDescP->kind != kBTIndexNode) && (nodeDescP->kind != kBTLeafNode) )
			{
				*statusFlag |= S_RebuildBTree;
				RcdError( GPtr, E_NType );
				result		=  noErr;
			}	
			if ( nodeDescP->height != calculatedBTCB->treeDepth - GPtr->BTLevel + 1 )
			{
				*statusFlag |= S_RebuildBTree;
				RcdError( GPtr, E_NHeight );
				result		=  noErr;
			}
				
			if ( parKey[0] != 0 )
			{
				GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, 0, &keyPtr, &dataPtr, &recSize );
				if ( CompareKeys( (BTreeControlBlockPtr)calculatedBTCB, (BTreeKey *)parKey, keyPtr ) != 0 )
				{
					*statusFlag |= S_RebuildBTree;
					RcdError( GPtr, E_IKey );
				}
			}
			if ( nodeDescP->kind == kBTIndexNode )
 			{
				if ( result = CheckForStop( GPtr ) )
					goto exit;
			}
			
			GPtr->itemsProcessed++;
		}
		
		numRecs = nodeDescP->numRecords;
	
		/* 
		 * for an index node ...
		 */
		if ( nodeDescP->kind == kBTIndexNode )
		{
			index++;	/* on to next index record */
			if ( index >= numRecs )
			{
				GPtr->BTLevel--;
				continue;	/* No more records */
			}
			
			tprP->TPRRIndx	= index;
			parentP			= tprP;
			GPtr->BTLevel++;

			if ( GPtr->BTLevel > BTMaxDepth )
			{
				*statusFlag |= S_RebuildBTree;
				RcdError( GPtr, E_BTDepth );
				result		=  noErr;
			}				
			tprP = &(*GPtr->BTPTPtr)[GPtr->BTLevel -1];
			
			GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, index, &keyPtr, &dataPtr, &recSize );
			
			nodeNum = *(UInt32*)dataPtr;
			if ( (nodeNum <= 0) || (nodeNum >= calculatedBTCB->totalNodes) )
			{
				RcdError( GPtr, E_IndxLk );
				result = E_IndxLk;
				goto exit;
			}	
			/* 
			 * Make a copy of the parent's key so we can compare it
			 * with the child's key later.
			 */
			keyLen = ( calculatedBTCB->attributes & kBTBigKeysMask )
						? keyPtr->length16 + sizeof(UInt16)
						: keyPtr->length8 + sizeof(UInt8);
			CopyMemory(keyPtr, parKey, keyLen);
				
			tprP->TPRNodeN = nodeNum;
			tprP->TPRRIndx = -1;
			tprP->TPRLtSib = 0;	/* left sibling */
			
			if ( index > 0 )
			{
				GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, index-1, &keyPtr, &dataPtr, &recSize );

				nodeNum = *(UInt32*)dataPtr;
				if ( (nodeNum <= 0) || (nodeNum >= calculatedBTCB->totalNodes) )
				{
					*statusFlag |= S_RebuildBTree;
					RcdError( GPtr, E_IndxLk );
					result =  noErr;	/* FLASHING */
				}
				tprP->TPRLtSib = nodeNum;
			}
			else
			{
				if ( parentP->TPRLtSib != 0 )
					tprP->TPRLtSib = tprP->TPRRtSib;	/* Fill in the missing link */
			}
				
			tprP->TPRRtSib = 0;	/* right sibling */
			if ( index < (numRecs -1) )
			{
				GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, index+1, &keyPtr, &dataPtr, &recSize );
				nodeNum = *(UInt32*)dataPtr;
				if ( (nodeNum <= 0) || (nodeNum >= calculatedBTCB->totalNodes) )
				{
					*statusFlag |= S_RebuildBTree;
					RcdError( GPtr, E_IndxLk );
					result =  noErr;	/* FLASHING */
				}
				tprP->TPRRtSib = nodeNum;
			}
			else
			{
				if ( parentP->TPRRtSib != 0 )
					tprP->TPRRtSib = -1;	/* Link to be filled in later */
			}
		}			
	
		/*
		 * For a leaf node ...
		 */
		else
		{
			if ( tprP->TPRLtSib == 0 )
				calculatedBTCB->firstLeafNode = nodeNum;
			if ( tprP->TPRRtSib == 0 )
				calculatedBTCB->lastLeafNode = nodeNum;
			leafRecords	+= nodeDescP->numRecords;
		

			if (checkLeafRecord != NULL) {
				for (i = 0; i < nodeDescP->numRecords; i++) {
					GetRecordByIndex(calculatedBTCB, nodeDescP, i, &keyPtr, &dataPtr, &recSize);
					result = checkLeafRecord(keyPtr, dataPtr, recSize);
					if (result) goto exit;
				}
			}
			GPtr->BTLevel--;
			continue;
		}		
	} /* end while */

	calculatedBTCB->leafRecords = leafRecords;

exit:
	if (node.buffer != NULL)
		(void) ReleaseNode(calculatedBTCB, &node);
	
	return( result );

} /* end of BTCheck */



/*------------------------------------------------------------------------------

Routine:	BTMapChk - (BTree Map Check)

Function:	Checks out the structure of a BTree allocation map.
			
Input:		GPtr		- pointer to scavenger global area
		fileRefNum	- refnum of BTree file

Output:		BTMapChk	- function result:			
			0 = no error
			n = error
------------------------------------------------------------------------------*/

int BTMapChk( SGlobPtr GPtr, short fileRefNum )
{
	OSErr				result;
	UInt16				recSize;
	SInt32				mapSize;
	UInt32				nodeNum;
	SInt16				recIndx;
	NodeRec				node;
	NodeDescPtr			nodeDescP;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( fileRefNum );

	result = noErr;
	nodeNum	= 0;	/* Start with header node */
	node.buffer = NULL;
	recIndx	= 2;	
	mapSize	= ( calculatedBTCB->totalNodes + 7 ) / 8;	/* size in bytes */

	/*
	 * Enumerate the map structure starting with the map record in the header node
	 */
	while ( mapSize > 0 )
	{
		GPtr->TarBlock = nodeNum;
		
		if (node.buffer != NULL)
			(void) ReleaseNode(calculatedBTCB, &node);
		result = GetNode( calculatedBTCB, nodeNum, &node );
		if ( result != noErr )
		{
			if ( result == fsBTInvalidNodeErr )	/* CheckNode failed */
			{
				RcdError( GPtr, E_BadNode );
				result	= E_BadNode;
			}
			return( result );
		}
		
		nodeDescP = node.buffer;
		
		/* Check out the node if its not the header node */	

		if ( nodeNum != 0 )
		{
			result = AllocBTN( GPtr, fileRefNum, nodeNum );
			if (result) goto exit;	/* Error, node already allocated? */
				
			if ( nodeDescP->kind != kBTMapNode )
			{
				RcdError( GPtr, E_BadMapN );
				result = E_BadMapN;
				goto exit;
			}	
			if ( nodeDescP->numRecords != Num_MRecs )
			{
				RcdError( GPtr, E_BadMapN );
				result = E_BadMapN;
				goto exit;
			}	
			if ( nodeDescP->height != 0 )
				RcdError( GPtr, E_NHeight );
		}

		//	Move on to the next map node
		recSize  = GetRecordSize( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, recIndx );
		mapSize -= recSize;	/* Adjust remaining map size */

		recIndx	= 0;	/* Map record is now record 0 */	
		nodeNum	= nodeDescP->fLink;						
		if (nodeNum == 0)
			break;
	
	} /* end while */


	if ( (nodeNum != 0) || (mapSize > 0) )
	{
		RcdError( GPtr, E_MapLk);
		result = E_MapLk;	/* bad map node linkage */
	}
exit:
	if (node.buffer != NULL)
		(void) ReleaseNode(calculatedBTCB, &node);
	
	return( result );
	
} /* end BTMapChk */



/*------------------------------------------------------------------------------

Routine:	CmpBTH - (Compare BTree Header)

Function:	Compares the scavenger BTH info with the BTH on disk.
			
Input:		GPtr - pointer to scavenger global area
		fileRefNum - file refnum

Output:		CmpBTH - function result:			
			0 = no error
			n = error
------------------------------------------------------------------------------*/

OSErr	CmpBTH( SGlobPtr GPtr, SInt16 fileRefNum )
{
	OSErr err;
	BTHeaderRec bTreeHeader;
	BTreeControlBlock *calculatedBTCB = GetBTreeControlBlock( fileRefNum );
	SInt16 *statP;
	SFCB * fcb;

	if (fileRefNum == kCalculatedCatalogRefNum) {
		statP = (SInt16 *)&GPtr->CBTStat;
		fcb = GPtr->calculatedCatalogFCB;
	} else {
		statP = (SInt16 *)&GPtr->EBTStat;
		fcb = GPtr->calculatedExtentsFCB;
	}

	/* 
	 * Get BTree header record from disk 
	 */
	GPtr->TarBlock = 0;	//	Set target node number

	err = GetBTreeHeader(GPtr, fcb, &bTreeHeader );
	ReturnIfError( err );

	if (calculatedBTCB->leafRecords != bTreeHeader.leafRecords) {
		char goodStr[32], badStr[32];

		*statP = *statP | S_BTH;
		PrintError(GPtr, E_LeafCnt, 0);
		sprintf(goodStr, "%ld", (long)calculatedBTCB->leafRecords);
		sprintf(badStr, "%ld", (long)bTreeHeader.leafRecords);
		PrintError(GPtr, E_BadValue, 2, goodStr, badStr);
	} else if (
	    (calculatedBTCB->treeDepth	   != bTreeHeader.treeDepth)	 ||
	    (calculatedBTCB->rootNode	   != bTreeHeader.rootNode)	 ||
	    (calculatedBTCB->firstLeafNode != bTreeHeader.firstLeafNode) ||
	    (calculatedBTCB->lastLeafNode  != bTreeHeader.lastLeafNode)  ||
	    (calculatedBTCB->nodeSize	   != bTreeHeader.nodeSize)	 ||
	    (calculatedBTCB->maxKeyLength  != bTreeHeader.maxKeyLength)  ||
	    (calculatedBTCB->totalNodes    != bTreeHeader.totalNodes)	 ||
	    (calculatedBTCB->freeNodes	   != bTreeHeader.freeNodes) ) {

		*statP = *statP | S_BTH;
		PrintError(GPtr, E_InvalidBTreeHeader, 0);
	}
	
	return( noErr );
}



/*------------------------------------------------------------------------------

Routine:	CmpBlock

Function:	Compares two data blocks for equality.
			
Input:		Blk1Ptr		-	pointer to 1st data block.
			Blk2Ptr		-	pointer to 2nd data block.
			len			-	size of the blocks (in bytes)

Output:		CmpBlock	-	result code	
			0 = equal
			1 = not equal
------------------------------------------------------------------------------*/

OSErr	CmpBlock( void *block1P, void *block2P, UInt32 length )
{
	Byte	*blk1Ptr = block1P;
	Byte	*blk2Ptr = block2P;

	while ( length-- ) 
		if ( *blk1Ptr++ != *blk2Ptr++ )
			return( -1 );
	
	return( noErr );
	
}



/*------------------------------------------------------------------------------

Routine:	CmpBTM - (Compare BTree Map)

Function:	Compares the scavenger BTM with the BTM on disk.
			
Input:		GPtr		-	pointer to scavenger global area
			fileRefNum		-	file refnum

Output:		CmpBTM	-	function result:			
								0	= no error
								n 	= error
------------------------------------------------------------------------------*/

int CmpBTM( SGlobPtr GPtr, short fileRefNum )
{
	OSErr			result;
	UInt16			recSize;
	short			mapSize;
	short			size;
	UInt32			nodeNum;
	short			recIndx;
	char			*p;
	char			*sbtmP;
	UInt8 *			dataPtr;
	NodeRec			node;
	NodeDescPtr		nodeDescP;
	BTreeControlBlock	*calculatedBTCB;
	SInt16			*statP;

	result = noErr;
	calculatedBTCB	= GetBTreeControlBlock( fileRefNum );
	statP = (SInt16 *)((fileRefNum == kCalculatedCatalogRefNum) ? &GPtr->CBTStat : &GPtr->EBTStat);

	nodeNum	= 0;	/* start with header node */
	node.buffer = NULL;
	recIndx	= 2;
	recSize = size = 0;
	mapSize	= (calculatedBTCB->totalNodes + 7) / 8;		/* size in bytes */
	sbtmP	= ((BTreeExtensionsRec*)calculatedBTCB->refCon)->BTCBMPtr;
	dataPtr = NULL;

	/*
	 * Enumerate BTree map records starting with map record in header node
	 */
	while ( mapSize > 0 )
	{
		GPtr->TarBlock = nodeNum;
		
		if (node.buffer != NULL)	
			(void) ReleaseNode(calculatedBTCB, &node);

		result = GetNode( calculatedBTCB, nodeNum, &node );
		if (result) goto exit;	/* error, could't get map node */

		nodeDescP = node.buffer;

		recSize = GetRecordSize( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, recIndx );
		dataPtr = GetRecordAddress( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, recIndx );
	
		size	= ( recSize  > mapSize ) ? mapSize : recSize;
			
		result = CmpBlock( sbtmP, dataPtr, size );
		if ( result != noErr )
		{ 	
			*statP = *statP | S_BTM;	/* didn't match, mark it damaged */
			goto exit;
		}
	
		recIndx	= 0;			/* map record is now record 0 */			
		mapSize	-= size;		/* adjust remaining map size */
		sbtmP	= sbtmP + size;
		nodeNum	= nodeDescP->fLink;	/* next node number */						
		if (nodeNum == 0)
			break;
	
	} /* end while */

	/* 
	 * Make sure the unused portion of the last map record is zero
	 */
	for ( p = (Ptr)dataPtr + size ; p < (Ptr)dataPtr + recSize ; p++ )
		if ( *p != 0 ) 
			*statP	= *statP | S_BTM;	/* didn't match, mark it damaged */

exit:
	if (node.buffer != NULL)	
		(void) ReleaseNode(calculatedBTCB, &node);

	return( result );
	
} /* end CmpBTM */


/*------------------------------------------------------------------------------

Routine:	BTKeyChk - (BTree Key Check)

Function:	Checks out the key structure within a Btree node.
			
Input:		GPtr		-	pointer to scavenger global area
		NodePtr		-	pointer to target node
		BTCBPtr		-	pointer to BTreeControlBlock

Output:		BTKeyChk	-	function result:			
			0 = no error
			n = error code
------------------------------------------------------------------------------*/
extern HFSPlusCatalogKey gMetaDataDirKey;

static int BTKeyChk( SGlobPtr GPtr, NodeDescPtr nodeP, BTreeControlBlock *btcb )
{
	SInt16				index;
	UInt16				dataSize;
	UInt16				keyLength;
	KeyPtr 				keyPtr;
	UInt8				*dataPtr;
	KeyPtr				prevkeyP	= nil;


	if ( nodeP->numRecords == 0 )
	{
		if ( (nodeP->fLink == 0) && (nodeP->bLink == 0) )
		{
			RcdError( GPtr, E_BadNode );
			return( E_BadNode );
		}
	}
	else
	{
		/*
		 * Loop on number of records in node
		 */
		for ( index = 0; index < nodeP->numRecords; index++)
		{
			GetRecordByIndex( (BTreeControlBlock *)btcb, nodeP, (UInt16) index, &keyPtr, &dataPtr, &dataSize );
	
			if (btcb->attributes & kBTBigKeysMask)
				keyLength = keyPtr->length16;
			else
				keyLength = keyPtr->length8;
				
			if ( keyLength > btcb->maxKeyLength )
			{
				RcdError( GPtr, E_KeyLen );
				return( E_KeyLen );
			}
	
			if ( prevkeyP != nil )
			{
				if ( CompareKeys( (BTreeControlBlockPtr)btcb, prevkeyP, keyPtr ) >= 0 )
				{
					/*
					 * When the HFS+ MetaDataDirKey is out of order we mark
					 * the result code so that it can be deleted later.
					 */
					if ((btcb->maxKeyLength == kHFSPlusCatalogKeyMaximumLength)  &&
					    (CompareKeys(btcb, prevkeyP, (KeyPtr)&gMetaDataDirKey) == 0))
					{
						if (GPtr->logLevel > 0)
							printf("Problem: b-tree key for \"HFS+ Private Data\" directory is out of order.\n");
						return( E_KeyOrd + 1000 );
					} 
					else
					{
						RcdError( GPtr, E_KeyOrd );
						return( E_KeyOrd );
					}
				}
			}
			prevkeyP = keyPtr;
		}
	}

	return( noErr );
}



/*------------------------------------------------------------------------------

Routine:	ChkCName (Check Catalog Name)

Function:	Checks out a generic catalog name.
			
Input:		GPtr		-	pointer to scavenger global area.
		CNamePtr	-	pointer to CName.
			
Output:		ChkCName	-	function result:			
					0 = CName is OK
					E_CName = invalid CName
------------------------------------------------------------------------------*/

OSErr ChkCName( SGlobPtr GPtr, const CatalogName *name, Boolean unicode )
{
	UInt32	length;
	OSErr	err		= noErr;
	
	length = CatalogNameLength( name, unicode );
	
	if ( unicode )
	{
		if ( (length == 0) || (length > kHFSPlusMaxFileNameChars) )
			err = E_CName;
	}
	else
	{
		if ( (length == 0) || (length > kHFSMaxFileNameChars) )
			err = E_CName;
	}
	
	return( err );
}


/*------------------------------------------------------------------------------

Routine:	CmpMDB - (Compare Master Directory Block)

Function:	Compares the scavenger MDB info with the MDB on disk.
			
Input:		GPtr			-	pointer to scavenger global area

Output:		CmpMDB			- 	function result:
									0 = no error
									n = error
			GPtr->VIStat	-	S_MDB flag set in VIStat if MDB is damaged.
------------------------------------------------------------------------------*/

int CmpMDB( SGlobPtr GPtr,  HFSMasterDirectoryBlock * mdbP)
{
	short					i;
	SFCB *  fcbP;
	SVCB *  vcb;

	//	Set up
	GPtr->TarID = MDB_FNum;
	vcb = GPtr->calculatedVCB;
	
	/* 
	 * compare VCB info with MDB
	 */
	if ( mdbP->drSigWord	!= vcb->vcbSignature )		goto MDBDamaged;
	if ( mdbP->drCrDate	!= vcb->vcbCreateDate )		goto MDBDamaged;
	if ( mdbP->drLsMod	!= vcb->vcbModifyDate )		goto MDBDamaged;
	if ( mdbP->drAtrb	!= (UInt16)vcb->vcbAttributes )	goto MDBDamaged;
	if ( mdbP->drVBMSt	!= vcb->vcbVBMSt )			goto MDBDamaged;
	if ( mdbP->drNmAlBlks	!= vcb->vcbTotalBlocks )		goto MDBDamaged;
	if ( mdbP->drClpSiz	!= vcb->vcbDataClumpSize )		goto MDBDamaged;
	if ( mdbP->drAlBlSt	!= vcb->vcbAlBlSt )			goto MDBDamaged;
	if ( mdbP->drNxtCNID	!= vcb->vcbNextCatalogID )		goto MDBDamaged;
	if ( CmpBlock( mdbP->drVN, vcb->vcbVN, mdbP->drVN[0]+1 ) )	goto MDBDamaged;
	goto ContinueChecking;

MDBDamaged:
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 1, 0 );
		return( noErr );
	}
	
ContinueChecking:
	if ((mdbP->drVolBkUp	!= vcb->vcbBackupDate)		||
	    (mdbP->drVSeqNum	!= vcb->vcbVSeqNum)			||
	    (mdbP->drWrCnt	!= vcb->vcbWriteCount)		||
	    (mdbP->drXTClpSiz	!= vcb->vcbExtentsFile->fcbClumpSize)	||
	    (mdbP->drCTClpSiz	!= vcb->vcbCatalogFile->fcbClumpSize)	||
	    (mdbP->drNmRtDirs	!= vcb->vcbNmRtDirs)			||
	    (mdbP->drFilCnt	!= vcb->vcbFileCount)			||
	    (mdbP->drDirCnt	!= vcb->vcbFolderCount)		||
	    (CmpBlock(mdbP->drFndrInfo, vcb->vcbFinderInfo, 32 ))	//||
	//  (mdbP->drEmbedSigWord		!= vcb->vcbEmbedSigWord)		||
	//  (mdbP->drEmbedExtent.startBlock	!= vcb->vcbEmbedExtent.startBlock)	||
	//  (mdbP->drEmbedExtent.blockCount	!= vcb->vcbEmbedExtent.blockCount)
	   )
	{ 
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 2, 0 );
		return( noErr );
	}

	/* 
	 * compare extent file allocation info with MDB
	 */
	fcbP = vcb->vcbExtentsFile;	/* compare PEOF for extent file */
	if ( mdbP->drXTFlSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 3, 0 );
		return( noErr );
	}
	for ( i = 0; i < GPtr->numExtents; i++ )
	{
		if ( (mdbP->drXTExtRec[i].startBlock != fcbP->fcbExtents16[i].startBlock) ||
		     (mdbP->drXTExtRec[i].blockCount != fcbP->fcbExtents16[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_MDBDamaged, 4, 0 );
			return( noErr );
		}
	}

	/*
	 * compare catalog file allocation info with MDB
	 */		
	fcbP = vcb->vcbCatalogFile;	/* compare PEOF for catalog file */
	if ( mdbP->drCTFlSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 5, 0 );
		return( noErr );
	}
	for ( i = 0; i < GPtr->numExtents; i++ )
	{
		if ( (mdbP->drCTExtRec[i].startBlock != fcbP->fcbExtents16[i].startBlock) ||
		     (mdbP->drCTExtRec[i].blockCount != fcbP->fcbExtents16[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_MDBDamaged, 6, 0 );
			return( noErr );
		}
	}
	
	return( noErr );
} /* end CmpMDB */



/*------------------------------------------------------------------------------

Routine:	CompareVolumeHeader - (Compare VolumeHeader Block)

Function:	Compares the scavenger VolumeHeader info with the VolumeHeader on disk.
			
Input:		GPtr			-	pointer to scavenger global area

Output:		CmpMDB			- 	function result:
									0 = no error
									n = error
			GPtr->VIStat	-	S_MDB flag set in VIStat if MDB is damaged.
------------------------------------------------------------------------------*/

OSErr CompareVolumeHeader( SGlobPtr GPtr, HFSPlusVolumeHeader *volumeHeader)
{
	SInt16					i;
	SVCB				*vcb;
	SFCB						*fcbP;
	UInt32					hfsPlusIOPosOffset;
	UInt32 goodValue, badValue;
	short errID;

	vcb = GPtr->calculatedVCB;
	GPtr->TarID = MDB_FNum;
	
	hfsPlusIOPosOffset = vcb->vcbEmbeddedOffset;

	errID = 0;
	goodValue = badValue = 0;
	if (volumeHeader->fileCount != vcb->vcbFileCount) {
		errID = E_FilCnt;
		goodValue = vcb->vcbFileCount;
		badValue = volumeHeader->fileCount;
	}
	if (volumeHeader->folderCount != vcb->vcbFolderCount) {
		errID = E_DirCnt;
		goodValue = vcb->vcbFolderCount;
		badValue = volumeHeader->folderCount;
	}
	if (volumeHeader->freeBlocks != vcb->vcbFreeBlocks) {
		errID = E_FreeBlocks;
		goodValue = vcb->vcbFreeBlocks;
		badValue = volumeHeader->freeBlocks;
	}

	if (errID) {
		char goodStr[32], badStr[32];

		PrintError(GPtr, errID, 0);
		sprintf(goodStr, "%d", goodValue);
		sprintf(badStr, "%d", badValue);
		PrintError(GPtr, E_BadValue, 2, goodStr, badStr);
		goto VolumeHeaderDamaged;
	}

	if ( kHFSPlusSigWord				!= vcb->vcbSignature )	goto VolumeHeaderDamaged;
	if ( volumeHeader->encodingsBitmap		!= vcb->vcbEncodingsBitmap )	goto VolumeHeaderDamaged;
	if ( (SInt16) (hfsPlusIOPosOffset/512)		!= vcb->vcbAlBlSt )		goto VolumeHeaderDamaged;
	if ( volumeHeader->createDate			!= vcb->vcbCreateDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->modifyDate			!= vcb->vcbModifyDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->backupDate			!= vcb->vcbBackupDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->checkedDate			!= vcb->vcbCheckedDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->rsrcClumpSize		!= vcb->vcbRsrcClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->dataClumpSize		!= vcb->vcbDataClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->nextCatalogID		!= vcb->vcbNextCatalogID &&
	     (volumeHeader->attributes & kHFSCatalogNodeIDsReused) == 0)  goto VolumeHeaderDamaged;
	if ( volumeHeader->writeCount			!= vcb->vcbWriteCount )	goto VolumeHeaderDamaged;
	if ( volumeHeader->nextAllocation		!= vcb->vcbNextAllocation )	goto VolumeHeaderDamaged;
	if ( volumeHeader->totalBlocks			!= vcb->vcbTotalBlocks )	goto VolumeHeaderDamaged;
	if ( volumeHeader->blockSize			!= vcb->vcbBlockSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->attributes			!= vcb->vcbAttributes )	goto VolumeHeaderDamaged;

	if ( volumeHeader->extentsFile.clumpSize	!= vcb->vcbExtentsFile->fcbClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->catalogFile.clumpSize	!= vcb->vcbCatalogFile->fcbClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->allocationFile.clumpSize	!= vcb->vcbAllocationFile->fcbClumpSize )	goto VolumeHeaderDamaged;

	if ( CmpBlock( volumeHeader->finderInfo, vcb->vcbFinderInfo, sizeof(vcb->vcbFinderInfo) ) )	goto VolumeHeaderDamaged;
	goto ContinueChecking;
	
		
VolumeHeaderDamaged:
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		if (errID == 0)
			WriteError ( GPtr, E_VolumeHeaderDamaged, 1, 0 );
		return( noErr );

ContinueChecking:

	/*
	 * compare extent file allocation info with VolumeHeader
	 */		
	fcbP = vcb->vcbExtentsFile;
	if ( volumeHeader->extentsFile.totalBlocks * vcb->vcbBlockSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_VolumeHeaderDamaged, 3, 0 );
		return( noErr );
	}
	for ( i=0; i < GPtr->numExtents; i++ )
	{
		if ( (volumeHeader->extentsFile.extents[i].startBlock != fcbP->fcbExtents32[i].startBlock) ||
		     (volumeHeader->extentsFile.extents[i].blockCount != fcbP->fcbExtents32[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_VolumeHeaderDamaged, 4, 0 );
			return( noErr );
		}
	}

	/*
	 * compare catalog file allocation info with MDB
	 */	
	fcbP = vcb->vcbCatalogFile;	/* compare PEOF for catalog file */
	if ( volumeHeader->catalogFile.totalBlocks * vcb->vcbBlockSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_VolumeHeaderDamaged, 5, 0 );
		return( noErr );
	}
	for ( i=0; i < GPtr->numExtents; i++ )
	{
		if ( (volumeHeader->catalogFile.extents[i].startBlock != fcbP->fcbExtents32[i].startBlock) ||
		     (volumeHeader->catalogFile.extents[i].blockCount != fcbP->fcbExtents32[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_VolumeHeaderDamaged, 6, 0 );
			return( noErr );
		}
	}


	/*
	 * compare bitmap file allocation info with MDB
	 */		
	fcbP = vcb->vcbAllocationFile;
	if ( volumeHeader->allocationFile.totalBlocks * vcb->vcbBlockSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_VolumeHeaderDamaged, 7, 0 );
		return( noErr );
	}
	for ( i=0; i < GPtr->numExtents; i++ )
	{
		if ( (volumeHeader->allocationFile.extents[i].startBlock != fcbP->fcbExtents32[i].startBlock) ||
		     (volumeHeader->allocationFile.extents[i].blockCount != fcbP->fcbExtents32[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;				/* didn't match, mark MDB damaged */
			WriteError ( GPtr, E_VolumeHeaderDamaged, 8, 0 );
			return( noErr );
		}
	}

	return( noErr );
}



