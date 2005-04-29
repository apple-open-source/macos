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

CContinue::CContinue ( DeallocateProc *inProcPtr )
{
	fHashArrayLength = 32;
	fLookupTable = (sDSTableEntry**)calloc(fHashArrayLength, sizeof(sDSTableEntry*));

	fDeallocProcPtr = inProcPtr;

} // CContinue


CContinue::CContinue ( DeallocateProc *inProcPtr, uInt32 inHashArrayLength )
{
	fHashArrayLength = inHashArrayLength;
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

sInt32 CContinue::AddItem ( void *inData, uInt32 inRefNum )
{
	sInt32			siResult	= eDSNoErr;
	uInt32			uiSlot		= 0;
	uInt32			uiTmpRef	= 0;
	sDSTableEntry	   *pNewEntry	= nil;
	sDSTableEntry	   *pCurrEntry	= nil;

	fMutex.Wait();

	// Change the pointer into a long.
	uiTmpRef = (uInt32)inData;

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
			}
		}
	}

	fMutex.Signal();

	return( siResult );

} // AddItem


//------------------------------------------------------------------------------------
//	* VerifyItem
//------------------------------------------------------------------------------------

bool CContinue::VerifyItem ( void *inData )
{
	bool			bResult		= false;
	uInt32			uiTmpRef	= 0;
	uInt32			uiSlot		= 0;
	sDSTableEntry	   *pEntry		= nil;

	fMutex.Wait();

	// Change the pointer into a long.
	uiTmpRef = (uInt32)inData;

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

	fMutex.Signal();

	// A return of NULL means that we did not find the item
	return( bResult );

} // VerifyItem


//------------------------------------------------------------------------------------
//	* RemoveItem
//
//		- Remove the item.  There could be duplicates, so we must deal with this.
//
//------------------------------------------------------------------------------------

sInt32 CContinue::RemoveItem ( void *inData )
{
	sInt32			siResult	= eDSIndexNotFound;
	uInt32			uiTmpRef	= 0;
	uInt32			uiSlot		= 0;
	sDSTableEntry	   *pCurrEntry	= nil;
	sDSTableEntry	   *pPrevEntry	= nil;

	fMutex.Wait();

	// Change the pointer into a long.
	uiTmpRef = (uInt32)inData;

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
				fMutex.Signal();
				(fDeallocProcPtr)( pCurrEntry->fData );
				fMutex.Wait();
			}
			free( pCurrEntry );
			pCurrEntry = nil;

			siResult = eDSNoErr;

			break;
		}

		if ( pCurrEntry != nil )
		{
			pPrevEntry = pCurrEntry;
			pCurrEntry = pPrevEntry->fNext;
		}
	}

	fMutex.Signal();

	return( siResult );

} // RemoveItem


//------------------------------------------------------------------------------------
//	* RemoveItems
//
//		- Remove the items.  There could be duplicates, so we must deal with this.
//
//------------------------------------------------------------------------------------

sInt32 CContinue::RemoveItems ( uInt32 inRefNum )
{
	bool			bGetNext	= true;
	uInt32			i			= 0;
	sInt32			siResult	= eDSIndexNotFound;
	sDSTableEntry	   *pCurrEntry	= nil;
	sDSTableEntry	   *pPrevEntry	= nil;
	sDSTableEntry	   *pDeadEntry	= nil;

	fMutex.Wait();

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
					fMutex.Signal();
					(fDeallocProcPtr)( pDeadEntry->fData );
					fMutex.Wait();
				}
				free( pDeadEntry );
				pDeadEntry = nil;

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

	fMutex.Signal();

	return( siResult );

} // RemoveItems


//------------------------------------------------------------------------------------
//	* GetRefNumForItem
//------------------------------------------------------------------------------------

uInt32 CContinue::GetRefNumForItem ( void *inData )
{
	uInt32			uiResult	= 0;
	uInt32			uiTmpRef	= 0;
	uInt32			uiSlot		= 0;
	sDSTableEntry	   *pEntry		= nil;

	fMutex.Wait();

	// Change the pointer into a long.
	uiTmpRef = (uInt32)inData;

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

	fMutex.Signal();

	// A return of 0 means that we did not find the item
	return( uiResult );

} // GetRefNumForItem
