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
 * @header CDSRefTable
 * DirectoryService Framework reference table for all
 * "client side buffer parsing" operations on plugin data
 * returned in standard buffer format.
 * References here always use 0x00300000 bits
 */

#include "CDSRefTable.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysctl.h>	// for struct kinfo_proc and sysctl()
#include <syslog.h>		// for syslog() to log calls
#include "PrivateTypes.h"



//--------------------------------------------------------------------------------------------------
//	* Globals
//--------------------------------------------------------------------------------------------------

DSMutexSemaphore		*gFWTableMutex	= nil;


//FW client references logging
dsBool	gLogFWRefCalls = false;
//KW need realtime mechanism to set this to true ie. getenv variable OR ?
//KW greatest difficulty is the bookkeeping arrays which were CF based and need to be replaced
//so that the FW does not depend upon CoreFoundation - is this really a problem?

//------------------------------------------------------------------------------------
//	* CDSRefTable
//------------------------------------------------------------------------------------

CDSRefTable::CDSRefTable ( RefFWDeallocateProc *deallocProc )
{
/*RCF
	CFNumberRef				aPIDCount			= 0;
	CFStringRef				aPIDString			= 0;
	CFMutableArrayRef		aClientPIDArray;
	uInt32					aCount				= 0;
	CFMutableDictionaryRef	aPIDDict			= 0;

	fSunsetTime = time( nil);
	
	fClientPIDListLock = new DSMutexSemaphore;
	if (fClientPIDListLock != nil )
	{
		fClientPIDListLock->Wait();
		if (fClientPIDList == nil)
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
		fClientPIDListLock->Signal();		
	}
RCF */
	fTableCount		= 0;
	::memset( fRefTables, 0, sizeof( fRefTables ) );

	if ( gFWTableMutex == nil )
	{
		gFWTableMutex = new DSMutexSemaphore();
		if ( gFWTableMutex == nil ) throw((sInt32)eMemoryAllocError);
	}
	fDeallocProc = deallocProc;
} // CDSRefTable


//------------------------------------------------------------------------------------
//	* ~CDSRefTable
//------------------------------------------------------------------------------------

CDSRefTable::~CDSRefTable ( void )
{
	uInt32		i	= 1;
	uInt32		j	= 1;

	for ( i = 1; i <= kMaxFWTables; i++ )	//array is still zero based even if first entry NOT used
										//-- added the last kMaxTable in the .h file so this should work now
	{
		if ( fRefTables[ i ] != nil )
		{
			for (j=0; j< kMaxFWTableItems; j++)
			{
				if (fRefTables[ i ]->fTableData[j] != nil)
				{
					free(fRefTables[ i ]->fTableData[j]);
					fRefTables[ i ]->fTableData[j] = nil;
				}
			}
			free( fRefTables[ i ] );  //free all the calloc'ed sRefEntries inside as well - code above
			fRefTables[ i ] = nil;
		}
	}

/*RCF
	if (fClientPIDListLock != nil)
	{
		fClientPIDListLock->Wait();
		if (fClientPIDList != nil)
		{
			CFRelease(fClientPIDList);
			fClientPIDList = 0;
		}
		fClientPIDListLock->Signal();
		delete(fClientPIDListLock);
		fClientPIDListLock = nil;
	}
RCF */

	if ( gFWTableMutex != nil )
	{
		delete( gFWTableMutex );
		gFWTableMutex = nil;
	}
} // ~CDSRefTable


//--------------------------------------------------------------------------------------------------
//	* VerifyReference
//
//--------------------------------------------------------------------------------------------------

tDirStatus CDSRefTable::VerifyReference (	tDirReference	inDirRef,
                                            uInt32			inType,
                                            sInt32			inPID )
{
	tDirStatus		siResult	= eDSNoErr;
	sFWRefEntry	   *refData		= nil;
	sPIDFWInfo	   *pPIDInfo	= nil;

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

	return( siResult );

} // VerifyReference



