/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
	File:		BTreeNodeOps.c

	Contains:	Single-node operations for the BTree Module.

	Version:	xxx put the technology version here xxx

	Written by:	Gordon Sheridan and Bill Bruffey

	Copyright:	й 1992-1999 by Apple Computer, Inc., all rights reserved.
*/

#include "BTreePrivate.h"
#include "hfs_endian.h"


///////////////////////// BTree Module Node Operations //////////////////////////
//
//	GetNode 			- Call FS Agent to get node
//	GetNewNode			- Call FS Agent to get a new node
//	ReleaseNode			- Call FS Agent to release node obtained by GetNode.
//	UpdateNode			- Mark a node as dirty and call FS Agent to release it.
//
//	CheckNode			- Checks the validity of a node.
//	ClearNode			- Clear a node to all zeroes.
//
//	InsertRecord		- Inserts a record into a BTree node.
//	InsertKeyRecord		- Inserts a key and record pair into a BTree node.
//	DeleteRecord		- Deletes a record from a BTree node.
//
//	SearchNode			- Return index for record that matches key.
//	LocateRecord		- Return pointer to key and data, and size of data.
//
//	GetNodeDataSize		- Return the amount of space used for data in the node.
//	GetNodeFreeSize		- Return the amount of free space in the node.
//
//	GetRecordOffset		- Return the offset for record "index".
//	GetRecordAddress	- Return address of record "index".
//	GetOffsetAddress	- Return address of offset for record "index".
//
//	InsertOffset		- Inserts a new offset into a node.
//	DeleteOffset		- Deletes an offset from a node.
//
//	MoveRecordsLeft		- Move records left within a node.
//	MoveRecordsRight	- Move records right within a node.
//
/////////////////////////////////////////////////////////////////////////////////



////////////////////// Routines Internal To BTreeNodeOps.c //////////////////////

UInt16		GetRecordOffset		(BTreeControlBlockPtr	 btree,
								 NodeDescPtr			 node,
								 UInt16					 index );

UInt16	   *GetOffsetAddress	(BTreeControlBlockPtr	btreePtr,
								 NodeDescPtr			 node,
								 UInt16					index );
								 
void		InsertOffset		(BTreeControlBlockPtr	 btreePtr,
								 NodeDescPtr			 node,
								 UInt16					 index,
								 UInt16					 delta );

void		DeleteOffset		(BTreeControlBlockPtr	 btreePtr,
								 NodeDescPtr			 node,
								 UInt16					 index );


/////////////////////////////////////////////////////////////////////////////////

#define GetRecordOffset(btreePtr,node,index)		(*(short *) ((UInt8 *)(node) + (btreePtr)->nodeSize - ((index) << 1) - kOffsetSize))

static void PrintNode(const NodeDescPtr node, UInt16 nodeSize, UInt32 nodeNumber);

/*-------------------------------------------------------------------------------

Routine:	GetNode	-	Call FS Agent to get node

Function:	Gets an existing BTree node from FS Agent and verifies it.

Input:		btreePtr	- pointer to BTree control block
			nodeNum		- number of node to request
			
Output:		nodePtr		- pointer to beginning of node (nil if error)
			
Result:
			noErr		- success
			!= noErr	- failure
-------------------------------------------------------------------------------*/

