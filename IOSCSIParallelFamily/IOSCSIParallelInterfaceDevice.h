/*
 * Copyright (c) 2002-2008 Apple Inc. All rights reserved.
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

#ifndef __IOKIT_IO_SCSI_PARALLEL_INTERFACE_DEVICE_H__
#define __IOKIT_IO_SCSI_PARALLEL_INTERFACE_DEVICE_H__


 /*!
  @header IOSCSIParallelInterfaceDevice
	The IOSCSIParallelInterfaceDevice class represents a target device
	on a SCSI bus.
*/


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <kern/queue.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/IOSCSIProtocolServices.h>

// SCSI Parallel Family Headers
#include "IOSCSIParallelInterfaceController.h"
#include "SCSIParallelTask.h"


//-----------------------------------------------------------------------------
//	Class Declarations
//-----------------------------------------------------------------------------

class IOSCSIParallelInterfaceDevice: public IOSCSIProtocolServices
{
	
	OSDeclareDefaultStructors ( IOSCSIParallelInterfaceDevice )
	
#if 0	
#pragma mark -
#pragma mark Client API
#endif
	
public:
	
	/*!
		@function IsProtocolServiceSupported
		@abstract Called by SCSI Protocol Layer to determine if a specific SCSIProtocolFeature
		is supported by this protocol layer driver and the device it controls.
		@discussion	Called by SCSI Protocol Layer to determine if a specific SCSIProtocolFeature
		is supported by this protocol layer driver and the device it controls.
		@param feature Requested feature selector.
		@param serviceValue pointer to service value see enum comment for type information
		@result returns true if requested feature is supported.
	*/
	virtual bool	IsProtocolServiceSupported (
									SCSIProtocolFeature 		feature,
									void * 						serviceValue );

	/*!
		@function HandleProtocolServiceFeature
		@abstract Called by SCSI Protocol Layer to handle a specific SCSIProtocolFeature.
		@discussion	Called by SCSI Protocol Layer to handle a specific SCSIProtocolFeature.
		@param feature requested feature selector
		@param serviceValue pointer to service value see enum comment for type information
		@result returns true if service feature is handled properly.
	*/
	virtual bool	HandleProtocolServiceFeature (
									SCSIProtocolFeature 		feature, 
									void * 						serviceValue );
	
	
	/*
	 * Member routines for services available to the client and controller.
	 */
	
	/*!
		@function GetTargetIdentifier
		@abstract Method to retrieve the SCSITargetIdentifier.
		@discussion	Method to allow the client to query for the SCSITargetIndentifier
		of the SCSI device represented by the object.
		@result returns SCSITargetIdentifier.
	*/
	SCSITargetIdentifier	GetTargetIdentifier ( void );

	/*!
		@function FindTaskForAddress
		@abstract Method to retrieve a SCSIParallelTaskIdentifier given a Lun and Tag.
		@discussion	Find the outstanding task for the Task Address of this Target and the 
		specified Lun and Tag.
		@param theL the LUN
		@param theQ the tagged task identifier which represents the queue.
		@result returns A valid SCSIParallelTaskIdentifier or NULL.
	*/
	SCSIParallelTaskIdentifier	FindTaskForAddress ( 
									SCSILogicalUnitNumber		theL,
									SCSITaggedTaskIdentifier	theQ );
	
	
	SCSIParallelTaskIdentifier	FindTaskForControllerIdentifier ( 
									UInt64						theIdentifier );
							
	bool	SetInitialTargetProperties ( OSDictionary * properties );
	
	bool	SetTargetProperty (
					const char * 	key,
					OSObject *		value );
	
	void	RemoveTargetProperty ( const char * key );
	
	bool	IsFeatureNegotiationNecessary ( SCSIParallelFeature	feature );
	
	/*
	 * Member routines for services available only to controller.
	 */
	
	/*!
		@function GetHBADataPointer
		@abstract Method to retrieve the HBA Data Pointer for HBA specific data associated
		with the target device.
		@discussion	Method to retrieve the HBA Data Pointer for HBA specific data associated
		with the target device.
		@result returns pointer to HBA data buffer or NULL if not found or size is zero.
	*/
	void *	GetHBADataPointer ( void );
	
