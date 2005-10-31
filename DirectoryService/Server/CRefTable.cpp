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
 * @header CRefTable
 */

#include "CRefTable.h"
#include "CLog.h"
#include "DSNetworkUtilities.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysctl.h>	// for struct kinfo_proc and sysctl()
#include <syslog.h>		// for syslog()

using namespace std;

//--------------------------------------------------------------------------------------------------
//	* Globals
//--------------------------------------------------------------------------------------------------

//API logging
extern dsBool					gLogAPICalls;
extern uInt32					gRefCountWarningLimit;
extern uInt32					gDaemonIPAddress;

//------------------------------------------------------------------------------------
//	* CRefTable
//------------------------------------------------------------------------------------

CRefTable::CRefTable ( RefDeallocateProc *deallocProc )
{
	fRefCleanUpEntriesHead  = nil;
	fRefCleanUpEntriesTail  = nil;
	
	fClientPIDListLock = new DSMutexSemaphore();
	fTableCount		= 0;
	::memset( fRefTables, 0, sizeof( fRefTables ) );

	fDeallocProc = deallocProc;
} // CRefTable


//------------------------------------------------------------------------------------
//	* ~CRefTable
//------------------------------------------------------------------------------------

CRefTable::~CRefTable ( void )
{
	uInt32		i	= 1;
	uInt32		j	= 1;

	for ( i = 1; i <= kMaxTables; i++ )	//KW80 - note that array is still zero based even if first entry NOT used
										//-- added the last kMaxTable in the .h file so this should work now
	{
		if ( fRefTables[ i ] != nil )
		{
			for (j=0; j< kMaxTableItems; j++)
			{
				if (fRefTables[ i ]->fTableData[j] != nil)
				{
					free(fRefTables[ i ]->fTableData[j]);
					fRefTables[ i ]->fTableData[j] = nil;
				}
			}
			free( fRefTables[ i ] );  //KW80 need to free all the calloc'ed sRefEntries inside as well - code above
			fRefTables[ i ] = nil;
		}
	}

	delete(fClientPIDListLock);
	fClientPIDListLock = nil;
} // ~CRefTable


//--------------------------------------------------------------------------------------------------
//	* Lock
//
//--------------------------------------------------------------------------------------------------

void CRefTable::Lock (  )
{
	fTableMutex.Wait();
}


//--------------------------------------------------------------------------------------------------
//	* Unlock
//
//--------------------------------------------------------------------------------------------------

void CRefTable::Unlock (  )
{
	fTableMutex.Signal();
}


