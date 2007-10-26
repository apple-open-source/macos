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
extern UInt32					gRefCountWarningLimit;
extern UInt32					gDaemonIPAddress;
extern DSMutexSemaphore			*gTableMutex;
extern CRefTable				*gRefTable;

//------------------------------------------------------------------------------------
//	* CRefTable
//------------------------------------------------------------------------------------

CRefTable::CRefTable ( RefDeallocateProc *deallocProc ) : fTableMutex("CRefTable::fTableMutex")
{
	fRefCleanUpEntriesHead  = nil;
	fRefCleanUpEntriesTail  = nil;
	
	fClientPIDListLock = new DSMutexSemaphore("CRefTable::fClientPIDListLock");
	fTableCount		= 0;
	::memset( fRefTables, 0, sizeof( fRefTables ) );

	fDeallocProc = deallocProc;
} // CRefTable


//------------------------------------------------------------------------------------
//	* ~CRefTable
//------------------------------------------------------------------------------------

CRefTable::~CRefTable ( void )
{
	UInt32		i	= 1;
	UInt32		j	= 1;

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
	fTableMutex.WaitLock();
}


//--------------------------------------------------------------------------------------------------
//	* Unlock
//
//--------------------------------------------------------------------------------------------------

void CRefTable::Unlock (  )
{
	fTableMutex.SignalLock();
}


//--------------------------------------------------------------------------------------------------
//	* VerifyReference
//
//--------------------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyReference ( tDirReference	inDirRef,
										UInt32			inType,
										CServerPlugin **outPlugin,
										SInt32			inPID,
										UInt32			inIPAddress,
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
			DbgLog( kLogHandler, "Given reference value of <%u> found but does not match reference type.", inDirRef);
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
										SInt32				inPID,
										UInt32				inIPAddress )
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
										SInt32				inPID,
										UInt32				inIPAddress)
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->VerifyReference( inDirNodeRef, eNodeRefType, outPlugin, inPID, inIPAddress );
	}

	return( siResult );

} // VerifyNodeRef


//------------------------------------------------------------------------------------
//	* GetNodeRefName (static)
//
//------------------------------------------------------------------------------------

char* CRefTable::GetNodeRefName ( tDirNodeReference	inDirNodeRef )
{
	char* outNodeName = nil;

	if ( gRefTable != nil )
	{
		outNodeName = gRefTable->GetNodeName( inDirNodeRef );
	}

	return( outNodeName );

} // GetNodeRefName


//------------------------------------------------------------------------------------
//	* VerifyRecordRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyRecordRef (	tRecordReference	inRecordRef,
										CServerPlugin	  **outPlugin,
										SInt32				inPID,
										UInt32				inIPAddress,
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
											SInt32				inPID,
											UInt32				inIPAddress )
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
											SInt32					inPID,
											UInt32					inIPAddress )
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

tDirStatus CRefTable::NewDirRef (	UInt32	   *outNewRef,
									SInt32 		inPID,
									UInt32		inIPAddress )
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

tDirStatus CRefTable::NewNodeRef (	UInt32			*outNewRef,
									CServerPlugin	*inPlugin,
									UInt32			inParentID,
									SInt32			inPID,
									UInt32			inIPAddress,
									const char	   *inNodeName)
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->GetNewRef( outNewRef, inParentID, eNodeRefType, inPlugin, inPID, inIPAddress, inNodeName );
	}

	return( siResult );

} // NewNodeRef


//------------------------------------------------------------------------------------
//	* NewRecordRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::NewRecordRef (	UInt32		   *outNewRef,
										CServerPlugin  *inPlugin,
										UInt32			inParentID,
										SInt32			inPID,
										UInt32			inIPAddress )
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

tDirStatus CRefTable::NewAttrListRef (	UInt32			*outNewRef,
										CServerPlugin	*inPlugin,
										UInt32			inParentID,
										SInt32			inPID,
										UInt32			inIPAddress )
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

tDirStatus CRefTable::NewAttrValueRef ( UInt32			*outNewRef,
									 	CServerPlugin	*inPlugin,
									 	UInt32			inParentID,
										SInt32			inPID,
										UInt32			inIPAddress )
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

tDirStatus CRefTable::RemoveDirRef (	UInt32	inDirRef,
										SInt32	inPID,
										UInt32	inIPAddress )
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

tDirStatus CRefTable::RemoveNodeRef (	UInt32	inNodeRef,
										SInt32	inPID,
										UInt32	inIPAddress )
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

tDirStatus CRefTable::RemoveRecordRef (	UInt32	inRecRef,
										SInt32	inPID,
										UInt32	inIPAddress )
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

tDirStatus CRefTable::RemoveAttrListRef (	UInt32	inAttrListRef,
											SInt32	inPID,
											UInt32	inIPAddress )
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

tDirStatus CRefTable::RemoveAttrValueRef (	UInt32	inAttrValueRef,
											SInt32	inPID,
											UInt32	inIPAddress )
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

