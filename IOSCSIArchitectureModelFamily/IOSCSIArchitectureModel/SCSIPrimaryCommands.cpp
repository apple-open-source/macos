/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#include <libkern/OSByteOrder.h>

// SCSI Command set related headers
#include "SCSIPrimaryCommands.h"
#include "SCSICommandOperationCodes.h"

#define DEBUG			0
#define DEBUG_LEVEL		0	// Eventually we'll use this to limit
							// the amount of debugging info we see

#if ( DEBUG == 1 )
#define STATUS_LOG( x )		IOLog x
#define DEBUG_ASSERT( x )	assert x
#else
#define STATUS_LOG( x )
#define DEBUG_ASSERT( x )
#endif

#define super OSObject
OSDefineMetaClassAndStructors ( SCSIPrimaryCommands, OSObject );


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::CreateSCSIPrimaryCommandObject
//
//----------------------------------------------------------------------
//
//		get an instance of the command builder
//
//----------------------------------------------------------------------

SCSIPrimaryCommands *
SCSIPrimaryCommands::CreateSCSIPrimaryCommandObject ( void )
{
	return new SCSIPrimaryCommands;
}


#pragma mark Utility Methods

// Utility routines used by all SCSI Command Set objects


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::IsParameterValid
//
//----------------------------------------------------------------------
//
//		Validate Parameter used for 1 bit to 1 byte paramaters
//
//----------------------------------------------------------------------

