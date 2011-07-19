/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
	File:		IsochronousDataHandler.i

	Contains:	Component Manager based Isochronous Data Handler
					
	Copyright:	© 1997-2001 by Apple Computer, Inc., all rights reserved.

		$Log: IsochronousDataHandler.i,v $
		Revision 1.9  2010/08/05 20:59:16  calderon
		<rdar://problem/6822618> IOFWDVComponents should add APSL headers to each source file

		Revision 1.8  2002/12/20 22:33:25  wgulland
		Fix radars 3118059 and 3071011
		
		Revision 1.7  2002/10/15 19:05:39  ayanowit
		Merged in changes to support DVCPro50.
		
		Revision 1.6  2002/03/21 01:55:44  wgulland
		Use IOFireWireFamily isoc user client instead of DV kext
		
		Revision 1.5  2001/10/05 16:46:32  wgulland
		Add inputFormat to IDHDeviceStatus structure
		
		Revision 1.4  2001/09/27 00:43:29  wgulland
		Keep retrying if starting write fails
		
		Revision 1.22  2001/04/26 21:20:26  kledzik
		Switch to cvs style header comment
		

	Old History:

		(ngk)	Nick Kledzik
		(WG)	William Gulland
		(dav)	Dave Chilson
		(KW)	Kevin Williams
		(jkl)	Jay Lloyd
		(GDW)	George D. Wilson Jr.
		(RS)	Richard Sepulveda
		(SW)	Sean Williams

		<21>	 4/17/01	WG		Remove incorrect exportset=IDHLib_10 for IDHGetDeviceTime
		<20>	 4/17/01	ngk		Change uses of IDHNotificationProc to IDHNotificationUPP. Update
									some usages of old syntax.
		<19>	 4/17/01	WG		[2567324]  Added <nativeUPP,etc> for IDHNotificationProc Added
									new X function IDHGetDeviceTime
		<18>	12/20/00	dav		This is part of Quicktime, so the proper version set is
									CarbonMultimedia_13
		<17>	12/14/00	dav		[2567275]  Add CarbonMultimedia_13 export set
		<16>	 12/7/99	RS		Added error code 'kIDHErrCallNotSupported' since all isoch calls
									are not support in all implementations.
		<15>	 12/7/99	jkl		Changed isocMediaType to isochMediaType. Added a default
									configuration atom that can be set as a hint for a configuration
									to use for clients that do not have an interface to allow users
									to select a device. Added defines to support the default
									configuration.
		<14>	10/29/99	jkl		Added useCMP atom type.
		<13>	 8/16/99	RS		Added kIDHIsochVersionAtomType to atom list.
		<12>	  8/9/99	RS		Added IDHUpdateDeviceList() to Isoch API.
		<11>	  8/6/99	jkl		Added kIDHInvalidDeviceID define.
		<10>	  8/5/99	RS		Added kIDHErrDeviceBusy error to list.
		 <9>	 7/15/99	RS		Changed error number assignments to avoid conflict with existing
									DVFamily.h
		 <8>	 6/28/99	RS		Moved IDHGetClientID() prototype to IsochPriv.i since it is a
									priv function. Added deviceID atom. Added deviceTimeout error
									code. Added IDHResolution and IDHDimension structures.
		 <7>	 6/24/99	KW		Added IDHDeviceID as parameter to IDHNewNotification. Added some
									reserved events.
		 <6>	 6/23/99	jkl		Added error codes in DV range. Added IDHGetDeviceID routine.
		 <5>	 6/20/99	RS		Made the notification flags match the DVFamily lib flags for
									consistency.
		 <4>	 6/18/99	RS		Added new atom type kIDHUniqueID to device atom list. Changed
									type of status structure element from PsuedoFWID to PsuedoID to
									remain bus independent.
		 <3>	 6/16/99	GDW		Added get device component call.
		 <2>	 6/14/99	RS		Added 'result' parameter to IDHParameterBlock structure.
		 <1>	 6/11/99	SW		first checked in
*/

