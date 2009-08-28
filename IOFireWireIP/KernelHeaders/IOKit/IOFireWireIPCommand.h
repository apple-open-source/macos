/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOKIT_IOFIREWIREIPCOMMAND_H
#define _IOKIT_IOFIREWIREIPCOMMAND_H

extern "C"{
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
}
 
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "IOFWIPDefinitions.h"
#include "IOFireWireIP.h"

#define MAX_ALLOWED_SEGS	7

class IOFireWireIP;
class IOFWIPMBufCommand;
class IOFWIPBusInterface;

/*! @class IOFWIPAsyncWriteCommand
*/
class IOFWIPAsyncWriteCommand : public IOFWWriteCommand
{
    OSDeclareDefaultStructors(IOFWIPAsyncWriteCommand)
	
		
protected:
    IOBufferMemoryDescriptor	*fBuffer;
	IOMemoryDescriptor			*fMem;
    const UInt8					*fCommand;
    // Maximum length for the pre allocated buffer, can be changed dynamically
    UInt32						maxBufLen;
	IOFWIPMBufCommand			*fMBufCommand;
    mbuf_t						fTailMbuf;
	UInt8*						fCursorBuf;
	UInt32						fOffset;
	bool						fCopy;
	IOAddressRange				fVirtualRange[MAX_ALLOWED_SEGS];
	UInt32						fIndex;
	UInt32						fLength;
	UInt32						fHeaderSize;
	FragmentType				fLinkFragmentType;
    IOFireWireIP				*fIPLocalNode;
	IOFWIPBusInterface			*fIPBusIf;
	UInt32						reInitCount;
	UInt32						resetCount;
	
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;
    
    virtual void		free();
    
public:

	/*!
        @function initAll
		Initializes the Asynchronous write command object
        @result true if successfull.
    */
	bool initAll(IOFireWireIP *networkObject, IOFWIPBusInterface *fwIPBusIfObject,
					UInt32 cmdLen,FWAddress devAddress, FWDeviceCallback completion, void *refcon, bool failOnReset);

	/*!
        @function reinit
		reinit will re-initialize all the variables for this command object, good
		when we have to reconfigure our outgoing command objects.
        @result true if successfull.
    */
    IOReturn reinit(IOFireWireNub *device, UInt32 cmdLen, FWAddress devAddress, 
					FWDeviceCallback completion, void *refcon, bool failOnReset, 
					bool deferNotify);  

	IOReturn transmit(IOFireWireNub *device, UInt32 cmdLen, FWAddress devAddress,
					  FWDeviceCallback completion, void *refcon, bool failOnReset, 
					  bool deferNotify, bool doQueue, FragmentType fragmentType);

	IOReturn transmit(IOFireWireNub *device, UInt32 cmdLen, FWAddress devAddress,
					  FWDeviceCallback completion, void *refcon, bool failOnReset, 
					  bool deferNotify, bool doQueue);

	void wait();
	
	bool notDoubleComplete();

	void gotAck(int ackCode);
	
	/*!
		@function createFragmentedDescriptors
		@abstract creates IOVirtual ranges for fragmented Mbuf packets.
		@param length - length to copy.
		@result 0 if copied successfully else non-negative value
	*/
	IOReturn createFragmentedDescriptors();

	/*!
		@function createUnFragmentedDescriptors
		@abstract creates IOVirtual ranges for fragmented Mbuf packets.
		@param none.
		@result kIOReturnSuccess if successfull, else kIOReturnError.
	*/
	IOReturn createUnFragmentedDescriptors();
	
	/*!
		@function copyToBufferDescriptors
		@abstract copies mbuf data into the buffer pointed by IOMemoryDescriptor.
		@param none.
		@result 0 if copied successfully else non-negative value
	*/
	IOReturn copyToBufferDescriptors();
	
	/*!
		@function initDescriptor
		@abstract copies mbuf data into the buffer pointed by IOMemoryDescriptor.
		@param unfragmented - indicates whether the packet is fragmented or unfragmented.
		@param length - length to copy.
		@result kIOReturnSuccess, if successfull.
	*/
	IOReturn initDescriptor(UInt32 length);

	/*!
		@function resetDescriptor
		@abstract resets the IOMemoryDescriptor & reinitializes the cursorbuf.
		@result void.
	*/
	void resetDescriptor(IOReturn status);

	/*!
		@function initPacketHeader
		@abstract returns a descriptor header based on fragmentation and copying
				  of payload.
		@result void.
	*/
	void* initPacketHeader(IOFWIPMBufCommand *mBufCommand, bool doCopy, FragmentType unfragmented, UInt32 headerSize, UInt32 offset);

	/*!
		@function getCursorBuf
		@abstract returns the pointer from the current position of fBuffer.
		@result void* - pre-allocated buffer pointer
	*/
	void* getCursorBuf();

	/*!
		@function getBufferFromDescriptor
		@abstract returns the head pointer position of fBuffer. 
		@result void* - pre-allocated buffer pointer
	*/
	void* getBufferFromDescriptor();

	/*!
		@function getMaxBufLen
		@abstract Usefull when MTU changes to a greater value and we need to
				accomodate more data in the buffer without a 1394 fragmentation
		@result UInt32 - size of the pre-allocated buffer
	*/
	UInt32 getMaxBufLen();
	
private:
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncWriteCommand, 0);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncWriteCommand, 1);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncWriteCommand, 2);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncWriteCommand, 3);
};


/*! @class IOFWIPAsyncStreamTxCommand
*/
class IOFWIPAsyncStreamTxCommand : public IOFWAsyncStreamCommand
{
    OSDeclareDefaultStructors(IOFWIPAsyncStreamTxCommand)
	
		
protected:
    IOBufferMemoryDescriptor	*fBuffer;
    IOMemoryDescriptor			*fMem;
    const UInt8					*fCommand;
    // Maximum length for the pre allocated buffer, can be changed dynamically
    UInt32						maxBufLen;
    IOFireWireIP				*fIPLocalNode;
	IOFWIPBusInterface			*fIPBusIf;
    
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;
    
    virtual void		free();
    
public:
	/*!
        @function initAll
		Initializes the Asynchronous transmit command object
        @result true if successfull.
    */
    virtual bool	initAll(
							IOFireWireIP			*networkObject,
  							IOFireWireController 	*control,
							IOFWIPBusInterface		*fwIPBusIfObject,
                            UInt32 					generation, 
                            UInt32 					channel,
                            UInt32 					sync,
                            UInt32 					tag,
							UInt32					cmdLen,
                            int						speed,
                            FWAsyncStreamCallback	completion,
                            void 					*refcon);
	/*!
        @function reinit
		reinit will re-initialize all the variables for this command object, good
		when we have to reconfigure our outgoing command objects.
        @result true if successfull.
    */
    virtual IOReturn	reinit(	UInt32 					generation, 
                                UInt32 					channel,
                                UInt32					cmdLen,
                                int						speed,
                               	FWAsyncStreamCallback	completion,
                                void 					*refcon);

	void wait();
	
	/*!
        @function getBufferFromDesc
		Usefull for copying data from the mbuf
		@result void* - pre-allocated buffer pointer
    */
	void* getBufferFromDesc();    
    

	/*!
        @function getMaxBufLen
		Usefull when MTU changes to a greater value and we need to
		accomodate more data in the buffer without a 1394 fragmentation
		@result UInt32 - size of the pre-allocated buffer
    */
    UInt32 getMaxBufLen();

private:
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 0);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 1);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 2);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 3);
};

#endif // _IOKIT_IOFIREWIREIPCOMMAND_H

