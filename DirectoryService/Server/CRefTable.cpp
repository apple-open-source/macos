/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CRefTable
 */

#include "CRefTable.h"
#include "CLog.h"
//DSSERVERTCP needed to direct TCP error logging
#define DSSERVERTCP 1
#include "DSNetworkUtilities.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysctl.h>	// for struct kinfo_proc and sysctl()
#include <syslog.h>		// for syslog()



//--------------------------------------------------------------------------------------------------
//	* Globals
//--------------------------------------------------------------------------------------------------

DSMutexSemaphore		*gTableMutex	= nil;


//API logging
extern dsBool					gLogAPICalls;

extern uInt32					gDaemonIPAddress;

//------------------------------------------------------------------------------------
//	* CRefTable
//------------------------------------------------------------------------------------

CRefTable::CRefTable ( RefDeallocateProc *deallocProc )
{
	CFNumberRef				aPIDCount			= 0;
	CFStringRef				aPIDString			= 0;
	CFStringRef				anIPString			= 0;
	CFMutableArrayRef		aClientPIDArray;
	uInt32					aCount				= 0;
	CFMutableDictionaryRef	aPIDDict			= 0;

	fSunsetTime = time( nil);
	fClientPIDList = nil;
	
	fClientPIDListLock = new DSMutexSemaphore();
	if (fClientPIDListLock != nil )
	{
		fClientPIDListLock->Wait();
		if (fClientIPList == nil)
		{
			fClientIPList = CFDictionaryCreateMutable(	kCFAllocatorDefault,
														0,
														&kCFTypeDictionaryKeyCallBacks,
														&kCFTypeDictionaryValueCallBacks );
																
			{
				fClientPIDList = CFDictionaryCreateMutable( kCFAllocatorDefault,
															0,
															&kCFTypeDictionaryKeyCallBacks,
															&kCFTypeDictionaryValueCallBacks );
															
				aClientPIDArray = CFArrayCreateMutable( 	kCFAllocatorDefault,
															0,
															&kCFTypeArrayCallBacks);
	
				aPIDString	= CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), getpid());
	
				aPIDDict	= CFDictionaryCreateMutable(	kCFAllocatorDefault,
															0,
															&kCFTypeDictionaryKeyCallBacks,
															&kCFTypeDictionaryValueCallBacks );
															
				aPIDCount	= CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&aCount);
				
				CFDictionarySetValue( aPIDDict, CFSTR("RefCount"), aPIDCount );
				CFDictionarySetValue( fClientPIDList, aPIDString, aPIDDict );
				
				CFArrayAppendValue(aClientPIDArray, aPIDString);
				CFDictionarySetValue( fClientPIDList, CFSTR("ClientPIDArray"), (const void*)aClientPIDArray );
	
				CFRelease(aPIDCount);
				CFRelease(aPIDDict);
				CFRelease(aClientPIDArray);
				CFRelease(aPIDString);
			}

			anIPString	= CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), gDaemonIPAddress);
			CFDictionarySetValue( fClientIPList, anIPString, fClientPIDList );
			
			CFRelease(anIPString);
		}
		fClientPIDListLock->Signal();		
	}

	fTableCount		= 0;
	::memset( fRefTables, 0, sizeof( fRefTables ) );

	if ( gTableMutex == nil )
	{
		gTableMutex = new DSMutexSemaphore();
		if ( gTableMutex == nil ) throw((sInt32)eMemoryAllocError);
	}
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

	if (fClientPIDListLock != nil)
	{
		fClientPIDListLock->Wait();
		if (fClientIPList != nil)
		{
			CFRelease(fClientIPList);
			fClientIPList = 0;
		}
		fClientPIDListLock->Signal();
		delete(fClientPIDListLock);
		fClientPIDListLock = nil;
	}

	if ( gTableMutex != nil )
	{
		delete( gTableMutex );
		gTableMutex = nil;
	}
} // ~CRefTable