//--------------------------------------------------------------------------------------------------
//	* VerifyReference
//
//--------------------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyReference ( tDirReference	inDirRef,
										uInt32			inType,
										CServerPlugin **outPlugin,
										sInt32			inPID,
										uInt32			inIPAddress,
										bool			inDaemonPID_OK )
{
	tDirStatus		siResult	= eDSNoErr;
	sRefEntry	   *refData		= nil;
	sPIDInfo	   *pPIDInfo	= nil;

	siResult = GetReference( inDirRef, &refData );
	//ref actually exists
	if ( siResult == eDSNoErr )
	{
		//check if the type is correct
		if ( refData->fType != inType )
		{
			DBGLOG1( kLogHandler, "Given reference value of <%u> found but does not match reference type.", inDirRef);
			siResult = eDSInvalidRefType;
		}
		else
		{
			//check the PIDs and IPAddresses here - both parent and child client ones
			siResult = eDSInvalidRefType;
			//parent client PID and IPAddress check
			if ( inDaemonPID_OK && inPID == 0 )
			{
				siResult = eDSNoErr;
			}
			else if ( (refData->fPID == inPID) && (refData->fIPAddress == inIPAddress) )
			{
				siResult = eDSNoErr;
			}
			else
			{
				//child clients PID and IPAddress check
				pPIDInfo = refData->fChildPID;
				while ( (pPIDInfo != nil) && (siResult != eDSNoErr) )
				{
					if ( (pPIDInfo->fPID == inPID) && (pPIDInfo->fIPAddress == inIPAddress) )
					{
						siResult = eDSNoErr;
					}
					pPIDInfo = pPIDInfo->fNext;
				}
			}
			if (siResult == eDSNoErr)
			{
				if ( outPlugin != nil )
				{
					*outPlugin = refData->fPlugin;
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

tDirStatus CRefTable::VerifyDirRef (	tDirReference		inDirRef,
										CServerPlugin	  **outPlugin,
										sInt32				inPID,
										uInt32				inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->VerifyReference( inDirRef, eDirectoryRefType, outPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // VerifyDirRef



//------------------------------------------------------------------------------------
//	* VerifyNodeRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyNodeRef (	tDirNodeReference	inDirNodeRef,
										CServerPlugin	  **outPlugin,
										sInt32				inPID,
										uInt32				inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->VerifyReference( inDirNodeRef, eNodeRefType, outPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // VerifyNodeRef


//------------------------------------------------------------------------------------
//	* VerifyRecordRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyRecordRef (	tRecordReference	inRecordRef,
										CServerPlugin	  **outPlugin,
										sInt32				inPID,
										uInt32				inIPAddress,
										bool				inDaemonPID_OK )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->VerifyReference( inRecordRef, eRecordRefType, outPlugin, inPID, inIPAddress, inDaemonPID_OK );
	}

	return( siResult );

} // VerifyRecordRef



//------------------------------------------------------------------------------------
//	* VerifyAttrListRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyAttrListRef (	tAttributeListRef	inAttributeListRef,
											CServerPlugin	  **outPlugin,
											sInt32				inPID,
											uInt32				inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->VerifyReference( inAttributeListRef, eAttrListRefType, outPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // VerifyAttrListRef


//------------------------------------------------------------------------------------
//	* VerifyAttrValueRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyAttrValueRef (	tAttributeValueListRef	inAttributeValueListRef,
											CServerPlugin		  **outPlugin,
											sInt32					inPID,
											uInt32					inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->VerifyReference( inAttributeValueListRef, eAttrValueListRefType, outPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // VerifyAttrValueRef


//------------------------------------------------------------------------------------
//	* NewDirRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::NewDirRef (	uInt32	   *outNewRef,
									sInt32 		inPID,
									uInt32		inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->GetNewRef( outNewRef, 0, eDirectoryRefType, nil, inPID, inIPAddress );
	}

	return( siResult );

} // NewDirRef


//------------------------------------------------------------------------------------
//	* NewNodeRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::NewNodeRef (	uInt32			*outNewRef,
									CServerPlugin	*inPlugin,
									uInt32			inParentID,
									sInt32			inPID,
									uInt32			inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->GetNewRef( outNewRef, inParentID, eNodeRefType, inPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // NewNodeRef


//------------------------------------------------------------------------------------
//	* NewRecordRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::NewRecordRef (	uInt32		   *outNewRef,
										CServerPlugin  *inPlugin,
										uInt32			inParentID,
										sInt32			inPID,
										uInt32			inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->GetNewRef( outNewRef, inParentID, eRecordRefType, inPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // NewRecordRef


//------------------------------------------------------------------------------------
//	* NewAttrListRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::NewAttrListRef (	uInt32			*outNewRef,
										CServerPlugin	*inPlugin,
										uInt32			inParentID,
										sInt32			inPID,
										uInt32			inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->GetNewRef( outNewRef, inParentID, eAttrListRefType, inPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // NewAttrListRef


//------------------------------------------------------------------------------------
//	* NewAttrValueRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::NewAttrValueRef ( uInt32			*outNewRef,
									 	CServerPlugin	*inPlugin,
									 	uInt32			inParentID,
										sInt32			inPID,
										uInt32			inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->GetNewRef( outNewRef, inParentID, eAttrValueListRefType, inPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // NewAttrValueRef


//------------------------------------------------------------------------------------
//	* RemoveDirRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveDirRef (	uInt32	inDirRef,
										sInt32	inPID,
										uInt32	inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->RemoveRef( inDirRef, eDirectoryRefType, inPID, inIPAddress, true );
	}

	return( siResult );

} // RemoveDirRef


//------------------------------------------------------------------------------------
//	* RemoveNodeRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveNodeRef (	uInt32	inNodeRef,
										sInt32	inPID,
										uInt32	inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->RemoveRef( inNodeRef, eNodeRefType, inPID, inIPAddress, true );
	}

	return( siResult );

} // RemoveNodeRef


//------------------------------------------------------------------------------------
//	* RemoveRecordRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveRecordRef (	uInt32	inRecRef,
										sInt32	inPID,
										uInt32	inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->RemoveRef( inRecRef, eRecordRefType, inPID, inIPAddress, true );
	}

	return( siResult );

} // RemoveRecordRef


//------------------------------------------------------------------------------------
//	* RemoveAttrListRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveAttrListRef (	uInt32	inAttrListRef,
											sInt32	inPID,
											uInt32	inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->RemoveRef( inAttrListRef, eAttrListRefType, inPID, inIPAddress, true );
	}

	return( siResult );

} // RemoveAttrListRef


//------------------------------------------------------------------------------------
//	* RemoveAttrValueRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveAttrValueRef (	uInt32	inAttrValueRef,
											sInt32	inPID,
											uInt32	inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->RemoveRef( inAttrValueRef, eAttrValueListRefType, inPID, inIPAddress, true );
	}

	return( siResult );

} // RemoveAttrValueRef


