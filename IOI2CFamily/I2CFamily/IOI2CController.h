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
 *	File: $Id: IOI2CController.h,v 1.6 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CController.h,v $
 *		Revision 1.6  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.5  2004/12/15 00:37:14  jlehrer
 *		[3917744,3917697] Added newUserClient method.
 *		Misc cleanup. Removed cancelTransactions.
 *		
 *		Revision 1.4  2004/11/04 20:16:35  jlehrer
 *		Added isRemoving argument to registerPowerStateInterest method.
 *		
 *		Revision 1.3  2004/09/17 20:36:30  jlehrer
 *		Removed APSL headers.
 *		
 *		Revision 1.2  2004/07/03 00:07:05  jlehrer
 *		Added support for dynamic max-i2c-data-length.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#ifndef _IOI2CController_H
#define _IOI2CController_H


#include <IOKit/IOService.h>
#include <IOKit/IONotifier.h>
#include <IOI2C/IOI2CDefs.h>

class IOI2CController : public IOService
{
	OSDeclareAbstractStructors( IOI2CController )

protected:
	const OSSymbol	*symLockI2CBus;
	const OSSymbol	*symUnlockI2CBus;
	const OSSymbol	*symReadI2CBus;
	const OSSymbol	*symWriteI2CBus;
	const OSSymbol	*symPowerInterest;
	const OSSymbol	*symPowerClient;
	const OSSymbol	*symPowerAcked;
	const OSSymbol	*symGetMaxI2CDataLength;
	UInt32			fMaxI2CDataLength;
    IOService		*fProvider;
	bool			fDisablePowerManagement;

private:
	enum
	{
		kIOI2C_CLIENT_KEY_LOCKED	= (1 << 0),
		kIOI2C_CLIENT_KEY_VALID		= 0x1000,
		kIOI2C_CLIENT_KEY_RESERVED	= 0x80000000,
	};

	IOLock			*fPowerLock;
	IOLock			*fClientLock;			// Used for I2C arbitration and power state transition synchronization.
	volatile semaphore_t
					fClientSem;
	UInt32			fClientLockKey;
	bool			fDeviceIsUsable;
	bool			fTransactionInProgress;
	unsigned long	fCurrentPowerState;
//	IONotifier		*fSysPowerNotifier;		// 
	thread_call_t	fIOSyncThreadCall;		// Thread call used for power state transitions.
	UInt32			fI2CBus;				// Our single bus ID from the AAPL,i2c-bus property,
											// for multi-bus fI2CBus is set to kIOI2CMultiBusID.

	#define			kIOI2CMultiBusID	0xcafe12c

	/*!
		I2C controllers are configured in the device-tree for a single-bus or multi-bus operation:

		If only a single I2C bus is controlled by this device:
			Each I2C slave device will be a direct child node of this controller in the device-tree., and
			The controllers bus ID will be identified by an "AAPL,i2c-bus" property in the device-tree., and
			The fI2CBus will be set to the "AAPL,i2c-bus" property value and used for all transactions.

		Otherwise...
		If multiple I2C busses are controlled by this device:
			An "i2c-bus" compatible child node will be present for each bus in the device-tree., and
			Each I2C slave device will be a child node of its respective i2c-bus., and
			The "AAPL,i2c-bus" property will not be present, and
			The fI2CBus will be set to kIOI2CMultiBusID to identify multi-bus capability., and
			Each child i2c-bus driver will be responsible for setting the bus ID on each transaction.
	*/

public:
	virtual bool start ( IOService *provider );
    virtual void stop ( IOService *provider );
	virtual void free ( void );

	using IOService::callPlatformFunction;
	virtual IOReturn callPlatformFunction(
		const OSSymbol *functionSymbol,
        bool waitForFunction,
        void *param1, void *param2,
        void *param3, void *param4 );

private:
	IOReturn clientReadI2C(
		IOI2CCommand	*cmd,
		UInt32			clientKey);

	IOReturn clientWriteI2C(
		IOI2CCommand	*cmd,
		UInt32			clientKey);

	IOReturn clientLockI2C(
		UInt32			bus,
		UInt32			*clientKeyRef);

	IOReturn clientUnlockI2C(
		UInt32			bus,
		UInt32			clientKey);

protected:
	IOReturn publishChildren(void);

	// I2C resource init and cleanup...
	IOReturn initI2CResources(void);
	void freeI2CResources(void);

	enum
	{
		kIOI2CPowerState_OFF	= 0,
		kIOI2CPowerState_SLEEP,
		kIOI2CPowerState_DOZE,
		kIOI2CPowerState_ON,
		kIOI2CPowerState_COUNT
	};

	// Power Management Methods...
	IOReturn InitializePowerManagement(void);
//	virtual unsigned long maxCapabilityForDomainState ( IOPMPowerFlags domainState );

	virtual IOReturn setPowerState(
		unsigned long	newPowerState,
		IOService		*dontCare);

	static IOReturn sSysPowerDownHandler(
		void			*target,
		void			*refCon,
		UInt32			messageType,
		IOService		*service,
		void			*messageArgument,
		vm_size_t		argSize);

	static void sIOSyncCallback(
		thread_call_param_t	p0,
		thread_call_param_t	p1);

	IOReturn registerPowerStateInterest(
		IOService		*client,
		bool			isRegistering);

	bool notifyPowerStateInterest(void);

	virtual IOReturn acknowledgeNotification(
		IONotificationRef	notification,
		IOOptionBits		response );

	// Subclass required methods...
	virtual IOReturn processLockI2CBus(
		UInt32			bus) = 0;

	virtual IOReturn processUnlockI2CBus(
		UInt32			bus) = 0;

	virtual IOReturn processReadI2CBus(
		IOI2CCommand	*cmd) = 0;

	virtual IOReturn processWriteI2CBus(
		IOI2CCommand	*cmd) = 0;

	// IOI2CUserClient...
	using IOService::newUserClient;
	virtual IOReturn newUserClient(
		task_t					owningTask,
		void					*securityID,
		UInt32					type,
		OSDictionary			*properties,
		IOUserClient			**handler);

protected:
	/*!	@struct ExpansionData
		@discussion This structure helps to expand the capabilities of this class in the future.
	*/
	typedef struct ExpansionData {};

	/*! @var reserved
		Reserved for future use.  (Internal use only)
	*/
	ExpansionData *reserved;

	// Space reserved for future expansion.
    OSMetaClassDeclareReservedUnused ( IOI2CController,  0 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  1 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  2 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  3 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  4 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  5 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  6 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  7 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  8 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  9 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  10 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  11 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  12 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  13 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  14 );
    OSMetaClassDeclareReservedUnused ( IOI2CController,  15 );
};

#endif // _IOI2CController_H
