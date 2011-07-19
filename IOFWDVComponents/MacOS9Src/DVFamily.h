/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
/*
	File:		DVFamily.h

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Written by:	Steve Smith

	Copyright:	© 1996-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(jkl)	Jay Lloyd
		(KW)	Kevin Williams
		(ck)	Casey King
		(RS)	Richard Sepulveda
		(CP)	Collin Pieper
		(CLP)	Collin Pieper
		(AW)	Adrienne Wang
		(SS)	Steve Smith

	Change History (most recent first):

		<24>	 6/17/99	jkl		Added fwClientID to DVGetLocalFWReferenceID call to make the
									fwClientID part of the device info so IDH can execute
									getDeviceStandard by itself.
		<23>	  6/8/99	KW		Added an another event for input. Will go away when DVFamilyLib
									is dead of course.
		<22>	 1/15/99	ck		Firewire.h should be FireWire.h
		<21>	  1/4/99	GDW		Changed DVFamily names.
		<20>	 12/9/98	SS		Added FireWire ID for the local node in the DVDeviceInfo struct.
									Changed DVGetDeviceClock to return a component instead of a
									component instance.
		<19>	11/16/98	SS		Rearranged some data structures to work better in mixed
									environment (Premiere plugins use 68K alignment). Added
									deviceIsOnline flag in deviceInfo for Kevin.
		<18>	11/13/98	SS		Added DVGetDeviceClock() proto.
		<17>	 9/24/98	RS		Added bufferSize to DVIsochCompleteEventStruct structure.
		<16>	 9/17/98	SS		Added generic dv event notification support. Removed AppleEvent
									support. Minor DV API semantic changes.
		<15>	 9/10/98	SS		Somewhat gratuitous semantic change in the DV API to further
									abstract the implementation, specifically, from DVDeviceRefNum
									to DVDeviceConnectionID, and from DVOpen/CloseDriver to
									DVOpen/CloseDeviceConnection.
		<14>	  9/3/98	SS		Checked in first pass of dv device info stuff.
		<13>	 3/23/98	RS		Changed to registered error codes
		<12>	 3/12/98	SS		Changed semantics of DV driver API to use deviceIDs and refNums
									instead of driverIDs everywhere.
		<11>	  3/8/98	RS		Added new call DVIsEnabled() to query if a device has already
									been enabled.
		<10>	 2/26/98	CP		Added new error code for driver busy errors
		 <9>	 2/24/98	CP		Added two new error codes returned only if your mean to the
									driver...
		 <8>	  2/4/98	CP		Added new defines for device standards
		 <7>	 1/21/98	SS		Removed AVC enums, typedefs, structs and prototypes. Base
									support is now for the DVDoAVCTransaction() call only.
		 <6>	 1/19/98	CP		Tossed out obsolete constants and structures and moved ones that
									should be private to safety...
		 <5>	 1/19/98	AW		Added prototype for determining whether camera is still
									searching for timecode
		 <4>	 1/13/98	SS		Added DriverID parameter to AVC helper routines. These routines
									should probably be moved to a separate file.
		 <3>	 1/12/98	GDW		Fixed KernelID prob.
		 <2>	 1/12/98	SS		Checked in Collin's changes: added new API calls, reorganized
									layout.
		 <1>	 8/18/97	CLP		first checked in
		 <1>	 3/27/97	SS		first checked in
		 <6>	 3/27/97	SS		Added macro & function prototype to check whether the Family Lib
									is fully initialized, i.e. determine if there are any FW devices
									to control.
		 <5>	  3/3/97	AW		Added AVC Commands for getting/setting signals
		 <4>	 2/19/97	SS		Updated to 1.0a2 FSL
		 <3>	 2/12/97	AW		Updated to 1.0d18 FSL
		 <2>	10/31/96	SS		Updated to Blaze d16 API.  Added helper APIs for device control.

*/

//
//	DVFamily.h
//

#ifndef __DVFAMILY__
#define __DVFAMILY__

#include <Types.h>
#include <NameRegistry.h>
#include <DriverServices.h>
#include <AppleEvents.h>
#include <Components.h>

