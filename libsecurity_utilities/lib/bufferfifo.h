/*
 * Copyright (c) 2000-2002,2004 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_BUFFERFIFO
#define _H_BUFFERFIFO

#include "streams.h"
#include "buffers.h"
#include <list>
#include <queue>


namespace Security {


//
// A BufferFifo acts as a First-in First-out queue of Buffer objects.
// This is usually used as a flexible I/O buffer queue mechanism.
// For convenience, a BufferFifo is a Sink, so you can push data
// into it directly using the Sink mechanism.
// Note that there is currently no mechanism for restricting the
// memory footprint of a BufferFifo.
//
class BufferFifo : public Sink {
public:
    BufferFifo(size_t es = 4096) : bufferLength(es) { }
    ~BufferFifo();
    
    Buffer *top() const			{ assert(!mBuffers.empty()); return mBuffers.front(); }
    Buffer *pop();
    void push(Buffer *b)		{ mBuffers.push(b); }
    
    bool isEmpty() const		{ return mBuffers.empty(); }
    size_t size() const			{ return mBuffers.size(); }
    size_t topLength() const	{ assert(!isEmpty()); return mBuffers.front()->length(); }
    
    // Sink implementation
    void consume(const void *data, size_t size);
    void clearBuffer();

private:
    typedef queue< Buffer *, list<Buffer *> > BufferQueue;
    BufferQueue mBuffers;
    const size_t bufferLength;
};



}	// end namespace Security


#endif /* _H_BUFFERFIFO */
