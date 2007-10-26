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
 * @header CContinue
 */

#include "CContinue.h"

#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------------
//	* CContinue
//------------------------------------------------------------------------------------

CContinue::CContinue ( DeallocateProc *inProcPtr ) : fMutex("CContinue::fMutex")
{
	fHashArrayLength = 32;
	fRefNumCount = 0;
	fLookupTable = (sDSTableEntry**)calloc(fHashArrayLength, sizeof(sDSTableEntry*));

	fDeallocProcPtr = inProcPtr;

} // CContinue


CContinue::CContinue ( DeallocateProc *inProcPtr, UInt32 inHashArrayLength ) : fMutex("CContinue::fMutex")
{
	fHashArrayLength = inHashArrayLength;
	fRefNumCount = 0;
	fLookupTable = (sDSTableEntry**)calloc(fHashArrayLength, sizeof(sDSTableEntry*));

	fDeallocProcPtr = inProcPtr;

} // CContinue


//------------------------------------------------------------------------------------
//	* ~CContinue
//------------------------------------------------------------------------------------

CContinue::~CContinue ( void )
{
} // ~CContinue


//------------------------------------------------------------------------------------
//	* AddItem
//
//		- Sets the ref count == 1
//
//------------------------------------------------------------------------------------

SInt32 CContinue::AddItem ( void *inData, UInt32 inRefNum )
{
	SInt32			siResult	= eDSNoErr;
	UInt32			uiSlot		= 0;
	UInt32			uiTmpRef	= 0;
	sDSTableEntry	   *pNewEntry	= nil;
	sDSTableEntry	   *pCurrEntry	= nil;

	fMutex.WaitLock();

	// Change the pointer into a long.
	uiTmpRef = (UInt32)inData;

	// Create the new entry object
	pNewEntry = (sDSTableEntry *)::malloc( sizeof( sDSTableEntry ) );
	if ( pNewEntry != nil )
	{
		::memset( pNewEntry, 0, sizeof( sDSTableEntry ) );
		pNewEntry->fRefNum		= inRefNum;
		pNewEntry->fData		= inData;
	}
	else
	{
		siResult = eMemoryError;
	}

	if ( siResult == eDSNoErr )
	{
		// Calculate where we are going to put this entry
		uiSlot = uiTmpRef % fHashArrayLength;

		if ( fLookupTable[ uiSlot ] == nil )
		{
			// This slot is currently empty so this is the first one
			fLookupTable[ uiSlot ] = pNewEntry;
			fRefNumCount++;
		}
		else
		{
			// Check to see if this item has already been added
			pCurrEntry = fLookupTable[ uiSlot ];
			while ( pCurrEntry != nil )
			{
				if ( pCurrEntry->fData == inData )
				{
					// We found a duplicate.
					siResult = eDSInvalidIndex;
					free( pNewEntry );
					pNewEntry = nil;
					break;
				}
				pCurrEntry = pCurrEntry->fNext;
			}

			if ( siResult == eDSNoErr )
			{
				// This slot is occupied so add it to the head of the list
				pCurrEntry = fLookupTable[ uiSlot ];

				pNewEntry->fNext = pCurrEntry;

				fLookupTable[ uiSlot ] = pNewEntry;
				fRefNumCount++;
			}
		}
	}

	fMutex.SignalLock();

	return( siResult );

} // AddItem


//------------------------------------------------------------------------------------
//	* VerifyItem
//------------------------------------------------------------------------------------

bool CContinue::VerifyItem ( void *inData )
{
	bool			bResult		= false;
	UInt32			uiTmpRef	= 0;
	UInt32			uiSlot		= 0;
	sDSTableEntry	   *pEntry		= nil;

	fMutex.WaitLock();

	// Change the pointer into a long.
	uiTmpRef = (UInt32)inData;

	// Calculate where we thought we put it last
	uiSlot = uiTmpRef % fHashArrayLength;

	// Look across all entries at this position
	pEntry = fLookupTable[ uiSlot ];
	while ( pEntry != nil )
	{
		// Is it the one we want
		if ( pEntry->fData == inData )
		{
			bResult = true;

			break;
		}
		pEntry = pEntry->fNext;
	}

	fMutex.SignalLock();

	// A return of NULL means that we did not find the item
	return( bResult );

} // VerifyItem


//------------------------------------------------------------------------------------
//	* RemoveItem
//
//		- Remove the item.  There could be duplicates, so we must deal with this.
//
//------------------------------------------------------------------------------------