//------------------------------------------------------------------------------------
//	* SetNodePluginPtr
//------------------------------------------------------------------------------------

tDirStatus CRefTable::SetNodePluginPtr ( tDirNodeReference inNodeRef,
										 CServerPlugin *inPlugin )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->SetPluginPtr( inNodeRef, eNodeRefType, inPlugin );
	}

	return( siResult );

} // SetNodePluginPtr


//------------------------------------------------------------------------------------
//	* GetTableRef
//------------------------------------------------------------------------------------

sRefEntry* CRefTable::GetTableRef ( uInt32 inRefNum )
{
	uInt32			uiSlot			= 0;
	uInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	uInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	sRefTable	   *pTable			= nil;
	sRefEntry	   *pOutEntry		= nil;

	fTableMutex.Wait();

	try
	{
		pTable = GetThisTable( uiTableNum );
		if ( pTable == nil ) throw( (sInt32)eDSInvalidReference );

		uiSlot = uiRefNum % kMaxTableItems;
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
		DBGLOG2( kLogAssert, "Reference %l error = %l", inRefNum, err );
	}

	fTableMutex.Signal();

	return( pOutEntry );

} // GetTableRef


//------------------------------------------------------------------------------------
//	* GetNextTable
//------------------------------------------------------------------------------------

sRefTable* CRefTable::GetNextTable ( sRefTable *inTable )
{
	uInt32		uiTblNum	= 0;
	sRefTable	*pOutTable	= nil;

	fTableMutex.Wait();

	try
	{
		if ( inTable == nil )
		{
			// Get the first reference table, create it if it wasn't already
			if ( fRefTables[ 1 ] == nil )
			{
				// No tables have been allocated yet so lets make one
				fRefTables[ 1 ] = (sRefTable *)::calloc( sizeof( sRefTable ), sizeof( char ) );
				if ( fRefTables[ 1 ] == nil ) throw((sInt32)eMemoryAllocError);

				fRefTables[ 1 ]->fTableNum = 1;

				fTableCount = 1;
			}

			pOutTable = fRefTables[ 1 ];
		}
		else
		{
			uiTblNum = inTable->fTableNum + 1;
			if (uiTblNum > kMaxTables) throw( (sInt32)eDSInvalidReference );

			if ( fRefTables[ uiTblNum ] == nil )
			{
				// Set the table counter
				fTableCount = uiTblNum;

				// No tables have been allocated yet so lets make one
				fRefTables[ uiTblNum ] = (sRefTable *)::calloc( sizeof( sRefTable ), sizeof( char ) );
				if ( fRefTables[ uiTblNum ] == nil ) throw((sInt32)eMemoryAllocError);

				if (uiTblNum == 0) throw( (sInt32)eDSInvalidReference );
				fRefTables[ uiTblNum ]->fTableNum = uiTblNum;
			}

			pOutTable = fRefTables[ uiTblNum ];
		}
	}

	catch( sInt32 err )
	{
		DBGLOG1( kLogAssert, "Reference table error = %l", err );
	}

	fTableMutex.Signal();

	return( pOutTable );

} // GetNextTable


//------------------------------------------------------------------------------------
//	* GetThisTable
//------------------------------------------------------------------------------------

sRefTable* CRefTable::GetThisTable ( uInt32 inTableNum )
{
	sRefTable	*pOutTable	= nil;

	fTableMutex.Wait();

	pOutTable = fRefTables[ inTableNum ];

	fTableMutex.Signal();

	return( pOutTable );

} // GetThisTable


