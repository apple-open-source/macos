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
 * @header DirServicesPriv
 */

// app
#include "DirServicesPriv.h"
#include "DirServicesUtils.h"
#include "DirServicesTypesPriv.h"
#include "PrivateTypes.h"
#include <stdlib.h>
#include <string.h>
#include "CRCCalc.h"

extern sInt32 gProcessPID;

static const	uInt32	kFWBuffPad	= 16;

//--------------------------------------------------------------------------------------------------
//
//	Name:	VerifyTDataBuff
//
//--------------------------------------------------------------------------------------------------

tDirStatus VerifyTDataBuff ( tDataBuffer *inBuff, tDirStatus inNullErr, tDirStatus inEmptyErr )
{
	if ( inBuff == nil )
	{
		return( inNullErr );
	}
	if ( inBuff->fBufferSize == 0 )
	{
		return( inEmptyErr );
	}

	return( eDSNoErr );

} // VerifyTDataBuff


//--------------------------------------------------------------------------------------------------
//
//	Name:	VerifyTNodeList
//
//--------------------------------------------------------------------------------------------------

tDirStatus VerifyTNodeList ( tDataList *inDataList, tDirStatus inNullErr, tDirStatus inEmptyErr )
{
	if ( inDataList == nil )
	{
		return( inNullErr );
	}
	if ( inDataList->fDataNodeCount == 0 )
	{
		return( inEmptyErr );
	}
	if ( dsGetDataLength( inDataList ) == 0 )
	{
		return( inEmptyErr );
	}

	return( eDSNoErr );

} // VerifyTNodeList


//------------------------------------------------------------------------------------
//	Name: IsStdBuffer
//------------------------------------------------------------------------------------

tDirStatus IsStdBuffer ( tDataBufferPtr inOutDataBuff )