	/*!
		@function GetHBADataSize
		@abstract Method to retrieve the HBA Data Size in bytes.
		@discussion	Method to retrieve the HBA Data Size in bytes.
		@result returns size of HBA data buffer in bytes.
	*/
	UInt32	GetHBADataSize ( void );
	
	/*!
		@function CompleteSCSITask
		@abstract Method called to complete a SCSIParallelTask.
		@discussion	Method called to complete a SCSIParallelTask.
		@param completedTask A valid SCSIParallelTaskIdentifier.
		@param serviceResponse A valid SCSIServiceResponse.
		@param taskStatus A valid SCSITaskStatus.
	*/
	virtual void	CompleteSCSITask (
									SCSIParallelTaskIdentifier 	completedTask,
									SCSIServiceResponse 		serviceResponse,
									SCSITaskStatus 				taskStatus );
	
	
	/*
	 * Member routines for services available only to SCSI Parallel Family.
	 */
	
	/*!
		@function CreateTarget
		@abstract Creates a IOSCSIParallelInterfaceDevice object which represents
		the target device at the specified target ID.
		@discussion	Creates a IOSCSIParallelInterfaceDevice object which represents
		the target device at the specified target ID.
		@param targetID A valid SCSITargetIdentifier.
		@param sizeOfHBAData Size in bytes of HBA specific data required
		by the controller. This value can be zero if no data is required.
		@result return a pointer to IOSCSIParallelInterfaceDevice
	*/
	static IOSCSIParallelInterfaceDevice *	CreateTarget ( 
									SCSITargetIdentifier 		targetID,
									UInt32 						sizeOfHBAData,
									IORegistryEntry *			entry = NULL );
	
	/*!
		@function DestroyTarget
		@abstract Destroys a IOSCSIParallelInterfaceDevice object which represents
		the target device.
		@discussion	The object will be destroyed.
	*/
	void	DestroyTarget ( void );
	
	/*!
		@function GetPreviousDeviceInList
		@abstract Method to retrieve previous device in internal device list.
		@discussion	Method to retrieve previous device in internal device list.
		@result returns a pointer to IOSCSIParallelInterfaceDevice, or NULL if none found.
	*/
	IOSCSIParallelInterfaceDevice *	GetPreviousDeviceInList ( void );
	
	/*!
		@function SetPreviousDeviceInList
		@abstract Method to set previous device in internal device list.
		@discussion	Method to set previous device in internal device list.
		@param device is a pointer to a valid IOSCSIParallelInterfaceDevice object.
	*/
	void	SetPreviousDeviceInList ( IOSCSIParallelInterfaceDevice * device );
	
	/*!
		@function GetNextDeviceInList
		@abstract Method to retrieve next device in internal device list.
		@discussion	Method to retrieve next device in internal device list.
		@result returns a pointer to IOSCSIParallelInterfaceDevice, or NULL if none found.
	*/
	IOSCSIParallelInterfaceDevice *	GetNextDeviceInList ( void );
	
	/*!
		@function SetNextDeviceInList
		@abstract Method to set next device in internal device list.
		@discussion	Method to set next device in internal device list.
		@param device is a pointer to a valid IOSCSIParallelInterfaceDevice object.
	*/
	void 	SetNextDeviceInList ( IOSCSIParallelInterfaceDevice * device );
	
#if 0
#pragma mark -
#pragma mark Child Class API
#endif
	
protected:
	
	/*!
		@function InitTarget
		@asbtract Method called to initialize a device.
		@discussion Method called to initialize a device.
		@param targetID A valid SCSITargetIdentifier.
		@param sizeOfHBAData The size of the HBA specific data to allocate.
		@param entry A registry entry.
		@result returns a boolean value indicating successful initialization.
	*/
	bool	InitTarget ( SCSITargetIdentifier 		targetID, 
						 UInt32 					sizeOfHBAData,
						 IORegistryEntry *			entry );
	
	/*!
		@function ExecuteParallelTask
		@abstract Method called to execute a client request.
		@discussion	Method called to execute a client request.
		@param parallelRequest A valid SCSIParallelTaskIdentifier
		which represents the client request.
		@result returns A valid SCSIServiceResponse.
	*/
	SCSIServiceResponse	ExecuteParallelTask (
							SCSIParallelTaskIdentifier parallelRequest );
	
