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
 *	File: $Id: IOI2CDevice.h,v 1.5 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CDevice.h,v $
 *		Revision 1.5  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.4  2005/02/08 20:53:16  jlehrer
 *		Added debug flags.
 *		
 *		Revision 1.3  2004/12/15 00:50:07  jlehrer
 *		[3867728] Added kStateFlags_TEARDOWN to assist terminating gracefully with bad hardware.
 *		[3905559] Added kStateFlags_DISABLE_PM to disable power management for AOA.
 *		Added fEnableOnDemandPlatformFunctions.
 *		
 *		Revision 1.2  2004/09/17 20:53:30  jlehrer
 *		Removed ASPL headers.
 *		Fixed headerDoc comments.
 *		Added external client read/write symbols to ExpansionData struct.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#ifndef _IOI2CDevice_H
#define _IOI2CDevice_H


#include <IOKit/IOService.h>
#include <IOI2C/IOI2CDefs.h>

class IOPlatformFunction;

class IOI2CDevice : public IOService
{
	OSDeclareDefaultStructors(IOI2CDevice)

public:
	virtual bool init ( OSDictionary *dict );
	virtual bool start ( IOService *provider );
	virtual void stop ( IOService *provider );
	virtual void free ( void );

	/*!
		@method newUserClient
		@abstract Method for creating an IOI2CUserClient instance.
		@discussion By default, if the type parameter = 0, this method will create an IOI2CUserClient instance. User applications may request creating the IOUserClient class type specified in the subclass drivers IOKitPersonalities IOUserClientClass property by passing a non-zero value in the type parameter.
	*/
	virtual IOReturn newUserClient(
		task_t			owningTask,
		void			*securityID,
		UInt32			type,
		OSDictionary	*properties,
		IOUserClient	**handler );

	// External client API...
	using IOService::callPlatformFunction;
	virtual IOReturn callPlatformFunction (
		const OSSymbol	*functionName,
		bool			waitForFunction,
		void			*param1,
		void			*param2,
		void			*param3,
		void			*param4 );

	/*!
		@method message
		@abstract This IOService method is called by the parent IOI2CController driver to notify each device of power down events.
	*/
	virtual IOReturn message(
		UInt32			type,
		IOService		*provider,
		void			*argument = 0);

protected:
	// I2C resource init and cleanup...
	/*!
		@method initI2CResources
		@abstract This method allocates resources used by this driver.
		@discussion This method is called from start.
	*/
	IOReturn initI2CResources(void);

	/*!
		@method freeI2CResources
		@abstract This method deallocates all resources used by this driver.
		@discussion This method is called from the stop and free methods. If the subclass driver fails on start it should call this method.
	*/
	void freeI2CResources(void);

protected:
	IOService		*fProvider;				// Our cached IOService provider. (not-retained)
	OSArray			*fPlatformFuncArray;	// An OSarray of IO platform functions.

	const OSSymbol	*symLockI2CBus;			// CallPlatformFunction Symbol for locking the I2C bus
	const OSSymbol	*symUnlockI2CBus;		// CallPlatformFunction Symbol for unlocking the I2C bus
	const OSSymbol	*symReadI2CBus;			// CallPlatformFunction Symbol for reading the I2C bus
	const OSSymbol	*symWriteI2CBus;		// CallPlatformFunction Symbol for writing the I2C bus

private:
	UInt32			fI2CAddress;			// This devices I2C bus address.
	IOLock			*fClientLock;			// Used for I2C arbitration and power state transition synchronization.
	volatile semaphore_t
					fClientSem;
	unsigned long	fCurrentPowerState;		// The current power state.
	thread_call_t	fPowerStateThreadCall;	// Thread call used for power state transitions.
	thread_t		fPowerThreadID;			// Used to allow only the fPowerStateThreadCall to do transactions during power state transitions.
	void			*fSysPowerRef;			// Used for priority power state notification from the root domain. When non-zero the system is about to power off or restart.
	bool			fClientIOBlocked;		// Flag to indicate only power transition thread transactions allowed.
	bool			fDeviceOffline;			// Flag to indicate this device is offline.
	UInt32			fStateFlags;			// Flags bitfield for driver state.

protected:
	// fStateFlags bitfield definitions...
	enum
	{
		kStateFlags_STARTUP			= (1 << 31),	// indicates the first power state ON transition is processed as a STARTUP event.
		kStateFlags_TEARDOWN		= (1 << 30),	// indicates freeI2CResources has been called and the driver is preparing for teardown.
		kStateFlags_DISABLE_PM		= (1 << 28),	// indicates power management is disabled.
		kStateFlags_IOLog			= (1 << 27),	// Enables IOLog for every read/write transaction
		kStateFlags_kprintf			= (1 << 26),	// Enables kprintf for every read/write transaction
		kStateFlags_PMInit			= (1 << 25),	// True if PMInit was called, False if not or PMStop was called.
	};

