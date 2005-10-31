/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CDSRefMap
 * DirectoryService Framework reference map table to references of
 * all TCP connected "remote" DirectoryService daemons.
 * References here always use 0x00C00000 bits
 */

#include "CDSRefMap.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysctl.h>	// for struct kinfo_proc and sysctl()
#include "PrivateTypes.h"

#ifdef __LITTLE_ENDIAN__
static tRefMap		fServerToLocalRefMap;
static tRefMap		fMsgIDToServerRefMap;
static tRefMap		fMsgIDToCustomCodeMap;
#endif

//--------------------------------------------------------------------------------------------------
//	* Globals
//--------------------------------------------------------------------------------------------------

DSMutexSemaphore	   *gFWRefMapMutex	= nil;

//FW client references logging
dsBool	gLogFWRefMapCalls = false;
//KW need realtime mechanism to set this to true ie. getenv variable OR ?
//KW greatest difficulty is the bookkeeping arrays which were CF based and need to be replaced
//so that the FW does not depend upon CoreFoundation - is this really a problem?

//------------------------------------------------------------------------------------
//	* CDSRefMap
//------------------------------------------------------------------------------------

CDSRefMap::CDSRefMap ( RefMapDeallocateProc *deallocProc )
{
	fTableCount		= 0;
	::memset( fRefMapTables, 0, sizeof( fRefMapTables ) );

	if ( gFWRefMapMutex == nil )
	{
		gFWRefMapMutex = new DSMutexSemaphore();
		if( gFWRefMapMutex == nil ) throw((sInt32)eMemoryAllocError);
	}
	fDeallocProc = deallocProc;
} // CDSRefMap


//------------------------------------------------------------------------------------
//	* ~CDSRefMap
//------------------------------------------------------------------------------------

CDSRefMap::~CDSRefMap ( void )
{
	uInt32		i	= 1;
	uInt32		j	= 1;

	for ( i = 1; i <= kMaxFWTables; i++ )	//array is still zero based even if first entry NOT used
										//-- added the last kMaxTable in the .h file so this should work now
	{
		if ( fRefMapTables[ i ] != nil )
		{
			for (j=0; j< kMaxFWTableItems; j++)
			{
				if (fRefMapTables[ i ]->fTableData[j] != nil)
				{
					free(fRefMapTables[ i ]->fTableData[j]);
					fRefMapTables[ i ]->fTableData[j] = nil;
				}
			}
			free( fRefMapTables[ i ] );  //free all the calloc'ed sRefEntries inside as well - code above
			fRefMapTables[ i ] = nil;
		}
	}

	if ( gFWRefMapMutex != nil )
	{
		delete( gFWRefMapMutex );
		gFWRefMapMutex = nil;
	}
} // ~CDSRefMap


//--------------------------------------------------------------------------------------------------
//	* VerifyReference
//
//--------------------------------------------------------------------------------------------------

tDirStatus CDSRefMap::VerifyReference (	tDirReference	inDirRef, //should be generic ref here
										uInt32			inType,
										sInt32			inPID )
{
	tDirStatus		siResult	= eDSInvalidReference;
	sFWRefMapEntry	   *refData		= nil;
	sPIDFWInfo	   *pPIDInfo	= nil;
	
	if ((inDirRef & 0x00C00000) != 0)
	{
		siResult = GetReference( inDirRef, &refData );
		//ref actually exists
		if ( siResult == eDSNoErr )
		{
			//check if the type is correct
			if ( refData->fType != inType )
			{
				siResult = eDSInvalidRefType;
			}
			else
			{
				//check the PIDs here - both parent and child client ones
				siResult = eDSInvalidRefType;
				//parent client PID check
				if (refData->fPID == inPID)
				{
					siResult = eDSNoErr;
				}
				else
				{
					//child clients PID check
					pPIDInfo = refData->fChildPID;
					while ( (pPIDInfo != nil) && (siResult != eDSNoErr) )
					{
						if (pPIDInfo->fPID == inPID)
						{
							siResult = eDSNoErr;
						}
						pPIDInfo = pPIDInfo->fNext;
					}
				}
			}
		}
	}

	return( siResult );

} // VerifyReference



//------------------------------------------------------------------------------------
//	* VerifyDirRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::VerifyDirRef (	tDirReference		inDirRef,
										sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->VerifyReference( inDirRef, eDirectoryRefType, inPID );
	}

	return( siResult );

} // VerifyDirRef



//------------------------------------------------------------------------------------
//	* VerifyNodeRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::VerifyNodeRef (	tDirNodeReference	inDirNodeRef,
										sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->VerifyReference( inDirNodeRef, eNodeRefType, inPID );
	}

	return( siResult );

} // VerifyNodeRef


