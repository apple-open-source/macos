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

#include "ip_firewire.h"


#define MAX_ALLOWED_SEGS	7

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
    struct mbuf*				fMBuf;
    struct mbuf*				fTailMbuf;
	UInt8*						fCursorBuf;
	UInt32						fOffset;
    bool						fUnfragmented;
	bool						fCopy;
	IOVirtualRange				fVirtualRange[MAX_ALLOWED_SEGS];
	UInt32						fIndex;
	UInt32						fLength;
	UInt32						fHeaderSize;
	UInt32						fLinkFragmentType;
	
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
	bool initAll(IOFireWireNub *device, UInt32 cmdLen,FWAddress devAddress,
             FWDeviceCallback completion, void *refcon, bool failOnReset);

	/*!
        @function reinit
		reinit will re-initialize all the variables for this command object, good
		when we have to reconfigure our outgoing command objects.
        @result true if successfull.
    */
    IOReturn reinit(IOFireWireNub *device, UInt32 cmdLen,
                FWAddress devAddress, FWDeviceCallback completion, void *refcon, bool failOnReset);  

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
	IOReturn initDescriptor(bool unfragmented, UInt32 length);

	/*!
		@function resetDescriptor
		@abstract resets the IOMemoryDescriptor & reinitializes the cursorbuf.
		@result void.
	*/
	void resetDescriptor();

	/*!
		@function getDescriptorHeader
		@abstract returns a descriptor header based on fragmentation and copying
				  of payload.
		@result void.
	*/
	void* getDescriptorHeader(bool unfragmented);

	/*!
		@function setOffset
		@abstract offset to traverse into the Mbuf.
		@result void.
	*/
	void setOffset(UInt32 offset, bool fFirst);

	/*!
		@function setLinkFragmentType
		@abstract sets the link fragment type.
		@result void.
	*/
	void setLinkFragmentType(UInt32 fType = UNFRAGMENTED);

	/*!
		@function getLinkFragmentType
		@abstract gets the link fragment type.
		@result void.
	*/
	UInt32 getLinkFragmentType();
	
	/*!
		@function setHeaderSize
		@abstract Header size to account for in the IOVirtual range and buffer descriptor.
		@result void.
	*/
	void setHeaderSize(UInt32 headerSize);

	/*!
		@function setMbuf
		@abstract sets the Mbuf to be used in the current command object.
		@result void.
	*/
	void setMbuf(struct mbuf * pkt, bool doCopy);
	
	/*!
		@function getMbuf
		@abstract returns the Mbuf from the current command object.
		@result void.
	*/
	struct mbuf* getMbuf();


	/*!
		@function setDeviceObject
		@abstract The Target device object is set, so we can 
				send a Asynchronous write to the device.
		@result void.
	*/
	void setDeviceObject(IOFireWireNub *device);

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
	struct mbuf*				fMBuf; 
    
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
  							IOFireWireController 	*control,
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

	void setMbuf(struct mbuf * pkt);
	
	struct mbuf *getMbuf();
	
private:
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 0);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 1);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 2);
    OSMetaClassDeclareReservedUnused(IOFWIPAsyncStreamTxCommand, 3);
};

#endif // _IOKIT_IOFIREWIREIPCOMMAND_H

