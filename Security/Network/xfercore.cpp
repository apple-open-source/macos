/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// xfercore - core data transfer engine
//
#include "xfercore.h"
#include <Security/debugging.h>


namespace Security {
namespace Network {


//
// Create an engine-level client object.
// @@@ Defer buffer allocation to mating?
// @@@ Defer state initialization to mating?
//
TransferEngine::Client::Client()
    : mMode(invalidInput), mAutoCopyOut(false),
    mSink(NULL), mSource(NULL),
    mAutoFlush(true),
    mReadBuffer(16384), mWriteBuffer(16384)
{
}

TransferEngine::Client::~Client()
{
}


//
// Add and remove clients to/from the engine
//
void TransferEngine::add(Client *client)
{
    client->io = client->fileDesc(); // punch master I/O down to Selector client level
    Selector::add(client->io, *client, input | critical);	// initial registration
}

void TransferEngine::remove(Client *client)
{
#ifndef NDEBUG
    if (!client->mReadBuffer.isEmpty())
        secdebug("xferengine", "xfer %p(%d) HAD %ld BYTES READ LEFT",
            client, client->fileDesc(), client->mReadBuffer.length());
    if (!client->mWriteBuffer.isEmpty())
        secdebug("xferengine", "xfer %p(%d) HAD %ld BYTES WRITE LEFT",
            client, client->fileDesc(), client->mWriteBuffer.length());
#endif //NDEBUG
    if (client->io.fd () != -1) { // did we have a live socket?
        Selector::remove(client->io);
    }

    client->io = FileDesc();	// invalidate
}


//
// Mode switching.
// In addition to the generic switcher (mode), there are variants that set associated
// information, such as sources/sinks.
//
void TransferEngine::Client::mode(InputMode newMode)
{
    secdebug("xferengine", "xfer %p(%d) switching to mode %d", this, fileDesc(), newMode);
    switch (newMode) {
    case rawInput:
    case lineInput:
        mMode = newMode;
        break;
    case connecting:
        enable(output);
        mMode = connecting;
        break;
    default:
        assert(false);	// can't switch to these modes like that
    }
}

void TransferEngine::Client::mode(Sink &sink, size_t byteCount)
{
    mMode = autoReadInput;
    mSink = &sink;
    mResidualReadCount = byteCount;
    secdebug("xferengine", "xfer %p(%d) switching to autoReadInput (%ld bytes)", 
        this, fileDesc(), byteCount);
}

void TransferEngine::Client::mode(Source &source, size_t byteCount)
{
    assert (!mAutoCopyOut);		// no replacements, please
    mAutoCopyOut = true;
    mSource = &source;
    mResidualWriteCount = byteCount;
    secdebug("xferengine", "xfer %p(%d) enabling autoCopyOut mode (%ld bytes)",
        this, fileDesc(), byteCount);
    enable(output);
}


//
// Output methods. This queues output to be sent to the client's connection
// as soon as practical.
//
void TransferEngine::Client::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void TransferEngine::Client::vprintf(const char *format, va_list args)
{
    mWriteBuffer.vprintf(format, args);
#if !defined(NDEBUG)
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    secdebug("engineio", "%p(%d) <-- %s", this, fileDesc(), buffer);
#endif //NDEBUG
    startOutput();
}

void TransferEngine::Client::printfe(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintfe(format, args);
    va_end(args);
}

void TransferEngine::Client::vprintfe(const char *format, va_list args)
{
    mWriteBuffer.vprintf(format, args);
    mWriteBuffer.printf("\r\n");
#if !defined(NDEBUG)
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    secdebug("engineio", "%p(%d) <-- %s[CRNL]", this, fileDesc(), buffer);
#endif //NDEBUG
    startOutput();
}


//
// Set output auto-flush mode. Think of this as a weak output-hold mode.
// If autoflush is off, we don't try hard to send data out immediately. If it's
// on, we send data as soon as it's generated.
// Calling flushOutput(true) always generates I/O as needed to send output
// data NOW (even if the mode was already on).
// 
void TransferEngine::Client::flushOutput(bool autoFlush)
{
    mAutoFlush = autoFlush;
    secdebug("engineio", "%p(%d) output flush %s", this, fileDesc(), autoFlush? "on" : "off");
    if (mAutoFlush)
        startOutput();
}


//
// StartOutput is called by output generators to get output flowing.
// It may generate output I/O, or hold things in buffers according to
// current settings.
//
void TransferEngine::Client::startOutput()
{
    if (mAutoFlush) {
        if (mAutoCopyOut && !mWriteBuffer.isFull())
            autoCopy();						// try to tack on some autoCopy output
        if (!mWriteBuffer.isEmpty()) {
            mWriteBuffer.write(*this);
            if (mAutoFlush || !mWriteBuffer.isEmpty()) { // possibly more output
                enable(output);				// ask for output-drain notification
            } else {
                disable(output);			// no need for output-possible events
            }
        }
    }
}


//
// Discard any data still in the input buffer.
// This is used to cope with unexpected garbage (protocol violations
// from the server), and shouldn't be used indiscriminately.
//
void TransferEngine::Client::flushInput()
{
    if (!mReadBuffer.isEmpty()) {
        secdebug("engineio", "flushing %ld bytes of input", mReadBuffer.length());
        mReadBuffer.clear();
        mInputFlushed = true;	// inhibit normal buffer ops
    }
}


//
// Given that autoCopyOut mode is active, try to transfer some bytes
// into the write buffer. This is a lazy, fast push, suitable for tacking on
// when you are about to send data for some other reason.
// Returns the number of bytes retrieved from the auto-Source (possibly zero).
//
size_t TransferEngine::Client::autoCopy()
{
    size_t len = mWriteBuffer.available();	//@@@ (true) ?
    if (mResidualWriteCount && mResidualWriteCount < len)
        len = mResidualWriteCount;
    void *addr; mWriteBuffer.locatePut(addr, len);
    mSource->produce(addr, len);
    secdebug("xferengine", "xfer %p(%d) autoCopyOut source delivered %ld bytes",
        this, fileDesc(), len);
    mWriteBuffer.usePut(len);
    return len;
}


//
// This is the notify function called by the IP Selector layer when I/O is possible.
// It runs the state machines for all current clients, calling their transit methods
// in turn.
//
void TransferEngine::Client::notify(int fd, Type type)
{
    try {
        //@@@ Note: We do not currently do anything special about critical events.

        if (type & Selector::output) {
            // if we're in connecting mode
            if (mMode == connecting) {
                Socket s; s = fd;	// Socket(fd) means something different...
                int error = s.error();
                secdebug("xferengine", "xfer %p(%d) connect (errno %d)",
                    this, fd, error);
                transit(connectionDone, NULL, error);
                return;
            }
            
            //@@@ use high/low water marks here
            if (mAutoCopyOut && !mWriteBuffer.isFull()) {
                if (autoCopy() == 0) {
                    switch (mSource->state()) {
                    case Source::stalled:
                        // ah well, maybe later
                        secdebug("xferengine", "xfer %p(%d) autoCopyOut source is stalled", this, fd);
                        break;
                    case Source::endOfData:
                        mAutoCopyOut = false;	// done
                        secdebug("xferengine", "xfer %p(%d) autoCopyOut end of data", this, fd);
                        if (mResidualWriteCount > 0)
                            secdebug("xferengine", "xfer %p(%d) has %ld autoCopy bytes left",
                                this, fd, mResidualWriteCount);
                        transit(autoWriteDone);
                        if (!isActive())
                            return;		// transit removed us; stop now
                        break;
                    default:
                        assert(false);
                    }
                }
            }
            if (mWriteBuffer.isEmpty()) {	// output possible, no output pending
                secdebug("xferengine", "xfer %p(%d) disabling output (empty)", this, fd);
                disable(output);
            } else {					// stuff some more
                size_t length = mWriteBuffer.write(*this);
                secdebug("xferengine", "xfer %p(%d) writing %ld bytes", this, fd, length);
            }
        }
    
        if (type & Selector::input) {
            secdebug("xferengine", "xfer %p(%d) input ready %d bytes",
                this, fd, io.iocget<int>(FIONREAD));
    
            do {
                mInputFlushed = false;	// preset normal
                
                //@@@ break out after partial buffer to give Equal Time to other transfers? good idea?!
                if (!atEnd() && mReadBuffer.read(*this) == 0 && !atEnd()) {
                    mReadBuffer.read(*this, true);
                }
                
                if (mReadBuffer.isEmpty() && atEnd()) {
                    transit(endOfInput);
                    break;
                }
                switch (mMode) {
                case rawInput:
                    rawInputTransit();
                    break;
                case lineInput:
                    if (!lineInputTransit())
                        return;		// no full line; try again later
                    break;
                case autoReadInput:
                    autoReadInputTransit();
                    if (mMode != autoIODone)
                        break;
                    // autoRead completed; fall through to autoIODone handling
                case autoIODone:
                    mMode = invalidInput;		// pre-mark error
                    transit(autoReadDone);		// notify; this must reset mode or exit
                    if (!isActive())			// if we're terminated...
                        return;					// ... then go
                    assert(mMode != invalidInput); // else enforce mode reset
                    break;
                case connecting:
                    {
                        // we should never be here. Selector gave us "read but not write" while connecting. FUBAR
                        Socket s; s = fd;
                        secdebug("xferengine",
                            "fd %d input while connecting (errno=%d, type=%d)",
                            fd, s.error(), type);
                        UnixError::throwMe(ECONNREFUSED);	// likely interpretation
                    }
                default:
                    secdebug("xferengine", "mode error in input sequencer (mode=%d)", mMode);
                    assert(false);
                }
                if (!io)		// client has unhooked; clear buffer and exit loop
                    flushInput();
            } while (!mReadBuffer.isEmpty());
            //@@@ feed back for more output here? But also see comments above...
            //@@@ probably better to take the trip through the Selector
        }
    } catch (const CssmCommonError &err) {
        transitError(err);
    } catch (...) {
        transitError(UnixError::make(EIO));		// best guess (could be anything)
    }
}

void TransferEngine::Client::rawInputTransit()
{
    // just shove it at the user
    char *addr; size_t length = mReadBuffer.length();
    mReadBuffer.locateGet(addr, length);
    secdebug("engineio", "%p(%d) --> %d bytes RAW",
        this, fileDesc(), io.iocget<int>(FIONREAD));
    transit(inputAvailable, addr, length);
    if (!mInputFlushed)
        mReadBuffer.useGet(length);
}

bool TransferEngine::Client::lineInputTransit()
{
    char *line; size_t length = mReadBuffer.length();
    mReadBuffer.locateGet(line, length);

    char *nl;
    for (nl = line; nl < line + length && *nl != '\n'; nl++) ;
    if (nl == line + length)				// no end-of-line, wait for more
        return false;
        
    if (nl > line && nl[-1] == '\r') {		// proper \r\n termination
        nl[-1] = '\0';						// terminate for transit convenience
        secdebug("engineio", "%p(%d) --> %s", this, fileDesc(), line);
        transit(inputAvailable, line, nl - line - 1);
    } else {								// improper, tolerate
        nl[0] = '\0';						// terminate for transit convenience
        secdebug("engineio", "%p(%d) [IMPROPER] --> %s", this, fileDesc(), line);
        transit(inputAvailable, line, nl - line);
    }
    if (!mInputFlushed)
        mReadBuffer.useGet(nl - line + 1);
    return true;
}

void TransferEngine::Client::autoReadInputTransit()
{
    secdebug("xferengine", "xfer %p(%d) %ld pending %d available",
        this, fileDesc(), mReadBuffer.length(), io.iocget<int>(FIONREAD));
    void *data; size_t length = mReadBuffer.length();
    if (mResidualReadCount && mResidualReadCount < length)
        length = mResidualReadCount;
    mReadBuffer.locateGet(data, length);
    secdebug("engineio", "%p(%d) --> %ld bytes autoReadInput", this, fileDesc(), length);
    mSink->consume(data, length);
    if (!mInputFlushed)
        mReadBuffer.useGet(length);
    if (mResidualReadCount && (mResidualReadCount -= length) == 0)
        mMode = autoIODone;
}


//
// The (protected) tickle() method causes a one-time scan
// of the requesting client. This will simulate an input-ready event
// and possibly call the transit method.
// This is designed to be used from validate() or in other unusual
// external situations. Don't call this from within transit().
//
void TransferEngine::Client::tickle()
{
    notify(io, input | critical);
}


//
// The default read/write methods perform direct I/O on the underlying file descriptor.
//
size_t TransferEngine::Client::read(void *data, size_t size)
{ return io.read(data, size); }

size_t TransferEngine::Client::write(const void *data, size_t size)
{ return io.write(data, size); }

bool TransferEngine::Client::atEnd() const
{ return io.atEnd(); }


}	// end namespace Network
}	// end namespace Security
