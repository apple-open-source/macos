/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_IOFIREWIREAVCLIB_H_
#define _IOKIT_IOFIREWIREAVCLIB_H_

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>

// Unit type UUID
/* DAAAEB01-8889-11D6-93B8-00039347A812 */
#define kIOFireWireIPLibUnitTypeID CFUUIDGetConstantUUIDWithBytes(NULL,		\
0xDA, 0xAA, 0xEB, 0x01, 0x88, 0x89, 0x11, 0xD6, 0x93, 0xB8, 0x00, 0x03, 0x93, 0x47, 0xA8, 0x12)

// Unit Factory UUID
/* 37EABD89-889E-11D6-8A75-00039347A812 */
#define kIOFireWireIPLibUnitFactoryID CFUUIDGetConstantUUIDWithBytes(NULL, 	\
0x37, 0xEA, 0xBD, 0x89, 0x88, 0x9E, 0x11, 0xD6, 0x8A, 0x75, 0x00, 0x03, 0x93, 0x47, 0xA8, 0x12)

// IOFireWireIPUnitInterface UUID
/* 3ED331BB-889E-11D6-B190-00039347A812 */
#define kIOFireWireIPLibUnitInterfaceID CFUUIDGetConstantUUIDWithBytes(NULL, 	\
0x3E, 0xD3, 0x31, 0xBB, 0x88, 0x9E, 0x11, 0xD6, 0xB1, 0x90, 0x00, 0x03, 0x93, 0x47, 0xA8, 0x12)


typedef void (*IOFWIPMessageCallback)( void * refCon, UInt32 type, void * arg );

/*! @typedef IOFWIPRequestCallback
	@abstract Callback called to handle IP commands sent to an IP subunit inside the Mac.
	@param refCon user specified reference number passed in when requests were enabled.
    @param generation Bus generation command was received in
	@param srcNodeID Node ID of the sender
	@param command Pointer to the received data
	@param cmdLen Length in bytes of incoming command
	@param response Pointer to buffer to store response which will be sent back to the requesting device
	@param responseLen On entry, *responseLen is max size of response buffer. On exit, set to number of bytes
    to send back.
    @result return kIOReturnSuccess if the command was handled and response now contains the IP response to be
    sent back to the device.
*/
typedef IOReturn (*IOFWIPRequestCallback)( void *refCon, UInt32 generation, UInt16 srcNodeID,
                const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen);

/*!
    @class IOFireWireIPLibUnitInterface
    @abstract Initial interface discovered for all IP Unit drivers. 
    @discussion The IOFireWireIPLibUnitInterface is the initial interface discovered by most drivers. It supplies the methods that control the operation of the IP unit as a whole.
    Finally the Unit can supply a reference to the IOFireWireUnit.  This can be useful if a driver wishes to access the standard FireWire APIs.  
*/

typedef struct
 {
	IUNKNOWN_C_GUTS;

	UInt16	version;						
    UInt16	revision;

    /*!
		@function open
		@abstract Exclusively opens a connection to the in-kernel device.
		@discussion Exclusively opens a connection to the in-kernel device.  As long as the in-kernel 
        device object is open, no other drivers will be able to open a connection to the device. When 
        open the device on the bus may disappear, but the in-kernel object representing it will stay
        instantiated and can begin communicating with the device again if it ever reappears. 
        @param self Pointer to IOFireWireIPLibUnitInterface.
        @result Returns kIOReturnSuccess on success.
    */
    
	IOReturn (*open)( void * self );
    
    /*!
		@function openWithSessionRef
		@abstract Opens a connection to a device that is not already open.
		@discussion Sometimes it is desirable to open multiple user clients on a device.  In the case 
        of FireWire sometimes we wish to have both the FireWire User Client and the IP User Client 
        open at the same time.  The technique to arbitrate this is as follows.  First open normally 
        the device furthest from the root in the IORegistry.  Second, get its sessionRef with the 
        getSessionRef call.  Third open the device further up the chain by calling this method and 
        passing the sessionRef returned from the call in step 2.
        @param sessionRef SessionRef returned from getSessionRef call. 
        @param self Pointer to IOFireWireIPLibUnitInterface.
        @result Returns kIOReturnSuccess on success.
    */
    
	IOReturn (*openWithSessionRef)( void * self, IOFireWireSessionRef sessionRef );

    /*!
		@function openWithSessionRef
		@abstract Opens a connection to a device that is not already open.
		@discussion Sometimes it is desirable to open multiple user clients on a device.  In the case 
        of FireWire sometimes we wish to have both the FireWire User Client and the IP User Client 
        open at the same time.  The technique to arbitrate this is as follows.  First open normally 
        the device furthest from the root in the IORegistry.  Second, get its sessionRef with the 
        with a call to this method.  Third open the device further up the chain by calling 
        openWithSessionRef and passing the sessionRef returned from this call.
        @param self Pointer to IOFireWireIPLibUnitInterface.
        @result Returns a sessionRef on success.
    */

	IOFireWireSessionRef (*getSessionRef)(void * self);

    /*!
		@function close
		@abstract Opens a connection to a device that is not already open.
		@discussion Closes an exclusive access to the device.  When a device is closed it may be 
        unloaded by the kernel.  If it is unloaded and then later reappears it will be represented 
        by a different object.  You won't be able to use this user client on the new object.  The 
        new object will have to be looked up in the IORegistry and a new user client will have to 
        be opened on it. 
        @param self Pointer to IOFireWireIPLibUnitInterface.
    */

	void (*close)( void * self );

    /*!
		@function setMessageCallback
		@abstract Set callback for user space message routine.
		@discussion In FireWire & IP bus status messages are delivered via IOKit's message routine.  
        This routine is emulated in user space for IP & FireWire messages via this callback.  You should
        register here for bus reset, and reconnect messages.
        @param self Pointer to IOFireWireIPLibUnitInterface.
        @param refCon RefCon to be returned as first argument of completion routine
        @param callback Address of completion routine.
    */
    
	void (*setMessageCallback)( void *self, void * refCon, IOFWIPMessageCallback callback);
    
    /*!
		@function IPCommand
		@abstract Sends an IP command to the device and returns the response.
		@discussion This function will block until the device returns a response or the kernel driver times out. 
        @param self Pointer to IOFireWireIPLibUnitInterface.
        @param command Pointer to command to send
        @param cmdLen Length (in bytes) of command
        @param response Pointer to place to store the response sent by the device
        @param responseLen Pointer to place to store the length of the response
    */

	IOReturn (*IPCommand)( void * self,
        const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen);

    /*!
		@function showLcb
		@abstract Prints the link control block of the IOFireWireLocalNode.
		@discussion This function will block until the device returns a response or the kernel driver times out. 
        @param self Pointer to IOFireWireIPLibUnitInterface.
        @param command Pointer to command to send
        @param cmdLen Length (in bytes) of command
        @param response Pointer to place to store the response sent by the device
        @param responseLen Pointer to place to store the length of the response
    */

	IOReturn (*showLcb)( void * self,
        const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen);

} IOFireWireIPLibUnitInterface;

#endif