//------------------------------------------------------------------------------------
//	* GetNewRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::GetNewRef (	uInt32		   *outRef,
									uInt32			inParentID,
									eRefTypes		inType,
									CServerPlugin  *inPlugin,
									sInt32			inPID,
									uInt32			inIPAddress )
{
	bool			done		= false;
	tDirStatus		outResult	= eDSNoErr;
	sRefTable	   *pCurTable	= nil;
	uInt32			uiRefNum	= 0;
	uInt32			uiCntr		= 0;
	uInt32			uiSlot		= 0;
	uInt32			uiTableNum	= 0;
	uInt32			refCountUpdate	= 0;

	fTableMutex.Wait();

	try
	{
		*outRef = 0;

		while ( !done )
		{
			pCurTable = GetNextTable( pCurTable );
			if ( pCurTable == nil ) throw( (sInt32)eDSRefTableAllocError );

			if ( pCurTable->fItemCnt < kMaxTableItems )
			{
				uiCntr = 0;
				uiTableNum = pCurTable->fTableNum;
				while ( (uiCntr < kMaxTableItems) && !done )	//KW80 - uiCntr was a condition never used
																//fixed below with uiCntr++; code addition
				{
					if ( (pCurTable->fCurRefNum == 0) || 
						 (pCurTable->fCurRefNum > 0x000FFFFF) )
					{
						// Either it's the first reference for this table or we have
						//	used 1024 * 1024 references and need to start over
						pCurTable->fCurRefNum = 1;
					}

					// Get the ref num and increment its place holder
					uiRefNum = pCurTable->fCurRefNum++;

					// Find a slot in the table for this ref number
					uiSlot = uiRefNum % kMaxTableItems;
					if ( pCurTable->fTableData[ uiSlot ] == nil )
					{
						pCurTable->fTableData[ uiSlot ] = (sRefEntry *)::calloc( sizeof( sRefEntry ), sizeof( char ) );
						if ( pCurTable->fTableData[ uiSlot ] == nil ) throw( (sInt32)eDSRefTableAllocError );
						
						// We found an empty slot, now set this table entry
						pCurTable->fTableData[ uiSlot ]->fRefNum		= uiRefNum;
						pCurTable->fTableData[ uiSlot ]->fType			= inType;
						pCurTable->fTableData[ uiSlot ]->fParentID		= inParentID;
						pCurTable->fTableData[ uiSlot ]->fPID			= inPID;
						pCurTable->fTableData[ uiSlot ]->fIPAddress		= inIPAddress;
						pCurTable->fTableData[ uiSlot ]->fPlugin		= inPlugin;
						pCurTable->fTableData[ uiSlot ]->fChildren		= nil;
						pCurTable->fTableData[ uiSlot ]->fChildPID		= nil;

						// Add the table number to the reference number
						uiTableNum = (uiTableNum << 24);
						uiRefNum = uiRefNum | uiTableNum;

						*outRef = uiRefNum;

						// Up the item count
						pCurTable->fItemCnt++;

						outResult = eDSNoErr;
						done = true;
						if (inType == eDirectoryRefType)
						{
							refCountUpdate = UpdateClientPIDRefCount(inPID, inIPAddress, true, uiRefNum);
						}
						else
						{
							refCountUpdate = UpdateClientPIDRefCount(inPID, inIPAddress, true);
						}
					}
					uiCntr++;	//KW80 needed for us to only go through the table once
								//ie the uiCntr does not get used directly BUT the uiRefNum gets
								//incremented only kMaxTableItems times since uiCntr is in the while condition
				}
			}
		}

		if ( inParentID != 0 )
		{
			outResult = LinkToParent( *outRef, inType, inParentID, inPID, inIPAddress );
		}
	}

	catch( sInt32 err )
	{
		outResult = (tDirStatus)err;
	}

	fTableMutex.Signal();

	return( outResult );

} // GetNewRef


//------------------------------------------------------------------------------------
//	* LinkToParent
//------------------------------------------------------------------------------------