OSStatus	GetNode		(BTreeControlBlockPtr	 btreePtr,
						 UInt32					 nodeNum,
						 NodeRec				*nodePtr )
{
	OSStatus			err;
	GetBlockProcPtr		getNodeProc;
	

//	LogStartTime(kTraceGetNode);

	//ее is nodeNum within proper range?
	if( nodeNum >= btreePtr->totalNodes )
	{
		Panic("\pGetNode:nodeNum >= totalNodes");
		err = fsBTInvalidNodeErr;
		goto ErrorExit;
	}
	
	getNodeProc = btreePtr->getBlockProc;
	err = getNodeProc (btreePtr->fcbPtr,
					   nodeNum,
					   kGetBlock,
					   nodePtr );

	if (err != noErr)
	{
		Panic ("\pGetNode: getNodeProc returned error.");
		nodePtr->buffer = nil;
		goto ErrorExit;
	}
	++btreePtr->numGetNodes;
	
	SWAP_BT_NODE(nodePtr, (btreePtr->fcbPtr->fcbVolume->vcbSignature == kHFSPlusSigWord),
			btreePtr->fcbPtr->fcbFileID, 0);

	//
	// Optimization
	// Only call CheckNode if the node came from disk.
	// If it was in the cache, we'll assume its already a valid node.
	//
	
	if ( nodePtr->blockReadFromDisk )	// if we read it from disk then check it
	{
		err = CheckNode (btreePtr, nodePtr->buffer);
		if (err != noErr)
		{
			if (DEBUG_BUILD && ((NodeDescPtr)nodePtr->buffer)->numRecords != 0)
				PrintNode(nodePtr->buffer, btreePtr->nodeSize, nodeNum);

			if (DEBUG_BUILD)
			{
				// With the removal of bounds checking in IsItAHint(), it's possible that
				// GetNode() will be called to fetch a clear (all zeroes) node. We want
				// CheckNode() to fail in this case (it does), however we don't want to assert
				// this case because it is note really an "error". Returning an error from GetNode()
				// in this case will cause the hint checking code to ignore the hint and revert to
				// the full search mode.
				
				{
					UInt32	*cur;
					UInt32	*lastPlusOne;
					
					cur 		= nodePtr->buffer;
					lastPlusOne = (UInt32 *) ((UInt8 *) cur + btreePtr->nodeSize);
					
					while( cur < lastPlusOne )
					{
						if( *cur++ != 0 )
						{
							Panic ("\pGetNode: CheckNode returned error.");
							break;
						}
					}
				}
			}
			
			(void) ReleaseNode (btreePtr, nodePtr);			// ignore error
			goto ErrorExit;
		}
	}

//	LogEndTime(kTraceGetNode, noErr);

	return noErr;

ErrorExit:
	nodePtr->buffer			= nil;
	nodePtr->blockHeader	= nil;
	
//	LogEndTime(kTraceGetNode, err);

	return	err;
}



/*-------------------------------------------------------------------------------

Routine:	GetNewNode	-	Call FS Agent to get a new node

Function:	Gets a new BTree node from FS Agent and initializes it to an empty
			state.

Input:		btreePtr		- pointer to BTree control block
			nodeNum			- number of node to request
			
Output:		returnNodePtr	- pointer to beginning of node (nil if error)
			
Result:		noErr		- success
			!= noErr	- failure
-------------------------------------------------------------------------------*/

OSStatus	GetNewNode	(BTreeControlBlockPtr	 btreePtr,
						 UInt32					 nodeNum,
						 NodeRec				*returnNodePtr )
{
	OSStatus			 err;
	NodeDescPtr			 node;
	void				*pos;
	GetBlockProcPtr		 getNodeProc;
	

	//////////////////////// get buffer for new node ////////////////////////////

	getNodeProc = btreePtr->getBlockProc;
	err = getNodeProc (btreePtr->fcbPtr,
					   nodeNum,
					   kGetBlock+kGetEmptyBlock,
					   returnNodePtr );
					   
	if (err != noErr)
	{
		Panic ("\pGetNewNode: getNodeProc returned error.");
		returnNodePtr->buffer = nil;
		return err;
	}
	++btreePtr->numGetNewNodes;
	

	////////////////////////// initialize the node //////////////////////////////

	node = returnNodePtr->buffer;
	
	ClearNode (btreePtr, node);						// clear the node

	pos = (char *)node + btreePtr->nodeSize - 2;	// find address of last offset
	*(UInt16 *)pos = sizeof (BTNodeDescriptor);		// set offset to beginning of free space


	return noErr;
}



/*-------------------------------------------------------------------------------

Routine:	ReleaseNode	-	Call FS Agent to release node obtained by GetNode.

Function:	Informs the FS Agent that a BTree node may be released.

Input:		btreePtr		- pointer to BTree control block
			nodeNum			- number of node to release
						
Result:		noErr		- success
			!= noErr	- failure
-------------------------------------------------------------------------------*/