	// Subclass client I2C API's...

	/*!
		@method getI2CAddress
		@abstract This method returns the I2C address of this drivers device.
		@result I2C address.
	*/
	UInt32 getI2CAddress(void);

	/*!
		@method lockI2CBus
		@abstract This method attains a mutually exclusive lock on the I2C device and bus for subsequent I2C transactions.
		@discussion The client must unlock the bus. The bus should be locked for a minimum period. All other clients will block while the lock is held.
		@param clientKeyRef Address of a client allocated UInt32 which receives a key value for subsequent read and write transactions while the lock is held.
		@result kIOReturnSuccess, or other IOReturn error code.
	*/
	IOReturn lockI2CBus(
		UInt32	*clientKeyRef);

	/*!
		@method unlockI2CBus
		@abstract This method releases the exclusive lock on the I2C device and bus.
		@param clientKeyRef The client allocated UInt32 returned from calling lockI2CBus.
		@result kIOReturnSuccess, or other IOReturn error code.
	*/
	IOReturn unlockI2CBus(
		UInt32	clientKey);

	/*!
		@method writeI2C
		@abstract This method initiates an I2C write transaction to this drivers device.
		@discussion This is a convienience method to initialize an IOI2CCommand struct parameters and initiate the write transaction.
		@param subAddress The slave sub-address register. The format is as follows:
			bits[31..24] specify length: 0=8bit; 1=8bit; 2=16bit; 3=24bit.
			bits[23..0] specify subaddress:
				8bit subaddresses use bits[7..0],
				16bit subaddresses use bits[15..0],
				24bit subaddresses use bits[23..0].
		@param data Address of client allocated buffer containing the data to write.
		@param count Number of bytes to write from the data buffer.
		@param clientKey (optional) Either a valid key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT.
			If the client passes kIOI2C_CLIENT_KEY_DEFAULT this method will block waiting for exclusive access to the I2C bus. (default)
			If a valid key is used then this method is guaranteed to execute synchronously on the calling thread.
		@param mode The I2C transaction mode: (bits[3..0]: 0=unspecified; 1=standard; 2=subaddress; 3=combined) (default=2)
		@param retries The number of transaction retries to attempt before giving up.(default=0)
		@param timeout_us The number of microseconds to allow for a successful transaction before giving up.(default=0)
		@param options Optional transaction flags TBD.
		@result kIOReturnSuccess, or other IOReturn error code.
	*/
	virtual IOReturn writeI2C (
		UInt32	subAddress,
		UInt8	*data,
		UInt32	count,
		UInt32	clientKey = kIOI2C_CLIENT_KEY_DEFAULT,
		UInt32	mode = kI2CMode_StandardSub,
		UInt32	retries = 0,
		UInt32	timeout_uS = 0,
		UInt32	options = 0);

	/*!
		@method readI2C
		@abstract This method initiates an I2C read transaction to this drivers device.
		@discussion This is a convienience method to initialize an IOI2CCommand struct parameters and initiate the read transaction.
		@param subAddress The slave sub-address register. The format is as follows:
			bits[31..24] specify length: 0=8bit; 1=8bit; 2=16bit; 3=24bit.
			bits[23..0] specify subaddress:
				8bit subaddresses use bits[7..0],
				16bit subaddresses use bits[15..0],
				24bit subaddresses use bits[23..0].
		@param data Address of client allocated buffer which receives the read data.
		@param count Number of bytes to read into the data buffer.
		@param clientKey (optional) Either a valid key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT.
			If the client passes kIOI2C_CLIENT_KEY_DEFAULT this method will block waiting for exclusive access to the I2C bus. (default)
			If a valid key is used then this method is guaranteed to execute synchronously on the calling thread.
		@param mode The I2C transaction mode: (bits[3..0]: 0=unspecified; 1=standard; 2=subaddress; 3=combined) (default=3)
		@param retries The number of transaction retries to attempt before giving up. (default=0)
		@param timeout_us The number of microseconds to allow for a successful transaction before giving up. (default=0)
		@param options Optional transaction flags TBD.
		@result kIOReturnSuccess, or other IOReturn error code.
	*/
	virtual IOReturn readI2C (
		UInt32	subAddress,
		UInt8	*data,
		UInt32	count,
		UInt32	clientKey = kIOI2C_CLIENT_KEY_DEFAULT,
		UInt32	mode = kI2CMode_Combined,
		UInt32	retries = 0,
		UInt32	timeout_uS = 0,
		UInt32	options = 0);