sRefEntry* CRefTable::GetTableRef ( UInt32 inRefNum )
{
	UInt32			uiSlot			= 0;
	UInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	UInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	sRefTable	   *pTable			= nil;
	sRefEntry	   *pOutEntry		= nil;

	fTableMutex.WaitLock();

	try
	{
		pTable = GetThisTable( uiTableNum );
		if ( pTable == nil ) throw( (SInt32)eDSInvalidReference );

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

	catch( SInt32 err )
	{
		DbgLog( kLogAssert, "Reference %l error = %l", inRefNum, err );
	}

	fTableMutex.SignalLock();

	return( pOutEntry );

} // GetTableRef


//------------------------------------------------------------------------------------
//	* GetNextTable
//------------------------------------------------------------------------------------

sRefTable* CRefTable::GetNextTable ( sRefTable *inTable )
{
	UInt32		uiTblNum	= 0;
	sRefTable	*pOutTable	= nil;

	fTableMutex.WaitLock();

	try
	{
		if ( inTable == nil )
		{
			// Get the first reference table, create it if it wasn't already
			if ( fRefTables[ 1 ] == nil )
			{
				// No tables have been allocated yet so lets make one
				fRefTables[ 1 ] = (sRefTable *)::calloc( sizeof( sRefTable ), sizeof( char ) );
				if ( fRefTables[ 1 ] == nil ) throw((SInt32)eMemoryAllocError);

				fRefTables[ 1 ]->fTableNum = 1;

				fTableCount = 1;
			}

			pOutTable = fRefTables[ 1 ];
		}
		else
		{
			uiTblNum = inTable->fTableNum + 1;
			if (uiTblNum > kMaxTables) throw( (SInt32)eDSInvalidReference );

			if ( fRefTables[ uiTblNum ] == nil )
			{
				// Set the table counter
				fTableCount = uiTblNum;

				// No tables have been allocated yet so lets make one
				fRefTables[ uiTblNum ] = (sRefTable *)::calloc( sizeof( sRefTable ), sizeof( char ) );
				if ( fRefTables[ uiTblNum ] == nil ) throw((SInt32)eMemoryAllocError);

				if (uiTblNum == 0) throw( (SInt32)eDSInvalidReference );
				fRefTables[ uiTblNum ]->fTableNum = uiTblNum;
			}

			pOutTable = fRefTables[ uiTblNum ];
		}
	}

	catch( SInt32 err )
	{
		DbgLog( kLogAssert, "Reference table error = %l", err );
	}

	fTableMutex.SignalLock();

	return( pOutTable );

} // GetNextTable


//------------------------------------------------------------------------------------
//	* GetThisTable
//------------------------------------------------------------------------------------

sRefTable* CRefTable::GetThisTable ( UInt32 inTableNum )
{
	sRefTable	*pOutTable	= nil;

	fTableMutex.WaitLock();

	pOutTable = fRefTables[ inTableNum ];

	fTableMutex.SignalLock();

	return( pOutTable );

} // GetThisTable


//------------------------------------------------------------------------------------
//	* GetNewRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::GetNewRef (	UInt32		   *outRef,
									UInt32			inParentID,
									eRefTypes		inType,
									CServerPlugin  *inPlugin,
									SInt32			inPID,
									UInt32			inIPAddress,
									const char	   *inNodeName)
{
	bool			done		= false;
	tDirStatus		outResult	= eDSNoErr;
	sRefTable	   *pCurTable	= nil;
	UInt32			uiRefNum	= 0;
	UInt32			uiCntr		= 0;
	UInt32			uiSlot		= 0;
	UInt32			uiTableNum	= 0;
	UInt32			refCountUpdate	= 0;

	fTableMutex.WaitLock();

	try
	{
		*outRef = 0;

		while ( !done )
		{
			pCurTable = GetNextTable( pCurTable );
			if ( pCurTable == nil ) throw( (SInt32)eDSRefTableAllocError );

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
						if ( pCurTable->fTableData[ uiSlot ] == nil ) throw( (SInt32)eDSRefTableAllocError );
						
						// We found an empty slot, now set this table entry
						pCurTable->fTableData[ uiSlot ]->fRefNum		= uiRefNum;
						pCurTable->fTableData[ uiSlot ]->fType			= inType;
						pCurTable->fTableData[ uiSlot ]->fParentID		= inParentID;
						pCurTable->fTableData[ uiSlot ]->fPID			= inPID;
						pCurTable->fTableData[ uiSlot ]->fIPAddress		= inIPAddress;
						pCurTable->fTableData[ uiSlot ]->fPlugin		= inPlugin;
						pCurTable->fTableData[ uiSlot ]->fChildren		= nil;
						pCurTable->fTableData[ uiSlot ]->fChildPID		= nil;
						if (inNodeName != nil)
							pCurTable->fTableData[ uiSlot ]->fNodeName	= strdup(inNodeName);
						else
							pCurTable->fTableData[ uiSlot ]->fNodeName	= nil;

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

	catch( SInt32 err )
	{
		outResult = (tDirStatus)err;
	}

	fTableMutex.SignalLock();

	return( outResult );

} // GetNewRef


//------------------------------------------------------------------------------------
//	* LinkToParent
//------------------------------------------------------------------------------------

tDirStatus CRefTable::LinkToParent (	UInt32	inRefNum,
										UInt32	inType,
										UInt32	inParentID,
										SInt32	inPID,
										UInt32	inIPAddress )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;
	sListInfo	   *pChildInfo		= nil;

	fTableMutex.WaitLock();

	try
	{
		pCurrRef = GetTableRef( inParentID );
		if ( pCurrRef == nil ) throw( (SInt32)eDSInvalidReference );

		// This is the one we want
		pChildInfo = (sListInfo *)::calloc( sizeof( sListInfo ), sizeof( char ) );
		if ( pChildInfo == nil ) throw( (SInt32)eDSRefTableAllocError );

		// Save the info required later for removal if the parent gets removed
		pChildInfo->fRefNum		= inRefNum;
		pChildInfo->fType		= inType;
		pChildInfo->fPID		= inPID;
		pChildInfo->fIPAddress	= inIPAddress;

		// Set the link info
		pChildInfo->fNext = pCurrRef->fChildren;
		pCurrRef->fChildren = pChildInfo;
	}

	catch( SInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	fTableMutex.SignalLock();

	return( dsResult );

} // LinkToParent


//------------------------------------------------------------------------------------
//	* UnlinkFromParent
//------------------------------------------------------------------------------------

tDirStatus CRefTable::UnlinkFromParent ( UInt32 inRefNum )
{
	tDirStatus		dsResult		= eDSNoErr;
	UInt32			i				= 1;
	UInt32			parentID		= 0;
	sRefEntry	   *pCurrRef		= nil;
	sRefEntry	   *pParentRef		= nil;
	sListInfo	   *pCurrChild		= nil;
	sListInfo	   *pPrevChild		= nil;

	fTableMutex.WaitLock();

	try
	{
		//Node references are currently not linked to their parent dir reference
		//So, there is no issue when any child client PID has a reference linked to a parent since
		//it is unique to that child client PID and can be unlinked as before
		
		// Get the current reference
		pCurrRef = GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (SInt32)eDSInvalidReference );

		parentID = pCurrRef->fParentID;

		if ( parentID != 0 )
		{
			// Get the parent reference
			pParentRef = GetTableRef( parentID );
			if ( pParentRef == nil ) throw( (SInt32)eDSInvalidReference );

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

	catch( SInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	fTableMutex.SignalLock();

	return( dsResult );

} // UnlinkFromParent


//------------------------------------------------------------------------------------
//	* GetReference
//------------------------------------------------------------------------------------

tDirStatus CRefTable::GetReference ( UInt32 inRefNum, sRefEntry **outRefData )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;

	fTableMutex.WaitLock();

	try
	{
		pCurrRef = GetTableRef( inRefNum );
		if ( pCurrRef == nil )
		{
			DbgLog( kLogHandler, "Given reference value of <%u> has no valid reference table entry.", inRefNum);
			throw( (SInt32)eDSInvalidReference );
		}

		*outRefData = pCurrRef;
	}

	catch( SInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	fTableMutex.SignalLock();

	return( dsResult );

} // GetReference


//------------------------------------------------------------------------------------
//	* RemoveRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveRef (	UInt32	inRefNum,
									UInt32	inType,
									SInt32	inPID,
									UInt32	inIPAddress,
									bool	inbAtTop)
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;
	sRefTable	   *pTable			= nil;
	UInt32			uiSlot			= 0;
	UInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	UInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	bool			doFree			= false;
	sPIDInfo	   *pPIDInfo		= nil;
	sPIDInfo	   *pPrevPIDInfo	= nil;
	UInt32			refCountUpdate	= 0;
	sRefCleanUpEntry	   *aRefCleanUpEntries		= nil;
	sRefCleanUpEntry	   *aRefCleanUpEntriesIter  = nil;
	SInt32			deallocResult   = eDSNoErr;


	fTableMutex.WaitLock();

	try
	{
		dsResult = VerifyReference( inRefNum, inType, nil, inPID, inIPAddress );

		if ( dsResult == eDSNoErr )
		{
			pTable = GetThisTable( uiTableNum );
			if ( pTable == nil ) throw( (SInt32)eDSInvalidReference );

			uiSlot = uiRefNum % kMaxTableItems;

			if ( inType != eDirectoryRefType ) // API refs have no parents
			{
				dsResult = UnlinkFromParent( inRefNum );
				if ( dsResult != eDSNoErr ) throw( (SInt32)dsResult );
			}

			pCurrRef = GetTableRef( inRefNum );	//KW80 - here we need to know where the sRefEntry is in the table to delete it
												//that is all the code added above
												// getting this sRefEntry is the same path used by uiSlot and pTable above
			if ( pCurrRef == nil ) throw( (SInt32)eDSInvalidReference );
			if (inType != pCurrRef->fType) throw( (SInt32)eDSInvalidReference );

			DSFreeString(pCurrRef->fNodeName); //only present if this was a node ref

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

	catch( SInt32 err )
	{
		dsResult = (tDirStatus)err;
	}

	if (inbAtTop) //this tells us that we are at the top of the recursive stack in removing children and need to cleanup refs
	{
		aRefCleanUpEntriesIter	= fRefCleanUpEntriesHead;
		fRefCleanUpEntriesHead  = nil;
		fRefCleanUpEntriesTail  = nil;
	}

	fTableMutex.SignalLock();
	
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
									SInt32			inPID,
									UInt32			inIPAddress )
{
	sListInfo	   *pCurrChild		= nil;
	sListInfo	   *pNextChild		= nil;

	fTableMutex.WaitLock();

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

	catch( SInt32 err )
	{
	}

	fTableMutex.SignalLock();

} // RemoveChildren



//------------------------------------------------------------------------------------
//	* SetPluginPtr
//------------------------------------------------------------------------------------

tDirStatus CRefTable::SetPluginPtr ( UInt32 inRefNum, UInt32 inType, CServerPlugin *inPlugin )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;

	fTableMutex.WaitLock();

	try
	{
		pCurrRef = GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (SInt32)eDSInvalidReference );
		if (inType != pCurrRef->fType) throw( (SInt32)eDSInvalidReference );

		pCurrRef->fPlugin	= inPlugin;
	}

	catch( SInt32 err )
	{
	}

	fTableMutex.SignalLock();

	return( dsResult );

} // SetPluginPtr

//------------------------------------------------------------------------------------
//	* AddChildPIDToRef (static)
//------------------------------------------------------------------------------------

tDirStatus CRefTable:: AddChildPIDToRef ( UInt32 inRefNum, UInt32 inParentPID, SInt32 inChildPID, UInt32 inIPAddress )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;
	sPIDInfo	   *pChildPIDInfo	= nil;

	gRefTable->Lock();

	try
	{
		dsResult = gRefTable->VerifyReference( inRefNum, eNodeRefType, nil, inParentPID, inIPAddress );
		if ( dsResult != eDSNoErr ) throw( (SInt32)dsResult );

		pCurrRef = gRefTable->GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (SInt32)eDSInvalidReference );

		pChildPIDInfo = (sPIDInfo *)::calloc( 1, sizeof( sPIDInfo ) );
		if ( pChildPIDInfo == nil ) throw( (SInt32)eDSRefTableAllocError );

		// Save the info required for verification of ref
		pChildPIDInfo->fPID			= inChildPID;
		pChildPIDInfo->fIPAddress	= inIPAddress;

		// Set the link info
		pChildPIDInfo->fNext = pCurrRef->fChildPID;
		pCurrRef->fChildPID = pChildPIDInfo;
	}

	catch( SInt32 err )
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

UInt32 CRefTable:: UpdateClientPIDRefCount ( SInt32 inClientPID, UInt32 inIPAddress, bool inUpRefCount, UInt32 inDirRef )
{
	UInt32					aCount				= 0;

	fClientPIDListLock->WaitLock();
	
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
						DbgLog( kLogHandler, "Potential VM growth in DirectoryService since client PID: %d, has %d open references when the warning limit is %d.", inClientPID, aCount, gRefCountWarningLimit);
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
		
   	fClientPIDListLock->SignalLock();

	return aCount;
	
} // UpdateClientPIDRefCount

// ----------------------------------------------------------------------------
//	* CleanClientRefs() pass through static
//
// ----------------------------------------------------------------------------

void CRefTable::CleanClientRefs ( UInt32 inIPAddress, UInt32 inPIDorPort )
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

void CRefTable::DoCleanClientRefs ( UInt32 inIPAddress, UInt32 inPIDorPort )
{
	tDirRefSet	cleanupSet;

	fClientPIDListLock->WaitLock();

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
				DbgLog( kLogHandler, "Client PID: %d, had %d open references before cleanup.", inPIDorPort, aPIDentry->second );
			}
		}

		// let's log how many refs, etc. we have
		if (gLogAPICalls)
		{
			aIPrefentry = fClientRefCountMap.begin();
			while( aIPrefentry != fClientRefCountMap.end() )
			{
				tPIDRefCountMap::iterator aPIDentry = aIPrefentry->second.begin();
				while( aPIDentry != aIPrefentry->second.end() )
				{
					if( aPIDentry->first != inPIDorPort )
					{
						syslog( LOG_ALERT, "Client PID: %d, has %d open references in table.", aPIDentry->first, aPIDentry->second );
					}
					++aPIDentry;
				}
				++aIPrefentry;
			}
		}
	}
	catch( ... )
	{
	}

	// release mutex so we can go through our list and clean up
	fClientPIDListLock->SignalLock();

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


