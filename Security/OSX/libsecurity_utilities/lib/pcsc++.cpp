/*
 * Copyright (c) 2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// pcsc++ - PCSC client interface layer in C++
//
#include "pcsc++.h"
#include <security_utilities/debugging.h>
#include <PCSC/pcsclite.h>
#include <PCSC/wintypes.h>
#include <Security/cssmapple.h>

namespace Security {
namespace PCSC {


//
// Internal utilities
//
static void decode(vector<string> &names, const char *buffer, size_t size)
{
	names.clear();
	for (size_t pos = 0; pos < size - 1; ) {
		size_t len = strlen(buffer + pos);
		names.push_back(string(buffer + pos, len));
		pos += len + 1;
	}
}

inline void decode(vector<string> &names, const vector<char> &buffer, size_t size)
{
	decode(names, &buffer[0], size);
}


//
// PCSC domain errors
//
Error::Error(unsigned long err) : error(err)
{
    SECURITY_EXCEPTION_THROW_PCSC(this, (unsigned int)err);
    secnotice("security_exception", "pcsc: %d", (unsigned int) err);
}


const char *Error::what() const throw ()
{
	return pcsc_stringify_error((int32_t)error);
}


void Error::throwMe(unsigned long err)
{
	throw Error(err);
}


OSStatus Error::osStatus() const
{
	switch (error)
	{
	// @@@ more errors should be mapped here
	case SCARD_W_RESET_CARD:
		return CSSMERR_CSP_DEVICE_RESET;
	default:
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

int Error::unixError() const
{
	return EINVAL;  //@@@ preliminary
}


//
// PodWrappers
//
void ReaderState::set(const char *name, unsigned long known)
{
	clearPod();
	szReader = name;
	pvUserData = NULL;
	dwCurrentState = (uint32_t)known;
}


void ReaderState::lastKnown(unsigned long s)
{
	// clear out CHANGED and UNAVAILABLE
	dwCurrentState = (uint32_t)s & ~(SCARD_STATE_CHANGED | SCARD_STATE_UNAVAILABLE);
}


void ReaderState::setATR(const void *atr, size_t size)
{
	if (size > sizeof(rgbAtr))
		Error::throwMe(SCARD_E_INVALID_ATR);
	memcpy(rgbAtr, atr, size);
	cbAtr = (uint32_t)size;
}


#if defined(DEBUGDUMP)

void ReaderState::dump()
{
	Debug::dump("reader(%s) state=0x%x events=0x%x",
		szReader ? szReader : "(null)", dwCurrentState, dwEventState);
	Debug::dumpData(" ATR", rgbAtr, cbAtr);
}

#endif //DEBUGDUMP


//
// Session objects
//
Session::Session()
	: mIsOpen(false)
{
}


Session::~Session()
{
	close();
}


//
// (Re)establish PCSC context.
// Don't fail on SCARD_F_INTERNAL_ERROR (pcscd not running - urgh).
//
void Session::open()
{
	if (!mIsOpen) {
		try {
			Error::check(::SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &mContext));
			mIsOpen = true;
			secinfo("pcsc", "context opened");
		} catch (const Error &err) {
			if (err.error == SCARD_F_INTERNAL_ERROR)
			{
				secinfo("pcsc", "got internal error; assuming pcscd absent; context not ready");
				return;
			}
		}
	}
}

void Session::close()
{
	if (mIsOpen) {
		mIsOpen = false;
		try {
			if (mContext)
			Error::check(SCardReleaseContext(mContext));
			secinfo("pcsc", "context closed");
		} catch (const Error &err) {
			if (err.error == SCARD_F_INTERNAL_ERROR)
			{
				secinfo("pcsc", "got internal error; assuming pcscd absent; context not ready");
				return;
			}
		}
	}
}

bool Session::check(long rc)
{
	switch ((unsigned long) rc) {
	case SCARD_S_SUCCESS:
		return true;	// got reader(s), call succeeded
	case SCARD_E_READER_UNAVAILABLE:
		return false;   // no readers, but don't treat as error
	default:
		Error::throwMe(rc);
		return false;   // placebo
	}
}


void Session::listReaders(vector<string> &readers, const char *groups)
{
	uint32_t size = uint32_t(mReaderBuffer.size());
	for (;;)
	{
		int32_t rc = ::SCardListReaders(mContext, groups, &mReaderBuffer[0], &size);
		switch (rc) {
		case SCARD_S_SUCCESS:
			if (size <= mReaderBuffer.size()) {
				decode(readers, mReaderBuffer, size);
				return;
			}
		case (int32_t) SCARD_E_INSUFFICIENT_BUFFER:
			mReaderBuffer.resize(size);
			break;
		default:
			Error::throwMe(rc);
		}
	}
}


//
// Reader status check
//
void Session::statusChange(ReaderState *readers, unsigned int nReaders, long timeout)
{
	check(::SCardGetStatusChange(mContext, (uint32_t)timeout, readers, nReaders));
}


//
// PCSC Card objects
//
Card::Card()
	: mConnectedState(kInitial)
{
}

Card::~Card()
{
}

void Card::setIOType(unsigned long activeProtocol)
{
	switch (activeProtocol)
	{
	case SCARD_PROTOCOL_T0:
		mIOType = SCARD_PCI_T0;
		break;
	case SCARD_PROTOCOL_T1:
		mIOType = SCARD_PCI_T1;
		break;
	default:
		mIOType = SCARD_PCI_RAW;
		break;
	}
}

void Card::connect(Session &session, const char *reader,
	unsigned long share, unsigned long protocols)
{
	uint32_t activeProtocol;
	Error::check(::SCardConnect(session.mContext,
		reader, (uint32_t)share, (uint32_t)protocols, &mHandle, &activeProtocol));
	setIOType(activeProtocol);
	mConnectedState = kConnected;
}

void Card::reconnect(unsigned long share, unsigned long protocols, unsigned long initialization)
{
	assert(mConnectedState != kInitial);

	uint32_t activeProtocol;
	Error::check(::SCardReconnect(mHandle, (uint32_t)share, (uint32_t)protocols,
		(uint32_t)initialization, &activeProtocol));
	setIOType(activeProtocol);
	mConnectedState = kConnected;
}

void Card::disconnect(unsigned long disposition)
{
	if (mConnectedState == kConnected)
	{
		if (mTransactionNestLevel > 0)
		{
			secinfo("pcsc", "%p: disconnect, dropping: %d transactions", this, mTransactionNestLevel);
			mTransactionNestLevel = 0;
		}

		checkReset(::SCardDisconnect(mHandle, (uint32_t)disposition));
		didDisconnect();
		mConnectedState = kInitial;
	}
}

void
Card::checkReset(unsigned int rv)
{
	if (rv == SCARD_W_RESET_CARD)
	{
		secinfo("pcsc", "%p: card reset during pcsc call, we're disconnected", this);
		didDisconnect();
	}
    Error::check(rv);
}

void
Card::didDisconnect()
{
	mConnectedState = kDisconnected;
	mTransactionNestLevel = 0;
}

void
Card::didEnd()
{
}

void
Card::transmit(const unsigned char *pbSendBuffer, size_t cbSendLength,
	unsigned char *pbRecvBuffer, size_t &pcbRecvLength)
{
	if (mConnectedState == kDisconnected)
	{
		secinfo("pcsc", "%p: transmit after disconnect, reconnecting", this);
		reconnect();
	}

	IFDUMPING("pcsc", dump("->", pbSendBuffer, cbSendLength));

	uint32_t tmpRecvLength = (uint32_t)pcbRecvLength;
	checkReset(::SCardTransmit(mHandle, mIOType, pbSendBuffer, (uint32_t)cbSendLength,
		NULL, pbRecvBuffer, &tmpRecvLength));
	pcbRecvLength = tmpRecvLength;
	
	IFDUMPING("pcsc", dump("<-", pbRecvBuffer, pcbRecvLength));
}

void Card::begin()
{
	// Only the first transaction started is sent to PCSC
	if (mTransactionNestLevel == 0)
	{
		if (mConnectedState == kDisconnected)
		{
			secinfo("pcsc", "%p: begin transaction after disconnect, reconnecting", this);
			reconnect();
		}

		checkReset(::SCardBeginTransaction(mHandle));
	}
	mTransactionNestLevel++;
	secinfo("pcsc", "%p begin transaction: %d", this, mTransactionNestLevel);
}

void Card::end(unsigned long disposition)
{
	// Only the last transaction ended is sent to PCSC
	secinfo("pcsc", "%p end transaction: %d", this, mTransactionNestLevel);
	if (disposition == SCARD_RESET_CARD)
	{
		if (mConnectedState == kDisconnected)
		{
			secinfo("pcsc", "%p: end transaction after disconnect, reconnecting to reset card", this);
			reconnect();
		}

		checkReset(::SCardEndTransaction(mHandle, (uint32_t)disposition));
		didDisconnect();
	}
	else if (mTransactionNestLevel > 0)
	{
		--mTransactionNestLevel;
		if (mTransactionNestLevel == 0)
		{
			if (mConnectedState == kDisconnected)
				secinfo("pcsc", "%p: end transaction while disconnected ignored", this);
			else
			{
				checkReset(::SCardEndTransaction(mHandle, (uint32_t)disposition));
				didEnd();
			}
		}
	}
}

void Card::cancel()
{
	end(/*SCARD_RESET_CARD*/);
}

#if defined(DEBUGDUMP)

void
Card::dump(const char *direction, const unsigned char *buffer, size_t length)
{
	Debug::dump("[%02lu]%s:", length, direction); 
    
	for (size_t ix = 0; ix < length; ++ix)
		Debug::dump(" %02x", buffer[ix]);

	Debug::dump("\n");
}

#endif


void Transaction::commitAction()
{
	mCarrier.end(mDisposition);
}


}   // namespace PCSC
}   // namespace Security