	/*!
		@function GetSCSIParallelTask
		@abstract Method to retrieve a SCSIParallelTaskIdentifier in order
		to process a client request.
		@discussion	Method to retrieve a SCSIParallelTaskIdentifier in order
		to process a client request.
		@param blockForCommand If true, the thread calling this method will
		block until a command becomes available. If false, it will not block
		and could possibly return NULL.
		@result returns If blockForCommand is true, this call is guaranteed
		to return a valid SCSIParallelTaskIdentifier. If blockForCommand is
		false, it may return a valid SCSIParallelTaskIdentifier or NULL.
	*/
	SCSIParallelTaskIdentifier 	GetSCSIParallelTask ( bool blockForCommand );
	
	/*!
		@function FreeSCSIParallelTask
		@abstract Method to free a SCSIParallelTaskIdentifier after processing
		a client request.
		@discussion	Method to free a SCSIParallelTaskIdentifier after processing
		a client request.
		@param returnTask A valid SCSIParallelTaskIdentifier.
	*/
	void	FreeSCSIParallelTask ( SCSIParallelTaskIdentifier returnTask );
	
	/*!
		@function DoesHBASupportSCSIParallelFeature
		@abstract Method to query the HBA for support of a specific SCSIParallelFeature.
		@discussion	Method to query the HBA for support of a specific SCSIParallelFeature.
		@param theFeature A valid SCSIParallelFeature.
		@result returns true if requested feature is supported.
	*/
	bool	DoesHBASupportSCSIParallelFeature ( SCSIParallelFeature theFeature );
	
	// SCSI Parallel Task access member routines

	/*!
		@function AddToOutstandingTaskList
		@abstract Adds specified SCSIParallelTaskIdentifier to outstanding task list.
		@discussion	Adds specified SCSIParallelTaskIdentifier to outstanding task list.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns true if successful
	*/
	bool	AddToOutstandingTaskList ( 
							SCSIParallelTaskIdentifier 	parallelTask );
	
	/*!
		@function RemoveFromOutstandingTaskList
		@abstract Removes specified SCSIParallelTaskIdentifier from outstanding task list. 
		@discussion	Removes specified SCSIParallelTaskIdentifier from outstanding task list.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
	*/
	void	RemoveFromOutstandingTaskList ( 
					SCSIParallelTaskIdentifier 	parallelTask );
	
	bool	AddToResendTaskList ( 
					SCSIParallelTaskIdentifier	parallelTask );

	void	RemoveFromResendTaskList ( 
					SCSIParallelTaskIdentifier 	parallelTask );
	
	void	SendFromResendTaskList ( void );
	
	// ---- Methods for Accessing the local data in the SCSI Parallel Task Object ----
	
	/*!
		@function SetSCSITaskIdentifier
		@abstract Method to set the SCSITaskIdentifier to be associated
		with a SCSIParallelTaskIdentifier.
		@discussion Method to set the SCSITaskIdentifier to be associated
		with a SCSIParallelTaskIdentifier.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param scsiRequest SCSITaskIdentifier that represents the original request from the SCSI 
		Application Layer client.
		@result returns true if successful.
	*/
	bool	SetSCSITaskIdentifier ( 
					SCSIParallelTaskIdentifier	parallelTask, 
					SCSITaskIdentifier			scsiRequest );
	
	/*!
		@function GetSCSITaskIdentifier
		@abstract Method to retrieve the SCSITaskIdentifier associated
		with a SCSIParallelTaskIdentifier.
		@discussion Method to retrieve the SCSITaskIdentifier associated
		with a SCSIParallelTaskIdentifier.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns SCSITaskIdentifier that represents the original request from the SCSI 
		Application Layer client.
	*/
	SCSITaskIdentifier	GetSCSITaskIdentifier ( 
								SCSIParallelTaskIdentifier 	parallelTask );
			
	/*!
		@function SetDevice
		@abstract Method to set the device to be associated
		with a SCSIParallelTaskIdentifier.
		@discussion Method to set the device to be associated
		with a SCSIParallelTaskIdentifier.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param device A pointer to a valid  IOSCSIParallelInterfaceDevice.
		@result returns true if successful.
	*/
	bool				SetDevice ( 
							SCSIParallelTaskIdentifier			parallelTask,
							IOSCSIParallelInterfaceDevice * 	device );