%if FRAMEWORKS	
	#include <CoreServices.i>
%else
    #include <MacTypes.i>
%endif
#include <MoviesFormat.i>
#include <QuickTimeComponents.i>


 
 
enum <rez>
{
	kIDHComponentType				= 'ihlr',		// Component type
	kIDHSubtypeDV					= 'dv  ',		// Subtype for DV (over FireWire)
	kIDHSubtypeFireWireConference	= 'fwc ',		// Subtype for FW Conference
};


//
// Version of Isochronous Data Handler API
//
enum <rez>
{
	kIDHInterfaceVersion1			= 0x0001,		// Initial relase (Summer '99)
};


// atom types

enum {
	kIDHDeviceListAtomType		 		= 'dlst',
	kIDHDeviceAtomType					= 'devc',	// to be defined elsewhere
	kIDHIsochServiceAtomType			= 'isoc',
	kIDHIsochModeAtomType				= 'mode',

	kIDHDeviceIDType					= 'dvid',
	kIDHDefaultIOType					= 'dfio',
	kIDHIsochVersionAtomType			= 'iver',
	kIDHUniqueIDType					= 'unid',
	kIDHNameAtomType					= 'name',
	kIDHUseCMPAtomType					= 'ucmp',
	kIDHIsochMediaType					= 'av  ',
	
	kIDHDataTypeAtomType				= 'dtyp',
	kIDHDataSizeAtomType				= 'dsiz',	// ??? packet size vs. buffer size
	kIDHDataBufferSizeAtomType			= 'dbuf',	// ??? packet size vs. buffer size
	kIDHDataIntervalAtomType			= 'intv',
	kIDHDataIODirectionAtomType			= 'ddir',

	kIDHSoundMediaAtomType				= 'soun',
	kIDHSoundTypeAtomType				= 'type',
	kIDHSoundChannelCountAtomType		= 'ccnt',
	kIDHSoundSampleSizeAtomType			= 'ssiz',
	kIDHSoundSampleRateAtomType			= 'srat',
	
	// same as video out... (what does this comment mean?)
	kIDHVideoMediaAtomType				= 'vide',
	kIDHVideoDimensionsAtomType			= 'dimn',
	kIDHVideoResolutionAtomType			= 'resl',
	kIDHVideoRefreshRateAtomType		= 'refr',
	kIDHVideoPixelTypeAtomType			= 'pixl',

	kIDHVideoDecompressorAtomType		= 'deco',
	kIDHVideoDecompressorTypeAtomType	= 'dety',
	kIDHVideoDecompressorContinuousAtomType	= 'cont',
	kIDHVideoDecompressorComponentAtomType	= 'cmpt'

};

//
// I/O Flags 
//
enum
{
	kIDHDataTypeIsInput					= 1L << 0,
	kIDHDataTypeIsOutput				= 1L << 1,
	kIDHDataTypeIsInputAndOutput		= 1L << 2,
};


//
// Permission Flags 
//
enum
{
	kIDHOpenForReadTransactions			= 1L << 0,
	kIDHOpenForWriteTransactions		= 1L << 1,
	kIDHOpenWithExclusiveAccess			= 1L << 2,
	kIDHOpenWithHeldBuffers				= 1L << 3, // IDH will hold buffer until ReleaseBuffer()
	kIDHCloseForReadTransactions		= 1L << 4,
	kIDHCloseForWriteTransactions		= 1L << 5,
};


//
// Errors 
//	These REALLY need to be moved into Errors.h
// ¥¥¥Êneeds officially assigned numbers
enum
{
	kIDHErrDeviceDisconnected	= -14101,
	kIDHErrInvalidDeviceID		= -14102,
	kIDHErrDeviceInUse			= -14104,
	kIDHErrDeviceNotOpened		= -14105,
	kIDHErrDeviceBusy			= -14106,
	kIDHErrDeviceReadError		= -14107,
	kIDHErrDeviceWriteError		= -14108,
	kIDHErrDeviceNotConfigured	= -14109,
	kIDHErrDeviceList			= -14110,
	kIDHErrCompletionPending	= -14111,
	kIDHErrDeviceTimeout		= -14112,
	kIDHErrInvalidIndex			= -14113,
	kIDHErrDeviceCantRead		= -14114,
	kIDHErrDeviceCantWrite		= -14115,
	kIDHErrCallNotSupported		= -14116
};