	/*!
		@method writeI2C
		@abstract This method initiates an I2C write transaction to this drivers device.
		@discussion If the client passes kIOI2C_CLIENT_KEY_DEFAULT this method may block waiting for access to the I2C bus.
		If a valid key is used then this method is guaranteed to execute synchronously on the calling thread.
		@param cmd A pointer to an IOI2CCommand struct allocated by the caller.
		@param clientKey Either a valid key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT.
		@result kIOReturnSuccess, or other IOReturn error code.
	*/
	virtual IOReturn writeI2C(
		IOI2CCommand	*cmd,
		UInt32		clientKey);

	/*!
		@method readI2C
		@abstract This method initiates an I2C read transaction to this drivers device.
		@discussion If the client passes kIOI2C_CLIENT_KEY_DEFAULT this method may block waiting for access to the I2C bus.
		If a valid key is used then this method is guaranteed to execute synchronously on the calling thread.
		@param cmd A pointer to an IOI2CCommand struct allocated by the caller.
		@param clientKey Either a valid key returned from lockI2CBus or kIOI2C_CLIENT_KEY_DEFAULT.
		@result kIOReturnSuccess, or other IOReturn error code.
	*/
	virtual IOReturn readI2C(
		IOI2CCommand	*cmd,
		UInt32		clientKey);






	// Common IOPlatformFunction Methods...

	/*!
		@method InitializePlatformFunctions
		@abstract This method instanciates our providers IOPlatformFunctions and stores them in the fPlatformFuncArray.
		@discussion Any functions found with the kIOPFFlagOnInit flag set are performed immediately. The fPlatformFuncArray can be searched with the getPlatformFunction method.
		@result kIOReturnSuccess if found, or an IOReturn error code if instanciation failed.
	*/
	IOReturn InitializePlatformFunctions(void);

	/*!
		@method getPlatformFunction
		@abstract This method returns an IOPlatformFunction by name.
		@discussion The returned IOPlatformFunction is retained by the fPlatformFuncArray and is released upon termination. The PF can only be performed when I2C is online.
		@param functionSym OSSymbol of the function name.
		@param funcRef Address of an IOPlatformFunction pointer which upon success is set to the platform function address.
		@param flags Optional argument used to limit which function to match. By default (or if zero) any function flags will match.
		@result kIOReturnSuccess if found, or kIOReturnNotFound if not found.
	*/
	IOReturn getPlatformFunction (
		const OSSymbol		*functionSym,
		IOPlatformFunction	**funcRef,
		UInt32				flags = 0);

	/*!
		@constant kI2CPF_READ_BUFFER_LEN
		@abstract This is the maximum IOPlatformFunction read transaction length supported by the performFunction method.
	*/
	#define kI2CPF_READ_BUFFER_LEN	32

	enum
	{
		kPFMode_Dumb		= 1,
		kPFMode_Standard	= 2,
		kPFMode_Subaddress	= 3,
		kPFMode_Combined	= 4,
	};

	/*!
		@method performFunction
		@abstract This method may be called by the subclass to execute an "on demand" IOPlatformFunction.
		@param func the IOPlatformFunction to perform.
		@param pfParam1 optional argument.
		@param pfParam2 optional argument.
		@param pfParam3 optional argument.
		@param pfParam4 optional argument.
		@result kIOReturnSuccess, or an IOReturn error code.
	*/
	virtual IOReturn performFunction (
		IOPlatformFunction *func,
		void *pfParam1 = 0,
		void *pfParam2 = 0,
		void *pfParam3 = 0,
		void *pfParam4 = 0 );

