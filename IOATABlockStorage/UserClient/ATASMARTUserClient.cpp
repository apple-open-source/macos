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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Private includes
#include "ATASMARTUserClient.h"
#include "ATASMARTLib.h"

// IOKit includes
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>

// IOKit ATA includes
#include <IOKit/ata/IOATADevice.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATACommand.h>

#include "IOATABlockStorageDevice.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// For debugging, set ATA_SMART_USER_CLIENT_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG
#define ATA_SMART_USER_CLIENT_DEBUGGING_LEVEL 0

#if ( ATA_SMART_USER_CLIENT_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( ATA_SMART_USER_CLIENT_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( ATA_SMART_USER_CLIENT_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define super IOUserClient
OSDefineMetaClassAndStructors ( ATASMARTUserClient, IOUserClient );


#if 0
#pragma mark -
#pragma mark Constants
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Enums
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// S.M.A.R.T command code
enum
{
	kATAcmdSMART	= 0xB0
};

// S.M.A.R.T command features register codes
enum
{
	kFeaturesRegisterReadData					= 0xD0,
	kFeaturesRegisterReadDataThresholds			= 0xD1,
	kFeaturesRegisterEnableDisableAutoSave		= 0xD2,
	// Reserved
	kFeaturesRegisterExecuteOfflineImmed		= 0xD4,
	kFeaturesRegisterReadLogAtAddress			= 0xD5,
	kFeaturesRegisterWriteLogAtAddress			= 0xD6,
	// Reserved
	kFeaturesRegisterEnableOperations			= 0xD8,
	kFeaturesRegisterDisableOperations			= 0xD9,
	kFeaturesRegisterReturnStatus				= 0xDA
};

// S.M.A.R.T 'magic' values
enum
{
	kSMARTMagicCylinderLoValue	= 0x4F,
	kSMARTMagicCylinderHiValue	= 0xC2
};

// S.M.A.R.T Return Status validity values
enum
{
	kSMARTReturnStatusValidLoValue	= 0xF4,
	kSMARTReturnStatusValidHiValue	= 0x2C
};

// S.M.A.R.T Auto-Save values
enum
{
	kSMARTAutoSaveEnable	= 0xF1,
	kSMARTAutoSaveDisable	= 0x00
};

enum
{
	kATAThirtySecondTimeoutInMS	= 30000
};


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOExternalMethod
ATASMARTUserClient::sMethods[kIOATASMARTMethodCount] =
{
	{
		// Method #0 EnableDisableOperations
		0,
		( IOMethod ) &ATASMARTUserClient::EnableDisableOperations,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #1 EnableDisableAutoSave
		0,
		( IOMethod ) &ATASMARTUserClient::EnableDisableAutoSave,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #2 ReturnStatus
		0,
		( IOMethod ) &ATASMARTUserClient::ReturnStatus,
		kIOUCScalarIScalarO,
		0,
		1
	},
	{
		// Method #3 ExecuteOfflineImmediate
		0,
		( IOMethod ) &ATASMARTUserClient::ExecuteOfflineImmediate,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #4 ReadData
		0,
		( IOMethod ) &ATASMARTUserClient::ReadData,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #5 ReadDataThresholds
		0,
		( IOMethod ) &ATASMARTUserClient::ReadDataThresholds,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #6 ReadLogAtAddress
		0,
		( IOMethod ) &ATASMARTUserClient::ReadLogAtAddress,
		kIOUCScalarIStructI,
		0,
		sizeof ( ATASMARTReadLogStruct )
	},
	{
		// Method #7 WriteLogAtAddress
		0,
		( IOMethod ) &ATASMARTUserClient::WriteLogAtAddress,
		kIOUCScalarIStructI,
		0,
		sizeof ( ATASMARTWriteLogStruct )
	}
	
};


#if 0
#pragma mark -
#pragma mark Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ init - Initializes member variables							[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