tDirStatus CRefTable::LinkToParent (	uInt32	inRefNum,
										uInt32	inType,
										uInt32	inParentID,
										sInt32	inPID,
										uInt32	inIPAddress )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;
	sListInfo	   *pChildInfo		= nil;

	fTableMutex.Wait();

	try
	{
		pCurrRef = GetTableRef( inParentID );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		// This is the one we want
		pChildInfo = (sListInfo *)::calloc( sizeof( sListInfo ), sizeof( char ) );
		if ( pChildInfo == nil ) throw( (sInt32)eDSRefTableAllocError );

		// Save the info required later for removal if the parent gets removed
		pChildInfo->fRefNum		= inRefNum;
		pChildInfo->fType		= inType;
		pChildInfo->fPID		= inPID;
		pChildInfo->fIPAddress	= inIPAddress;

		// Set the link info
		pChildInfo->fNext = pCurrRef->fChildren;
		pCurrRef->fChildren = pChildInfo;
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	fTableMutex.Signal();

	return( dsResult );

} // LinkToParent


//------------------------------------------------------------------------------------
//	* UnlinkFromParent
//------------------------------------------------------------------------------------

tDirStatus CRefTable::UnlinkFromParent ( uInt32 inRefNum )
{
	tDirStatus		dsResult		= eDSNoErr;
	uInt32			i				= 1;
	uInt32			parentID		= 0;
	sRefEntry	   *pCurrRef		= nil;
	sRefEntry	   *pParentRef		= nil;
	sListInfo	   *pCurrChild		= nil;
	sListInfo	   *pPrevChild		= nil;

	fTableMutex.Wait();

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

	fTableMutex.Signal();

	return( dsResult );

} // UnlinkFromParent


//------------------------------------------------------------------------------------
//	* GetReference
//------------------------------------------------------------------------------------

tDirStatus CRefTable::GetReference ( uInt32 inRefNum, sRefEntry **outRefData )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;

	fTableMutex.Wait();

	try
	{
		pCurrRef = GetTableRef( inRefNum );
		if ( pCurrRef == nil )
		{
			DBGLOG1( kLogHandler, "Given reference value of <%u> has no valid reference table entry.", inRefNum);
			throw( (sInt32)eDSInvalidReference );
		}

		*outRefData = pCurrRef;
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	fTableMutex.Signal();

	return( dsResult );

} // GetReference


