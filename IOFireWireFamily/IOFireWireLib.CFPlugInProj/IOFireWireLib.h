/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLib.h
 *  IOFireWireLib
 *
 *  Created by NWG on Thu Apr 27 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

/*! @header IOFireWireLib.h
IOFireWireLib is the software used by user space software to communicate with FireWire
devices and control the FireWire bus. IOFireWireLib is the lowest-level FireWire interface available
in user space.

To communicate with a device on the FireWire bus, an instance of $link IOFireWireDeviceInterface (a struct
which is defined below) is created (see below). The methods of IOFireWireDeviceInterface allow you
to communicate with the device and create instances of other interfaces which provide extended 
functionality (for example, creation of unit directories on the local machine).

References to interfaces should be kept using the interface reference typedefs defined herein.
For example, you should use IOFireWireLibDeviceRef to refer to instances of IOFireWireDeviceInterface, 
IOFireWireLibCommandRef to refer to instances of IOFireWireCommandInterface, and so on.

To obtain an IOFireWireDeviceInterface for a device on the FireWire bus, use the function 
IOCreatePlugInInterfaceForService() defined in IOKit/IOCFPlugIn.h. (Note the "i" in "PlugIn" is 
always upper-case.) Quick usage reference:<br>
<ul>
	<li>'service' is a reference to the IOKit registry entry of the kernel object 
		(usually of type IOFireWireDevice) representing the device
		of interest. This reference can be obtained using the functions defined in
		IOKit/IOKitLib.h.</li>
	<li>'plugInType' should be CFUUIDGetUUIDBytes(kIOCFPlugInInterfaceID)</li>
	<li>'interfaceType' should be CFUUIDGetUUIDBytes(kIOFireWireLibTypeID) when using IOFireWireLib</li>
</ul>
The interface returned by $link IOCreatePlugInInterfaceForService() should be deallocated using 
IODestroyPlugInInterface(). Do not call Release() on it.
*/

#ifndef __IOFireWireLib_H__
#define __IOFireWireLib_H__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>

#include <IOKit/firewire/IOFWIsoch.h>

// === [CFPlugIn support constants] ========================================

// ============================================================
// device interface
// ============================================================

#pragma mark -
#pragma mark --- device interface UUIDs ---

// version 2 interface -- includes isochronous functions
//  uuid string: B3993EB8-56E2-11D5-8BD0-003065423456
#define kIOFireWireDeviceInterfaceID_v2	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xB3, 0x99, 0x3E, 0xB8, 0x56, 0xE2, 0x11, 0xD5,\
											0x8B, 0xD0, 0x00, 0x30, 0x65, 0x42, 0x34, 0x56)

//	uuid string: E3DF4460-F197-11D4-8AC8-000502072F80
#define kIOFireWireDeviceInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xE3, 0xDF, 0x44, 0x60, 0xF1, 0x97, 0x11, 0xD4,\
											0x8A, 0xC8, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

// ============================================================
// plugin loading
// ============================================================
#pragma mark -
#pragma mark --- plugin loading UUIDs ---

// 	uuid string: A1478010-F197-11D4-A28B-000502072F80
#define	kIOFireWireLibFactoryID			CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xA1, 0x47, 0x80, 0x10,0xF1, 0x97, 0x11, 0xD4,\
											0xA2, 0x8B, 0x00, 0x05,0x02, 0x07, 0x2F, 0x80)
											
//	uuid string: CDCFCA94-F197-11D4-87E6-000502072F80
#define kIOFireWireLibTypeID			CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xCD, 0xCF, 0xCA, 0x94, 0xF1, 0x97, 0x11, 0xD4,\
											0x87, 0xE6, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

// ============================================================
// command objects
// ============================================================

//	uuid string: F8B6993A-F197-11D4-A3F1-000502072F80
#define kIOFireWireCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0xF8, 0xB6, 0x99, 0x3A, 0xF1, 0x97, 0x11, 0xD4,\
											0xA3, 0xF1, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

//	uuid string: 6E32F9D4-F63A-11D4-A194-003065423456
#define kIOFireWireReadCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x6E, 0x32, 0xF9, 0xD4, 0xF6, 0x3A, 0x11, 0xD4,\
											0xA1, 0x94, 0x00, 0x30, 0x65, 0x42, 0x34, 0x56)

//	uuid string: 3D72672A-F64A-11D4-9683-0050E4D93B36
#define kIOFireWireReadQuadletCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x3D, 0x72, 0x67, 0x2A, 0xF6, 0x4A, 0x11, 0xD4,\
											0x96, 0x83, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x36)

//	uuid string: 4EDDED10-F64A-11D4-B7A5-0050E4D93B36
#define kIOFireWireWriteCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x4E, 0xDD, 0xED, 0x10, 0xF6, 0x4A, 0x11, 0xD4,\
											0xB7, 0xA5, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x36)

//	uuid string: 5C9423CE-F64A-11D4-AB7B-0050E4D93B36
#define kIOFireWireWriteQuadletCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x5C, 0x94, 0x23, 0xCE, 0xF6, 0x4A, 0x11, 0xD4,\
											0xAB, 0x7B, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x3)

//	uuid string: 70C10E38-F64A-11D4-AFE7-0050E4D93B36
#define kIOFireWireCompareSwapCommandInterfaceID	CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x70, 0xC1, 0x0E, 0x38, 0xF6, 0x4A, 0x11, 0xD4,\
											0xAF, 0xE7, 0x00, 0x50, 0xE4, 0xD9, 0x3B, 0x36)

//	uuid string: 0D32AC50-F198-11D4-8DB5-000502072F80
#define kIOFireWirePseudoAddressSpaceInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x0D, 0x32, 0xAC, 0x50, 0xF1, 0x98, 0x11, 0xD4,\
											0x8D, 0xB5, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

//	uuid string: 489110F6-F198-11D4-8BEB-000502072F80
#define kIOFireWirePhysicalAddressSpaceInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x48, 0x91, 0x10, 0xF6, 0xF1, 0x98, 0x11, 0xD4,\
											0x8B, 0xEB, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

// ============================================================
// config ROM
// ============================================================

//	uuid string: 69CA4D74-F198-11D4-B325-000502072F80
#define kIOFireWireLocalUnitDirectoryInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x69, 0xCA, 0x4D, 0x74, 0xF1, 0x98, 0x11, 0xD4,\
											0xB3, 0x25, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

//  uuid string: 7D43B506-F198-11D4-AA10-000502072F80
#define kIOFireWireConfigDirectoryInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault,\
											0x7D, 0x43, 0xB5, 0x06, 0xF1, 0x98, 0x11, 0xD4,\
											0xAA, 0x10, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

// ============================================================
//
// IOFireWireLib interface typedefs
//
// ============================================================