OSStatus	ReleaseNode	(BTreeControlBlockPtr	 btreePtr,
						 NodePtr				 nodePtr )
{
	OSStatus			 err;
	ReleaseBlockProcPtr	 releaseNodeProc;
	
	
//	LogStartTime(kTraceReleaseNode);

	err = noErr;
	
	if (nodePtr->buffer != nil)
	{
		/*
		 * The nodes must remain in the cache as big endian!
		 */
		SWAP_BT_NODE(nodePtr, (btreePtr->fcbPtr->fcbVolume->vcbSignature == kHFSPlusSigWord),
			btreePtr->fcbPtr->fcbFileID, 1);

		releaseNodeProc = btreePtr->releaseBlockProc;
		err = releaseNodeProc (btreePtr->fcbPtr,
							   nodePtr,
							   kReleaseBlock );
		PanicIf (err, "\pReleaseNode: releaseNodeProc returned error.");
		++btreePtr->numReleaseNodes;
	}
	
	nodePtr->buffer = nil;
	nodePtr->blockHeader = nil;
	
//	LogEndTime(kTraceReleaseNode, err);

	return err;
}


/*-------------------------------------------------------------------------------

Routine:	TrashNode	-	Call FS Agent to release node obtained by GetNode, and
							not store it...mark it as bad.

Function:	Informs the FS Agent that a BTree node may be released and thrown away.

Input:		btreePtr		- pointer to BTree control block
			nodeNum			- number of node to release
						
Result:		noErr		- success
			!= noErr	- failure
-------------------------------------------------------------------------------*/

OSStatus	TrashNode	(BTreeControlBlockPtr	 btreePtr,
						 NodePtr				 nodePtr )
{
	OSStatus			 err;
	ReleaseBlockProcPtr	 releaseNodeProc;
	

	err = noErr;
	
	if (nodePtr->buffer != nil)
	{
		releaseNodeProc = btreePtr->releaseBlockProc;
		err = releaseNodeProc (btreePtr->fcbPtr,
							   nodePtr,
							   kReleaseBlock | kTrashBlock );
		PanicIf (err, "\TrashNode: releaseNodeProc returned error.");
		++btreePtr->numReleaseNodes;
	}

	nodePtr->buffer			= nil;
	nodePtr->blockHeader	= nil;
	
	return err;
}


/*-------------------------------------------------------------------------------

Routine:	UpdateNode	-	Mark a node as dirty and call FS Agent to release it.

Function:	Marks a BTree node dirty and informs the FS Agent that it may be released.

			//ее have another routine that clears & writes a node, so we can call
			CheckNode from this routine.

Input:		btreePtr		- pointer to BTree control block
			nodeNum			- number of node to release
						
Result:		noErr		- success
			!= noErr	- failure
-------------------------------------------------------------------------------*/

OSStatus	UpdateNode	(BTreeControlBlockPtr	 btreePtr,
						 NodePtr				 nodePtr )
{
	OSStatus			 err;
	ReleaseBlockProcPtr	 releaseNodeProc;
	
	
	err = noErr;
		
	if (nodePtr->buffer != nil)			//ее why call UpdateNode if nil ?!?
	{
		if (DEBUG_BUILD)
		{
			if ( btreePtr->attributes & kBTVariableIndexKeysMask )
				(void) CheckNode (btreePtr, nodePtr->buffer);
		}

	//	LogStartTime(kTraceReleaseNode);
		SWAP_BT_NODE(nodePtr, (btreePtr->fcbPtr->fcbVolume->vcbSignature == kHFSPlusSigWord),
			btreePtr->fcbPtr->fcbFileID, 1);

		releaseNodeProc = btreePtr->releaseBlockProc;
		err = releaseNodeProc (btreePtr->fcbPtr,
							   nodePtr,
							   kMarkBlockDirty );
							   
	//	LogEndTime(kTraceReleaseNode, err);

		M_ExitOnError (err);
		++btreePtr->numUpdateNodes;
	}
	
	nodePtr->buffer			= nil;
	nodePtr->blockHeader	= nil;

	return	noErr;

ErrorExit:
	
	return	err;
}



/*-------------------------------------------------------------------------------

Routine:	CheckNode	-	Checks the validity of a node.

Function:	Checks the validity of a node by verifying that the fLink and bLink fields
			are within the forks EOF. The node type must be one of the four known
			types. The node height must be less than or equal to the tree height. The
			node must not have more than the maximum number of records, and the record
			offsets must make sense.

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node to check
						
Result:		noErr		- success
			fsBTInvalidNodeErr		- failure
-------------------------------------------------------------------------------*/