//------------------------------------------------------------------------------------
//	* VerifyDirRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::VerifyDirRef (	tDirReference		inDirRef,
										sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->VerifyReference( inDirRef, eDirectoryRefType, inPID );
	}

	return( siResult );

} // VerifyDirRef



//------------------------------------------------------------------------------------
//	* VerifyNodeRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::VerifyNodeRef (	tDirNodeReference	inDirNodeRef,
										sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->VerifyReference( inDirNodeRef, eNodeRefType, inPID );
	}

	return( siResult );

} // VerifyNodeRef


//------------------------------------------------------------------------------------
//	* VerifyRecordRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::VerifyRecordRef (	tRecordReference	inRecordRef,
                                            sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->VerifyReference( inRecordRef, eRecordRefType, inPID );
	}

	return( siResult );

} // VerifyRecordRef



//------------------------------------------------------------------------------------
//	* VerifyAttrListRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::VerifyAttrListRef (	tAttributeListRef	inAttributeListRef,
											sInt32				inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->VerifyReference( inAttributeListRef, eAttrListRefType, inPID );
	}

	return( siResult );

} // VerifyAttrListRef


//------------------------------------------------------------------------------------
//	* VerifyAttrValueRef (static)
//
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::VerifyAttrValueRef (	tAttributeValueListRef	inAttributeValueListRef,
                                                sInt32					inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->VerifyReference( inAttributeValueListRef, eAttrValueListRefType, inPID );
	}

	return( siResult );

} // VerifyAttrValueRef


//------------------------------------------------------------------------------------
//	* NewDirRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::NewDirRef ( uInt32 *outNewRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->GetNewRef( outNewRef, 0, eDirectoryRefType, inPID );
	}

	return( siResult );

} // NewDirRef


//------------------------------------------------------------------------------------
//	* NewNodeRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::NewNodeRef (	uInt32			*outNewRef,
										uInt32			inParentID,
										sInt32			inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->GetNewRef( outNewRef, inParentID, eNodeRefType, inPID );
	}

	return( siResult );

} // NewNodeRef


//------------------------------------------------------------------------------------
//	* NewRecordRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::NewRecordRef (	uInt32			*outNewRef,
										uInt32			inParentID,
										sInt32			inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->GetNewRef( outNewRef, inParentID, eRecordRefType, inPID );
	}

	return( siResult );

} // NewRecordRef


//------------------------------------------------------------------------------------
//	* NewAttrListRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::NewAttrListRef (	uInt32			*outNewRef,
											uInt32			inParentID,
											sInt32			inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->GetNewRef( outNewRef, inParentID, eAttrListRefType, inPID );
	}

	return( siResult );

} // NewAttrListRef


//------------------------------------------------------------------------------------
//	* NewAttrValueRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::NewAttrValueRef (	uInt32			*outNewRef,
											uInt32			inParentID,
											sInt32			inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->GetNewRef( outNewRef, inParentID, eAttrValueListRefType, inPID );
	}

	return( siResult );

} // NewAttrValueRef


//------------------------------------------------------------------------------------
//	* RemoveDirRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::RemoveDirRef ( uInt32 inDirRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->RemoveRef( inDirRef, eDirectoryRefType, inPID );
	}

	return( siResult );

} // RemoveDirRef


//------------------------------------------------------------------------------------
//	* RemoveNodeRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::RemoveNodeRef ( uInt32 inNodeRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->RemoveRef( inNodeRef, eNodeRefType, inPID );
	}

	return( siResult );

} // RemoveNodeRef


//------------------------------------------------------------------------------------
//	* RemoveRecordRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::RemoveRecordRef ( uInt32 inRecRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->RemoveRef( inRecRef, eRecordRefType, inPID );
	}

	return( siResult );

} // RemoveRecordRef


