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
 * @header CBuff
 */

// sys
#include <stdlib.h>
#include <string.h>

// Class headers
#include "CBuff.h"

//------------------------------------------------------------------------------------
//	* CBuff
//------------------------------------------------------------------------------------

CBuff::CBuff ( void )
{
	fBuff		= nil;
	fOffset		= 0;
	fWhatsLeft	= 0;
	fStatus		= kNotInitalized;
} // CBuff


//------------------------------------------------------------------------------------
//	* ~CBuff
//------------------------------------------------------------------------------------

CBuff::~CBuff ( void )
{
} // ~CBuff


//------------------------------------------------------------------------------------
//	* CBuff
//------------------------------------------------------------------------------------

sInt32 CBuff::Initialize ( tDataBuffer *inBuff, bool inClear )
{
	fBuff = inBuff;

	if ( fBuff == nil )
	{
		fStatus = eDSNullDataBuff;
	}
	else
	{
		// Must be at least 12 bytes big
		if ( fBuff->fBufferSize < 12 )
		{
			fStatus = eDSBufferTooSmall;
		}
		else
		{
			fStatus	= eDSNoErr;
			fOffset = fBuff->fBufferSize;
			fWhatsLeft = fBuff->fBufferSize - 12;

			if ( inClear == true )
			{
				::memset( fBuff->fBufferData, 0, fBuff->fBufferSize );
			}
		}
	}

	return( fStatus );

} // CBuff


//------------------------------------------------------------------------------------
//	* SetBuffLen
//------------------------------------------------------------------------------------

sInt32 CBuff::SetBuffLen ( uInt32 inBuffLen )
{
	sInt32	siResult	= eDSNoErr;

	if ( fBuff != nil )
	{
		fBuff->fBufferLength = inBuffLen;
	}
	else
	{
		siResult = kBuffNull;
	}

	return( siResult );

} // SetBuffLen


//------------------------------------------------------------------------------------
//	* GetBuffer
//------------------------------------------------------------------------------------

tDataBuffer* CBuff::GetBuffer ( void )
{
	return( fBuff );
} // GetBuffer


//------------------------------------------------------------------------------------
//	* GetBuffStatus
//------------------------------------------------------------------------------------

sInt32 CBuff::GetBuffStatus ( void )
{
	return( fStatus );
} // GetBuffStatus


//------------------------------------------------------------------------------------
//	* GetBuffType
//------------------------------------------------------------------------------------

sInt32 CBuff::GetBuffType ( uInt32 *outType )
{
	sInt32	siResult	= eDSNoErr;

	if ( fBuff != nil) 
	{
		if ( fBuff->fBufferSize > 3 )
		{
			::memcpy( outType, fBuff->fBufferData, 4 );
		}
		else
		{
			siResult = kBuffTooSmall;
		}
	}
	else
	{
		siResult = kBuffNull;
	}

	return( siResult );

} // GetBuffType


//------------------------------------------------------------------------------------
//	* SetBuffType
//------------------------------------------------------------------------------------

sInt32 CBuff::SetBuffType ( uInt32 inBuffType )
{
	uInt32		uiTemp	= 0;

	if ( fStatus == eDSNoErr )
	{
		// Set the buffer type
		::memcpy( fBuff->fBufferData, &inBuffType, 4 );

		// Set the data block count to 0
		uiTemp = 0;
		::memcpy( fBuff->fBufferData + 4, &uiTemp, 4 );

		// Set the tag to indicate the end of the offset block
		uiTemp = kEndTag;
		::memcpy( fBuff->fBufferData + 8, &uiTemp, 4 );

		// Set the current length of the data buffer
		this->SetBuffLen( 12 );
	}

	return( fStatus );

} // SetBuffType


//------------------------------------------------------------------------------------
//	* AddData
//------------------------------------------------------------------------------------

sInt32 CBuff::AddData ( char *inData, uInt32 inLen )
{
	sInt32	siResult	= eDSNoErr;

	if ( fBuff != nil) 
	{
		if ( (inLen + 8) < fWhatsLeft )
		{
			// Set the next offset for the length of the incoming data and
			//	4 bytes for it's length
			siResult = SetNextOffset( inLen + 4 );
			if ( siResult == eDSNoErr )
			{
				::memcpy( fBuff->fBufferData + fOffset, &inLen, 4 );
				::memcpy( fBuff->fBufferData + fOffset + 4, inData, inLen );
				fBuff->fBufferLength += (inLen + 4);
			}
		}
		else
		{
			siResult = kBuffFull;
		}
	}
	else
	{
		siResult = kBuffNull;
	}

	return( siResult );

} // AddData


//------------------------------------------------------------------------------------
//	* SetNextOffset
//------------------------------------------------------------------------------------