inline bool
SCSIPrimaryCommands::IsParameterValid ( SCSICmdField1Byte param,
										SCSICmdField1Byte mask )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::IsParameterValid called\n" ) );
	
	if ( ( param | mask ) != mask )
	{
		
		STATUS_LOG ( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
		
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::IsParameterValid
//
//----------------------------------------------------------------------
//
//		Validate Parameter used for 9 bit to 2 byte paramaters
//
//----------------------------------------------------------------------

inline bool
SCSIPrimaryCommands::IsParameterValid ( SCSICmdField2Byte param,
										SCSICmdField2Byte mask )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::IsParameterValid called\n" ) );
	
	if ( ( param | mask ) != mask )
	{
		
		STATUS_LOG ( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
		
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::IsParameterValid
//
//----------------------------------------------------------------------
//
//		Validate Parameter used for 17 bit to 4 byte paramaters
//
//----------------------------------------------------------------------

inline bool
SCSIPrimaryCommands::IsParameterValid ( SCSICmdField4Byte param,
										SCSICmdField4Byte mask )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::IsParameterValid called\n" ) );
	
	if ( ( param | mask ) != mask )
	{
		
		STATUS_LOG ( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
		
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::IsBufferAndCapacityValid
//
//----------------------------------------------------------------------
//
//		Check that the buffer is valid and of the required size
//
//----------------------------------------------------------------------

inline bool
SCSIPrimaryCommands::IsBufferAndCapacityValid (
				IOMemoryDescriptor *		dataBuffer,
				UInt32						requiredSize )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::IsBufferAndCapacityValid called\n" ) );
	
	if ( dataBuffer == NULL )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid\n", dataBuffer ) );
		return false;
		
	}
	
	if ( dataBuffer->getLength ( ) < requiredSize )
	{
		
		STATUS_LOG ( ( "dataBuffer length = %x not valid, requiredSize = %x\n",
						dataBuffer->getLength ( ), requiredSize ) );
		
		return false;
		
	}
	
	return true;
	
}


// ---- Methods for accessing the SCSITask attributes ----
// The setCommandDescriptorBlock methods will populate the CDB of the
// appropriate size.  These methods will returns true if the CDB could 
// be filled out, false if it couldn't.

//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//----------------------------------------------------------------------
//
//		Populate the 6 Byte Command Descriptor Block
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5 )
{
	
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5 );
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//----------------------------------------------------------------------
//
//		Populate the 10 Byte Command Descriptor Block
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9 )
{
	
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5,
					cdbByte6,
					cdbByte7,
					cdbByte8,
					cdbByte9 );
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//----------------------------------------------------------------------
//
//		Populate the 12 Byte Command Descriptor Block
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9,
							UInt8			cdbByte10,
							UInt8			cdbByte11 )
{
	
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5,
					cdbByte6,
					cdbByte7,
					cdbByte8,
					cdbByte9,
					cdbByte10,
					cdbByte11 );
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SetCommandDescriptorBlock
//
//----------------------------------------------------------------------
//
//		Populate the 16 Byte Command Descriptor Block
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SetCommandDescriptorBlock ( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9,
							UInt8			cdbByte10,
							UInt8			cdbByte11,
							UInt8			cdbByte12,
							UInt8			cdbByte13,
							UInt8			cdbByte14,
							UInt8			cdbByte15 )
{
	
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return request->SetCommandDescriptorBlock (
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5,
					cdbByte6,
					cdbByte7,
					cdbByte8,
					cdbByte9,
					cdbByte10,
					cdbByte11,
					cdbByte12,
					cdbByte13,
					cdbByte14,
					cdbByte15 );
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SetDataTransferControl
//
//----------------------------------------------------------------------
//
//		Set up the control information for the transfer, including
//		the transfer direction and the number of bytes to transfer.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SetDataTransferControl ( 
							SCSITask *				request,
						    UInt32					timeoutDuration,
							UInt8					dataTransferDirection,
							IOMemoryDescriptor *	dataBuffer = NULL,
							UInt64					transferCountInBytes = 0 )
{
	
	bool	result = false;
		
	STATUS_LOG ( ( "SCSIPrimaryCommands::SetDataTransferControl called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Needs to do more extensive checking based on buffer count and control values
	if ( ( transferCountInBytes != 0 ) && ( dataBuffer == NULL ) )
	{
		
		STATUS_LOG ( ( "transferCountInBytes = %x not valid, dataBuffer = %x\n",
						transferCountInBytes, dataBuffer ) );
		return result;
		
	}
	
	result = request->SetTimeoutDuration ( timeoutDuration );
	if ( result == false )
	{
		
		STATUS_LOG ( ( "SetTimeoutDuration failed, timeoutDuration = %x\n",
						timeoutDuration ) );
		return result;
		
	}
	
	result = request->SetDataTransferDirection ( dataTransferDirection );
	if ( result == false )
	{
		
		STATUS_LOG ( ( "SetDataTransferDirection failed, dataTransferDirection = %x\n",
						dataTransferDirection ) );
		return result;
		
	}
	
	result = request->SetDataBuffer ( dataBuffer );
	if ( result == false )
	{
		
		STATUS_LOG ( ( "SetDataBuffer failed, dataBuffer = %x\n",
						dataBuffer ) );
		return result;
		
	}
	
	result = request->SetRequestedDataTransferCount ( transferCountInBytes );
	if ( result == false )
	{
		
		STATUS_LOG ( ( "SetRequestedDataTransferCount failed, transferCountInBytes = %x\n",
						transferCountInBytes ) );
		return result;
		
	}
	
	return true;
	
}


#pragma mark -
#pragma mark SPC Methods

// SCSI Primary Commands as defined in T10:1236D SPC-2,
// Revision 18, dated 21 May 2000

//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::CHANGE_DEFINITION
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The CHANGE_DEFINITION command as defined in SPC
//		revision 11a, section 7.1.  SPC-2 obsoleted this command.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::CHANGE_DEFINITION (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			SAVE,
    			SCSICmdField7Bit 			DEFINITION_PARAMETER,
    			SCSICmdField1Byte 			PARAMETER_DATA_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::CHANGE_DEFINITION *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( SAVE, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "SAVE = %x not valid\n", SAVE ) );
		return false;
	
	}

	if ( IsParameterValid ( DEFINITION_PARAMETER, kSCSICmdFieldMask7Bit ) == false )
	{
		
		STATUS_LOG ( ( "DEFINITION_PARAMETER = %x not valid\n", DEFINITION_PARAMETER ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PARAMETER_DATA_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_DATA_LENGTH = %x not valid\n", PARAMETER_DATA_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_DATA_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_DATA_LENGTH = %x\n",
						dataBuffer, PARAMETER_DATA_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_CHANGE_DEFINITION,
								0x00,
								SAVE,
								DEFINITION_PARAMETER,
								0x00,
								0x00,
								0x00,
								0x00,
								PARAMETER_DATA_LENGTH,
								CONTROL );

	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_DATA_LENGTH );
	
    return true;
    
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::COMPARE
//
//----------------------------------------------------------------------
//
//		The COMPARE command as defined in section 7.2.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::COMPARE (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PAD,
    			SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::COMPARE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PAD, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PAD = %x not valid\n", PAD ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n", PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_COMPARE,
								PAD,
								0x00,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
	                   			0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
    
    return true;
    
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::COPY
//
//----------------------------------------------------------------------
//
//		The COPY command as defined in section 7.3.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::COPY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PAD,
    			SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::COPY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PAD, kSCSICmdFieldMask1Bit ) == false )
	{

		STATUS_LOG ( ( "PAD = %x not valid\n", PAD ) );
		return false;

	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n", PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{

		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );

		return false;

	}

	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
 								kSCSICmd_COPY,
								PAD,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
	                   			0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
    return true;
    
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::COPY_AND_VERIFY
//
//----------------------------------------------------------------------
//
//		The COPY_AND_VERIFY command as defined in section 7.4.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::COPY_AND_VERIFY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			BYTCHK,
    			SCSICmdField1Bit 			PAD,
    			SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::COPY_AND_VERIFY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );

	if ( IsParameterValid ( BYTCHK, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "BYTCHK = %x not valid\n", BYTCHK ) );
		return false;
		
	}

	if ( IsParameterValid ( PAD, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PAD = %x not valid\n", PAD ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n", PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
	
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
	
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_COPY_AND_VERIFY,
								( BYTCHK << 1 ) | PAD,
								0x00,
 								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
	                  			0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::EXTENDED_COPY
//
//----------------------------------------------------------------------
//
//		The EXTENDED_COPY command as defined in section 7.5.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::EXTENDED_COPY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::EXTENDED_COPY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n", PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 16-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_EXTENDED_COPY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 24 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::INQUIRY
//
//----------------------------------------------------------------------
//
//		The INQUIRY command as defined in section 7.6.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::INQUIRY (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			CMDDT,
    			SCSICmdField1Bit 			EVPD,
    			SCSICmdField1Byte 			PAGE_OR_OPERATION_CODE,
    			SCSICmdField1Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::INQUIRY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );

	if ( IsParameterValid ( CMDDT, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "CMDDT = %x not valid\n", CMDDT ) );
		return false;
		
	}

	if ( IsParameterValid ( EVPD, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "EVPD = %x not valid\n", EVPD ) );
		return false;
		
	}

	if ( IsParameterValid ( PAGE_OR_OPERATION_CODE, kSCSICmdFieldMask1Byte ) == false )
	{

		STATUS_LOG ( ( "PAGE_OR_OPERATION_CODE = %x not valid\n",
						PAGE_OR_OPERATION_CODE ) );
		return false;

	}
	
	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{

		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n", ALLOCATION_LENGTH ) );
		return false;

	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
	
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
	
	}
	
	// If the PAGE_OR_OPERATION_CODE parameter is not zero and both the CMDDT
	// and EVPD parameters are, indicate that the PAGE_OR_OPERATION_CODE is not 
	// valid.
	if ( ( CMDDT == 0 ) && ( EVPD == 0 ) && ( PAGE_OR_OPERATION_CODE != 0 ) )
    {
		
		// A valid CDB could not be created, return false to let the
		// client know.
		STATUS_LOG ( ( "combo not valid, CMDDT = %x, EVPD = %x, PAGE_OR_OPERATION_CODE = %x\n",
						CMDDT, EVPD, PAGE_OR_OPERATION_CODE ) );
		return false;
		
    }
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_INQUIRY,
								( CMDDT << 1 ) | EVPD,
								PAGE_OR_OPERATION_CODE,
								0x00,
								ALLOCATION_LENGTH,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::LOG_SELECT
