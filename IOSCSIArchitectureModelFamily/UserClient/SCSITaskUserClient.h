/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_SCSI_TASK_USER_CLIENT_H_
#define _IOKIT_SCSI_TASK_USER_CLIENT_H_


#if defined(KERNEL) && defined(__cplusplus)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// IOKit includes
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/SCSITask.h>
#include <IOKit/scsi/IOSCSIProtocolInterface.h>

// Private includes
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "SCSITaskDefinition.h"

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

enum
{
	kMaxSCSITaskArraySize	= 16
};

enum
{
	kCommandTypeExecuteSync		= 0,
	kCommandTypeExecuteAsync	= 1,
	kCommandTypeNonExclusive	= 2
};

// Forward class declaration
class SCSITaskUserClient;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Typedefs
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

struct SCSITaskRefCon
{
	SCSITaskUserClient *	self;
	IOMemoryDescriptor *	taskResultsBuffer;
	OSAsyncReference		asyncReference;
	UInt32					commandType;
};
typedef struct SCSITaskRefCon SCSITaskRefCon;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


class SCSITaskUserClient : public IOUserClient
{
	
	OSDeclareDefaultStructors ( SCSITaskUserClient );
	
public:
	
	virtual bool 	initWithTask 		( task_t owningTask, void * securityToken, UInt32 type );
	
    virtual bool 	start 				( IOService * provider );
	virtual void	free				( void );
	
    virtual IOReturn clientClose 		( void );
	
	virtual IOReturn IsExclusiveAccessAvailable ( void );
	virtual IOReturn ObtainExclusiveAccess 	( void );
	virtual IOReturn ReleaseExclusiveAccess ( void );
	
    virtual IOReturn CreateTask 		( SInt32 * taskReference );
    virtual IOReturn ReleaseTask 		( SInt32 taskReference );
	virtual IOReturn ExecuteTask 		( SCSITaskData * args, UInt32 argSize );
	virtual IOReturn AbortTask 			( SInt32 taskReference );
	virtual IOReturn SetBuffers 		( SInt32 taskReference,
										  vm_address_t results,
										  vm_address_t senseData,
										  UInt32 senseBufferSize );
	
    virtual IOReturn SetAsyncCallback 	( OSAsyncReference asyncRef,
    									  SInt32 taskReference,
										  void * callback,
										  void * userRefCon );
	
	// MMC Device methods
	virtual IOReturn Inquiry 			( AppleInquiryStruct * 	inquiryData,
							  			  SCSITaskStatus * 		taskStatus,
							  			  UInt32				inStructSize,
							  			  UInt32 *				outStructSize );

	virtual IOReturn TestUnitReady 		( vm_address_t 		senseDataBuffer,
										  SCSITaskStatus * 	outTaskStatus,
										  UInt32 * 			outStructSize );

	virtual IOReturn GetPerformance 	( AppleGetPerformanceStruct * 	performanceData,
									 	  SCSITaskStatus * 				taskStatus,
									 	  UInt32 						inStructSize,
									 	  UInt32 * 						outStructSize );

	virtual IOReturn GetConfiguration 	( AppleGetConfigurationStruct * 	configData,
									   	  SCSITaskStatus * 					taskStatus,
									   	  UInt32 							inStructSize,
									   	  UInt32 * 							outStructSize );

	virtual IOReturn ModeSense10 		( AppleModeSense10Struct * 	modeSenseData,
								  		  SCSITaskStatus * 			taskStatus,
								  		  UInt32 					inStructSize,
								  		  UInt32 * 					outStructSize );
	
	virtual IOReturn SetWriteParametersModePage ( AppleWriteParametersModePageStruct * paramsData,
												  SCSITaskStatus * 	taskStatus,
												  UInt32 			inStructSize,
												  UInt32 * 			outStructSize );
	
	virtual IOReturn GetTrayState 		( UInt32 * trayState );

	virtual IOReturn SetTrayState 		( UInt32 trayState );
	
