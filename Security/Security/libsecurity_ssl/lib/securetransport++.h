/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
// securetransport++ - C++ interface to Apple's Secure Transport layer
//
#ifndef _H_SECURETRANSPORTPLUSPLUS
#define _H_SECURETRANSPORTPLUSPLUS

#include <security_utilities/ip++.h>
#include <Security/SecureTransport.h>


namespace Security {
namespace IPPlusPlus {


//
// The common-code core of a SecureTransport context and session.
// Abstract - do not use directly.
//
class SecureTransportCore {
public:
    SecureTransportCore();
    virtual ~SecureTransportCore();

    void open();		// open SSL (but not underlying I/O)
    void close();		// close SSL (but not underlying I/O)

    SSLSessionState state() const;

    SSLProtocol version() const;
    void version(SSLProtocol v);

	size_t numSupportedCiphers() const;
	void supportedCiphers(SSLCipherSuite *ciphers, size_t &numCiphers) const;

	size_t numEnabledCiphers() const;
	void enabledCiphers(SSLCipherSuite *ciphers, size_t &numCiphers) const;	// get
	void enabledCiphers(SSLCipherSuite *ciphers, size_t numCiphers);		// set

    bool allowsExpiredCerts() const;
    void allowsExpiredCerts(bool allow);

    bool allowsUnknownRoots() const;
    void allowsUnknownRoots(bool allow);

    void peerId(const void *data, size_t length);
    template <class T> void peerId(const T &obj)	{ peerId(&obj, sizeof(obj)); }

    size_t read(void *data, size_t length);
    size_t write(const void *data, size_t length);
    bool atEnd() const		{ return mAtEnd; }

protected:
    virtual size_t ioRead(void *data, size_t length) const = 0;
    virtual size_t ioWrite(const void *data, size_t length) const = 0;
    virtual bool ioAtEnd() const = 0;

private:
	static OSStatus sslReadFunc(SSLConnectionRef, void *, size_t *);
	static OSStatus sslWriteFunc(SSLConnectionRef, const void *, size_t *);

    bool continueHandshake();

private:
    SSLContextRef mContext;		// SecureTransport session/context object
    bool mAtEnd;				// end-of-data flag derived from last SSLRead
};


//
// This is what you use. The constructor argument is a FileDescoid object
// of some kind, such as a FileDesc, Socket, etc.
// Note that SecureTransport is in turn a FileDescoid object, so you can read/write
// it in the usual fashion, and it will in turn read/write cipher data from its I/O source.
//
template <class IO>
class SecureTransport : public SecureTransportCore {
public:
    SecureTransport(IO &ioRef) : io(ioRef) { }
    ~SecureTransport() { close(); }

    IO &io;

private:
    size_t ioRead(void *data, size_t length) const			{ return io.read(data, length); }
    size_t ioWrite(const void *data, size_t length) const	{ return io.write(data, length); }
    bool ioAtEnd() const									{ return io.atEnd(); }
};


}	// end namespace IPPlusPlus
}	// end namespace Security


#endif //_H_SECURETRANSPORTPLUSPLUS