//------------------------------------------------------------------------------------
//	* VerifyRecordRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::VerifyRecordRef (	tRecordReference	inRecordRef,
                                            sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->VerifyReference( inRecordRef, eRecordRefType, inPID );
	}

	return( siResult );

} // VerifyRecordRef



//------------------------------------------------------------------------------------
//	* VerifyAttrListRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::VerifyAttrListRef (	tAttributeListRef	inAttributeListRef,
											sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->VerifyReference( inAttributeListRef, eAttrListRefType, inPID );
	}

	return( siResult );

} // VerifyAttrListRef


//------------------------------------------------------------------------------------
//	* VerifyAttrValueRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::VerifyAttrValueRef (	tAttributeValueListRef	inAttributeValueListRef,
                                                sInt32					inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->VerifyReference( inAttributeValueListRef, eAttrValueListRefType, inPID );
	}

	return( siResult );

} // VerifyAttrValueRef


//------------------------------------------------------------------------------------
//	* NewDirRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::NewDirRefMap ( uInt32 *outNewRef, sInt32 inPID,
											uInt32		serverRef,
											uInt32		messageIndex )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->GetNewRef( outNewRef, 0, eDirectoryRefType, inPID, serverRef, messageIndex );
	}

	return( siResult );

} // NewDirRef


//------------------------------------------------------------------------------------
//	* NewNodeRefMap
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::NewNodeRefMap (	uInt32			*outNewRef,
										uInt32			inParentID,
										sInt32			inPID,
										uInt32			serverRef,
										uInt32			messageIndex,
										char		   *inPluginName)
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->GetNewRef( outNewRef, inParentID, eNodeRefType, inPID, serverRef, messageIndex );
		if (siResult == eDSNoErr)
		{
			siResult = gFWRefMap->SetPluginName( *outNewRef, eNodeRefType, inPluginName, inPID );
		}
	}

	return( siResult );

} // NewNodeRefMap


//------------------------------------------------------------------------------------
//	* NewRecordRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::NewRecordRefMap (	uInt32			*outNewRef,
										uInt32			inParentID,
										sInt32			inPID,
											uInt32		serverRef,
											uInt32		messageIndex )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->GetNewRef( outNewRef, inParentID, eRecordRefType, inPID, serverRef, messageIndex );
	}

	return( siResult );

} // NewRecordRef


//------------------------------------------------------------------------------------
//	* NewAttrListRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::NewAttrListRefMap (	uInt32			*outNewRef,
											uInt32			inParentID,
											sInt32			inPID,
											uInt32		serverRef,
											uInt32		messageIndex )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->GetNewRef( outNewRef, inParentID, eAttrListRefType, inPID, serverRef, messageIndex );
	}

	return( siResult );

} // NewAttrListRef


//------------------------------------------------------------------------------------
//	* NewAttrValueRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::NewAttrValueRefMap (	uInt32			*outNewRef,
											uInt32			inParentID,
											sInt32			inPID,
											uInt32		serverRef,
											uInt32		messageIndex )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->GetNewRef( outNewRef, inParentID, eAttrValueListRefType, inPID, serverRef, messageIndex );
	}

	return( siResult );

} // NewAttrValueRef


//------------------------------------------------------------------------------------
//	* RemoveDirRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::RemoveDirRef ( uInt32 inDirRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->RemoveRef( inDirRef, eDirectoryRefType, inPID );
	}

	return( siResult );

} // RemoveDirRef


//------------------------------------------------------------------------------------
//	* RemoveNodeRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::RemoveNodeRef ( uInt32 inNodeRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->RemoveRef( inNodeRef, eNodeRefType, inPID );
	}

	return( siResult );

} // RemoveNodeRef


//------------------------------------------------------------------------------------
//	* RemoveRecordRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::RemoveRecordRef ( uInt32 inRecRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->RemoveRef( inRecRef, eRecordRefType, inPID );
	}

	return( siResult );

} // RemoveRecordRef


//------------------------------------------------------------------------------------
//	* RemoveAttrListRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::RemoveAttrListRef ( uInt32 inAttrListRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->RemoveRef( inAttrListRef, eAttrListRefType, inPID );
	}

	return( siResult );

} // RemoveAttrListRef


//------------------------------------------------------------------------------------
//	* RemoveAttrValueRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::RemoveAttrValueRef ( uInt32 inAttrValueRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefMap != nil )
	{
		siResult = gFWRefMap->RemoveRef( inAttrValueRef, eAttrValueListRefType, inPID );
	}

	return( siResult );

} // RemoveAttrValueRef


//------------------------------------------------------------------------------------
//	* GetTableRef
//------------------------------------------------------------------------------------