//
//----------------------------------------------------------------------
//
//		The LOG_SELECT command as defined in section 7.7.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::LOG_SELECT (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PCR,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField2Bit 			PC,
    			SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::LOG_SELECT called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );

	if ( IsParameterValid ( PCR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PCR = %x not valid\n", PCR ) );
		return false;
		
	}

	if ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "SP = %x not valid\n", SP ) );
		return false;
		
	}

	if ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ) == false )
	{
		
		STATUS_LOG ( ( "PC = %x not valid\n", PC ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOG_SELECT,
								( PCR << 1) | SP,
								PC << 6,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}  


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::LOG_SENSE
//
//----------------------------------------------------------------------
//
//		The LOG_SENSE command as defined in section 7.8.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::LOG_SENSE (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PPC,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField2Bit 			PC,
    			SCSICmdField6Bit 			PAGE_CODE,
    			SCSICmdField2Byte 			PARAMETER_POINTER,
    			SCSICmdField2Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::LOG_SENSE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PPC, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PPC = %x not valid\n", PPC ) );
		return false;
		
	}

	if ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "SP = %x not valid\n", SP ) );
		return false;
		
	}

	if ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ) == false )
	{
		
		STATUS_LOG ( ( "PC = %x not valid\n", PC ) );
		return false;
		
	}

	if ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ) == false )
	{
		
		STATUS_LOG ( ( "PAGE_CODE = %x not valid\n", PAGE_CODE ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_POINTER, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_POINTER = %x not valid\n",
						PARAMETER_POINTER ) );
		return false;
		
	}
	
	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOG_SENSE,
								( PPC << 1 ) | SP,
								( PC << 6 ) | PAGE_CODE,
								0x00,
								0x00,
 								( PARAMETER_POINTER >> 8 ) & 0xFF,
								  PARAMETER_POINTER 	   & 0xFF,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );

	return true;
	
}  


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::MODE_SELECT_6
//
//----------------------------------------------------------------------
//
//		The MODE_SELECT(6) command as defined in section 7.9.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::MODE_SELECT_6 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PF,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField1Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SELECT_6 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PF, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PF = %x not valid\n", PF ) );
		return false;
		
	}

	if ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "SP = %x not valid\n", SP ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SELECT_6,
								( PF << 4) | SP,
								0x00,
								0x00,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}  


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::MODE_SELECT_10
//
//----------------------------------------------------------------------
//
//		The MODE_SELECT(10) command as defined in section 7.10.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::MODE_SELECT_10 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PF,
    			SCSICmdField1Bit 			SP,
    			SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SELECT_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PF, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PF = %x not valid\n", PF ) );
		return false;
		
	}

	if ( IsParameterValid ( SP, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "SP = %x not valid\n", SP ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SELECT_10,
								( PF << 4 ) | SP,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}  


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::MODE_SENSE_6
//
//----------------------------------------------------------------------
//
//		The MODE_SENSE(6) command as defined in section 7.11.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::MODE_SENSE_6 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DBD,
   				SCSICmdField2Bit 			PC,
   				SCSICmdField6Bit 			PAGE_CODE,
   				SCSICmdField1Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SENSE_6 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( DBD, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "DBD = %x not valid\n", DBD ) );
		return false;
		
	}

	if ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ) == false )
	{
		
		STATUS_LOG ( ( "PC = %x not valid\n", PC ) );
		return false;
		
	}

	if ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ) == false )
	{
		
		STATUS_LOG ( ( "PAGE_CODE = %x not valid\n", PAGE_CODE ) );
		return false;
		
	}

	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SENSE_6,
								DBD << 3,
								( PC << 6) | PAGE_CODE,
								0x00,
								ALLOCATION_LENGTH,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::MODE_SENSE_10