OSStatus	CheckNode	(BTreeControlBlockPtr	 btreePtr, NodeDescPtr	 node )
{
	SInt32		index;
	SInt32		maxRecords;
	UInt32		maxNode;
	UInt16		nodeSize;
	UInt16		offset;
	UInt16		prevOffset;

	nodeSize = btreePtr->nodeSize;

	///////////////////// are fLink and bLink within EOF ////////////////////////
	
	maxNode = (btreePtr->fcbPtr->fcbLogicalSize / nodeSize) - 1;
	
	if ( (node->fLink > maxNode) || (node->bLink > maxNode) )
		return fsBTInvalidNodeErr;
	
	
	/////////////// check node type (leaf, index, header, map) //////////////////
	
	if ( (node->kind < kBTLeafNode) || (node->kind > kBTMapNode) )
		return fsBTInvalidNodeErr;
	

	///////////////////// is node height > tree depth? //////////////////////////

	if ( node->height > btreePtr->treeDepth )
		return fsBTInvalidNodeErr;
	

	//////////////////////// check number of records ////////////////////////////
		
	//XXX can we calculate a more accurate minimum record size?
	maxRecords = ( nodeSize - sizeof (BTNodeDescriptor) ) >> 3;
	
	if (node->numRecords > maxRecords)
		return fsBTInvalidNodeErr;


	////////////////////////// check record offsets /////////////////////////////

	index = node->numRecords;		/* start index at free space */
	prevOffset = nodeSize - (index << 1);	/* use 2 bytes past end of free space */

	do {
		offset = GetRecordOffset (btreePtr, node, index);
			
		if (offset & 1)								// offset is odd
			return fsBTInvalidNodeErr;
		
		if (offset >= prevOffset)					// offset >= previous offset
			return fsBTInvalidNodeErr;
		
		/* reject keys that overflow record slot */
		if ((node->kind == kBTLeafNode) &&
		    (index < node->numRecords) &&	/* ignore free space record */
		    (CalcKeySize(btreePtr, (KeyPtr) ((Ptr)node + offset)) > (prevOffset - offset))) {
			return fsBTInvalidNodeErr;
		}
		
		prevOffset = offset;
	} while ( --index >= 0 );
	
	if (offset < sizeof (BTNodeDescriptor) )	// first offset < minimum ?
		return fsBTInvalidNodeErr;
	
	return noErr;
}


#if BSD
static void PrintNode(const NodeDescPtr node, UInt16 nodeSize, UInt32 nodeNumber)
{
	struct row {
		UInt16	word[8];
	};
	struct row	*offset;
	UInt16	rows;
	UInt32	*lp;

	printf("Dump of B-tree node #%d ($%08X)\n", nodeNumber, nodeNumber);

	rows = nodeSize/16;
	lp = (UInt32*) node;
	offset = 0;
	
	while (rows-- > 0) {
		printf( "%04X: %08X %08X %08X %08X\n", (u_int)offset++, 
				*(lp + 0), *(lp + 1), *(lp + 2), *(lp + 3) );
		lp += 4;
	}
}
#endif


/*-------------------------------------------------------------------------------

Routine:	ClearNode	-	Clear a node to all zeroes.

Function:	Writes zeroes from beginning of node for nodeSize bytes.

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node to clear
						
Result:		none
-------------------------------------------------------------------------------*/

void	ClearNode	(BTreeControlBlockPtr	btreePtr, NodeDescPtr	 node )
{
	ClearMemory( node, btreePtr->nodeSize );
}

/*-------------------------------------------------------------------------------

Routine:	InsertRecord	-	Inserts a record into a BTree node.

Function:	

Note:		Record size must be even!

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node to insert the record
			index			- position record is to be inserted
			recPtr			- pointer to record to insert

Result:		noErr		- success
			fsBTFullErr	- record larger than remaining free space.
-------------------------------------------------------------------------------*/

