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
 *	File: $Id: IOI2C.h,v 1.5 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2C.h,v $
 *		Revision 1.5  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.4  2004/12/15 00:29:45  jlehrer
 *		Added options to extended read/write.
 *		Removed type from openI2CDevice.
 *		
 *		Revision 1.3  2004/09/17 20:25:22  jlehrer
 *		Removed ASPL headers.
 *		
 *		Revision 1.2  2004/06/08 23:40:25  jlehrer
 *		Added getSMUI2CInterface
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOI2C/IOI2CDefs.h>

#ifndef IOI2C_H_
#define IOI2C_H_

/*! @typedef I2CInterfaceRef
	@abstract Data type to represent an IOService connection to a PPCI2CInterface.
	@discussion Use the I2CInterfaceRef with PPCI2CInterface API functions.
*/
typedef io_connect_t I2CInterfaceRef;

/*! @typedef I2CDeviceRef
	@abstract Data type to represent an IOService connection to an IOI2CDevice or IOI2CController.
	@discussion Use the I2CDeviceRef with IOI2CFamily API functions.
*/
typedef struct
{
	io_connect_t	_i2c_connect;
	UInt32			_i2c_key;

} I2CDeviceRef;

#pragma mark ***
#pragma mark *** IOI2CFamily API
#pragma mark ***

/*!	@function findI2CDevices
	@abstract Returns an array of available I2C Devices
	@discussion This function will probe the I/O Registry and find any available I2C Devices.
	@result Returns an array of dictionaries. Each dictionary will contain properties of an IOI2CDevice derived driver found in the IOService plane. The dictionary will contain a "path" key/value containing the IOService path of the device. If no IOI2CDevices are found or an error occurs, a NULL reference will be returned. The array must be released by the caller.
*/
	CFArrayRef findI2CDevices(void);
	CFArrayRef findI2CControllers(void);

/*!	@function openI2CDevice
	@abstract Opens a user client for the specified I2C Device.
	@param device The address of a client allocated I2CDeviceRef.
	@param path The NSCFString containing the IOService path of the I2C device.
	@result If successful returns kIOReturnSuccess and opens a user client with the specified device. The caller must call closeI2CDevice when finished.
*/
	IOReturn openI2CDevice(I2CDeviceRef *device, CFStringRef path);

/*!	@function closeI2CDevice
	@abstract Closes a user client for the specified I2C Device.
	@param device The address of an opened I2CDeviceRef.
	@result If successful returns kIOReturnSuccess and closes the user client with the specified device.
*/
	IOReturn closeI2CDevice(I2CDeviceRef *device);

/*!	@function lockI2CDevice
	@abstract Locks the I2C bus of the specified I2C Device for subsequent mutually exclusive access.
	@discussion Locking is necessary only when exclusive access to a device is required. The client must unlock the I2C bus as quickly as possible. All other client threads, including kernel threads, are blocked while the I2C bus is locked. Note: A device can be read and writen to without locking its I2C bus.
	@param device The address of an opened I2CDeviceRef.
	@result If successful returns kIOReturnSuccess and locks the I2C bus of the specified device.
*/
	IOReturn lockI2CDevice(I2CDeviceRef	*device);

/*!	@function unlockI2CDevice
	@abstract Unlocks the I2C bus of the specified I2C Device and releases mutually exclusive access.
	@param device The address of an opened and locked I2CDeviceRef.
	@result If successful returns kIOReturnSuccess and unlocks the I2C bus of the specified device.
*/
	IOReturn unlockI2CDevice(I2CDeviceRef *device);

/*!	@function readI2CDevice
	@abstract Performs a read transaction with the specified I2C Device.
	@param device The address of an opened I2CDeviceRef.
	@param subAddress The sub-address register to access.
	@param readBuf The client provided UInt8 array containing the result of the read transaction.
	@param count The number of bytes to read and the size of the clients readBuf array.
	@result If successful returns kIOReturnSuccess and performs the read transaction with the specified device.
*/
	IOReturn readI2CDevice(I2CDeviceRef *device, UInt32 subAddress, UInt8 *readBuf, UInt32 count);

/*!	@function writeI2CDevice
	@abstract Performs a write transaction with the specified I2C Device.
	@param device The address of an opened I2CDeviceRef.
	@param subAddress The sub-address register to access.
	@param writeBuf The client provided UInt8 array containing the data to write.
	@param count The number of bytes to write and the size of the clients writeBuf array.
	@result If successful returns kIOReturnSuccess and performs the write transaction with the specified device.
*/
	IOReturn writeI2CDevice(I2CDeviceRef *device, UInt32 subAddress, UInt8 *writeBuf, UInt32 count);



/*!	@function lockI2CExtended
	@abstract Locks the I2C bus of the specified IOI2CController for subsequent mutually exclusive access.
	@discussion The bus must be locked for a minimal duration or the system may lock up. The I2CDeviceRef must be unlocked by calling the unlockI2CDevice function.
	@param device The address of an opened I2CDeviceRef.
	@param bus Specifies the I2C bus to lock.
	@result If successful returns kIOReturnSuccess and locks the I2C bus of the specified device.
*/

	IOReturn lockI2CExtended(
		I2CDeviceRef	*device,
		UInt32			bus);