//
//----------------------------------------------------------------------
//
//		The MODE_SENSE(10) command as defined in section 7.12.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::MODE_SENSE_10 (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			LLBAA,
				SCSICmdField1Bit 			DBD,
				SCSICmdField2Bit 			PC,
				SCSICmdField6Bit 			PAGE_CODE,
				SCSICmdField2Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::MODE_SENSE_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( LLBAA, kSCSICmdFieldMask1Bit ) == false )
	{
	
		STATUS_LOG ( ( "LLBAA = %x not valid\n", LLBAA ) );
		return false;
		
	}

	if ( IsParameterValid ( DBD, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "DBD = %x not valid\n", DBD ) );
		return false;
		
	}

	if ( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ) == false )
	{
		
		STATUS_LOG ( ( "PC = %x not valid\n", PC ) );
		return false;
		
	}

	if ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ) == false )
	{
		
		STATUS_LOG ( ( "PAGE_CODE = %x not valid\n", PAGE_CODE ) );
		return false;
		
	}

	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{

		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;

	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SENSE_10,
								( LLBAA << 4 ) | ( DBD << 3 ),
								( PC << 6 ) | PAGE_CODE,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::PERSISTENT_RESERVE_IN
//
//----------------------------------------------------------------------
//
//		The PERSISTENT_RESERVE_IN command as defined in section 7.13.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::PERSISTENT_RESERVE_IN (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
   				SCSICmdField5Bit 			SERVICE_ACTION,
   				SCSICmdField2Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::PERSISTENT_RESERVE_IN called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ) == false )
	{
		
		STATUS_LOG ( ( "SERVICE_ACTION = %x not valid\n",
						SERVICE_ACTION ) );
		return false;
		
	}

	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PERSISTENT_RESERVE_IN,
								SERVICE_ACTION,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::PERSISTENT_RESERVE_OUT
