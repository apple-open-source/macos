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
 * @header CPlugInRef
 */

#include "CPlugInRef.h"

#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------------
//	* CPlugInRef
//------------------------------------------------------------------------------------

CPlugInRef::CPlugInRef ( DeallocateProc *inProcPtr )
{
	fHashArrayLength = 128;
	fLookupTable = (sDSTableEntry**)calloc(fHashArrayLength, sizeof(sDSTableEntry*));

	fDeallocProcPtr = inProcPtr;

} // CPlugInRef


CPlugInRef::CPlugInRef ( DeallocateProc *inProcPtr, uInt32 inHashArrayLength )
{
	fHashArrayLength = inHashArrayLength;
	fLookupTable = (sDSTableEntry**)calloc(fHashArrayLength, sizeof(sDSTableEntry*));

	fDeallocProcPtr = inProcPtr;

} // CPlugInRef


//------------------------------------------------------------------------------------
//	* ~CPlugInRef
//------------------------------------------------------------------------------------

CPlugInRef::~CPlugInRef ( void )
{
} // ~CPlugInRef


//------------------------------------------------------------------------------------
//	* AddItem
//------------------------------------------------------------------------------------

sInt32 CPlugInRef::AddItem ( uInt32 inRefNum, void *inData )
{
	sInt32			siResult	= eDSNoErr;
	uInt32			uiSlot		= 0;
	sDSTableEntry	   *pNewEntry	= nil;
	sDSTableEntry	   *pCurrEntry	= nil;

	fMutex.Wait();

	// Create the new entry object
	pNewEntry = (sDSTableEntry *)::calloc( 1, sizeof( sDSTableEntry ) );
	if ( pNewEntry != nil )
	{
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
		uiSlot = inRefNum % fHashArrayLength;

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
				if ( pCurrEntry->fRefNum == inRefNum )
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
//	* GetItem
//------------------------------------------------------------------------------------

void *CPlugInRef::GetItemData ( uInt32 inRefNum )
{
	void		   *pvResult	= nil;
	uInt32			uiSlot		= 0;
	sDSTableEntry	   *pEntry		= nil;

	fMutex.Wait();

	// Calculate where we thought we put it last
	uiSlot = inRefNum % fHashArrayLength;

	// Look across all entries at this position
	pEntry = fLookupTable[ uiSlot ];
	while ( pEntry != nil )
	{
		// Is it the one we want
		if ( pEntry->fRefNum == inRefNum )
		{
			// Get the data
			pvResult = pEntry->fData;

			break;
		}
		pEntry = pEntry->fNext;
	}

	fMutex.Signal();

	// A return of NULL meas that we did not find the item
	return( pvResult );

} // GetItem


//------------------------------------------------------------------------------------
//	* RemoveItem
//
//		- Remove the item.  There could be duplicates, so we must deal with this.
//
//------------------------------------------------------------------------------------

sInt32 CPlugInRef::RemoveItem ( uInt32 inRefNum )
{
	sInt32			siResult	= eDSIndexNotFound;
	uInt32			uiSlot		= 0;
	sDSTableEntry	   *pCurrEntry	= nil;
	sDSTableEntry	   *pPrevEntry	= nil;

	fMutex.Wait();

	// Calculate where we thought we put it last
	uiSlot = inRefNum % fHashArrayLength;

	// Look across all entries at this position
	pCurrEntry = fLookupTable[ uiSlot ];
	pPrevEntry = fLookupTable[ uiSlot ];
	while ( pCurrEntry != nil )
	{
		// Is it the one we want
		if ( pCurrEntry->fRefNum == inRefNum )
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
//	* DoOnAllItems
//------------------------------------------------------------------------------------

void CPlugInRef:: DoOnAllItems ( OperationProc *inProcPtr )
{
	uInt32			uiSlot		= 0;
	sDSTableEntry	   *pEntry		= nil;

	if (inProcPtr == nil)
	{
		return;
	}
	fMutex.Wait();

	for (uiSlot = 0; uiSlot < fHashArrayLength; uiSlot++)
	{
		// Look across all entries at this position
		pEntry = fLookupTable[ uiSlot ];
		while ( pEntry != nil )
		{
			if ( pEntry->fData != nil )
			{
				(inProcPtr)(pEntry->fData);
			}
			pEntry = pEntry->fNext;
		}
	}

	fMutex.Signal();

} // DoOnAllItems