	/*!
		@function SetTargetIdentifier
		@abstract Set Target Identifier
		@discussion xxx
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param theTargetID
		@result returns true if successful
	*/
	bool	SetTargetIdentifier ( 
								SCSIParallelTaskIdentifier 	parallelTask,
								SCSITargetIdentifier 		theTargetID );

	/*!
		@function GetTargetIdentifier
		@abstract Get Target Identifier
		@discussion Method to retrieve the Target Identifier for this request.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns SCSITargetIdentifier
	*/
	SCSITargetIdentifier	GetTargetIdentifier ( 
								SCSIParallelTaskIdentifier 	parallelTask );

	/*!
		@function SetDMABuffer
		@abstract Set DMA buffer
		@discussion xxx
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param buffer A valid IOMemoryDescriptor.
		@result returns IOReturn value.
	*/
	IOReturn	SetDMABuffer ( 
							SCSIParallelTaskIdentifier 	parallelTask,
							IOMemoryDescriptor *		buffer );
	
	// ---- Methods for Accessing data in the client's SCSI Task Object ----	
	// Method to retrieve the LUN that identifies the Logical Unit whose Task
	// Set to which this task is to be added.
	/*!
		@function GetLogicalUnitNumber
		@abstract Get Logical Unit Number
		@discussion xxx
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns a SCSILogicalUnitNumber
	*/
	SCSILogicalUnitNumber	GetLogicalUnitNumber ( SCSIParallelTaskIdentifier parallelTask );
	
	/*!
		@function GetTaggedTaskIdentifier
		@abstract Method to retrieve the SCSI Tagged Task Identifier of the 
		task.  If the returned value is equal to kSCSIUntaggedTaskIdentifier,
		then this task is untagged.  
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result an SCSITaskAttribute value.
	*/
	
	SCSITaggedTaskIdentifier GetTaggedTaskIdentifier (
							SCSIParallelTaskIdentifier	parallelTask );

	/*!
		@function GetTaskAttribute
		@abstract Method to retrieve the SCSI Task Attribute of the task 
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result an SCSITaskAttribute value.
	*/
	
	SCSITaskAttribute		GetTaskAttribute (
							SCSIParallelTaskIdentifier	parallelTask );

	/*!
		@function GetCommandDescriptorBlockSize
		@abstract Get the size of Command Descriptor Block(CDB) Size
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns the size of the Command Descriptor Block in bytes
	*/
	UInt8	GetCommandDescriptorBlockSize ( SCSIParallelTaskIdentifier parallelTask );
	
	/*!
		@function GetCommandDescriptorBlock
		@abstract xxx
		@discussion This will always return a 16 Byte CDB.  If the Protocol Layer driver
		does not support 16 Byte CDBs, it will have to create a local SCSICommandDescriptorBlock
		variable to get the CDB data and then transfer the needed bytes from there.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param cdbData is a SCSICommandDescriptorBlock pointer to 16 byte CDB
		@result returns true if data was copied to cdbData pointer
	*/
	bool	GetCommandDescriptorBlock ( 
							SCSIParallelTaskIdentifier 		parallelTask,
							SCSICommandDescriptorBlock * 	cdbData );

	/*!
		@function GetDataTransferDirection
		@abstract Get Data Transfer Direction
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result xxx
	*/
	UInt8	GetDataTransferDirection ( SCSIParallelTaskIdentifier parallelTask );

	/*!
		@function GetRequestedDataTransferCount
		@abstract Get Requested Data Transfer Count
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result xxx
	*/
	UInt64	GetRequestedDataTransferCount ( SCSIParallelTaskIdentifier parallelTask );
	
	/*!
		@function GetRealizedDataTransferCount
		@abstract Get Realized Data Transfer Count
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result xxx
	*/
	UInt64	GetRealizedDataTransferCount ( 
							SCSIParallelTaskIdentifier 	parallelTask );

	/*!
		@function SetRealizedDataTransferCount
		@abstract Set Realized Data Transfer Count
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param realizedTransferCountInBytes
		@result xxx
	*/
	bool	SetRealizedDataTransferCount ( 
							SCSIParallelTaskIdentifier 	parallelTask,
							UInt64 						realizedTransferCountInBytes );