//
// Holds Device Identification...
//
typedef UInt32 IDHDeviceID;			
enum
{
	kIDHInvalidDeviceID			= 0,
	kIDHDeviceIDEveryDevice		= 0xFFFFFFFF
};

//
// Values for 5 bit STYPE part of CIP header
enum
{
    kIDHDV_SD					= 0,
    kIDHDV_SDL					= 1,
    kIDHDV_HD					= 2,
    kIDHDVCPro_25				= 0x1e,
    kIDHDVCPro_50				= 0x1d

};

//
//	Isoch Interval Atom Data
//
struct IDHIsochInterval
{
	SInt32			duration;
	TimeScale		scale;
};


// Need to fix this.  For now, cast this as a FWReferenceID
struct opaque PsuedoID;

//
// Isoch Device Status
//	This is atom-like, but isnÕt an atom
//
struct IDHDeviceStatus
{
	UInt32			version;
	Boolean			physicallyConnected;
	Boolean			readEnabled;
	Boolean			writeEnabled;
	Boolean			exclusiveAccess;
	UInt32			currentBandwidth;
	UInt32			currentChannel;
	PsuedoID		localNodeID;				//¥¥¥Êmay go in atoms 
	SInt16			inputStandard;			// One of the QT input standards
	Boolean			deviceActive;
    UInt8			inputFormat;			// Expected STYPE of data from device
    UInt32			outputFormats;			// Bitmask for supported STYPE values, if version > 0x200
};

//
// Isochronous Data Handler Events
//  
enum unsigned long IDHEvent
{
	kIDHEventInvalid			= 0,
	
	kIDHEventDeviceAdded		= 1L << 0,		// A new device has been added to the bus
	kIDHEventDeviceRemoved		= 1L << 1,		// A device has been removed from the bus
	kIDHEventDeviceChanged		= 1L << 2,		// Some device has changed state on the bus
	kIDHEventReadEnabled		= 1L << 3,		// A client has enabled a device for read
	kIDHEventFrameDropped       = 1L << 4, 		// software failed to keep up with isoc data flow
	kIDHEventReadDisabled	 	= 1L << 5,		// A client has disabled a device from read
	kIDHEventWriteEnabled		= 1L << 6,		// A client has enabled a device for write
	kIDHEventReserved2			= 1L << 7,		// Reserved for future use
	kIDHEventWriteDisabled		= 1L << 8,		// A client has disabled a device for write

	kIDHEventEveryEvent			= 0xFFFFFFFF,

};

typedef UInt32 IDHNotificationID;

struct IDHEventHeader {
	IDHDeviceID				deviceID;			// Device which generated event
	IDHNotificationID		notificationID;	
	IDHEvent				event;				// What the event is
};





//
// IDHGenericEvent
//	An IDH will often have to post events from at interrupt time.  Since memory
//	allocation cannot occur from the interrupt handler, the IDH can preallocate
//	storage needed for handling the event by creating some IDHGenericEvent items.
//	Subsequently, when an event is generated, the type of event (specified in the
//	IDHEventHeader) will dictate how the IDHGenericEvent should be interpretted.
//	
//	IMPORTANT NOTE : This means that a specific event structure can NEVER be greater
//	than the size of the generic one.
//	
struct IDHGenericEvent
{		
	IDHEventHeader		eventHeader;
	UInt32				pad[4];
};


//
// IDHDeviceConnectionEvent
//	For kIDHEventDeviceAdded or kIDHEventDeviceRemoved events.
//
struct IDHDeviceConnectionEvent
{
	IDHEventHeader	eventHeader;
};