bool
ATASMARTUserClient::init ( OSDictionary * dictionary )
{
	
	STATUS_LOG ( ( "ATASMARTUserClient::init\n" ) );
	
	if ( !super::init ( dictionary ) )
		return false;
	
	fTask 					= NULL;
	fProvider	 			= NULL;
	fOutstandingCommands	= 0;
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Start providing services								[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
ATASMARTUserClient::start ( IOService * provider )
{
	
	IOWorkLoop *	workLoop;

	STATUS_LOG ( ( "ATASMARTUserClient::start\n" ) );
	
	if ( fProvider != NULL )
	{
		
		ERROR_LOG ( ( "fProvider != NULL, returning false\n" ) );
		return false;
		
	}
	
	STATUS_LOG ( ( "assigning fProvider\n" ) );	
	fProvider = OSDynamicCast ( IOATABlockStorageDevice, provider );
	if ( fProvider == NULL )
	{
		
		ERROR_LOG ( ( "Provider not IOATABlockStorageDevice\n" ) );
		return false;
		
	}
	
	if ( !super::start ( provider ) )
	{
		
		ERROR_LOG ( ( "super rejected provider in start\n" ) );
		return false;
		
	}
	
	STATUS_LOG ( ( "Creating command gate\n" ) );
	fCommandGate = IOCommandGate::commandGate ( this );
	if ( fCommandGate == NULL )
	{
		
		ERROR_LOG ( ( "Command gate creation failed\n" ) );
		return false;
		
	}

	workLoop = getWorkLoop ( );
	if ( workLoop == NULL )
	{
		
		ERROR_LOG ( ( "workLoop == NULL\n" ) );
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
	STATUS_LOG ( ( "Adding command gate\n" ) );
	workLoop->addEventSource ( fCommandGate );
	
	STATUS_LOG ( ( "Opening provider\n" ) );
	
	if ( !fProvider->open ( this, kIOATASMARTUserClientAccessMask, 0 ) )
	{
		
		ERROR_LOG ( ( "Open failed\n" ) );
		fCommandGate->release ( );
		fCommandGate = NULL;
		return false;
		
	}
	
	STATUS_LOG ( ( "start done\n" ) );
	
	// Yes, we found an object to use as our interface
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ initWithTask - Save task_t and validate the connection type	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
ATASMARTUserClient::initWithTask ( task_t	owningTask,
								   void *	securityToken,
								   UInt32	type )
{
	
	STATUS_LOG ( ( "ATASMARTUserClient::initWithTask called\n" ) );
	
	if ( type != kIOATASMARTLibConnection )
		return false;
	
	fTask = owningTask;
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ clientClose - Close down services.							[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::clientClose ( void )
{
	
	STATUS_LOG ( ( "clientClose called\n" ) );
	
	HandleTerminate ( fProvider );
		
	STATUS_LOG ( ( "Done\n" ) );
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ clientClose - Same thing as clientClose, unless we were terminated by
//					a hotunplug event.								[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::clientDied ( void )
{
	
	STATUS_LOG ( ( "ATASMARTUserClient::clientDied called\n" ) );
	
	if ( isInactive ( ) == false )
	{
		return clientClose ( );
	}
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Releases any items we need to release.					[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
ATASMARTUserClient::free ( void )
{
	
	if ( fCommandGate != NULL )
	{
		
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Handles termination messages.						[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::message ( UInt32 type, IOService * provider, void * arg )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "message called\n" ) );
	
	STATUS_LOG ( ( "type = %ld, provider = %p\n", type, provider ) );
	
	switch ( type )
	{
		
		case kIOMessageServiceIsRequestingClose:
			break;
			
		case kIOMessageServiceIsTerminated:
			
			STATUS_LOG ( ( "kIOMessageServiceIsTerminated called\n" ) );
			status = HandleTerminate ( provider );
			break;
			
		default:
			status = super::message ( type, provider, arg );
			break;
		
	}
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnableDisableOperations - Enables/Disables SMART operations.	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::EnableDisableOperations ( UInt32 enable )
{
	
	IOReturn		status = kIOReturnSuccess;
	IOATACommand *	command;
	
	STATUS_LOG ( ( "EnableDisableOperations called\n" ) );
	
	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
	
	if ( enable == 0 )
	{
		
		// They want to disable SMART operations.
		command->setFeatures ( kFeaturesRegisterDisableOperations );
		
	}
	
	else
	{
		
		// The want to enable SMART operations.
		command->setFeatures ( kFeaturesRegisterEnableOperations );
		
	}
	
	command->setOpcode		( kATAFnExecIO );
	command->setTimeoutMS	( kATAThirtySecondTimeoutInMS );
	command->setCylLo		( kSMARTMagicCylinderLoValue );
	command->setCylHi		( kSMARTMagicCylinderHiValue );
	command->setCommand		( kATAcmdSMART );
	
	status = SendSMARTCommand ( command );
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "Enable/Disable unsupported\n" ) );
			status = kIOReturnUnsupported;
			
		}
		
	}
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnableDisableAutoSave - Enables/Disables SMART autosave.		[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::EnableDisableAutoSave ( UInt32 enable )
{
	
	IOReturn		status = kIOReturnSuccess;
	IOATACommand *	command;
	
	STATUS_LOG ( ( "EnableDisableAutoSave called\n" ) );
	
	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
	
	if ( enable == 0 )
	{
		
		// They want to disable SMART autosave operations.
		command->setSectorCount ( kSMARTAutoSaveDisable );
		
	}
	
	else
	{
		
		// They want to enable SMART autosave operations.
		command->setSectorCount ( kSMARTAutoSaveEnable );
		
	}
	
	command->setFeatures 	( kFeaturesRegisterEnableDisableAutoSave );
	command->setOpcode		( kATAFnExecIO );
	command->setTimeoutMS	( kATAThirtySecondTimeoutInMS );
	command->setCylLo		( kSMARTMagicCylinderLoValue );
	command->setCylHi		( kSMARTMagicCylinderHiValue );
	command->setCommand		( kATAcmdSMART );
	
	status = SendSMARTCommand ( command );
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "Enable/Disable autosave unsupported\n" ) );
			status = kIOReturnUnsupported;
			
		}
		
	}
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReturnStatus - Returns SMART status.							[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::ReturnStatus ( UInt32 * exceededCondition )
{
	
	IOReturn		status 	= kIOReturnSuccess;
	IOATACommand *	command	= NULL;
	UInt8			lbaMid 	= kSMARTMagicCylinderLoValue;
	UInt8			lbaHigh	= kSMARTMagicCylinderHiValue;

	STATUS_LOG ( ( "ReturnStatus called\n" ) );
	
	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
		
	command->setFeatures 	( kFeaturesRegisterReturnStatus );
	command->setOpcode		( kATAFnExecIO );
	command->setTimeoutMS	( kATAThirtySecondTimeoutInMS );
	command->setCylLo		( lbaMid );
	command->setCylHi		( lbaHigh );
	command->setCommand		( kATAcmdSMART );
	command->setRegMask		( ( ataRegMask ) ( mATACylinderHiValid | mATACylinderLoValid ) );
	command->setFlags		( mATAFlagTFAccessResult );
	
	status = SendSMARTCommand ( command );
	
	lbaMid 	= command->getCylLo ( );
	lbaHigh = command->getCylHi ( );
	
	if ( status == kIOReturnSuccess )
	{
	
		// Check if threshold exceeded
		if ( ( lbaMid == kSMARTReturnStatusValidLoValue ) &&
			 ( lbaHigh == kSMARTReturnStatusValidHiValue ) )
		{
			*exceededCondition = 1;
		}
		
		else
		{
			*exceededCondition = 0;
		}
	
	}
	
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "Return Status unsupported\n" ) );
			status = kIOReturnUnsupported;
			
		}
		
	}
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ExecuteOfflineImmediate - Executes self test offline immediately. [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::ExecuteOfflineImmediate ( UInt32 extendedTest )
{
	
	IOReturn		status 	= kIOReturnSuccess;
	IOATACommand *	command	= NULL;
	
	STATUS_LOG ( ( "ExecuteOfflineImmediate called\n" ) );
	
	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
		
	command->setFeatures 		( kFeaturesRegisterExecuteOfflineImmed );
	command->setOpcode			( kATAFnExecIO );
	command->setTimeoutMS		( kATAThirtySecondTimeoutInMS );
	command->setSectorNumber	( ( extendedTest == 0 ) ? 0x01 : 0x02 );
	command->setCylLo			( kSMARTMagicCylinderLoValue );
	command->setCylHi			( kSMARTMagicCylinderHiValue );
	command->setCommand			( kATAcmdSMART );
	
	status = SendSMARTCommand ( command );
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "Execute Offline Immediate unsupported\n" ) );
			status = kIOReturnUnsupported;
			
		}
		
	}
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadData - Reads SMART data. 									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::ReadData ( vm_address_t data )
{
	
	IOReturn				status 	= kIOReturnSuccess;
	IOATACommand *			command	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	
	STATUS_LOG ( ( "ReadData called\n" ) );
	
	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
	
	buffer = IOMemoryDescriptor::withAddress ( 	data,
												sizeof ( ATASMARTData ),
												kIODirectionIn,
												fTask );
	
	if ( buffer == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseCommand;
		
	}
	
	status = buffer->prepare ( );
	if ( status != kIOReturnSuccess )
	{
		
		goto ReleaseBuffer;
		
	}
	
	command->setBuffer 			( buffer );
	command->setByteCount 		( sizeof ( ATASMARTData ) );
	command->setFeatures 		( kFeaturesRegisterReadData );
	command->setOpcode			( kATAFnExecIO );
	command->setTimeoutMS		( kATAThirtySecondTimeoutInMS );
	command->setCylLo			( kSMARTMagicCylinderLoValue );
	command->setCylHi			( kSMARTMagicCylinderHiValue );
	command->setCommand			( kATAcmdSMART );
	command->setFlags 			( mATAFlagIORead );
	
	status = SendSMARTCommand ( command );
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "ReadData unsupported\n" ) );
			status = kIOReturnUnsupported;
			
		}
		
		if ( command->getEndErrorReg ( ) & 0x10 )
		{
			
			ERROR_LOG ( ( "ReadData Not readable\n" ) );
			status = kIOReturnNotReadable;
			
		}
		
	}
	
	buffer->complete ( );
	
	