Boolean		InsertRecord	(BTreeControlBlockPtr	btreePtr,
							 NodeDescPtr 			node,
							 UInt16	 				index,
							 RecordPtr				recPtr,
							 UInt16					recSize )
{
	UInt16		freeSpace;
	UInt16		indexOffset;
	UInt16		freeOffset;
	UInt16		bytesToMove;
	void	   *src;
	void	   *dst;
	
	//// will new record fit in node?

	freeSpace = GetNodeFreeSize (btreePtr, node);
											//ее we could get freeOffset & calc freeSpace
	if ( freeSpace < recSize + 2)
	{
		return false;
	}

	
	//// make hole for new record

	indexOffset = GetRecordOffset (btreePtr, node, index);
	freeOffset	= GetRecordOffset (btreePtr, node, node->numRecords);

	src = ((Ptr) node) + indexOffset;
	dst = ((Ptr) src)  + recSize;
	bytesToMove = freeOffset - indexOffset;
	MoveRecordsRight (src, dst, bytesToMove);


	//// adjust offsets for moved records

	InsertOffset (btreePtr, node, index, recSize);


	//// move in the new record

	dst = ((Ptr) node) + indexOffset;
	MoveRecordsLeft (recPtr, dst, recSize);

	return true;
}



/*-------------------------------------------------------------------------------

Routine:	InsertKeyRecord	-	Inserts a record into a BTree node.

Function:	

Note:		Record size must be even!

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node to insert the record
			index			- position record is to be inserted
			keyPtr			- pointer to key for record to insert
			keyLength		- length of key (or maxKeyLength)
			recPtr			- pointer to record to insert
			recSize			- number of bytes to copy for record

Result:		noErr		- success
			fsBTFullErr	- record larger than remaining free space.
-------------------------------------------------------------------------------*/

Boolean		InsertKeyRecord		(BTreeControlBlockPtr	 btreePtr,
								 NodeDescPtr 			 node,
								 UInt16	 				 index,
								 KeyPtr					 keyPtr,
								 UInt16					 keyLength,
								 RecordPtr				 recPtr,
								 UInt16					 recSize )
{
	UInt16		freeSpace;
	UInt16		indexOffset;
	UInt16		freeOffset;
	UInt16		bytesToMove;
	UInt8 *		src;
	UInt8 *		dst;
	UInt16		keySize;
	UInt16		rawKeyLength;
	UInt16		sizeOfLength;
	
	//// calculate actual key size

	if ( btreePtr->attributes & kBTBigKeysMask )
		keySize = keyLength + sizeof(UInt16);
	else
		keySize = keyLength + sizeof(UInt8);
	
	if ( M_IsOdd (keySize) )
		++keySize;			// add pad byte


	//// will new record fit in node?

	freeSpace = GetNodeFreeSize (btreePtr, node);
											//ее we could get freeOffset & calc freeSpace
	if ( freeSpace < keySize + recSize + 2)
	{
		return false;
	}

	
	//// make hole for new record

	indexOffset = GetRecordOffset (btreePtr, node, index);
	freeOffset	= GetRecordOffset (btreePtr, node, node->numRecords);

	src = ((UInt8 *) node) + indexOffset;
	dst = ((UInt8 *) src) + keySize + recSize;
	bytesToMove = freeOffset - indexOffset;
	MoveRecordsRight (src, dst, bytesToMove);


	//// adjust offsets for moved records

	InsertOffset (btreePtr, node, index, keySize + recSize);
	

	//// copy record key

	dst = ((UInt8 *) node) + indexOffset;

	if ( btreePtr->attributes & kBTBigKeysMask )
	{
		*((UInt16*) dst)++ = keyLength;		// use keyLength rather than key.length
		rawKeyLength = keyPtr->length16;
		sizeOfLength = 2;
	}
	else
	{
		*dst++ = keyLength;					// use keyLength rather than key.length
		rawKeyLength = keyPtr->length8;
		sizeOfLength = 1;
	}
	
	MoveRecordsLeft ( ((UInt8 *) keyPtr) + sizeOfLength, dst, rawKeyLength);	// copy key

	// any pad bytes?
	bytesToMove = keySize - rawKeyLength;
	if (bytesToMove)
		ClearMemory (dst + rawKeyLength, bytesToMove);	// clear pad bytes in index key


	//// copy record data

	dst = ((UInt8 *) node) + indexOffset + keySize;
	MoveRecordsLeft (recPtr, dst, recSize);

	return true;
}



/*-------------------------------------------------------------------------------

Routine:	DeleteRecord	-	Deletes a record from a BTree node.

Function:	

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node to insert the record
			index			- position record is to be inserted

Result:		none
-------------------------------------------------------------------------------*/