sFWRefMapEntry* CDSRefMap::GetTableRef ( uInt32 inRefNum )
{
	uInt32			uiSlot			= 0;
	uInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	uInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	sRefMapTable	   *pTable			= nil;
	sFWRefMapEntry	   *pOutEntry		= nil;

	gFWRefMapMutex->Wait();

	try
	{
		pTable = GetThisTable( uiTableNum );
		if ( pTable == nil ) throw( (sInt32)eDSInvalidReference );

		uiSlot = uiRefNum % kMaxFWTableItems;
		if ( pTable->fTableData != nil)
		{
			if ( pTable->fTableData[ uiSlot ] != nil )
			{
				if ( uiRefNum == pTable->fTableData[ uiSlot ]->fRefNum )
				{
					pOutEntry = pTable->fTableData[ uiSlot ];
				}
			}
		}
	}

	catch( sInt32 err )
	{
	}

	gFWRefMapMutex->Signal();

	return( pOutEntry );

} // GetTableRef


//------------------------------------------------------------------------------------
//	* GetNextTable
//------------------------------------------------------------------------------------

sRefMapTable* CDSRefMap::GetNextTable ( sRefMapTable *inTable )
{
	uInt32		uiTblNum	= 0;
	sRefMapTable	*pOutTable	= nil;

	gFWRefMapMutex->Wait();

	try
	{
		if ( inTable == nil )
		{
			// Get the first reference table, create it if it wasn't already
			if ( fRefMapTables[ 1 ] == nil )
			{
				// No tables have been allocated yet so lets make one
				fRefMapTables[ 1 ] = (sRefMapTable *)::calloc( sizeof( sRefMapTable ), sizeof( char ) );
				if ( fRefMapTables[ 1 ] == nil ) throw((sInt32)eMemoryAllocError);

				fRefMapTables[ 1 ]->fTableNum = 1;

				fTableCount = 1;
			}

			pOutTable = fRefMapTables[ 1 ];
		}
		else
		{
			uiTblNum = inTable->fTableNum + 1;
			if (uiTblNum > kMaxFWTables) throw( (sInt32)eDSInvalidReference );

			if ( fRefMapTables[ uiTblNum ] == nil )
			{
				// Set the table counter
				fTableCount = uiTblNum;

				// No tables have been allocated yet so lets make one
				fRefMapTables[ uiTblNum ] = (sRefMapTable *)::calloc( sizeof( sRefMapTable ), sizeof( char ) );
				if( fRefMapTables[ uiTblNum ] == nil ) throw((sInt32)eMemoryAllocError);

				if (uiTblNum == 0) throw( (sInt32)eDSInvalidReference );
				fRefMapTables[ uiTblNum ]->fTableNum = uiTblNum;
			}

			pOutTable = fRefMapTables[ uiTblNum ];
		}
	}

	catch( sInt32 err )
	{
	}

	gFWRefMapMutex->Signal();

	return( pOutTable );

} // GetNextTable


//------------------------------------------------------------------------------------
//	* GetThisTable
//------------------------------------------------------------------------------------

sRefMapTable* CDSRefMap::GetThisTable ( uInt32 inTableNum )
{
	sRefMapTable	*pOutTable	= nil;

	gFWRefMapMutex->Wait();

	pOutTable = fRefMapTables[ inTableNum ];

	gFWRefMapMutex->Signal();

	return( pOutTable );

} // GetThisTable