/*!	@function writeI2CExtended
	@abstract Performs a write transaction with the specified IOI2CDevice or IOI2CController.
	@discussion This function allows specifying extended parameters for I2C write transactions. Typically this is useful for executing a write transaction with a mode other than the default (subaddress mode).
	The extended write also allows requesting transactions directly through an IOI2CController. This is handy for debugging new hardware not specified in the device-tree (or incorrectly described). All parameters are specified by the caller. However, transactions requested from an IOI2CDevice instance will be directed only to that device and the bus and address params will be ignored.
	@param device The address of an opened I2CDeviceRef.
	@param bus Specifies the bus number. Used only for IOI2CController user clients.
	@param address Specifies the device address. Used only for IOI2CController user clients.
	@param subAddress The sub-address register to access.
	@param writeBuf The client provided UInt8 array containing the data for the write transaction.
	@param count The number of bytes to write and the size of the clients writeBuf array.
	@result If successful returns kIOReturnSuccess and performs the write transaction with the specified device.
*/

	IOReturn writeI2CExtended(
		I2CDeviceRef	*device,
		UInt32			bus,
		UInt32			address,
		UInt32			subAddress,
		UInt8			*writeBuf,
		UInt32			count,
		UInt32			mode,
		UInt32			options);

/*!	@function readI2CExtended
	@abstract Performs a read transaction with the specified IOI2CDevice or IOI2CController.
	@discussion This function allows specifying extended parameters for I2C read transactions. Typically this is useful for executing a read transaction with a mode other than the default (combined mode).
	The extended read also allows requesting transactions directly through an IOI2CController. This is handy for debugging new hardware not specified in the device-tree (or incorrectly described). All parameters are specified by the caller. However, transactions requested from an IOI2CDevice instance will be directed only to that device and the bus and address params will be ignored.
	@param device The address of an opened I2CDeviceRef.
	@param bus Specifies the bus number. Used only for IOI2CController user clients.
	@param address Specifies the device address. Used only for IOI2CController user clients.
	@param subAddress The sub-address register to access.
	@param readBuf The client provided UInt8 array containing the result of the read transaction.
	@param count The number of bytes to read and the size of the clients readBuf array.
	@result If successful returns kIOReturnSuccess and performs the read transaction with the specified device.
*/

	IOReturn readI2CExtended(
		I2CDeviceRef	*device,
		UInt32			bus,
		UInt32			address,
		UInt32			subAddress,
		UInt8			*readBuf,
		UInt32			count,
		UInt32			mode,
		UInt32			options);


#pragma mark ***
#pragma mark *** PPCI2CInterface API
#pragma mark ***

CFArrayRef findPPCI2CInterfaces(void);

IOReturn readI2CInterface(
	I2CInterfaceRef iface,
	UInt32			bus,
	UInt32			address,
	UInt32			subAddress,
	UInt8			*buffer,
	UInt32			count,
	UInt32			mode);

IOReturn writeI2CInterface(
	I2CInterfaceRef iface,
	UInt32			bus,
	UInt32			address,
	UInt32			subAddress,
	UInt8			*buffer,
	UInt32			count,
	UInt32			mode);



/*!
	@function findI2CInterfaces
	@abstract Returns a list of available I2C Interfaces
	@discussion This function will probe the I/O Registry and find any available I2C interfaces.
	@result Any I2C interfaces found will be identified by their respective paths in the I/O Registry service plane.  This/these path(s) will be encapsulated as CFStrings, and placed in a CFArray.  On success, this CFArray is returned.  The caller must release the array when finished.  If no interfaces are found or an error occurs, a NULL reference will be returned.
*/

	CFArrayRef findI2CInterfaces(void);

/*!
	@function getMacIOI2CInterface
	@abstract Returns the MacIO I2C interface, if present
	@discussion This function will probe the I/O Registry and find the MacIO I2C Interface, if present on the current platform
	@result If a MacIO I2C interface is found, its I/O Registry path in the service plane will be returned as a CFString.  The caller must release the string.  If no MacIO I2C interface is found, or an error occurs, a NULL reference will be returned.
*/

	CFStringRef getMacIOI2CInterface(void);

/*!
	@function getUniNI2CInterface
	@abstract Returns the UniNorth I2C interface, if present
	@discussion This function will probe the I/O Registry and find the UniNorth I2C Interface, if present on the current platform
	@result If a UniNorth I2C interface is found, its I/O Registry path in the service plane will be returned as a CFString.  The caller must release the string.  If no UniNorth I2C interface is found, or an error occurs, a NULL reference will be returned.
*/

	CFStringRef getUniNI2CInterface(void);

