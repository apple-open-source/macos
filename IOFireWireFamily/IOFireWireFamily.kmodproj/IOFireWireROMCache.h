/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#ifndef __IOFIREWIREROMCACHE_H__
#define __IOFIREWIREROMCACHE_H__

#include <libkern/c++/OSObject.h>
#include <IOKit/system.h>

#include <libkern/c++/OSData.h>
#include <IOKit/IOLocks.h>

class IOFireWireDevice;

/*!
    @class IOFireWireROMCache
    @abstract A class to hold a cache of a device's config ROM.
*/

class IOFireWireROMCache : public OSObject
{
    OSDeclareDefaultStructors(IOFireWireROMCache)

public:

	enum ROMState
	{
		kROMStateSuspended,
		kROMStateResumed,
		kROMStateInvalid
	};
	
protected:
 
	IOFireWireDevice * 	fOwner;
	IORecursiveLock *	fLock;
    
	OSData *			fROM;
	ROMState			fState;
	UInt32				fGeneration;
	
	struct ExpansionData { };
    
    /*! @var reserved
        Reserved for future use.  (Internal use only)  
	*/
    
	ExpansionData *reserved;

public:
	
	/*!
        @function initWithOwnerAndBytes
        @abstract A member function to initialize an instance of IOFireWireROMCache which references a block of data.
		@param owner A reference to the owner of this ROM
        @param bytes A reference to a block of data
        @param inLength The length of the block of data.
		@param generation The bus generation
        @result Returns true if initialization was successful, false otherwise.
    */
    
	virtual bool initWithOwnerAndBytes( IOFireWireDevice * owner, const void *bytes, unsigned int inLength, UInt32 generation );


	/*!
        @function withOwnerAndBytes
        @abstract A static constructor function to create and initialize an instance of IOFireWireROMCache and copies in the provided data.
		@param owner A reference to the owner of this ROM
        @param bytes A buffer of data.
        @param inLength The size of the given buffer.
		@param generation The bus generation
        @result Returns an instance of IOFireWireROMCache or 0 if a failure occurs.
    */
    
	static IOFireWireROMCache *withOwnerAndBytes( IOFireWireDevice * owner, const void *bytes, unsigned int inLength, UInt32 generation );

    /*!
        @function free
        @abstract A member function which releases all resources created or used by the IOFireWireROMCache object.
        @discussion Do not call this function directly, use release() instead.
    */
    
	virtual void free(void) APPLE_KEXT_OVERRIDE;

    /*!
        @function getLength
        @abstract A member function which returns the length of the internal data buffer.
        @result Returns an integer value for the length of data in the object's internal data buffer.
    */
    
	virtual unsigned int getLength();

    /*!
        @function ensureCapacity
        @abstract A member function which will expand the size of the collection to a given storage capacity.
        @param newCapacity The new capacity for the data buffer.
        @result Returns the new capacity of the data buffer or the previous capacity upon error.
    */
    
	virtual unsigned int ensureCapacity(unsigned int newCapacity);
    
	/*!
        @function appendBytes
        @abstract A member function which appends a buffer of data onto the end of the object's internal data buffer.
        @param bytes A pointer to the block of data.
        @param inLength The length of the data block.
        @result Returns true if the object was able to append the new data, false otherwise.
    */
    
	virtual bool appendBytes(const void *bytes, unsigned int inLength);
    
	/*!
        @function appendBytes
        @abstract A member function which appends the data contained in an IOFireWireROMCache object to the receiver.
        @param other An IOFireWireROMCache object.
        @result Returns true if appending the new data was successful, false otherwise.
    */
    
	virtual bool appendBytes(const OSData *other);

    /*!
        @function getBytesNoCopy
        @abstract A member function to return a pointer to the IOFireWireROMCache object's internal data buffer.
        @result Returns a reference to the IOFireWireROMCache object's internal data buffer.
    */
    
	virtual const void *getBytesNoCopy();
    
	/*!
        @function getBytesNoCopy
        @abstract Returns a reference into the IOFireWireROMCache object's internal data buffer at particular offset and with a particular length.
        @param start The offset from the base of the internal data buffer.
        @param inLength The length of window.
        @result Returns a pointer at a particular offset into the data buffer, or 0 if the starting offset or length are not valid.
    */
    
	virtual const void *getBytesNoCopy(unsigned int start,
                                       unsigned int inLength);
									   
	virtual void lock( void );
	virtual void unlock( void );

	virtual IOReturn updateROMCache( UInt32 offset, UInt32 length );

	virtual bool hasROMChanged( const UInt32 * newBIB, UInt32 newBIBSize );

	virtual void setROMState( ROMState state, UInt32 generation = 0 );
	virtual IOReturn checkROMState( UInt32 &generation );
	virtual IOReturn checkROMState( void );
	
	virtual bool serialize( OSSerialize * s ) const APPLE_KEXT_OVERRIDE;
	
private:
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 0);
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 1);
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 2);
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 3);
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 4);
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 5);
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 6);
    OSMetaClassDeclareReservedUnused(IOFireWireROMCache, 7);
};

#endif