//------------------------------------------------------------------------------------
//	* GetNewRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::GetNewRef (	uInt32		   *outRef,
									uInt32			inParentID,
									eRefTypes		inType,
									sInt32			inPID,
									uInt32			serverRef,
									uInt32			messageIndex )
{
	bool			done		= false;
	tDirStatus		outResult	= eDSNoErr;
	sRefMapTable	   *pCurTable	= nil;
	uInt32			uiRefNum	= 0;
	uInt32			uiCntr		= 0;
	uInt32			uiSlot		= 0;
	uInt32			uiTableNum	= 0;

	gFWRefMapMutex->Wait();

	try
	{
		*outRef = 0;

		while ( !done )
		{
			pCurTable = GetNextTable( pCurTable );
			if ( pCurTable == nil ) throw( (sInt32)eDSRefTableFWAllocError );

			if ( pCurTable->fItemCnt < kMaxFWTableItems )
			{
				uiCntr = 0;
				uiTableNum = pCurTable->fTableNum;
				while ( (uiCntr < kMaxFWTableItems) && !done )	//KW80 - uiCntr was a condition never used
																//fixed below with uiCntr++; code addition
				{
					if ( (pCurTable->fCurRefNum == 0) || 
						 (pCurTable->fCurRefNum > 0x000FFFFF) )
					{
						// Either it's the first reference for this table or we have
						//	used 1024*1024 references and need to start over
						pCurTable->fCurRefNum = 1;
					}

					// Get the ref num and increment its place holder
					uiRefNum = pCurTable->fCurRefNum++;
					uiRefNum += 0x00C00000;

					// Find a slot in the table for this ref number
					uiSlot = uiRefNum % kMaxFWTableItems;
					if ( pCurTable->fTableData[ uiSlot ] == nil )
					{
						pCurTable->fTableData[ uiSlot ] = (sFWRefMapEntry *)::calloc( sizeof( sFWRefMapEntry ), sizeof( char ) );
						if ( pCurTable->fTableData[ uiSlot ] == nil ) throw( (sInt32)eDSRefTableFWAllocError );
						
						// We found an empty slot, now set this table entry
						pCurTable->fTableData[ uiSlot ]->fRefNum		= uiRefNum;
						pCurTable->fTableData[ uiSlot ]->fType			= inType;
						pCurTable->fTableData[ uiSlot ]->fParentID		= inParentID;
						pCurTable->fTableData[ uiSlot ]->fPID			= inPID;
						pCurTable->fTableData[ uiSlot ]->fRemoteRefNum	= serverRef;
						pCurTable->fTableData[ uiSlot ]->fChildren		= nil;
						pCurTable->fTableData[ uiSlot ]->fChildPID		= nil;
						pCurTable->fTableData[ uiSlot ]->fMessageTableIndex = messageIndex;

						// Add the table number to the reference number
						uiTableNum = (uiTableNum << 24);
						uiRefNum = uiRefNum | uiTableNum;

						*outRef = uiRefNum;

						// Up the item count
						pCurTable->fItemCnt++;

						outResult = eDSNoErr;
						done = true;
					}
					uiCntr++;	//KW80 needed for us to only go through the table once
								//ie the uiCntr does not get used directly BUT the uiRefNum gets
								//incremented only kMaxFWTableItems times since uiCntr is in the while condition
				}
			}
		}

		if ( inParentID != 0 )
		{
			outResult = LinkToParent( *outRef, inType, inParentID, inPID );
		}
	}

	catch( sInt32 err )
	{
		outResult = (tDirStatus)err;
	}

	gFWRefMapMutex->Signal();

	return( outResult );

} // GetNewRef


//------------------------------------------------------------------------------------
//	* LinkToParent
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::LinkToParent ( uInt32 inRefNum, uInt32 inType, uInt32 inParentID, sInt32 inPID )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefMapEntry	   *pCurrRef		= nil;
	sListFWInfo	   *pChildInfo		= nil;

	gFWRefMapMutex->Wait();

	try
	{
		pCurrRef = GetTableRef( inParentID );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		// This is the one we want
		pChildInfo = (sListFWInfo *)::calloc( sizeof( sListFWInfo ), sizeof( char ) );
		if ( pChildInfo == nil ) throw( (sInt32)eDSRefTableFWAllocError );

		// Save the info required later for removal if the parent gets removed
		pChildInfo->fRefNum		= inRefNum;
		pChildInfo->fType		= inType;
		pChildInfo->fPID		= inPID;

		// Set the link info
		pChildInfo->fNext = pCurrRef->fChildren;
		pCurrRef->fChildren = pChildInfo;
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	gFWRefMapMutex->Signal();

	return( dsResult );

} // LinkToParent


