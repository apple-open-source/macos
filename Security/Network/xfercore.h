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
#ifndef _H_XFERCORE
#define _H_XFERCORE

#include <Security/ip++.h>
#include <Security/selector.h>
#include <Security/buffers.h>
#include <Security/streams.h>
#include <cstdarg>

#if defined(SOCKS_SUPPORT)
# include <Security/socks++.h>
# define TCPClientSocket SocksClientSocket
# define TCPServerSocket SocksServerSocket
#endif

using Security::Buffer;
using namespace IPPlusPlus;


namespace Security {
namespace Network {


class TransferEngine : public Selector {
public:
    TransferEngine() { }
    virtual ~TransferEngine() { }
    
public:
    class Client : public Selector::Client {
        friend class TransferEngine;
    public:
        Client();
        virtual ~Client();
    
    public:
        enum InputMode {
            invalidInput,				// error mode (invalid)
            connecting,					// working on TCP connection
            rawInput,					// raw chunk input (whatever's on the wire)
            lineInput,					// Internet lines input (\r\n)
            autoReadInput,				// bulk read to docked sink
            
            autoIODone					// transiition marker
        };
        InputMode mode() const		{ return mMode; }
        void mode(InputMode m);			// set (switch) mode
        void mode(Sink &sink, size_t byteCount = 0);
        
        void mode(Source &source, size_t byteCount = 0);
        bool autoWriteActive() const	{ return mSource; }
        
        enum Event {					// event type: (input, length) arguments
            inputAvailable,				// input available in current mode: (data, length)
            connectionDone,				// TCP connection event: (NULL, errno)
            autoReadDone,				// autoReadInput has completed: (NULL, 0)
            autoWriteDone,				// autoWriteOutput has completed: (NULL, 0)
            endOfInput,					// end of data stream from remote end: (NULL, 0)
            ioError						// I/O failed: (CssmCommonError *, 0)
        };
        
        virtual void transit(Event event, char *data = NULL, size_t length = 0) = 0;
        virtual void transitError(const CssmCommonError &error) = 0;
        virtual int fileDesc() const = 0;
        
    public:
        // override this to implement I/O filters - default is pass-through
        virtual size_t read(void *data, size_t size);
        virtual size_t write(const void *data, size_t size);
        virtual bool atEnd() const;
        
    protected:
        void printf(const char *format, ...);
        void printfe(const char *format, ...);
        void vprintf(const char *format, va_list args);
        void vprintfe(const char *format, va_list args);
        
        void flushOutput(bool autoFlush = true);
        
        void flushInput();
        
        void tickle();
    
    private:
        void notify(int fd, Type type);
        
    private:
        void rawInputTransit();
        bool lineInputTransit();
        void autoReadInputTransit();
        
        void startOutput();
        size_t autoCopy();
        
    private:
        InputMode mMode;				// current mode
        bool mAutoCopyOut;				// auto-copyout overlay mode
        Sink *mSink;					// sink for autoReadInput mode
        Source *mSource;				// source for copyout overlay mode
        size_t mResidualReadCount;		// bytes left to autoReadInput (zero => unlimited)
        size_t mResidualWriteCount;		// bytes left to autoCopyOut (zero => unlimited)
        bool mAutoFlush;				// output auto-flush mode
        bool mInputFlushed;				// transit flushed input; do not complete buffer ops
        
        FileDesc io;
        
        Buffer mReadBuffer;
        Buffer mWriteBuffer;
    };
        
public:
    void add(Client *client);
    void remove(Client *client);
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_XFERCORE