// ----------------------------------------------------------------------------
//	* LockClientPIDList()
//
// ----------------------------------------------------------------------------

void CRefTable::LockClientPIDList( void )
{
	fClientPIDListLock->WaitLock();	
}


// ----------------------------------------------------------------------------
//	* UnlockClientPIDList()
//
// ----------------------------------------------------------------------------

void CRefTable::UnlockClientPIDList( void )
{
	fClientPIDListLock->SignalLock();	
}


// ----------------------------------------------------------------------------
//	* CreateNextClientPIDListString()
//
// ----------------------------------------------------------------------------

char* CRefTable::CreateNextClientPIDListString( bool inStart, tIPPIDDirRefMap::iterator &inOutIPEntry, tPIDDirRefMap::iterator &inOutPIDEntry )
{
	SInt32		theIPAddress		= 0;
	UInt32		thePIDValue			= 0;
	UInt32		numDirRefs			= 0;
	UInt32		theTotalRefCount	= 0;
	char	   *outString			= nil;
	
	if (!inStart)
	{
		++inOutPIDEntry;
	}

	if (inStart)
	{
		inOutIPEntry = fClientDirRefMap.begin();
	}
	else if (inOutPIDEntry == inOutIPEntry->second.end())
	{
		++inOutIPEntry;
		if (inOutIPEntry != fClientDirRefMap.end())
		{
			inOutPIDEntry = inOutIPEntry->second.begin();
		}
		else
		{
			return(nil);
		}
	}
	
	if (inStart)
	{
		inOutPIDEntry = inOutIPEntry->second.begin();
	}
	
	theIPAddress	= inOutIPEntry->first;
	thePIDValue		= inOutPIDEntry->first;
	numDirRefs		= inOutPIDEntry->second.size();
	
	//calculate a max string length here using the following constraints
	//IP Address xxx.xxx.xxx.xxx to string of length 15
	//PID Value UInt32 to string of length 6
	//Total Ref Count value to string of length 6
	//Dir Ref values to strings of length 8
	//format of output string is:
	// IP:IPAddress,PID:PIDValue,TRC:TotalRefCount,DRs:,DirRef1,DirRef2,DirRef3,...,DirRefn
	outString = (char *)calloc(3+15+5+6+5+6+22+numDirRefs*(1+8)+1,sizeof(char*));
	DSNetworkUtilities::Initialize();
	if (theIPAddress == 0)
	{
		const char* ipAddressString = DSNetworkUtilities::GetOurIPAddressString(0); //only get first one
		if (ipAddressString != nil)
		{
			sprintf(outString,"IP:%15s",ipAddressString);
		}
		else
		{
			sprintf(outString,"IP:%15s","localhost");
		}
	}
	else
	{
		IPAddrStr	ipAddrString;
		DSNetworkUtilities::IPAddrToString( theIPAddress, ipAddrString, MAXIPADDRSTRLEN );
		if (ipAddrString[0] != '\0')
		{
			sprintf(outString,"IP:%15s",ipAddrString);
		}
		else
		{
			sprintf(outString,"IP:%15s","unknown");
		}
	}
	if (thePIDValue == 0)
	{
		sprintf(outString+18,",PID:%6u",(unsigned int)::getpid());
	}
	else
	{
		sprintf(outString+18,",PID:%6u",(unsigned int)thePIDValue);
	}

	// first grab the total RefCount
	tIPPIDRefCountMap::iterator aRefCountEntry = fClientRefCountMap.find( inOutIPEntry->first );
	
	// if we have an entry
	if( aRefCountEntry != fClientRefCountMap.end() )
	{
		tPIDRefCountMap::iterator aPIDEntry = aRefCountEntry->second.find( inOutPIDEntry->first );
		if( aPIDEntry != aRefCountEntry->second.end() )
		{
			theTotalRefCount = aPIDEntry->second;
		}
	}
	sprintf(outString+29,",TotalRefCnt:%6u,DirRefs:",(unsigned int)theTotalRefCount);

	tDirRefSet::iterator aDirRefEntry = inOutPIDEntry->second.begin();
	UInt32 cntDRs = 0;
	while( aDirRefEntry != inOutPIDEntry->second.end() )
	{
		char* ptr = outString+(57+cntDRs*(1+8));
		sprintf(ptr,",%8u",(unsigned int)(*aDirRefEntry));
		cntDRs++;
		
		++aDirRefEntry;
	}
	
	return(outString);
}