//------------------------------------------------------------------------------------
//	* UnlinkFromParent
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::UnlinkFromParent ( uInt32 inRefNum )
{
	tDirStatus		dsResult		= eDSNoErr;
	uInt32			i				= 1;
	uInt32			parentID		= 0;
	sFWRefMapEntry	   *pCurrRef		= nil;
	sFWRefMapEntry	   *pParentRef		= nil;
	sListFWInfo	   *pCurrChild		= nil;
	sListFWInfo	   *pPrevChild		= nil;

	gFWRefMapMutex->Wait();

	try
	{
		//Node references are currently not linked to their parent dir reference
		//So, there is no issue when any child client PID has a reference linked to a parent since
		//it is unique to that child client PID and can be unlinked as before
		
		// Get the current reference
		pCurrRef = GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		parentID = pCurrRef->fParentID;

		if ( parentID != 0 )
		{
			// Get the parent reference
			pParentRef = GetTableRef( parentID );
			if ( pParentRef == nil ) throw( (sInt32)eDSInvalidReference );

			pCurrChild = pParentRef->fChildren;
			pPrevChild = pParentRef->fChildren;

			while ( pCurrChild != nil )
			{
				//this will only work if no two or more children have the same ref number
				if ( pCurrChild->fRefNum == inRefNum )
				{
					// Remove this node
					if ( i == 1 )
					{
						// Link the next node to the base pointer
						pParentRef->fChildren = pCurrChild->fNext;
					}
					else
					{
						// Link the next node to the previous node
						pPrevChild->fNext = pCurrChild->fNext;
					}

					free( pCurrChild );
					pCurrChild = nil;
					break;
				}
				pPrevChild = pCurrChild;
				pCurrChild = pCurrChild->fNext;

				i++;
			}
		}
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	gFWRefMapMutex->Signal();

	return( dsResult );

} // UnlinkFromParent


//------------------------------------------------------------------------------------
//	* GetReference
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::GetReference ( uInt32 inRefNum, sFWRefMapEntry **outRefData )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefMapEntry	   *pCurrRef		= nil;

	gFWRefMapMutex->Wait();

	try
	{
		pCurrRef = GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		*outRefData = pCurrRef;
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	gFWRefMapMutex->Signal();

	return( dsResult );

} // GetReference


//------------------------------------------------------------------------------------
//	* RemoveRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::RemoveRef ( uInt32 inRefNum, uInt32 inType, sInt32 inPID )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefMapEntry	   *pCurrRef		= nil;
	sRefMapTable	   *pTable			= nil;
	uInt32			uiSlot			= 0;
	uInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	uInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	bool			doFree			= false;
	sPIDFWInfo	   *pPIDInfo		= nil;
	sPIDFWInfo	   *pPrevPIDInfo	= nil;


	gFWRefMapMutex->Wait();

	try
	{
		dsResult = VerifyReference( inRefNum, inType, inPID );

		if ( dsResult == eDSNoErr )
		{
			pTable = GetThisTable( uiTableNum );
			if ( pTable == nil ) throw( (sInt32)eDSInvalidReference );

			uiSlot = uiRefNum % kMaxFWTableItems;

			if ( inType != eDirectoryRefType ) // API refs have no parents
			{
				dsResult = UnlinkFromParent( inRefNum );
				if ( dsResult != eDSNoErr ) throw( (sInt32)dsResult );
			}

			pCurrRef = GetTableRef( inRefNum );	//KW80 - here we need to know where the sFWRefMapEntry is in the table to delete it
												//that is all the code added above
												// getting this sFWRefMapEntry is the same path used by uiSlot and pTable above
			if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );
			if (inType != pCurrRef->fType) throw( (sInt32)eDSInvalidReference );

			if ( pCurrRef->fChildren != nil )
			{
				gFWRefMapMutex->Signal();
				RemoveChildren( pCurrRef->fChildren, inPID );
				gFWRefMapMutex->Wait();
			}

			//Now we check to see if this was a child or parent client PID that we removed - only applies to case of Node refs really
			if (pCurrRef->fPID == inPID)
			{
				pCurrRef->fPID = -1;
				if (pCurrRef->fChildPID == nil)
				{
					doFree = true;
				}
			}
			else
			{
				pPIDInfo		= pCurrRef->fChildPID;
				pPrevPIDInfo	= pCurrRef->fChildPID;
				//remove all child client PIDs that match since all children refs for that PID removed already above
				//ie. within scope of a PID the refs are NOT ref counted
				while (pPIDInfo != nil)
				{
					if (pPIDInfo->fPID == inPID)
					{
						//need to remove this particular PID entry
						if (pPIDInfo == pCurrRef->fChildPID)
						{
							pCurrRef->fChildPID = pCurrRef->fChildPID->fNext;
							free(pPIDInfo);
							pPIDInfo			= pCurrRef->fChildPID;
							pPrevPIDInfo		= pCurrRef->fChildPID;
						}
						else
						{
							pPrevPIDInfo->fNext = pPIDInfo->fNext;
							free(pPIDInfo);
							pPIDInfo			= pPrevPIDInfo->fNext;
						}
					}
					else
					{
						pPrevPIDInfo = pPIDInfo;
						pPIDInfo = pPIDInfo->fNext;
					}
				}
				//child client PIDs now removed so re-eval free
				if ( (pCurrRef->fPID == -1) && (pCurrRef->fChildPID == nil) )
				{
					doFree = true;
				}
			}
			
			if (doFree)
			{
				if ( pTable->fTableData[ uiSlot ] != nil )
				{
					if ( uiRefNum == pTable->fTableData[ uiSlot ]->fRefNum )
					{
						// need to callback to make sure that the plug-ins clean up
						pCurrRef = pTable->fTableData[ uiSlot ];
						pTable->fTableData[ uiSlot ] = nil;
						pTable->fItemCnt--;
						//set counter even if plugin itself fails to cleanup ie. tracking the ds server ref counts here specifically
						if (fDeallocProc != nil)
						{
							// need to make sure we release the table mutex before the callback
							// since the search node will try to close dir nodes
							//this Signal works in conjunction with others to handle the recursive nature of the calls here
							//note that since RemoveRef is potentially highly recursive we don't want to run into a mutex deadlock when
							//we employ different threads to do the cleanup inside the fDeallocProc like in the case of the search node
							gFWRefMapMutex->Signal();
							dsResult = (tDirStatus)(*fDeallocProc)(inRefNum, pCurrRef);
							gFWRefMapMutex->Wait();
						}
#ifdef __LITTLE_ENDIAN__
						//cleanup the servertolocalrefmap if required
						RemoveServerToLocalRefMap(pCurrRef->fRemoteRefNum);
#endif						
						free(pCurrRef);
						pCurrRef = nil;
					}
				}
			}
		}
			
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	gFWRefMapMutex->Signal();

	return( dsResult );

} // RemoveRef