	/*!
		@method performFunction
		@abstract This method may be called by the subclass to execute an "on demand" IOPlatformFunction by name.
		@param funcName the C string name of the IOPlatformFunction to perform.
		@param pfParam1 optional argument.
		@param pfParam2 optional argument.
		@param pfParam3 optional argument.
		@param pfParam4 optional argument.
		@result kIOReturnSuccess, or an IOReturn error code.
	*/
	virtual IOReturn performFunction (
		const char *funcName,
		void *pfParam1 = 0,
		void *pfParam2 = 0,
		void *pfParam3 = 0,
		void *pfParam4 = 0 );

	/*!
		@method performFunction
		@abstract This method may be called by the subclass to execute an "on demand" IOPlatformFunction by name.
		@param funcSym the symbol name of the IOPlatformFunction to perform.
		@param pfParam1 optional argument.
		@param pfParam2 optional argument.
		@param pfParam3 optional argument.
		@param pfParam4 optional argument.
		@result kIOReturnSuccess, or an IOReturn error code.
	*/
	virtual IOReturn performFunction (
		const OSSymbol *funcSym,
		void *pfParam1 = 0,
		void *pfParam2 = 0,
		void *pfParam3 = 0,
		void *pfParam4 = 0 );

	/*!
		@method performFunctionsWithFlags
		@abstract This method used to execute the IOPlatformFunctions which match the flags argument.
		@discussion This method is called automatically during power state transitions: on init, wake, sleep, termination.
	*/
	void performFunctionsWithFlags(UInt32 flags);

	


	// Power Management Methods...

	/*!
		@method InitializePowerManagement
		@abstract This method is called from the start method to initialize and start power management services.
		@discussion When this method is called this driver will be set to the kIOI2CPowerState_ON power state.
		And the kI2CPowerEvent_STARTUP event will immediately be sent to the processPowerEvent method.
	*/
	IOReturn InitializePowerManagement(void);

	/*!
		@method maxCapabilityForDomainState
		@abstract The policy maker calls this method to find out the highest power state possible for a given domain state
	*/
	virtual unsigned long maxCapabilityForDomainState ( IOPMPowerFlags domainState );

	/*!
		@enum kIOI2CPowerState_xxx
		@abstract Power state enumerations defined by this power controlling driver.
	*/
	enum
	{
		kIOI2CPowerState_OFF	= 0,
		kIOI2CPowerState_SLEEP,
		kIOI2CPowerState_DOZE,
		kIOI2CPowerState_ON,
		kIOI2CPowerState_COUNT
	};

	/*!
		@method getCurrentPowerState
		@abstract This method returns the current power state of this driver instance and may be used to process power state transitions.
		@discussion During power transition events the subclass can call this method to get the power state which we are transitioning from.
	*/
	UInt32 getCurrentPowerState(void);

private:
	/*!
		@method setPowerState
		@abstract This method is called by power management and is used to process power state transitions.
		@discussion This private method forwards control to the powerStateThreadCall method.
	*/
	virtual IOReturn setPowerState(
		unsigned long	newPowerState,
		IOService		*dontCare);

	enum
	{
		/*!
			@enum kSetPowerStateTimeout
			@abstract Timeout in microseconds to allow for processing power state transitions before being acknowledged.
			@discussion This timeout must not be exceeded. All transactions must be performed before the power state change times out.
				important If this timeout is exceeded the power change will occur regardless of being acknowledged and the device and driver may be left in an unstable state.
		*/
		kSetPowerStateTimeout = (100 * 1000 * 1000),

		/*!
			@enum kSysPowerDownTimeout
			@abstract Timeout in microseconds to allow for processing system power down or restart events before being acknowledged.
			@discussion This timeout must not be exceeded. All transactions must be performed before the shutdown occurs.
				important If this timeout is exceeded the system will shut down regardless of being acknowledged.
		*/
		kSysPowerDownTimeout = (20 * 1000 * 1000)
	};

	/*!
		@method powerStateThreadCall
		@abstract C to C++ glue for threadcall entrypoint.
		@discussion This static private method forwards control to the IOI2CDevice instance powerStateThreadCall method.
	*/
	static void sPowerStateThreadCall(
		thread_call_param_t	p0,
		thread_call_param_t	p1);

