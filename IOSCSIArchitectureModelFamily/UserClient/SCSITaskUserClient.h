/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_SCSI_TASK_USER_CLIENT_H_
#define _IOKIT_SCSI_TASK_USER_CLIENT_H_

#if defined(KERNEL) && defined(__cplusplus)

#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/scsi-commands/SCSITask.h>
#include <IOKit/scsi-commands/IOSCSIProtocolInterface.h>
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"


class SCSITaskUserClient : public IOUserClient
{
	
	OSDeclareDefaultStructors ( SCSITaskUserClient );
	
public:
	
	virtual bool initWithTask ( task_t owningTask, void * securityToken, UInt32 type );
	
    virtual bool init ( OSDictionary * dictionary = 0 );
    virtual bool start ( IOService * provider );
	
    virtual IOReturn message ( UInt32 type, IOService * provider, void * arg );
	
    virtual IOReturn clientClose ( void );
    virtual IOReturn clientDied ( void );
	
	virtual IOReturn HandleTermination ( IOService * provider );
	
	virtual IOReturn IsExclusiveAccessAvailable ( void );	
	virtual IOReturn ObtainExclusiveAccess ( void );
	virtual IOReturn ReleaseExclusiveAccess ( void );
	
    virtual IOReturn CreateTask ( SCSITask ** outSCSITask, void *, void *, void *, void *, void * );
    virtual IOReturn ReleaseTask ( SCSITask * inSCSITask, void *, void *, void *, void *, void * );
	virtual IOReturn ExecuteTaskAsync ( SCSITask * inSCSITask, void *, void *, void *, void *, void * );
	virtual IOReturn ExecuteTaskSync ( SCSITask * inSCSITask, vm_address_t senseDataBuffer,
									  SCSITaskStatus * taskStatus, UInt32 * tranferCountHi,
									  UInt32 * tranferCountLo, void * );
	virtual IOReturn AbortTask ( SCSITask * inSCSITask, void *, void *, void *, void *, void * );
    virtual IOReturn SetAsyncCallback ( OSAsyncReference asyncRef, SCSITask * inTask,
										void * callback, void * refCon, void *, void * );
	
	virtual IOReturn IsTaskActive ( SCSITask * inSCSITask, UInt32 * active, void *, void *, void *, void * );
	virtual IOReturn SetTransferDirection ( SCSITask * inSCSITask, UInt32 transferDirection, void *, void *, void *, void * );
	virtual IOReturn SetTaskAttribute ( SCSITask * inSCSITask, UInt32 attribute, void *, void *, void *, void * );
	virtual IOReturn GetTaskAttribute ( SCSITask * inSCSITask, UInt32 * attribute, void *, void *, void *, void * );
	virtual IOReturn SetCommandDescriptorBlock ( SCSITask * inSCSITask, UInt32 size, UInt8 * cdb, UInt32 dataSize, void *, void * );
	
	virtual IOReturn SetScatterGatherList ( SCSITask * inSCSITask, UInt32 inScatterGatherEntries,
											UInt32 inTransferCountHi, UInt32 inTransferCountLo,
											UInt32 inTransferDirection, IOVirtualRange * inScatterGatherList );
	
	virtual IOReturn SetSenseDataBuffer ( SCSITask * inSCSITask, vm_address_t buffer, UInt32 bufferSize, void *, void *, void * );
	
	virtual IOReturn SetTimeoutDuration ( SCSITask * inSCSITask, UInt32 timeoutMS, void *, void *, void *, void * );
	virtual IOReturn GetTimeoutDuration ( SCSITask * inSCSITask, UInt32 * timeoutMS, void *, void *, void *, void * );
	