	virtual IOReturn ReadTableOfContents ( AppleReadTableOfContentsStruct * 	readTOCData,
										   SCSITaskStatus * 					taskStatus,
										   UInt32 								inStructSize,
										   UInt32 * 							outStructSize );
										   
	virtual IOReturn ReadDiscInformation ( AppleReadDiscInfoStruct * 	discInfoData,
										   SCSITaskStatus * 			taskStatus,
										   UInt32 						inStructSize,
										   UInt32 * 					outStructSize );
	
	virtual IOReturn ReadTrackInformation ( AppleReadTrackInfoStruct * 	trackInfoData,
										    SCSITaskStatus * 			taskStatus,
										    UInt32 						inStructSize,
										    UInt32 * 					outStructSize );
	
	virtual IOReturn ReadDVDStructure 	( AppleReadDVDStructureStruct * dvdStructData,
									   	  SCSITaskStatus * 				taskStatus,
									   	  UInt32 						inStructSize,
									   	  UInt32 * 						outStructSize );
	
	virtual IOReturn SetCDSpeed 		( AppleSetCDSpeedStruct *		setCDSpeedData,
										  SCSITaskStatus * 				taskStatus,
									   	  UInt32 						inStructSize,
									   	  UInt32 * 						outStructSize );
	
	virtual IOReturn ReadFormatCapacities ( AppleReadFormatCapacitiesStruct *	readFormatCapacitiesData,
											SCSITaskStatus * 					taskStatus,
											UInt32 								inStructSize,
											UInt32 * 							outStructSize );
	
	bool didTerminate ( IOService * provider, IOOptionBits options, bool * defer );
	
protected:
	
	static IOExternalMethod				sMethods[kSCSITaskUserClientMethodCount];
	static IOExternalAsyncMethod		sAsyncMethods[kSCSITaskUserClientAsyncMethodCount];

	static IOReturn	sCreateTask 		( void * self, SCSITask * task, SInt32 * taskReference );
	static IOReturn	sReleaseTask 		( void * self, SInt32 taskReference, void * task );
	static IOReturn	sWaitForTask 		( void * userClient, SCSITask * request );
	static IOReturn	sValidateTask 		( void * userClient, SCSITask * request, SCSITaskData * args, UInt32 argSize );
	static void 	sTaskCallback		( SCSITaskIdentifier completedTask );
	
	virtual IOReturn GatedCreateTask 	( SCSITask * task, SInt32 * taskReference );
	virtual IOReturn GatedReleaseTask 	( SInt32 taskReference, SCSITask ** task );
	virtual IOReturn GatedWaitForTask 	( SCSITask * request );
	virtual IOReturn GatedValidateTask 	( SCSITask * request, SCSITaskData * args, UInt32 argSize );
	virtual void	 TaskCallback		( SCSITask * task, SCSITaskRefCon * refCon );
	
	task_t								fTask;
	IOService *							fProvider;
	IOSCSIProtocolInterface *			fProtocolInterface;
	SCSITask *							fArray[kMaxSCSITaskArraySize];
	IOCommandGate *						fCommandGate;
	IOWorkLoop *						fWorkLoop;
	UInt32								fOutstandingCommands;
	
	virtual IOExternalAsyncMethod *		getAsyncTargetAndMethodForIndex ( IOService ** target, UInt32 index );	
	virtual IOExternalMethod *			getTargetAndMethodForIndex 		( IOService ** target, UInt32 index );
	
	virtual IOReturn 	HandleTerminate	( IOService * provider );
	virtual IOReturn	SendCommand 	( SCSITask * request, void * senseBuffer, SCSITaskStatus * taskStatus );
	
	virtual IOReturn	SetupTask		( SCSITask ** task );
	virtual IOReturn	PrepareBuffers	( IOMemoryDescriptor ** buffer, void * userBuffer, IOByteCount bufferSize, IODirection direction );
	virtual IOReturn	CompleteBuffers ( IOMemoryDescriptor * buffer );
	
};


#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif /* ! _IOKIT_SCSI_TASK_USER_CLIENT_H_ */