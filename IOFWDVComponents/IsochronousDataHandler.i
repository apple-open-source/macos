/*
	File:		IsochronousDataHandler.i

	Contains:	The defines the client API to an Isochronous Data Handler, which is
				a Component Manager based item.
				
				
	Version:	xxx put version here xxx

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Sean Williams

		Other Contact:		Richard Sepulveda

		Technology:			Isochronous Data Handlers

	Writers:

		(KW)	Kevin Williams
		(jkl)	Jay Lloyd
		(GDW)	George D. Wilson Jr.
		(RS)	Richard Sepulveda
		(SW)	Sean Williams

	Change History (most recent first):

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


%CPassThru "//";
%CPassThru "// Check for Prior Inclusion of IsochronousDataHandler.r";
%CPassThru "//	If this header is trying to be included via a Rez path, make it act";
%CPassThru "//	as a NOP.  This will allow both Rez & C files to get to use the";
%CPassThru "//	contants for the component type, subtype, and interface version.";
%CPassThru "#ifndef __ISOCHRONOUSDATAHANDLER_R__";


%RezPassThru "//";
%RezPassThru "// Check for Prior Inclusion of IsochronousDataHandler.h";
%RezPassThru "//	If this header is trying to be included via a C path, make it act";
%RezPassThru "//	as a NOP.  This will allow both Rez & C files to get to use the";
%RezPassThru "//	contants for the component type, subtype, and interface version.";
%RezPassThru "#ifndef __ISOCHRONOUSDATAHANDLER__";


#include <MacTypes.i>
#include <Dialogs.i>
#include <MoviesFormat.i>
#include <QuickTimeComponents.i>


 
 
%TellEmitter "rez" "useNextEnum";
enum
{
	kIDHComponentType				= 'ihlr',		// Component type
	kIDHSubtypeDV					= 'dv  ',		// Subtype for DV (over FireWire)
	kIDHSubtypeFireWireConference	= 'fwc ',		// Subtype for FW Conference
};


//
// Version of Isochronous Data Handler API
//
%TellEmitter "rez" "useNextEnum";
enum
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
};

//
// Isochronous Data Handler Events
//  

typedef UInt32 IDHEvent;
enum
{
	kIDHEventInvalid			= 0,
	
	kIDHEventDeviceAdded		= 1L << 0,		// A new device has been added to the bus
	kIDHEventDeviceRemoved		= 1L << 1,		// A device has been removed from the bus
	kIDHEventDeviceChanged		= 1L << 2,		// Some device has changed state on the bus
	kIDHEventReadEnabled		= 1L << 3,		// A client has enabled a device for read
	kIDHEventReserved1			= 1L << 4,		// Reserved for future use
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
} ;


//
// IDHDeviceIOEnableEvent
//	For kIDHEventReadEnabled, kIDHEventReadDisabled, kIDHEventWriteEnabled, or
//	kIDHEventWriteDisabled.
//
struct IDHDeviceIOEnableEvent
{
	IDHEventHeader	eventHeader;
};


typedef extern OSStatus (*IDHNotificationProc)(IDHGenericEvent*	event, void* userData);

struct IDHParameterBlock {
	UInt32					reserved1;
	UInt16					reserved2;
	void*					buffer;
	ByteCount				requestedCount;
	ByteCount				actualCount;
	IDHNotificationProc		completionProc;
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


pascal <exportset=IDHLib_10>
ComponentResult IDHGetDeviceList(ComponentInstance idh, QTAtomContainer* deviceList) = ComponentCall(1);

pascal <exportset=IDHLib_10>
ComponentResult IDHGetDeviceConfiguration(ComponentInstance idh, QTAtomSpec* configurationID) = ComponentCall(2);

pascal <exportset=IDHLib_10>
ComponentResult IDHSetDeviceConfiguration(ComponentInstance idh, const QTAtomSpec* configurationID) = ComponentCall(3);

pascal <exportset=IDHLib_10>
ComponentResult IDHGetDeviceStatus(ComponentInstance idh, const QTAtomSpec* configurationID, IDHDeviceStatus* status) = ComponentCall(4);

pascal <exportset=IDHLib_10>
ComponentResult IDHGetDeviceClock(ComponentInstance idh, Component* clock) = ComponentCall(5);

pascal <exportset=IDHLib_10>
ComponentResult IDHOpenDevice(ComponentInstance idh, UInt32 permissions) = ComponentCall(6);

pascal <exportset=IDHLib_10>
ComponentResult IDHCloseDevice(ComponentInstance idh) = ComponentCall(7);

pascal <exportset=IDHLib_10>
ComponentResult IDHRead(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(8);

pascal <exportset=IDHLib_10>
ComponentResult IDHWrite(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(9);

pascal <exportset=IDHLib_10>
ComponentResult IDHNewNotification(ComponentInstance idh, IDHDeviceID deviceID, IDHNotificationProc notificationProc, void* userData, IDHNotificationID* notificationID) = ComponentCall(10);

pascal <exportset=IDHLib_10>
ComponentResult IDHNotifyMeWhen(ComponentInstance idh, IDHNotificationID	notificationID, IDHEvent events) = ComponentCall(11);

pascal <exportset=IDHLib_10>
ComponentResult IDHCancelNotification(ComponentInstance idh, IDHNotificationID notificationID)  = ComponentCall(12);

pascal <exportset=IDHLib_10>
ComponentResult IDHDisposeNotification(ComponentInstance idh, IDHNotificationID notificationID) = ComponentCall(13);

pascal <exportset=IDHLib_10>
ComponentResult IDHReleaseBuffer(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(14);

pascal <exportset=IDHLib_10>
ComponentResult IDHCancelPendingIO(ComponentInstance idh, IDHParameterBlock* pb) = ComponentCall(15);

pascal <exportset=IDHLib_10>
ComponentResult IDHGetDeviceControl(ComponentInstance idh, ComponentInstance *deviceControl) = ComponentCall(16);										

pascal <exportset=IDHLib_10>
ComponentResult IDHUpdateDeviceList(ComponentInstance idh, QTAtomContainer* deviceList) = ComponentCall(17);

pascal <exportset=IDHLib_10>
ComponentResult IDHGetDeviceTime(ComponentInstance idh, TimeRecord* deviceTime) = ComponentCall(18);

%TellEmitter "components" "emitProcInfos";
%TellEmitter "c" "emitComponentSelectors";



%RezPassThru "#endif /* ifndef __ISOCHRONOUSDATAHANDLER__ */";
%CPassThru "#endif /* ifndef __ISOCHRONOUSDATAHANDLER_R__ */";