{
	tDirStatus	outResult	= eDSNoErr;
	sInt32		siResult	= eDSNoErr;
	CBuff		inBuff;
    uInt32		bufTag		= 0;

    //check to determine whether we employ client side buffer parsing
    //ie. look for 'StdA' OR 'StdB' tags

	try
	{
		if ( inOutDataBuff == nil ) throw( (sInt32)eDSEmptyBuffer );
		if (inOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		siResult = inBuff.Initialize( inOutDataBuff );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inBuff.GetBuffType( &bufTag );
		if ( siResult != eDSNoErr ) throw( siResult );
        
        if ( (bufTag != 'StdA') && (bufTag != 'StdB') )  //KW should make 'StdB' and 'StdA" readily available constants
        {
            outResult = eDSInvalidTag;
        }

    }
    
	catch( sInt32 err )
	{
		outResult = (tDirStatus)err;
	}

	return( outResult );
    
} // IsStdBuffer

//------------------------------------------------------------------------------------
//	Name: IsFWReference
//------------------------------------------------------------------------------------

tDirStatus IsFWReference ( uInt32 inRef )

{
	tDirStatus	outResult	= eDSInvalidReference;
	
    //check to determine whether this is a FW reference
	//ie. 0x0030000 bits are set

	if ((inRef & 0x00300000) != 0)
	{
		outResult = eDSNoErr;
	}

	return( outResult );
    
} // IsFWReference

//------------------------------------------------------------------------------------
//	Name: ExtractRecordEntry
//------------------------------------------------------------------------------------

tDirStatus ExtractRecordEntry ( tDataBufferPtr		inOutDataBuff,
								unsigned long		inRecordEntryIndex,
								tAttributeListRef	*outAttributeListRef,
								tRecordEntryPtr		*outRecEntryPtr )

{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiIndex			= 0;
	uInt32					uiCount			= 0;
	uInt32					uiOffset		= 0;
	uInt32					uberOffset		= 0;
	char 				   *pData			= nil;
	tRecordEntryPtr			pRecEntry		= nil;
	CBuff					inBuff;
	uInt32					offset			= 0;
	uInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	uInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	uInt16					usAttrCnt		= 0;
	uInt32					buffLen			= 0;
    uInt32					bufTag			= 0;

	try
	{
		if ( inOutDataBuff == nil ) throw( (sInt32)eDSNullDataBuff );
		if (inOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		siResult = inBuff.Initialize( inOutDataBuff );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inBuff.GetBuffType( &bufTag );
		if ( siResult != eDSNoErr ) throw( siResult );
        
		siResult = inBuff.GetDataBlockCount( &uiCount );
		if ( siResult != eDSNoErr ) throw( siResult );

		uiIndex = inRecordEntryIndex;
		if ((uiIndex > uiCount) || (uiIndex == 0)) throw( (sInt32)eDSInvalidIndex );

		pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
		if ( pData == nil ) throw( (sInt32)eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past this same record length obtained from GetDataBlockLength
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record type
		::memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record name
		::memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute count
		::memcpy( &usAttrCnt, pData, 2 );

		pRecEntry = (tRecordEntry *)::calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kFWBuffPad );

		pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kFWBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

		// Add the record name length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
		uiOffset += 2;

		// Add the record name
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
		uiOffset += usNameLen;

		// Add the record type length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

		// Add the record type
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

		pRecEntry->fRecordAttributeCount = usAttrCnt;

        //create a reference here
        siResult = CDSRefTable::NewAttrListRef( outAttributeListRef, 0, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );
		        
		//uberOffset + offset + 4;	// context used by next calls of GetAttributeEntry
									// include the four bytes of the buffLen
		siResult = CDSRefTable::SetOffset( *outAttributeListRef, eAttrListRefType, uberOffset + offset + 4, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );
		siResult = CDSRefTable::SetBufTag( *outAttributeListRef, eAttrListRefType, bufTag, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outRecEntryPtr = pRecEntry;
        pRecEntry = nil;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}
    
    //clean up pRecEntry if throwing an error
    if (pRecEntry != nil)
    {
        dsDeallocRecordEntry(0,pRecEntry);
        pRecEntry = nil;
    }

	return( (tDirStatus)siResult );

} // ExtractRecordEntry


//------------------------------------------------------------------------------------
//	Name: ExtractAttributeEntry
//------------------------------------------------------------------------------------

tDirStatus ExtractAttributeEntry (	tDataBufferPtr			inOutDataBuff,
									tAttributeListRef		inAttrListRef,
									unsigned long			inAttrInfoIndex,
									tAttributeValueListRef	*outAttrValueListRef,
									tAttributeEntryPtr		*outAttrInfoPtr )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt16					usAttrLen16			= 0;
	uInt32					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt16					usValueLen16		= 0;
	uInt32					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 0;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tAttributeEntryPtr		pAttribInfo			= nil;
	uInt32					attrListOffset		= 0;
	uInt32					bufTag				= 0;

	try
	{
	
		siResult = CDSRefTable::GetOffset( inAttrListRef, eAttrListRefType, &attrListOffset, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = CDSRefTable::GetBufTag( inAttrListRef, eAttrListRefType, &bufTag, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );

		uiIndex = inAttrInfoIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
				
		if ( inOutDataBuff == nil ) throw( (sInt32)eDSNullDataBuff );
		if (inOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		buffSize	= inOutDataBuff->fBufferSize;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= inOutDataBuff->fBufferData + attrListOffset;
		offset	= attrListOffset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (sInt32)eDSInvalidIndex );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		if (bufTag == 'StdB')
		{
			// Skip to the attribute that we want
			for ( i = 1; i < uiIndex; i++ )
			{
				// Do record check, verify that offset is not past end of buffer, etc.
				if (2 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
			
				// Get the length for the attribute
				::memcpy( &usAttrLen16, p, 2 );
	
				// Move the offset past the length word and the length of the data
				p		+= 2 + usAttrLen16;
				offset	+= 2 + usAttrLen16;
			}
	
			// Get the attribute offset
			uiOffset = offset;
	
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
			
			// Get the length for the attribute block
			::memcpy( &usAttrLen16, p, 2 );
	
			// Skip past the attribute length
			p		+= 2;
			offset	+= 2;
			
			usAttrLen = (uInt32)usAttrLen16;
		}
		else
		{
			// Skip to the attribute that we want
			for ( i = 1; i < uiIndex; i++ )
			{
				// Do record check, verify that offset is not past end of buffer, etc.
				if (4 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
			
				// Get the length for the attribute
				::memcpy( &usAttrLen, p, 4 );
	
				// Move the offset past the length word and the length of the data
				p		+= 4 + usAttrLen;
				offset	+= 4 + usAttrLen;
			}
	
			// Get the attribute offset
			uiOffset = offset;
	
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
			
			// Get the length for the attribute block
			::memcpy( &usAttrLen, p, 4 );
	
			// Skip past the attribute length
			p		+= 4;
			offset	+= 4;
		}

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
			
		if (bufTag == 'StdB')
		{
			for ( i = 0; i < usValueCnt; i++ )
			{
				// Do record check, verify that offset is not past end of buffer, etc.
				if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
			
				// Get the length for the value
				::memcpy( &usValueLen16, p, 2 );
				
				p		+= 2 + usValueLen16;
				offset	+= 2 + usValueLen16;
				
				uiTotalValueSize += usValueLen16;
			}
		}
		else
		{
			for ( i = 0; i < usValueCnt; i++ )
			{
				// Do record check, verify that offset is not past end of buffer, etc.
				if (4 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
			
				// Get the length for the value
				::memcpy( &usValueLen, p, 4 );
				
				p		+= 4 + usValueLen;
				offset	+= 4 + usValueLen;
				
				uiTotalValueSize += usValueLen;
			}
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kFWBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kFWBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

        //create a reference here
        siResult = CDSRefTable::NewAttrValueRef( outAttrValueListRef, 0, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );
		        
		siResult = CDSRefTable::SetOffset( *outAttrValueListRef, eAttrValueListRefType, uiOffset, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );
		siResult = CDSRefTable::SetBufTag( *outAttrValueListRef, eAttrValueListRefType, bufTag, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outAttrInfoPtr = pAttribInfo;
		pAttribInfo = nil;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

    //clean up pRecEntry if throwing an error
    if (pAttribInfo != nil)
    {
        dsDeallocAttributeEntry(0,pAttribInfo);
        pAttribInfo = nil;
    }

	return( (tDirStatus)siResult );

} // ExtractAttributeEntry


//------------------------------------------------------------------------------------
//	Name: ExtractAttributeValue
//------------------------------------------------------------------------------------

tDirStatus ExtractAttributeValue (	tDataBufferPtr			 inOutDataBuff,
									tAttributeValueListRef	 inAttrValueListRef,
									unsigned long			 inAttrValueIndex,
									tAttributeValueEntryPtr	*outAttrValue )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt16						usValueLen16	= 0;
	uInt32						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt16						attrLen16		= 0;
	uInt32						attrLen			= 0;
	uInt32						attrValueOffset	= 0;
	uInt32						bufTag			= 0;

	try
	{
		siResult = CDSRefTable::GetOffset( inAttrValueListRef, eAttrValueListRefType, &attrValueOffset, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = CDSRefTable::GetBufTag( inAttrValueListRef, eAttrValueListRefType, &bufTag, gProcessPID );
		if ( siResult != eDSNoErr ) throw( siResult );

		uiIndex = inAttrValueIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );

		if ( inOutDataBuff == nil ) throw( (sInt32)eDSNullDataBuff );
		if (inOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		buffSize	= inOutDataBuff->fBufferSize;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= inOutDataBuff->fBufferData + attrValueOffset;
		offset	= attrValueOffset;

		if (bufTag == 'StdB')
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
					
			// Get the buffer length
			::memcpy( &attrLen16, p, 2 );
	
			//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
			//AND add the length of the buffer length var as stored ie. 2 bytes
			buffLen		= attrLen16 + attrValueOffset + 2;
			if (buffLen > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
	
			// Skip past the attribute length
			p		+= 2;
			offset	+= 2;
		}
		else
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
					
			// Get the buffer length
			::memcpy( &attrLen, p, 4 );
	
			//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
			//AND add the length of the buffer length var as stored ie. 4 bytes
			buffLen		= attrLen + attrValueOffset + 4;
			if (buffLen > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
	
			// Skip past the attribute length
			p		+= 4;
			offset	+= 4;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)  throw( (sInt32)eDSInvalidIndex );

		if (bufTag == 'StdB')
		{
			// Skip to the value that we want
			for ( i = 1; i < uiIndex; i++ )
			{
				// Do record check, verify that offset is not past end of buffer, etc.
				if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
			
				// Get the length for the value
				::memcpy( &usValueLen16, p, 2 );
				
				p		+= 2 + usValueLen16;
				offset	+= 2 + usValueLen16;
			}
	
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
			
			::memcpy( &usValueLen16, p, 2 );
			
			p		+= 2;
			offset	+= 2;
			
			usValueLen = (uInt32)usValueLen16;
		}
		else
		{
			// Skip to the value that we want
			for ( i = 1; i < uiIndex; i++ )
			{
				// Do record check, verify that offset is not past end of buffer, etc.
				if (4 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
			
				// Get the length for the value
				::memcpy( &usValueLen, p, 4 );
				
				p		+= 4 + usValueLen;
				offset	+= 4 + usValueLen;
			}
	
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
			
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4;
			offset	+= 4;
		}

		//if (usValueLen == 0)  throw( (sInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kFWBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kFWBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( usValueLen + offset > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		*outAttrValue = pAttrValue;
		pAttrValue = nil;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

    //clean up pRecEntry if throwing an error
    if (pAttrValue != nil)
    {
        dsDeallocAttributeValueEntry(0,pAttrValue);
        pAttrValue = nil;
    }

	return( (tDirStatus)siResult );

} // ExtractAttributeValue

// ---------------------------------------------------------------------------
//	* CalcCRC
// ---------------------------------------------------------------------------

uInt32 CalcCRC ( const char *inStr )
{
	const char 	   *p			= inStr;
	sInt32			siI			= 0;
	sInt32			siStrLen	= 0;
	uInt32			uiCRC		= 0xFFFFFFFF;
	CRCCalc			aCRCCalc;

	if ( inStr != nil )
	{
		siStrLen = ::strlen( inStr );

		for ( siI = 0; siI < siStrLen; ++siI )
		{
			uiCRC = aCRCCalc.UPDC32( *p, uiCRC );
			p++;
		}
	}

	return( uiCRC );

} // CalcCRC


//------------------------------------------------------------------------------------
//	Name: IsNodePathStrBuffer
//------------------------------------------------------------------------------------

tDirStatus IsNodePathStrBuffer ( tDataBufferPtr inOutDataBuff )

{
	tDirStatus	outResult	= eDSNoErr;
	sInt32		siResult	= eDSNoErr;
	CBuff		inBuff;
    uInt32		bufTag		= 0;

    //check to determine whether we can use client side buffer parsing of node path strings
    //ie. look for 'npss' tag

	try
	{
		if ( inOutDataBuff == nil ) throw( (sInt32)eDSEmptyBuffer );
		if (inOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		siResult = inBuff.Initialize( inOutDataBuff );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inBuff.GetBuffType( &bufTag );
		if ( siResult != eDSNoErr ) throw( siResult );
        
        if (bufTag != 'npss')  //KW should make 'npss' a private available constant
        {
            outResult = eDSInvalidTag;
        }

    }
    
	catch( sInt32 err )
	{
		outResult = (tDirStatus)err;
	}

	return( outResult );
    
} // IsNodePathStrBuffer


//------------------------------------------------------------------------------------
//	Name: ExtractDirNodeName
//------------------------------------------------------------------------------------

tDirStatus ExtractDirNodeName (	tDataBufferPtr	inOutDataBuff,
								unsigned long	inDirNodeIndex,
								tDataListPtr   *outDataList )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueLen		= 0;
	uInt32						iSegment		= 0;
	uInt32						uiIndex			= 0;
	uInt32						uiCount			= 0;
	uInt32						offset			= 0;
	char					   *p				= nil;
	uInt32						buffSize		= 0;
	char					   *outNodePathStr	= nil;
	uInt16						segmentCount	= 0;

	try
	{
		//buffer format ie. only used by FW in dsGetDirNodeName
		//4 byte tag
		//4 byte count of node paths
		//repeated:
		// 2 byte count of string segments - ttt
		// sub repeated:
		// 2 byte segment string length - ttt
		// actual segment string - ttt
		// ..... blank space
		// offsets for each node path in reverse order from end of buffer
		// note that buffer length is length of data aboved tagged with ttt
		uiIndex = inDirNodeIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );

		if ( inOutDataBuff == nil ) throw( (sInt32)eDSNullDataBuff );
		if (inOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );
		
		if ( outDataList == nil ) throw( (sInt32)eDSNullDataList );

		//validate count and tag together are not past end of buffer
		if (inOutDataBuff->fBufferSize < 8) throw( (sInt32)eDSInvalidBuffFormat );

		buffSize	= inOutDataBuff->fBufferSize;

		p			= inOutDataBuff->fBufferData + 4; //already know tag is correct
		offset		= 4;

		// Get the buffer node path string count
		::memcpy( &uiCount, p, 4 );

		//validate count contains number
		if (uiCount == 0) throw( (sInt32)eDSEmptyBuffer );

		//validate that index requested is in this buffer
		if (uiIndex > uiCount)  throw( (sInt32)eDSInvalidIndex );
		
		//retrieve the offset into the data for this index
		::memcpy(&offset,inOutDataBuff->fBufferData + buffSize - (uiIndex * 4) , 4);
		p = inOutDataBuff->fBufferData + offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
		
		//retrieve number of segments in node path string
		::memcpy( &segmentCount, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		//build the tDataList here
		*outDataList = dsDataListAllocate(0x00F0F0F0);

		//retrieve each segment and add it to the data list
		for (iSegment = 1; iSegment <= segmentCount; iSegment++)
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if ( 2 + offset > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );
		
			//retrieve the segment's length
			::memcpy( &usValueLen, p, 2 );
		
			p		+= 2;
			offset	+= 2;

			// Do record check, verify that offset is not past end of buffer, etc.
			if ( usValueLen > (sInt32)(buffSize - offset) ) throw( (sInt32)eDSInvalidBuffFormat );
		
			outNodePathStr = (char *) calloc( 1, usValueLen + 1);
			::memcpy( outNodePathStr, p, usValueLen );
	
			//add to the tDataList here
			dsAppendStringToListAlloc( 0x00F0F0F0, *outDataList, outNodePathStr );
			
			free(outNodePathStr);
			outNodePathStr = nil;

			p		+= usValueLen;
			offset	+= usValueLen;

		}
		
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( (tDirStatus)siResult );

} // ExtractDirNodeName

//------------------------------------------------------------------------------------
//	Name: IsRemoteReferenceMap
//------------------------------------------------------------------------------------

tDirStatus IsRemoteReferenceMap ( uInt32 inRef )

{
	tDirStatus	outResult	= eDSInvalidReference;
	
    //check to determine whether this is a remote reference map
	//ie. 0x00C0000 bits are set

	if ((inRef & 0x00C00000) != 0)
	{
		outResult = eDSNoErr;
	}

	return( outResult );
    
} // IsRemoteReferenceMap