//
//----------------------------------------------------------------------
//
//		The PERSISTENT_RESERVE_OUT command as defined in section 7.14.
//		
//	NB:	There is no PARAMETER_LIST_LENGTH parameter as this value is
// 		always 0x18 for the SPC version of this command. The buffer for
//		the data must be at least of that size.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::PERSISTENT_RESERVE_OUT (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
   				SCSICmdField5Bit 			SERVICE_ACTION,
   				SCSICmdField4Bit 			SCOPE,
   				SCSICmdField4Bit 			TYPE,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::PERSISTENT_RESERVE_OUT called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ) == false )
	{
		
		STATUS_LOG ( ( "SERVICE_ACTION = %x not valid\n",
						SERVICE_ACTION ) );
		return false;
		
	}

	if ( IsParameterValid ( SCOPE, kSCSICmdFieldMask4Bit ) == false )
	{
		
		STATUS_LOG ( ( "SCOPE = %x not valid\n", SCOPE ) );
		return false;
		
	}

	if ( IsParameterValid ( TYPE, kSCSICmdFieldMask4Bit ) == false )
	{
		
		STATUS_LOG ( ( "TYPE = %x not valid\n", TYPE ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, 0x18 ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid = %x\n", dataBuffer ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PERSISTENT_RESERVE_OUT,
								SERVICE_ACTION,
								( SCOPE << 4 ) | TYPE,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x18,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								0x18 );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::PREVENT_ALLOW_MEDIUM_REMOVAL
//
//----------------------------------------------------------------------
//
//		The PREVENT_ALLOW_MEDIUM_REMOVAL command as defined in
//		section 7.15.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::PREVENT_ALLOW_MEDIUM_REMOVAL ( 
				SCSITask *					request,
     			SCSICmdField2Bit 			PREVENT,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::PREVENT_ALLOW_MEDIUM_REMOVAL called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PREVENT, kSCSICmdFieldMask2Bit ) == false )
	{

		STATUS_LOG ( ( "PREVENT = %x not valid\n", PREVENT ) );
		return false;

	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL,
								0x00,
								0x00,
								0x00,
								PREVENT,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::READ_BUFFER
//
//----------------------------------------------------------------------
//
//		The READ_BUFFER command as defined in section 7.16.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::READ_BUFFER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField4Bit 			MODE,
				SCSICmdField1Byte 			BUFFER_ID,
				SCSICmdField3Byte 			BUFFER_OFFSET,
				SCSICmdField3Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::READ_BUFFER called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( MODE, kSCSICmdFieldMask4Bit ) == false )
	{

		STATUS_LOG ( ( "MODE = %x not valid\n", MODE ) );
		return false;

	}

	if ( IsParameterValid ( BUFFER_ID, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "BUFFER_ID = %x not valid\n", BUFFER_ID ) );
		return false;
		
	}

	if ( IsParameterValid ( BUFFER_OFFSET, kSCSICmdFieldMask3Byte ) == false )
	{
	
		STATUS_LOG ( ( "BUFFER_OFFSET = %x not valid\n",
						BUFFER_OFFSET ) );
		return false;
	
	}

	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
	
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
	
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_BUFFER,
								MODE,
								BUFFER_ID,
								( BUFFER_OFFSET >> 16 ) & 0xFF,
								( BUFFER_OFFSET >> 8 )  & 0xFF,
								BUFFER_OFFSET & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8 )  & 0xFF,
								ALLOCATION_LENGTH & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RECEIVE
//
//----------------------------------------------------------------------
//
//		The RECEIVE command as defined in section 9.2.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RECEIVE ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
	 			SCSICmdField3Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::RECEIVE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
		
		STATUS_LOG ( ( "TRANSFER_LENGTH = %x not valid\n",
						TRANSFER_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, TRANSFER_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, TRANSFER_LENGTH = %x\n",
						dataBuffer, TRANSFER_LENGTH ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RECEIVE,
								0x00,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8  ) & 0xFF,
								  TRANSFER_LENGTH 		  & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								TRANSFER_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RECEIVE_COPY_RESULTS
//
//----------------------------------------------------------------------
//
//		The RECEIVE_COPY_RESULTS command as defined in section 7.17.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RECEIVE_COPY_RESULTS (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
 				SCSICmdField5Bit 			SERVICE_ACTION,
 				SCSICmdField1Byte			LIST_IDENTIFIER,
 				SCSICmdField4Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::RECEIVE_COPY_RESULTS called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ) == false )
	{
		
		STATUS_LOG ( ( "SERVICE_ACTION = %x not valid\n",
						SERVICE_ACTION ) );
		return false;
		
	}

	if ( IsParameterValid ( LIST_IDENTIFIER, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "LIST_IDENTIFIER = %x not valid\n",
						LIST_IDENTIFIER ) );
		return false;
		
	}

	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// This is a 16-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RECEIVE_COPY_RESULTS,
								SERVICE_ACTION,
								LIST_IDENTIFIER,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 24 ) & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8  ) & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RECEIVE_DIAGNOSTICS_RESULTS