typedef struct IOFireWireDeviceInterface_t**	 			IOFireWireLibDeviceRef ;
typedef		   IOFireWireLibDeviceRef						IOFireWireLibUnitRef ;
typedef struct IOFireWirePseudoAddressSpaceInterface_t**	IOFireWireLibPseudoAddressSpaceRef ;
typedef struct IOFireWirePhysicalAddressSpaceInterface_t**	IOFireWireLibPhysicalAddressSpaceRef ;
typedef struct IOFireWireLocalUnitDirectoryInterface_t**	IOFireWireLibLocalUnitDirectoryRef ;
typedef struct IOFireWireConfigDirectoryInterface_t**		IOFireWireLibConfigDirectoryRef ;

typedef struct IOFireWireCommandInterface_t**				IOFireWireLibCommandRef ;
typedef struct IOFireWireReadCommandInterface_t**			IOFireWireLibReadCommandRef ;
typedef struct IOFireWireReadQuadletCommandInterface_t**	IOFireWireLibReadQuadletCommandRef ;
typedef struct IOFireWireWriteCommandInterface_t**			IOFireWireLibWriteCommandRef ;
typedef struct IOFireWireWriteQuadletCommandInterface_t**	IOFireWireLibWriteQuadletCommandRef ;
typedef struct IOFireWireCompareSwapCommandInterface_t**	IOFireWireLibCompareSwapCommandRef ;

// --- isoch interfaces ----------
typedef struct IOFireWireIsochChannelInterface_t**			IOFireWireLibIsochChannelRef ;
typedef struct IOFireWireIsochPortInterface_t**				IOFireWireLibIsochPortRef ;
typedef struct IOFireWireRemoteIsochPortInterface_t**		IOFireWireLibRemoteIsochPortRef ;
typedef struct IOFireWireLocalIsochPortInterface_t**		IOFireWireLibLocalIsochPortRef ;
typedef struct IOFireWireDCLCommandPoolInterface_t**		IOFireWireLibDCLCommandPoolRef ;

// ============================================================
//
// IOFireWireLib callback typedefs
//
// ============================================================

/*!	@typedef IOFireWirePseudoAddressSpaceReadHandler
	@abstract This callback is called to handle read requests to pseudo address spaces. This function
		should fill in the specified area in the pseudo address space backing store and call
		ClientCommandIsComplete with the specified command ID
	@param addressSpace The address space to which the request is being made
	@param commandID An FWClientCommandID which should be passed to ClientCommandIsComplete when
		the buffer has been filled in
	@param packetLen number of bytes requested
	@param packetOffset number of bytes from beginning of address space backing store
	@param srcNodeID nodeID of the requester
	@param destAddressHi high 16 bits of destination address on this computer
	@param destAddressLo low 32 bits of destination address on this computer
	@param refCon user specified reference number passed in when the address space was created
*/
typedef UInt32	(*IOFireWirePseudoAddressSpaceReadHandler)(
					IOFireWireLibPseudoAddressSpaceRef	addressSpace,
					FWClientCommandID					commandID,
					UInt32								packetLen,
					UInt32								packetOffset,
					UInt16								srcNodeID,		// nodeID of requester
					UInt32								destAddressHi,	// destination on this node
					UInt32								destAddressLo,
					UInt32								refCon) ;

/*!	@typedef IOFireWirePseudoAddressSpaceSkippedPacketHandler
	@abstract Callback called when incoming packets have been dropped from the internal queue
	@param addressSpace The address space which dropped the packet(s)
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
	@param skippedPacketCount The number of skipped packets
*/
typedef void	(*IOFireWirePseudoAddressSpaceSkippedPacketHandler)(
					IOFireWireLibPseudoAddressSpaceRef	addressSpace,
					FWClientCommandID					commandID,
					UInt32								skippedPacketCount) ;

/*! @typedef IOFireWirePseudoAddressSpaceWriteHandler
	@abstract Callback called to handle write requests to a pseudo address space.
	@param addressSpace The address space to which the write is being made
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
	@param packetLen Length in bytes of incoming packet
	@param packet Pointer to the received data
	@param srcNodeID Node ID of the sender
	@param destAddressHi high 16 bits of destination address on this computer
	@param destAddressLo low 32 bits of destination address on this computer
	@param refCon user specified reference number passed in when the address space was created
*/
typedef UInt32 (*IOFireWirePseudoAddressSpaceWriteHandler)(
					IOFireWireLibPseudoAddressSpaceRef	addressSpace,
					FWClientCommandID					commandID,
					UInt32								packetLen,
					void*								packet,
					UInt16								srcNodeID,		// nodeID of sender
					UInt32								destAddressHi,	// destination on this node
					UInt32								destAddressLo,
					UInt32								refCon) ;

/*!	@typedef IOFireWireBusResetHandler
	@abstract Called when a bus reset has occured, but before FireWire has completed
		configuring the bus.
	@param interface A reference to the device on which the callback was installed
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
*/
typedef void 	(*IOFireWireBusResetHandler)(
					IOFireWireLibDeviceRef				interface,
					FWClientCommandID					commandID );	// parameters may change
					
/*!
	@typedef IOFireWireBusResetDoneHandler
	@abstract Called when a bus reset has occured and FireWire has completed configuring
		the bus.
	@param interface A reference to the device on which the callback was installed
	@param commandID An FWClientCommandID to be passed to ClientCommandIsComplete()
*/
typedef void 	(*IOFireWireBusResetDoneHandler)(
					IOFireWireLibDeviceRef				interface,
					FWClientCommandID					commandID ) ;	// parameters may change

/*!	@typedef IOFireWireLibCommandCallback
	@abstract Callback called when an asynchronous command has completed executing
	@param refCon A user specified reference value set before command object was submitted
*/
typedef void	(*IOFireWireLibCommandCallback)(
					void*								refCon,
					IOReturn							completionStatus) ;

// unused.
typedef void (*IOFireWireDeviceAddedCallback)() ;
typedef void (*IOFireWireDeviceRemovedCallback)() ;

// unused.
typedef struct FWInterfaceCallBacks_t
{
	IOFireWireBusResetHandler					busResetHandler ;
	IOFireWireBusResetDoneHandler				busResetDoneHandler ;
} FWInterfaceCallBacks ;

// unused.
typedef struct IOFireWirePseudoAddressSpaceCallbacks_t
{
	IOFireWirePseudoAddressSpaceReadHandler				readHandler ;
	IOFireWirePseudoAddressSpaceSkippedPacketHandler	skippedPacketHandler ;
	IOFireWirePseudoAddressSpaceWriteHandler			writeHandler ;
} IOFireWirePseudoAddressSpaceCallbacks ;

// ============================================================
//
// IOFireWireDeviceInterface
//
// ============================================================