//------------------------------------------------------------------------------------
//	* RemoveRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveRef (	uInt32	inRefNum,
									uInt32	inType,
									sInt32	inPID,
									uInt32	inIPAddress,
									bool	inbAtTop)
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;
	sRefTable	   *pTable			= nil;
	uInt32			uiSlot			= 0;
	uInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	uInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	bool			doFree			= false;
	sPIDInfo	   *pPIDInfo		= nil;
	sPIDInfo	   *pPrevPIDInfo	= nil;
	uInt32			refCountUpdate	= 0;
	sRefCleanUpEntry	   *aRefCleanUpEntries		= nil;
	sRefCleanUpEntry	   *aRefCleanUpEntriesIter  = nil;
	sInt32			deallocResult   = eDSNoErr;


	fTableMutex.Wait();

	try
	{
		dsResult = VerifyReference( inRefNum, inType, nil, inPID, inIPAddress );

		if ( dsResult == eDSNoErr )
		{
			pTable = GetThisTable( uiTableNum );
			if ( pTable == nil ) throw( (sInt32)eDSInvalidReference );

			uiSlot = uiRefNum % kMaxTableItems;

			if ( inType != eDirectoryRefType ) // API refs have no parents
			{
				dsResult = UnlinkFromParent( inRefNum );
				if ( dsResult != eDSNoErr ) throw( (sInt32)dsResult );
			}

			pCurrRef = GetTableRef( inRefNum );	//KW80 - here we need to know where the sRefEntry is in the table to delete it
												//that is all the code added above
												// getting this sRefEntry is the same path used by uiSlot and pTable above
			if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );
			if (inType != pCurrRef->fType) throw( (sInt32)eDSInvalidReference );

			if ( pCurrRef->fChildren != nil )
			{
				RemoveChildren( pCurrRef->fChildren, inPID, inIPAddress );
			}

			//Now we check to see if this was a child or parent client PID and IPAddress that we removed - only applies to case of Node refs really
			if ( (pCurrRef->fPID == inPID) && (pCurrRef->fIPAddress == inIPAddress) )
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
					if ( (pPIDInfo->fPID == inPID) && (pPIDInfo->fIPAddress == inIPAddress) )
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
						if (inType == eDirectoryRefType)
						{
							refCountUpdate = UpdateClientPIDRefCount(inPID, inIPAddress, false, inRefNum);
						}
						else
						{
							refCountUpdate = UpdateClientPIDRefCount(inPID, inIPAddress, false);
						}
						if (fDeallocProc != nil)
						{
							//here we add the ref to be released in the plugin to our list of ref cleanup entries
							//note that since RemoveRef is potentially highly recursive we need to make these release calls later after
							//giving up the mutex for this ref table
							if (fRefCleanUpEntriesHead == nil)
							{
								//this is the first one
								fRefCleanUpEntriesHead					=   (sRefCleanUpEntry *)calloc(1, sizeof(sRefCleanUpEntry));
								fRefCleanUpEntriesHead->fRefNum			=   inRefNum; //==pCurrRef->fRefNum
								fRefCleanUpEntriesHead->fType			=   pCurrRef->fType;
								fRefCleanUpEntriesHead->fPlugin			=   pCurrRef->fPlugin;
								fRefCleanUpEntriesHead->fNext			=   nil;
								fRefCleanUpEntriesTail					=   fRefCleanUpEntriesHead;
							}
							else
							{
								fRefCleanUpEntriesTail->fNext			=   (sRefCleanUpEntry *)calloc(1, sizeof(sRefCleanUpEntry));
								fRefCleanUpEntriesTail->fNext->fRefNum  =   inRefNum; //==pCurrRef->fRefNum
								fRefCleanUpEntriesTail->fNext->fType	=   pCurrRef->fType;
								fRefCleanUpEntriesTail->fNext->fPlugin  =   pCurrRef->fPlugin;
								fRefCleanUpEntriesTail->fNext->fNext	=   nil;
								fRefCleanUpEntriesTail					=   fRefCleanUpEntriesTail->fNext;
							}
						}
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

	if (inbAtTop) //this tells us that we are at the top of the recursive stack in removing children and need to cleanup refs
	{
		aRefCleanUpEntriesIter	= fRefCleanUpEntriesHead;
		fRefCleanUpEntriesHead  = nil;
		fRefCleanUpEntriesTail  = nil;
	}

	fTableMutex.Signal();
	
	if (inbAtTop) //we can now clean up
	{
		while (aRefCleanUpEntriesIter != nil)
		{
			aRefCleanUpEntries = aRefCleanUpEntriesIter;
			if (aRefCleanUpEntries->fPlugin != nil)
			{
				//call the dealloc routine but don't bother to pass back the status
				deallocResult = (tDirStatus)(*fDeallocProc)( aRefCleanUpEntries->fRefNum, aRefCleanUpEntries->fType, aRefCleanUpEntries->fPlugin );
			}
			aRefCleanUpEntriesIter = aRefCleanUpEntries->fNext;
			free(aRefCleanUpEntries);
		}
	}

	return( dsResult );

} // RemoveRef


//------------------------------------------------------------------------------------
//	* RemoveChildren
//------------------------------------------------------------------------------------

void CRefTable::RemoveChildren (	sListInfo	   *inChildList,
									sInt32			inPID,
									uInt32			inIPAddress )
{
	sListInfo	   *pCurrChild		= nil;
	sListInfo	   *pNextChild		= nil;

	fTableMutex.Wait();

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
			if ( ( pCurrChild->fPID == inPID ) && ( pCurrChild->fIPAddress == inIPAddress ) )
			{
				RemoveRef( pCurrChild->fRefNum, pCurrChild->fType, inPID, inIPAddress, false );
			}

			pCurrChild = pNextChild;
		}
	}

	catch( sInt32 err )
	{
	}

	fTableMutex.Signal();

} // RemoveChildren



//------------------------------------------------------------------------------------
//	* SetPluginPtr
//------------------------------------------------------------------------------------

tDirStatus CRefTable::SetPluginPtr ( uInt32 inRefNum, uInt32 inType, CServerPlugin *inPlugin )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;

	fTableMutex.Wait();

	try
	{
		pCurrRef = GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );
		if (inType != pCurrRef->fType) throw( (sInt32)eDSInvalidReference );

		pCurrRef->fPlugin	= inPlugin;
	}

	catch( sInt32 err )
	{
	}

	fTableMutex.Signal();

	return( dsResult );

} // SetPluginPtr

//------------------------------------------------------------------------------------
//	* AddChildPIDToRef (static)
//------------------------------------------------------------------------------------