//------------------------------------------------------------------------------------
//	* RemoveChildren
//------------------------------------------------------------------------------------

void CDSRefMap::RemoveChildren ( sListFWInfo *inChildList, sInt32 inPID )
{
	sListFWInfo	   *pCurrChild		= nil;
	sListFWInfo	   *pNextChild		= nil;
//	sFWRefMapEntry	   *pCurrRef		= nil;

	gFWRefMapMutex->Wait();

	try
	{
		pCurrChild = inChildList;

		//walk the child list of refs
		while ( pCurrChild != nil )
		{
			//we need to get the next entry first before RemoveRef since it will call UnlinkParent
			//which in turn will delete the current entry out from under us
			pNextChild = pCurrChild->fNext;

			//remove ref if it matches the inPID
			if ( pCurrChild->fPID == inPID )
			{
				gFWRefMapMutex->Signal();
				RemoveRef( pCurrChild->fRefNum, pCurrChild->fType, inPID );
				gFWRefMapMutex->Wait();
			}

			pCurrChild = pNextChild;
		}
	}

	catch( sInt32 err )
	{
	}

	gFWRefMapMutex->Signal();

} // RemoveChildren


//------------------------------------------------------------------------------------
//	* AddChildPIDToRef (static)
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap:: AddChildPIDToRef ( uInt32 inRefNum, uInt32 inParentPID, sInt32 inChildPID )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefMapEntry	   *pCurrRef		= nil;
	sPIDFWInfo	   *pChildPIDInfo	= nil;

	gFWRefMapMutex->Wait();

	try
	{
		dsResult = gFWRefMap->VerifyReference( inRefNum, eNodeRefType, inParentPID );
		if ( dsResult != eDSNoErr ) throw( (sInt32)dsResult );

		pCurrRef = gFWRefMap->GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		pChildPIDInfo = (sPIDFWInfo *)::calloc( 1, sizeof( sPIDFWInfo ) );
		if ( pChildPIDInfo == nil ) throw( (sInt32)eDSRefTableFWAllocError );

		// Save the info required for verification of ref
		pChildPIDInfo->fPID = inChildPID;

		// Set the link info
		pChildPIDInfo->fNext = pCurrRef->fChildPID;
		pCurrRef->fChildPID = pChildPIDInfo;
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	gFWRefMapMutex->Signal();

	return( dsResult );

} // AddChildPIDToRef


//------------------------------------------------------------------------------------
//	* SetRemoteRefNum
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::SetRemoteRefNum ( uInt32 inRefNum, uInt32 inType, uInt32 inRemoteRefNum, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;
	sFWRefMapEntry	   *pCurrRef	= nil;

	if (gFWRefMap != nil)
	{
        siResult = gFWRefMap->VerifyReference( inRefNum, inType, inPID );
        
        if (siResult == eDSNoErr)
        {
            pCurrRef = gFWRefMap->GetTableRef( inRefNum );
            
            siResult = eDSInvalidReference;
            if ( pCurrRef != nil )
            {
                pCurrRef->fRemoteRefNum = inRemoteRefNum;
                siResult = eDSNoErr;
            }
        }
    }
    
	return( siResult );

} // SetRemoteRefNum


//------------------------------------------------------------------------------------
//	* SetMessageTableIndex
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::SetMessageTableIndex ( uInt32 inRefNum, uInt32 inType, uInt32 inMsgTableIndex, sInt32 inPID )
{
	tDirStatus			siResult	= eDSDirSrvcNotOpened;
	sFWRefMapEntry	   *pCurrRef	= nil;

	if (gFWRefMap != nil)
	{
        siResult = gFWRefMap->VerifyReference( inRefNum, inType, inPID );
        
        if (siResult == eDSNoErr)
        {
            pCurrRef = gFWRefMap->GetTableRef( inRefNum );
            
            siResult = eDSInvalidReference;
            if ( pCurrRef != nil )
            {
                pCurrRef->fMessageTableIndex = inMsgTableIndex;
                siResult = eDSNoErr;
            }
        }
    }
    
	return( siResult );

} // SetMessageTableIndex


//------------------------------------------------------------------------------------
//	* SetPluginName
//------------------------------------------------------------------------------------

tDirStatus CDSRefMap::SetPluginName ( uInt32 inRefNum, uInt32 inType, char* inPluginName, sInt32 inPID )
{
	tDirStatus			siResult	= eDSDirSrvcNotOpened;
	sFWRefMapEntry	   *pCurrRef	= nil;

	if (gFWRefMap != nil)
	{
        siResult = gFWRefMap->VerifyReference( inRefNum, inType, inPID );
        
        if (siResult == eDSNoErr)
        {
            pCurrRef = gFWRefMap->GetTableRef( inRefNum );
            
            siResult = eDSInvalidReference;
            if ( pCurrRef != nil )
            {
                pCurrRef->fPluginName = inPluginName;
                siResult = eDSNoErr;
            }
        }
    }
    
	return( siResult );

} // SetPluginName