/*!	@class IOFireWireDeviceInterface
	@abstract IOFireWireDeviceInterface is your primary gateway to the functionality contained in
		IOFireWireLib.
	@discussion	
		You can use IOFireWireDeviceInterface to:<br>
	<ul>
		<li>perform synchronous read, write and lock operations</li>
		<li>perform other miscellanous bus operations, such as reset the FireWire bus. </li>
		<li>create FireWire command objects and interfaces used to perform
			synchronous/asynchronous read, write and lock operations. These include:</li>
		<ul type="square">
			<li>IOFireWireReadCommandInterface
			<li>IOFireWireReadQuadletCommandInterface
			<li>IOFireWireWriteCommandInterface
			<li>IOFireWireWriteQuadletCommandInterface
			<li>IOFireWireCompareSwapCommandInterface
		</ul>
		<li>create interfaces which provide a other extended services. These include:</li>
		<ul type="square">	
			<li>IOFireWirePseudoAddressSpaceInterface -- pseudo address space services</li>
			<li>IOFireWirePhysicalAddressSpaceInterface -- physical address space services</li>
			<li>IOFireWireLocalUnitDirectoryInterface -- manage local unit directories in the mac</li>
			<li>IOFireWireConfigDirectoryInterface -- access and browse remote device config directories</li>
		</ul>
		<li>create interfaces which provide isochronous services (see IOFireWireLibIsoch.h). These include:</li>
		<ul type="square">	
			<li>IOFireWireIsochChannelInterface -- create/manage talker and listener isoch channels</li>
			<li>IOFireWireLocalIsochPortInterface -- create local isoch ports</li>
			<li>IOFireWireRemoteIsochPortInterface -- create remote isoch ports</li>
			<li>IOFireWireDCLCommandPoolInterface -- create a DCL command pool allocator.</li>
		</ul>
	</ul>

*/
typedef struct IOFireWireDeviceInterface_t
{
	IUNKNOWN_C_GUTS ;

	UInt32 version, revision ; // version/revision
	
		// --- maintenance methods -------------
    /*!
        @function InterfaceIsInited
        @abstract Determine whether interface has been properly inited.
        @param self The device interface to use.
        @result Returns true if interface is inited and false if is it not.
    */
	Boolean				(*InterfaceIsInited)(IOFireWireLibDeviceRef self) ;

    /*!
        @function GetDevice
        @abstract Get the IOKit service to which this interface is connected.
        @param self The device interface to use.
        @result Returns an io_object_t corresponding to the device the interface is
			using
    */
	io_object_t			(*GetDevice)(IOFireWireLibDeviceRef self) ;

    /*!
        @function Open
        @abstract Open the connected device for exclusive access. When you have
			the device open using this method, all accesses by other clients of 
			this device will be denied until Close() is called.
        @param self The device interface to use.
        @result An IOReturn error code
    */
	IOReturn			(*Open)(IOFireWireLibDeviceRef self) ;

    /*!
        @function OpenWithSessionRef
        @abstract An open function which allows this interface to have access
			to the device when already opened. The service which has already opened
			the device must be able to provide an IOFireWireSessionRef.
        @param self The device interface to use
		@param IOFireWireSessionRef The sessionRef returned from the client who has
			the device open
        @result An IOReturn error code
    */
	IOReturn			(*OpenWithSessionRef)(IOFireWireLibDeviceRef self, IOFireWireSessionRef sessionRef) ;

    /*!
        @function Close
        @abstract Release exclusive access to the device
        @param self The device interface to use
    */
	void				(*Close)(IOFireWireLibDeviceRef self) ;
	
		// --- notification --------------------
	/*!
		@function NotificationIsOn
		@abstract Determine whether callback notifications for this interface are currently active
        @param self The device interface to use
		@result A Boolean value where true indicates notifications are active
	*/
	const Boolean		(*NotificationIsOn)(IOFireWireLibDeviceRef self) ;
	
	/*!
		@function AddCallbackDispatcherToRunLoop
		@abstract Installs the proper run loop event source to allow callbacks to function. This method
			must be called before callback notifications for this interface or any interfaces
			created using this interface can function.
        @param self The device interface to use.
		@param inRunLoop The run loop on which to install the event source
	*/
	const IOReturn 		(*AddCallbackDispatcherToRunLoop)(IOFireWireLibDeviceRef self, CFRunLoopRef inRunLoop) ;
	
	/*!
		@function RemoveCallbackDispatcherFromRunLoop
		@abstract Reverses the effects of AddCallbackDispatcherToRunLoop(). This method removes 
			the run loop event source that was added to the specified run loop preventing any 
			future callbacks from being called
        @param self The device interface to use.			
	*/
	const void			(*RemoveCallbackDispatcherFromRunLoop)(IOFireWireLibDeviceRef self) ;
	
	/*!
		@function TurnOnNotification
		@abstract Activates any callbacks specified for this device interface. Only works after 
			AddCallbackDispatcherToRunLoop has been called. See also AddIsochCallbackDispatcherToRunLoop().
        @param self The device interface to use.
		@result A Boolean value. Returns true on success.
	*/
	const Boolean 		(*TurnOnNotification)(IOFireWireLibDeviceRef self) ;

	/*!
		@function TurnOffNotification
		@abstract Deactivates and callbacks specified for this device interface. Reverses the 
			effects of TurnOnNotification()
        @param self The device interface to use.
	*/
	void				(*TurnOffNotification)(IOFireWireLibDeviceRef self) ;
	
	/*!
		@function SetBusResetHandler
		@abstract Sets the callback that should be called when a bus reset occurs. Note that this callback
			can be called multiple times before the bus reset done handler is called. (f.ex., multiple bus
			resets might occur before bus reconfiguration has completed.)
        @param self The device interface to use.
		@param handler Function pointer to the handler to install
		@result Returns an IOFireWireBusResetHandler function pointer to the previously installed
			bus reset handler. Returns 0 if none was set.
	*/
	const IOFireWireBusResetHandler	
						(*SetBusResetHandler)(IOFireWireLibDeviceRef self, IOFireWireBusResetHandler handler) ;
	
	/*!
		@function SetBusResetDoneHandler
		@abstract Sets the callback that should be called after a bus reset has occurred and reconfiguration
			of the bus has been completed. This function will only be called once per bus reset.
        @param self The device interface to use.
		@param handler Function pointer to the handler to install
		@result Returns on IOFireWireBusResetDoneHandler function pointer to the previously installed
			bus reset handler. Returns 0 if none was set.
	*/
	const IOFireWireBusResetDoneHandler	
						(*SetBusResetDoneHandler)(IOFireWireLibDeviceRef self, IOFireWireBusResetDoneHandler handler) ;
	/*!
		@function ClientCommandIsComplete
		@abstract This function must be called from callback routines once they have completed processing
			a callback. This function only applies to callbacks which take an IOFireWireLibDeviceRef (i.e. bus reset),
			parameter.
		@param commandID The command ID passed to the callback function when it was called
		@param status An IOReturn value indicating the completion status of the callback function
	*/
	void				(*ClientCommandIsComplete)(IOFireWireLibDeviceRef self, FWClientCommandID commandID, IOReturn status) ;
	
		// --- read/write/lock operations -------
	/*!
		@function Read
		@abstract Perform synchronous block read
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to read. 
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param buf A pointer to a buffer where the results will be stored
		@param size Number of bytes to read
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in generation. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn			(*Read)(
								IOFireWireLibDeviceRef	self, 
								io_object_t 		device,
								const FWAddress* addr, 
								void* 				buf, 
								UInt32* 			size, 
								Boolean 			failOnReset, 
								UInt32 				generation) ;

	/*!
		@function ReadQuadlet
		@abstract Perform synchronous quadlet read
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to read. 
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param value A pointer to where to data should be stored
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in generation. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn			(*ReadQuadlet)(
								IOFireWireLibDeviceRef	self, 
								io_object_t 		device,
								const FWAddress* addr, 
								UInt32* 			val, 
								Boolean 			failOnReset, 
								UInt32 				generation) ;
	/*!
		@function Write
		@abstract Perform synchronous block write
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param buf A pointer to a buffer where the results will be stored
		@param size Number of bytes to read
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn			(*Write)(	IOFireWireLibDeviceRef 	self, 
									io_object_t 			device, 
									const FWAddress* 		addr, 
									const void* 			buf, 
									UInt32* 				size,
									Boolean 				failOnReset, 
									UInt32 					generation) ;

	/*!
		@function WriteQuadlet
		@abstract Perform synchronous quadlet write
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param val The value to write
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling $link GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn (*WriteQuadlet)(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, const UInt32 val, Boolean failOnReset, UInt32 generation) ;

	/*!
		@function CompareSwap
		@abstract Perform synchronous lock operation
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param cmpVal The check/compare value
		@param newVal Value to set
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling $link GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOReturn error code
	*/
	IOReturn (*CompareSwap)(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, UInt32 cmpVal, UInt32 newVal, Boolean failOnReset, UInt32 generation) ;
	
	// --- FireWire command object methods ---------

	/*!
		@function CreateReadCommand
		@abstract Create a block read command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param buf A pointer to a buffer where the results will be stored
		@param size Number of bytes to read
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling $link GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See $link IOFireWireLibCommandRef.
	*/
	IOFireWireLibCommandRef (*CreateReadCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, void* buf, UInt32 size, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!	@function CreateReadQuadletCommand
		@abstract Create a quadlet read command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param quads An array of quadlets where results should be stored
		@param numQuads Number of quadlets to read
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling $link GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@param 
		@result An IOFireWireLibCommandRef interface. See $link IOFireWireLibCommandRef.*/
	IOFireWireLibCommandRef (*CreateReadQuadletCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, UInt32 quads[], UInt32 numQuads, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!	@function CreateWriteCommand
		@abstract Create a block write command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param buf A pointer to the buffer containing the data to be written
		@param size Number of bytes to write
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling $link GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See $link IOFireWireLibCommandRef.*/
	IOFireWireLibCommandRef (*CreateWriteCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, void* buf, UInt32  size, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!
		@function CreateWriteQuadletCommand
		@abstract Create a quadlet write command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param quads An array of quadlets containing quadlets to be written
		@param numQuads Number of quadlets to write
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling $link GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See $link IOFireWireLibCommandRef.
	*/
	IOFireWireLibCommandRef (*CreateWriteQuadletCommand)(IOFireWireLibDeviceRef	self, io_object_t device, const FWAddress *	addr, UInt32 quads[], UInt32 numQuads, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

	/*!
		@function CreateCompareSwapCommand
		@abstract Create a quadlet compare/swap command object.
		@param self The device interface to use.
		@param device The service (representing an attached FireWire device) to which to write.
			For 48-bit, device relative addressing, pass the service used to create the device interface. This
			can be obtained by calling GetDevice(). For 64-bit absolute addressing, pass 0. Other values are
			unsupported.
		@param addr Command target address
		@param quads An array of quadlets containing quadlets to be written
		@param numQuads Number of quadlets to write
		@param failOnReset Pass true if the command should only be executed during the FireWire bus generation
			specified in 'generation'. Pass false to ignore the generation parameter. The generation can be
			obtained by calling $link GetGenerationAndNodeID()
		@param generation The FireWire bus generation during which the command should be executed. Ignored
			if failOnReset is false.
		@result An IOFireWireLibCommandRef interface. See $link IOFireWireLibCommandRef.	*/
	IOFireWireLibCommandRef (*CreateCompareSwapCommand)( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress *  addr, UInt32      cmpVal, UInt32      newVal, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid) ;

		// --- other methods ---------------------------
	/*!	@function BusReset
		@abstract Cause a bus reset
		@param self The device interface to use. */	
	IOReturn (*BusReset)( IOFireWireLibDeviceRef  self) ;

	/*!	@function GetCycleTime
		@abstract Get bus cycle time.
		@param self The device interface to use.
		@param outCycleTime A pointer to a UInt32 to hold the result
		@result An IOReturn error code.	*/	
	IOReturn (*GetCycleTime)( IOFireWireLibDeviceRef  self, UInt32*  outCycleTime) ;

	/*!	@function GetGenerationAndNodeID
		@abstract Get bus generation and remote device node ID.
		@param self The device interface to use.
		@param outGeneration A pointer to a UInt32 to hold the generation result
		@param outNodeID A pointer to a UInt16 to hold the remote device node ID
		@result An IOReturn error code.	*/	
	IOReturn (*GetGenerationAndNodeID)( IOFireWireLibDeviceRef  self, UInt32*  outGeneration, UInt16*  outNodeID) ;

	/*!	@function GetLocalNodeID
		@abstract Get local node ID.
		@param self The device interface to use.
		@param outNodeID A pointer to a UInt16 to hold the local device node ID
		@result An IOReturn error code.	*/	
	IOReturn (*GetLocalNodeID)( IOFireWireLibDeviceRef  self, UInt16*  outLocalNodeID) ;

	/*!	@function GetResetTime
		@abstract Get time since last bus reset.
		@param self The device interface to use.
		@param outResetTime A pointer to an AbsolutTime to hold the result.
		@result An IOReturn error code.	*/	
	IOReturn			(*GetResetTime)(
								IOFireWireLibDeviceRef 	self, 
								AbsoluteTime* 			outResetTime) ;

		// --- unit directory support ------------------
	/*!	@function CreateLocalUnitDirectory
		@abstract Creates a local unit directory object and returns an interface to it. An
			instance of a unit directory object corresponds to an instance of a unit 
			directory in the local machine's configuration ROM.
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created unit directory object.
		@result An IOFireWireLibLocalUnitDirectoryRef. Returns 0 upon failure */
	 IOFireWireLibLocalUnitDirectoryRef (*CreateLocalUnitDirectory)( IOFireWireLibDeviceRef  self, REFIID  iid) ;

		// --- config directory support ----------------
	/*!	@function GetConfigDirectory
		@abstract Creates a config directory object and returns an interface to it. The
			created config directory object represents the config directory in the remote
			device or unit to which the creating device interface is attached.
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created config directory object.
		@result An IOFireWireLibConfigDirectoryRef. Returns 0 upon failure */
	IOFireWireLibConfigDirectoryRef (*GetConfigDirectory)( IOFireWireLibDeviceRef  self, REFIID  iid) ;

	/*!	@function CreateConfigDirectoryWithIOObject
		@abstract This function can be used to create a config directory object and a
			corresponding interface from an opaque IOObject reference. Some configuration
			directory interface methods may return an io_object_t instead of an
			IOFireWireLibConfigDirectoryRef. Use this function to obtain an 
			IOFireWireLibConfigDirectoryRef from an io_object_t.
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created config directory object.
		@result An IOFireWireLibConfigDirectoryRef. Returns 0 upon failure */
	IOFireWireLibConfigDirectoryRef (*CreateConfigDirectoryWithIOObject)( IOFireWireLibDeviceRef  self, io_object_t  inObject, REFIID  iid) ;

		// --- address space support -------------------
	/*!	@function CreatePseudoAddressSpace
		@abstract Creates a pseudo address space object and returns an interface to it. This
			will create a pseudo address space (software-backed) on the local machine. 
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created pseudo address space object.
		@result An IOFireWireLibPseudoAddressSpaceRef. Returns 0 upon failure */
	IOFireWireLibPseudoAddressSpaceRef (*CreatePseudoAddressSpace)( IOFireWireLibDeviceRef  self, UInt32  inSize, void*  inRefCon, UInt32  inQueueBufferSize, void*  inBackingStore, UInt32  inFlags, REFIID  iid) ;

	/*!	@function CreatePhysicalAddressSpace
		@abstract Creates a physical address space object and returns an interface to it. This
			will create a physical address space on the local machine. 
		@param self The device interface to use.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created physical address space object.
		@result An IOFireWireLibPhysicalAddressSpaceRef. Returns 0 upon failure */
	IOFireWireLibPhysicalAddressSpaceRef (*CreatePhysicalAddressSpace)( IOFireWireLibDeviceRef  self, UInt32  inSize, void*  inBackingStore, UInt32  inFlags, REFIID  iid) ;
		
		// --- debugging -------------------------------
	IOReturn (*FireBugMsg)( IOFireWireLibDeviceRef  self, const char*  msg) ;

	//
	// NOTE: the following methods available only in interface v2 and later
	//

		// --- eye-sock-run-U.S. -----------------------
	/*!	@function AddIsochCallbackDispatchToRunLoop
		@abstract This function add an event source for the isochronous callback dispatcher
			to the specified CFRunLoop. Isochronous related callbacks will not function
			before this function is called. This functions is similar to $link 
			AddCallbackDispatcherToRunLoop. The passed CFRunLoop can be different
			from that passed to AddCallbackDispatcherToRunLoop. 
		@param self The device interface to use.
		@param inRunLoop A CFRunLoopRef for the run loop to which the event loop source 
			should be added
		@result An IOReturn error code.	*/	
	IOReturn			(*AddIsochCallbackDispatcherToRunLoop)(
								IOFireWireLibDeviceRef 	self, 
								CFRunLoopRef 			inRunLoop) ;

	/*!	@function CreateRemoteIsochPort
		@abstract Creates a remote isochronous port object and returns an interface to it. A
			remote isochronous port object is an abstract entity used to represent a remote
			talker or listener device on an isochronous channel. 
		@param self The device interface to use.
		@param inTalking Press true if this port represents an isochronous talker. Pass
			false if this port represents an isochronous listener.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created remote isochronous port object.
		@result An IOFireWireLibRemoteIsochPortRef. Returns 0 upon failure */
	IOFireWireLibRemoteIsochPortRef
						(*CreateRemoteIsochPort)(
								IOFireWireLibDeviceRef	self,
								Boolean					inTalking,
								REFIID					iid) ;

	/*!	@function CreateLocalIsochPort
		@abstract Creates a local isochronous port object and returns an interface to it. A
			local isochronous port object is an abstract entity used to represent a
			talking or listening endpoint in the local machine. 
		@param self The device interface to use.
		@param inTalking Press true if this port represents an isochronous talker. Pass
			false if this port represents an isochronous listener.
		@param inDCLProgram A pointer to the first DCL command struct of the DCL program
			to be compiled and used to send or receive data on this port.
		@param inStartEvent Start event bits
		@param inStartState Start state bits
		@param inStartMask Start mask bits
		@param inDCLProgramRanges This is an optional optimization parameter which can be used
			to decrease the time the local port object spends determining which set of virtual
			ranges the passed DCL program occupies. Pass a pointer to an array of IOVirtualRange
			structs or nil to ignore this parameter.
		@param inDCLProgramRangeCount The number of virtual ranges passed to inDCLProgramRanges.
			Pass 0 for none.
		@param inBufferRanges This is an optional optimization parameter which can be used
			to decrease the time the local port object spends determining which set of virtual
			ranges the data buffers referenced by the passed DCL program occupy. Pass a pointer
			to an array of IOVirtualRange structs or nil to ignore this parameter.
		@param inBufferRangeCount The number of virtual ranges passed to inBufferRanges.
			Pass 0 for none.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created object.
		@result An IOFireWireLibLocalIsochPortRef. Returns 0 upon failure */
	IOFireWireLibLocalIsochPortRef
						(*CreateLocalIsochPort)(
								IOFireWireLibDeviceRef 	self, 
								Boolean					inTalking,
								DCLCommandPtr			inDCLProgram,
								UInt32					inStartEvent,
								UInt32					inStartState,
								UInt32					inStartMask,
								IOVirtualRange			inDCLProgramRanges[],	// optional optimization parameters
								UInt32					inDCLProgramRangeCount,	
								IOVirtualRange			inBufferRanges[],
								UInt32					inBufferRangeCount, 
								REFIID 					iid) ; 

	/*!	@function CreateIsochChannel
		@abstract Creates an isochronous channel object and returns an interface to it. An
			isochronous channel object is an abstract entity used to represent a
			FireWire isochronous channel. 
		@param self The device interface to use.
		@param inTalking Press true if this port represents an isochronous talker. Pass
			false if this port represents an isochronous listener.
		@param inDCLProgram A pointer to the first DCL command struct of the DCL program
			to be compiled and used to send or receive data on this port.
		@param inStartEvent Start event bits
		@param inStartState Start state bits
		@param inStartMask Start mask bits
		@param inDCLProgramRanges This is an optional optimization parameter which can be used
			to decrease the time the local port object spends determining which set of virtual
			ranges the passed DCL program occupies. Pass a pointer to an array of IOVirtualRange
			structs or nil to ignore this parameter.
		@param inDCLProgramRangeCount The number of virtual ranges passed to inDCLProgramRanges.
			Pass 0 for none.
		@param inBufferRanges This is an optional optimization parameter which can be used
			to decrease the time the local port object spends determining which set of virtual
			ranges the data buffers referenced by the passed DCL program occupy. Pass a pointer
			to an array of IOVirtualRange structs or nil to ignore this parameter.
		@param inBufferRangeCount The number of virtual ranges passed to inBufferRanges.
			Pass 0 for none.
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created object.
		@result An IOFireWireLibLocalIsochPortRef. Returns 0 upon failure */
	IOFireWireLibIsochChannelRef
						(*CreateIsochChannel)(
								IOFireWireLibDeviceRef 	self, 
								Boolean 				doIrm, 
								UInt32 					packetSize, 
								IOFWSpeed 				prefSpeed, 
								REFIID 					iid ) ;

	/*!	@function CreateDCLCommandPool
		@abstract Creates a command pool object and returns an interface to it. The command 
			pool can be used to build DCL programs.
		@param self The device interface to use.
		@param size Starting size of command pool
		@param iid An ID number, of type CFUUIDBytes (see CFUUID.h), identifying the
			type of interface to be returned for the created object.
		@result An IOFireWireLibDCLCommandPoolRef. Returns 0 upon failure */
	IOFireWireLibDCLCommandPoolRef
						(*CreateDCLCommandPool)(
								IOFireWireLibDeviceRef	self, 
								IOByteCount 			size, 
								REFIID 					iid ) ;

	// --- refcons ---------------------------------
	void*				(*GetRefCon)(
								IOFireWireLibDeviceRef	self) ;
	void				(*SetRefCon)(
								IOFireWireLibDeviceRef	self,
								const void*				refCon) ;

	// --- debugging -------------------------------
	// do not use
	CFTypeRef			(*GetDebugProperty)(
								IOFireWireLibDeviceRef	self,
								void*					interface,
								CFStringRef				inPropertyName,
								CFTypeID*				outPropertyType) ;
	void 				(*PrintDCLProgram)(
								IOFireWireLibDeviceRef	self, 
								const DCLCommandPtr		inProgram, 
								UInt32 					inLength) ;
} IOFireWireDeviceInterface, IOFireWireUnitInterface ;

// ============================================================
//
// IOFireWirePseudoAddressSpaceInterface
//
// ============================================================

/*!	@class IOFireWirePseudoAddressSpace (IOFireWireLib)
	@discussion Represents and provides management functions for a pseudo address 
		space (software-backed) in the local machine.

		Pseudo address space objects can be created using IOFireWireDeviceInterface.*/
typedef struct IOFireWirePseudoAddressSpaceInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;

	/*!	@function SetWriteHandler
		@abstract Set the callback that should be called to handle write accesses to
			the corresponding address space
		@param self The address space interface to use.
		@param inWriter The callback to set.
		@result Returns the callback that was previously set or nil for none.*/
	const IOFireWirePseudoAddressSpaceWriteHandler (*SetWriteHandler)( IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceWriteHandler	inWriter) ;

	/*!	@function SetWriteHandler
		@abstract Set the callback that should be called to handle read accesses to
			the corresponding address space
		@param self The address space interface to use.
		@param inReader The callback to set.
		@result Returns the callback that was previously set or nil for none.*/
	const IOFireWirePseudoAddressSpaceReadHandler (*SetReadHandler)( IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceReadHandler		inReader) ;

	/*!	@function SetSkippedPacketHandler
		@abstract Set the callback that should be called when incoming packets are
			dropped by the address space.
		@param self The address space interface to use.
		@param inHandler The callback to set.
		@result Returns the callback that was previously set or nil for none.*/
	const IOFireWirePseudoAddressSpaceSkippedPacketHandler (*SetSkippedPacketHandler)( IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceSkippedPacketHandler inHandler) ;

	/*!	@function NotificationIsOn
		@abstract Is notification on?
		@param self The address space interface to use.
		@result Returns true if packet notifications for this address space are active */
	Boolean (*NotificationIsOn)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function TurnOnNotification
		@abstract Try to turn on packet notifications for this address space.
		@param self The address space interface to use.
		@result Returns true upon success */
	Boolean (*TurnOnNotification)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function TurnOffNotification
		@abstract Force packet notification off.
		@param self The pseudo address interface to use. */
	void (*TurnOffNotification)(IOFireWireLibPseudoAddressSpaceRef self) ;	

	/*!	@function ClientCommandIsComplete
		@abstract Notify the address space that a packet notification handler has completed.
		@discussion Packet notifications are received one at a time, in order. This function
			must be called after a packet handler has completed its work.
		@param self The address space interface to use.
		@param commandID The ID of the packet notification being completed. This is the same
			ID that was passed when a packet notification handler is called.
		@param status The completion status of the packet handler */
	void		(*ClientCommandIsComplete)(IOFireWireLibPseudoAddressSpaceRef self, FWClientCommandID commandID, IOReturn status) ;

		// --- accessors ----------
	/*!	@function GetFWAddress
		@abstract Get the FireWire address of this address space
		@param self The pseudo address interface to use. */
	void (*GetFWAddress)(IOFireWireLibPseudoAddressSpaceRef self, FWAddress* outAddr) ;

	/*!	@function GetBuffer
		@abstract Get a pointer to the backing store for this address space
		@param self The address space interface to use.
		@result A pointer to the backing store of this pseudo address space. Returns
			nil if none. */
	void* (*GetBuffer)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function GetBufferSize
		@abstract Get the size in bytes of this address space.
		@param self The address space interface to use.
		@result Size of the pseudo address space in bytes. Returns 0 for none.*/
	const UInt32 (*GetBufferSize)(IOFireWireLibPseudoAddressSpaceRef self) ;

	/*!	@function GetRefCon
		@abstract Returns the user refCon value for this address space.
		@param self The address space interface to use.
		@result Size of the pseudo address space in bytes. Returns 0 for none.*/
	void* (*GetRefCon)(IOFireWireLibPseudoAddressSpaceRef self) ;


} IOFireWirePseudoAddressSpaceInterface ;

// ============================================================
//
// IOFireWireLocalUnitDirectoryInterface
//
// ============================================================

typedef struct IOFireWireLocalUnitDirectoryInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;

	// --- adding to ROM -------------------
	IOReturn			(*AddEntry_Ptr)(IOFireWireLibLocalUnitDirectoryRef self, int key, void* inBuffer, size_t inLen, CFStringRef inDesc) ;
	IOReturn			(*AddEntry_UInt32)(IOFireWireLibLocalUnitDirectoryRef self, int key, UInt32 value, CFStringRef inDesc) ;
	IOReturn			(*AddEntry_FWAddress)(IOFireWireLibLocalUnitDirectoryRef self, int key, const FWAddress* value, CFStringRef inDesc) ;

	// Use this function to cause your unit directory to appear in the Mac's config ROM.
	IOReturn			(*Publish)(IOFireWireLibLocalUnitDirectoryRef self) ;
	IOReturn			(*Unpublish)(IOFireWireLibLocalUnitDirectoryRef self) ;
} IOFireWireLocalUnitDirectoryInterface ;

// ============================================================
//
// IOFireWireLibPhysicalAddressSpaceInterface
//
// ============================================================

/*!	@class IOFireWirePhysicalAddressSpace
	@abstract IOFireWireLib physical address space object. ( interface name: IOFireWirePhysicalAddressSpaceInterface )
	@discussion Represents and provides management functions for a physical address 
		space (hardware-backed) in the local machine.<br>
		Physical address space objects can be created using IOFireWireDeviceInterface.*/
typedef struct IOFireWirePhysicalAddressSpaceInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	/*!	@function GetPhysicalSegments
		@abstract Returns the list of physical memory ranges this address space occupies
			on the local machine.
		@param self The address space interface to use.
		@param ioSegmentCount Pass in a pointer to the number of list entries in 
			outSegments and outAddress. Upon completion, this will contain the actual
			number of segments returned in outSegments and outAddress
		@param outSegments A pointer to an array to hold the function results. Upon
			completion, this will contain the lengths of the physical segments this
			address space occupies on the local machine
		@param outAddress A pointer to an array to hold the function results. Upon
			completion, this will contain the addresses of the physical segments this
			address space occupies on the local machine. */
	void				(*GetPhysicalSegments)(
								IOFireWireLibPhysicalAddressSpaceRef self,
								UInt32*				ioSegmentCount,
								IOByteCount			outSegments[],
								IOPhysicalAddress	outAddresses[]) ;						
	/*!	@function GetPhysicalSegment
		@abstract Returns the physical segment containing the address at a specified offset
			from the beginning of this address space
		@param self The address space interface to use.
		@param offset Offset from beginning of address space
		@param length Pointer to a value which upon completion will contain the length of
			the segment returned by the function.
		@result The address of the physical segment containing the address at the specified
			offset of the address space	*/
	IOPhysicalAddress	(*GetPhysicalSegment)(
								IOFireWireLibPhysicalAddressSpaceRef self,
								IOByteCount 		offset,
								IOByteCount*		length) ;

	/*!	@function GetPhysicalAddress
		@abstract Returns the physical address of the beginning of this address space
		@param self The address space interface to use.
		@result The physical address of the start of this address space	*/
	IOPhysicalAddress	(*GetPhysicalAddress)(
								IOFireWireLibPhysicalAddressSpaceRef self) ;

		// --- accessors ----------
	/*!	@function GetFWAddress
		@abstract Get the FireWire address of this address space
		@param self The address space interface to use.	*/
	void				(*GetFWAddress)(void* self, FWAddress* outAddr) ;

	/*!	@function GetBuffer
		@abstract Get a pointer to the backing store for this address space
		@param self The address space interface to use.
		@result A pointer to the backing store of this address space.*/
	void*				(*GetBuffer)(void* self) ;

	/*!	@function GetBufferSize
		@abstract Get the size in bytes of this address space.
		@param self The address space interface to use.
		@result Size of the pseudo address space in bytes.	*/
	const UInt32		(*GetBufferSize)(void* self) ;

} IOFireWirePhysicalAddressSpaceInterface ;

// ============================================================
//
// IOFireWireLibPhysicalAddressSpaceInterface
//
// ============================================================

#define IOFIREWIRELIBCOMMAND_C_GUTS \
	IOReturn			(*GetStatus)(IOFireWireLibCommandRef	self) ;	\
	UInt32				(*GetTransferredBytes)(IOFireWireLibCommandRef self) ; \
	void				(*GetTargetAddress)(IOFireWireLibCommandRef self, FWAddress* outAddr) ; \
	void				(*SetTarget)(IOFireWireLibCommandRef self, const FWAddress* addr) ;	\
	void				(*SetGeneration)(IOFireWireLibCommandRef self, UInt32 generation) ;	\
	void				(*SetCallback)(IOFireWireLibCommandRef self, IOFireWireLibCommandCallback inCallback) ;	\
	void				(*SetRefCon)(IOFireWireLibCommandRef self, void* refCon) ;	\
	const Boolean		(*IsExecuting)(IOFireWireLibCommandRef self) ;	\
	IOReturn			(*Submit)(IOFireWireLibCommandRef self) ;	\
	IOReturn			(*SubmitWithRefconAndCallback)(IOFireWireLibCommandRef self, void* refCon, IOFireWireLibCommandCallback inCallback) ;\
	IOReturn			(*Cancel)(IOFireWireLibCommandRef self, IOReturn reason)
	
typedef struct IOFireWireCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	/*!	@function GetStatus
		@abstract Return command completion status.
		@param self The command object interface of interest
		@result An IOReturn error code indicating the completion error (if any) returned the last
			time this command object was executed	*/
		/*
		IOReturn (*GetStatus)(IOFireWireLibCommandRef	self) ;
		*/

	/*!	@function GetTransferredBytes
		@abstract Return number of bytes transferred by this command object when it last completed
			execution.
		@param self The command object interface of interest
		@result A UInt32 containing the bytes transferred value	*/
		/*
		UInt32				(*GetTransferredBytes)(IOFireWireLibCommandRef self) ;
		*/
		
	/*!	@function GetTargetAddress
		@abstract Get command target address.
		@param self The command object interface of interest
		@param outAddr A pointer to an FWAddress to contain the function result. */
		/*
		void				(*GetTargetAddress)(IOFireWireLibCommandRef self, FWAddress* outAddr) ;
		*/
	
	/*!	@function SetTarget
		@abstract Set command target address
		@param self The command object interface of interest
		@param addr A pointer to an FWAddress. */
		/*
		void				(*SetTarget)(IOFireWireLibCommandRef self, const FWAddress* addr) ;
		*/

	/*!	@function SetGeneration
		@abstract Set FireWire bus generation for which the command object shall be valid.
			If the failOnReset attribute has been set, the command will only be considered for
			execution during the bus generation specified by this function.
		@param self The command object interface of interest
		@param generation A bus generation. The current bus generation can be obtained
			from IOFireWireDeviceInterface::GetGenerationAndNodeID().	*/
		/*
		void				(*SetGeneration)(IOFireWireLibCommandRef self, UInt32 generation) ;
		*/
		
	/*!	@function SetCallback
		@abstract Set the completion handler to be called once the command completes
			asynchronous execution .
		@param self The command object interface of interest
		@param inCallback A callback handler. Passing nil forces the command object to 
			execute synchronously. */
		/*
		void				(*SetCallback)(IOFireWireLibCommandRef self, IOFireWireLibCommandCallback inCallback) ;
		*/
		
	/*!	@function SetRefCon
		@abstract Set the user refCon value. This is the user defined value that will be passed
			in the refCon argument to the completion function.	*/
		/*
		void				(*SetRefCon)(IOFireWireLibCommandRef self, void* refCon) ;
		*/
		
	/*!	@function IsExecuting
		@abstract Is this command object currently executing?
		@param self The command object interface of interest
		@result Returns true if the command object is executing.	*/
		/*
		const Boolean		(*IsExecuting)(IOFireWireLibCommandRef self) ;
		*/
		
	/*!	@function Submit
		@abstract Submit this command object to FireWire for execution.
		@param self The command object interface of interest
		@result An IOReturn result code indicating whether or not the command was successfully
			submitted */
		/*
		IOReturn			(*Submit)(IOFireWireLibCommandRef self) ;
		*/
		
	/*!	@function SubmitWithRefconAndCallback
		@abstract Set the command refCon value and callback handler, and submit the command
			to FireWire for execution.
		@param self The command object interface of interest
		@result An IOReturn result code indicating whether or not the command was successfully
			submitted	*/
		/*
		IOReturn			(*SubmitWithRefconAndCallback)(IOFireWireLibCommandRef self, void* refCon, IOFireWireLibCommandCallback inCallback) ;
		*/
		
	/*!	@function Cancel
		@abstract Cancel command execution
		@param self The command object interface of interest
		@result An IOReturn result code	*/
		/*
		IOReturn			(*Cancel)(IOFireWireLibCommandRef self, IOReturn reason) ;
		*/
		
	IOFIREWIRELIBCOMMAND_C_GUTS ;
} IOFireWireCommandInterface ;

