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

#ifndef _IOKIT_ATA_SMART_USER_CLIENT_H_
#define _IOKIT_ATA_SMART_USER_CLIENT_H_


#if defined(KERNEL) && defined(__cplusplus)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// IOKit includes
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>

// Private includes
#include "ATASMARTLib.h"
#include "ATASMARTLibPriv.h"
#include "IOATABlockStorageDevice.h"

#include <IOKit/ata/IOATACommand.h>

// Forward class declaration
class ATASMARTUserClient;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Typedefs
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

struct ATASMARTRefCon
{
	ATASMARTUserClient *	self;
	bool					isDone;
	bool					sleepOnIt;
};
typedef struct ATASMARTRefCon ATASMARTRefCon;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


class ATASMARTUserClient : public IOUserClient
{
	
	OSDeclareDefaultStructors ( ATASMARTUserClient );
	
public:
	
	virtual bool 	 initWithTask 		( task_t owningTask, void * securityToken, UInt32 type );
	
    virtual bool 	 init 				( OSDictionary * dictionary = 0 );
    virtual bool 	 start 				( IOService * provider );
	virtual void	 free				( void );
    virtual IOReturn message 			( UInt32 type, IOService * provider, void * arg );
	
    virtual IOReturn clientClose 		( void );
    virtual IOReturn clientDied 		( void );
		
	virtual IOReturn	EnableDisableOperations ( UInt32 enable );
	virtual IOReturn	EnableDisableAutoSave 	( UInt32 enable );
	virtual IOReturn	ReturnStatus		 	( UInt32 * exceedsCondition );
	virtual IOReturn	ExecuteOfflineImmediate	( UInt32 extendedTest );
	virtual IOReturn	ReadData				( vm_address_t data );
	virtual IOReturn	ReadDataThresholds		( vm_address_t data );
	virtual IOReturn	ReadLogAtAddress		( ATASMARTReadLogStruct *	readLogData,
												  UInt32					inStructSize );
	virtual IOReturn	WriteLogAtAddress		( ATASMARTWriteLogStruct *	writeLogData,
												  UInt32					inStructSize );
	
protected:
	
	static IOExternalMethod					sMethods[kIOATASMARTMethodCount];

	static IOReturn		sWaitForCommand 	( void * userClient, IOATACommand * command );
	static void 		sCommandCallback	( IOATACommand * command );
	
	virtual IOReturn 	GatedWaitForCommand	( IOATACommand * command );
	virtual void	 	CommandCallback		( IOATACommand * command );
	
	task_t									fTask;
	IOATABlockStorageDevice *				fProvider;
	IOCommandGate *							fCommandGate;
	UInt32									fOutstandingCommands;
	
	virtual IOExternalMethod *				getTargetAndMethodForIndex ( IOService **	target,
																		 UInt32			index );
	
	virtual IOReturn 		HandleTerminate		( IOService * provider );
	virtual IOReturn		SendSMARTCommand	( IOATACommand * command );
	virtual IOATACommand *	AllocateCommand		( void );
	virtual void			DeallocateCommand	( IOATACommand * command );

};


#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif /* ! _IOKIT_ATA_SMART_USER_CLIENT_H_ */