sInt32 CBuff::SetNextOffset ( uInt32 inLen )
{
	sInt32		siResult	= eDSNoErr;
	uInt32		uiTmp		= 0;
	uInt32		uiCntr		= 8;

	while ( (uiTmp != kEndTag) && (uiCntr < (fOffset - 4)) )
	{
		::memcpy( &uiTmp, fBuff->fBufferData + uiCntr, 4 );
		uiCntr += 4;
	}

	if ( (uiCntr > 11) && (uiCntr + 4 < fOffset) )
	{
		uiCntr -= 4;

		// update the current offset pointer
		fOffset = fOffset - inLen;

		// Set the new data offset
		uiTmp = fOffset;
		::memcpy( fBuff->fBufferData + uiCntr, &uiTmp, 4 );

		// Move the tag to just past the end
		uiTmp = kEndTag;
		::memcpy( fBuff->fBufferData + uiCntr + 4, &uiTmp, 4 );

		// increment the data block counter
		uiTmp = 0;
		::memcpy( &uiTmp, fBuff->fBufferData + 4, 4 );
		uiTmp++;
		::memcpy( fBuff->fBufferData + 4, &uiTmp, 4 );

		fWhatsLeft -= (inLen + 4);
	}
	else
	{
		siResult = kBuffInvalFormat;
	}

	return( siResult );

} // SetNextOffset


//------------------------------------------------------------------------------------
//	* ClearBuff
//------------------------------------------------------------------------------------

void CBuff::ClearBuff ( void )
{
	if ( fBuff != nil )
	{
		::memset( fBuff->fBufferData, 0, fBuff->fBufferSize );
		fBuff->fBufferLength = 0;
		fWhatsLeft = fBuff->fBufferSize - 12;
		fOffset = fBuff->fBufferSize;
	}
} // ClearBuff


//------------------------------------------------------------------------------------
//	* SetLengthToSize
//------------------------------------------------------------------------------------

void CBuff::SetLengthToSize ( void )
{
	if ( fBuff != nil )
	{
		fBuff->fBufferLength = fBuff->fBufferSize;
	}
} // SetLengthToSize


//------------------------------------------------------------------------------------
//	* GetDataBlockCount
//------------------------------------------------------------------------------------

sInt32 CBuff::GetDataBlockCount ( uInt32 *outCount )
{
	sInt32	siResult	= 0;

	if ( fBuff != nil ) 
	{
		if ( fBuff->fBufferSize >= 8 )
		{
			::memcpy( outCount, fBuff->fBufferData + 4, 4 );
		}
		else
		{
			siResult = kBuffTooSmall;
		}
	}
	else
	{
		siResult = kBuffNull;
	}

	return( siResult );

} // GetDataBlockCount



//------------------------------------------------------------------------------------
//	* GetDataBlock
//------------------------------------------------------------------------------------

char* CBuff::GetDataBlock ( uInt32 inIndex, uInt32 *outOffset )
{
	char	   *pResult			= nil;
	uInt32		uiLenOffset		= 0;
	uInt32		uiBlockOffset	= 0;
	uInt32		uiBlockLen		= 0;

	*outOffset = 0;

	if ( (fStatus == eDSNoErr) && (inIndex != 0) ) 
	{
		uiLenOffset = (inIndex * 4) + 4;
		if ( (uiLenOffset + 4) <= fBuff->fBufferSize )
		{
			::memcpy( &uiBlockOffset, fBuff->fBufferData + uiLenOffset, 4 );
			if ( (uiBlockOffset + 4) <= fBuff->fBufferSize )
			{
				pResult = fBuff->fBufferData + uiBlockOffset;

				// Get the length of this data block
				::memcpy( &uiBlockLen, pResult, 4 );

				if ( (uiBlockOffset + uiBlockLen) > fBuff->fBufferSize )
				{
					pResult = nil;
				}
				else
				{
					*outOffset = uiBlockOffset;
				}
			}
		}
	}

	return( pResult );

} // GetDataBlock


//------------------------------------------------------------------------------------
//	* GetDataBlockLength
//------------------------------------------------------------------------------------

uInt32 CBuff::GetDataBlockLength ( uInt32 inIndex )
{
	uInt32		uiResult		= 0;
	uInt32		uiLenOffset		= 0;
	uInt32		uiBlockOffset	= 0;

	if ( (fStatus == eDSNoErr) && (inIndex != 0) ) 
	{
		uiLenOffset = (inIndex * 4) + 4;
		if ( (uiLenOffset + 4) <= fBuff->fBufferSize )
		{
			::memcpy( &uiBlockOffset, fBuff->fBufferData + uiLenOffset, 4 );
			if ( (uiBlockOffset + 4) <= fBuff->fBufferSize )
			{
				// Get the length of this data block
				::memcpy( &uiResult, fBuff->fBufferData + uiBlockOffset, 4 );

				if ( (uiBlockOffset + uiResult) > fBuff->fBufferSize )
				{
					uiResult = 0;
				}
			}
		}
	}

	return( uiResult );

} // GetDataBlock

