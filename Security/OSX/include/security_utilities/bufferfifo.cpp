/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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


//
// bufferfifo - a Sink that queues data in a FIFO of buffers for retrieval
//
#include "bufferfifo.h"
#include <security_utilities/memutils.h>


namespace Security {


//
// On destruction, throw away all queued buffers (that haven't been picked up)
//
BufferFifo::~BufferFifo()
{
    while (!mBuffers.empty()) {
        delete mBuffers.front();
        mBuffers.pop();
    }
}

void BufferFifo::clearBuffer()
{
    while (!mBuffers.empty()) {
        delete mBuffers.front();
        mBuffers.pop();
    }
    mSize = 0;
}

//
// This is the put function of a Sink. We store the data in at most two buffers:
// First we append to the last (partially filled) one; then we allocate a new one
// (if needed) to hold the rest.
//
void BufferFifo::consume(const void *data, size_t size)
{
    mSize += size;
    
    // step 1: fill the rearmost (partially filled) buffer
    if (size > 0 && !mBuffers.empty()) {
        Buffer *current = mBuffers.back();
        size_t length = current->put(data, size);
        data = LowLevelMemoryUtilities::increment(data, length);
        size -= length;
    }
    // step 2: if there's anything left, make a new buffer and fill it
    if (size > 0) {	// not done
        Buffer *current = new Buffer(max(bufferLength, size));
        mBuffers.push(current);
        assert(current->available() >= size);
        current->put(data, size);
    }
}


//
// Pull the first (FI) buffer off the queue and deliver it.
// We retain no memory of it; it belongs to the caller now.
//
Buffer *BufferFifo::pop()
{
    assert(!mBuffers.empty());
    Buffer *top = mBuffers.front();
    mBuffers.pop();
    return top;
}


}	// end namespace Security
