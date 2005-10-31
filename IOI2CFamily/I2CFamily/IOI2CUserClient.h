/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CUserClient.h,v 1.3 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CUserClient.h,v $
 *		Revision 1.3  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.2  2004/09/17 20:23:18  jlehrer
 *		Removed ASPL headers.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#ifndef _IOI2CUserClient_H
#define _IOI2CUserClient_H


#include <IOKit/IOUserClient.h>
#include <IOI2C/IOI2CDefs.h>


class IOI2CUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(IOI2CUserClient)

public:
	virtual bool init(OSDictionary *dict);
	virtual bool initWithTask(task_t owningTask, void *security_id, UInt32 type);
	virtual bool start(IOService *provider);
	virtual void free(void);

	virtual IOExternalMethod *getTargetAndMethodForIndex(
			IOService **target,
			UInt32 Index);

	virtual IOReturn clientClose(void);

private:
	task_t			fOwningTask;
	IOService		*fProvider;
	UInt32			fClientKey;

protected:
	const OSSymbol	*symLockI2CBus;
	const OSSymbol	*symUnlockI2CBus;
	const OSSymbol	*symReadI2CBus;
	const OSSymbol	*symWriteI2CBus;

	// Externally accessible methods...

	/*! @function lockI2CBus
		@abstract Request exclusive access to this devices I2C bus.
		@discussion This function provides a method for clients to obtain exclusive access to the I2C bus. This method should only be used when it is necessary to perform several bus transactions atomically. The client must unlock the bus. All other I2C client threads will block while the bus is locked.
		@param clientKeyRef A pointer to the clients 32-bit key. Upon success this key is used for subsequent read, write transations. */
	IOReturn lockI2CBus(
		UInt32		bus,
		UInt32		*clientKeyRef);

	/*! @function unlockI2CBus
		@abstract Release exclusive access to this devices I2C bus.
		@discussion This function releases exclusive access to the I2C bus. The client must unlock the bus.
		@param clientKey The clients 32-bit key acquired from lockI2CBus. */
	IOReturn unlockI2CBus(UInt32 clientKey);

	/*! @function readI2CBus
		@abstract Initiate an I2C read transaction with this device.
		@discussion This function provides a method for initiating I2C read transaction with this device. This method may be called with or without locking the I2C bus.
		@param input A pointer to the clients input parameter struct.
		@param output A pointer to the clients output parameter struct.
		@param inputSize The size in bytes of the clients output parameter struct.
		@param outputSize The size in bytes of the clients output parameter struct. */
	IOReturn readI2CBus(
		I2CUserReadInput	*input,
		I2CUserReadOutput	*output,
		IOByteCount		inputSize,
		IOByteCount		*outputSizeP,
		void *p5, void *p6 );

	/*! @function writeI2CBus
		@abstract Initiate an I2C write transaction with this device.
		@discussion This function provides a method for initiating an I2C write transaction with this device. This method may be called with or without locking the I2C bus.
		@param input A pointer to the clients input parameter struct.
		@param output A pointer to the clients output parameter struct.
		@param inputSize The size in bytes of the clients output parameter struct.
		@param outputSizeP A pointer to a IOByteCount containing the size in bytes of the clients output parameter struct. */
	IOReturn writeI2CBus(
		I2CUserWriteInput	*input,
		I2CUserWriteOutput	*output,
		IOByteCount		inputSize,
		IOByteCount		*outputSizeP,
		void *p5, void *p6 );

	/*! @function userClientRMW
		@abstract Initiate an I2C read-modify-write transaction with this device.
		@discussion This function provides a method for initiating an I2C read-modify-write transaction with this device. This method may be called with or without locking the I2C bus.
		@param input A pointer to the clients input parameter struct.
		@param output A pointer to the clients output parameter struct.
		@param inputSize The size in bytes of the clients output parameter struct.
		@param outputSizeP A pointer to a IOByteCount containing the size in bytes of the clients output parameter struct. */
	IOReturn readModifyWriteI2CBus(
		I2CRMWInput		*input,
		IOByteCount		inputSize,
		void *p3, void *p4, void *p5, void *p6 );

	/*!
		Method space reserved for future expansion.
		According to the IOKit doc you can change each reserved method from private to protected or public as they become used.
		This should not break binary compatibility. Refer the following URL or a C++ reference for more info on these keywords.
		http://developer.apple.com/documentation/DeviceDrivers/Conceptual/WritingDeviceDriver/CPluPlusRuntime/chapter_2_section_5.html
	*/
protected:
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  0 );
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  1 );
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  2 );
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  3 );
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  4 );
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  5 );
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  6 );
    OSMetaClassDeclareReservedUnused ( IOI2CUserClient,  7 );
};

#endif // _IOI2CUserClient_H