//------------------------------------------------------------------------------------
//	* RemoveAttrListRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::RemoveAttrListRef ( uInt32 inAttrListRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->RemoveRef( inAttrListRef, eAttrListRefType, inPID );
	}

	return( siResult );

} // RemoveAttrListRef


//------------------------------------------------------------------------------------
//	* RemoveAttrValueRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::RemoveAttrValueRef ( uInt32 inAttrValueRef, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;

	if ( gFWRefTable != nil )
	{
		siResult = gFWRefTable->RemoveRef( inAttrValueRef, eAttrValueListRefType, inPID );
	}

	return( siResult );

} // RemoveAttrValueRef


//------------------------------------------------------------------------------------
//	* GetTableRef
//------------------------------------------------------------------------------------

sFWRefEntry* CDSRefTable::GetTableRef ( uInt32 inRefNum )
{
	uInt32			uiSlot			= 0;
	uInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	uInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	sRefFWTable	   *pTable			= nil;
	sFWRefEntry	   *pOutEntry		= nil;

	gFWTableMutex->Wait();

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

	gFWTableMutex->Signal();

	return( pOutEntry );

} // GetTableRef


//------------------------------------------------------------------------------------
//	* GetNextTable
//------------------------------------------------------------------------------------

sRefFWTable* CDSRefTable::GetNextTable ( sRefFWTable *inTable )
{
	uInt32		uiTblNum	= 0;
	sRefFWTable	*pOutTable	= nil;

	gFWTableMutex->Wait();

	try
	{
		if ( inTable == nil )
		{
			// Get the first reference table, create it if it wasn't already
			if ( fRefTables[ 1 ] == nil )
			{
				// No tables have been allocated yet so lets make one
				fRefTables[ 1 ] = (sRefFWTable *)::calloc( sizeof( sRefFWTable ), sizeof( char ) );
				if ( fRefTables[ 1 ] == nil )  throw((sInt32)eMemoryAllocError);

				fRefTables[ 1 ]->fTableNum = 1;

				fTableCount = 1;
			}

			pOutTable = fRefTables[ 1 ];
		}
		else
		{
			uiTblNum = inTable->fTableNum + 1;
			if (uiTblNum > kMaxFWTables) throw( (sInt32)eDSInvalidReference );

			if ( fRefTables[ uiTblNum ] == nil )
			{
				// Set the table counter
				fTableCount = uiTblNum;

				// No tables have been allocated yet so lets make one
				fRefTables[ uiTblNum ] = (sRefFWTable *)::calloc( sizeof( sRefFWTable ), sizeof( char ) );
				if ( fRefTables[ uiTblNum ] == nil ) throw((sInt32)eMemoryAllocError);

				if (uiTblNum == 0) throw( (sInt32)eDSInvalidReference );
				fRefTables[ uiTblNum ]->fTableNum = uiTblNum;
			}

			pOutTable = fRefTables[ uiTblNum ];
		}
	}

	catch( sInt32 err )
	{
	}

	gFWTableMutex->Signal();

	return( pOutTable );

} // GetNextTable


//------------------------------------------------------------------------------------
//	* GetThisTable
//------------------------------------------------------------------------------------

sRefFWTable* CDSRefTable::GetThisTable ( uInt32 inTableNum )
{
	sRefFWTable	*pOutTable	= nil;

	gFWTableMutex->Wait();

	pOutTable = fRefTables[ inTableNum ];

	gFWTableMutex->Signal();

	return( pOutTable );

} // GetThisTable


