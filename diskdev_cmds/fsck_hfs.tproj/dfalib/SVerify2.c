/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
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
/*
	File:		SVerify2.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.
*/

#include <sys/ioctl.h>
#include <sys/disk.h>

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
	Boolean		isHFSPlus;
	UInt32		numABlks;
	UInt32		maxNABlks;
	UInt32		extentBlockCount;
	UInt32		extentStartBlock;

	maxNABlks = GPtr->calculatedVCB->vcbTotalBlocks;
	numABlks = 1;
	isHFSPlus = VolumeObjectIsHFSPlus( );

	for ( i=0 ; i<GPtr->numExtents ; i++ )
	{
		if ( isHFSPlus )
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
		goto RebuildBTreeExit;
	}
	calculatedBTCB->treeDepth = header->treeDepth;
	
	if ( header->rootNode >= calculatedBTCB->totalNodes ||
		 (header->treeDepth != 0 && header->rootNode == kHeaderNodeNum) )
	{
		RcdError( GPtr, E_BTRoot );
		goto RebuildBTreeExit;
	}
	calculatedBTCB->rootNode = header->rootNode;

	if ( (calculatedBTCB->treeDepth == 0) || (calculatedBTCB->rootNode == 0) )
	{
		if ( calculatedBTCB->treeDepth == calculatedBTCB->rootNode )
			goto exit;	/* empty BTree */

		RcdError( GPtr, E_BTDepth );
		goto RebuildBTreeExit;
	}		

#if 0
	printf( "\nB-Tree header rec: \n" );
	printf( "    treeDepth     = %d \n", header->treeDepth );
	printf( "    rootNode      = %d \n", header->rootNode );
	printf( "    leafRecords   = %d \n", header->leafRecords );
	printf( "    firstLeafNode = %d \n", header->firstLeafNode );
	printf( "    lastLeafNode  = %d \n", header->lastLeafNode );
	printf( "    totalNodes    = %d \n", header->totalNodes );
	printf( "    freeNodes     = %d \n", header->freeNodes );
#endif
		
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
#if 0 //
			// this will print out our leaf node order
			if ( nodeDescP->kind == kBTLeafNode ) 
			{
				static int		myCounter = 0;
				if ( myCounter > 19 )
				{
					myCounter = 0;
					printf( "\n  " );
				}
				printf( "%d ", nodeNum );
				
				myCounter++;
			}
