/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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

#ifndef __MMC_DEVICE_USER_CLIENT_CLASS_H__
#define __MMC_DEVICE_USER_CLIENT_CLASS_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// IOKit includes
#include <IOKit/IOCFPlugIn.h>

// Private includes
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "SCSITaskIUnknown.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


class MMCDeviceUserClientClass : public SCSITaskIUnknown
{
	
	public:
	
		MMCDeviceUserClientClass ( void );
		virtual ~MMCDeviceUserClientClass ( void );
		
		static IOCFPlugInInterface ** alloc ( void );
		
	protected:
		
		static IOCFPlugInInterface				sIOCFPlugInInterface;
		static MMCDeviceInterface				sMMCDeviceInterface;
		struct InterfaceMap						fMMCDeviceInterfaceMap;
		
		io_service_t 	fService;
		io_connect_t 	fConnection;
		
		// utility function to get "this" pointer from interface
		static inline MMCDeviceUserClientClass * getThis ( void * self )
			{ return ( MMCDeviceUserClientClass * ) ( ( InterfaceMap * ) self )->obj; };
		
		
		// CFPlugIn/IOCFPlugIn stuff
		virtual HRESULT 	QueryInterface ( REFIID iid, void ** ppv );
		
		virtual IOReturn 	Probe (	CFDictionaryRef propertyTable,
									io_service_t	service,
									SInt32 *		order );
		
		virtual IOReturn 	Start (	CFDictionaryRef		propertyTable,
									io_service_t		service );
		
		virtual IOReturn 	Stop ( void );
		
		
		// MMC stuff
		virtual IOReturn 	Inquiry ( 	SCSICmd_INQUIRY_StandardData * 	inquiryBuffer,
										SCSICmdField1Byte				bufferSize,
										SCSITaskStatus *				taskStatus,
										SCSI_Sense_Data *				senseDataBuffer );
		
		virtual IOReturn 	TestUnitReady (		SCSITaskStatus *	taskStatus,
												SCSI_Sense_Data *	senseDataBuffer );
		
		virtual IOReturn 	GetPerformance ( 	SCSICmdField5Bit 	DATA_TYPE,
												SCSICmdField4Byte	STARTING_LBA,
												SCSICmdField2Byte	MAXIMUM_NUMBER_OF_DESCRIPTORS,
												SCSICmdField1Byte	TYPE,
												void *				buffer,
												SCSICmdField2Byte	bufferSize,
												SCSITaskStatus *	taskStatus,
												SCSI_Sense_Data *	senseDataBuffer );
		
		virtual IOReturn 	GetConfiguration (	SCSICmdField1Byte	RT,
												SCSICmdField2Byte	STARTING_FEATURE_NUMBER,
												void *				buffer,
												SCSICmdField2Byte	bufferSize,
												SCSITaskStatus *	taskStatus,
												SCSI_Sense_Data *	senseDataBuffer );
		
		virtual IOReturn 	ModeSense10 ( 	SCSICmdField1Bit	LLBAA,
											SCSICmdField1Bit	DBD,
											SCSICmdField2Bit	PC,
											SCSICmdField6Bit	PAGE_CODE,
											void *				buffer,
											SCSICmdField2Byte	bufferSize,
											SCSITaskStatus * 	taskStatus,
											SCSI_Sense_Data * 	senseDataBuffer );

		virtual IOReturn 	SetWriteParametersModePage ( 	void *				buffer,
															SCSICmdField2Byte	bufferSize,
															SCSITaskStatus * 	taskStatus,
															SCSI_Sense_Data *	senseDataBuffer );

		
		virtual IOReturn	GetTrayState ( UInt8 * trayState );
		
		virtual IOReturn	SetTrayState ( UInt8 trayState );
		
		virtual IOReturn	ReadTableOfContents ( 	SCSICmdField1Bit 	MSF,
													SCSICmdField4Bit 	FORMAT,
													SCSICmdField1Byte	TRACK_SESSION_NUMBER,
													void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );
		
		virtual IOReturn	ReadDiscInformation ( 	void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );
		
		virtual IOReturn	ReadTrackInformation ( 	SCSICmdField2Bit	ADDRESS_NUMBER_TYPE,
													SCSICmdField4Byte	LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
													void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );
												 		   
		virtual IOReturn	ReadDVDStructure ( 	SCSICmdField4Byte	ADDRESS,
												SCSICmdField1Byte	LAYER_NUMBER,
												SCSICmdField1Byte	FORMAT,
												void *				buffer,
												SCSICmdField2Byte	bufferSize,
												SCSITaskStatus *	taskStatus,
												SCSI_Sense_Data *	senseDataBuffer );
		
		virtual IOReturn 	SetCDSpeed ( SCSICmdField2Byte		LOGICAL_UNIT_READ_SPEED,
										 SCSICmdField2Byte		LOGICAL_UNIT_WRITE_SPEED,
										 SCSITaskStatus *		taskStatus,
										 SCSI_Sense_Data *		senseDataBuffer );
		
		virtual IOReturn	ReadFormatCapacities ( 	void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );
		
		virtual SCSITaskDeviceInterface ** GetSCSITaskDeviceInterface ( void );
		
		
		// Static functions (C->C++ glue code)
		
		static IOReturn 	sProbe ( 	void *			self,
										CFDictionaryRef propertyTable,
										io_service_t	service,
										SInt32 *		order );
		
		static IOReturn 	sStart ( 	void *			self,
										CFDictionaryRef propertyTable,
										io_service_t	service );

		static IOReturn 	sStop ( void * self );