//------------------------------------------------------------------------------------
//	* GetRefNum
//------------------------------------------------------------------------------------

uInt32 CDSRefMap::GetRefNum ( uInt32 inRefNum, uInt32 inType, sInt32 inPID )
{
	sInt32				siResult	= eDSNoErr;
	uInt32				theRefNum	= inRefNum; //return the input if not found here
	sFWRefMapEntry	   *pCurrRef	= nil;

	if ((inRefNum & 0x00C00000) != 0)
	{
		if (gFWRefMap != nil)
		{
			siResult = gFWRefMap->VerifyReference( inRefNum, inType, inPID );
			if (siResult == eDSNoErr)
			{
				pCurrRef = gFWRefMap->GetTableRef( inRefNum );
				if ( pCurrRef != nil )
				{
					theRefNum = pCurrRef->fRemoteRefNum;
				}
			}
		}
	}
    
	return( theRefNum );

} // GetRefNum


//------------------------------------------------------------------------------------
//	* GetMessageTableIndex
//------------------------------------------------------------------------------------

uInt32 CDSRefMap::GetMessageTableIndex ( uInt32 inRefNum, uInt32 inType, sInt32 inPID )
{
	sInt32				siResult			= eDSNoErr;
	uInt32				theMsgTableIndex	= 0; //return zero if not remote connection
	sFWRefMapEntry	   *pCurrRef			= nil;

	if ((inRefNum & 0x00C00000) != 0)
	{
		if (gFWRefMap != nil)
		{
			siResult = gFWRefMap->VerifyReference( inRefNum, inType, inPID );
			if (siResult == eDSNoErr)
			{
				pCurrRef = gFWRefMap->GetTableRef( inRefNum );
				if ( pCurrRef != nil )
				{
					theMsgTableIndex = pCurrRef->fMessageTableIndex;
				}
			}
		}
	}
    
	return( theMsgTableIndex );

} // GetMessageTableIndex


//------------------------------------------------------------------------------------
//	* GetPluginName
//------------------------------------------------------------------------------------

char* CDSRefMap::GetPluginName( uInt32 inRefNum, sInt32 inPID )
{
	sInt32				siResult			= eDSNoErr;
	sFWRefMapEntry	   *pCurrRef			= nil;
	char			   *outPluginName		= nil;

	if ((inRefNum & 0x00C00000) != 0)
	{
		if (gFWRefMap != nil)
		{
			siResult = gFWRefMap->VerifyReference( inRefNum, eNodeRefType, inPID );
			if (siResult == eDSNoErr)
			{
				pCurrRef = gFWRefMap->GetTableRef( inRefNum );
				if ( pCurrRef != nil )
				{
					outPluginName = pCurrRef->fPluginName;
				}
			}
		}
	}
    
	return( outPluginName );

} // GetPluginName


//------------------------------------------------------------------------------------
//	* GetRefNumMap
//------------------------------------------------------------------------------------

uInt32 CDSRefMap::GetRefNumMap ( uInt32 inRefNum, uInt32 inType, sInt32 inPID )
{
	sInt32				siResult	= eDSNoErr;
	uInt32				theRefNum	= inRefNum; //return the input if not found here
	sFWRefMapEntry	   *pCurrRef	= nil;

	if ((inRefNum & 0x00C00000) != 0)
	{
		if (gFWRefMap != nil)
		{
			siResult = gFWRefMap->VerifyReference( inRefNum, inType, inPID );
			if (siResult == eDSNoErr)
			{
				pCurrRef = gFWRefMap->GetTableRef( inRefNum );
				if ( pCurrRef != nil )
				{
					theRefNum = pCurrRef->fRemoteRefNum;
				}
			}
		}
	}
    
	return( theRefNum );

} // GetRefNumMapMap


#ifdef __LITTLE_ENDIAN__

//------------------------------------------------------------------------------------
//	* MapServerRefToLocalRef
//------------------------------------------------------------------------------------

void CDSRefMap::MapServerRefToLocalRef( uInt32 inServerRef, uInt32 inLocalRef )
{
	if (inServerRef != 0 && inLocalRef != 0)
	{
		//add this to the fServerToLocalRefMap
		fServerToLocalRefMap[inServerRef] = inLocalRef;
	}
	return;
} // MapServerRefToLocalRef


//------------------------------------------------------------------------------------
//	* RemoveServerToLocalRefMap
//------------------------------------------------------------------------------------