//------------------------------------------------------------------------------------
//	* GetNewRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::GetNewRef (	uInt32		   *outRef,
									uInt32			inParentID,
									eRefTypes		inType,
									sInt32			inPID )
{
	bool			done		= false;
	tDirStatus		outResult	= eDSNoErr;
	sRefFWTable	   *pCurTable	= nil;
	uInt32			uiRefNum	= 0;
	uInt32			uiCntr		= 0;
	uInt32			uiSlot		= 0;
	uInt32			uiTableNum	= 0;
	uInt32			refCountUpdate	= 0;

	gFWTableMutex->Wait();

	try
	{
		*outRef = 0;

		while ( !done )
		{
			pCurTable = GetNextTable( pCurTable );
			if ( pCurTable == nil ) throw( (sInt32)eDSRefTableCSBPAllocError );

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
					uiRefNum += 0x00300000;

					// Find a slot in the table for this ref number
					uiSlot = uiRefNum % kMaxFWTableItems;
					if ( pCurTable->fTableData[ uiSlot ] == nil )
					{
						pCurTable->fTableData[ uiSlot ] = (sFWRefEntry *)::calloc( sizeof( sFWRefEntry ), sizeof( char ) );
						if ( pCurTable->fTableData[ uiSlot ] == nil ) throw( (sInt32)eDSRefTableCSBPAllocError );
						
						// We found an empty slot, now set this table entry
						pCurTable->fTableData[ uiSlot ]->fRefNum		= uiRefNum;
						pCurTable->fTableData[ uiSlot ]->fType			= inType;
						pCurTable->fTableData[ uiSlot ]->fParentID		= inParentID;
						pCurTable->fTableData[ uiSlot ]->fPID			= inPID;
						pCurTable->fTableData[ uiSlot ]->fOffset		= 0;
						pCurTable->fTableData[ uiSlot ]->fBufTag		= 0;
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
							refCountUpdate = UpdateClientPIDRefCount(inPID, true, uiRefNum);
						}
						else
						{
							refCountUpdate = UpdateClientPIDRefCount(inPID, true);
						}
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

	gFWTableMutex->Signal();

	return( outResult );

} // GetNewRef


//------------------------------------------------------------------------------------
//	* LinkToParent
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::LinkToParent ( uInt32 inRefNum, uInt32 inType, uInt32 inParentID, sInt32 inPID )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefEntry	   *pCurrRef		= nil;
	sListFWInfo	   *pChildInfo		= nil;

	gFWTableMutex->Wait();

	try
	{
		pCurrRef = GetTableRef( inParentID );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		// This is the one we want
		pChildInfo = (sListFWInfo *)::calloc( sizeof( sListFWInfo ), sizeof( char ) );
		if ( pChildInfo == nil ) throw( (sInt32)eDSRefTableCSBPAllocError );

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

	gFWTableMutex->Signal();

	return( dsResult );

} // LinkToParent


//------------------------------------------------------------------------------------
//	* UnlinkFromParent
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::UnlinkFromParent ( uInt32 inRefNum )
{
	tDirStatus		dsResult		= eDSNoErr;
	uInt32			i				= 1;
	uInt32			parentID		= 0;
	sFWRefEntry	   *pCurrRef		= nil;
	sFWRefEntry	   *pParentRef		= nil;
	sListFWInfo	   *pCurrChild		= nil;
	sListFWInfo	   *pPrevChild		= nil;

	gFWTableMutex->Wait();

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

	gFWTableMutex->Signal();

	return( dsResult );

} // UnlinkFromParent


//------------------------------------------------------------------------------------
//	* GetReference
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::GetReference ( uInt32 inRefNum, sFWRefEntry **outRefData )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefEntry	   *pCurrRef		= nil;

	gFWTableMutex->Wait();

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

	gFWTableMutex->Signal();

	return( dsResult );

} // GetReference


//------------------------------------------------------------------------------------
//	* RemoveRef
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::RemoveRef ( uInt32 inRefNum, uInt32 inType, sInt32 inPID )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefEntry	   *pCurrRef		= nil;
	sRefFWTable	   *pTable			= nil;
	uInt32			uiSlot			= 0;
	uInt32			uiTableNum		= (inRefNum & 0xFF000000) >> 24;
	uInt32			uiRefNum		= (inRefNum & 0x00FFFFFF);
	bool			doFree			= false;
	sPIDFWInfo	   *pPIDInfo		= nil;
	sPIDFWInfo	   *pPrevPIDInfo	= nil;
	uInt32			refCountUpdate	= 0;


	gFWTableMutex->Wait();

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

			pCurrRef = GetTableRef( inRefNum );	//KW80 - here we need to know where the sFWRefEntry is in the table to delete it
												//that is all the code added above
												// getting this sFWRefEntry is the same path used by uiSlot and pTable above
			if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );
			if (inType != pCurrRef->fType) throw( (sInt32)eDSInvalidReference );

			if ( pCurrRef->fChildren != nil )
			{
				// Its ok to keep the mutex when calling RemoveChildren, it also calls Wait but we are on the same thread
// KA - Need to revisit, commenting these out seems to have caused a regression and more deadlocking.
				gFWTableMutex->Signal();
				RemoveChildren( pCurrRef->fChildren, inPID );
				gFWTableMutex->Wait();
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
						refCountUpdate = UpdateClientPIDRefCount(inPID, false);
						if (fDeallocProc != nil)
						{
							// need to make sure we release the table mutex before the callback
							// since the search node will try to close dir nodes
							//this Signal works in conjunction with others to handle the recursive nature of the calls here
							//note that since RemoveRef is potentially highly recursive we don't want to run into a mutex deadlock when
							//we employ different threads to do the cleanup inside the fDeallocProc like in the case of the search node
							gFWTableMutex->Signal();
							dsResult = (tDirStatus)(*fDeallocProc)(inRefNum, pCurrRef);
							gFWTableMutex->Wait();
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

	gFWTableMutex->Signal();

	return( dsResult );

} // RemoveRef


//------------------------------------------------------------------------------------
//	* RemoveChildren
//------------------------------------------------------------------------------------

void CDSRefTable::RemoveChildren ( sListFWInfo *inChildList, sInt32 inPID )
{
	sListFWInfo	   *pCurrChild		= nil;
	sListFWInfo	   *pNextChild		= nil;
//	sFWRefEntry	   *pCurrRef		= nil;

	gFWTableMutex->Wait();

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
				// Its ok to keep the mutex when calling RemoveRef, it also calls Wait but we are on the same thread
// KA - Need to revisit, commenting these out seems to have caused a regression and more deadlocking.
				gFWTableMutex->Signal();
				RemoveRef( pCurrChild->fRefNum, pCurrChild->fType, inPID );
				gFWTableMutex->Wait();
			}

			pCurrChild = pNextChild;
		}
	}

	catch( sInt32 err )
	{
	}

	gFWTableMutex->Signal();

} // RemoveChildren