	/*!
		@method powerStateThreadCall
		@abstract Threadcall entrypoint used to process power state transitions.
		@discussion This private method handles I2C transaction synchronization during device power state transitions and system power-down and restarts.
	*/
	void powerStateThreadCall(
		unsigned long	newPowerState);

protected:
	/*!
		@enum kI2CPowerEvent_xxx
		@abstract Power state transition event enumerations are passed to the subclassed processPowerEvent method.
		@discussion Subclassed drivers process these events to save and restore their device state across sleep, etc.
	*/
	enum
	{
		kI2CPowerEvent_OFF,			/*! @enum kI2CPowerEvent_OFF @discussion called when device is powering off. */
		kI2CPowerEvent_SLEEP,		/*! @enum kI2CPowerEvent_SLEEP @discussion called when device is entering sleep. Subclass can save device state if necessary. */
		kI2CPowerEvent_DOZE,		/*! @enum kI2CPowerEvent_DOZE @discussion called when device is entering doze. */
		kI2CPowerEvent_WAKE,		/*! @enum kI2CPowerEvent_WAKE @discussion called when device is waking from sleep. Subclass can restore device state if necessary. */
		kI2CPowerEvent_ON,			/*! @enum kI2CPowerEvent_ON @discussion called when device power is turning on. */
		kI2CPowerEvent_STARTUP,		/*! @enum kI2CPowerEvent_STARTUP @discussion This is the first event sent when power management is started. No prior transaction to this device will be processed until this event occurs. Subclass can do device initialization transactions during this event if necessary. */
		kI2CPowerEvent_SHUTDOWN,	/*! @enum kI2CPowerEvent_SHUTDOWN @discussion called when system power shutting down. This event is the last chance to do transactions before the system shuts down or restarts. */

		kI2CPowerEvent_COUNT		/*! @enum kI2CPowerEvent_COUNT @discussion Number of power event enumerations. */
	};

	/*!
		@method processPowerEvent
		@abstract Subclassed drivers may override this processPowerEvent method to receive power state transition events.
		@discussion Power events to save and restore their device state across sleep, etc.
	*/
	virtual void processPowerEvent(UInt32 eventType);

	/*!
		@method isI2COffline
		@abstract Called to determine if this device is offline.
		@discussion If the device is offline then it is not capable of executing I2C transactions.
		@result true if offline, false if online.
	*/
	bool isI2COffline(void);

	/*!	@struct ExpansionData
		@discussion This structure helps to expand the capabilities of this class in the future.
	*/
	typedef struct ExpansionData
	{
		const OSSymbol	*symClientWrite;		// CallPlatformFunction Symbol for writing the I2C bus
		const OSSymbol	*symClientRead;			// CallPlatformFunction Symbol for reading the I2C bus
		const OSSymbol	*symPowerInterest;
		bool			fEnableOnDemandPlatformFunctions;
	};

	/* var reserved		Reserved for future use.  (Internal use only) */
	ExpansionData *reserved;

	#define symClientWrite		(reserved->symClientWrite)
	#define symClientRead		(reserved->symClientRead)
	#define symPowerInterest	(reserved->symPowerInterest)
	#define fEnableOnDemandPlatformFunctions	(reserved->fEnableOnDemandPlatformFunctions)

	/*
		Method space reserved for future expansion.
		According to the IOKit doc you can change each reserved method from private to protected or public as they become used.
		This should not break binary compatibility. Refer the following URL or a C++ reference for more info on these keywords.
		http://developer.apple.com/documentation/DeviceDrivers/Conceptual/WritingDeviceDriver/CPluPlusRuntime/chapter_2_section_5.html
	*/
protected:
    OSMetaClassDeclareReservedUsed ( IOI2CDevice,  0 );

	IOReturn getPlatformFunction (
		const char		*functionName,
		IOPlatformFunction	**funcRef,
		UInt32				flags = 0);

    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  1 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  2 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  3 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  4 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  5 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  6 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  7 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  8 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  9 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  10 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  11 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  12 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  13 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  14 );
    OSMetaClassDeclareReservedUnused ( IOI2CDevice,  15 );
};

#endif // _IOI2CDevice_H