//
//----------------------------------------------------------------------
//
//		The RECEIVE_DIAGNOSTICS_RESULTS command as defined in
//		section 7.18.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RECEIVE_DIAGNOSTICS_RESULTS ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
	 			SCSICmdField1Bit 			PCV,
	 			SCSICmdField1Byte			PAGE_CODE,
	 			SCSICmdField2Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::RECEIVE_DIAGNOSTICS_RESULTS called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PCV, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PCV = %x not valid\n", PCV ) );
		return false;
		
	}

	if ( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "PAGE_CODE = %x not valid\n", PAGE_CODE ) );
		return false;
		
	}

	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS,
								PCV,
								PAGE_CODE,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RELEASE_10
//
//----------------------------------------------------------------------
//
//		The RELEASE(10) command as defined in section 7.19.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RELEASE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY,
				SCSICmdField1Bit 			LONGID,
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "THRDPTY = %x not valid\n", THRDPTY ) );
		return false;
		
	}

	if ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "LONGID = %x not valid\n", LONGID ) );
		return false;
		
	}

	if ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "THIRD_PARTY_DEVICE_ID = %x not valid\n",
						THIRD_PARTY_DEVICE_ID ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ),
								0x00,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RELEASE_10
//
//----------------------------------------------------------------------
//
//	еее OBSOLETE еее
//
//		The RELEASE(10) command as defined in SPC revision 11a,
//		section 7.17. The SPC-2 specification obsoleted the EXTENT and
//		RESERVATION_IDENTIFICATION fields.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RELEASE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY,
				SCSICmdField1Bit 			LONGID,
				SCSICmdField1Bit 			EXTENT,
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_10 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "THRDPTY = %x not valid\n", THRDPTY ) );
		return false;
		
	}

	if ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "LONGID = %x not valid\n", LONGID ) );
		return false;
		
	}

	if ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "EXTENT = %x not valid\n", EXTENT ) );
		return false;
		
	}

	if ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "RESERVATION_IDENTIFICATION = %x not valid\n",
						RESERVATION_IDENTIFICATION ) );
		return false;
		
	}

	if ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "THIRD_PARTY_DEVICE_ID = %x not valid\n",
						THIRD_PARTY_DEVICE_ID ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ) | EXTENT,
								RESERVATION_IDENTIFICATION,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH 	   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RELEASE_6
//
//----------------------------------------------------------------------
//
//		The RELEASE(6) command as defined in section 7.20
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RELEASE_6 ( 
				SCSITask *					request,
				SCSICmdField1Byte			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_6 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_6,
								0x00,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RELEASE_6
//
//----------------------------------------------------------------------
//
//	еее OBSOLETE еее
//
//		The RELEASE(6) command as defined in SPC revision 11a,
//		section 7.18. The SPC-2 specification obsoleted the EXTENT and
//		RESERVATION_IDENTIFICATION fields.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RELEASE_6 ( 
				SCSITask *					request,
				SCSICmdField1Bit 			EXTENT,
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RELEASE_6 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "EXTENT = %x not valid\n", EXTENT ) );
		return false;
		
	}
	
	if ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "RESERVATION_IDENTIFICATION = %x not valid\n",
						RESERVATION_IDENTIFICATION ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RELEASE_6,
								EXTENT,
								RESERVATION_IDENTIFICATION,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::REPORT_DEVICE_IDENTIFIER
//
//----------------------------------------------------------------------
//
//		The REPORT_DEVICE_IDENTIFIER command as defined in section 7.21.
//
//	NB:	There is no SERVICE_ACTION parameter as the value for this field
//		for the SPC version of this command is defined to always be 0x05.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::REPORT_DEVICE_IDENTIFIER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField4Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::REPORT_DEVICE_IDENTIFIER called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPORT_DEVICE_IDENTIFIER,
								0x05,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 24 ) & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8  ) & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::REPORT_LUNS
//
//----------------------------------------------------------------------
//
//		The REPORT_LUNS command as defined in section 7.22.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::REPORT_LUNS ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
			 	SCSICmdField4Byte 			ALLOCATION_LENGTH,
			 	SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::REPORT_LUNS called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPORT_LUNS,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 24 ) & 0xFF,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >> 8  ) & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::REQUEST_SENSE