//--------------------------------------------------------------------------------------------------
//	* VerifyReference
//
//--------------------------------------------------------------------------------------------------

tDirStatus CRefTable::VerifyReference ( tDirReference	inDirRef,
										uInt32			inType,
										CServerPlugin **outPlugin,
										sInt32			inPID,
										uInt32			inIPAddress )
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
			siResult = eDSInvalidRefType;
		}
		else
		{
			//check the PIDs and IPAddresses here - both parent and child client ones
			siResult = eDSInvalidRefType;
			//parent client PID and IPAddress check
			if ( (refData->fPID == inPID) && (refData->fIPAddress == inIPAddress) )
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
										uInt32				inIPAddress )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gRefTable != nil )
	{
		siResult = gRefTable->VerifyReference( inRecordRef, eRecordRefType, outPlugin, inPID, inIPAddress );
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
		siResult = gRefTable->RemoveRef( inDirRef, eDirectoryRefType, inPID, inIPAddress );
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
		siResult = gRefTable->RemoveRef( inNodeRef, eNodeRefType, inPID, inIPAddress );
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
		siResult = gRefTable->RemoveRef( inRecRef, eRecordRefType, inPID, inIPAddress );
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
		siResult = gRefTable->RemoveRef( inAttrListRef, eAttrListRefType, inPID, inIPAddress );
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
		siResult = gRefTable->RemoveRef( inAttrValueRef, eAttrValueListRefType, inPID, inIPAddress );
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

	gTableMutex->Wait();

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
		ERRORLOG2( kLogAssert, "Reference %l error = %l", inRefNum, err );
	}

	gTableMutex->Signal();

	return( pOutEntry );

} // GetTableRef


//------------------------------------------------------------------------------------
//	* GetNextTable
//------------------------------------------------------------------------------------

sRefTable* CRefTable::GetNextTable ( sRefTable *inTable )
{
	uInt32		uiTblNum	= 0;
	sRefTable	*pOutTable	= nil;

	gTableMutex->Wait();

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
			if (uiTblNum > kMaxTables) throw( (sInt32)kErrEndOfTableList );

			if ( fRefTables[ uiTblNum ] == nil )
			{
				// Set the table counter
				fTableCount = uiTblNum;

				// No tables have been allocated yet so lets make one
				fRefTables[ uiTblNum ] = (sRefTable *)::calloc( sizeof( sRefTable ), sizeof( char ) );
				if ( fRefTables[ uiTblNum ] == nil ) throw((sInt32)eMemoryAllocError);

				if (uiTblNum == 0) throw( (sInt32)kErrZeorRefTableNum );
				fRefTables[ uiTblNum ]->fTableNum = uiTblNum;
			}

			pOutTable = fRefTables[ uiTblNum ];
		}
	}

	catch( sInt32 err )
	{
		DBGLOG1( kLogAssert, "Reference table error = %l", err );
		ERRORLOG1( kLogAssert, "Reference table error = %l", err );
	}

	gTableMutex->Signal();

	return( pOutTable );

} // GetNextTable


//------------------------------------------------------------------------------------
//	* GetThisTable
//------------------------------------------------------------------------------------

sRefTable* CRefTable::GetThisTable ( uInt32 inTableNum )
{
	sRefTable	*pOutTable	= nil;

	gTableMutex->Wait();

	pOutTable = fRefTables[ inTableNum ];

	gTableMutex->Signal();

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

	gTableMutex->Wait();

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

	gTableMutex->Signal();

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

	gTableMutex->Wait();

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

	gTableMutex->Signal();

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

	gTableMutex->Wait();

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

	gTableMutex->Signal();

	return( dsResult );

} // UnlinkFromParent


//------------------------------------------------------------------------------------
//	* GetReference
//------------------------------------------------------------------------------------

tDirStatus CRefTable::GetReference ( uInt32 inRefNum, sRefEntry **outRefData )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;

	gTableMutex->Wait();

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

	gTableMutex->Signal();

	return( dsResult );

} // GetReference