//------------------------------------------------------------------------------------
//	* AddChildPIDToRef (static)
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable:: AddChildPIDToRef ( uInt32 inRefNum, uInt32 inParentPID, sInt32 inChildPID )
{
	tDirStatus		dsResult		= eDSNoErr;
	sFWRefEntry	   *pCurrRef		= nil;
	sPIDFWInfo	   *pChildPIDInfo	= nil;

	gFWTableMutex->Wait();

	try
	{
		dsResult = gFWRefTable->VerifyReference( inRefNum, eNodeRefType, inParentPID );
		if ( dsResult != eDSNoErr ) throw( (sInt32)dsResult );

		pCurrRef = gFWRefTable->GetTableRef( inRefNum );
		if ( pCurrRef == nil ) throw( (sInt32)eDSInvalidReference );

		pChildPIDInfo = (sPIDFWInfo *)::calloc( 1, sizeof( sPIDFWInfo ) );
		if ( pChildPIDInfo == nil ) throw( (sInt32)eDSRefTableCSBPAllocError );

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

	gFWTableMutex->Signal();

	return( dsResult );

} // AddChildPIDToRef


// ----------------------------------------------------------------------------
//	* UpdateClientPIDRefCount()
//
// ----------------------------------------------------------------------------

uInt32 CDSRefTable:: UpdateClientPIDRefCount ( sInt32 inClientPID, bool inUpRefCount, uInt32 inDirRef )
{
/*RCF
	//if inUpRefCount is true then increment else decrement

	CFNumberRef			aRefCount			= 0;
	CFStringRef			aComparePIDString	= 0;
	uInt32				aCount				= 0;
	CFMutableDictionaryRef
						aPIDDict			= 0;

	//what to do here:
	//lock list
	//check if list not nil
	//create compare pid string
	//check if PID already in dict
	//if in then get ref count and update
	//if not in then add with ref count of one
	//unlock list
	
   	fClientPIDListLock->Wait();
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
				if (gLogFWRefCalls)
				{
					syslog(LOG_INFO,"Client PID: %d, has %d open references.", inClientPID, aCount);
				}
//				DBGLOG2( kLogHandler, "The client PID %d has %d open references.", inClientPID, aCount );
			}
			else
			{
				aCount--;
				if (gLogFWRefCalls)
				{
					syslog(LOG_INFO,"Client PID: %d, has %d open references.", inClientPID, aCount);
				}
//				DBGLOG2( kLogHandler, "The client PID %d has %d open references.", inClientPID, aCount );
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
   	fClientPIDListLock->Signal();


	return aCount;
RCF */	
    return 0;
} // UpdateClientPIDRefCount

// ----------------------------------------------------------------------------
//	* CheckClientPIDs() pass through static
//
// ----------------------------------------------------------------------------

void CDSRefTable::CheckClientPIDs ( bool inUseTimeOuts )
{
/*RCF
	if (gFWRefTable != nil)
	{
		gFWRefTable->DoCheckClientPIDs(inUseTimeOuts);
	}
RCF */
} // CheckClientPIDs


// ----------------------------------------------------------------------------
//	* DoCheckClientPIDs()
//
// ----------------------------------------------------------------------------

void CDSRefTable::DoCheckClientPIDs ( bool inUseTimeOuts )
{
/*RCF
	CFStringRef			aPIDString			= 0;
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
		return;
	}

	aNullStringName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), 0);
	
	//retrieve the process table for client PIDs
	// Allocate space for complete process list.
	if ( 0 > std::sysctl( mib, 4, NULL, &ulSize, NULL, 0) )
	{
		return; //ie. do nothing
	}

	iSize = ulSize / sizeof(struct kinfo_proc);
	struct kinfo_proc *kpspArray = new kinfo_proc[ iSize ];
	if (!kpspArray)
	{
		return; //ie. do nothing
	}

	// Get the proc list.
	ulSize = iSize * sizeof(struct kinfo_proc);
	if ( 0 > std::sysctl( mib, 4, kpspArray, &ulSize, NULL, 0 ) )
	{
		delete [] kpspArray;
		return; //ie. do nothing
	}

	//check if any client PID is no longer a process
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
			aPIDValue = atoi(aStringBuff);
			
			//bPIDRunning set to false unless found in loop below
			bPIDRunning = false;

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

			if ( CFDictionaryContainsKey( fClientPIDList, aPIDString ) )
			{
				aPIDDict	= (CFMutableDictionaryRef)CFDictionaryGetValue( fClientPIDList, aPIDString );
				aRefCount	= (CFNumberRef)CFDictionaryGetValue( aPIDDict, CFSTR("RefCount") );
				CFMutableArrayRef aDirRefArray = (CFMutableArrayRef)CFDictionaryGetValue(aPIDDict, CFSTR("DirRefs"));
				
				//don't believe aRefCount is retained so don't release
   				CFNumberGetValue(aRefCount, kCFNumberIntType, &aCount);
				if (bPIDRunning)
				{
					if (gLogFWRefCalls)
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

						siResult = CDSRefTable::RemoveDirRef( aDirRef, aPIDValue );
					}
					fClientPIDListLock->Wait();
					//KW for now output info always
					if (gLogFWRefCalls)
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
		do
		{
			if (kCFCompareEqualTo == CFStringCompare((CFStringRef)CFArrayGetValueAtIndex(clientPIDArray, indexToPID), aNullStringName, 0))
			{
				CFArrayRemoveValueAtIndex(clientPIDArray, indexToPID);
			}
			indexToPID--;
			
		} while (indexToPID > 0); //note index 0 points to this process so no need to check
	}
	
	CFRelease(aNullStringName);
	
	fSunsetTime = time(nil) + 300;

	fClientPIDListLock->Signal();

	delete [] kpspArray;
RCF */
} // DoCheckClientPIDs

//------------------------------------------------------------------------------------
//	* GetOffset
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::GetOffset ( uInt32 inRefNum, uInt32 inType, uInt32* outOffset, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;
	sFWRefEntry	   *pCurrRef	= nil;

	if (gFWRefTable != nil)
	{
        siResult = gFWRefTable->VerifyReference( inRefNum, inType, inPID );
        
        if (siResult == eDSNoErr)
        {
            pCurrRef = gFWRefTable->GetTableRef( inRefNum );
            
            siResult = eDSInvalidReference;
            if ( pCurrRef != nil )
            {
                *outOffset = pCurrRef->fOffset;
                siResult = eDSNoErr;
            }
        }
    }
    
	return( siResult );

} // GetOffset

//------------------------------------------------------------------------------------
//	* SetOffset
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::SetOffset ( uInt32 inRefNum, uInt32 inType, uInt32 inOffset, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;
	sFWRefEntry	   *pCurrRef	= nil;

	if (gFWRefTable != nil)
	{
        siResult = gFWRefTable->VerifyReference( inRefNum, inType, inPID );
        
        if (siResult == eDSNoErr)
        {
            pCurrRef = gFWRefTable->GetTableRef( inRefNum );
            
            siResult = eDSInvalidReference;
            if ( pCurrRef != nil )
            {
                pCurrRef->fOffset = inOffset;
                siResult = eDSNoErr;
            }
        }
    }
    
	return( siResult );

} // SetOffset

//------------------------------------------------------------------------------------
//	* GetBufTag
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::GetBufTag ( uInt32 inRefNum, uInt32 inType, uInt32* outBufTag, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;
	sFWRefEntry	   *pCurrRef	= nil;

	if (gFWRefTable != nil)
	{
        siResult = gFWRefTable->VerifyReference( inRefNum, inType, inPID );
        
        if (siResult == eDSNoErr)
        {
            pCurrRef = gFWRefTable->GetTableRef( inRefNum );
            
            siResult = eDSInvalidReference;
            if ( pCurrRef != nil )
            {
                *outBufTag = pCurrRef->fBufTag;
                siResult = eDSNoErr;
            }
        }
    }
    
	return( siResult );

} // GetBufTag

//------------------------------------------------------------------------------------
//	* SetBufTag
//------------------------------------------------------------------------------------

tDirStatus CDSRefTable::SetBufTag ( uInt32 inRefNum, uInt32 inType, uInt32 inBufTag, sInt32 inPID )
{
	tDirStatus		siResult	= eDSDirSrvcNotOpened;
	sFWRefEntry	   *pCurrRef	= nil;

	if (gFWRefTable != nil)
	{
        siResult = gFWRefTable->VerifyReference( inRefNum, inType, inPID );
        
        if (siResult == eDSNoErr)
        {
            pCurrRef = gFWRefTable->GetTableRef( inRefNum );
            
            siResult = eDSInvalidReference;
            if ( pCurrRef != nil )
            {
                pCurrRef->fBufTag = inBufTag;
                siResult = eDSNoErr;
            }
        }
    }
    
	return( siResult );

} // SetBufTag