tDirStatus CRefTable:: AddChildPIDToRef ( uInt32 inRefNum, uInt32 inParentPID, sInt32 inChildPID, uInt32 inIPAddress )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;
	sPIDInfo	   *pChildPIDInfo	= nil;

	gRefTable->Lock();

	try
	{
		dsResult = gRefTable->VerifyReference( inRefNum, eNodeRefType, nil, inParentPID, inIPAddress );
		if ( dsResult != eDSNoErr ) throw( (sInt32)dsResult );

		pCurrRef = gRefTable->GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		pChildPIDInfo = (sPIDInfo *)::calloc( 1, sizeof( sPIDInfo ) );
		if ( pChildPIDInfo == nil ) throw( (sInt32)eDSRefTableAllocError );

		// Save the info required for verification of ref
		pChildPIDInfo->fPID			= inChildPID;
		pChildPIDInfo->fIPAddress	= inIPAddress;

		// Set the link info
		pChildPIDInfo->fNext = pCurrRef->fChildPID;
		pCurrRef->fChildPID = pChildPIDInfo;
	}

	catch( sInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	gRefTable->Unlock();

	return( dsResult );

} // AddChildPIDToRef


// ----------------------------------------------------------------------------
//	* UpdateClientPIDRefCount()
//
// ----------------------------------------------------------------------------

uInt32 CRefTable:: UpdateClientPIDRefCount ( sInt32 inClientPID, uInt32 inIPAddress, bool inUpRefCount, uInt32 inDirRef )
{
	uInt32					aCount				= 0;

	fClientPIDListLock->Wait();
	
	try
	{
		// if we are upping the ref count and we have a dirRef...
		if ( inDirRef != 0 )
		{
			// here we just locate if we have a map for the IPAddress->PID to dirRef entry
			tIPPIDDirRefMap::iterator	aIPentry = fClientDirRefMap.find( inIPAddress );
			
			// if we have an entry, let's add the DirRef to the list
			if( aIPentry != fClientDirRefMap.end() )
			{
				tPIDDirRefMap::iterator aPIDentry = aIPentry->second.find( inClientPID );
				if( aPIDentry != aIPentry->second.end() )
				{
					if( inUpRefCount )
					{
						aPIDentry->second.insert( inDirRef ); // add the dirRef to the existing set
					}
					else
					{
						aPIDentry->second.erase( inDirRef ); // erase the dirRef from the existing set
					}
				}
				else // no PID entry
				{
					tDirRefSet	newSet;
					
					newSet.insert( inDirRef );				// add the dirRef to the new set
					aIPentry->second[inClientPID] = newSet;	// map the newSet to the PID
				}
			}
			else // no existing map add it
			{
				tDirRefSet		newSet;
				tPIDDirRefMap	aPIDentry;
				
				newSet.insert( inDirRef );					// add the dirRef to the new set
				aPIDentry[inClientPID] = newSet;			// map the newSet to the PID
				fClientDirRefMap[inIPAddress] = aPIDentry;	// map the ipAddress to the PID entry
			}
		}
		
		// now let's look at the RefCount table
		tIPPIDRefCountMap::iterator aIPrefentry = fClientRefCountMap.find( inIPAddress );
		
		// if we have an entry
		if( aIPrefentry != fClientRefCountMap.end() )
		{
			tPIDRefCountMap::iterator aPIDentry = aIPrefentry->second.find( inClientPID );
			if( aPIDentry != aIPrefentry->second.end() )
			{
				if (inUpRefCount)
				{
					aPIDentry->second += 1;
					aCount = aPIDentry->second;
					if ( (aCount > gRefCountWarningLimit) && !(aCount % 25) )
					{
						syslog(LOG_ALERT,"Potential VM growth in DirectoryService since client PID: %d, has %d open references when the warning limit is %d.", inClientPID, aCount, gRefCountWarningLimit);
						DBGLOG3( kLogHandler, "Potential VM growth in DirectoryService since client PID: %d, has %d open references when the warning limit is %d.", inClientPID, aCount, gRefCountWarningLimit);
					}
					else if (gLogAPICalls)
					{
						syslog(LOG_ALERT,"Client PID: %d, has %d open references.", inClientPID, aCount);
					}
				}
				else
				{
					aPIDentry->second -= 1;
					aCount = aPIDentry->second;
					if (gLogAPICalls)
					{
						syslog(LOG_ALERT,"Client PID: %d, has %d open references.", inClientPID, aCount);
					}
				}
				
				if( aCount == 0 )
				{
					// clean up both tables if we go to 0
					tIPPIDDirRefMap::iterator	aIPentry = fClientDirRefMap.find( inIPAddress );
					if( aIPentry != fClientDirRefMap.end() )
					{
						aIPentry->second.erase( inClientPID );
					}
					aIPrefentry->second.erase( aPIDentry );
				}
			}
			else // entry doesn't exist
			{
				aIPrefentry->second[ inClientPID ] = 1;
				aCount = 1;
			}
		}
		else if( inUpRefCount ) // no existing map add it
		{
			tPIDRefCountMap	newMap;
			
			newMap[inClientPID] = 1;
			fClientRefCountMap[ inIPAddress ] = newMap;
			aCount = 1;
		}
	}
	catch( ... )
	{
		
	}
		
   	fClientPIDListLock->Signal();

	return aCount;
	
} // UpdateClientPIDRefCount