#endif

			result = AllocBTN( GPtr, refNum, nodeNum );
			if ( result ) 
			{
				/* node already allocated can be fixed if it is an index node */
				if ( nodeDescP->kind == kBTIndexNode )
					goto RebuildBTreeExit;	
				goto exit;
			}
				
			result = BTKeyChk( GPtr, nodeDescP, calculatedBTCB );
			if ( result ) 
			{
				/* we should be able to fix any E_KeyOrd error or any B-Tree key */
				/* errors with an index node. */
				if ( E_KeyOrd == result || nodeDescP->kind == kBTIndexNode )
					goto RebuildBTreeExit;	
				goto exit;
			}
				
			if ( nodeDescP->bLink != tprP->TPRLtSib )
			{
				RcdError( GPtr, E_SibLk );
				/* bad sibling link can be fixed if it is an index node */
				if ( nodeDescP->kind == kBTIndexNode )
					goto RebuildBTreeExit;	
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
					result = E_SibLk;
					/* bad sibling link can be fixed if it is an index node */
					if ( nodeDescP->kind == kBTIndexNode )
						goto RebuildBTreeExit;	
					goto exit;
				}
			}
			
			if ( (nodeDescP->kind != kBTIndexNode) && (nodeDescP->kind != kBTLeafNode) )
			{
				RcdError( GPtr, E_NType );
				goto exit;
			}	
			if ( nodeDescP->height != calculatedBTCB->treeDepth - GPtr->BTLevel + 1 )
			{
				RcdError( GPtr, E_NHeight );
				/* node height can be fixed if it is an index node */
				if ( nodeDescP->kind == kBTIndexNode )
					goto RebuildBTreeExit;	
				goto exit;
			}
				
			if ( parKey[0] != 0 )
			{
				GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, 0, &keyPtr, &dataPtr, &recSize );
				if ( CompareKeys( (BTreeControlBlockPtr)calculatedBTCB, (BTreeKey *)parKey, keyPtr ) != 0 )
				{
					RcdError( GPtr, E_IKey );
					goto RebuildBTreeExit;
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
				RcdError( GPtr, E_BTDepth );
				goto RebuildBTreeExit;
			}				
			tprP = &(*GPtr->BTPTPtr)[GPtr->BTLevel -1];
			
			GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, 
							  index, &keyPtr, &dataPtr, &recSize );
			
			nodeNum = *(UInt32*)dataPtr;
			if ( (nodeNum == kHeaderNodeNum) || (nodeNum >= calculatedBTCB->totalNodes) )
			{
				RcdError( GPtr, E_IndxLk );
				goto RebuildBTreeExit;
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
				if ( (nodeNum == kHeaderNodeNum) || (nodeNum >= calculatedBTCB->totalNodes) )
				{
					RcdError( GPtr, E_IndxLk );
					goto RebuildBTreeExit;
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
				if ( (nodeNum == kHeaderNodeNum) || (nodeNum >= calculatedBTCB->totalNodes) )
				{
					RcdError( GPtr, E_IndxLk );
					goto RebuildBTreeExit;
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

RebuildBTreeExit:
	/* force a B-Tree file rebuild */
	*statusFlag |= S_RebuildBTree;
	result = errRebuildBtree;
	goto exit;

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
        return( noErr );
	} 
    
	if ( calculatedBTCB->treeDepth != bTreeHeader.treeDepth ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid tree depth - calculated %d header %d \n", 
                    calculatedBTCB->treeDepth, bTreeHeader.treeDepth);
    } else if ( calculatedBTCB->rootNode != bTreeHeader.rootNode ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid root node - calculated %d header %d \n", 
                    calculatedBTCB->rootNode, bTreeHeader.rootNode);
    } else if ( calculatedBTCB->firstLeafNode != bTreeHeader.firstLeafNode ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid first leaf node - calculated %d header %d \n", 
                    calculatedBTCB->firstLeafNode, bTreeHeader.firstLeafNode);
    } else if ( calculatedBTCB->lastLeafNode != bTreeHeader.lastLeafNode ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid last leaf node - calculated %d header %d \n", 
                    calculatedBTCB->lastLeafNode, bTreeHeader.lastLeafNode);
    } else if ( calculatedBTCB->nodeSize != bTreeHeader.nodeSize ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid node size - calculated %d header %d \n", 
                    calculatedBTCB->nodeSize, bTreeHeader.nodeSize);
    } else if ( calculatedBTCB->maxKeyLength != bTreeHeader.maxKeyLength ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid max key length - calculated %d header %d \n", 
                    calculatedBTCB->maxKeyLength, bTreeHeader.maxKeyLength);
    } else if ( calculatedBTCB->totalNodes != bTreeHeader.totalNodes ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid total nodes - calculated %d header %d \n", 
                    calculatedBTCB->totalNodes, bTreeHeader.totalNodes);
    } else if ( calculatedBTCB->freeNodes != bTreeHeader.freeNodes ) {
        if ( GPtr->logLevel >= kDebugLog ) 
            printf("\tinvalid free nodes - calculated %d header %d \n", 
                    calculatedBTCB->freeNodes, bTreeHeader.freeNodes);
	} else
        return( noErr );

    *statP = *statP | S_BTH;
    PrintError(GPtr, E_InvalidBTreeHeader, 0);
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
	SInt32			mapSize;
	SInt32			size;
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
			RcdError(GPtr, E_BadMapN);
			result = 0;			/* mismatch isn't fatal; let us continue */
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
	if ( mdbP->drSigWord	!= vcb->vcbSignature ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drSigWord \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drCrDate	!= vcb->vcbCreateDate )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drCrDate \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drLsMod	!= vcb->vcbModifyDate )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drLsMod \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drAtrb	!= (UInt16)vcb->vcbAttributes )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drAtrb \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drVBMSt	!= vcb->vcbVBMSt )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drVBMSt \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drNmAlBlks	!= vcb->vcbTotalBlocks ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drNmAlBlks \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drClpSiz	!= vcb->vcbDataClumpSize )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drClpSiz \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drAlBlSt	!= vcb->vcbAlBlSt )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drAlBlSt \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drNxtCNID	!= vcb->vcbNextCatalogID )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drNxtCNID \n" );
		goto MDBDamaged;
	}	
	if ( CmpBlock( mdbP->drVN, vcb->vcbVN, mdbP->drVN[0]+1 ) )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drVN \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drVolBkUp	!= vcb->vcbBackupDate )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drVolBkUp \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drVSeqNum	!= vcb->vcbVSeqNum )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drVSeqNum \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drWrCnt	!= vcb->vcbWriteCount )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drWrCnt \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drXTClpSiz	!= vcb->vcbExtentsFile->fcbClumpSize )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drXTClpSiz \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drCTClpSiz	!= vcb->vcbCatalogFile->fcbClumpSize )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drCTClpSiz \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drNmRtDirs	!= vcb->vcbNmRtDirs )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drNmRtDirs \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drFilCnt	!= vcb->vcbFileCount )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drFilCnt \n" );
		goto MDBDamaged;
	}	
	if ( mdbP->drDirCnt	!= vcb->vcbFolderCount )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drDirCnt \n" );
		goto MDBDamaged;
	}	
	if ( CmpBlock(mdbP->drFndrInfo, vcb->vcbFinderInfo, 32 ) )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid MDB drFndrInfo \n" );
		goto MDBDamaged;
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
	
