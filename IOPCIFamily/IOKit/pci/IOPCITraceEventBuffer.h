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

#ifndef _IOKIT_IOPCITRACEEVENTBUFFER_H
#define _IOKIT_IOPCITRACEEVENTBUFFER_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOLocks.h>
#include <IOKit/pci/IOPCITraceEventDefinitions.h>


#define DEFAULT_EVENT_BUFFER_SIZE   1024

using timestamp_t = uint64_t;

#define MAX_EVENT_DATA_SIZE         31							// max size that can be represented in 5 bits 

#define MAX_EVENT_SIZE              (sizeof(IOPCITraceEventHeader) + sizeof(timestamp_t) + MAX_EVENT_DATA_SIZE)
/*
 * Event header is defined as an array of two bytes and accessed via shifts and
 * masks to make event size determination at runtime more efficient, not reliant 
 * on compiler-specific, and in general more efficient. Specifically,
 *
 * |15 |14               10| 9                                    0|
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * | T |                   |                                       |
 * | S |  DATA BYTE COUNT  |              EVENT CODE               |
 * |   |                   |                                       |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |            Byte 0             |            Byte 1             |
 *
 * The type definition and inline functions abstract away these details.
 */

using IOPCITraceEventHeader = uint8_t[ 2 ];

static inline bool 
getEventTimestampFlag(const IOPCITraceEventHeader& header) {
    return (header[0] >> 7) & 0x1;
}

static inline uint8_t 
getEventDataByteCount(const IOPCITraceEventHeader& header) {
    return (header[0] >> 2) & 0x1F;
}

static inline uint16_t 
getEventCode(const IOPCITraceEventHeader& header) {
    return static_cast<uint16_t>(((header[0] & 0x03) << 8) | header[1]);
}

static inline void 
setEventTimestampFlag(IOPCITraceEventHeader& header, bool flag) {
    header[0] = static_cast<uint8_t>((header[0] & 0x7F) | ((flag & 0x1) << 7));
}

static inline void 
setEventDataByteCount(IOPCITraceEventHeader& header, uint8_t dataByteCount) {
    header[0] = static_cast<uint8_t>((header[0] & 0x83) | ((dataByteCount & 0x1F) << 2));
}

static inline void 
setEventCode(IOPCITraceEventHeader& header, uint16_t eventCode) {
    header[0] = (header[0] & 0xFC) | ((eventCode >> 8) & 0x03);
    header[1] = eventCode & 0xFF;
}


using IOPCIRawTraceEvent = uint8_t[MAX_EVENT_SIZE];

PACKED_STRUCT(IOPCIProtoTraceEvent) {
    IOPCITraceEventHeader       header;
    uint8_t                     eventData[MAX_EVENT_DATA_SIZE];
};

PACKED_STRUCT(IOPCIProtoTraceEventWithTimestamp) {
    IOPCITraceEventHeader       header;
    uint64_t                    timestamp;
    uint8_t                     eventData[MAX_EVENT_DATA_SIZE];
};


__exported_push
class IOPCITraceEventBuffer
{
public:
    IOPCITraceEventBuffer();
    ~IOPCITraceEventBuffer();
    
    void logPCITraceEvent(uint16_t eventCode, void* eventData = nullptr, size_t dataByteCount = 0);
    
    void logPCITraceEventWithTimestamp(uint16_t eventCode, void* eventData = nullptr, size_t dataByteCount = 0);
    
    IOReturn readPCITraceEvent(IOPCIRawTraceEvent event);

private:
    void logPCITraceEvent(uint16_t eventCode, void* eventData, size_t dataByteCount, bool includeTimestamp);

    void writePCITraceEvent(void* event, size_t eventSize);
    
    size_t          _writeIndex = 0;
    size_t          _readIndex = 0;
    uint8_t*        _buffer = nullptr;
    size_t          _bufferSize = DEFAULT_EVENT_BUFFER_SIZE;
    
    IOSimpleLock*   _bufferLock;

    size_t          availableSpace();
};
__exported_pop

#endif /* _IOKIT_IOPCITRACEEVENTBUFFER_H */