	virtual IOReturn GetTaskStatus ( SCSITask * inSCSITask, SCSITaskStatus * attribute, void *, void *, void *, void * );
	virtual IOReturn GetSCSIServiceResponse ( SCSITask * inSCSITask, SCSIServiceResponse * serviceResponse, void *, void *, void *, void * );
	virtual IOReturn GetTaskState ( SCSITask * inSCSITask, SCSITaskState * taskState, void *, void *, void *, void * );
	virtual IOReturn GetRealizedDataTransferCount ( SCSITask * inSCSITask, UInt64 * transferCount, void *, void *, void *, void * );
	virtual IOReturn GetAutoSenseData ( SCSITask * inSCSITask, SCSI_Sense_Data * senseDataBuffer, void *, void *, void *, void * );
	
	// MMC-2 Device methods
	virtual IOReturn Inquiry ( UInt32 inqBufferSize, vm_address_t inqBuffer, vm_address_t senseBuffer,
							  UInt32 * outTaskStatus, void *, void * );

	virtual IOReturn TestUnitReady ( vm_address_t senseDataBuffer, UInt32 * outTaskStatus, void *, void *, void *, void * );
	virtual IOReturn GetPerformance ( UInt32 TOLERANCE_WRITE_EXCEPT, UInt32 STARTING_LBA, UInt32 MAXIMUM_NUMBER_OF_DESCRIPTORS_AND_BUFFER_SIZE,
									  vm_address_t performanceBuffer, vm_address_t senseDataBuffer, UInt32 * outTaskStatus );
	virtual IOReturn GetConfiguration ( UInt32 RT, UInt32 STARTING_FEATURE_NUMBER, vm_address_t configBuffer, UInt32 bufferSize,
										vm_address_t senseDataBuffer, UInt32 * outTaskStatus );
	virtual IOReturn ModeSense10 ( UInt32 LLBAAandDBD, UInt32 PCandPageCode, vm_address_t pageBuffer,
								  UInt32 bufferSize, vm_address_t senseDataBuffer, UInt32 * outTaskStatus );
	virtual IOReturn SetWriteParametersModePage ( vm_address_t paramBuffer, UInt32 bufferSize, vm_address_t senseDataBuffer, UInt32 * outTaskStatus );
	virtual IOReturn GetTrayState ( UInt32 * trayState );
	virtual IOReturn SetTrayState ( UInt32 trayState );
	virtual IOReturn ReadTableOfContents ( UInt32 MSF_FORMAT, UInt32 TRACK_SESSION_NUMBER, vm_address_t tocBuffer,
										  UInt32 bufferSize, vm_address_t senseDataBuffer, UInt32 * outTaskStatus );
	virtual IOReturn ReadDiscInformation ( vm_address_t discInfoBuffer, UInt32 bufferSize, vm_address_t senseDataBuffer, UInt32 * outTaskStatus, void *, void * );
	virtual IOReturn ReadTrackInformation ( UInt32 ADDRESS_NUMBER_TYPE, UInt32 LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
											vm_address_t trackInfoBuffer, UInt32 bufferSize, vm_address_t senseDataBuffer,
											UInt32 * outTaskStatus );
	virtual IOReturn ReadDVDStructure ( UInt32 LBA, UInt32 LAYER_FORMAT, vm_address_t dvdBuffer,
									    UInt32 bufferSize, vm_address_t senseDataBuffer, UInt32 * outTaskStatus );
	
protected:
	
	static IOExternalMethod			sMethods[kSCSITaskUserClientMethodCount];
	static IOExternalAsyncMethod	sAsyncMethods[kSCSITaskUserClientAsyncMethodCount];
	
	task_t							fTask;
	IOService *						fProvider;
	IOSCSIProtocolInterface *		fProtocolInterface;
	OSSet *							fSetOfSCSITasks;
	
	virtual IOExternalAsyncMethod *	getAsyncTargetAndMethodForIndex ( IOService ** target, UInt32 index );	
	virtual IOExternalMethod *		getTargetAndMethodForIndex ( IOService ** target, UInt32 index );
	
	virtual SCSIServiceResponse		SendCommand ( SCSITask * request );
	static	void 					sTaskCallback ( SCSITaskIdentifier completedTask );
	static	void 					sAsyncTaskCallback ( SCSITaskIdentifier completedTask );
	
};

#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif /* ! _IOKIT_SCSI_TASK_USER_CLIENT_H_ */