// ----------------------------------------------------------------------------
//	* CreateNextClientPIDListRecordName()
//
// ----------------------------------------------------------------------------

char*	CRefTable::CreateNextClientPIDListRecordName(	bool inStart,
											tIPPIDDirRefMap::iterator &inOutIPEntry, tPIDDirRefMap::iterator &inOutPIDEntry,
											char** outIPAddress, char** outPIDValue,
											SInt32 &outIP, UInt32 &outPID,
											UInt32 &outTotalRefCount,
											UInt32 &outDirRefCount, char** &outDirRefs )
{
	SInt32		theIPAddress		= 0;
	UInt32		thePID				= 0;
	UInt32		numDirRefs			= 0;
	char	   *outString			= nil;
	
	if (!inStart)
	{
		++inOutPIDEntry;
	}

	if (inStart)
	{
		inOutIPEntry = fClientDirRefMap.begin();
	}
	else if (inOutPIDEntry == inOutIPEntry->second.end())
	{
		++inOutIPEntry;
		if (inOutIPEntry != fClientDirRefMap.end())
		{
			inOutPIDEntry = inOutIPEntry->second.begin();
		}
		else
		{
			return(nil);
		}
	}
	
	if (inStart)
	{
		inOutPIDEntry = inOutIPEntry->second.begin();
	}
	
	theIPAddress	= inOutIPEntry->first;
	outIP			= theIPAddress;
	thePID			= inOutPIDEntry->first;
	outPID			= thePID;
	numDirRefs		= inOutPIDEntry->second.size();
	outDirRefCount	= numDirRefs;
	
	//calculate a string length here using the following constraints
	//IP Address xxx.xxx.xxx.xxx to string of length 15
	//PID Value UInt32 to string of length 6
	//format of output recordname string is:
	// IPAddress and PIDValue
	outString = (char *)calloc(15+6+1,sizeof(char*));
	DSNetworkUtilities::Initialize();
	if (theIPAddress == 0)
	{
        sprintf(outString,"%s","localhost");
	}
	else
	{
		IPAddrStr	ipAddrString;
		DSNetworkUtilities::IPAddrToString( theIPAddress, ipAddrString, MAXIPADDRSTRLEN );
		if (ipAddrString[0] != '\0')
		{
			sprintf(outString,"%15s",ipAddrString);
		}
		else
		{
			sprintf(outString,"%15s","unknown");
		}
	}
	*outIPAddress = strdup(outString);

	*outPIDValue = (char*)calloc(6+1, sizeof(char*));
	if (thePID == 0)
	{
		sprintf(outString+strlen(outString),",%u",(unsigned int)::getpid());
		sprintf(*outPIDValue, "%u", (unsigned int)::getpid());
	}
	else
	{
		sprintf(outString+strlen(outString),",%u",(unsigned int)thePID);
		sprintf(*outPIDValue, "%u", (unsigned int)thePID);
	}

	// first grab the total RefCount
	tIPPIDRefCountMap::iterator aRefCountEntry = fClientRefCountMap.find( inOutIPEntry->first );
	
	// if we have an entry
	if( aRefCountEntry != fClientRefCountMap.end() )
	{
		tPIDRefCountMap::iterator aPIDEntry = aRefCountEntry->second.find( inOutPIDEntry->first );
		if( aPIDEntry != aRefCountEntry->second.end() )
		{
			outTotalRefCount = aPIDEntry->second;
		}
	}

	tDirRefSet::iterator aDirRefEntry = inOutPIDEntry->second.begin();
	outDirRefs = (char **)calloc(numDirRefs + 1, sizeof (char*));
	UInt32 idx = 0;
	while( aDirRefEntry != inOutPIDEntry->second.end() )
	{
		char *ptr = (char*)calloc(9, 1 );
		outDirRefs[idx] = ptr;
		idx++;
		sprintf(ptr,"%8u",(unsigned int)(*aDirRefEntry));
		++aDirRefEntry;
	}
	
	return(outString);
}