/*!	@class IOFireWireReadCommandInterface
	@abstract IOFireWireLib block read command object.
	@discussion Represents an object that is configured and submitted to issue synchronous
		and asynchronous block read commands. This interface contains all methods of
		IOFireWireCommandInterface in addition to those described below */
typedef struct IOFireWireReadCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	void (*SetBuffer)(IOFireWireLibReadCommandRef self, UInt32 size, void* buf) ;
	void (*GetBuffer)(IOFireWireLibReadCommandRef self, UInt32* outSize, void** outBuf) ;
} IOFireWireReadCommandInterface ;

/*!	@class IOFireWireReadQuadletCommandInterface
	@abstract IOFireWireLib quadlet read command object.
	@discussion Represents an object that is configured and submitted to issue synchronous
		and asynchronous quadlet read commands. This interface contains all methods of
		IOFireWireCommandInterface in addition to those described below */

typedef struct IOFireWireReadQuadletCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	/*!	@function SetQuads
		@abstract Set destination for read data
		@param self The command object interface of interest
		@param inQuads An array of quadlets
		@param inNumQuads Number of quadlet in 'inQuads'	*/
	void (*SetQuads)(IOFireWireLibReadQuadletCommandRef self, UInt32 inQuads[], UInt32 inNumQuads) ;
} IOFireWireReadQuadletCommandInterface ;

