/*
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

#include <IOKit/system.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODataQueueShared.h>
#include "IOHIDEventQueue.h"
    
#define super IODataQueue
OSDefineMetaClassAndStructors( IOHIDEventQueue, IODataQueue )

//---------------------------------------------------------------------------
// Factory methods.

IOHIDEventQueue * IOHIDEventQueue::withCapacity( UInt32 size )
{
    IOHIDEventQueue * queue = new IOHIDEventQueue;
    
    if ( queue && !queue->initWithCapacity(size) )
    {
        queue->release();
        queue = 0;
    }

    queue->_started             = false;
    queue->_lock                = IOLockAlloc();
    queue->_numEntries          = size / DEFAULT_HID_ENTRY_SIZE;
    queue->_currentEntrySize  = DEFAULT_HID_ENTRY_SIZE;
    queue->_maxEntrySize      = DEFAULT_HID_ENTRY_SIZE;
    
    return queue;
}

IOHIDEventQueue * IOHIDEventQueue::withEntries( UInt32 numEntries,
                                                UInt32 entrySize )
{
    IOHIDEventQueue * queue = new IOHIDEventQueue;

    if ( queue && !queue->initWithEntries(numEntries, entrySize) )
    {
        queue->release();
        queue = 0;
    }

    queue->_started             = false;
    queue->_lock                = IOLockAlloc();
    queue->_numEntries          = numEntries;
    queue->_currentEntrySize  = DEFAULT_HID_ENTRY_SIZE;
    queue->_maxEntrySize      = DEFAULT_HID_ENTRY_SIZE;

    return queue;
}

void IOHIDEventQueue::free()
{
    if (_lock)
    {
        IOLockLock(_lock);
        IOLock*	 tempLock = _lock;
        _lock = NULL;
        IOLockUnlock(tempLock);
        IOLockFree(tempLock);
    }
    
    if ( _descriptor )
    {
        _descriptor->release();
        _descriptor = 0;
    }

    super::free();
}


//---------------------------------------------------------------------------
// Add data to the queue.

Boolean IOHIDEventQueue::enqueue( void * data, UInt32 dataSize )
{
    IOReturn ret = true;
    
    if ( _lock )
        IOLockLock(_lock);

    // if we are not started, then dont enqueue
    // for now, return true, since we dont wish to push an error back
    if (_started)
        ret = super::enqueue(data, dataSize);

    if ( _lock )
        IOLockUnlock(_lock);

    return ret;
}


//---------------------------------------------------------------------------
// Start the queue.

void IOHIDEventQueue::start() 
{
    if ( _lock )
        IOLockLock(_lock);

    if ( _started )
        goto START_END;

    if ( _currentEntrySize != _maxEntrySize )
    {
        // Free the existing queue data
        if (dataQueue) {
            IOFreeAligned(dataQueue, round_page_32(dataQueue->queueSize + DATA_QUEUE_MEMORY_HEADER_SIZE));
        }
        
        if (_descriptor) {
            _descriptor->release();
            _descriptor = 0;
        }
        
        // init the queue again.  This will allocate the appropriate data.
        if ( !initWithEntries(_numEntries, _maxEntrySize) ) {
            goto START_END;
        }
        
        _currentEntrySize = _maxEntrySize;
    }
    else if ( dataQueue )
    {
        dataQueue->head = 0;
        dataQueue->tail = 0;
    }

    _started = true;

START_END:
    if ( _lock )
        IOLockUnlock(_lock);

}

void IOHIDEventQueue::stop()
{
    if ( _lock )
        IOLockLock(_lock);

    _started = false;

    if ( _lock )
        IOLockUnlock(_lock);
}

Boolean IOHIDEventQueue::isStarted()
{
    bool ret;
    
    if ( _lock )
        IOLockLock(_lock);

    ret = _started;

    if ( _lock )
        IOLockUnlock(_lock);
        
    return ret;
}

//---------------------------------------------------------------------------
// Add element to the queue.

void IOHIDEventQueue::addElement( IOHIDElement * element )
{
    UInt32 elementSize;
    
    if ( !element )
        return;
        
    if ( !_elementSet )
    {
        _elementSet = OSSet::withCapacity(4);
    }
    
    if ( _elementSet->containsObject( element ) )
        return;
        
    elementSize = element->getElementValueSize() + sizeof(void *);
    
    if ( _maxEntrySize < elementSize )
        _maxEntrySize = elementSize;
}

//---------------------------------------------------------------------------
// Remove element from the queue.

void IOHIDEventQueue::removeElement( IOHIDElement * element )
{
    OSCollectionIterator *      iterator;
    IOHIDElement *       temp;
    UInt32                      size        = 0;
    UInt32                      maxSize     = DEFAULT_HID_ENTRY_SIZE;
    
    if ( !element || !_elementSet || !_elementSet->containsObject( element ))
        return;
        
    _elementSet->removeObject( element );
    
    if ( iterator = OSCollectionIterator::withCollection(_elementSet) )
    {
        while ( temp = (IOHIDElement *)iterator->getNextObject() )
        {
            size = temp->getElementValueSize() + sizeof(void *);
            
            if ( maxSize < size )
                maxSize = size;   
        }
    
        iterator->release();
    }
        
    _maxEntrySize = maxSize;
}

//---------------------------------------------------------------------------
// get entry size from the queue.

UInt32 IOHIDEventQueue::getEntrySize( )
{
    return _maxEntrySize;
}


//---------------------------------------------------------------------------
// get a mem descriptor.  replacing default behavior

IOMemoryDescriptor * IOHIDEventQueue::getMemoryDescriptor()
{
    if ((dataQueue != 0) && !_descriptor) {
        _descriptor = IOMemoryDescriptor::withAddress(dataQueue, dataQueue->queueSize + DATA_QUEUE_MEMORY_HEADER_SIZE, kIODirectionOutIn);
    }

    return _descriptor;
}

//---------------------------------------------------------------------------
// 

OSMetaClassDefineReservedUnused(IOHIDEventQueue,  0);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  1);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  2);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  3);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  4);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  5);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  6);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  7);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  8);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  9);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 10);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 11);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 12);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 13);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 14);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 15);