ReleaseBuffer:
	
	
	buffer->release ( );
	buffer = NULL;
	
	
ReleaseCommand:
	
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadDataThresholds - Reads SMART data thresholds. 			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::ReadDataThresholds ( vm_address_t data )
{
	
	IOReturn				status 	= kIOReturnSuccess;
	IOATACommand *			command	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	
	STATUS_LOG ( ( "ReadDataThresholds called\n" ) );
	
	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
	
	buffer = IOMemoryDescriptor::withAddress ( 	data,
												sizeof ( ATASMARTDataThresholds ),
												kIODirectionIn,
												fTask );
	
	if ( buffer == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseCommand;
		
	}
	
	status = buffer->prepare ( );
	if ( status != kIOReturnSuccess )
	{
		
		goto ReleaseBuffer;
		
	}
	
	command->setBuffer 			( buffer );
	command->setByteCount 		( sizeof ( ATASMARTDataThresholds ) );
	command->setFeatures 		( kFeaturesRegisterReadDataThresholds );
	command->setOpcode			( kATAFnExecIO );
	command->setTimeoutMS		( kATAThirtySecondTimeoutInMS );
	command->setCylLo			( kSMARTMagicCylinderLoValue );
	command->setCylHi			( kSMARTMagicCylinderHiValue );
	command->setCommand			( kATAcmdSMART );
	command->setFlags 			( mATAFlagIORead );
	
	status = SendSMARTCommand ( command );
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "ReadDataThresholds unsupported\n" ) );
			status = kIOReturnUnsupported;
			
		}
		
		if ( command->getEndErrorReg ( ) & 0x10 )
		{
			
			ERROR_LOG ( ( "ReadDataThresholds Not readable\n" ) );
			status = kIOReturnNotReadable;
			
		}
		
	}
	
	buffer->complete ( );
	
	
