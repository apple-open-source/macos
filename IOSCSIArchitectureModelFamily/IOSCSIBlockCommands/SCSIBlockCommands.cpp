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
#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>

#include "SCSIBlockCommands.h"

#if ( SCSI_SBC_COMMANDS_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#define DEBUG_ASSERT(x)		assert x
#else
#define PANIC_NOW(x)
#define DEBUG_ASSERT(x)
#endif

#if ( SCSI_SBC_COMMANDS_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_SBC_COMMANDS_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define super SCSIPrimaryCommands
OSDefineMetaClassAndStructors ( SCSIBlockCommands, SCSIPrimaryCommands );

//----------------------------------------------------------------------
//
//		SCSIBlockCommands::CreateSCSIBlockCommandObject
//
//----------------------------------------------------------------------
//		
//		Factory method for getting an instance of the command builder
//
//----------------------------------------------------------------------

SCSIBlockCommands *
SCSIBlockCommands::CreateSCSIBlockCommandObject ( void )
{

	return new SCSIBlockCommands;

}

#pragma mark SBC Command Methods

// SCSI Block Commands as defined in T10:990D SBC, Revision 8c,
// dated 13 November 1997

//----------------------------------------------------------------------
//
//		SCSIBlockCommands::ERASE_10
//
//----------------------------------------------------------------------
//		
//		The ERASE(10) command as defined in section 6.2.1
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::ERASE_10 (
				SCSITask *					request,
    			SCSICmdField1Bit 			ERA,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::ERASE_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( ERA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "ERA = %x not valid\n", ERA ) );
		return false;
		
	}

	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
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

	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_ERASE_10,
								( ERA << 2 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS 		& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::ERASE_12
//
//----------------------------------------------------------------------
//		
//		The ERASE(12) command as defined in section 6.2.2
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::ERASE_12 (
				SCSITask *					request,
    			SCSICmdField1Bit 			ERA,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField4Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::ERASE_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( ERA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "ERA = %x not valid\n", ERA ) );
		return false;
		
	}
	
	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}
	
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}
	
	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ) == false )
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
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_ERASE_12,
								( ERA << 2 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS		   & 0xFF,
								( TRANSFER_LENGTH >> 24 ) & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >> 8  ) & 0xFF,
								  TRANSFER_LENGTH		  & 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl (	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::FORMAT_UNIT
//
//----------------------------------------------------------------------
//		
//		The FORMAT_UNIT command as defined in section 6.1.1
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::FORMAT_UNIT (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			IOByteCount					defectListSize,
    			SCSICmdField1Bit 			FMTDATA,
    			SCSICmdField1Bit 			CMPLST,
    			SCSICmdField3Bit 			DEFECT_LIST_FORMAT,
    			SCSICmdField1Byte 			VENDOR_SPECIFIC,
    			SCSICmdField2Byte 			INTERLEAVE,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::FORMAT_UNIT called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( FMTDATA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "FMTDATA = %x not valid\n", FMTDATA ) );
		return false;
		
	}

	if ( IsParameterValid ( CMPLST, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "CMPLST = %x not valid\n", CMPLST ) );
		return false;
		
	}

	if ( IsParameterValid ( DEFECT_LIST_FORMAT, kSCSICmdFieldMask3Bit ) == false )
	{
		
		STATUS_LOG ( ( "DEFECT_LIST_FORMAT = %x not valid\n", DEFECT_LIST_FORMAT ) );
		return false;
		
	}

	if ( IsParameterValid ( VENDOR_SPECIFIC, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "VENDOR_SPECIFIC = %x not valid\n", VENDOR_SPECIFIC ) );
		return false;
		
	}

	if ( IsParameterValid ( INTERLEAVE, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "INTERLEAVE = %x not valid\n", INTERLEAVE ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	if ( defectListSize > 0 )
	{
		
		// We have data to send to the device, 
		// make sure that we were given a valid buffer
		if ( IsBufferAndCapacityValid ( dataBuffer, defectListSize  )
				== false )
		{
			
			STATUS_LOG ( ( "dataBuffer = %x not valid, defectListSize = %x\n",
							dataBuffer, defectListSize ) );
			return false;
			
		}
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_FORMAT_UNIT,
								( FMTDATA << 4 ) | ( CMPLST << 4 ) | DEFECT_LIST_FORMAT,
								VENDOR_SPECIFIC,
								( INTERLEAVE >> 8 ) & 0xFF,
								  INTERLEAVE		& 0xFF,
								CONTROL );
 	
 	if ( FMTDATA == 0 )
	{
		
		// No DEFECT LIST is being sent to the device, so there
		// will be no data transfer for this request.
		SetDataTransferControl ( 	request,
                            		0,
									kSCSIDataTransfer_NoDataTransfer );	
	
	}
	
	else
	{
		
		// The client has requested a DEFECT LIST be sent to the device
		// to be used with the format command
		SetDataTransferControl ( 	request,
                            		0,
									kSCSIDataTransfer_FromInitiatorToTarget,
									dataBuffer );
									// -->Need defect list size	
	
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::LOCK_UNLOCK_CACHE
//
//----------------------------------------------------------------------
//		
//		The LOCK_UNLOCK_CACHE command as defined in section 6.1.2
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::LOCK_UNLOCK_CACHE (
				SCSITask *					request,
    			SCSICmdField1Bit 			LOCK,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::LOCK_UNLOCK_CACHE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( LOCK, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "LOCK = %x not valid\n", LOCK ) );
		return false;
		
	}

	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( NUMBER_OF_BLOCKS, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "NUMBER_OF_BLOCKS = %x not valid\n",
						NUMBER_OF_BLOCKS ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOCK_UNLOCK_CACHE,
								( LOCK << 1 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( NUMBER_OF_BLOCKS >> 8 ) 	& 0xFF,
								  NUMBER_OF_BLOCKS			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_NoDataTransfer );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::MEDIUM_SCAN
//
//----------------------------------------------------------------------
//		
//		The MEDIUM_SCAN command as defined in section 6.2.3
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::MEDIUM_SCAN (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
   				SCSICmdField1Bit 			WBS,
   				SCSICmdField1Bit 			ASA,
   				SCSICmdField1Bit 			RSD,
   				SCSICmdField1Bit 			PRA,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::MEDIUM_SCAN called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( WBS, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "WBS = %x not valid\n", WBS ) );
		return false;
		
	}

	if ( IsParameterValid ( ASA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "ASA = %x not valid\n", ASA ) );
		return false;
		
	}

	if ( IsParameterValid ( RSD, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RSD = %x not valid\n", RSD ) );
		return false;
		
	}

	if ( IsParameterValid ( PRA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PRA = %x not valid\n", PRA ) );
		return false;
		
	}

	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
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

	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MEDIUM_SCAN,
								( WBS << 4 ) | ( ASA << 3 ) | ( RSD << 2 ) |
									( PRA << 1 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								0x00,
								PARAMETER_LIST_LENGTH,
								CONTROL );
	
	if ( PARAMETER_LIST_LENGTH > 0 )
	{
		
		SetDataTransferControl ( 	request,
                            		0,
									kSCSIDataTransfer_FromInitiatorToTarget,
									dataBuffer,
									PARAMETER_LIST_LENGTH );	
	
	}
	
	else
	{
		
		SetDataTransferControl ( 	request,
                            		0,
									kSCSIDataTransfer_NoDataTransfer );	
	
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::PREFETCH
//
//----------------------------------------------------------------------
//		
//		The PREFETCH command as defined in section 6.1.3
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::PREFETCH (
				SCSITask *					request,
    			SCSICmdField1Bit 			IMMED,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::PREFETCH called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "IMMED = %x not valid\n", IMMED ) );
		return false;
		
	}

	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
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
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PREFETCH,
								( IMMED << 1 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS 		& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_NoDataTransfer );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_6
//
//----------------------------------------------------------------------
//		
//		The READ(6) command as defined in section 6.1.4
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_6 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				UInt64						transferCount,
    			SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField1Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_6 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask21Bit ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask1Byte ) == false )
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
								kSCSICmd_READ_6,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0x1F,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								TRANSFER_LENGTH,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								transferCount );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_10