//------------------------------------------------------------------------------------
//	* RemoveRef
//------------------------------------------------------------------------------------

tDirStatus CRefTable::RemoveRef (	uInt32	inRefNum,
									uInt32	inType,
									sInt32	inPID,
									uInt32	inIPAddress )
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


	gTableMutex->Wait();

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
				// need to make sure we release the table mutex before the callback
				gTableMutex->Signal();
				RemoveChildren( pCurrRef->fChildren, inPID, inIPAddress );
				gTableMutex->Wait();
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
						refCountUpdate = UpdateClientPIDRefCount(inPID, inIPAddress, false);
						if (fDeallocProc != nil)
						{
							// need to make sure we release the table mutex before the callback
							// since the search node will try to close dir nodes
							//this Signal works in conjunction with others to handle the recursive nature of the calls here
							//note that since RemoveRef is potentially highly recursive we don't want to run into a mutex deadlock when
							//we employ different threads to do the cleanup inside the fDeallocProc like in the case of the search node
							gTableMutex->Signal();
							dsResult = (tDirStatus)(*fDeallocProc)(inRefNum, pCurrRef);
							gTableMutex->Wait();
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

	gTableMutex->Signal();

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
//	sRefEntry	   *pCurrRef		= nil;

	gTableMutex->Wait();

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
				gTableMutex->Signal();
				RemoveRef( pCurrChild->fRefNum, pCurrChild->fType, inPID, inIPAddress );
				gTableMutex->Wait();
			}

			pCurrChild = pNextChild;
		}
	}

	catch( sInt32 err )
	{
	}

	gTableMutex->Signal();

} // RemoveChildren



//------------------------------------------------------------------------------------
//	* SetPluginPtr
//------------------------------------------------------------------------------------

tDirStatus CRefTable::SetPluginPtr ( uInt32 inRefNum, uInt32 inType, CServerPlugin *inPlugin )
{
	tDirStatus		dsResult		= eDSNoErr;
	sRefEntry	   *pCurrRef		= nil;

	gTableMutex->Wait();

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

	gTableMutex->Signal();

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

	gTableMutex->Wait();

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

	gTableMutex->Signal();

	return( dsResult );

} // AddChildPIDToRef


// ----------------------------------------------------------------------------
//	* UpdateClientPIDRefCount()
//
// ----------------------------------------------------------------------------

