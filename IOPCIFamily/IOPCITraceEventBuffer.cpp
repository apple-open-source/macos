/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
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


#include <IOKit/pci/IOPCITraceEventBuffer.h>
#include <IOKit/IOLib.h>


#define REQUIRE(_expr)					\
do {									\
	if (!(_expr))						\
		panic("%s:%u: REQUIRE failed: %s", __PRETTY_FUNCTION__,	__LINE__, #_expr);	\
} while(0)


IOPCITraceEventBuffer::IOPCITraceEventBuffer()
{
	_bufferLock = IOSimpleLockAlloc();

	REQUIRE(_bufferLock);

	/*
     * Check bootargs for special conditions such as buffer size. If no
     * bootarg, then _bufferSize will default to DEFAULT_EVENT_BUFFER_SIZE.
	 */
    uint32_t eventBufferSize = 0;
    if (PE_parse_boot_argn("pci_trace_event_buffer_size", &eventBufferSize, sizeof(eventBufferSize))) {
		/*
         * Make sure our buffer size has enough room for at least one event.
         */
        if (eventBufferSize < MAX_EVENT_SIZE) {
	        eventBufferSize = MAX_EVENT_SIZE;
		}

		_bufferSize = eventBufferSize;
    }
	
    _buffer = (uint8_t*)IOMallocZeroData(_bufferSize);
    
    REQUIRE(_buffer);
}

IOPCITraceEventBuffer::~IOPCITraceEventBuffer()
{
    if (_buffer) {
        IOFreeData(_buffer, _bufferSize);
        _buffer = nullptr;
    }

	if (_bufferLock) {
		IOSimpleLockFree(_bufferLock);
		_bufferLock = nullptr;
	}
}

void
IOPCITraceEventBuffer::logPCITraceEvent(uint16_t eventCode, void* eventData, size_t dataByteCount, bool includeTimestamp)
{
	REQUIRE(!(eventData == nullptr && dataByteCount != 0));
	REQUIRE(!(eventData != nullptr && dataByteCount == 0));
	REQUIRE(dataByteCount <= MAX_EVENT_DATA_SIZE);

    size_t eventSize;

	if (includeTimestamp) {
		IOPCIProtoTraceEventWithTimestamp event;
		setEventTimestampFlag(event.header, true);

		event.timestamp = mach_continuous_time();

		setEventDataByteCount(event.header, dataByteCount);
		setEventCode(event.header, eventCode);

		if (eventData) {
			memcpy(event.eventData, eventData, dataByteCount);
		}

		eventSize = sizeof(IOPCITraceEventHeader) + sizeof(timestamp_t) + dataByteCount;

		writePCITraceEvent(&event, eventSize);
	} else {
		IOPCIProtoTraceEvent event;
		setEventTimestampFlag(event.header, false);
 
		setEventDataByteCount(event.header, dataByteCount);
		setEventCode(event.header, eventCode);

		if (eventData) {
			memcpy(event.eventData, eventData, dataByteCount);
		}

		eventSize = sizeof(IOPCITraceEventHeader) + dataByteCount;

		writePCITraceEvent(&event, eventSize);
    }
}

void
IOPCITraceEventBuffer::logPCITraceEvent(uint16_t eventCode, void* eventData, size_t dataByteCount)
{
	logPCITraceEvent(eventCode, eventData, dataByteCount, false);
}

void
IOPCITraceEventBuffer::logPCITraceEventWithTimestamp(uint16_t eventCode, void* eventData, size_t dataByteCount)
{
	 logPCITraceEvent(eventCode, eventData, dataByteCount, true);
}

size_t
IOPCITraceEventBuffer::availableSpace()
{
	if (_writeIndex >= _readIndex) {
		return _bufferSize - (_writeIndex - _readIndex);
	} else {
		return _readIndex - _writeIndex;
	}
}

void
IOPCITraceEventBuffer::writePCITraceEvent(void* event, size_t eventSize)
{
	/*
     * Arguments event and eventSize have been vetted
     */

    IOSimpleLockLock(_bufferLock);
    
	/*
     * If we don't have enough space, overwrite the
     * oldest event(s) and update _readIndex accordingly.
	 */
	while (availableSpace() < eventSize) {
	    
	    IOPCITraceEventHeader* oldEventHeader = (IOPCITraceEventHeader*)&_buffer[_readIndex];

		/*
         * Figure out size of old event from its header.
		 */
        size_t oldEventSize = sizeof(IOPCITraceEventHeader) + 
        						(getEventTimestampFlag(*oldEventHeader) ? sizeof(timestamp_t) : 0) + 
            					getEventDataByteCount(*oldEventHeader);
	    
		/*
	     * Update _readIndex beyond old event.
		 */
	    _readIndex = (_readIndex + oldEventSize) % _bufferSize;
	}
	
	/*
	 * Now insert the new event into the buffer.
	 */
    size_t bytesUntilEnd = min(eventSize, _bufferSize - _writeIndex);
    memcpy(&_buffer[_writeIndex], event, bytesUntilEnd);
    
    if (bytesUntilEnd < eventSize) {
        memcpy(&_buffer[0], &((IOPCIRawTraceEvent*)event)[bytesUntilEnd], eventSize - bytesUntilEnd);
    }
    
    _writeIndex = (_writeIndex + eventSize) % _bufferSize;
    
    IOSimpleLockUnlock(_bufferLock);
}

IOReturn
IOPCITraceEventBuffer::readPCITraceEvent(IOPCIRawTraceEvent event)
{
    if (event == nullptr) {
        return kIOReturnBadArgument;
	}
    
	IOSimpleLockLock(_bufferLock);

	/*
     * If we've consumed all events, return underrun.
	 */
    if (_readIndex == _writeIndex) {
		IOSimpleLockUnlock(_bufferLock);
        return kIOReturnUnderrun;
	}

	IOPCITraceEventHeader* oldEventHeader = (IOPCITraceEventHeader*)&_buffer[_readIndex];
    size_t oldEventSize = sizeof(IOPCITraceEventHeader) + 
                                (getEventTimestampFlag(*oldEventHeader) ? sizeof(timestamp_t) : 0) + 
                                getEventDataByteCount(*oldEventHeader);
                                
    if (oldEventSize > sizeof(IOPCIRawTraceEvent)) {
		IOSimpleLockUnlock(_bufferLock);
        IOLog("Size of event being read is too large! Size = %ld, max allowed is %ld!\n", oldEventSize, MAX_EVENT_SIZE);
        return kIOReturnError;
    }

	/*
	 * Now insert the new event into the buffer.
	 */
    size_t bytesUntilEnd = min(oldEventSize, _bufferSize - _readIndex);
    memcpy(event, &_buffer[_readIndex], bytesUntilEnd);

    if (bytesUntilEnd < oldEventSize) {
        memcpy(event, &_buffer[0], oldEventSize - bytesUntilEnd);
    }
    
    _readIndex = (_readIndex + oldEventSize) % _bufferSize;
    
    IOSimpleLockUnlock(_bufferLock);
    
    return kIOReturnSuccess;
}