MDBDamaged:
	GPtr->VIStat = GPtr->VIStat | S_MDB;
	WriteError ( GPtr, E_MDBDamaged, 1, 0 );
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

OSErr CompareVolumeHeader( SGlobPtr GPtr, HFSPlusVolumeHeader *volumeHeader )
{
	SInt16			i;
	SVCB			*vcb;
	SFCB			*fcbP;
	UInt32			hfsPlusIOPosOffset;
	UInt32 			goodValue, badValue;
	int				isWriteable;
	short 			errID;

	vcb = GPtr->calculatedVCB;
	GPtr->TarID = MDB_FNum;
	
	hfsPlusIOPosOffset = vcb->vcbEmbeddedOffset;

	errID = 0;
	goodValue = badValue = 0;
	
	// CatHChk will flag valence errors and display the good and bad values for
	// our file and folder counts.  It will set S_Valence in CatStat when this
	// problem is detected.  We do NOT want to flag the error here in that case
	// since the volume header counts cannot be trusted and it will lead to 
	// confusing messages.
	if ( volumeHeader->fileCount != vcb->vcbFileCount && 
		 (GPtr->CatStat & S_Valence) == 0 ) {
		errID = E_FilCnt;
		goodValue = vcb->vcbFileCount;
		badValue = volumeHeader->fileCount;
	}
	if ( volumeHeader->folderCount != vcb->vcbFolderCount && 
		 (GPtr->CatStat & S_Valence) == 0 ) {
		errID = E_DirCnt;
		goodValue = vcb->vcbFolderCount;
		badValue = volumeHeader->folderCount;
	}
	if (volumeHeader->freeBlocks != vcb->vcbFreeBlocks) {
		errID = E_FreeBlocks;
		goodValue = vcb->vcbFreeBlocks;
		badValue = volumeHeader->freeBlocks;
	}
	
	/* 
	 * some Finder burned CDs will have very small clump sizes, but since 
	 * clump size for read-only media is irrelevant we skip the clump size 
	 * check to avoid non useful warnings. 
	 */
	isWriteable = 0;
	ioctl( GPtr->DrvNum, DKIOCISWRITABLE, &isWriteable );
	if ( isWriteable != 0 && 
		 volumeHeader->catalogFile.clumpSize != vcb->vcbCatalogFile->fcbClumpSize ) {
		errID = E_InvalidClumpSize;
		goodValue = vcb->vcbCatalogFile->fcbClumpSize;
		badValue = volumeHeader->catalogFile.clumpSize;
	}

	if (errID) {
		char goodStr[32], badStr[32];

		PrintError(GPtr, errID, 0);
		sprintf(goodStr, "%u", goodValue);
		sprintf(badStr, "%u", badValue);
		PrintError(GPtr, E_BadValue, 2, goodStr, badStr);
		goto VolumeHeaderDamaged;
	}

	if ( volumeHeader->signature != kHFSPlusSigWord  &&
	     volumeHeader->signature != kHFSXSigWord) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB signature \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->encodingsBitmap		!= vcb->vcbEncodingsBitmap )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB encodingsBitmap \n" );
		goto VolumeHeaderDamaged;
	}
	if ( (UInt16) (hfsPlusIOPosOffset/512)		!= vcb->vcbAlBlSt ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB AlBlSt \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->createDate			!= vcb->vcbCreateDate )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB createDate \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->modifyDate			!= vcb->vcbModifyDate )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB modifyDate \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->backupDate			!= vcb->vcbBackupDate )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB backupDate \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->checkedDate			!= vcb->vcbCheckedDate ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB checkedDate \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->rsrcClumpSize		!= vcb->vcbRsrcClumpSize ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB rsrcClumpSize \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->dataClumpSize		!= vcb->vcbDataClumpSize ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB dataClumpSize \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->nextCatalogID		!= vcb->vcbNextCatalogID &&
	     (volumeHeader->attributes & kHFSCatalogNodeIDsReused) == 0)  {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB nextCatalogID \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->writeCount			!= vcb->vcbWriteCount )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB writeCount \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->nextAllocation		!= vcb->vcbNextAllocation )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB nextAllocation \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->totalBlocks			!= vcb->vcbTotalBlocks ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB totalBlocks \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->blockSize			!= vcb->vcbBlockSize )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB blockSize \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->attributes			!= vcb->vcbAttributes )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB attributes \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->extentsFile.clumpSize	!= vcb->vcbExtentsFile->fcbClumpSize ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB extentsFile.clumpSize \n" );
		goto VolumeHeaderDamaged;
	}
	if ( volumeHeader->allocationFile.clumpSize	!= vcb->vcbAllocationFile->fcbClumpSize ) {
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB allocationFile.clumpSize \n" );
		goto VolumeHeaderDamaged;
	}
	if ( CmpBlock( volumeHeader->finderInfo, vcb->vcbFinderInfo, sizeof(vcb->vcbFinderInfo) ) )	{
		if ( GPtr->logLevel >= kDebugLog ) 
			printf( "\tinvalid VHB finderInfo \n" );
		goto VolumeHeaderDamaged;
	}
	
	goto ContinueChecking;
	
		
VolumeHeaderDamaged:
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		if (errID == 0)
			WriteError ( GPtr, E_VolumeHeaderDamaged, 2, 0 );
		return( noErr );

ContinueChecking:

	/*
	 * compare extent file allocation info with VolumeHeader
	 */		
	fcbP = vcb->vcbExtentsFile;
	if ( (UInt64)volumeHeader->extentsFile.totalBlocks * (UInt64)vcb->vcbBlockSize != fcbP->fcbPhysicalSize )
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
	if ( (UInt64)volumeHeader->catalogFile.totalBlocks * (UInt64)vcb->vcbBlockSize != fcbP->fcbPhysicalSize )
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
	if ( (UInt64)volumeHeader->allocationFile.totalBlocks * (UInt64)vcb->vcbBlockSize != fcbP->fcbPhysicalSize )
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