//
// IDHDeviceIOEnableEvent
//	For kIDHEventReadEnabled, kIDHEventReadDisabled, kIDHEventWriteEnabled, or
//	kIDHEventWriteDisabled.
//
struct IDHDeviceIOEnableEvent
{
	IDHEventHeader	eventHeader;
};

//
// IDHDeviceFrameDroppedEvent
//	For kIDHEventFrameDropped
//
struct IDHDeviceFrameDroppedEvent
{
	IDHEventHeader	eventHeader;
	UInt32			totalDropped;
	UInt32			newlyDropped;
};


typedef extern <nativeUPP, exportset=CarbonMultimedia_14, exportset=fw_DVComponentGlue_X>
OSStatus (*IDHNotificationProcPtr)(IDHGenericEvent* event, void* userData);
typedef IDHNotificationProcPtr IDHNotificationProc;	// old name
#pragma UPPSuite emitUPPTypes

struct IDHParameterBlock {
	UInt32					reserved1;
	UInt16					reserved2;
	void*					buffer;
	ByteCount				requestedCount;
	ByteCount				actualCount;
	IDHNotificationUPP		completionProc;
	void*					refCon;
	OSErr					result;
};

struct IDHResolution {
	UInt32	x;
	UInt32	y;
};

struct IDHDimension {
	Fixed 	x;
	Fixed 	y;
};


%TellEmitter "components" "prefix IDH";


pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHGetDeviceList(ComponentInstance idh, QTAtomContainer* deviceList) = ComponentCall(1);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHGetDeviceConfiguration(ComponentInstance idh, QTAtomSpec* configurationID) = ComponentCall(2);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHSetDeviceConfiguration(ComponentInstance idh, const QTAtomSpec* configurationID) = ComponentCall(3);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHGetDeviceStatus(ComponentInstance idh, const QTAtomSpec* configurationID, IDHDeviceStatus* status) = ComponentCall(4);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHGetDeviceClock(ComponentInstance idh, Component* clock) = ComponentCall(5);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHOpenDevice(ComponentInstance idh, UInt32 permissions) = ComponentCall(6);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHCloseDevice(ComponentInstance idh) = ComponentCall(7);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHRead(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(8);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHWrite(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(9);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHNewNotification(ComponentInstance idh, IDHDeviceID deviceID, IDHNotificationUPP notificationProc, void* userData, IDHNotificationID* notificationID) = ComponentCall(10);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHNotifyMeWhen(ComponentInstance idh, IDHNotificationID	notificationID, IDHEvent events) = ComponentCall(11);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHCancelNotification(ComponentInstance idh, IDHNotificationID notificationID)  = ComponentCall(12);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHDisposeNotification(ComponentInstance idh, IDHNotificationID notificationID) = ComponentCall(13);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHReleaseBuffer(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(14);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHCancelPendingIO(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(15);

pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHGetDeviceControl(ComponentInstance idh, ComponentInstance *deviceControl) = ComponentCall(16);										
pascal <exportset=IDHLib_10, exportset=CarbonMultimedia_13, exportset=fw_DVComponentGlue_X>
ComponentResult IDHUpdateDeviceList(ComponentInstance idh, QTAtomContainer* deviceList) = ComponentCall(17);

pascal <exportset=CarbonMultimedia_14, exportset=fw_DVComponentGlue_X>
ComponentResult IDHGetDeviceTime(ComponentInstance idh, TimeRecord* deviceTime) = ComponentCall(18);

pascal <exportset=CarbonMultimedia_15, exportset=fw_DVComponentGlue_X>
ComponentResult IDHSetFormat(ComponentInstance idh, UInt32 format) = ComponentCall(19);

pascal <exportset=CarbonMultimedia_15, exportset=fw_DVComponentGlue_X>
ComponentResult IDHGetFormat(ComponentInstance idh, UInt32 *format) = ComponentCall(20);


#pragma UPPSuite emitAll

%TellEmitter "components" "emitProcInfos";
%TellEmitter "c" "emitComponentSelectors";