typedef struct IOFireWireWriteCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	void (*SetBuffer)(IOFireWireLibWriteCommandRef self, UInt32 size, void* buf) ;
	void (*GetBuffer)(IOFireWireLibWriteCommandRef self, UInt32* outSize, const void** outBuf) ;
} IOFireWireWriteCommandInterface ;

typedef struct IOFireWireWriteQuadletCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	void (*SetQuads)(IOFireWireLibWriteQuadletCommandRef self, UInt32 inQuads[], UInt32 inNumQuads) ;
} IOFireWireWriteQuadletCommandInterface ;

typedef struct IOFireWireCompareSwapCommandInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOFIREWIRELIBCOMMAND_C_GUTS ;

	void	(*SetValues)(IOFireWireLibCompareSwapCommandRef self, UInt32 cmpVal, UInt32 newVal) ;
} IOFireWireCompareSwapCommandInterface ;

// ============================================================
//
// IOFireWireConfigDirectoryInterface
//
// ============================================================

typedef struct IOFireWireConfigDirectoryInterface_t
{
	IUNKNOWN_C_GUTS ;
	UInt32 version, revision ;
	
	IOReturn (*Update)							( IOFireWireLibConfigDirectoryRef self, UInt32 inOffset) ;
    IOReturn (*GetKeyType)						( IOFireWireLibConfigDirectoryRef self, int inKey, IOConfigKeyType* outType);
    IOReturn (*GetKeyValue_UInt32)				( IOFireWireLibConfigDirectoryRef self, int inKey, UInt32* outValue, CFStringRef* outText);
    IOReturn (*GetKeyValue_Data)				( IOFireWireLibConfigDirectoryRef self, int inKey, CFDataRef* outValue, CFStringRef* outText);
    IOReturn (*GetKeyValue_ConfigDirectory)		( IOFireWireLibConfigDirectoryRef self, int inKey, IOFireWireLibConfigDirectoryRef* outValue, REFIID iid, CFStringRef* outText);
    IOReturn (*GetKeyOffset_FWAddress)			( IOFireWireLibConfigDirectoryRef self, int inKey, FWAddress* outValue, CFStringRef* text);
    IOReturn (*GetIndexType)					( IOFireWireLibConfigDirectoryRef self, int inIndex, IOConfigKeyType* 	type);
    IOReturn (*GetIndexKey)						( IOFireWireLibConfigDirectoryRef self, int inIndex, int * key);
    IOReturn (*GetIndexValue_UInt32)			( IOFireWireLibConfigDirectoryRef self, int inIndex, UInt32 * value);
    IOReturn (*GetIndexValue_Data)				( IOFireWireLibConfigDirectoryRef self, int inIndex, CFDataRef * value);
    IOReturn (*GetIndexValue_String)			( IOFireWireLibConfigDirectoryRef self, int inIndex, CFStringRef* outValue);
    IOReturn (*GetIndexValue_ConfigDirectory)	( IOFireWireLibConfigDirectoryRef self, int inIndex, IOFireWireLibConfigDirectoryRef* outValue, REFIID iid);
    IOReturn (*GetIndexOffset_FWAddress)		( IOFireWireLibConfigDirectoryRef self, int inIndex, FWAddress* outValue);
    IOReturn (*GetIndexOffset_UInt32)			( IOFireWireLibConfigDirectoryRef self, int inIndex, UInt32* outValue);
    IOReturn (*GetIndexEntry)					( IOFireWireLibConfigDirectoryRef self, int inIndex, UInt32* outValue);
    IOReturn (*GetSubdirectories)				( IOFireWireLibConfigDirectoryRef self, io_iterator_t* outIterator);
    IOReturn (*GetKeySubdirectories)			( IOFireWireLibConfigDirectoryRef self, int inKey, io_iterator_t* outIterator);
	IOReturn (*GetType)							( IOFireWireLibConfigDirectoryRef self, int* outType) ;
	IOReturn (*GetNumEntries)					( IOFireWireLibConfigDirectoryRef self, int* outNumEntries) ;

} IOFireWireConfigDirectoryInterface ;

#endif //__IOFireWireLib_H__