ReleaseBuffer:
	
	
	buffer->release ( );
	buffer = NULL;
	
	
ReleaseCommand:
	
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadLogAtAddress - Reads the SMART log at the specified address. [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::ReadLogAtAddress ( 	ATASMARTReadLogStruct *	readLogData,
										UInt32					inStructSize )
{
	
	IOReturn				status 			= kIOReturnSuccess;
	IOATACommand *			command			= NULL;
	IOMemoryDescriptor *	buffer			= NULL;
	
	STATUS_LOG ( ( "ReadLogAtAddress called\n" ) );
	
	if ( inStructSize != sizeof ( ATASMARTReadLogStruct ) )
		return kIOReturnBadArgument;

	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
	
	buffer = IOMemoryDescriptor::withAddress ( 	( vm_address_t ) readLogData->buffer,
												readLogData->bufferSize,
												kIODirectionIn,
												fTask );
	
	if ( buffer == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseCommand;
		
	}
	
	status = buffer->prepare ( );
	if ( status != kIOReturnSuccess )
	{
		
		goto ReleaseBuffer;
		
	}
	
	command->setBuffer 			( buffer );
	command->setByteCount 		( readLogData->bufferSize );
	command->setFeatures 		( kFeaturesRegisterReadLogAtAddress );
	command->setOpcode			( kATAFnExecIO );
	command->setTimeoutMS		( kATAThirtySecondTimeoutInMS );
	command->setSectorCount		( readLogData->numSectors );
	command->setSectorNumber	( readLogData->logAddress );
	command->setCylLo			( kSMARTMagicCylinderLoValue );
	command->setCylHi			( kSMARTMagicCylinderHiValue );
	command->setCommand			( kATAcmdSMART );
	command->setFlags 			( mATAFlagIORead );
	
	status = SendSMARTCommand ( command );
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "ReadLogAtAddress %d unsupported\n", readLogData->logAddress ) );
			status = kIOReturnUnsupported;
			
		}
			
		if ( command->getEndErrorReg ( ) & 0x10 )
		{
			
			ERROR_LOG ( ( "ReadLogAtAddress %d unreadable\n", readLogData->logAddress ) );
			status = kIOReturnNotReadable;
			
		}
		
	}
	
	buffer->complete ( );
	
	