SInt32 CContinue::RemoveItem ( void *inData )
{
	SInt32			siResult	= eDSIndexNotFound;
	UInt32			uiTmpRef	= 0;
	UInt32			uiSlot		= 0;
	sDSTableEntry	   *pCurrEntry	= nil;
	sDSTableEntry	   *pPrevEntry	= nil;

	// nothing to do
	if ( inData == NULL )
		return eDSNoErr;
	
	fMutex.WaitLock();

	// Change the pointer into a long.
	uiTmpRef = (UInt32)inData;

	// Calculate where we thought we put it last
	uiSlot = uiTmpRef % fHashArrayLength;

	// Look across all entries at this position
	pCurrEntry = fLookupTable[ uiSlot ];
	pPrevEntry = fLookupTable[ uiSlot ];
	while ( pCurrEntry != nil )
	{
		// Is it the one we want
		if ( pCurrEntry->fData == inData )
		{
			// Is it the first one in the list
			if ( pCurrEntry == pPrevEntry )
			{
				// Remove the first item from the list
				fLookupTable[ uiSlot ] = pCurrEntry->fNext;
			}
			else
			{
				// Keep the list linked
				pPrevEntry->fNext = pCurrEntry->fNext;
			}

			if ( (fDeallocProcPtr != nil) && (pCurrEntry->fData != nil) )
			{
				fMutex.SignalLock();
				(fDeallocProcPtr)( pCurrEntry->fData );
				fMutex.WaitLock();
			}
			free( pCurrEntry );
			pCurrEntry = nil;
			fRefNumCount--;

			siResult = eDSNoErr;

			break;
		}

		if ( pCurrEntry != nil )
		{
			pPrevEntry = pCurrEntry;
			pCurrEntry = pPrevEntry->fNext;
		}
	}

	fMutex.SignalLock();

	return( siResult );

} // RemoveItem


//------------------------------------------------------------------------------------
//	* RemoveItems
//
//		- Remove the items.  There could be duplicates, so we must deal with this.
//
//------------------------------------------------------------------------------------

SInt32 CContinue::RemoveItems ( UInt32 inRefNum )
{
	bool			bGetNext	= true;
	UInt32			i			= 0;
	SInt32			siResult	= eDSIndexNotFound;
	sDSTableEntry	   *pCurrEntry	= nil;
	sDSTableEntry	   *pPrevEntry	= nil;
	sDSTableEntry	   *pDeadEntry	= nil;

	fMutex.WaitLock();

	for ( i = 0; i < fHashArrayLength; i++ )
	{
		pCurrEntry = fLookupTable[ i ];
		pPrevEntry = fLookupTable[ i ];
		while ( pCurrEntry != nil )
		{
			bGetNext = true;

			// Is it the one we want
			if ( pCurrEntry->fRefNum == inRefNum )
			{
				pDeadEntry = pCurrEntry;

				// Is it the first one in the list
				if ( pCurrEntry == pPrevEntry )
				{
					// Remove the first item from the list
					fLookupTable[ i ] = pCurrEntry->fNext;

					pCurrEntry = fLookupTable[ i ];
					pPrevEntry = fLookupTable[ i ];

					bGetNext = false;
				}
				else
				{
					// Keep the list linked
					pPrevEntry->fNext = pCurrEntry->fNext;

					pCurrEntry = pPrevEntry;
				}

				if ( (fDeallocProcPtr != nil) && (pDeadEntry->fData != nil) )
				{
					fMutex.SignalLock();
					(fDeallocProcPtr)( pDeadEntry->fData );
					fMutex.WaitLock();
				}
				free( pDeadEntry );
				pDeadEntry = nil;
				fRefNumCount--;

				siResult = eDSNoErr;
			}

			if ( pCurrEntry != nil )
			{
				if ( bGetNext == true )
				{
					pPrevEntry = pCurrEntry;
					pCurrEntry = pPrevEntry->fNext;
				}
			}
		}
	}

	fMutex.SignalLock();

	return( siResult );

} // RemoveItems


//------------------------------------------------------------------------------------
//	* GetRefNumForItem
//------------------------------------------------------------------------------------

UInt32 CContinue::GetRefNumForItem ( void *inData )
{
	UInt32			uiResult	= 0;
	UInt32			uiTmpRef	= 0;
	UInt32			uiSlot		= 0;
	sDSTableEntry	   *pEntry		= nil;

	fMutex.WaitLock();

	// Change the pointer into a long.
	uiTmpRef = (UInt32)inData;

	// Calculate where we thought we put it last
	uiSlot = uiTmpRef % fHashArrayLength;

	// Look across all entries at this position
	pEntry = fLookupTable[ uiSlot ];
	while ( pEntry != nil )
	{
		// Is it the one we want
		if ( pEntry->fData == inData )
		{
			uiResult = pEntry->fRefNum;

			break;
		}
		pEntry = pEntry->fNext;
	}

	fMutex.SignalLock();

	// A return of 0 means that we did not find the item
	return( uiResult );

} // GetRefNumForItem