void		DeleteRecord	(BTreeControlBlockPtr	btreePtr,
							 NodeDescPtr 			node,
							 UInt16	 				index )
{
	SInt16		indexOffset;
	SInt16		nextOffset;
	SInt16		freeOffset;
	SInt16		bytesToMove;
	void	   *src;
	void	   *dst;
	
	//// compress records
	indexOffset = GetRecordOffset (btreePtr, node, index);
	nextOffset	= GetRecordOffset (btreePtr, node, index + 1);
	freeOffset	= GetRecordOffset (btreePtr, node, node->numRecords);

	src = ((Ptr) node) + nextOffset;
	dst = ((Ptr) node) + indexOffset;
	bytesToMove = freeOffset - nextOffset;
	MoveRecordsLeft (src, dst, bytesToMove);

	//// Adjust the offsets
	DeleteOffset (btreePtr, node, index);
}



/*-------------------------------------------------------------------------------

Routine:	SearchNode	-	Return index for record that matches key.

Function:	Returns the record index for the record that matches the search key.
			If no record was found that matches the search key, the "insert index"
			of where the record should go is returned instead.

Algorithm:	A binary search algorithm is used to find the specified key.

Input:		btreePtr	- pointer to BTree control block
			node		- pointer to node that contains the record
			searchKey	- pointer to the key to match

Output:		index		- pointer to beginning of key for record

Result:		true	- success (index = record index)
			false	- key did not match anything in node (index = insert index)
-------------------------------------------------------------------------------*/

Boolean		SearchNode		(BTreeControlBlockPtr	 btreePtr,
							 NodeDescPtr			 node,
							 KeyPtr					 searchKey,
							 UInt16					*returnIndex )
{
	SInt32		lowerBound;
	SInt32		upperBound;
	SInt32		index;
	SInt32		result;
	KeyPtr		trialKey;
#if !SupportsKeyDescriptors
	KeyCompareProcPtr	compareProc = btreePtr->keyCompareProc;
#endif	

	lowerBound = 0;
	upperBound = node->numRecords - 1;
	
	while (lowerBound <= upperBound)
	{
		index = (lowerBound + upperBound) >> 1;		// divide by 2
		
		trialKey = (KeyPtr) GetRecordAddress (btreePtr, node, index );
		
	#if SupportsKeyDescriptors
		result = CompareKeys (btreePtr, searchKey, trialKey);
	#else
		result = compareProc(searchKey, trialKey);
	#endif
		
		if		(result <  0)		upperBound = index - 1;		// search < trial
		else if (result >  0)		lowerBound = index + 1;		// search > trial
		else													// search = trial
		{
			*returnIndex = index;
			return true;
		}
	}
	
	*returnIndex = lowerBound;				// lowerBound is insert index
	return false;
}


/*-------------------------------------------------------------------------------

Routine:	GetRecordByIndex	-	Return pointer to key and data, and size of data.

Function:	Returns a pointer to beginning of key for record, a pointer to the
			beginning of the data for the record, and the size of the record data
			(does not include the size of the key).

Input:		btreePtr	- pointer to BTree control block
			node		- pointer to node that contains the record
			index		- index of record to get

Output:		keyPtr		- pointer to beginning of key for record
			dataPtr		- pointer to beginning of data for record
			dataSize	- size of the data portion of the record

Result:		none
-------------------------------------------------------------------------------*/

OSStatus	GetRecordByIndex	(BTreeControlBlockPtr	 btreePtr,
								 NodeDescPtr			 node,
								 UInt16					 index,
								 KeyPtr					*keyPtr,
								 UInt8 *				*dataPtr,
								 UInt16					*dataSize )
{
	UInt16		offset;
	UInt16		nextOffset;
	UInt16		keySize;
	
	//
	//	Make sure index is valid (in range 0..numRecords-1)
	//
	if (index >= node->numRecords)
		return fsBTRecordNotFoundErr;

	//// find keyPtr
	offset		= GetRecordOffset (btreePtr, node, index);
	*keyPtr		= (KeyPtr) ((Ptr)node + offset);

	//// find dataPtr
	keySize	= CalcKeySize(btreePtr, *keyPtr);
	if ( M_IsOdd (keySize) )
		++keySize;	// add pad byte

	offset += keySize;			// add the key length to find data offset
	*dataPtr = (UInt8 *) node + offset;
	
	//// find dataSize
	nextOffset	= GetRecordOffset (btreePtr, node, index + 1);
	*dataSize	= nextOffset - offset;
	
	return noErr;
}
								 