	/*!
		@function IncrementRealizedDataTransferCount
		@abstract Increment Realized Data Transfer Count
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param realizedTransferCountInBytes
	*/
	void	IncrementRealizedDataTransferCount ( 
							SCSIParallelTaskIdentifier 	parallelTask,
							UInt64 						realizedTransferCountInBytes );
	/*!
		@function GetDataBuffer
		@abstract Accessor to get client buffer
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns pointer IOMemoryDescriptor of data buffer 
	*/
	IOMemoryDescriptor * GetDataBuffer ( SCSIParallelTaskIdentifier parallelTask );

	/*!
		@function GetDataBufferOffset
		@abstract Accessor to get offset into client buffer
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns offset in bytes
	*/
	UInt64	GetDataBufferOffset ( SCSIParallelTaskIdentifier parallelTask );

	/*!
		@function GetTimeoutDuration
		@abstract Accessor to get client timeout duration
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns timeout duration in microseconds
	*/
	UInt32	GetTimeoutDuration ( SCSIParallelTaskIdentifier parallelTask );
	
	/*!
		@function SetWideDataTransferNegotiationResult
		@abstract Set Wide Data Transfer Negotiation Result
		@discussion Set SCSI Parallel Device object if Wide Data Transfers need to be negotiated.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param newControl SCSIParallelFeatureControl
	*/
	void	SetSCSIParallelFeatureNegotiation ( 
							SCSIParallelTaskIdentifier 	parallelTask,
							SCSIParallelFeature 		requestedFeature, 
							SCSIParallelFeatureRequest 	newRequest );

	/*!
		@function GetWideDataTransferNegotiation
		@abstract Get Wide Data Transfer Negotiation
		@discussion Query as to whether the SCSI Parallel Device object has negotiated
		Wide Data Transfers.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param newControl SCSIParallelFeatureControl
	*/
	SCSIParallelFeatureRequest		GetSCSIParallelFeatureNegotiation ( 
							SCSIParallelTaskIdentifier 	parallelTask,
							SCSIParallelFeature 		requestedFeature );
	
	/*!
		@function GetSCSIParallelFeatureNegotiationCount
		@abstract Method to retrieve the number of requested negotiations.
		@discussion Query as to the number of SCSI Parallel Features that are
		requested to either be negotitated or cleared.  These are all features
		that are set to either kSCSIParallelFeature_AttemptNegotiation or 
		kSCSIParallelFeature_ClearNegotiation.  If the return value is zero,
		then all features are set to kSCSIParallelFeature_NoNegotiation
		and all feature negotiations are to remain as they currently exist.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result an unsigned integer up to 64 bits in size.
	*/
	
	UInt64		GetSCSIParallelFeatureNegotiationCount ( 
							SCSIParallelTaskIdentifier 	parallelTask);
	
	/*!
		@function GetWideDataTransferNegotiationResult
		@abstract Get Wide Data Transfer Negotiation Result
		@discussion Query as to whether the SCSI Parallel Controller object has negotiated
		Wide Data Transfers.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@param newControl SCSIParallelFeatureControl
	*/
	SCSIParallelFeatureResult		GetSCSIParallelFeatureNegotiationResult ( 
							SCSIParallelTaskIdentifier 	parallelTask,
							SCSIParallelFeature 		requestedFeature );
   
	/*!
		@function GetSCSIParallelFeatureNegotiationResultCount
		@abstract Method to retrieve the number of changed negotiations.
		@discussion Query as to the number of SCSI Parallel Features that have
		been changed to either negotitated or cleared.  These are all features
		that are set to either kSCSIParallelFeature_NegotitiationCleared or 
		kSCSIParallelFeature_NegotitiationSuccess.  If the return value is zero,
		then all features are set to kSCSIParallelFeature_NegotitiationUnchanged.
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result an unsigned integer up to 64 bits in size.
	*/
	
	UInt64		GetSCSIParallelFeatureNegotiationResultCount ( 
							SCSIParallelTaskIdentifier 	parallelTask);
	
	UInt64		GetControllerTaskIdentifier (
							SCSIParallelTaskIdentifier 	parallelTask);


	// The HBA Data related fields

