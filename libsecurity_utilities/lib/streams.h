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
// streams.h - lightweight source and sink objects
//
#ifndef _H_STREAMS
#define _H_STREAMS

#include "unix++.h"


namespace Security {

using UnixPlusPlus::FileDesc;


//
// An abstract Source object.
// Source can yield data when its produce method is called. Produce can yield
// anything between zero and length bytes and sets length accordingly.
// If the last call to produce returned zero bytes (and only then), the state method
// will yield an explanation:
//	producing -> we're in business; there just no data quite yet (try again)
//	stalled -> there may be more data coming, but not in the near future;
//			wait a while then call state again to see
//	endOfData -> no more data will be produced by this Source
// When called *before* the first call to produce, getSize may return the number
// of bytes that all calls to produce will yield together. If getSize returns unknownSize,
// this value cannot be determined beforehand. GetSize *may* yield the number of bytes
// yet to come when called after produce, but this is not guaranteed for all Sources.
//
class Source {
public:
    virtual void produce(void *data, size_t &length) = 0;
    virtual ~Source() { }

    static const size_t unknownSize = size_t(-1);
    virtual size_t getSize();
    
    enum State {
        producing,		// yielding data (go ahead)
        stalled,		// no data now, perhaps more later
        endOfData		// end of data (no more data)
    };
    virtual State state() const;
    
protected:
    State mState;		// auto-regulated state (can be overridden)
};


//
// An abstract Sink object.
// Sinks can cansume data when their consume method is called.
// Sinks cannot refuse data; they always consume all data given to consume.
// There is currently no flow control/throttle mechanism (one will probably
// be added soon).
//
class Sink {
public:
    Sink() : mSize(0) {}
    virtual ~Sink() { }
    virtual void consume(const void *data, size_t length) = 0;
    virtual void setSize(size_t expectedSize);
    size_t getSize() {return mSize;}
    
protected:
    size_t mSize;

};


//
// The NullSource produces no data.
//
class NullSource : public Source {
public:
    void produce(void *addr, size_t &len);
    State state() const;
};


//
// A FileSource reads from a UNIX file or file descriptor.
// Note that getSize will yield the size of the underlying i-node,
// which is usually correct but may not be in the case of simultaneous
// access.
//
class FileSource : public Source, public FileDesc {
public:
    FileSource(const char *path, int mode = O_RDONLY) : FileDesc(path, mode) { mState = producing; }
    FileSource(int fd) : FileDesc(fd) { mState = producing; }
    void produce(void *data, size_t &length);
    size_t getSize();
};


//
// A MemorySource yields the contents of a preset contiguous memory block.
//
class MemorySource : public Source {
public:
    MemorySource(const void *data, size_t length) : mData(data), mRemaining(length) { }

    template <class Data>
    MemorySource(const Data &data) : mData(data.data()), mRemaining(data.length()) { }
    
    void produce(void *data, size_t &length);
    size_t getSize();
    State state() const;
    
private:
    const void *mData;
    size_t mRemaining;
};


//
// A NullSink eats all data and discards it quietly.
//
class NullSink : public Sink {
public:
    void consume(const void *data, size_t length);
};


//
// A FileSink writes its received data to a UNIX file or file descriptor.
//
class FileSink : public Sink, public FileDesc {
public:
    FileSink(const char *path, int mode = O_WRONLY | O_CREAT | O_TRUNC)
        : FileDesc(path, mode) { }
    FileSink(int fd) : FileDesc(fd) { }
    void consume(const void *data, size_t length);
};


//
// MemorySinks collect output in a contiguous memory block.
// This is not often a good idea, so if you find yourself using this,
// consider consuming on-the-fly or streaming to secondary media,
// or (at least) use a BufferFifo instead.
//
class MemorySink : public Sink {
public:
    MemorySink() : mBuffer(NULL), mMax(0) { }
    ~MemorySink()	{ free(mBuffer); }
    
    void consume(const void *data, size_t length);
    void setSize(size_t expectedSize);
    
    void *data() const		{ return mBuffer; }
    size_t length() const	{ return mSize; }
    
    void clear()			{ free(mBuffer); mBuffer = NULL; mSize = mMax = 0; }
    
private:
    void grow(size_t newSize);
    
private:
    void *mBuffer;		// buffer base
    size_t mMax;		// currently allocated
};


}	// end namespace Security


#endif /* _H_STREAMS */
