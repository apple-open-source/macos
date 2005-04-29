/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
	IFDEBUG(debugDiagnose(this));
}


const char *Error::what() const throw ()
{
	return pcsc_stringify_error(error);
}


void Error::throwMe(unsigned long err)
{
	throw Error(err);
}


OSStatus Error::osStatus() const
{
	return -1;	//@@@ preliminary
}

int Error::unixError() const
{
	return EINVAL;  //@@@ preliminary
}

#if !defined(NDEBUG)
void Error::debugDiagnose(const void *id) const
{
    secdebug("exception", "%p PCSC::Error %s (%ld) osStatus %ld",
		id, pcsc_stringify_error(error), error, osStatus());
}
#endif //NDEBUG


//
// PodWrappers
//
void ReaderState::set(const char *name, unsigned long known)
{
	clearPod();
	szReader = name;
	pvUserData = NULL;
	dwCurrentState = known;
}


void ReaderState::lastKnown(unsigned long s)
{
	// clear out CHANGED and UNAVAILABLE
	dwCurrentState = s & ~(SCARD_STATE_CHANGED | SCARD_STATE_UNAVAILABLE);
}


void ReaderState::setATR(const void *atr, size_t size)
{
	if (size > sizeof(rgbAtr))
		Error::throwMe(SCARD_E_INVALID_ATR);
	memcpy(rgbAtr, atr, size);
	cbAtr = size;
}


#if defined(DEBUGDUMP)

void ReaderState::dump()
{
	Debug::dump("reader(%s) state=0x%lx events=0x%lx",
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
	if (mIsOpen)
		Error::check(SCardReleaseContext(mContext));
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
			secdebug("pcsc", "context opened");
		} catch (const Error &err) {
			if (err.error == SCARD_F_INTERNAL_ERROR)
			{
				secdebug("pcsc", "got internal error; assuming pcscd absent; context not ready");
				return;
			}
		}
	}
}


bool Session::check(long rc)
{
	switch (rc) {
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
	mReaderBuffer.resize(1000); //@@@ preliminary hack
	unsigned long size = mReaderBuffer.size();
	if (check(::SCardListReaders(mContext, groups, &mReaderBuffer[0], &size)))
		decode(readers, mReaderBuffer, size);
	else
		readers.clear();	// treat as success (returning zero readers)
}


//
// Reader status check
//
void Session::statusChange(ReaderState *readers, unsigned int nReaders, long timeout)
{
	if (nReaders == 0)
		return; // no readers, no foul
	check(::SCardGetStatusChange(mContext, timeout, readers, nReaders));
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
	DWORD activeProtocol;
	Error::check(::SCardConnect(session.mContext,
		reader, share, protocols, &mHandle, &activeProtocol));
	setIOType(activeProtocol);
	mConnectedState = kConnected;
}

void Card::reconnect(unsigned long share, unsigned long protocols, unsigned long initialization)
{
	assert(mConnectedState != kInitial);

	DWORD activeProtocol;
	Error::check(::SCardReconnect(mHandle, share, protocols,
		initialization, &activeProtocol));
	setIOType(activeProtocol);
	mConnectedState = kConnected;
}

void Card::disconnect(unsigned long disposition)
{
	if (mConnectedState == kConnected)
	{
		if (mTransactionNestLevel > 0)
		{
			secdebug("pcsc", "%p: disconnect, dropping: %d transactions", this, mTransactionNestLevel);
			mTransactionNestLevel = 0;
		}

		checkReset(::SCardDisconnect(mHandle, disposition));
		didDisconnect();
		mConnectedState = kInitial;
	}
}

void
Card::checkReset(unsigned int rv)
{
	if (rv == SCARD_W_RESET_CARD)
	{
		secdebug("pcsc", "%p: card reset during pcsc call, we're disconnected", this);
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
		secdebug("pcsc", "%p: transmit after disconnect, reconnecting", this);
		reconnect();
	}

	IFDUMPING("pcsc", dump("->", pbSendBuffer, cbSendLength));

	checkReset(::SCardTransmit(mHandle, mIOType, pbSendBuffer, cbSendLength,
		NULL, pbRecvBuffer, &pcbRecvLength));

	IFDUMPING("pcsc", dump("<-", pbRecvBuffer, pcbRecvLength));
}

void Card::begin()
{
	// Only the first transaction started is sent to PCSC
	if (mTransactionNestLevel == 0)
	{
		if (mConnectedState == kDisconnected)
		{
			secdebug("pcsc", "%p: begin transaction after disconnect, reconnecting", this);
			reconnect();
		}

		checkReset(::SCardBeginTransaction(mHandle));
	}
	mTransactionNestLevel++;
	secdebug("pcsc", "%p begin transaction: %d", this, mTransactionNestLevel);
}

void Card::end(unsigned long disposition)
{
	// Only the last transaction ended is sent to PCSC
	secdebug("pcsc", "%p end transaction: %d", this, mTransactionNestLevel);
	if (disposition == SCARD_RESET_CARD)
	{
		if (mConnectedState == kDisconnected)
		{
			secdebug("pcsc", "%p: end transaction after disconnect, reconnecting to reset card", this);
			reconnect();
		}

		checkReset(::SCardEndTransaction(mHandle, disposition));
		didDisconnect();
	}
	else if (mTransactionNestLevel > 0)
	{
		--mTransactionNestLevel;
		if (mTransactionNestLevel == 0)
		{
			if (mConnectedState == kDisconnected)
				secdebug("pcsc", "%p: end transaction while disconnected ignored", this);
			else
			{
				checkReset(::SCardEndTransaction(mHandle, disposition));
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