uInt32 CRefTable:: UpdateClientPIDRefCount ( sInt32 inClientPID, uInt32 inIPAddress, bool inUpRefCount, uInt32 inDirRef )
{
	//if inUpRefCount is true then increment else decrement

	CFNumberRef				aRefCount			= 0;
	CFStringRef				aComparePIDString	= 0;
	CFStringRef				aCompareIPString	= 0;
	uInt32					aCount				= 0;
	CFNumberRef				aPIDCount			= 0;
	CFStringRef				aPIDString			= 0;
	CFMutableArrayRef		aClientPIDArray;
	CFMutableDictionaryRef	aPIDDict			= 0;
	CFStringRef				anIPString			= 0;

	//what to do here:
	//lock list
	//check if list not nil
	//create compare pid string
	//check if PID already in dict
	//if in then get ref count and update
	//KW TODO if not in then add with ref count of one
	//unlock list
	
   	fClientPIDListLock->Wait();
	if (fClientIPList != nil)
	{
   		aCompareIPString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), inIPAddress);
		
		if ( CFDictionaryContainsKey( fClientIPList, aCompareIPString ) )
		{
		
			fClientPIDList = nil;
			fClientPIDList = (CFMutableDictionaryRef)CFDictionaryGetValue( fClientIPList, aCompareIPString );
			if (fClientPIDList != nil)
			{
				aComparePIDString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), inClientPID);
		
				//already have an entry
				if ( CFDictionaryContainsKey( fClientPIDList, aComparePIDString ) )
				{
					aPIDDict	= (CFMutableDictionaryRef)CFDictionaryGetValue( fClientPIDList, aComparePIDString );
					aRefCount	= (CFNumberRef)CFDictionaryGetValue( aPIDDict, CFSTR("RefCount") );
					CFNumberGetValue(aRefCount, kCFNumberIntType, &aCount);
					if (inUpRefCount)
					{
						aCount++;
						if (gLogAPICalls)
						{
							syslog(LOG_INFO,"Client PID: %d, has %d open references.", inClientPID, aCount);
						}
					}
					else
					{
						aCount--;
						if (gLogAPICalls)
						{
							syslog(LOG_INFO,"Client PID: %d, has %d open references.", inClientPID, aCount);
						}
					}
					//let's remove it entirely from the list when there are no refs
					if (aCount == 0)
					{
						CFMutableArrayRef	clientPIDArray;
						clientPIDArray = (CFMutableArrayRef)CFDictionaryGetValue(fClientPIDList, CFSTR("ClientPIDArray"));
						for (CFIndex indexToPID=0; indexToPID < CFArrayGetCount( clientPIDArray ); indexToPID++ )
						{
							if (kCFCompareEqualTo == CFStringCompare((CFStringRef)CFArrayGetValueAtIndex(clientPIDArray, indexToPID), aComparePIDString, 0))
							{
								CFArrayRemoveValueAtIndex(clientPIDArray, indexToPID);
								//remove the dict entry as well
								CFDictionaryRemoveValue(fClientPIDList, aComparePIDString);
								break;
							}
						}
					}
					//update since not zero
					else
					{
						//KW do I need to release the current CFNumberRef since I am creating a new one?
						//CFRelease(aRefCount);
						aRefCount = CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&aCount);
		
						if (inDirRef != 0)	//this should really only be for DS itself since we create the dict entry before we get a dir ref
											//OR a case where the dir ref is cleaned up and some ref is left over???
						{
							if ( CFDictionaryContainsKey( aPIDDict, CFSTR("DirRefs")) )
							{
								CFMutableArrayRef aDirRefArray = (CFMutableArrayRef)CFDictionaryGetValue(aPIDDict, CFSTR("DirRefs"));
								
								//add the dir ref to the array
								CFNumberRef aCFDirRef = CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&inDirRef);
								CFArrayAppendValue(aDirRefArray, aCFDirRef);
		
								CFRelease(aCFDirRef);
							}
							else
							{
								//create a dir ref array
								CFMutableArrayRef aDirRefArray = CFArrayCreateMutable(	kCFAllocatorDefault,
																					0,
																					&kCFTypeArrayCallBacks);
								//add the dir ref to the array
								CFNumberRef aCFDirRef = CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&inDirRef);
								CFArrayAppendValue(aDirRefArray, aCFDirRef);
								CFDictionarySetValue( aPIDDict, CFSTR("DirRefs"), aDirRefArray );
		
								CFRelease(aCFDirRef);
								CFRelease(aDirRefArray);
							}
						}
		
						//for all update the PID Count
						CFDictionaryReplaceValue(aPIDDict, CFSTR("RefCount"), aRefCount);
						CFRelease(aRefCount);
					}
				}
				//need to create a new entry only on the up count
				else if (inUpRefCount)
				{
					CFMutableArrayRef	clientPIDArray;
					clientPIDArray = (CFMutableArrayRef)CFDictionaryGetValue(fClientPIDList, CFSTR("ClientPIDArray"));
					CFArrayAppendValue(clientPIDArray, aComparePIDString);
		
					aCount = 1;
					aRefCount	= CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&aCount);
					aPIDDict	= CFDictionaryCreateMutable(	kCFAllocatorDefault,
																0,
																&kCFTypeDictionaryKeyCallBacks,
																&kCFTypeDictionaryValueCallBacks );
					
					CFDictionarySetValue( aPIDDict, CFSTR("RefCount"), aRefCount );
					CFRelease(aRefCount);
					if (inDirRef != 0)
					{
						//create a dir ref array
						CFMutableArrayRef aDirRefArray = CFArrayCreateMutable(	kCFAllocatorDefault,
																			0,
																			&kCFTypeArrayCallBacks);
						//add the dir ref to the array
						CFNumberRef aCFDirRef = CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&inDirRef);
						CFArrayAppendValue(aDirRefArray, aCFDirRef);
						CFDictionarySetValue( aPIDDict, CFSTR("DirRefs"), aDirRefArray );
		
						CFRelease(aCFDirRef);
						CFRelease(aDirRefArray);
						
					}
					CFDictionarySetValue( fClientPIDList, aComparePIDString, aPIDDict );
		
					CFRelease(aPIDDict);
				}
		
				CFRelease(aComparePIDString);
			}
		}
		else
		{
			fClientPIDList = CFDictionaryCreateMutable( kCFAllocatorDefault,
														0,
														&kCFTypeDictionaryKeyCallBacks,
														&kCFTypeDictionaryValueCallBacks );
														
			aClientPIDArray = CFArrayCreateMutable( 	kCFAllocatorDefault,
														0,
														&kCFTypeArrayCallBacks);

			aPIDString	= CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), inClientPID);

			aPIDDict	= CFDictionaryCreateMutable(	kCFAllocatorDefault,
														0,
														&kCFTypeDictionaryKeyCallBacks,
														&kCFTypeDictionaryValueCallBacks );
			if (inDirRef != 0)
			{
				//create a dir ref array
				CFMutableArrayRef aDirRefArray = CFArrayCreateMutable(	kCFAllocatorDefault,
																		0,
																		&kCFTypeArrayCallBacks);
				//add the dir ref to the array
				CFNumberRef aCFDirRef = CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&inDirRef);
				CFArrayAppendValue(aDirRefArray, aCFDirRef);
				CFDictionarySetValue( aPIDDict, CFSTR("DirRefs"), aDirRefArray );

				aCount		= 1;											
				aPIDCount	= CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&aCount);
				
				CFDictionarySetValue( aPIDDict, CFSTR("RefCount"), aPIDCount );
				CFDictionarySetValue( fClientPIDList, aPIDString, aPIDDict );
			
				CFRelease(aCFDirRef);
				CFRelease(aDirRefArray);
				
			}
			
			CFArrayAppendValue(aClientPIDArray, aPIDString);
			CFDictionarySetValue( fClientPIDList, CFSTR("ClientPIDArray"), (const void*)aClientPIDArray );

			CFRelease(aPIDCount);
			CFRelease(aPIDDict);
			CFRelease(aClientPIDArray);
			CFRelease(aPIDString);

			anIPString	= CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), inIPAddress);
			CFDictionarySetValue( fClientIPList, anIPString, fClientPIDList );
			
			CFRelease(anIPString);
		}
		
		CFRelease(aCompareIPString);
	}
   	fClientPIDListLock->Signal();


	return aCount;
	
} // UpdateClientPIDRefCount