/*-------------------------------------------------------------------------------

Routine:	GetNodeDataSize	-	Return the amount of space used for data in the node.

Function:	Gets the size of the data currently contained in a node, excluding
			the node header. (record data + offset overhead)

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node that contains the record

Result:		- number of bytes used for data and offsets in the node.
-------------------------------------------------------------------------------*/

UInt16		GetNodeDataSize	(BTreeControlBlockPtr	btreePtr, NodeDescPtr	 node )
{
	UInt16 freeOffset;
	
	freeOffset = GetRecordOffset (btreePtr, node, node->numRecords);
	
	return	freeOffset + (node->numRecords << 1) - sizeof (BTNodeDescriptor);
}



/*-------------------------------------------------------------------------------

Routine:	GetNodeFreeSize	-	Return the amount of free space in the node.

Function:	

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node that contains the record

Result:		- number of bytes of free space in the node.
-------------------------------------------------------------------------------*/

UInt16		GetNodeFreeSize	(BTreeControlBlockPtr	btreePtr, NodeDescPtr	 node )
{
	UInt16	freeOffset;
	
	freeOffset = GetRecordOffset (btreePtr, node, node->numRecords);	//ее inline?
	
	return btreePtr->nodeSize - freeOffset - (node->numRecords << 1) - kOffsetSize;
}



/*-------------------------------------------------------------------------------

Routine:	GetRecordOffset	-	Return the offset for record "index".

Function:	

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node that contains the record
			index			- record to obtain offset for

Result:		- offset (in bytes) from beginning of node of record specified by index
-------------------------------------------------------------------------------*/
// make this a macro (for inlining)
#if 0
UInt16		GetRecordOffset	(BTreeControlBlockPtr	btreePtr,
							 NodeDescPtr			node,
							 UInt16					index )
{
	void	*pos;
	
		
	pos = (UInt8 *)node + btreePtr->nodeSize - (index << 1) - kOffsetSize;
	
	return *(short *)pos;
}
#endif



/*-------------------------------------------------------------------------------

Routine:	GetRecordAddress	-	Return address of record "index".

Function:	

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node that contains the record
			index			- record to obtain offset address for

Result:		- pointer to record "index".
-------------------------------------------------------------------------------*/
// make this a macro (for inlining)
#if 0
UInt8 *		GetRecordAddress	(BTreeControlBlockPtr	btreePtr,
								 NodeDescPtr			node,
								 UInt16					index )
{
	UInt8 *		pos;
	
	pos = (UInt8 *)node + GetRecordOffset (btreePtr, node, index);
	
	return pos;
}
#endif



/*-------------------------------------------------------------------------------

Routine:	GetRecordSize	-	Return size of record "index".

Function:	

Note:		This does not work on the FreeSpace index!

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node that contains the record
			index			- record to obtain record size for

Result:		- size of record "index".
-------------------------------------------------------------------------------*/

UInt16		GetRecordSize		(BTreeControlBlockPtr	btreePtr,
								 NodeDescPtr			node,
								 UInt16					index )
{
	UInt16	*pos;
		
	pos = (UInt16 *) ((Ptr)node + btreePtr->nodeSize - (index << 1) - kOffsetSize);
	
	return  *(pos-1) - *pos;
}



/*-------------------------------------------------------------------------------
Routine:	GetOffsetAddress	-	Return address of offset for record "index".

Function:	

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node that contains the record
			index			- record to obtain offset address for

Result:		- pointer to offset for record "index".
-------------------------------------------------------------------------------*/

UInt16	   *GetOffsetAddress	(BTreeControlBlockPtr	btreePtr,
								 NodeDescPtr			node,
								 UInt16					index )
{
	void	*pos;
	
	pos = (Ptr)node + btreePtr->nodeSize - (index << 1) -2;
	
	return (UInt16 *)pos;
}



