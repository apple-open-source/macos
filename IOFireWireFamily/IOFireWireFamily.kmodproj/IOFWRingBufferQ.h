/*
 * Copyright (c) 1998-2009 Apple Computer, Inc. All rights reserved.
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
 *  IOFWBufferQ.h
 *  IOFireWireFamily
 *
 *  Created by calderon on 9/17/09.
 *  Copyright 2009 Apple. All rights reserved.
 *
 */

#ifndef __IOFWRingBufferQ_H__
#define __IOFWRingBufferQ_H__

// public
#import <IOKit/IOMemoryDescriptor.h>

//using namespace IOFireWireLib;

// IOFWRingBufferQ
// Description: A ring buffered FIFO queue
	
class IOFWRingBufferQ: public OSObject
{
	OSDeclareDefaultStructors(IOFWRingBufferQ);
	
public:
	
	static IOFWRingBufferQ *	withAddressRange( mach_vm_address_t address, mach_vm_size_t length, IOOptionBits options, task_t task );
	
	virtual bool			initQ( mach_vm_address_t address, mach_vm_size_t length, IOOptionBits options, task_t task );
	virtual void			free( void );	
	virtual bool			isEmpty( void );
	virtual bool			dequeueBytes( IOByteCount size );
	virtual bool			dequeueBytesWithCopy( void * copy, IOByteCount size );
	virtual IOByteCount		readBytes(IOByteCount offset, void * bytes, IOByteCount withLength);
	virtual bool			enqueueBytes( void * bytes, IOByteCount size );
	virtual bool			isSpaceAvailable( IOByteCount size, IOByteCount * offset );
	virtual bool			front( void * copy, IOByteCount size, IOByteCount * paddingBytes );
	virtual IOByteCount		spaceAvailable( void );
	virtual bool			willFitAtEnd( IOByteCount sizeOfEntry, IOByteCount * offset, IOByteCount * paddingBytes );
	virtual IOByteCount		frontEntryOffset( IOByteCount sizeOfEntry, IOByteCount * paddingBytes );
	
private:
	IOMemoryDescriptor *			fMemDescriptor;
	bool							fMemDescriptorPrepared;
	IOByteCount						fBufferSize;
	IOByteCount						fFrontOffset;
	IOByteCount						fQueueLength;
} ;

#endif //__IOFWRingBufferQ_H__
