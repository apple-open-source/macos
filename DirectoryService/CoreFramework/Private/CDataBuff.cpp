/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CDataBuff
 */

// sys
#include <strings.h>
#include <string.h>
#include <stdlib.h> // for calloc and free

// app
#include "CDataBuff.h"

// ---------------------------------------------------------------------------
//	CDataBuff ()
// ---------------------------------------------------------------------------

CDataBuff::CDataBuff ( uInt32 inSize )
	: fSize( 0 ), fLength( 0 ), fData( nil )
{
	GrowBuff( inSize );
} // CDataBuff


// ---------------------------------------------------------------------------
//	~CDataBuff ()
// ---------------------------------------------------------------------------

CDataBuff::~CDataBuff ( void )
{
	if ( fData != nil )
	{
		::free( fData );
		fData = nil;
	}
} // ~CDataBuff


// ---------------------------------------------------------------------------
//	AppendString ()
// ---------------------------------------------------------------------------

void CDataBuff::AppendString ( const char *inStr )
{
	uInt32	len	= 0;

	if ( inStr != nil )
	{
		len = ::strlen( inStr );
		GrowBuff( fLength + len + 1 );

		::strcpy ( &fData[ fLength ], inStr );
		fLength += len;
	}
} // AppendString


// ---------------------------------------------------------------------------
//	AppendLong ()
// ---------------------------------------------------------------------------

void CDataBuff::AppendLong ( uInt32 inLong )
{
	uInt32	len	= 4;

   	GrowBuff( fLength + len + 1 );

	::memcpy( &fData[ fLength ], &inLong, 4 );

	fLength += len;

} // AppendLong


// ---------------------------------------------------------------------------
//	AppendShort ()
// ---------------------------------------------------------------------------

void CDataBuff::AppendShort ( uInt16 inShort )
{
	uInt32	len	= 2;

   	GrowBuff( fLength + len + 1 );

	::memcpy( &fData[ fLength ], &inShort, 2 );

	fLength += len;

} // AppendShort


// ---------------------------------------------------------------------------
//	AppendBlock ()
// ---------------------------------------------------------------------------

void CDataBuff::AppendBlock ( const void *inData, uInt32 inLength )
{
   	GrowBuff( fLength + inLength + 1 );

	::memcpy( &fData[ fLength ], inData, inLength );

	fLength += inLength;

} // AppendBlock


// ---------------------------------------------------------------------------
//	Clear ()
// ---------------------------------------------------------------------------

void CDataBuff::Clear ( uInt32 inSize )
{
	fLength = 0;

	if ( inSize < fSize )
	{
		::free( fData );
		fData = nil;

		GrowBuff( inSize ); // will calloc
	}
	else
	{
		::memset( fData, 0, fSize );
	}
} // Clear


// ---------------------------------------------------------------------------
//	GetSize ()
// ---------------------------------------------------------------------------

uInt32 CDataBuff::GetSize ( void )
{
	return( fSize );
} // GetSize


// ---------------------------------------------------------------------------
//	GetLength ()
// ---------------------------------------------------------------------------

uInt32 CDataBuff::GetLength ( void )
{
	return( fLength );
} // GetLength


// ---------------------------------------------------------------------------
//	GetData ()
// ---------------------------------------------------------------------------

char* CDataBuff::GetData ( void )
{
	return( fData );
} // GetData


// ---------------------------------------------------------------------------
//	GrowBuff ()
// ---------------------------------------------------------------------------

void CDataBuff::GrowBuff ( uInt32 inNewSize )
{
	uInt32	newSize	= inNewSize;

	// Allocate the default length if requested.
	if ( newSize == 0 )
	{
		newSize = kDefaultSize;
	}

	// Don't bother reallocating if there is already enough room.
	if ( (newSize <= fSize) && (fData != nil) )
	{
		return;
	}

	// Round the requested size to the nearest power of two (> 16).
	//	The comparison is an optimization for the most common case.
	if ( newSize != kDefaultSize )
	{
		register uInt32 pow2 = 16;
		while ( pow2 < newSize )
		{
			pow2 <<= 1;
		}
		newSize = pow2;
	}

	register char *newData = (char *)::calloc( 1, newSize );

	if ( fData != nil )
	{
		if ( fLength != 0 )
		{
			// Copy the old data to the new block
			::memcpy( newData, fData, fLength );
		}

		::free( fData );
		fData = nil;
	}

	fSize = newSize;
	fData = newData;

} // Grow