	/*!
		@function GetHBADataSize
		@abstract Accessor to Get HBA Data Size
		@discussion xxx
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns HBA Data size in bytes
	*/
	UInt32	GetHBADataSize ( SCSIParallelTaskIdentifier parallelTask );

	/*!
		@function GetHBADataPointer
		@abstract Accessor to Get HBA Data Pointer
		@discussion xxx
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns pointer to buffer for HBA specific data, NULL if 
		none found or GetHBADataSize is zero.
	*/
	void *	GetHBADataPointer ( SCSIParallelTaskIdentifier parallelTask );

	/*!
		@function GetHBADataDescriptor
		@abstract Accessor to Get HBA Data Descriptor
		@discussion xxx
		@param parallelTask A valid SCSIParallelTaskIdentifier.
		@result returns pointer to IOMemoryDescriptor that wraps the HBA specific
		data buffer, NULL if none found or GetHBADataSize is zero.
	*/
	IOMemoryDescriptor * 	GetHBADataDescriptor ( 
								SCSIParallelTaskIdentifier 	parallelTask );


	void 					InitializePowerManagement ( IOService * provider );
	
	
#if 0
#pragma mark -
#pragma mark For Internal Use Only
#endif
	
public:
	
	/*
	 * IOService support member routines.
	 */
	bool		start ( IOService * provider );
	void		stop ( IOService *  provider );
	void		free ( void );
	bool		finalize ( IOOptionBits options );	
	
	IOReturn	message ( UInt32 clientMsg, IOService * forProvider, void * forArg = 0 );
	IOReturn	requestProbe ( IOOptionBits options );
	
	/*
	 * IOSCSIProtocolServices support member routines.
	 */
	
	// return false when we are busy and command can't be taken
	virtual bool SendSCSICommand ( 	SCSITaskIdentifier 			request,
									SCSIServiceResponse * 		serviceResponse,
									SCSITaskStatus * 			taskStatus );
	
	// This member routine is obsoleted and should not be used by a client.
	virtual SCSIServiceResponse AbortSCSICommand ( SCSITaskIdentifier request );

protected:
	
	
	SCSIServiceResponse		HandleAbortTask ( 
											UInt8 						theLogicalUnit, 
											SCSITaggedTaskIdentifier 	theTag );
	
	SCSIServiceResponse		HandleAbortTaskSet ( 
											UInt8 						theLogicalUnit );
	
	SCSIServiceResponse		HandleClearACA ( 
											UInt8 						theLogicalUnit );
	
	SCSIServiceResponse		HandleClearTaskSet (
											UInt8 						theLogicalUnit );
	
	SCSIServiceResponse		HandleLogicalUnitReset (
											UInt8 						theLogicalUnit );
											
	SCSIServiceResponse		HandleTargetReset ( void );
	
private:
	
	// The SCSI Target Identifer for this device
	SCSITargetIdentifier		fTargetIdentifier;
	
	// Member variables that indicate if the target supports wide transfers
	// and if it has been succesfully negotiated.
	bool						fITNexusSupportsFeature[kSCSIParallelFeature_TotalFeatureCount];
	bool						fFeatureIsNegotiated[kSCSIParallelFeature_TotalFeatureCount];
	
	// This is the size and space of the HBA data as requested
	// when the device object was created.
	UInt32						fHBADataSize;
	void *						fHBAData;
	
	// Lock for controlling access to the queues.
	IOSimpleLock *				fQueueLock;
	queue_head_t				fOutstandingTaskList;
	queue_head_t				fResendTaskList;
	bool						fAllowResends;
	bool						fResendThreadScheduled;
	bool						fMultiPathSupport;
	
	IOSCSIParallelInterfaceController *	fController;
	
	// Member variables to maintain the previous and next element in the 
	// Parallel device list.
	IOSCSIParallelInterfaceDevice *		fPreviousParallelDevice;
	IOSCSIParallelInterfaceDevice *		fNextParallelDevice;
	
	// Member routine to query the device for SCSI Parallel Features supported
	// such as synchronous negotiation, wide negotiation, qas,
	// tagged command queueing, etc.
	void 		DetermineParallelFeatures ( UInt8 * inqData );
	
};


#endif	/* __IOKIT_IO_SCSI_PARALLEL_INTERFACE_DEVICE_H__ */