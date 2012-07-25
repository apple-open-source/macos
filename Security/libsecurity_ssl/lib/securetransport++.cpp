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
#include "securetransport++.h"
#include <security_utilities/debugging.h>


namespace Security {
namespace IPPlusPlus {


//
// Construct a core object.
// This creates the Context object and sets the I/O functions.
//
SecureTransportCore::SecureTransportCore() : mAtEnd(false)
{
    MacOSError::check(SSLNewContext(false, &mContext));
    try {
    	MacOSError::check(SSLSetIOFuncs(mContext, sslReadFunc, sslWriteFunc));
        MacOSError::check(SSLSetConnection(mContext, this));
        secdebug("ssl", "%p constructed", this);
    } catch (...) {
        SSLDisposeContext(mContext);
        throw;
    }
}


//
// On destruction, we force a close and destroy the Context.
//
SecureTransportCore::~SecureTransportCore()
{
    SSLDisposeContext(mContext);	// ignore error (can't do anything if error)
    secdebug("ssl", "%p destroyed", this);
}


//
// Open initiates or continues the SSL handshake.
// In nonblocking mode, open may return while handshake is still in
// Progress. Keep calling open until state() != errSSLWouldBlock, or
// go directly to I/O.
//
void SecureTransportCore::open()
{
    switch (OSStatus err = SSLHandshake(mContext)) {
    case noErr:
    case errSSLWouldBlock:
        secdebug("ssl", "%p open, state=%d", this, state());
        return;
    default:
        MacOSError::throwMe(err);
    }
}


//
// Close the SSL layer if needed.
// Note that this does nothing to the underlying I/O layer.
//
void SecureTransportCore::close()
{
    switch (state()) {
    case kSSLHandshake:
    case kSSLConnected:
        secdebug("ssl", "%p closed", this);
        SSLClose(mContext);
        break;
    default:
        break;
    }
}


//
// Read bytes from the SSL layer. This is the standard FileDescoid
// read function.
// Note that if the connection is still handshaking, handshake will proceed
// and no bytes will be read (yet).
//
size_t SecureTransportCore::read(void *data, size_t length)
{
    if (continueHandshake())
        return 0;
    size_t bytesRead;
    switch (OSStatus err = SSLRead(mContext, data, length, &bytesRead)) {
    case noErr:					// full read
    case errSSLWouldBlock:		// partial read
        return bytesRead;		// (may be zero in non-blocking scenarios)
    case errSSLClosedGraceful:	// means end-of-data, but we may still return some
    case errSSLClosedNoNotify:	// peer closed abruptly (not sending SSL layer shutdown)
        if (bytesRead == 0)
            mAtEnd = true;		// no more data - set final end-of-data flag
        return bytesRead;
    default:
        MacOSError::throwMe(err);
    }
}


//
// Write bytes to the SSL layer. This is the standard FileDescoid write function.
// Note that if the connection is still handshaking, handshake will proceed
// and no bytes will be written (yet).
//
size_t SecureTransportCore::write(const void *data, size_t length)
{
    if (continueHandshake())
        return 0;
    size_t bytesWritten;
    switch (OSStatus err = SSLWrite(mContext, data, length, &bytesWritten)) {
    case noErr:
        return bytesWritten;
    case errSSLWouldBlock:
        return 0;	// no data, no error, no fuss
    default:
        MacOSError::throwMe(err);
    }
}


//
// Continue handshake processing if necessary.
// Returns true if handshake is in Progress and not yet complete.
//
bool SecureTransportCore::continueHandshake()
{
    if (state() == kSSLHandshake) {
        // still in handshake mode; prod it along
        secdebug("ssl", "%p continuing handshake", this);
        switch (OSStatus err = SSLHandshake(mContext)) {
        case noErr:
        case errSSLWouldBlock:
            break;
        default:
            MacOSError::throwMe(err);
        }
        IFDEBUG(if (state() != kSSLHandshake) secdebug("ssl", "%p handshake complete", this));
        return state() == kSSLHandshake;
    } else
        return false;
}


//
// State access methods
//
SSLSessionState SecureTransportCore::state() const
{
    SSLSessionState state;
    MacOSError::check(SSLGetSessionState(mContext, &state));
    return state;
}

SSLProtocol SecureTransportCore::version() const
{
    SSLProtocol version;
    MacOSError::check(SSLGetProtocolVersion(mContext, &version));
    return version;
}

void SecureTransportCore::version(SSLProtocol version)
{
    MacOSError::check(SSLSetProtocolVersion(mContext, version));
}

size_t SecureTransportCore::numSupportedCiphers() const
{
	size_t numCiphers;
    MacOSError::check(SSLGetNumberSupportedCiphers(mContext, &numCiphers));
    return numCiphers;
}

void SecureTransportCore::supportedCiphers(
	SSLCipherSuite *ciphers,
	size_t &numCiphers) const
{
    MacOSError::check(SSLGetSupportedCiphers(mContext, ciphers, &numCiphers));
}

size_t SecureTransportCore::numEnabledCiphers() const
{
	size_t numCiphers;
    MacOSError::check(SSLGetNumberEnabledCiphers(mContext, &numCiphers));
    return numCiphers;
}

void SecureTransportCore::enabledCiphers(
	SSLCipherSuite *ciphers,
	size_t &numCiphers) const
{
    MacOSError::check(SSLGetEnabledCiphers(mContext, ciphers, &numCiphers));
}

void SecureTransportCore::enabledCiphers(
	SSLCipherSuite *ciphers,
	size_t numCiphers)
{
    MacOSError::check(SSLSetEnabledCiphers(mContext, ciphers, numCiphers));
}

bool SecureTransportCore::allowsExpiredCerts() const
{
    Boolean allow;
    MacOSError::check(SSLGetAllowsExpiredCerts(mContext, &allow));
    return allow;
}

void SecureTransportCore::allowsExpiredCerts(bool allow)
{
    MacOSError::check(SSLSetAllowsExpiredCerts(mContext, allow));
}

bool SecureTransportCore::allowsUnknownRoots() const
{
    Boolean allow;
    MacOSError::check(SSLGetAllowsAnyRoot(mContext, &allow));
    return allow;
}

void SecureTransportCore::allowsUnknownRoots(bool allow)
{
    MacOSError::check(SSLSetAllowsAnyRoot(mContext, allow));
}

void SecureTransportCore::peerId(const void *id, size_t length)
{
    MacOSError::check(SSLSetPeerID(mContext, id, length));
}


//
// Implement SecureTransport's read/write transport functions.
// Note that this API is very un-UNIX in that error codes (errSSLClosedGraceful, errSSLWouldBlock)
// are returned even though data has been produced.
//
OSStatus SecureTransportCore::sslReadFunc(SSLConnectionRef connection,
    void *data, size_t *length)
{
    const SecureTransportCore *stc = reinterpret_cast<const SecureTransportCore *>(connection);
    try {
        size_t lengthRequested = *length;
        *length = stc->ioRead(data, lengthRequested);
        secdebug("sslconio", "%p read %lu of %lu bytes", stc, *length, lengthRequested);
        if (*length == lengthRequested)	// full deck
            return noErr;
        else if (stc->ioAtEnd()) {
            secdebug("sslconio", "%p end of source input, returning %lu bytes",
                stc, *length);
            return errSSLClosedGraceful;
        } else
            return errSSLWouldBlock;
    } catch (const UnixError &err) {
        *length = 0;
        if (err.error == ECONNRESET)
            return errSSLClosedGraceful;
        throw;
    } catch (const CommonError &err) {
        *length = 0;
        return err.osStatus();
    } catch (...) {
        *length = 0;
        return -1;	//@@@ generic internal error?
    }
}

OSStatus SecureTransportCore::sslWriteFunc(SSLConnectionRef connection,
    const void *data, size_t *length)
{
    const SecureTransportCore *stc = reinterpret_cast<const SecureTransportCore *>(connection);
    try {
        size_t lengthRequested = *length;
        *length = stc->ioWrite(data, lengthRequested);
        secdebug("sslconio", "%p wrote %lu of %lu bytes", stc, *length, lengthRequested);
        return *length == lengthRequested ? OSStatus(noErr) : OSStatus(errSSLWouldBlock);
    } catch (const CommonError &err) {
        *length = 0;
        return err.osStatus();
    } catch (...) {
        *length = 0;
        return -1;	//@@@ generic internal error?
    }
}


}	// end namespace IPPlusPlus
}	// end namespace Security