/*-------------------------------------------------------------------------------
Routine:	GetChildNodeNum	-	Return child node number from index record "index".

Function:	Returns the first UInt32 stored after the key for record "index".

Assumes:	The node is an Index Node.
			The key.length stored at record "index" is ODD. //ее change for variable length index keys

Input:		btreePtr		- pointer to BTree control block
			node			- pointer to node that contains the record
			index			- record to obtain child node number from

Result:		- child node number from record "index".
-------------------------------------------------------------------------------*/

UInt32		GetChildNodeNum			(BTreeControlBlockPtr	 btreePtr,
									 NodeDescPtr			 nodePtr,
									 UInt16					 index )
{
	UInt8 *		pos;
	
	pos = GetRecordAddress (btreePtr, nodePtr, index);
	pos += CalcKeySize(btreePtr, (BTreeKey *) pos);		// key.length + size of length field
	
	return	*(UInt32 *)pos;
}



/*-------------------------------------------------------------------------------
Routine:	InsertOffset	-	Add an offset and adjust existing offsets by delta.

Function:	Add an offset at 'index' by shifting 'index+1' through the last offset
			and adjusting them by 'delta', the size of the record to be inserted.
			The number of records contained in the node is also incremented.

Input:		btreePtr	- pointer to BTree control block
			node		- pointer to node
			index		- index at which to insert record
			delta		- size of record to be inserted

Result:		none
-------------------------------------------------------------------------------*/

void		InsertOffset		(BTreeControlBlockPtr	 btreePtr,
								 NodeDescPtr			 node,
								 UInt16					 index,
								 UInt16					 delta )
{
	UInt16		*src, *dst;
	UInt16		 numOffsets;
	
	src = GetOffsetAddress (btreePtr, node, node->numRecords);	// point to free offset
	dst = src - 1; 												// point to new offset
	numOffsets = node->numRecords++ - index;			// subtract index  & postincrement
	
	do {
		*dst++ = *src++ + delta;								// to tricky?
	} while (numOffsets--);
}



/*-------------------------------------------------------------------------------

Routine:	DeleteOffset	-	Delete an offset.

Function:	Delete the offset at 'index' by shifting 'index+1' through the last offset
			and adjusting them by the size of the record 'index'.
			The number of records contained in the node is also decremented.

Input:		btreePtr	- pointer to BTree control block
			node		- pointer to node
			index		- index at which to delete record

Result:		none
-------------------------------------------------------------------------------*/

void		DeleteOffset		(BTreeControlBlockPtr	 btreePtr,
								 NodeDescPtr			 node,
								 UInt16					 index )
{
	UInt16		*src, *dst;
	UInt16		 numOffsets;
	UInt16		 delta;
	
	dst			= GetOffsetAddress (btreePtr, node, index);
	src			= dst - 1;
	delta		= *src - *dst;
	numOffsets	= --node->numRecords - index;	// predecrement numRecords & subtract index
	
	while (numOffsets--)
	{
		*--dst = *--src - delta;				// work our way left
	}
}



/*-------------------------------------------------------------------------------

Routine:	MoveRecordsLeft	-	Move records left within a node.

Function:	Moves a number of bytes from src to dst. Safely handles overlapping
			ranges if the bytes are being moved to the "left". No bytes are moved
			if bytesToMove is zero.

Input:		src				- pointer to source
			dst				- pointer to destination
			bytesToMove		- number of bytes to move records

Result:		none
-------------------------------------------------------------------------------*/
#if 0
void		MoveRecordsLeft		(UInt8 *				 src,
								 UInt8 *				 dst,
								 UInt16					 bytesToMove )
{
	while (bytesToMove--)
		*dst++ = *src++;
}
#endif							 


/*-------------------------------------------------------------------------------

Routine:	MoveRecordsRight	-	Move records right within a node.

Function:	Moves a number of bytes from src to dst. Safely handles overlapping
			ranges if the bytes are being moved to the "right". No bytes are moved
			if bytesToMove is zero.

Input:		src				- pointer to source
			dst				- pointer to destination
			bytesToMove		- number of bytes to move records
			
Result:		none
-------------------------------------------------------------------------------*/
#if 0
void		MoveRecordsRight	(UInt8 *				 src,
								 UInt8 *				 dst,
								 UInt16					 bytesToMove )
{
	src += bytesToMove;
	dst += bytesToMove;
	
	while (bytesToMove--)
		*--dst = *--src;
}
#endif