//
//----------------------------------------------------------------------
//		
//		The READ(10) command as defined in section 6.1.5
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				UInt64						transferCount,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField2Byte 			TRANSFER_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "DPO = %x not valid\n", DPO ) );
		return false;
		
	}

	if ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "FUA = %x not valid\n", FUA ) );
		return false;
		
	}

	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
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
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_10,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								transferCount );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_12
//
//----------------------------------------------------------------------
//		
//		The READ(12) command as defined in section 6.2.4
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_12 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				UInt64						transferCount,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField4Byte 			TRANSFER_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "DPO = %x not valid\n", DPO ) );
		return false;
		
	}

	if ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "FUA = %x not valid\n", FUA ) );
		return false;
		
	}

	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ) == false )
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
	
	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_12,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								( TRANSFER_LENGTH >> 24 ) 		& 0xFF,
								( TRANSFER_LENGTH >> 16 ) 		& 0xFF,
								( TRANSFER_LENGTH >> 8  ) 		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								transferCount );	

	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_CAPACITY
//
//----------------------------------------------------------------------
//		
//		The READ_CAPACITY command as defined in section 6.1.6
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_CAPACITY (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField1Bit 			PMI,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_CAPACITY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}
	
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( PMI, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PMI = %x not valid\n", PMI ) );
		return false;
		
	}
	
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CAPACITY,
								RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								0x00,
								PMI,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								8 );	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_DEFECT_DATA_10