/*!
	@function getPMUI2CInterface
	@abstract Returns the PMU I2C interface, if present
	@discussion This function will probe the I/O Registry and find the PMU I2C Interface, if present on the current platform
	@result If a PMU I2C interface is found, its I/O Registry path in the service plane will be returned as a CFString.  The caller must release the string.  If no PMU I2C interface is found, or an error occurs, a NULL reference will be returned.
*/

	CFStringRef getPMUI2CInterface(void);

/*!
	@function getSMUI2CInterface
	@abstract Returns the SMU I2C interface, if present
	@discussion This function will probe the I/O Registry and find the SMU I2C Interface, if present on the current platform
	@result If a SMU I2C interface is found, its I/O Registry path in the service plane will be returned as a CFString.  The caller must release the string.  If no PMU I2C interface is found, or an error occurs, a NULL reference will be returned.
*/

	CFStringRef getSMUI2CInterface(void);

/*!
	@function openI2CInterface
	@abstract Opens a user client connection to the desired I2C interface
	@discussion Does the necessary work to instantiate and open an I2C user client connection.  The I2CInterfaceRef obtained from this function can be used to perform read/write operations on the bus(es) it controls.  Calling this function does not actually open or hold any I2C buses, just creates a user client connection to the driver controlling the interface.  This function will fail if the calling task is not executing with EUID equal to 0 (root).
	@param interface The I/O Registry service plane path to the PPCI2CInterface object that controls the desired bus.  This should be obtained from one of the search functions included in this framework.
	@param iface A pointer to an I2CInterfaceRef in which, on success, this function will store the user client connection handle for use with read/write functions.
	@result On success, iface will hold a valid user client connection handle and kIOReturnSuccess will be returned.  If an error occurs, an appropriate error code will be returned.
*/

	IOReturn openI2CInterface(CFStringRef interface, I2CInterfaceRef *iface);

/*!
	@function closeI2CInterface
	@abstract Closes a user client connection to the desired I2C interface
	@discussion Does the necessary work to close and terminate the specified I2C user client connection
	@param iface An I2CInterfaceRef that refers to the I2C interface to be closed
	@result On success, kIOReturnSuccess will be returned.  If an error occurs, and appropriate error code will be returned.
*/

	IOReturn closeI2CInterface(I2CInterfaceRef iface);

/*!
	@function readI2CBus
	@abstract Perform an atomic read operation on the specified I2C interface
	@discussion	This function takes a valid interface reference and input/output parameter blocks and calls into the kernel to perform a completely atomic I2C read cycle.  This function does not return to the caller until the I2C bus has been released.
	@param iface A valid I2CInterfaceRef obtained from openI2CInterface()
	@param inputs A pointer to a populated I2CReadInput structure, which specifies all the transaction parameters (see I2CUserClient.h)
	@param outputs A pointer to a caller-allocated I2CReadOutput structure where the results of the read transaction will be placed
	@result On success, returns kIOReturnSuccess and populates outputs with the data from the read (in outputs->buf) and the number of bytes actually read (inputs->realCount).  On failure, an appropriate error code is returned and the contents of outputs is undefined
*/

	IOReturn readI2CBus(I2CInterfaceRef iface, I2CReadInput *inputs,
			I2CReadOutput *outputs);

/*!
	@function writeI2CBus
	@abstract Perform an atomic write operation on the specified I2C interface
	@discussion This function takes a valid interface reference and input/output parameter blocks and calls into the kernel to perform a completely atomic I2C write cycle.  This function does not return to the caller until the I2C bus has been released.
	@param iface A valid I2CInterfaceRef obtained from openI2CInterface()
	@param inputs A pointer to a populated I2CWriteInput structure, which specifies all the transaction parameters (see I2CUserClient.h)
	@param outputs A pointer to a caller-allocated I2CWriteOutput structure where the results of the write transaction will be placed
	@result On success, returns kIOReturnSuccess and populates outputs with the number of bytes actually written.  If an error occurs, an appropriate error code will be returned and the contents of outputs will be undefined
*/

	IOReturn writeI2CBus(I2CInterfaceRef iface, I2CWriteInput *inputs,
			I2CWriteOutput *outputs);

/*!
	@function rmwI2CBus
	@abstract Perform an atomic single byte read-modify-write operation on the specified I2C interface
	@discussion This function takes a valid interface reference and input parameter block and calls into the kernel to perform a completely atomic single byte I2C read-modify-write cycle.  First, the bus is opened and a read is performed using the specified transaction mode.  The mask and value are applied to the value that was just read.  The resulting byte is written back to I2C using the specified write transaction mode.  Then the bus is released and the function returns to the caller.
	@param iface A valid I2CInterfaceRef obtained from openI2CInterface()
	@param inputs A pointer to a populated I2CRMWInput structure, which specifies all the transaction parameters (see I2CUserClient.h)
	@result On success, returns kIOReturnSuccess.  If an error occurs, an appropriate error code is returned
*/

	IOReturn rmwI2CBus(I2CInterfaceRef iface, I2CRMWInput *inputs);


#endif // IOI2C_H_