// ----------------------------------------------------------------------------
//	* RetrieveRefDataPerClientPIDAndIP()
//
// ----------------------------------------------------------------------------

void CRefTable::RetrieveRefDataPerClientPIDAndIP(	SInt32 inIP, UInt32 inPID, char** inDirRefs,
													UInt32 &outNodeRefCount, char** &outNodeRefs,
													UInt32 &outRecRefCount, char** &outRecRefs,
													UInt32 &outAttrListRefCount, char** &outAttrListRefs,
													UInt32 &outAttrListValueRefCount, char** &outAttrListValueRefs  )
{

	UInt32 nodeRefListSize = 8;
	UInt32 recRefListSize = 8;
	UInt32 attrListRefListSize = 8;
	UInt32 attrListValueRefListSize = 8;
	UInt32 aRefNum = 0;
	UInt32 uiTableNum = 0;
	sRefTable* pTable = nil;
	sRefEntry* pCurrRef = nil;
	sListInfo* aChildPtr = nil;
	char* aRefString = nil;
	
	outNodeRefCount = 0;
	outRecRefCount = 0;
	outAttrListRefCount = 0;
	outAttrListValueRefCount = 0;
	
//we alloc a std size for ref lists and check if we need more then double it
	outNodeRefs = (char **)calloc(nodeRefListSize + 1, sizeof(char *));
	outRecRefs = (char **)calloc(recRefListSize + 1, sizeof(char *));
	outAttrListRefs = (char **)calloc(attrListRefListSize + 1, sizeof(char *));
	outAttrListValueRefs = (char **)calloc(attrListValueRefListSize + 1, sizeof(char *));
	
	fTableMutex.WaitLock();

	if (inDirRefs != nil)
	{
		UInt32 dirRefIndex = 0;
		char *aDirRefString = inDirRefs[dirRefIndex];
		while(aDirRefString != nil)
		{
			aRefNum = atoi(aDirRefString);
			SInt32 dsResult = VerifyReference( aRefNum, eDirectoryRefType, nil, inPID, inIP );

			if ( dsResult == eDSNoErr )
			{
				uiTableNum = (aRefNum & 0xFF000000) >> 24;
				pTable = GetThisTable( uiTableNum );
				if ( pTable != nil )
				{
					pCurrRef = GetTableRef( aRefNum );
					if ( ( pCurrRef != nil ) && (eDirectoryRefType == pCurrRef->fType) )
					{
						aChildPtr = pCurrRef->fChildren;
						while (aChildPtr != nil)
						{
							aRefString = (char *)calloc(9, 1);
							sprintf(aRefString,"%8u",(unsigned int)(aChildPtr->fRefNum));
							if ( aChildPtr->fType == eNodeRefType )
							{
								outNodeRefs[outNodeRefCount] = aRefString;
								outNodeRefCount++;
								if (outNodeRefCount >= nodeRefListSize)
								{
									GrowList(outNodeRefs, nodeRefListSize);
								}
							}
							else if ( aChildPtr->fType == eRecordRefType )
							{
								outRecRefs[outRecRefCount] = aRefString;
								outRecRefCount++;
								if (outRecRefCount >= recRefListSize)
								{
									GrowList(outRecRefs, recRefListSize);
								}
							}
							else if ( aChildPtr->fType == eAttrListRefType )
							{
								outAttrListRefs[outAttrListRefCount] = aRefString;
								outAttrListRefCount++;
								if (outAttrListRefCount >= attrListRefListSize)
								{
									GrowList(outAttrListRefs, attrListRefListSize);
								}
							}
							else if ( aChildPtr->fType == eAttrValueListRefType )
							{
								outAttrListValueRefs[outAttrListValueRefCount] = aRefString;
								outAttrListValueRefCount++;
								if (outAttrListValueRefCount >= attrListValueRefListSize)
								{
									GrowList(outAttrListValueRefs, attrListValueRefListSize);
								}
							}
							aChildPtr = aChildPtr->fNext;
						}
					} //has children
				} //valid table
			} //valid ref
			dirRefIndex++;
			aDirRefString = inDirRefs[dirRefIndex];
		}
	}

	if (outNodeRefs != nil)
	{
		UInt32 nodeRefIndex = 0;
		char *aNodeRefString = outNodeRefs[nodeRefIndex];
		while(aNodeRefString != nil)
		{
			aRefNum = atoi(aNodeRefString);
			SInt32 dsResult = VerifyReference( aRefNum, eNodeRefType, nil, inPID, inIP );

			if ( dsResult == eDSNoErr )
			{
				uiTableNum = (aRefNum & 0xFF000000) >> 24;
				pTable = GetThisTable( uiTableNum );
				if ( pTable != nil )
				{
					pCurrRef = GetTableRef( aRefNum );
					if ( ( pCurrRef != nil ) && (eNodeRefType == pCurrRef->fType) )
					{
						aChildPtr = pCurrRef->fChildren;
						while (aChildPtr != nil)
						{
							aRefString = (char *)calloc(9, 1);
							sprintf(aRefString,"%8u",(unsigned int)(aChildPtr->fRefNum));
							if ( aChildPtr->fType == eRecordRefType )
							{
								outRecRefs[outRecRefCount] = aRefString;
								outRecRefCount++;
								if (outRecRefCount >= recRefListSize)
								{
									GrowList(outRecRefs, recRefListSize);
								}
							}
							else if ( aChildPtr->fType == eAttrListRefType )
							{
								outAttrListRefs[outAttrListRefCount] = aRefString;
								outAttrListRefCount++;
								if (outAttrListRefCount >= attrListRefListSize)
								{
									GrowList(outAttrListRefs, attrListRefListSize);
								}
							}
							else if ( aChildPtr->fType == eAttrValueListRefType )
							{
								outAttrListValueRefs[outAttrListValueRefCount] = aRefString;
								outAttrListValueRefCount++;
								if (outAttrListValueRefCount >= attrListValueRefListSize)
								{
									GrowList(outAttrListValueRefs, attrListValueRefListSize);
								}
							}
							aChildPtr = aChildPtr->fNext;
						}
					} //has children
				} //valid table
			} //valid ref
			nodeRefIndex++;
			aNodeRefString = outNodeRefs[nodeRefIndex];
		}
	}

	if (outRecRefs != nil)
	{
		UInt32 recRefIndex = 0;
		char *aRecRefString = outRecRefs[recRefIndex];
		while(aRecRefString != nil)
		{
			aRefNum = atoi(aRecRefString);
			SInt32 dsResult = VerifyReference( aRefNum, eRecordRefType, nil, inPID, inIP );

			if ( dsResult == eDSNoErr )
			{
				uiTableNum = (aRefNum & 0xFF000000) >> 24;
				pTable = GetThisTable( uiTableNum );
				if ( pTable != nil )
				{
					pCurrRef = GetTableRef( aRefNum );
					if ( ( pCurrRef != nil ) && (eRecordRefType == pCurrRef->fType) )
					{
						aChildPtr = pCurrRef->fChildren;
						while (aChildPtr != nil)
						{
							aRefString = (char *)calloc(9, 1);
							sprintf(aRefString,"%8u",(unsigned int)(aChildPtr->fRefNum));
							if ( aChildPtr->fType == eAttrListRefType )
							{
								outAttrListRefs[outAttrListRefCount] = aRefString;
								outAttrListRefCount++;
								if (outAttrListRefCount >= attrListRefListSize)
								{
									GrowList(outAttrListRefs, attrListRefListSize);
								}
							}
							else if ( aChildPtr->fType == eAttrValueListRefType )
							{
								outAttrListValueRefs[outAttrListValueRefCount] = aRefString;
								outAttrListValueRefCount++;
								if (outAttrListValueRefCount >= attrListValueRefListSize)
								{
									GrowList(outAttrListValueRefs, attrListValueRefListSize);
								}
							}
							aChildPtr = aChildPtr->fNext;
						}
					} //has children
				} //valid table
			} //valid ref
			recRefIndex++;
			aRecRefString = outRecRefs[recRefIndex];
		}
	}
	
	if (outAttrListRefs != nil)
	{
		UInt32 attrListRefIndex = 0;
		char *aAttrListRefString = outAttrListRefs[attrListRefIndex];
		while(aAttrListRefString != nil)
		{
			aRefNum = atoi(aAttrListRefString);
			SInt32 dsResult = VerifyReference( aRefNum, eAttrListRefType, nil, inPID, inIP );

			if ( dsResult == eDSNoErr )
			{
				uiTableNum = (aRefNum & 0xFF000000) >> 24;
				pTable = GetThisTable( uiTableNum );
				if ( pTable != nil )
				{
					pCurrRef = GetTableRef( aRefNum );
					if ( ( pCurrRef != nil ) && (eAttrListRefType == pCurrRef->fType) )
					{
						aChildPtr = pCurrRef->fChildren;
						while (aChildPtr != nil)
						{
							aRefString = (char *)calloc(9, 1);
							sprintf(aRefString,"%8u",(unsigned int)(aChildPtr->fRefNum));
							if ( aChildPtr->fType == eAttrValueListRefType )
							{
								outAttrListValueRefs[outAttrListValueRefCount] = aRefString;
								outAttrListValueRefCount++;
								if (outAttrListValueRefCount >= attrListValueRefListSize)
								{
									GrowList(outAttrListValueRefs, attrListValueRefListSize);
								}
							}
							aChildPtr = aChildPtr->fNext;
						}
					} //has children
				} //valid table
			} //valid ref
			attrListRefIndex++;
			aAttrListRefString = outAttrListRefs[attrListRefIndex];
		}
	}
	
	fTableMutex.SignalLock();

} //RetrieveRefDataPerClientPIDAndIP

void GrowList(char** &inOutRefs, UInt32 &inOutRefListSize)
{
	UInt32 origSize = inOutRefListSize;
	inOutRefListSize = inOutRefListSize * 2;
	char** tempList = (char** )calloc(inOutRefListSize + 1, sizeof(char*));
	memcpy(tempList, inOutRefs, origSize * sizeof(char *));
	free(inOutRefs);
	inOutRefs = tempList;
}

//--------------------------------------------------------------------------------------------------
//	* GetNodeName
//
//--------------------------------------------------------------------------------------------------

char* CRefTable::GetNodeName ( tDirReference inDirRef )
{
	tDirStatus		siResult	= eDSNoErr;
	sRefEntry	   *refData		= nil;
	char		   *outNodeName	= nil;

	siResult = GetReference( inDirRef, &refData );
	//ref actually exists
	if ( siResult == eDSNoErr )
	{
		outNodeName = refData->fNodeName;
	}
	
	return(outNodeName);
} // GetNodeName