// ----------------------------------------------------------------------------
//	* CleanClientRefs() pass through static
//
// ----------------------------------------------------------------------------

void CRefTable::CleanClientRefs ( uInt32 inIPAddress, uInt32 inPIDorPort )
{
	if (gRefTable != nil)
	{
		gRefTable->DoCleanClientRefs(inIPAddress, inPIDorPort);
	}
} // CheckClientPIDs


// ----------------------------------------------------------------------------
//	* DoCleanClientRefs() //if inIPAddress is zero then a local cleanup check
//
// ----------------------------------------------------------------------------

void CRefTable::DoCleanClientRefs ( uInt32 inIPAddress, uInt32 inPIDorPort )
{
	tDirRefSet	cleanupSet;

	//wait one second since refs might be updated for other client PIDs
	if (fClientPIDListLock->Wait(1) != eDSNoErr)
	{
		fClientPIDListLock->Signal(); //ensure mutex is released
		return;
	}

	try
	{
		// here we just locate if we have a map for the IPAddress->PID to dirRef entry
		tIPPIDDirRefMap::iterator	aIPentry = fClientDirRefMap.find( inIPAddress );
		
		// if we have an etnry, let's add the DirRef to the list
		if( aIPentry != fClientDirRefMap.end() )
		{
			tPIDDirRefMap::iterator aPIDentry = aIPentry->second.find( inPIDorPort );
			if( aPIDentry != aIPentry->second.end() )
			{
				// we'll copy the set so we can clean up..
				cleanupSet = aPIDentry->second;
			}
		}
		
		// now let's look at the RefCount table
		tIPPIDRefCountMap::iterator aIPrefentry = fClientRefCountMap.find( inIPAddress );
		
		// if we have an entry
		if( aIPrefentry != fClientRefCountMap.end() )
		{
			tPIDRefCountMap::iterator aPIDentry = aIPrefentry->second.find( inPIDorPort );
			if( aPIDentry != aIPrefentry->second.end() )
			{
				if (gLogAPICalls)
				{
					syslog(LOG_ALERT,"Client PID: %d, had %d open references before cleanup.",inPIDorPort, aPIDentry->second);
				}
				DBGLOG2( kLogHandler, "Client PID: %d, had %d open references before cleanup.", inPIDorPort, aPIDentry->second );
			}
		}

		// let's log how many refs, etc. we have
		if (gLogAPICalls)
		{
			aIPentry = fClientDirRefMap.begin();
			while( aIPentry != fClientDirRefMap.end() )
			{
				tPIDDirRefMap::iterator aPIDentry = aIPentry->second.begin();
				while( aPIDentry != aIPentry->second.end() )
				{
					if( aPIDentry->first != inPIDorPort )
					{
						syslog( LOG_ALERT, "Client PID: %d, has %d open references in table.", aPIDentry->first, aPIDentry->second.size() );
					}
					++aPIDentry;
				}
				++aIPentry;
			}
		}
	}
	catch( ... )
	{
	}

	// release mutex so we can go through our list and clean up
	fClientPIDListLock->Signal();

	try
	{
		// we found an entry for the PID in question, let's clean up if necessary..
		tDirRefSet::iterator aIterator = cleanupSet.begin();
		while( aIterator != cleanupSet.end() )
		{
			CRefTable::RemoveDirRef( *aIterator, inPIDorPort, inIPAddress );
			++aIterator;
		}
	}
	catch( ... )
	{
	}
	
} // DoCheckClientPIDs