		static IOReturn 	sInquiry ( 	void * 							self,
										SCSICmd_INQUIRY_StandardData * 	inquiryBuffer,
										UInt32 							inqBufferSize,
										SCSITaskStatus *				taskStatus,
										SCSI_Sense_Data *				senseDataBuffer );
		
		static IOReturn 	sTestUnitReady ( 	void * 				self,
											  	SCSITaskStatus * 	taskStatus,
											  	SCSI_Sense_Data *	senseDataBuffer );

		static IOReturn 	sGetPerformance ( 	void * 				self,
												SCSICmdField2Bit	TOLERANCE,
												SCSICmdField1Bit	WRITE,
												SCSICmdField2Bit	EXCEPT,
												SCSICmdField4Byte	STARTING_LBA,
												SCSICmdField2Byte	MAXIMUM_NUMBER_OF_DESCRIPTORS,
												void *				buffer,
												SCSICmdField2Byte	bufferSize,
												SCSITaskStatus * 	taskStatus,
												SCSI_Sense_Data *	senseDataBuffer );
		
		static IOReturn 	sGetConfiguration ( void *				self,
												SCSICmdField1Byte	RT,
												SCSICmdField2Byte	STARTING_FEATURE_NUMBER,
												void *				buffer,
												SCSICmdField2Byte	bufferSize,
												SCSITaskStatus *	taskStatus,
												SCSI_Sense_Data *	senseDataBuffer );
		
		
		static IOReturn 	sModeSense10 ( 	void *				self,
											SCSICmdField1Bit	LLBAA,
											SCSICmdField1Bit	DBD,
											SCSICmdField2Bit	PC,
											SCSICmdField6Bit	PAGE_CODE,
											void *				buffer,
											SCSICmdField2Byte	bufferSize,
											SCSITaskStatus * 	taskStatus,
											SCSI_Sense_Data * 	senseDataBuffer );
		
		static IOReturn 	sSetWriteParametersModePage ( 	void *				self,
															void *				buffer,
															SCSICmdField1Byte	bufferSize,
															SCSITaskStatus *	taskStatus,
															SCSI_Sense_Data *	senseDataBuffer );		
		
		static IOReturn 	sGetTrayState ( void * self, UInt8 * trayState );
		
		static IOReturn 	sSetTrayState ( void * self, UInt8 trayState );
		
		static IOReturn 	sReadTableOfContents ( 	void *				self,
													SCSICmdField1Bit 	MSF,
													SCSICmdField4Bit 	FORMAT,
													SCSICmdField1Byte	TRACK_SESSION_NUMBER,
													void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );
		
		static IOReturn 	sReadDiscInformation ( 	void *				self,
													void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );

		static IOReturn 	sReadTrackInformation ( void *				self,
													SCSICmdField2Bit	ADDRESS_NUMBER_TYPE,
													SCSICmdField4Byte	LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
													void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );

		static IOReturn 	sReadDVDStructure ( void *				self,
												SCSICmdField4Byte	ADDRESS,
												SCSICmdField1Byte	LAYER_NUMBER,
												SCSICmdField1Byte	FORMAT,
												void *				buffer,
												SCSICmdField2Byte	bufferSize,
												SCSITaskStatus *	taskStatus,
												SCSI_Sense_Data *	senseDataBuffer );
		
		static SCSITaskDeviceInterface ** 	sGetSCSITaskDeviceInterface ( void * self );
		
		
		static IOReturn sGetPerformanceV2 ( void * 				self,
											SCSICmdField5Bit 	DATA_TYPE,
											SCSICmdField4Byte	STARTING_LBA,
											SCSICmdField2Byte	MAXIMUM_NUMBER_OF_DESCRIPTORS,
											SCSICmdField1Byte	TYPE,
											void *				buffer,
											SCSICmdField2Byte	bufferSize,
											SCSITaskStatus *	taskStatus,
											SCSI_Sense_Data *	senseDataBuffer );
		
		static IOReturn 	sSetCDSpeed ( void *				self,
										  SCSICmdField2Byte		LOGICAL_UNIT_READ_SPEED,
										  SCSICmdField2Byte		LOGICAL_UNIT_WRITE_SPEED,
										  SCSITaskStatus *		taskStatus,
										  SCSI_Sense_Data *		senseDataBuffer );
		
		static IOReturn		sReadFormatCapacities ( void *				self,
													void *				buffer,
													SCSICmdField2Byte	bufferSize,
													SCSITaskStatus *	taskStatus,
													SCSI_Sense_Data *	senseDataBuffer );
		
	public:
		
		// IsParameterValid are used to validate that the parameter passed into
		// the command methods are of the correct value.
		
		// Validate Parameter used for 1 bit to 1 byte paramaters
		inline bool 	IsParameterValid ( 
								SCSICmdField1Byte 			param,
								SCSICmdField1Byte 			mask );
		
		// Validate Parameter used for 9 bit to 2 byte paramaters
		inline bool 	IsParameterValid ( 
								SCSICmdField2Byte 			param,
								SCSICmdField2Byte 			mask );
		
		// Validate Parameter used for 17 bit to 4 byte paramaters
		inline bool 	IsParameterValid ( 
								SCSICmdField4Byte 			param,
								SCSICmdField4Byte 			mask );
	
	private:
		
		// Disable Copying
		MMCDeviceUserClientClass ( MMCDeviceUserClientClass &src );
		void operator = ( MMCDeviceUserClientClass &src );
		
};


#endif /* __MMC_DEVICE_USER_CLIENT_CLASS_H__ */