#include "FireWire.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////
//
// constants
//
///////////////////////////////////////////////////////////////////////

enum 
{
	kInvalidDVDeviceID			= 0,
	kInvalidDVConnectionID		= 0
};

enum
{
	kDVDisconnectedErr			= -14101,
	kDVBadIDErr					= -14102,
	kUnknownStandardErr			= -14103,
	kAlreadyEnabledErr			= -14104,
	kNotEnabledErr				= -14105,
	kDVDeviceBusyErr			= -14106
};

enum
{
	kUnknownStandard	= 0,
	kNTSCStandard		= 1,
	kPALStandard 		= 2
};

// DV events
enum
{
	kInvalidDVDeviceEvent		= 0,
	
	kDVDeviceAdded				= 1 << 0,
	kDVDeviceRemoved			= 1 << 1,
	KDVDeviceInfoChanged		= 1 << 2,
	
	kDVIsochReadEnabled			= 1 << 3,
	kDVIsochReadComplete		= 1 << 4,
	kDVIsochReadDisabled	 	= 1 << 5,

	kDVIsochWriteEnabled		= 1 << 6,
	kDVIsochWriteComplete		= 1 << 7,
	kDVIsochWriteDisabled		= 1 << 8,

	kDVAVCEnabled				= 1 << 9,
	kDVAVCDisabled				= 1 << 10,
	kDVAVCTransactionComplete	= 1 << 11,

	// Tempory new event for input. Goes away DVFamily is dead.
	kDVInputEvent = 1 << 12,

	kDVEveryEvent				= 0x00001fff
};

enum
{
	kDVGlobalEventConnectionID	= 0xffffffff,
	kEventSpecificDataSize		= 16
};

///////////////////////////////////////////////////////////////////////
//
// types
//
///////////////////////////////////////////////////////////////////////

// holds our device identification...
typedef UInt32 DVCDeviceID;			

// holds our device connection identification...
typedef UInt32 DVCDeviceConnectionID;

// holds info about device's isoch channels
typedef struct DVCIsochChannelStatus {
	UInt32							fwChannelNum;
	UInt32							speed;
	UInt32							signalType;
	Boolean							isEnabled;
	UInt8							rsvd[3];		// alignment padding for 68k
} DVCIsochChannelStatus, *DVCIsochChannelStatusPtr;

// holds info about device
typedef struct DVCDeviceInfo {
	DVCDeviceID					dvDeviceID;
	CSRNodeUniqueID				uniqueID;
	UInt32						vendorID;
	RegEntryID					regEntryID;
	Boolean						deviceIsOnline;
	Boolean						AVCisEnabled;
	UInt16						rsvd;				// alignment padding for 68k
	DVCIsochChannelStatus		readChannel;
	DVCIsochChannelStatus		writeChannel;
	Str255						deviceName;
	FWReferenceID				localNodeID;		// FW reference ID for local node
													// (i.e. FW interface)
	FWReferenceID				fwClientID;
	
} DVCDeviceInfo, *DVCDeviceInfoPtr;

// for sending AVC commands
typedef struct AVCTransactionParamsStruct {
	Ptr						commandBufferPtr;
	UInt32					commandLength;
	Ptr						responseBufferPtr;
	UInt32					responseBufferSize;
	FCPResponseHandlerPtr	responseHandler;
} AVCTransactionParams, *AVCTransactionParamsPtr;

///////////////////////////////////////////////////////////////////////
//
// DV Event Notification
//

typedef struct OpaqueRef		*DVCNotificationID;

typedef struct DVCEventHeaderStruct {
	DVCDeviceID			deviceID;			// who it's from
	DVCNotificationID	notifID;
	UInt32				theEvent;			// what the event was
} DVCEventHeader, *DVCEventHeaderPtr;

typedef struct DVCEventRecordStruct {		// generalized form
	DVCEventHeader		eventHeader;
	UInt8				eventData[kEventSpecificDataSize];
} DVCEventRecord, *DVCEventRecordPtr;

