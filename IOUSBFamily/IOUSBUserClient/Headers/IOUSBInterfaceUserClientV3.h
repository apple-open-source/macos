/*
 * Copyright © 2012 Apple Inc. All rights reserved.
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

#ifndef _IOKIT_IOUSBINTERFACEUSERCLIENTV3_H
#define _IOKIT_IOUSBINTERFACEUSERCLIENTV3_H

//================================================================================================
//
//   Headers
//
//================================================================================================
#include "IOUSBInterfaceUserClient.h"

//================================================================================================
//
//   Class Definition for IOUSBInterfaceUserClientV3
//
//================================================================================================
//
/*!
 @class IOUSBInterfaceUserClientV3
 @abstract Connection to the IOUSBInterface objects from user space.
 @discussion This class can be overriden to provide for specific behaviors.
 */
class IOUSBInterfaceUserClientV3 : public IOUSBInterfaceUserClientV2
{
    OSDeclareDefaultStructors(IOUSBInterfaceUserClientV3)

private:
    struct IOUSBInterfaceUserClientV3ExpansionData
    {
    };
    IOUSBInterfaceUserClientV3ExpansionData *		fIOUSBInterfaceUserV3ClientExpansionData;
	
protected:
	
	static const IOExternalMethodDispatch		sV3Methods[kIOUSBLibInterfaceUserClientV3NumCommands];
		
public:
	
 	// IOUserClient methods
    //
	virtual IOReturn							externalMethod(	uint32_t selector, IOExternalMethodArguments * arguments, IOExternalMethodDispatch * dispatch, OSObject * target, void * reference);

	// IOUSBUserClientV2 methods
	virtual IOReturn							ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion);
	virtual IOReturn                            ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size);
	virtual IOReturn                            ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, IOByteCount *bytesRead);
	
	virtual IOReturn							WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion);
	virtual IOReturn                            WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, const void *buf, UInt32 size);
	virtual IOReturn                            WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem);
	
	// Streams
    static	IOReturn							_supportsStreams(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            SupportsStreams(UInt8 pipeRef, uint64_t *supportsStreams);
	
    static	IOReturn							_createStreams(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            CreateStreams(UInt8 pipeRef, UInt32 maxStreams);
	
    static	IOReturn							_getConfiguredStreams(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            GetConfiguredStreams(UInt8 pipeRef, uint64_t *configuredStreams);
	
	static	IOReturn							_ReadStreamsPipe(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn							ReadStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion);
	virtual IOReturn                            ReadStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, void *buffer, UInt32 *size);
	virtual IOReturn                            ReadStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *iomd, IOByteCount *bytesRead);

	static	IOReturn							_WriteStreamsPipe(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn							WriteStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion);
	virtual IOReturn                            WriteStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, const void *buffer, UInt32 size);
	virtual IOReturn                            WriteStreamsPipe(UInt8 pipeRef, UInt32 streamID, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *iomd);

	static	IOReturn							_AbortStreamsPipe(IOUSBInterfaceUserClientV3 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            AbortStreamsPipe(UInt8 pipeRef, UInt32 streamID);

	// padding methods
    //
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 0);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 1);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 2);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 3);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 4);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 5);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 6);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 7);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 8);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 9);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 10);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 11);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 12);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 13);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 14);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 15);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 16);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 17);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 18);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 19);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 20);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 21);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 22);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 23);
	OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV3, 24);
};

#endif