// ----------------------------------------------------------------------------
//	* CheckClientPIDs() pass through static
//
// ----------------------------------------------------------------------------

void CRefTable::CheckClientPIDs ( bool inUseTimeOuts, uInt32 inIPAddress, uInt32 inPIDorPort )
{
	if (gRefTable != nil)
	{
		gRefTable->DoCheckClientPIDs(inUseTimeOuts, inIPAddress, inPIDorPort);
	}
} // CheckClientPIDs


// ----------------------------------------------------------------------------
//	* DoCheckClientPIDs() //if inIPAddress is zero then a local cleanup check
//
// ----------------------------------------------------------------------------

void CRefTable::DoCheckClientPIDs ( bool inUseTimeOuts, uInt32 inIPAddress, uInt32 inPIDorPort )
{
	CFStringRef			aPIDString			= 0;
	CFStringRef			aCompareIPString	= 0;
	char				aStringBuff[8];
	sInt32				aPIDValue			= 0;
	CFNumberRef			aRefCount			= 0;
	uInt32				aCount				= 0;
	size_t				iSize;
	register size_t		i					= 0;
	bool		 		bPIDRunning			= true;
	int					mib []				= { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t				ulSize				= 0;
	CFMutableDictionaryRef
						aPIDDict			= 0;
	uInt32				aDirRef				= 0;
	CFStringRef			aNullStringName		= 0;
	CFIndex				indexToPID			= 0;
	sInt32				siResult			= eDSNoErr;
	struct kinfo_proc  *kpspArray			= nil;

	if (inUseTimeOuts)
	{
		if (::time( nil ) < fSunsetTime)
		{
			return;
		}
	}
	//wait one second since refs might be updated for other client PIDs
	if (fClientPIDListLock->Wait(1) != eDSNoErr)
	{
		fClientPIDListLock->Signal(); //ensure mutex is released
		return;
	}

	aNullStringName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), 0);
	
	if (inIPAddress == 0)
	{
		//retrieve the process table for client PIDs
		// Allocate space for complete process list.
		if ( 0 > sysctl( mib, 4, NULL, &ulSize, NULL, 0) )
		{
			fClientPIDListLock->Signal(); //ensure mutex is released
			return; //ie. do nothing
		}
	
		iSize = ulSize / sizeof(struct kinfo_proc);
		kpspArray = new kinfo_proc[ iSize ];
		if (!kpspArray)
		{
			fClientPIDListLock->Signal(); //ensure mutex is released
			return; //ie. do nothing
		}
	
		// Get the proc list.
		ulSize = iSize * sizeof(struct kinfo_proc);
		if ( 0 > sysctl( mib, 4, kpspArray, &ulSize, NULL, 0 ) )
		{
			delete [] kpspArray;
			fClientPIDListLock->Signal(); //ensure mutex is released
			return; //ie. do nothing
		}
	}

	//check if any client PID is no longer a process
	//can only look at PIDs that have the same IPAddress as this daemon ie. gDaemonIPAddress
	//if not then cleanup the references associated with that missing process
	//what to do here:
	//lock list
	//check if list not nil
	//loop over pids in the array within the list
	//each pid has refs since if zero in UpdateClientPIDRefCount method we take it out of the list
	//get all the pids from the process table and ... ?????
	//check if our pids are NOT in the process table
	//if they aren't then call cleanup functions for that pid - only need to call it on the DS references itself
	//since at that point all child refs will also be cleaned up
	//unlock the list
	
	if (fClientIPList != nil)
	{
		aCompareIPString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), inIPAddress);
		
		if ( CFDictionaryContainsKey( fClientIPList, aCompareIPString ) )
		{
		
			fClientPIDList = nil;
			fClientPIDList = (CFMutableDictionaryRef)CFDictionaryGetValue( fClientIPList, aCompareIPString );
			
			if (fClientPIDList != nil)
			{
				CFMutableArrayRef	clientPIDArray;
				clientPIDArray = (CFMutableArrayRef)CFDictionaryGetValue(fClientPIDList, CFSTR("ClientPIDArray"));
				for (indexToPID=0; indexToPID < CFArrayGetCount( clientPIDArray ); indexToPID++ )
				{
					aPIDString = (CFStringRef)CFArrayGetValueAtIndex(clientPIDArray, indexToPID);
					//getting a PID from the array means that there are refs left
					memset(aStringBuff, 0, sizeof(aStringBuff));
					CFStringGetCString( aPIDString, aStringBuff, sizeof( aStringBuff ), kCFStringEncodingMacRoman );
					char *endPtr = nil;
					aPIDValue = (sInt32)strtol(aStringBuff, &endPtr, 10);
					
					//bPIDRunning set to false unless found in loop below
					bPIDRunning = false;
		
					if (inIPAddress == 0)
					{
						i = iSize;
						register struct kinfo_proc	*kpsp = kpspArray;
			
						for ( ; i-- ; kpsp++ )
						{
							// skip our own process
							//if ( kpsp->kp_proc.p_pid == ::getpid() )
							//{
								//continue;
							//}
			
							if (aPIDValue == kpsp->kp_proc.p_pid)
							{
								bPIDRunning = true;
								break;
							}
						}
					}
					else if ((uInt32)aPIDValue != inPIDorPort)
					{
						bPIDRunning = true;
					}
		
					if ( CFDictionaryContainsKey( fClientPIDList, aPIDString ) )
					{
						aPIDDict	= (CFMutableDictionaryRef)CFDictionaryGetValue( fClientPIDList, aPIDString );
						aRefCount	= (CFNumberRef)CFDictionaryGetValue( aPIDDict, CFSTR("RefCount") );
						CFMutableArrayRef aDirRefArray = (CFMutableArrayRef)CFDictionaryGetValue(aPIDDict, CFSTR("DirRefs"));
						
						//don't believe aRefCount is retained so don't release
						CFNumberGetValue(aRefCount, kCFNumberIntType, &aCount);
						if (bPIDRunning)
						{
							if (gLogAPICalls)
							{
								syslog(LOG_INFO,"Client PID %d has ref count = %d",aPIDValue, aCount);
							}
							DBGLOG2( kLogHandler, "The client PID %d has ref count = %d.", aPIDValue, aCount );
						}
						else
						{
							//KW issue when a client crashes and we are still servicing the call ie. using the refs
							//but here we want to clean up all the refs SO
							//need to wait (?) some cleanup time period greater than say 5 minutes (current mach timeout)
							//so that there will not be a deadlock on the use of the references when we want to remove them and
							//at the same time we are finishing out the processing for the client's call
							//can we simply put a timetag inside the dict and check it if it has expried so that we
							//can proceed with the cleanup
		
							//remove the entry from the dict
							CFRetain(aPIDDict);
							CFDictionaryRemoveValue(fClientPIDList, aPIDString);
							CFArraySetValueAtIndex(clientPIDArray, indexToPID, aNullStringName);
							
							fClientPIDListLock->Signal();
							//let's clean up this PID's references
							for (CFIndex indexToDirRef=0; indexToDirRef < CFArrayGetCount( aDirRefArray ); indexToDirRef++ )
							{
								//get the dir ref value
								CFNumberRef aCFDirRef= (CFNumberRef)CFArrayGetValueAtIndex( aDirRefArray, indexToDirRef );
								CFNumberGetValue(aCFDirRef, kCFNumberIntType, &aDirRef);
		
								siResult = CRefTable::RemoveDirRef( aDirRef, aPIDValue, inIPAddress );
							}
							fClientPIDListLock->Wait();
							//KW for now output info always
							if (gLogAPICalls)
							{
								syslog(LOG_INFO,"Client PID %d had ref count = %d before cleanup.",aPIDValue, aCount);
							}
							DBGLOG2( kLogHandler, "The client PID %d had ref count = %d before cleanup.", aPIDValue, aCount );
							CFRelease(aPIDDict);
						}
					}
				}
				//cleanup all the removed PIDs
				indexToPID = (CFArrayGetCount( clientPIDArray )) - 1;
				while (indexToPID > 0) //note index 0 points to this process so no need to check
				{
					if (kCFCompareEqualTo == CFStringCompare((CFStringRef)CFArrayGetValueAtIndex(clientPIDArray, indexToPID), aNullStringName, 0))
					{
						CFArrayRemoveValueAtIndex(clientPIDArray, indexToPID);
					}
					indexToPID--;
				}
			}
		}
		
		CFRelease(aCompareIPString);
	}
	
	CFRelease(aNullStringName);
	
	fSunsetTime = time(nil) + 300;

	fClientPIDListLock->Signal();

	if (kpspArray != NULL)
	{
		delete [] kpspArray;
	}

} // DoCheckClientPIDs