typedef struct DVCConnectionEventStruct {
	DVCEventHeader		eventHeader;
} DVCConnectionEvent, *DVCConnectionEventPtr;

typedef struct DVCIsochCompleteEventStruct {
	DVCEventHeader		eventHeader;
	Ptr					pFrameBuffer;
	unsigned long		bufferSize;
	UInt32				fwCycleTime;
} DVCIsochCompleteEvent, *DVCIsochCompleteEventPtr;

typedef struct DVCAVCTransactionCompleteEventStruct {
	DVCEventHeader			eventHeader;
	Ptr						commandBufferPtr;
	UInt32					commandLength;
	Ptr						responseBufferPtr;
	UInt32					responseBufferSize;
} DVCAVCTransactionCompleteEvent, *DVAVCTransactionCompleteEventPtr;

// DV notification proc
typedef OSStatus (*DVCNotifyProc)(	DVCEventRecordPtr	event,
									void				*userData );
								
///////////////////////////////////////////////////////////////////////
//
// external prototypes
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
//
// general device management
//
UInt32 DVCCountDevices( void );
OSErr DVCGetIndDevice( DVCDeviceID * pDVDevice, UInt32 index );
OSErr DVCGetDeviceInfo( DVCDeviceID deviceID, DVCDeviceInfoPtr pInfo );

OSErr DVCSetDeviceName( DVCDeviceID deviceID, char * str );
OSErr DVCGetDeviceName( DVCDeviceID deviceID, char * str );

OSErr DVCOpenDeviceConnection( DVCDeviceID deviceID, DVCDeviceConnectionID *pConnID );
OSErr DVCCloseDeviceConnection( DVCDeviceConnectionID connID );

OSErr DVCGetDeviceClock( DVCDeviceID deviceID, Component *clock );

///////////////////////////////////////////////////////////////////////
//
// DV event notification
//
OSErr DVCNewNotification( DVCDeviceConnectionID connID, DVCNotifyProc notifyProc,
						void *userData, DVCNotificationID *pNotifyID );
	
OSErr DVCNotifyMeWhen( DVCDeviceConnectionID connID, DVCNotificationID notifyID, UInt32 events);
OSErr DVCCancelNotification( DVCDeviceConnectionID connID, DVCNotificationID notifyID );
OSErr DVCDisposeNotification( DVCDeviceConnectionID connID, DVCNotificationID notifyID );

///////////////////////////////////////////////////////////////////////
//
// DV Isoch Read
//
OSErr DVCEnableRead( DVCDeviceConnectionID connID );
OSErr DVCDisableRead( DVCDeviceConnectionID connID );
OSErr DVCReadFrame( DVCDeviceConnectionID connID, Ptr *ppReadBuffer, UInt32 * pSize );
OSErr DVCReleaseFrame( DVCDeviceConnectionID connID, Ptr pReadBuffer );

///////////////////////////////////////////////////////////////////////
//
// DV Isoch Write
//
OSErr DVCEnableWrite( DVCDeviceConnectionID connID );
OSErr DVCDisableWrite( DVCDeviceConnectionID connID );
OSErr DVCGetEmptyFrame( DVCDeviceConnectionID connID, Ptr *ppEmptyFrameBuffer, UInt32 * pSize );
OSErr DVCWriteFrame( DVCDeviceConnectionID connID, Ptr pWriteBuffer );

///////////////////////////////////////////////////////////////////////
//
// AVC transactions
//
OSErr DVCEnableAVCTransactions( DVCDeviceConnectionID connID );
OSErr DVCDoAVCTransaction( DVCDeviceConnectionID connID, AVCTransactionParamsPtr pParams );
OSErr DVCDisableAVCTransactions( DVCDeviceConnectionID connID );

///////////////////////////////////////////////////////////////////////
//
// to be discontinued...
//
OSErr DVCIsEnabled( DVCDeviceConnectionID connID, Boolean *isEnabled);
OSErr DVCGetDeviceStandard( DVCDeviceConnectionID connID, UInt32 * pStandard );




#ifdef __cplusplus
}
#endif

#endif __DVFAMILY__