//
//----------------------------------------------------------------------
//
//		The REQUEST_SENSE command as defined in section 7.23.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::REQUEST_SENSE (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
			 	SCSICmdField1Byte 			ALLOCATION_LENGTH,  
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::REQUEST_SENSE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REQUEST_SENSE,
								0x00,
								0x00,
								0x00,
								ALLOCATION_LENGTH,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RESERVE_10
//
//----------------------------------------------------------------------
//
//		The RESERVE(10) command as defined in section 7.24.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RESERVE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY, 
				SCSICmdField1Bit 			LONGID, 
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "THRDPTY = %x not valid\n", THRDPTY ) );
		return false;
		
	}
	
	if ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "LONGID = %x not valid\n", LONGID ) );
		return false;
		
	}
	
	if ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "THIRD_PARTY_DEVICE_ID = %x not valid\n",
						THIRD_PARTY_DEVICE_ID ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer, PARAMETER_LIST_LENGTH ) )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ),
								0x00,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH		   & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RESERVE_10
//
//----------------------------------------------------------------------
//
//	еее OBSOLETE еее
//
//		The RESERVE(10) command as defined in SPC revision 11a,
//		section 7.21. The SPC-2 specification obsoleted the EXTENT and 
//		RESERVATION_IDENTIFICATION fields.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RESERVE_10 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			THRDPTY, 
				SCSICmdField1Bit 			LONGID, 
				SCSICmdField1Bit 			EXTENT, 
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField1Byte 			THIRD_PARTY_DEVICE_ID,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_10 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( THRDPTY, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "THRDPTY = %x not valid\n", THRDPTY ) );
		return false;
		
	}
	
	if ( IsParameterValid ( LONGID, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "LONGID = %x not valid\n", LONGID ) );
		return false;
		
	}
	
	if ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "EXTENT = %x not valid\n", EXTENT ) );
		return false;
		
	}
	
	if ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "RESERVATION_IDENTIFICATION = %x not valid\n",
						RESERVATION_IDENTIFICATION ) );
		return false;
		
	}
	
	if ( IsParameterValid ( THIRD_PARTY_DEVICE_ID, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "THIRD_PARTY_DEVICE_ID = %x not valid\n",
						THIRD_PARTY_DEVICE_ID ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_10,
								( THRDPTY << 4 ) | ( LONGID << 1 ) | EXTENT,
								RESERVATION_IDENTIFICATION,
								THIRD_PARTY_DEVICE_ID,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RESERVE_6
//
//----------------------------------------------------------------------
//
//		The RESERVE(6) command as defined in section 7.25.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RESERVE_6 ( 
				SCSITask *					request,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_6 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_6,
								0x00,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::RESERVE_6
//
//----------------------------------------------------------------------
//
//	еее OBSOLETE еее
//
//		The RESERVE(6) command as defined in SPC revision 11a,
//		section 7.22. The SPC-2 specification obsoleted the EXTENT,
//		RESERVATION_IDENTIFICATION and PARAMETER_LIST_LENGTH fields.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::RESERVE_6 ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			EXTENT,
				SCSICmdField1Byte 			RESERVATION_IDENTIFICATION,
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::RESERVE_6 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( EXTENT, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "EXTENT = %x not valid\n", EXTENT ) );
		return false;
		
	}
	
	if ( IsParameterValid ( RESERVATION_IDENTIFICATION, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "RESERVATION_IDENTIFICATION = %x not valid\n",
						RESERVATION_IDENTIFICATION ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_6,
								EXTENT,
								RESERVATION_IDENTIFICATION,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SEND
//
//----------------------------------------------------------------------
//
//		The SEND command as defined in section 9.3.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SEND ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit			AER,
	 			SCSICmdField3Byte 			TRANSFER_LENGTH, 
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::SEND called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( AER, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "AER = %x not valid\n", AER ) );
		return false;
		
	}
	
	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
		
		STATUS_LOG ( ( "TRANSFER_LENGTH = %x not valid\n",
						TRANSFER_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND,
								AER,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8  ) & 0xFF,
								  TRANSFER_LENGTH		  & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								TRANSFER_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SEND_DIAGNOSTICS
//
//----------------------------------------------------------------------
//
//		The SEND_DIAGNOSTICS command as defined in section 7.26.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SEND_DIAGNOSTICS ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
			  	SCSICmdField3Bit 			SELF_TEST_CODE, 
				SCSICmdField1Bit 			PF, 
				SCSICmdField1Bit 			SELF_TEST, 
				SCSICmdField1Bit 			DEVOFFL, 
				SCSICmdField1Bit 			UNITOFFL, 
				SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::SEND_DIAGNOSTICS called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( SELF_TEST_CODE, kSCSICmdFieldMask3Bit ) == false )
	{
		
		STATUS_LOG ( ( "SELF_TEST_CODE = %x not valid\n",
						SELF_TEST_CODE ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PF, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PF = %x not valid\n", PF ) );
		return false;
		
	}
	
	if ( IsParameterValid ( SELF_TEST, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "SELF_TEST = %x not valid\n", SELF_TEST ) );
		return false;
		
	}
	
	if ( IsParameterValid ( DEVOFFL, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "DEVOFFL = %x not valid\n", DEVOFFL ) );
		return false;
		
	}
	
	if ( IsParameterValid ( UNITOFFL, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "UNITOFFL = %x not valid\n", UNITOFFL ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( ( SELF_TEST == 1 ) && ( SELF_TEST_CODE != 0x00 ) )
	{
		
		// When SELF_TEST bit is zero, SELF_TEST_CODE MUST be zero
		STATUS_LOG ( ( "combo not valid, SELF_TEST = %x, SELF_TEST_CODE = %x\n",
						SELF_TEST, SELF_TEST_CODE ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_DIAGNOSTICS,
								( SELF_TEST_CODE << 5 ) | ( PF << 4 ) |
									( SELF_TEST << 2 ) | ( DEVOFFL << 1 ) | UNITOFFL,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 )  & 0xFF,
								  PARAMETER_LIST_LENGTH 		& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::SET_DEVICE_IDENTIFIER
//
//----------------------------------------------------------------------
//
//		The SET_DEVICE_IDENTIFIER command as defined in section 7.27.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::SET_DEVICE_IDENTIFIER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField5Bit 			SERVICE_ACTION,
				SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::SET_DEVICE_IDENTIFIER called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( SERVICE_ACTION, kSCSICmdFieldMask5Bit ) == false )
	{
		
		STATUS_LOG ( ( "SERVICE_ACTION = %x not valid\n",
						SERVICE_ACTION ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_DEVICE_IDENTIFIER,
								SERVICE_ACTION,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 24 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8  ) & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::TEST_UNIT_READY
//
//----------------------------------------------------------------------
//
//		The TEST_UNIT_READY command as defined in section 7.28.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::TEST_UNIT_READY (
				SCSITask *					request,
    			SCSICmdField1Byte			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::TEST_UNIT_READY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_TEST_UNIT_READY,
								0x00,
								0x00,
								0x00,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								10 * 1000,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIPrimaryCommands::WRITE_BUFFER
//
//----------------------------------------------------------------------
//
//		The TEST_UNIT_READY command as defined in section 7.29.
//
//----------------------------------------------------------------------

bool
SCSIPrimaryCommands::WRITE_BUFFER ( 
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField4Bit 			MODE, 
				SCSICmdField1Byte 			BUFFER_ID, 
				SCSICmdField3Byte 			BUFFER_OFFSET, 
				SCSICmdField3Byte 			PARAMETER_LIST_LENGTH, 
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIPrimaryCommands::WRITE_BUFFER called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( MODE, kSCSICmdFieldMask4Bit ) == false )
	{

		STATUS_LOG ( ( "MODE = %x not valid\n", MODE ) );
		return false;

	}
	
	if ( IsParameterValid ( BUFFER_ID, kSCSICmdFieldMask1Byte ) == false )
	{

		STATUS_LOG ( ( "BUFFER_ID = %x not valid\n", BUFFER_ID ) );
		return false;

	}
	
	if ( IsParameterValid ( BUFFER_OFFSET, kSCSICmdFieldMask3Byte ) == false )
	{
		
		STATUS_LOG ( ( "BUFFER_OFFSET = %x not valid\n",
						BUFFER_OFFSET ) );
		return false;
		
	}
	
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
		
		STATUS_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_BUFFER,
								MODE,
								BUFFER_ID,
								( BUFFER_OFFSET >> 16 ) & 0xFF,
								( BUFFER_OFFSET >> 8  ) & 0xFF,
								  BUFFER_OFFSET			& 0xFF,
								( PARAMETER_LIST_LENGTH >> 16 ) & 0xFF,
								( PARAMETER_LIST_LENGTH >> 8  ) & 0xFF,
								  PARAMETER_LIST_LENGTH			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
	
}