ReleaseBuffer:
	
	
	buffer->release ( );
	buffer = NULL;
	
	
ReleaseCommand:
	
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WriteLogAtAddress - Writes the SMART log at the specified address. [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::WriteLogAtAddress ( ATASMARTWriteLogStruct *	writeLogData,
										UInt32						inStructSize )
{
	
	IOReturn				status 			= kIOReturnSuccess;
	IOATACommand *			command			= NULL;
	IOMemoryDescriptor *	buffer			= NULL;
	
	STATUS_LOG ( ( "WriteLogAtAddress called\n" ) );
	
	if ( inStructSize != sizeof ( ATASMARTWriteLogStruct ) )
		return kIOReturnBadArgument;
	
	fOutstandingCommands++;
	
	if ( isInactive ( ) )
	{
		
		status = kIOReturnNoDevice;
		goto ErrorExit;
		
	}
	
	fProvider->retain ( );
	
	command = AllocateCommand ( );
	if ( command == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseProvider;
		
	}
	
	buffer = IOMemoryDescriptor::withAddress ( 	( vm_address_t ) writeLogData->buffer,
												writeLogData->bufferSize,
												kIODirectionOut,
												fTask );
	
	if ( buffer == NULL )
	{
		
		status = kIOReturnNoResources;
		goto ReleaseCommand;
		
	}
	
	status = buffer->prepare ( );
	if ( status != kIOReturnSuccess )
	{
		
		goto ReleaseBuffer;
		
	}
	
	command->setBuffer 			( buffer );
	command->setByteCount 		( writeLogData->bufferSize );
	command->setFeatures 		( kFeaturesRegisterWriteLogAtAddress );
	command->setOpcode			( kATAFnExecIO );
	command->setTimeoutMS		( kATAThirtySecondTimeoutInMS );
	command->setSectorCount		( writeLogData->numSectors );
	command->setSectorNumber	( writeLogData->logAddress );
	command->setCylLo			( kSMARTMagicCylinderLoValue );
	command->setCylHi			( kSMARTMagicCylinderHiValue );
	command->setCommand			( kATAcmdSMART );
	command->setFlags 			( mATAFlagIOWrite ); 
	
	status = SendSMARTCommand ( command );
	if ( status == kIOReturnIOError )
	{
		
		if ( command->getEndErrorReg ( ) & 0x04 )
		{
			
			ERROR_LOG ( ( "ReadLogAtAddress %d unsupported\n", writeLogData->logAddress ) );
			status = kIOReturnUnsupported;
			
		}
			
		if ( command->getEndErrorReg ( ) & 0x10 )
		{
			
			ERROR_LOG ( ( "ReadLogAtAddress %d unwriteable\n", writeLogData->logAddress ) );
			status = kIOReturnNotWritable;
			
		}
		
	}
	
	buffer->complete ( );
	
	
ReleaseBuffer:
	
	
	buffer->release ( );
	buffer = NULL;
	
	
ReleaseCommand:
	
	
	DeallocateCommand ( command );
	command = NULL;
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getTargetAndMethodForIndex - 	Returns a pointer to the target of the
//									method call and the method vector itself
//									based on the provided index.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOExternalMethod *
ATASMARTUserClient::getTargetAndMethodForIndex ( IOService ** target, UInt32 index )
{
	
	if ( index >= kIOATASMARTMethodCount )
		return NULL;
	
	if ( isInactive ( ) )
		return NULL;
	
	*target = this;
	
	return &sMethods[index];
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GatedWaitForCommand -	Waits for signal to wake up. It must hold the
//							workloop lock in order to call commandSleep()
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::GatedWaitForCommand ( IOATACommand * command )
{
	
	IOReturn			status = kIOReturnSuccess;
	ATASMARTRefCon *	refCon;
	
	STATUS_LOG ( ( "GatedWaitForCommand called\n" ) );
	
	refCon = ( ATASMARTRefCon * ) command->refCon;
		
	// Check to make sure the command hasn't been completed and called the
	// callback handler before the stack unwinds and we get the workloop
	// lock. This usually won't happen since the callback runs on the
	// secondary interrupt context (involves going through the scheduler),
	// but it is still good to do this sanity check.
	while ( refCon->isDone != true )
	{
		
		fCommandGate->commandSleep ( &refCon->sleepOnIt, THREAD_UNINT );
		
	}
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleTerminate -	Handles terminating our object and any resources it
//						allocated.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::HandleTerminate ( IOService * provider )
{
	
	IOReturn		status 		= kIOReturnSuccess;
	IOWorkLoop *	workLoop	= NULL;
	
	STATUS_LOG ( ( "HandleTerminate called\n" ) );
	
	if ( provider->isOpen ( this ) )
	{
		
		while ( fOutstandingCommands != 0 )
		{
			IOSleep ( 10 );
		}
		
		STATUS_LOG ( ( "Closing provider\n" ) );
		provider->close ( this, kIOATASMARTUserClientAccessMask );
		
	}
		
	detach ( provider );
	
	workLoop = getWorkLoop ( );
	if ( workLoop != NULL )
		workLoop->removeEventSource ( fCommandGate );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendSMARTCommand - Sends a command to the hardware synchronously.
//						 This is a helper method for all our non-exclusive
//						 methods.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::SendSMARTCommand ( IOATACommand * command )
{
	
	ATASMARTRefCon		refCon;
	IOReturn			status 	= kIOReturnSuccess;
	
	bzero ( &refCon, sizeof ( refCon ) );
	
	refCon.isDone 	= false;
	refCon.self		= this;
	
	STATUS_LOG ( ( "SendSMARTCommand called\n" ) );
	
	command->setCallbackPtr ( &ATASMARTUserClient::sCommandCallback );
	command->refCon = ( void * ) &refCon;
	
	// Retain this object. It will be released by CommandCallback method.
	retain ( );
	
	status = fProvider->sendSMARTCommand ( command );
	if ( status == kIOReturnSuccess )
	{
		
		fCommandGate->runAction ( ( IOCommandGate::Action ) &ATASMARTUserClient::sWaitForCommand,
							  ( void * ) command );
		
		STATUS_LOG ( ( "SendSMARTCommand runAction done\n" ) );
		
		switch ( command->getResult ( ) )
		{
			
			case kATANoErr:
				status = kIOReturnSuccess;
				break;
				
			case kATATimeoutErr:
				status = kIOReturnTimeout;
				break;
				
			default:
				status = kIOReturnIOError;			
				break;
			
		}
				
	}
	
	else
	{
		
		// Error path, release this object since we called retain()
		release ( );
		
	}
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CommandCallback - 	This method is called by sCommandCallback as the
//							completion routine for all IOATACommands.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
ATASMARTUserClient::CommandCallback ( IOATACommand * command )
{
	
	ATASMARTRefCon *		refCon	= NULL;
		
	refCon = ( ATASMARTRefCon * ) command->refCon;
	fCommandGate->commandWakeup ( &refCon->sleepOnIt, true );
	release ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AllocateCommand - 	This method allocates an IOATACommand object.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOATACommand *
ATASMARTUserClient::AllocateCommand ( void )
{
	
	IOATACommand *	command = NULL;
	IOATADevice *	device	= NULL;
	
	STATUS_LOG ( ( "AllocateCommand called\n" ) );
	
	device = ( IOATADevice * ) ( fProvider->getProvider ( )->getProvider ( ) );
	command = device->allocCommand ( );
	
	return command;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DeallocateCommand - 	This method deallocates an IOATACommand object.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
ATASMARTUserClient::DeallocateCommand ( IOATACommand * command )
{
	
	STATUS_LOG ( ( "DeallocateCommand called\n" ) );
	
	if ( command != NULL )
	{
		
		IOATADevice *	device	= NULL;
		
		device = ( IOATADevice * ) ( fProvider->getProvider ( )->getProvider ( ) );
		device->freeCommand ( command );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sWaitForCommand - Called by runAction and holds the workloop lock.
//																	[STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
ATASMARTUserClient::sWaitForCommand ( void * 			userClient,
									  IOATACommand * 	command )
{
	
	return ( ( ATASMARTUserClient * ) userClient )->GatedWaitForCommand ( command );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sCommandCallback -	Static completion routine. Calls CommandCallback.
//							It holds the workloop lock as well since it is on
//							the completion chain from the controller.
//																	[STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
ATASMARTUserClient::sCommandCallback ( IOATACommand * command )
{
	
	ATASMARTRefCon *		refCon	= NULL;
	
	STATUS_LOG ( ( "ATASMARTUserClient::sCommandCallback called.\n") );
	
	refCon = ( ATASMARTRefCon * ) command->refCon;
	if ( refCon == NULL )
	{
		
		ERROR_LOG ( ( "ATASMARTUserClient::sCommandCallback refCon == NULL.\n" ) );
		PANIC_NOW ( ( "ATASMARTUserClient::sCommandCallback refCon == NULL." ) );
		return;
		
	}
	
	refCon->isDone = true;
	refCon->self->CommandCallback ( command );
	
}