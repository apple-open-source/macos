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

#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>
#include <IOKit/scsi-commands/SCSIMultimediaCommands.h>


#if ( SCSI_MMC_COMMANDS_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#define DEBUG_ASSERT(x)		assert x
#else
#define PANIC_NOW(x)
#define DEBUG_ASSERT(x)
#endif

#if ( SCSI_MMC_COMMANDS_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_MMC_COMMANDS_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super SCSIPrimaryCommands
OSDefineMetaClassAndStructors ( SCSIMultimediaCommands, SCSIPrimaryCommands );


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::CreateSCSIMultimediaCommandObject
//
//----------------------------------------------------------------------
//
//		return instance of the command builder
//
//----------------------------------------------------------------------

SCSIMultimediaCommands *
SCSIMultimediaCommands::CreateSCSIMultimediaCommandObject ( void )
{

	return new SCSIMultimediaCommands;

}

#pragma mark -
#pragma mark MMC Command Methods

// SCSI Multimedia Commands as defined in T10:1228-D MMC
// Revision 11a, August 30, 1999


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::BLANK
//
//----------------------------------------------------------------------
//
//		The BLANK command as defined in section 6.1.1.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::BLANK (
				SCSITask *					request,
				SCSICmdField1Bit			IMMED,
				SCSICmdField3Bit			BLANKING_TYPE,
				SCSICmdField4Byte			START_ADDRESS_TRACK_NUMBER,
				SCSICmdField1Byte			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::BLANK called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid IMMED?
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "IMMED = %x not valid \n", IMMED ) );
		return false;
		
	}

	// did we receive a valid BLANKING_TYPE?
	if ( IsParameterValid ( BLANKING_TYPE, kSCSICmdFieldMask3Bit ) == false )
	{
		
		ERROR_LOG ( ( "BLANKING_TYPE = %x not valid \n",
						BLANKING_TYPE ) );
		return false;
		
	}

	// did we receive a valid START_ADDRESS_TRACK_NUMBER?
	if ( IsParameterValid ( START_ADDRESS_TRACK_NUMBER,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "START_ADDRESS_TRACK_NUMBER = %x not valid \n",
						START_ADDRESS_TRACK_NUMBER ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_BLANK,
								( IMMED << 4 ) | BLANKING_TYPE,
								( START_ADDRESS_TRACK_NUMBER >> 24 )	& 0xFF,
								( START_ADDRESS_TRACK_NUMBER >> 16 )	& 0xFF,
								( START_ADDRESS_TRACK_NUMBER >>  8 )	& 0xFF,
    							  START_ADDRESS_TRACK_NUMBER			& 0xFF,
								0x00,
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
//		SCSIMultimediaCommands::CLOSE_TRACK_SESSION
//
//----------------------------------------------------------------------
//
//		The CLOSE TRACK/SESSION command as defined in section 6.1.2.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::CLOSE_TRACK_SESSION (
						SCSITask *					request,
						SCSICmdField1Bit			IMMED,
						SCSICmdField1Bit			SESSION,
						SCSICmdField1Bit			TRACK,
						SCSICmdField2Byte			TRACK_NUMBER,
						SCSICmdField1Byte			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::CLOSE_TRACK_SESSION called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid IMMED?
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "IMMED = %x not valid \n", IMMED ) );
		return false;
		
	}

	// did we receive a valid SESSION?
	if ( IsParameterValid ( SESSION, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "SESSION = %x not valid \n", SESSION ) );
		return false;
		
	}

	// did we receive a valid TRACK?
	if ( IsParameterValid ( TRACK, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "TRACK = %x not valid \n", TRACK ) );
		return false;
		
	}

	// did we receive a valid TRACK_NUMBER?
	if ( IsParameterValid ( TRACK_NUMBER,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "TRACK_NUMBER = %x not valid \n",
						TRACK_NUMBER ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_CLOSE_TRACK_SESSION,
								IMMED,
								( SESSION << 1 ) | TRACK,
								0x00,
								( TRACK_NUMBER >>  8 ) & 0xFF,
								  TRACK_NUMBER         & 0xFF,
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
//		SCSIMultimediaCommands::FORMAT_UNIT
//
//----------------------------------------------------------------------
//
//		The FORMAT UNIT command as defined in section 6.1.3.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::FORMAT_UNIT (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
	    			IOByteCount					parameterListSize,
					SCSICmdField1Bit 			FMT_DATA,
					SCSICmdField1Bit 			CMP_LIST,
					SCSICmdField3Bit 			FORMAT_CODE,
					SCSICmdField2Byte 			INTERLEAVE_VALUE,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::FORMAT_UNIT called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid FMT_DATA?
	if ( IsParameterValid ( FMT_DATA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "FMT_DATA = %x not valid \n", FMT_DATA ) );
		return false;
		
	}

	// did we receive a valid CMP_LIST?
	if ( IsParameterValid ( CMP_LIST, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "CMP_LIST = %x not valid \n", CMP_LIST ) );
		return false;
		
	}

	// did we receive a valid INTERLEAVE_VALUE?
	if ( IsParameterValid ( INTERLEAVE_VALUE,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "INTERLEAVE_VALUE = %x not valid \n",
						INTERLEAVE_VALUE ) );
		return false;
		
	}
	
	// did we receive a valid FORMAT_CODE?
	switch ( FORMAT_CODE )
	{
		
		case FORMAT_CODE_CD_RW:
		{
			
			STATUS_LOG ( ( "Using FORMAT_CODE_CD_RW\n" ) );
			
			if ( INTERLEAVE_VALUE != INTERLEAVE_VALUE_CD_RW )
			{
				
				ERROR_LOG ( ( "INTERLEAVE_VALUE = %x not valid \n",
								INTERLEAVE_VALUE ) );
				return false;
				
			}
			
			break;
			
		}
		
		case FORMAT_CODE_DVD_RAM:
		{
			
			STATUS_LOG ( ( "Using FORMAT_CODE_DVD_RAM\n" ) );
			
			if ( INTERLEAVE_VALUE != INTERLEAVE_VALUE_DVD_RAM )
			{
				
				ERROR_LOG ( ( "INTERLEAVE_VALUE = %x not valid \n",
								INTERLEAVE_VALUE ) );
				return false;
				
			}
			
			break;
			
		}
		
		default:
		{
			
			ERROR_LOG ( ( "FORMAT_CODE = %x not valid \n",
							FORMAT_CODE ) );
			return false;
			
		}
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// should we send a parameter list?
	if ( FMT_DATA == FMT_DATA_PRESENT )
	{
		
		// we should have a parameter list
		if ( parameterListSize == 0 )
		{
			
			ERROR_LOG ( ( "parameterListSize = %x not valid \n",
							parameterListSize ) );
			return false;
			
		}
		
		// is the buffer large enough to accomodate this request?
		if ( IsBufferAndCapacityValid ( dataBuffer,
										parameterListSize ) == false )
		{
			
			ERROR_LOG ( ( "dataBuffer = %x not valid, parameterListSize = %ld\n",
							dataBuffer, parameterListSize ) );
			return false;
			
		}
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_FORMAT_UNIT,
								( FMT_DATA << 4 ) | ( CMP_LIST << 3 ) | FORMAT_CODE,
								0x00,
								( INTERLEAVE_VALUE >> 8 ) 	& 0xFF,
								  INTERLEAVE_VALUE        	& 0xFF,
								CONTROL );
	
	if ( parameterListSize == 0 )
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
									dataBuffer,
									parameterListSize );
		
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::GET_CONFIGURATION
//
//----------------------------------------------------------------------
//
//		The GET CONFIGURATION command as defined in section 6.1.4.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::GET_CONFIGURATION (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				SCSICmdField2Bit 			RT,
				SCSICmdField2Byte 			STARTING_FEATURE_NUMBER,
				SCSICmdField2Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GET_CONFIGURATION called\n" ) );	
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid RT?
	if ( IsParameterValid ( RT, kSCSICmdFieldMask2Bit ) == false )
	{
		
		ERROR_LOG ( ( "RT = %x not valid \n", RT ) );
		return false;
		
	}

	// did we receive a valid STARTING_FEATURE_NUMBER?
	if ( IsParameterValid ( STARTING_FEATURE_NUMBER,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "STARTING_FEATURE_NUMBER = %x not valid \n",
						STARTING_FEATURE_NUMBER ) );
		return false;
		
	}
	
	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
	
	}

	// did we receive a valid CONTROL field?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_GET_CONFIGURATION,
								RT,
								( STARTING_FEATURE_NUMBER >> 8 ) & 0xFF,
								  STARTING_FEATURE_NUMBER        & 0xFF,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) 	& 0xFF,
								  ALLOCATION_LENGTH        	& 0xFF,
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
//		SCSIMultimediaCommands::GET_EVENT_STATUS_NOTIFICATION
//
//----------------------------------------------------------------------
//
//		The GET EVENT/STATUS NOTIFICATION command as defined in
//		section 6.1.5.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::GET_EVENT_STATUS_NOTIFICATION (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				SCSICmdField1Bit 			IMMED,
				SCSICmdField1Byte 			NOTIFICATION_CLASS_REQUEST,
				SCSICmdField2Byte 			ALLOCATION_LENGTH,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GET_EVENT_STATUS_NOTIFICATION called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid IMMED?
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "IMMED = %x not valid \n", IMMED ) );
		return false;
		
	}
	
	// did we receive a valid NOTIFICATION_CLASS_REQUEST?
	if ( IsParameterValid ( NOTIFICATION_CLASS_REQUEST,
							kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "NOTIFICATION_CLASS_REQUEST = %x not valid \n",
						NOTIFICATION_CLASS_REQUEST ) );
		return false;
		
	}

	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_GET_EVENT_STATUS_NOTIFICATION,
								IMMED,
								0x00,
								0x00,
								NOTIFICATION_CLASS_REQUEST,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 )	& 0xFF,
								  ALLOCATION_LENGTH        	& 0xFF,
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
//		SCSIMultimediaCommands::GET_PERFORMANCE
//
//----------------------------------------------------------------------
//
//		The GET PERFORMANCE command as defined in section 6.1.6.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::GET_PERFORMANCE (
				SCSITask *					request,
    			IOMemoryDescriptor *		dataBuffer,
				SCSICmdField2Bit 			TOLERANCE,
				SCSICmdField1Bit 			WRITE,
				SCSICmdField2Bit 			EXCEPT,
				SCSICmdField4Byte 			STARTING_LBA,
				SCSICmdField2Byte 			MAXIMUM_NUMBER_OF_DESCRIPTORS,
				SCSICmdField1Byte 			CONTROL )
{
	
	UInt32		returnDataCount = 0;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GET_PERFORMANCE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid TOLERANCE?
	if ( IsParameterValid ( TOLERANCE, kSCSICmdFieldMask2Bit ) == false )
	{
		
		ERROR_LOG ( ( "TOLERANCE = %x not valid \n", TOLERANCE ) );
		return false;
		
	}

	// did we receive a valid WRITE?
	if ( IsParameterValid ( WRITE, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "WRITE = %x not valid \n", WRITE ) );
		return false;
		
	}

	// did we receive a valid EXCEPT?
	if ( IsParameterValid ( EXCEPT, kSCSICmdFieldMask2Bit ) == false )
	{
		
		ERROR_LOG ( ( "EXCEPT = %x not valid \n", EXCEPT ) );
		return false;
		
	}

	// did we receive a valid STARTING_LBA?
	if ( IsParameterValid ( STARTING_LBA,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "STARTING_LBA = %x not valid \n",
						STARTING_LBA ) );
		return false;
		
	}

	// did we receive a valid MAXIMUM_NUMBER_OF_DESCRIPTORS?
	if ( IsParameterValid ( MAXIMUM_NUMBER_OF_DESCRIPTORS,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "MAXIMUM_NUMBER_OF_DESCRIPTORS = %x not valid \n",
						MAXIMUM_NUMBER_OF_DESCRIPTORS ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// Compute the data count size
	returnDataCount = PERFORMANCE_HEADER_SIZE +
			( PERFORMANCE_DESCRIPTOR_SIZE * MAXIMUM_NUMBER_OF_DESCRIPTORS );
	
	if ( IsBufferAndCapacityValid ( dataBuffer, returnDataCount ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, returnDataCount = %ld\n",
						dataBuffer, returnDataCount ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_GET_PERFORMANCE,
								( TOLERANCE << 3 ) | ( WRITE << 2 ) | EXCEPT,
								( STARTING_LBA >> 24 ) 	& 0xFF,
								( STARTING_LBA >> 16 ) 	& 0xFF,
								( STARTING_LBA >>  8 ) 	& 0xFF,
								  STARTING_LBA         	& 0xFF,
								0x00,
								0x00,
								( MAXIMUM_NUMBER_OF_DESCRIPTORS >> 8 )	& 0xFF,
								  MAXIMUM_NUMBER_OF_DESCRIPTORS			& 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								returnDataCount );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::LOAD_UNLOAD_MEDIUM
//
//----------------------------------------------------------------------
//
//		The LOAD/UNLOAD MEDIUM command as defined in section 6.1.7.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::LOAD_UNLOAD_MEDIUM (
					SCSITask *					request,
					SCSICmdField1Bit 			IMMED,
					SCSICmdField1Bit 			LO_UNLO,
					SCSICmdField1Bit 			START,
					SCSICmdField1Byte 			SLOT,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::LOAD_UNLOAD_MEDIUM called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid IMMED?
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "IMMED = %x not valid \n", IMMED ) );
		return false;
		
	}
	
	// did we receive a valid LO_UNLO?
	if ( IsParameterValid ( LO_UNLO, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "LO_UNLO = %x not valid \n", LO_UNLO ) );
		return false;
		
	}
	
	// did we receive a valid START?
	if ( IsParameterValid ( START, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "START = %x not valid \n", START ) );
		return false;
		
	}
	
	// did we receive a valid SLOT?
	if ( IsParameterValid ( SLOT, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "SLOT = %x not valid \n", SLOT ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_LOAD_UNLOAD_MEDIUM,
								IMMED,
								0x00,
								0x00,
								( LO_UNLO << 1 ) | START,
								0x00,
								0x00,
								0x00,
								SLOT,
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
//		SCSIMultimediaCommands::MECHANISM_STATUS
//
//----------------------------------------------------------------------
//
//		The MECHANISM STATUS command as defined in section 6.1.8.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::MECHANISM_STATUS (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::MECHANISM_STATUS called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MECHANISM_STATUS,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) 	& 0xFF,
								  ALLOCATION_LENGTH        	& 0xFF,
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
//		SCSIMultimediaCommands::PAUSE_RESUME
//
//----------------------------------------------------------------------
//
//		The PAUSE/RESUME command as defined in section 6.1.9.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::PAUSE_RESUME (
					SCSITask *					request,
					SCSICmdField1Bit 			RESUME,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PAUSE_RESUME called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid RESUME?
	if ( IsParameterValid ( RESUME, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "RESUME = %x not valid \n", RESUME ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PAUSE_RESUME,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								RESUME,
								CONTROL );

	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::PLAY_AUDIO_10
//
//----------------------------------------------------------------------
//
//		The PLAY AUDIO (10) command as defined in section 6.1.10.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::PLAY_AUDIO_10 (
			SCSITask *					request,
			SCSICmdField1Bit 			RELADR,
			SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
			SCSICmdField2Byte 			PLAY_LENGTH,
			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_AUDIO_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{
		
		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;
	
	}
	
	// did we receive a valid STARTING_LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "STARTING_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						STARTING_LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}
	
	// did we receive a valid PLAY_LENGTH?
	if ( IsParameterValid ( PLAY_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "PLAY_LENGTH = %x not valid \n", PLAY_LENGTH ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PLAY_AUDIO_10,
								RELADR,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								( PLAY_LENGTH >> 8 ) & 0xFF,
								  PLAY_LENGTH        & 0xFF,
								CONTROL );

	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::PLAY_AUDIO_12
//
//----------------------------------------------------------------------
//
//		The PLAY AUDIO (12) command as defined in section 6.1.11.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::PLAY_AUDIO_12 (
			SCSITask *					request,
			SCSICmdField1Bit 			RELADR,
			SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
			SCSICmdField4Byte 			PLAY_LENGTH,
			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_AUDIO_12 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{
		
		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;
		
	}
	
	// did we receive a valid STARTING_LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "STARTING_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						STARTING_LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid PLAY_LENGTH?
	if ( IsParameterValid ( PLAY_LENGTH,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "PLAY_LENGTH = %x not valid \n", PLAY_LENGTH ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PLAY_AUDIO_12,
								RELADR,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
								( PLAY_LENGTH >> 24 ) & 0xFF,
								( PLAY_LENGTH >> 16 ) & 0xFF,
								( PLAY_LENGTH >>  8 ) & 0xFF,
								  PLAY_LENGTH         & 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::PLAY_AUDIO_MSF
//
//----------------------------------------------------------------------
//
//		The PLAY AUDIO MSF command as defined in section 6.1.12.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::PLAY_AUDIO_MSF (
					SCSITask *					request,
					SCSICmdField3Byte 			STARTING_MSF,
					SCSICmdField3Byte 			ENDING_MSF,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_AUDIO_MSF called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid STARTING_MSF?
	if ( IsParameterValid ( STARTING_MSF,
							kSCSICmdFieldMask3Byte ) == false )
	{
		
		ERROR_LOG ( ( "STARTING_MSF = %x not valid \n",
						STARTING_MSF ) );
		return false;
		
	}
	
	if ( ( ( STARTING_MSF & 0xFF ) >= FRAMES_IN_A_SECOND )  ||
		 ( ( ( STARTING_MSF >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
	{
		
		if ( STARTING_MSF != 0xFFFFFF )
		{
			
			ERROR_LOG ( ( "STARTING_MSF = %x not valid \n",
							STARTING_MSF ) );
			return false;
		
		}
		
	}
	
	// did we receive a valid ENDING_MSF?
	if ( IsParameterValid ( ENDING_MSF,
							kSCSICmdFieldMask3Byte ) == false )
	{
		
		ERROR_LOG ( ( "ENDING_MSF = %x not valid \n", ENDING_MSF ) );
		return false;
		
	}
	
	if ( ( ( ENDING_MSF & 0xFF ) >= FRAMES_IN_A_SECOND )  ||
		 ( ( ( ENDING_MSF >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
	{
		
		ERROR_LOG ( ( "ENDING_MSF = %x not valid \n", ENDING_MSF ) );
		return false;
		
	}
	
	// did we receive compatible STARTING_MSF and ENDING_MSF?
	if ( STARTING_MSF > ENDING_MSF )
	{
		
		ERROR_LOG ( ( "STARTING_MSF > ENDING_MSF : %x, %x\n",
						STARTING_MSF, ENDING_MSF ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PLAY_AUDIO_MSF,
								0x00,
								0x00,
								( STARTING_MSF >> 16 )	& 0xFF,
								( STARTING_MSF >>  8 )	& 0xFF,
								  STARTING_MSF       	& 0xFF,
								( ENDING_MSF >> 16 )	& 0xFF,
								( ENDING_MSF >>  8 )	& 0xFF,
								  ENDING_MSF			& 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::PLAY_CD
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//
//		The PLAY CD command as defined in section 6.1.13. PLAY CD
//		is obsoleted by the MMC-2 specification.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::PLAY_CD (
			SCSITask *					request,
			SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
			SCSICmdField1Bit 			CMSF,
			SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS, 
			SCSICmdField4Byte 			PLAY_LENGTH_IN_BLOCKS, 
			SCSICmdField1Bit 			SPEED,
			SCSICmdField1Bit 			PORT2,
			SCSICmdField1Bit 			PORT1,
			SCSICmdField1Bit 			COMPOSITE,
			SCSICmdField1Bit 			AUDIO,
			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::PLAY_CD *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid EXPECTED_SECTOR_TYPE?
	if ( EXPECTED_SECTOR_TYPE > 5 )
	{
		
		ERROR_LOG ( ( "EXPECTED_SECTOR_TYPE = %x not valid \n",
						EXPECTED_SECTOR_TYPE ) );
		return false;
		
	}
	
	// Check if CMSF is set for LBA mode
	if ( CMSF == 0 )
	{
		
		STATUS_LOG ( ( "Using LBA Addressing Mode\n" ) );
		
		// did we receive a valid LBA STARTING_LOGICAL_BLOCK_ADDRESS?
		if ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
								kSCSICmdFieldMask4Byte ) == false )
		{
			
			ERROR_LOG ( ( "STARTING_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
							STARTING_LOGICAL_BLOCK_ADDRESS ) );
			return false;
			
		}
		
		// did we receive a valid LBA PLAY_LENGTH_IN_BLOCKS?
		if ( IsParameterValid ( PLAY_LENGTH_IN_BLOCKS,
								kSCSICmdFieldMask4Byte ) == false )
		{
			
			ERROR_LOG ( ( "PLAY_LENGTH_IN_BLOCKS = %x not valid \n",
							PLAY_LENGTH_IN_BLOCKS ) );
			return false;
			
		}
	
	}
	
	else
	{
		
		STATUS_LOG ( ( "Using MSF Addressing Mode\n" ) );
		
		// did we receive a valid MSF STARTING_LOGICAL_BLOCK_ADDRESS?
		if ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
								kSCSICmdFieldMask3Byte ) == false )
		{
			
			ERROR_LOG ( ( "STARTING_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
							STARTING_LOGICAL_BLOCK_ADDRESS ) );
			return false;
			
		}
		
		if ( ( ( STARTING_LOGICAL_BLOCK_ADDRESS & 0xFF ) >= FRAMES_IN_A_SECOND ) ||
			 ( ( ( STARTING_LOGICAL_BLOCK_ADDRESS >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
		{
			
			ERROR_LOG ( ( "STARTING_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
							STARTING_LOGICAL_BLOCK_ADDRESS ) );
			return false;
			
		}
		
		// since CMSF is set to 1, bytes 6-8 (the high three bytes of
		// PLAY_LENGTH_IN_BLOCKS) is where our end MSF address is.
		if ( IsParameterValid ( PLAY_LENGTH_IN_BLOCKS >> 8,
								kSCSICmdFieldMask3Byte ) == false )
		{
			
			ERROR_LOG ( ( "PLAY_LENGTH_IN_BLOCKS = %x not valid \n",
							PLAY_LENGTH_IN_BLOCKS ) );
			return false;
			
		}
		
		if ( ( ( PLAY_LENGTH_IN_BLOCKS >> 8 & 0xFF ) >= FRAMES_IN_A_SECOND ) ||
			 ( ( ( PLAY_LENGTH_IN_BLOCKS >> 16 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
		{
			
			ERROR_LOG ( ( "PLAY_LENGTH_IN_BLOCKS = %x not valid \n",
							PLAY_LENGTH_IN_BLOCKS ) );
			return false;
			
		}
		
	}

	// did we receive a valid SPEED?
	if ( IsParameterValid ( SPEED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "SPEED = %x not valid \n", SPEED ) );
		return false;
		
	}

	// did we receive a valid PORT2?
	if ( IsParameterValid ( PORT2, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "PORT2 = %x not valid \n", PORT2 ) );
		return false;
		
	}

	// did we receive a valid PORT1?
	if ( IsParameterValid ( PORT1, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "PORT1 = %x not valid \n", PORT1 ) );
		return false;
		
	}

	// did we receive a valid COMPOSITE?
	if ( IsParameterValid ( COMPOSITE, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "COMPOSITE = %x not valid \n", COMPOSITE ) );
		return false;
		
	}

	// did we receive a valid AUDIO?
	if ( IsParameterValid ( AUDIO, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "AUDIO = %x not valid \n", AUDIO ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	if ( CMSF == 0 )
	{
		
		SetCommandDescriptorBlock (	request,
									kSCSICmd_PLAY_CD,
									( EXPECTED_SECTOR_TYPE << 2 ) | ( CMSF << 1 ),
									( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
									( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
									( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
									  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 24 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 16 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >>  8 ) & 0xFF,
									  PLAY_LENGTH_IN_BLOCKS         & 0xFF,
									( SPEED << 7 ) | ( PORT2 << 3 ) | ( PORT1 << 2 ) | ( COMPOSITE << 1 ) | AUDIO,
									CONTROL );
		
	}
	
	else
	{
		
		// bytes 2 and 9 are reserved!
		SetCommandDescriptorBlock (	request,
									kSCSICmd_PLAY_CD,
									( EXPECTED_SECTOR_TYPE << 2 ) | ( CMSF << 1 ),
									0x00,
									( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
									( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
									  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 24 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >> 16 ) & 0xFF,
									( PLAY_LENGTH_IN_BLOCKS >>  8 ) & 0xFF,
									0x00,
									( SPEED << 7 ) | ( PORT2 << 3 ) | ( PORT1 << 2 ) | ( COMPOSITE << 1 ) | AUDIO,
									CONTROL );
		
	}
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::READ_BUFFER_CAPACITY
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The READ BUFFER CAPACITY command as defined in section 6.1.14.
//		READ BUFFER CAPACITY is obsoleted by the MMC-2 specification.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_BUFFER_CAPACITY (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_BUFFER_CAPACITY *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL field?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_BUFFER_CAPACITY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
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
//		SCSIMultimediaCommands::READ_CD
//
//----------------------------------------------------------------------
//
//		The READ CD command as defined in section 6.1.15.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_CD (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField3Byte 			TRANSFER_LENGTH,
				SCSICmdField1Bit 			SYNC,
				SCSICmdField2Bit 			HEADER_CODES,
				SCSICmdField1Bit 			USER_DATA,
				SCSICmdField1Bit 			EDC_ECC,
				SCSICmdField2Bit 			ERROR_FIELD,
				SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
				SCSICmdField1Byte 			CONTROL )
{
	
	UInt32			blockSize;
	UInt32 			requestedByteCount;
	bool			validBlockSize;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_CD called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{

		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;

	}

	// did we receive a valid STARTING_LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( STARTING_LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "STARTING_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						STARTING_LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid TRANSFER_LENGTH?
	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask3Byte ) == false )
	{
		
		ERROR_LOG ( ( "TRANSFER_LENGTH = %x not valid \n",
						TRANSFER_LENGTH ) );
		return false;
		
	}

	// did we receive a valid SUBCHANNEL_SELECTION_BITS?
	if ( IsParameterValid ( SUBCHANNEL_SELECTION_BITS,
							kSCSICmdFieldMask3Bit ) == false )
	{
		
		ERROR_LOG ( ( "SUBCHANNEL_SELECTION_BITS = %x not valid \n",
						SUBCHANNEL_SELECTION_BITS ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// determine the size of the transfer
	validBlockSize = GetBlockSize (	&blockSize,
									EXPECTED_SECTOR_TYPE,
									SYNC,
									HEADER_CODES,
									USER_DATA,
									EDC_ECC,
									ERROR_FIELD );
	
	if ( validBlockSize == false )
	{
		
		ERROR_LOG ( ( "blockSize = %x not valid \n", blockSize ) );
		return false;
		
	}
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	STATUS_LOG ( ( "blockSize = %ld\n", blockSize ) );
	STATUS_LOG ( ( "TRANSFER_LENGTH = %ld\n", TRANSFER_LENGTH ) );
	STATUS_LOG ( ( "requestedByteCount = %ld\n", requestedByteCount ) );

	if ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, requestedByteCount = %ld\n",
						dataBuffer, requestedByteCount ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CD,
								( EXPECTED_SECTOR_TYPE << 2 ) | RELADR,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( STARTING_LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  STARTING_LOGICAL_BLOCK_ADDRESS         & 0xFF,
								( TRANSFER_LENGTH >> 16 ) & 0xFF,
								( TRANSFER_LENGTH >>  8 ) & 0xFF,
								  TRANSFER_LENGTH         & 0xFF,
								( SYNC << 7 ) | ( HEADER_CODES << 5 ) | ( USER_DATA << 4 ) | ( EDC_ECC << 3 ) | ( ERROR_FIELD << 1 ),
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								requestedByteCount );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::READ_CD_MSF
//
//----------------------------------------------------------------------
//
//		The READ CD MSF command as defined in section 6.1.16.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_CD_MSF (
				SCSITask *					request,
				IOMemoryDescriptor *		dataBuffer,
				SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
				SCSICmdField3Byte 			STARTING_MSF,
				SCSICmdField3Byte 			ENDING_MSF,
				SCSICmdField1Bit 			SYNC,
				SCSICmdField2Bit 			HEADER_CODES,
				SCSICmdField1Bit 			USER_DATA,
				SCSICmdField1Bit 			EDC_ECC,
				SCSICmdField2Bit 			ERROR_FIELD,
				SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
				SCSICmdField1Byte 			CONTROL )
{
	
	UInt32			blockSize;
	UInt32 			requestedByteCount;
	bool			validBlockSize;

	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_CD_MSF called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid STARTING_MSF?
	if ( IsParameterValid ( STARTING_MSF,
							kSCSICmdFieldMask3Byte ) == false )
	{
		
		ERROR_LOG ( ( "STARTING_MSF = %x not valid \n",
						STARTING_MSF ) );
		return false;
		
	}
	
	if ( ( ( STARTING_MSF & 0xFF ) >= FRAMES_IN_A_SECOND ) ||
		 ( ( ( STARTING_MSF >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
	{
		
		ERROR_LOG ( ( "STARTING_MSF = %x not valid \n",
						STARTING_MSF ) );
		return false;
		
	}
	
	// did we receive a valid ENDING_MSF?
	if ( IsParameterValid ( ENDING_MSF,
							kSCSICmdFieldMask3Byte ) == false )
	{
		
		ERROR_LOG ( ( "ENDING_MSF = %x not valid \n",
						ENDING_MSF ) );
		return false;
		
	}
	
	if ( ( ( ENDING_MSF & 0xFF ) >= FRAMES_IN_A_SECOND ) ||
		 ( ( ( ENDING_MSF >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
	{
		
		ERROR_LOG ( ( "ENDING_MSF = %x not valid \n",
						ENDING_MSF ) );
		return false;
		
	}

	// did we receive compatible STARTING_MSF and ENDING_MSF?
	if ( STARTING_MSF > ENDING_MSF )
	{
		
		ERROR_LOG ( ( "STARTING_MSF > ENDING_MSF : %x %x\n",
						STARTING_MSF, ENDING_MSF ) );
		return false;
		
	}
	
	// did we receive a valid SUBCHANNEL_SELECTION_BITS?
	if ( IsParameterValid ( SUBCHANNEL_SELECTION_BITS,
							kSCSICmdFieldMask3Bit ) == false )
	{
		
		ERROR_LOG ( ( "SUBCHANNEL_SELECTION_BITS = %x not valid \n",
						SUBCHANNEL_SELECTION_BITS ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// determine the size of the transfer
	validBlockSize = GetBlockSize (	&blockSize,
									EXPECTED_SECTOR_TYPE,
									SYNC,
									HEADER_CODES,
									USER_DATA,
									EDC_ECC,
									ERROR_FIELD );
	
	if ( validBlockSize == false )
	{
		
		ERROR_LOG ( ( "blockSize = %x not valid \n", blockSize ) );
		return false;
		
	}
	
	requestedByteCount = ( ConvertMSFToLBA ( ENDING_MSF ) -
							ConvertMSFToLBA ( STARTING_MSF ) ) * blockSize;
	
	STATUS_LOG ( ( "requestedByteCount = %x\n", requestedByteCount ) );
	
	if ( IsBufferAndCapacityValid ( dataBuffer,
									requestedByteCount ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, requestedByteCount = %ld\n",
						dataBuffer, requestedByteCount ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CD_MSF,
								EXPECTED_SECTOR_TYPE << 2,
								0x00,
								( STARTING_MSF >> 16 )	& 0xFF,
								( STARTING_MSF >>  8 )	& 0xFF,
								  STARTING_MSF        	& 0xFF,
								( ENDING_MSF >> 16 )	& 0xFF,
								( ENDING_MSF >>  8 )	& 0xFF,
								  ENDING_MSF        	& 0xFF,
								( SYNC << 7 ) | ( HEADER_CODES << 5 ) | ( USER_DATA << 4 ) | ( EDC_ECC << 3 ) | ( ERROR_FIELD << 1 ),
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								requestedByteCount );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::READ_CAPACITY
//
//----------------------------------------------------------------------
//
//		The READ CAPACITY command as defined in section 6.1.17.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_CAPACITY (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField1Bit 			PMI,
					SCSICmdField1Byte 			CONTROL )
{

	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_CAPACITY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{
		
		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;
		
	}

	// did we receive a valid LOGICAL_BLOCK_ADDRESS?
	if ( LOGICAL_BLOCK_ADDRESS != 0 )
	{
		
		ERROR_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid PMI?
	if ( PMI != 0 )
	{
		
		ERROR_LOG ( ( "PMI = %x not valid \n", PMI ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									READ_CAPACITY_MAX_DATA ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, READ_CAPACITY_MAX_DATA = %ld\n",
						dataBuffer, READ_CAPACITY_MAX_DATA ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CAPACITY,
								RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								0x00,
								PMI,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								READ_CAPACITY_MAX_DATA );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::READ_DISC_INFORMATION
//
//----------------------------------------------------------------------
//
//		The READ DISC INFORMATION command as defined in section 6.1.18.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_DISC_INFORMATION (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_DISC_INFORMATION called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL field?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_DISC_INFORMATION,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
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
//		SCSIMultimediaCommands::READ_DVD_STRUCTURE
//
//----------------------------------------------------------------------
//
//		The READ DVD STRUCTURE command as defined in section 6.1.19.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_DVD_STRUCTURE (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField4Byte 			ADDRESS,
					SCSICmdField1Byte 			LAYER_NUMBER,
					SCSICmdField1Byte 			FORMAT,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField2Bit 			AGID,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_DVD_STRUCTURE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid ADDRESS?
	if ( IsParameterValid ( ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "ADDRESS = %x not valid \n", ADDRESS ) );
		return false;
		
	}

	// did we receive a valid LAYER_NUMBER?
	if ( IsParameterValid ( LAYER_NUMBER,
							kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "LAYER_NUMBER = %x not valid \n",
						LAYER_NUMBER ) );
		return false;
		
	}

	// did we receive a valid FORMAT?
	if ( IsParameterValid ( FORMAT, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "FORMAT = %x not valid \n", FORMAT ) );
		return false;
		
	}

	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid AGID?
	if ( IsParameterValid ( AGID, kSCSICmdFieldMask2Bit ) == false )
	{
		
		ERROR_LOG ( ( "AGID = %x not valid \n", AGID ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_DVD_STRUCTURE,
								0x00,
								( ADDRESS >> 24 ) & 0xFF,
								( ADDRESS >> 16 ) & 0xFF,
								( ADDRESS >>  8 ) & 0xFF,
								  ADDRESS         & 0xFF,
								LAYER_NUMBER,
								FORMAT,
								( ALLOCATION_LENGTH >> 8 )	& 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								AGID << 6,
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
//		SCSIMultimediaCommands::READ_FORMAT_CAPACITIES
//
//----------------------------------------------------------------------
//
//		The READ FORMAT CAPACITIES command as defined in section 6.1.20.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_FORMAT_CAPACITIES (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_FORMAT_CAPACITIES called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL field?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_FORMAT_CAPACITIES,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 )	& 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
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
//		SCSIMultimediaCommands::READ_HEADER
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The READ HEADER command as defined in section 6.1.21. READ HEADER
//		is obsoleted by the MMC-2 specification.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_HEADER (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			MSF,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_HEADER *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid MSF?
	if ( IsParameterValid ( MSF, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "MSF = %x not valid \n", MSF ) );
		return false;
		
	}

	// did we receive a valid LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_HEADER,
								MSF << 1,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								( ALLOCATION_LENGTH >>  8 ) & 0xFF,
								  ALLOCATION_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::READ_MASTER_CUE
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The READ MASTER CUE command as defined in section 6.1.22.
//		READ MASTER CUE is obsoleted by the MMC-2 specification.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_MASTER_CUE (
					SCSITask *					request,
    				IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Byte 			SHEET_NUMBER, 
					SCSICmdField3Byte 			ALLOCATION_LENGTH, 
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_MASTER_CUE *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid SHEET_NUMBER?
	if ( IsParameterValid ( SHEET_NUMBER,
							kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "SHEET_NUMBER = %x not valid \n",
						SHEET_NUMBER ) );
		return false;
		
	}

	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask3Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL field?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_MASTER_CUE,
								0x00,
								0x00,
								0x00,
								SHEET_NUMBER,
								0x00,
								( ALLOCATION_LENGTH >> 16 ) & 0xFF,
								( ALLOCATION_LENGTH >>  8 ) & 0xFF,
								  ALLOCATION_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::READ_SUB_CHANNEL
//
//----------------------------------------------------------------------
//
//		The READ SUB-CHANNEL command as defined in section 6.1.23.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_SUB_CHANNEL (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			MSF,
					SCSICmdField1Bit 			SUBQ,
					SCSICmdField1Byte 			SUB_CHANNEL_PARAMETER_LIST,
					SCSICmdField1Byte 			TRACK_NUMBER,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_SUB_CHANNEL called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid MSF?
	if ( IsParameterValid ( MSF, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "MSF = %x not valid \n", MSF ) );
		return false;
		
	}

	// did we receive a valid SUBQ?
	if ( IsParameterValid ( SUBQ, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "SUBQ = %x not valid \n", SUBQ ) );
		return false;
		
	}

	// did we receive a valid SUB_CHANNEL_PARAMETER_LIST?
	if ( IsParameterValid ( SUB_CHANNEL_PARAMETER_LIST,
							kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "SUB_CHANNEL_PARAMETER_LIST = %x not valid \n",
						SUB_CHANNEL_PARAMETER_LIST ) );
		return false;
		
	}

	// did we receive a valid TRACK_NUMBER?
	if ( SUB_CHANNEL_PARAMETER_LIST == 3 )
	{
		
		if ( TRACK_NUMBER > MAX_TRACK_NUMBER )
		{
			
			ERROR_LOG ( ( "TRACK_NUMBER = %x not valid \n",
							TRACK_NUMBER ) );
			return false;
			
		}
		
	}

	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_SUB_CHANNEL,
								MSF << 1,
								SUBQ << 6,
								SUB_CHANNEL_PARAMETER_LIST,
								0x00,
								0x00,
								( SUB_CHANNEL_PARAMETER_LIST == 3 ) ? TRACK_NUMBER : 0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
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
//		SCSIMultimediaCommands::READ_TOC_PMA_ATIP
//
//----------------------------------------------------------------------
//
//		The READ TOC/PMA/ATIP command as defined in section 6.1.24/25.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_TOC_PMA_ATIP (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			MSF,
					SCSICmdField4Bit 			FORMAT,
					SCSICmdField1Byte			TRACK_SESSION_NUMBER,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
		
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_TOC_PMA_ATIP called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid MSF parameter?
	if ( IsParameterValid ( MSF, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "MSF = %x not valid \n", MSF ) );
		return false;
		
	}
	
	// did we receive a valid FORMAT parameter?
	if ( ( FORMAT & kSCSICmdFieldMask4Bit ) > 5 )
	{
		
		ERROR_LOG ( ( "FORMAT = %x not valid \n", FORMAT ) );
		return false;
		
	}
	
	// did we receive a valid TRACK_SESSION_NUMBER?
	if ( IsParameterValid ( TRACK_SESSION_NUMBER,
							kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "TRACK_SESSION_NUMBER = %x not valid \n",
						TRACK_SESSION_NUMBER ) );
		return false;
		
	}

	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL field?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// Should we use ╥old-style╙ ATAPI SFF-8020i way?
	if ( FORMAT <= 0x03 )
	{
		
		// Use the ATAPI-SFF 8020i "old style" way of issuing READ_TOC_PMA_ATIP
		
		// fill out the cdb appropriately  
		SetCommandDescriptorBlock (	request,
									kSCSICmd_READ_TOC_PMA_ATIP,
									MSF << 1,
									0x00,
									0x00,
									0x00,
									0x00,
									TRACK_SESSION_NUMBER,
									( ALLOCATION_LENGTH >> 8 ) & 0xFF,
									  ALLOCATION_LENGTH        & 0xFF,
									( FORMAT & 0x03 ) << 6 );
		
	}
	
	else
	{
		
		// Use the MMC-2 "new style" way of issuing READ_TOC_PMA_ATIP
		
		// fill out the cdb appropriately  
		SetCommandDescriptorBlock (	request,
									kSCSICmd_READ_TOC_PMA_ATIP,
									MSF << 1,
									FORMAT,
									0x00,
									0x00,
									0x00,
									TRACK_SESSION_NUMBER,
									( ALLOCATION_LENGTH >> 8 ) & 0xFF,
									ALLOCATION_LENGTH        & 0xFF,
									CONTROL );
		
	}
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::READ_TRACK_INFORMATION
//
//----------------------------------------------------------------------
//
//		The READ TRACK INFORMATION command as defined in section 6.1.26.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::READ_TRACK_INFORMATION (
			SCSITask *					request,
			IOMemoryDescriptor *		dataBuffer,
			SCSICmdField2Bit 			ADDRESS_NUMBER_TYPE,
			SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
			SCSICmdField2Byte 			ALLOCATION_LENGTH,
			SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::READ_TRACK_INFORMATION called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid ADDRESS_NUMBER_TYPE?
	if ( IsParameterValid ( ADDRESS_NUMBER_TYPE,
							kSCSICmdFieldMask2Bit ) == false )
	{
		
		ERROR_LOG ( ( "ADDRESS_NUMBER_TYPE = %x not valid \n",
						ADDRESS_NUMBER_TYPE ) );
		return false;
		
	}
	
	// did we receive a valid LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER?
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER = %x not valid \n",
						LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER ) );
		return false;
		
	}
	
	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// is the buffer large enough to accomodate this request?
	if ( IsBufferAndCapacityValid ( dataBuffer,
									ALLOCATION_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_TRACK_INFORMATION,
								ADDRESS_NUMBER_TYPE,
								( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER         & 0xFF,
								0x00,
								( ALLOCATION_LENGTH >>  8 ) & 0xFF,
								  ALLOCATION_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::REPAIR_TRACK
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The REPAIR TRACK command as defined in section 6.1.27.
//		REPAIR TRACK is obsoleted by the MMC-2 specification.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::REPAIR_TRACK (
					SCSITask *					request,
					SCSICmdField2Byte 			TRACK_NUMBER,
					SCSICmdField1Byte 			CONTROL )

{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::REPAIR_TRACK *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid TRACK_NUMBER?
	if ( IsParameterValid ( TRACK_NUMBER,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "TRACK_NUMBER = %x not valid \n",
						TRACK_NUMBER ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPAIR_TRACK,
								0x00,
								0x00,
								0x00,
								( TRACK_NUMBER >>  8 ) & 0xFF,
								  TRACK_NUMBER         & 0xFF,
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
//		SCSIMultimediaCommands::REPORT_KEY
//
//----------------------------------------------------------------------
//
//		The REPORT KEY command as defined in section 6.1.28.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::REPORT_KEY (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			ALLOCATION_LENGTH,
					SCSICmdField2Bit 			AGID,
					SCSICmdField6Bit 			KEY_FORMAT,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::REPORT_KEY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}
	
	// did we receive a valid ALLOCATION_LENGTH?
	if ( IsParameterValid ( ALLOCATION_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "ALLOCATION_LENGTH = %x not valid \n",
						ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	// did we receive a valid AGID?
	if ( IsParameterValid ( AGID, kSCSICmdFieldMask2Bit ) == false )
	{
		
		ERROR_LOG ( ( "AGID = %x not valid \n", AGID ) );
		return false;
		
	}
	
	// did we receive a valid KEY_FORMAT?
	if ( IsParameterValid ( KEY_FORMAT,
							kSCSICmdFieldMask6Bit ) == false )
	{
		
		ERROR_LOG ( ( "KEY_FORMAT = %x not valid \n", KEY_FORMAT ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// Only check the buffer if the key format is not INVALIDATE_AGID
	if ( KEY_FORMAT != 0x3F )
	{
		
		// is the buffer large enough to accomodate this request?
		if ( IsBufferAndCapacityValid ( dataBuffer,
										ALLOCATION_LENGTH ) == false )
		{
			
			ERROR_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %ld\n",
							dataBuffer, ALLOCATION_LENGTH ) );
			return false;
			
		}
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_FromTargetToInitiator,
									dataBuffer,
									ALLOCATION_LENGTH );	
		
	}
	
	else
	{
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_NoDataTransfer );
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REPORT_KEY,
								0x00,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS         & 0xFF,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH        & 0xFF,
								( AGID << 6 ) | KEY_FORMAT,
								CONTROL );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::RESERVE_TRACK
//
//----------------------------------------------------------------------
//
//		The RESERVE TRACK command as defined in section 6.1.29.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::RESERVE_TRACK (
					SCSITask *					request,
					SCSICmdField4Byte			RESERVATION_SIZE,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::RESERVE_TRACK called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid RESERVATION_SIZE?
	if ( IsParameterValid ( RESERVATION_SIZE,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "RESERVATION_SIZE = %x not valid \n",
						RESERVATION_SIZE ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_RESERVE_TRACK,
								0x00,
								0x00,
								0x00,
								0x00,
								( RESERVATION_SIZE >> 24 ) & 0xFF,
								( RESERVATION_SIZE >> 16 ) & 0xFF,
								( RESERVATION_SIZE >>  8 ) & 0xFF,
								  RESERVATION_SIZE         & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::SCAN
//
//----------------------------------------------------------------------
//
//		The SCAN command as defined in section 6.1.30.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SCAN (
				SCSITask *					request,
				SCSICmdField1Bit 			DIRECT,
				SCSICmdField1Bit 			RELADR,
				SCSICmdField4Byte			SCAN_STARTING_ADDRESS_FIELD,
				SCSICmdField2Bit 			TYPE,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SCAN called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid DIRECT?
	if ( IsParameterValid ( DIRECT, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "DIRECT = %x not valid \n", DIRECT ) );
		return false;
		
	}
	
	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{
		
		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;
		
	}
	
	// did we receive a valid TYPE?
	switch ( TYPE )
	{
		
		// LBA
		case	0:
		{
			
			STATUS_LOG ( ( "Using LBA TYPE\n" ) );
			
			// did we receive a valid LBA SCAN_STARTING_ADDRESS_FIELD?
			if ( IsParameterValid ( SCAN_STARTING_ADDRESS_FIELD,
									kSCSICmdFieldMask4Byte ) == false )
			{
				
				ERROR_LOG ( ( "SCAN_STARTING_ADDRESS_FIELD = %x not valid \n",
								SCAN_STARTING_ADDRESS_FIELD ) );
				return false;
				
			}
			
			break;
			
		}
		
		// MSF
		case	1:
		{
			
			STATUS_LOG ( ( "Using MSF TYPE\n" ) );
			
			// did we receive a valid MSF SCAN_STARTING_ADDRESS_FIELD?
			if ( IsParameterValid ( SCAN_STARTING_ADDRESS_FIELD,
									kSCSICmdFieldMask3Byte ) == false )
			{
				
				ERROR_LOG ( ( "SCAN_STARTING_ADDRESS_FIELD = %x not valid \n",
								SCAN_STARTING_ADDRESS_FIELD ) );
				return false;
				
			}
			
			if ( ( ( SCAN_STARTING_ADDRESS_FIELD & 0xFF ) >= FRAMES_IN_A_SECOND ) ||
				 ( ( ( SCAN_STARTING_ADDRESS_FIELD >> 8 ) & 0xFF ) >= SECONDS_IN_A_MINUTE ) )
			{
				
				ERROR_LOG ( ( "SCAN_STARTING_ADDRESS_FIELD = %x not valid \n",
								SCAN_STARTING_ADDRESS_FIELD ) );
				return false;
				
			}
			break;
			
		}
		
		// track number
		case	2:
		{
			
			STATUS_LOG ( ( "Using Track Number TYPE\n" ) );
			
			// did we receive a valid track SCAN_STARTING_ADDRESS_FIELD?
			if ( SCAN_STARTING_ADDRESS_FIELD > MAX_TRACK_NUMBER )
			{
				
				ERROR_LOG ( ( "SCAN_STARTING_ADDRESS_FIELD = %x not valid \n",
								SCAN_STARTING_ADDRESS_FIELD ) );
				return false;
				
			}
			
			break;
			
		}
		
		// invalid TYPE
		default:
		{
			
			ERROR_LOG ( ( "TYPE = %x not valid \n", TYPE ) );
			return false;
			break;
			
		}
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SCAN_MMC,
								( DIRECT << 4 ) | RELADR,
								( SCAN_STARTING_ADDRESS_FIELD >> 24 ) & 0xFF,
								( SCAN_STARTING_ADDRESS_FIELD >> 16 ) & 0xFF,
								( SCAN_STARTING_ADDRESS_FIELD >>  8 ) & 0xFF,
								  SCAN_STARTING_ADDRESS_FIELD         & 0xFF,
								0x00,
								0x00,
								0x00,
								TYPE << 6,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::SEND_CUE_SHEET
//
//----------------------------------------------------------------------
//
//		The SEND CUE SHEET command as defined in section 6.1.31.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SEND_CUE_SHEET (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField3Byte			CUE_SHEET_SIZE,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_CUE_SHEET called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid CUE_SHEET_SIZE?
	if ( IsParameterValid ( CUE_SHEET_SIZE,
							kSCSICmdFieldMask3Byte ) == false )
	{
		
		ERROR_LOG ( ( "CUE_SHEET_SIZE = %x not valid \n",
						CUE_SHEET_SIZE ) );
		return false;
		
	}
	
	if ( CUE_SHEET_SIZE == 0 )
	{
		
		ERROR_LOG ( ( "CUE_SHEET_SIZE = %x not valid \n", CUE_SHEET_SIZE ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// is the buffer large enough to accomodate this request?
	if ( IsBufferAndCapacityValid ( dataBuffer,
									CUE_SHEET_SIZE ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, CUE_SHEET_SIZE = %ld\n",
						dataBuffer, CUE_SHEET_SIZE ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_CUE_SHEET,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( CUE_SHEET_SIZE >> 16 ) & 0xFF,
								( CUE_SHEET_SIZE >>  8 ) & 0xFF,
								  CUE_SHEET_SIZE         & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								CUE_SHEET_SIZE );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::SEND_DVD_STRUCTURE
//
//----------------------------------------------------------------------
//
//		The SEND DVD STRUCTURE command as defined in section 6.1.32.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SEND_DVD_STRUCTURE (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Byte			FORMAT,
					SCSICmdField2Byte 			STRUCTURE_DATA_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_DVD_STRUCTURE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid FORMAT?
	if ( IsParameterValid ( FORMAT, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "FORMAT = %x not valid \n", FORMAT ) );
		return false;
		
	}

	// did we receive a valid STRUCTURE_DATA_LENGTH?
	if ( IsParameterValid ( STRUCTURE_DATA_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "STRUCTURE_DATA_LENGTH = %x not valid \n",
						STRUCTURE_DATA_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// is the buffer large enough to accomodate this request?
	if ( IsBufferAndCapacityValid ( dataBuffer,
									STRUCTURE_DATA_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, STRUCTURE_DATA_LENGTH = %ld\n",
						dataBuffer, STRUCTURE_DATA_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_DVD_STRUCTURE,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								FORMAT,
								( STRUCTURE_DATA_LENGTH >>  8 ) & 0xFF,
								  STRUCTURE_DATA_LENGTH         & 0xFF,
								0x00,
								CONTROL );
	
	if ( STRUCTURE_DATA_LENGTH == 0 )
	{
		
		// No DVD structure is being sent to the device, so there
		// will be no data transfer for this request.
		SetDataTransferControl ( 	request,
									0,
									kSCSIDataTransfer_NoDataTransfer );
		
	}
	
	else
	{
		
		// The client has requested a DVD structure be sent to the device
		// to be used with the format command
		SetDataTransferControl ( 	request,
	                           		0,
									kSCSIDataTransfer_FromInitiatorToTarget,
									dataBuffer,
									STRUCTURE_DATA_LENGTH );
		
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::SEND_EVENT
//
//----------------------------------------------------------------------
//
//		The SEND EVENT command as defined in section 6.1.33.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SEND_EVENT (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			IMMED,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField1Byte 			CONTROL )

{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_EVENT called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid IMMED?
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "IMMED = %x not valid \n", IMMED ) );
		return false;
		
	}

	// did we receive a valid PARAMETER_LIST_LENGTH?
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid \n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// is the buffer large enough to accomodate this request?
	if ( IsBufferAndCapacityValid ( dataBuffer,
									PARAMETER_LIST_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %ld\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_EVENT,
								IMMED,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >>  8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::SEND_KEY
//
//----------------------------------------------------------------------
//
//		The SEND KEY command as defined in section 6.1.34.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SEND_KEY (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField2Bit 			AGID,
					SCSICmdField6Bit 			KEY_FORMAT,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_KEY called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid \n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// did we receive a valid AGID?
	if ( IsParameterValid ( AGID, kSCSICmdFieldMask2Bit ) == false )
	{
		
		ERROR_LOG ( ( "AGID = %x not valid \n", AGID ) );
		return false;
		
	}
	
	// did we receive a valid KEY_FORMAT?
	if ( IsParameterValid ( KEY_FORMAT, kSCSICmdFieldMask6Bit ) == false )
	{
		
		ERROR_LOG ( ( "KEY_FORMAT = %x not valid \n", KEY_FORMAT ) );
		return false;
		
	}
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// Only check the buffer if the key format is not INVALIDATE_AGID
	if ( KEY_FORMAT != 0x3F )
	{
		
		// is the buffer large enough to accomodate this request?
		if ( IsBufferAndCapacityValid ( dataBuffer,
										PARAMETER_LIST_LENGTH ) == false )
		{
			
			ERROR_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %ld\n",
							dataBuffer, PARAMETER_LIST_LENGTH ) );
			return false;
			
		}
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_FromInitiatorToTarget,
									dataBuffer,
									PARAMETER_LIST_LENGTH );	
		
	}
	
	else
	{
		
		SetDataTransferControl (	request,
									0,
									kSCSIDataTransfer_NoDataTransfer );
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_KEY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH        & 0xFF,
								( AGID << 6 ) | KEY_FORMAT,
								CONTROL );
		
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::SEND_OPC_INFORMATION
//
//----------------------------------------------------------------------
//
//		The SEND OPC INFORMATION command as defined in section 6.1.35.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SEND_OPC_INFORMATION (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField1Bit 			DO_OPC,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField1Byte 			CONTROL )

{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SEND_OPC_INFORMATION called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid DO_OPC?
	if ( IsParameterValid ( DO_OPC, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "DO_OPC = %x not valid \n", DO_OPC ) );
		return false;
		
	}

	if ( IsParameterValid ( PARAMETER_LIST_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid \n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// is the buffer large enough to accomodate this request?
	if ( IsBufferAndCapacityValid ( dataBuffer,
									PARAMETER_LIST_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %ld\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_OPC_INFORMATION,
								DO_OPC,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >>  8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::SET_CD_SPEED
//
//----------------------------------------------------------------------
//		
//	еее OBSOLETE еее
//		
//		The SET CD SPEED command as defined in section 6.1.36.
//		SET CD SPEED is obsoleted by the MMC-2 specification.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SET_CD_SPEED (
				SCSITask *					request,
				SCSICmdField2Byte 			LOGICAL_UNIT_READ_SPEED,
				SCSICmdField2Byte 			LOGICAL_UNIT_WRITE_SPEED,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SET_CD_SPEED *OBSOLETE* called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid LOGICAL_UNIT_READ_SPEED?
	if ( IsParameterValid ( LOGICAL_UNIT_READ_SPEED,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "LOGICAL_UNIT_READ_SPEED = %x not valid \n",
						LOGICAL_UNIT_READ_SPEED ) );
		return false;
		
	}

	// did we receive a valid LOGICAL_UNIT_WRITE_SPEED?
	if ( IsParameterValid ( LOGICAL_UNIT_WRITE_SPEED,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "LOGICAL_UNIT_WRITE_SPEED = %x not valid \n",
						LOGICAL_UNIT_WRITE_SPEED ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_CD_SPEED,
								0x00,
								( LOGICAL_UNIT_READ_SPEED >>   8 )	& 0xFF,
								  LOGICAL_UNIT_READ_SPEED			& 0xFF,
								( LOGICAL_UNIT_WRITE_SPEED >>  8 )	& 0xFF,
								  LOGICAL_UNIT_WRITE_SPEED			& 0xFF,
								0x00,
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
//		SCSIMultimediaCommands::SET_READ_AHEAD
//
//----------------------------------------------------------------------
//
//		The SET READ AHEAD command as defined in section 6.1.37.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SET_READ_AHEAD (
				SCSITask *					request,
				SCSICmdField4Byte 			TRIGGER_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField4Byte 			READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SET_READ_AHEAD called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid TRIGGER_LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( TRIGGER_LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "TRIGGER_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						TRIGGER_LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid READ_AHEAD_LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "READ_AHEAD_LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						READ_AHEAD_LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_READ_AHEAD,
								0x00,
								( TRIGGER_LOGICAL_BLOCK_ADDRESS >> 24 )		& 0xFF,
								( TRIGGER_LOGICAL_BLOCK_ADDRESS >> 16 )		& 0xFF,
								( TRIGGER_LOGICAL_BLOCK_ADDRESS >>  8 )		& 0xFF,
								  TRIGGER_LOGICAL_BLOCK_ADDRESS				& 0xFF,
								( READ_AHEAD_LOGICAL_BLOCK_ADDRESS >> 24 )	& 0xFF,
								( READ_AHEAD_LOGICAL_BLOCK_ADDRESS >> 16 )	& 0xFF,
								( READ_AHEAD_LOGICAL_BLOCK_ADDRESS >>  8 )	& 0xFF,
								  READ_AHEAD_LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								CONTROL );

	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::SET_STREAMING
//
//----------------------------------------------------------------------
//
//		The SET STREAMING command as defined in section 6.1.38.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SET_STREAMING (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer,
					SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SET_STREAMING called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	if ( IsParameterValid ( PARAMETER_LIST_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "PARAMETER_LIST_LENGTH = %x not valid \n",
						PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// is the buffer large enough to accomodate this request?
	if ( IsBufferAndCapacityValid ( dataBuffer,
									PARAMETER_LIST_LENGTH ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %ld\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SET_STREAMING,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >>  8 ) & 0xFF,
								  PARAMETER_LIST_LENGTH         & 0xFF,
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
//		SCSIMultimediaCommands::STOP_PLAY_SCAN
//
//----------------------------------------------------------------------
//
//		The STOP PLAY/SCAN command as defined in section 6.1.39.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::STOP_PLAY_SCAN (
					SCSITask *					request,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::STOP_PLAY_SCAN called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_STOP_PLAY_SCAN,
								0x00,
								0x00,
								0x00,
								0x00,
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
//		SCSIMultimediaCommands::SYNCHRONIZE_CACHE
//
//----------------------------------------------------------------------
//
//		The SYNCHRONIZE CACHE command as defined in section 6.1.40.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::SYNCHRONIZE_CACHE (
					SCSITask *					request,
					SCSICmdField1Bit 			IMMED,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
					SCSICmdField1Byte 			CONTROL )
{
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::SYNCHRONIZE_CACHE called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// did we receive a valid IMMED?
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "IMMED = %x not valid \n", IMMED ) );
		return false;
		
	}

	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{
		
		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;
		
	}

	// did we receive a valid LOGICAL_BLOCK_ADDRESS?
	if ( LOGICAL_BLOCK_ADDRESS != 0 )
	{
		
		ERROR_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid NUMBER_OF_BLOCKS?
	if ( NUMBER_OF_BLOCKS != 0 )
	{
		
		ERROR_LOG ( ( "NUMBER_OF_BLOCKS = %x not valid \n",
						NUMBER_OF_BLOCKS ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SYNCHRONIZE_CACHE,
								( IMMED << 1 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >>  24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>   8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS          & 0xFF,
								0x00,
								( NUMBER_OF_BLOCKS >>   8 ) & 0xFF,
								  NUMBER_OF_BLOCKS          & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
								0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::WRITE_10
//
//----------------------------------------------------------------------
//
//		The WRITE (10) command as defined in section 6.1.41.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::WRITE_10 (
					SCSITask *					request,
	    			IOMemoryDescriptor *		dataBuffer, 
	    			UInt32						blockSize,
					SCSICmdField1Bit 			DPO,
					SCSICmdField1Bit 			FUA,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 			TRANSFER_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	UInt32 		requestedByteCount;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::WRITE_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Make sure that we were given a valid blockSize
	if ( blockSize == 0 )
	{
		
		ERROR_LOG ( ( "blockSize = %x not valid \n", blockSize ) );
		return false;
		
	}
	
	// did we receive a valid DPO?
	if ( DPO != 0 )
	{
		
		ERROR_LOG ( ( "DPO = %x not valid \n", DPO ) );
		return false;
		
	}

	// did we receive a valid FUA?
	if ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "FUA = %x not valid \n", FUA ) );
		return false;
		
	}

	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{
		
		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;
		
	}

	// did we receive a valid LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid TANSFER_LENGTH?
	if ( IsParameterValid ( TRANSFER_LENGTH,
							kSCSICmdFieldMask2Byte ) == false )
	{
		
		ERROR_LOG ( ( "TRANSFER_LENGTH = %x not valid \n",
						TRANSFER_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// is the buffer large enough to accomodate this request?
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	if ( IsBufferAndCapacityValid ( dataBuffer,
									requestedByteCount ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, requestedByteCount = %ld\n",
						dataBuffer, requestedByteCount ) );
		return false;
		
	}

	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_10,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >>  24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>   8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS          & 0xFF,
								0x00,
 								( TRANSFER_LENGTH >>  8 ) & 0xFF,
								  TRANSFER_LENGTH         & 0xFF,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								requestedByteCount );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::WRITE_AND_VERIFY_10
//
//----------------------------------------------------------------------
//
//		The WRITE AND VERIFY (10) command as defined in section 6.1.42.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::WRITE_AND_VERIFY_10 (
					SCSITask *					request,
					IOMemoryDescriptor *		dataBuffer,
	    			UInt32						blockSize,
					SCSICmdField1Bit 			DPO,
					SCSICmdField1Bit 			BYT_CHK,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
					SCSICmdField4Byte 			TRANSFER_LENGTH,
					SCSICmdField1Byte 			CONTROL )
{
	
	UInt32 		requestedByteCount;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::WRITE_AND_VERIFY_10 called\n" ) );
	DEBUG_ASSERT ( ( request != NULL ) );
	
	// Make sure that we were given a valid blockSize
	if ( blockSize == 0 )
	{
		
		ERROR_LOG ( ( "blockSize = %x not valid \n", blockSize ) );
		return false;
		
	}
	
	// did we receive a valid DPO?
	if ( DPO != 0 )
	{
		
		ERROR_LOG ( ( "DPO = %x not valid \n", DPO ) );
		return false;
		
	}

	// did we receive a valid BYT_CHK?
	if ( BYT_CHK != 0 )
	{
		
		ERROR_LOG ( ( "BYT_CHK = %x not valid \n", BYT_CHK ) );
		return false;
		
	}

	// did we receive a valid RELADR?
	if ( RELADR != 0 )
	{
		
		ERROR_LOG ( ( "RELADR = %x not valid \n", RELADR ) );
		return false;
		
	}

	// did we receive a valid LOGICAL_BLOCK_ADDRESS?
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid \n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	// did we receive a valid TANSFER_LENGTH?
	if ( IsParameterValid ( TRANSFER_LENGTH,
							kSCSICmdFieldMask4Byte ) == false )
	{
		
		ERROR_LOG ( ( "TRANSFER_LENGTH = %x not valid \n",
						TRANSFER_LENGTH ) );
		return false;
		
	}

	// did we receive a valid CONTROL?
	if ( IsParameterValid ( CONTROL, kSCSICmdFieldMask1Byte ) == false )
	{
		
		ERROR_LOG ( ( "CONTROL = %x not valid \n", CONTROL ) );
		return false;
		
	}

	// is the buffer large enough to accomodate this request?
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	if ( IsBufferAndCapacityValid ( dataBuffer,
									requestedByteCount ) == false )
	{
		
		ERROR_LOG ( ( "dataBuffer = %x not valid, requestedByteCount = %ld\n",
						dataBuffer, requestedByteCount ) );
		return false;
		
	}
	
	// fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_AND_VERIFY_10,
								( DPO << 4 ) | ( BYT_CHK << 1 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >>  24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>  16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >>   8 ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS          & 0xFF,
								( TRANSFER_LENGTH >>  24 ) & 0xFF,
								( TRANSFER_LENGTH >>  16 ) & 0xFF,
								( TRANSFER_LENGTH >>   8 ) & 0xFF,
								  TRANSFER_LENGTH          & 0xFF,
								0x00,
								CONTROL );
	
	SetDataTransferControl ( 	request,
                           		0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								requestedByteCount );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::GetBlockSize
//
//----------------------------------------------------------------------
//
//		The block size decoding for Read CD and Read CD MSF as
//		defined in table 255.
//
//----------------------------------------------------------------------

bool
SCSIMultimediaCommands::GetBlockSize (
					UInt32 *					requestedByteCount,
					SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
					SCSICmdField1Bit 			SYNC,
					SCSICmdField2Bit 			HEADER_CODES,
					SCSICmdField1Bit 			USER_DATA,
					SCSICmdField1Bit 			EDC_ECC,
					SCSICmdField2Bit 			ERROR_FIELD )
{
	
	UInt32			userDataSize	= 0;
	UInt32			edcEccSize		= 0;
	UInt32			headerSize		= 0;
	UInt32			subHeaderSize	= 0;
	UInt32			syncSize		= 0;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::GetBlockSize called\n" ) );
	DEBUG_ASSERT ( ( requestedByteCount != NULL ) );
	
	// did we receive a valid EXPECTED_SECTOR_TYPE?
	if ( EXPECTED_SECTOR_TYPE > 5 )
	{
		
		ERROR_LOG ( ( "EXPECTED_SECTOR_TYPE = %x not valid \n",
						EXPECTED_SECTOR_TYPE ) );
		return false;
	
	}

	// did we receive a valid SYNC?
	if ( IsParameterValid ( SYNC, kSCSICmdFieldMask1Bit ) == false )
	{
		
		ERROR_LOG ( ( "SYNC = %x not valid \n", SYNC ) );
		return false;
		
	}

	// did we receive a valid HEADER_CODES?
	if ( IsParameterValid ( HEADER_CODES,
							kSCSICmdFieldMask2Bit ) == false )
	{
	
		ERROR_LOG ( ( "HEADER_CODES = %x not valid \n",
						HEADER_CODES ) );
		return false;
		
	}

	// did we receive a valid USER_DATA?
	if ( IsParameterValid ( USER_DATA,
							kSCSICmdFieldMask1Bit ) == false )
	{
	
		ERROR_LOG ( ( "USER_DATA = %x not valid \n", USER_DATA ) );
		return false;
		
	}

	// did we receive a valid EDC_ECC?
	if ( IsParameterValid ( EDC_ECC, kSCSICmdFieldMask1Bit ) == false )
	{

		ERROR_LOG ( ( "EDC_ECC = %x not valid \n", EDC_ECC ) );
		return false;

	}

	// did we receive a valid ERROR_FIELD?
	if ( IsParameterValid ( ERROR_FIELD,
							kSCSICmdFieldMask2Bit ) == false )
	{

		ERROR_LOG ( ( "ERROR_FIELD = %x not valid \n",
						ERROR_FIELD ) );
		return false;

	}

	// valid flag combination?
	switch ( ( SYNC << 4 ) | ( HEADER_CODES << 2 ) | ( USER_DATA << 1 ) |  EDC_ECC )
	{
		
		// invalid flag combinations
		case	0x00:		// nothing
		case	0x01:		// EDC_ECC
		case	0x05:		// HEADER + EDC_ECC
		case	0x09:		// SUB-HEADER + EDC_ECC
		case	0x0D:		// SUB-HEADER + HEADER + EDC_ECC
		case	0x10:		// SYNC
		case	0x11:		// SYNC + EDC_ECC
		case	0x12:		// SYNC + USER_DATA
		case	0x13:		// SYNC + USER_DATA + EDC_ECC
		case	0x15:		// SYNC + HEADER + EDC_ECC
		case	0x18:		// SYNC + SUB-HEADER
		case	0x19:		// SYNC + SUB-HEADER + EDC_ECC
		case	0x1A:		// SYNC + SUB-HEADER + USER_DATA
		case	0x1B:		// SYNC + SUB-HEADER + USER_DATA + EDC_ECC
		case	0x1D:		// SYNC + SUB-HEADER + HEADER + EDC_ECC
		{
		
			ERROR_LOG ( ( "invalid flag combo\n" ) );
			return false;
			
		}
		
		case	0x02:		// USER_DATA
		case	0x03:		// USER_DATA + EDC_ECC
		case	0x04:		// HEADER
		case	0x08:		// SUB-HEADER
		case	0x0A:		// SUB-HEADER + USER_DATA
		case	0x0B:		// SUB-HEADER + USER_DATA + EDC_ECC
		case	0x0C:		// SUB-HEADER + HEADER
		case	0x0E:		// SUB-HEADER + HEADER + USER_DATA
		case	0x0F:		// SUB-HEADER + HEADER + USER_DATA + EDC_ECC
		case	0x14:		// SYNC + HEADER
		case	0x1E:		// SYNC + SUB-HEADER + HEADER + USER_DATA
		case	0x1F:		// SYNC + SUB-HEADER + HEADER + USER_DATA + EDC_ECC
		{
			
			// legal combination
			break;
			
		}
		
		case	0x06:		// HEADER + USER_DATA
		case	0x07:		// HEADER + USER_DATA + EDC_ECC
		case	0x16:		// SYNC + HEADER + USER_DATA
		case	0x17:		// SYNC + HEADER + USER_DATA + EDC_ECC
		case	0x1C:		// SYNC + SUB-HEADER + HEADER
		{
			
			// illegal for mode 2, form 1 and mode 2, form 2 sectors
			if ( EXPECTED_SECTOR_TYPE > 3 )
			{
				
				ERROR_LOG ( ( "invalid flag combo for mode 2, form 1 and mode 2 form 2\n" ) );
				return false;
			
			}
			
			break;
			
		}
		
		default:
		{
			return false;
		}
		
	}

	headerSize	= 4;
	syncSize	= 12;

	switch ( EXPECTED_SECTOR_TYPE )
	{
		
		case	0:		// all types
		case	1:		// CD-DA
		{
			break;
		}
		
		case	2:		// mode 1
		{
			userDataSize	= 2048;
			edcEccSize		= 288;
			subHeaderSize	= 0;
			break;
		}
		
		case	3:		// mode 2, formless
		{
			userDataSize	= 2048 + 288;
			edcEccSize		= 0;
			subHeaderSize	= 0;
			break;
		}
		
		case	4:		// mode 2, form 1
		{
			userDataSize	= 2048;
			edcEccSize		= 280;
			subHeaderSize	= 8;
			break;
		}
		
		case	5:		// mode 2, form 2
		{
			userDataSize	= 2048 + 280;
			edcEccSize		= 0;
			subHeaderSize	= 8;
			break;
		}
		
		default:
		{
			return false;
		}
		
	}

	if ( ( EXPECTED_SECTOR_TYPE == 0 ) || ( EXPECTED_SECTOR_TYPE == 1 ) )
	{
		
		*requestedByteCount = 2352;
		
	}
	
	else
	{
		
		*requestedByteCount = 0;
		
		if ( SYNC )
		{
			
			*requestedByteCount += syncSize;
			
		}
		
		if ( HEADER_CODES & 0x01 )
		{
			
			*requestedByteCount += headerSize;
			
		}
		
		if ( HEADER_CODES & 0x02 )
		{
			
			*requestedByteCount += subHeaderSize;
			
		}
		
		if ( USER_DATA )
		{
			
			*requestedByteCount += userDataSize;
			
		}
		
		if ( EDC_ECC )
		{
			
			*requestedByteCount += edcEccSize;
			
		}
		
	}
	
	if ( ( ERROR_FIELD & 0x03 ) == 0x01 )
	{
		
		*requestedByteCount += C2_ERROR_BLOCK_DATA_SIZE;
		
	}
	
	else if ( ( ERROR_FIELD & 0x03 ) == 0x02 )
	{
		
		*requestedByteCount += C2_AND_BLOCK_ERROR_BITS_SIZE;
		
	}
	
	else if ( ERROR_FIELD != 0 )
	{
		
		ERROR_LOG ( ( "ERROR_FIELD is non-zero\n" ) );
		return false;
		
	}
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIMultimediaCommands::ConvertMSFToLBA
//
//----------------------------------------------------------------------
//
//		The MSF to LBA conversion routine.
//
//----------------------------------------------------------------------

SCSICmdField4Byte
SCSIMultimediaCommands::ConvertMSFToLBA (
								SCSICmdField3Byte 	MSF )
{
	
	SCSICmdField4Byte	LBA;
	
	STATUS_LOG ( ( "SCSIMultimediaCommands::ConvertMSFToLBA called\n" ) );
	
	LBA  = MSF >> 16;				// start with minutes
	LBA *= SECONDS_IN_A_MINUTE;		// convert minutes to seconds
	LBA += ( MSF >>  8 ) & 0xFF;	// add seconds
	LBA *= FRAMES_IN_A_SECOND;		// convert seconds to frames
	LBA += MSF & 0xFF;				// add frames
	
	// valid LBA?
	if ( LBA < LBA_0_OFFSET )
	{
		
		ERROR_LOG ( ( "LBA was less than LBA_0_OFFSET, setting LBA to 0.\n" ) );
		LBA  = 0;
		
	}
	
	else
	{
		
		LBA -= LBA_0_OFFSET;		// subtract the offset of LBA 0
		
	}
	
	return LBA;
	
}