void CDSRefMap::RemoveServerToLocalRefMap( uInt32 inServerRef )
{
	if (inServerRef != 0)
	{
		tRefMapI aRefMapI;

//do not think that we need to layer this to add a mutex for this map

		aRefMapI	= fServerToLocalRefMap.find(inServerRef);	

		// if it was found, then let's remove it
		if (aRefMapI != fServerToLocalRefMap.end())
		{
			fServerToLocalRefMap.erase(aRefMapI);
		}
	}
	return;
} // RemoveServerToLocalRefMap


//------------------------------------------------------------------------------------
//	* GetLocalRefFromServerMap
//------------------------------------------------------------------------------------

uInt32 CDSRefMap::GetLocalRefFromServerMap( uInt32 inServerRef )
{
	uInt32		retVal = 0;

	if (inServerRef != 0)
	{
		tRefMapI	aRefMapI;
		aRefMapI	= fServerToLocalRefMap.find(inServerRef);	

		// if it was found, then return it
		if (aRefMapI != fServerToLocalRefMap.end())
		{
			retVal = aRefMapI->second;
		}
	}
	return(retVal);
} // GetLocalRefFromServerMap


//------------------------------------------------------------------------------------
//	* MapMsgIDToServerRef
//------------------------------------------------------------------------------------

void CDSRefMap::MapMsgIDToServerRef( uInt32 inMsgID, uInt32 inServerRef )
{
	if (inMsgID != 0 && inServerRef != 0)
	{
		//add this to the fMsgIDToServerRefMap
		fMsgIDToServerRefMap[inMsgID] = inServerRef;
	}
	return;
} // MapMsgIDToServerRef


//------------------------------------------------------------------------------------
//	* RemoveMsgIDToServerRefMap
//------------------------------------------------------------------------------------

void CDSRefMap::RemoveMsgIDToServerRefMap( uInt32 inMsgID )
{
	if (inMsgID != 0)
	{
		tRefMapI aRefMapI;

//do not think that we need to layer this to add a mutex for this map

		aRefMapI	= fMsgIDToServerRefMap.find(inMsgID);	

		// if it was found, then let's remove it
		if (aRefMapI != fMsgIDToServerRefMap.end())
		{
			fMsgIDToServerRefMap.erase(aRefMapI);
		}
	}
	return;
} // RemoveMsgIDToServerRefMap


//------------------------------------------------------------------------------------
//	* GetServerRefFromMsgIDMap
//------------------------------------------------------------------------------------

uInt32 CDSRefMap::GetServerRefFromMsgIDMap( uInt32 inMsgID )
{
	uInt32		retVal = 0;

	if (inMsgID != 0)
	{
		tRefMapI	aRefMapI;
		aRefMapI	= fMsgIDToServerRefMap.find(inMsgID);	

		// if it was found, then return it
		if (aRefMapI != fMsgIDToServerRefMap.end())
		{
			retVal = aRefMapI->second;
		}
	}
	return(retVal);
} // GetServerRefFromMsgIDMap


//------------------------------------------------------------------------------------
//	* MapMsgIDToCustomCode
//------------------------------------------------------------------------------------

void CDSRefMap::MapMsgIDToCustomCode( uInt32 inMsgID, uInt32 inCustomCode )
{
	if (inMsgID != 0 && inCustomCode != 0)
	{
		//add this to the fMsgIDToCustomCodeMap
		fMsgIDToCustomCodeMap[inMsgID] = inCustomCode;
	}
	return;
} // MapMsgIDToCustomCode


//------------------------------------------------------------------------------------
//	* RemoveMsgIDToCustomCodeMap
//------------------------------------------------------------------------------------

void CDSRefMap::RemoveMsgIDToCustomCodeMap( uInt32 inMsgID )
{
	if (inMsgID != 0)
	{
		tRefMapI aRefMapI;

//do not think that we need to layer this to add a mutex for this map

		aRefMapI	= fMsgIDToCustomCodeMap.find(inMsgID);	

		// if it was found, then let's remove it
		if (aRefMapI != fMsgIDToCustomCodeMap.end())
		{
			fMsgIDToCustomCodeMap.erase(aRefMapI);
		}
	}
	return;
} // RemoveMsgIDToCustomCodeMap


//------------------------------------------------------------------------------------
//	* GetCustomCodeFromMsgIDMap
//------------------------------------------------------------------------------------

uInt32 CDSRefMap::GetCustomCodeFromMsgIDMap( uInt32 inMsgID )
{
	uInt32		retVal = 0;

	if (inMsgID != 0)
	{
		tRefMapI	aRefMapI;
		aRefMapI	= fMsgIDToCustomCodeMap.find(inMsgID);	

		// if it was found, then return it
		if (aRefMapI != fMsgIDToCustomCodeMap.end())
		{
			retVal = aRefMapI->second;
		}
	}
	return(retVal);
} // GetCustomCodeFromMsgIDMap

#endif