//
//----------------------------------------------------------------------
//		
//		The READ_DEFECT_DATA(10) command as defined in section 6.1.7
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_DEFECT_DATA_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PLIST,
    			SCSICmdField1Bit 			GLIST,
    			SCSICmdField3Bit 			DEFECT_LIST_FORMAT,
    			SCSICmdField2Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_DEFECT_DATA_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_DEFECT_DATA_12
//
//----------------------------------------------------------------------
//		
//		The READ_DEFECT_DATA(12) command as defined in section 6.2.5
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_DEFECT_DATA_12 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			PLIST,
    			SCSICmdField1Bit 			GLIST,
    			SCSICmdField3Bit 			DEFECT_LIST_FORMAT,
    			SCSICmdField4Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_DEFECT_DATA_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_GENERATION
//
//----------------------------------------------------------------------
//		
//		The READ_GENERATION command as defined in section 6.2.6
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_GENERATION (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField1Byte 			ALLOCATION_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_GENERATION called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_LONG
//
//----------------------------------------------------------------------
//		
//		The READ_LONG command as defined in section 6.1.8
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_LONG (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			CORRCT,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			BYTE_TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_LONG called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::READ_UPDATED_BLOCK_10
//
//----------------------------------------------------------------------
//		
//		The READ_UPDATED_BLOCK(10) command as defined in section 6.2.7
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::READ_UPDATED_BLOCK_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
    		 	SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField1Bit 			LATEST,
    		 	SCSICmdField15Bit 			GENERATION_ADDRESS,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::READ_UPDATED_BLOCK_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::REASSIGN_BLOCKS
//
//----------------------------------------------------------------------
//		
//		The REASSIGN_BLOCKS command as defined in section 6.1.9
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::REASSIGN_BLOCKS (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::REASSIGN_BLOCKS called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::REBUILD
//
//----------------------------------------------------------------------
//		
//		The REBUILD command as defined in section 6.1.10
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::REBUILD (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
    		 	SCSICmdField1Bit 			INTDATA,
    			SCSICmdField2Bit 			PORT_CONTROL,
    		 	SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField4Byte 			REBUILD_LENGTH,
    			SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::REBUILD called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::REGENERATE
//
//----------------------------------------------------------------------
//		
//		The REGENERATE command as defined in section 6.1.11
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::REGENERATE (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit			DPO,
    			SCSICmdField1Bit 			FUA,
    		 	SCSICmdField1Bit 			INTDATA,
    		 	SCSICmdField2Bit 			PORT_CONTROL,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField4Byte 			REBUILD_LENGTH,
    			SCSICmdField4Byte 			PARAMETER_LIST_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::REGENERATE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::REZERO_UNIT
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The REZERO_UNIT command as defined in SCSI-2 section 9.2.13.
//		REZERO_UNIT is obsoleted by the SBC specification.
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::REZERO_UNIT ( 
				SCSITask *					request,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::REZERO_UNIT *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SEARCH_DATA_EQUAL_10
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The SEARCH_DATA_EQUAL(10) command as defined in SCSI-2,
//		section 9.2.14.
//		SEARCH_DATA_EQUAL(10) is obsoleted by the SBC specification.
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SEARCH_DATA_EQUAL_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			INVERT,
    			SCSICmdField1Bit 			SPNDAT,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SEARCH_DATA_EQUAL_10 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SEARCH_DATA_HIGH_10
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The SEARCH_DATA_HIGH(10) command as defined in SCSI-2,
//		section 9.2.14.
//		SEARCH_DATA_HIGH(10) is obsoleted by the SBC specification.
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SEARCH_DATA_HIGH_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			INVERT,
    			SCSICmdField1Bit 			SPNDAT,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SEARCH_DATA_HIGH_10 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SEARCH_DATA_LOW_10
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The SEARCH_DATA_LOW(10) command as defined in SCSI-2,
//		section 9.2.14.
//		SEARCH_DATA_LOW(10) is obsoleted by the SBC specification.
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SEARCH_DATA_LOW_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			INVERT,
    			SCSICmdField1Bit 			SPNDAT,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SEARCH_DATA_LOW_10 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SEEK_6
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The SEEK(6) command as defined in SCSI-2, section 9.2.15.
//		SEEK(6) is obsoleted by the SBC specification.
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SEEK_6 (
				SCSITask *					request,
   				SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SEEK_6 *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SEEK_10
//
//----------------------------------------------------------------------
//		
//		The SEEK(10) command as defined in section 6.1.12
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SEEK_10 (
				SCSITask *					request,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SEEK_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SET_LIMITS_10
//
//----------------------------------------------------------------------
//		
//		The SET_LIMITS(10) command as defined in section 6.1.13
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SET_LIMITS_10 (
				SCSITask *					request,
    			SCSICmdField1Bit 			RDINH,
    			SCSICmdField1Bit 			WRINH,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SET_LIMITS_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SET_LIMITS_12
//
//----------------------------------------------------------------------
//		
//		The SET_LIMITS(12) command as defined in section 6.2.8
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SET_LIMITS_12 (
				SCSITask *					request,
    			SCSICmdField1Bit 			RDINH,
    			SCSICmdField1Bit 			WRINH,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField4Byte 			NUMBER_OF_BLOCKS,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SET_LIMITS_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::START_STOP_UNIT
//
//----------------------------------------------------------------------
//		
//		The START_STOP_UNIT command as defined in section 6.1.14
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::START_STOP_UNIT (
				SCSITask *					request,
				SCSICmdField1Bit 			IMMED,
				SCSICmdField4Bit 			POWER_CONDITIONS,
				SCSICmdField1Bit 			LOEJ,
				SCSICmdField1Bit 			START,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::START_STOP_UNIT called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );

	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "IMMED = %x not valid\n", IMMED ) );
		return false;
		
	}

	if ( IsParameterValid ( POWER_CONDITIONS, kSCSICmdFieldMask4Bit ) == false )
	{
		
		STATUS_LOG ( ( "POWER_CONDITIONS = %x not valid\n",
						POWER_CONDITIONS ) );
		return false;
		
	}

	if ( IsParameterValid ( LOEJ, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "LOEJ = %x not valid\n", LOEJ ) );
		return false;
		
	}

	if ( IsParameterValid ( START, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "START = %x not valid\n", START ) );
		return false;
		
	}

	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_START_STOP_UNIT,
								IMMED,
								0x00,
								0x00,
								( POWER_CONDITIONS << 4 ) | ( LOEJ << 1 ) | START,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::SYNCHRONIZE_CACHE
//
//----------------------------------------------------------------------
//		
//		The SYNCHRONIZE_CACHE command as defined in section 6.1.15
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::SYNCHRONIZE_CACHE (
				SCSITask *					request,
				SCSICmdField1Bit 			IMMED,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::SYNCHRONIZE_CACHE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );

	// Do the pre-flight check on the passed in parameters
    if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
    {
		
		STATUS_LOG ( ( "IMMED = %x not valid\n", IMMED ) );
		return false;
		
	}

    if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
    {
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

    if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
    {
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

    if ( IsParameterValid ( NUMBER_OF_BLOCKS, kSCSICmdFieldMask2Byte ) == false )
    {
		
		STATUS_LOG ( ( "NUMBER_OF_BLOCKS = %x not valid\n",
						NUMBER_OF_BLOCKS ) );
		return false;
		
	}

    if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
    {
		
		STATUS_LOG ( ( "CONTROL = %x not valid\n", CONTROL ) );
		return false;
		
	}

    // This is a 6-Byte command, fill out the cdb appropriately
    SetCommandDescriptorBlock (	request,
                                kSCSICmd_SYNCHRONIZE_CACHE,
                                ( IMMED << 1 ) | RELADR,
                                ( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
                                ( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
                                ( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
                                  LOGICAL_BLOCK_ADDRESS			& 0xFF,
                                0x00,
                                ( NUMBER_OF_BLOCKS >> 8 )	& 0xFF,
                                  NUMBER_OF_BLOCKS			& 0xFF,
                                CONTROL );

    SetDataTransferControl (	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::UPDATE_BLOCK
//
//----------------------------------------------------------------------
//		
//		The UPDATE_BLOCK command as defined in section 6.2.9
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::UPDATE_BLOCK (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::UPDATE_BLOCK called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::VERIFY_10
//
//----------------------------------------------------------------------
//		
//		The VERIFY(10) command as defined in sections 6.1.16 and 6.2.10
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::VERIFY_10 (
				SCSITask *					request,
				SCSICmdField1Bit 			DPO,
				SCSICmdField1Bit 			BLKVFY,
				SCSICmdField1Bit 			BYTCHK,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField2Byte 			VERIFICATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::VERIFY_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::VERIFY_12
//
//----------------------------------------------------------------------
//		
//		The VERIFY(12) command as defined in section 6.2.11
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::VERIFY_12 (
				SCSITask *					request,
				SCSICmdField1Bit 			DPO,
				SCSICmdField1Bit 			BLKVFY,
				SCSICmdField1Bit 			BYTCHK,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField4Byte 			VERIFICATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::VERIFY_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::WRITE_6
//
//----------------------------------------------------------------------
//		
//		The WRITE(6) command as defined in section 6.1.17
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::WRITE_6 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField2Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField1Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::WRITE_6 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::WRITE_10
//
//----------------------------------------------------------------------
//		
//		The WRITE(10) command as defined in sections 6.1.18 and 6.2.12
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::WRITE_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				UInt64						transferCount,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
				SCSICmdField1Bit 			EBP,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField2Byte 			TRANSFER_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::WRITE_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );

	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "DPO = %x not valid\n", DPO ) );
		return false;
		
	}

	if ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "FUA = %x not valid\n", FUA ) );
		return false;
		
	}

	if ( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
		
	}

	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
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
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_10,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								transferCount );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::WRITE_12
//
//----------------------------------------------------------------------
//		
//		The WRITE(12) command as defined in section 6.2.13
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::WRITE_12 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
				SCSICmdField1Bit 			EBP,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
				SCSICmdField4Byte 			TRANSFER_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::WRITE_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::WRITE_AND_VERIFY_10
//
//----------------------------------------------------------------------
//		
//		The WRITE_AND_VERIFY(10) command as defined in sections 6.1.19
//		and 6.2.14
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::WRITE_AND_VERIFY_10 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			EBP,
    			SCSICmdField1Bit 			BYTCHK,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::WRITE_AND_VERIFY_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::WRITE_AND_VERIFY_12
//
//----------------------------------------------------------------------
//		
//		The WRITE_AND_VERIFY(12) command as defined in sections 6.2.15
//		and 6.2.14
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::WRITE_AND_VERIFY_12 (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			EBP,
    			SCSICmdField1Bit 			BYTCHK,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField4Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::WRITE_AND_VERIFY_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::WRITE_LONG
//
//----------------------------------------------------------------------
//		
//		The WRITE_LONG command as defined in section 6.1.20
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::WRITE_LONG (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::WRITE_LONG called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::WRITE_SAME
//
//----------------------------------------------------------------------
//		
//		The WRITE_SAME command as defined in section 6.1.21
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::WRITE_SAME (
				SCSITask *					request,
     			IOMemoryDescriptor *		dataBuffer,
	   			SCSICmdField1Bit 			PBDATA,
    			SCSICmdField1Bit 			LBDATA,
    			SCSICmdField1Bit 			RELADR,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::WRITE_SAME called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::XDREAD
//
//----------------------------------------------------------------------
//		
//		The XDREAD command as defined in section 6.1.22
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::XDREAD (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::XDREAD called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::XDWRITE
//
//----------------------------------------------------------------------
//		
//		The XDWRITE command as defined in section 6.1.23
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::XDWRITE (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
    			SCSICmdField1Bit 			DISABLE_WRITE,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::XDWRITE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::XDWRITE_EXTENDED
//
//----------------------------------------------------------------------
//		
//		The XDWRITE_EXTENDED command as defined in section 6.1.24
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::XDWRITE_EXTENDED (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			TABLE_ADDRESS,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
    			SCSICmdField1Bit 			DISABLE_WRITE,
    			SCSICmdField2Bit 			PORT_CONTROL,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField4Byte 			SECONDARY_BLOCK_ADDRESS,
    			SCSICmdField4Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			SECONDARY_ADDRESS,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::XDWRITE_EXTENDED called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
}


//----------------------------------------------------------------------
//
//		SCSIBlockCommands::XPWRITE
//
//----------------------------------------------------------------------
//		
//		The XPWRITE command as defined in section 6.1.25
//
//----------------------------------------------------------------------

bool
SCSIBlockCommands::XPWRITE (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
    			SCSICmdField1Bit 			DPO,
    			SCSICmdField1Bit 			FUA,
    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
    			SCSICmdField2Byte 			TRANSFER_LENGTH,
    			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIBlockCommands::XPWRITE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	return false;
	
