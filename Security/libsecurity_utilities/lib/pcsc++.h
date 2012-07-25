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
// NOTE: TO BE MOVED TO security_utilities LAYER.
//
#ifndef _H_PCSC_PP
#define _H_PCSC_PP

#include <security_utilities/utilities.h>
#include <security_utilities/errors.h>
#include <security_utilities/transactions.h>
#include <security_utilities/debugging.h>
#include <PCSC/winscard.h>
#include <vector>
#include <string>

#include <cstdio>


namespace Security {
namespace PCSC {


//
// PCSC-domain error exceptions
//
class Error : public CommonError {
public:
	Error(unsigned long err);

    const unsigned long error;
	OSStatus osStatus() const;
	int unixError() const;
	const char *what () const throw ();
	
	static void check(unsigned long err) { if (err != SCARD_S_SUCCESS) throwMe(err); }
	static void throwMe(unsigned long err);
};


//
// A PODWrapper for the PCSC READERSTATE structure
//
class ReaderState : public PodWrapper<ReaderState, SCARD_READERSTATE> {
public:
	void set(const char *name, unsigned long known = SCARD_STATE_UNAWARE);
	
	const char *name() const	{ return szReader; }
	void name(const char *s)	{ szReader = s; }

	unsigned long lastKnown() const { return dwCurrentState; }
	void lastKnown(unsigned long s);

	unsigned long state() const { return dwEventState; }
	bool state(unsigned long it) const { return state() & it; }
	bool changed() const		{ return state(SCARD_STATE_CHANGED); }
	
	template <class T>
	T * &userData() { return reinterpret_cast<T * &>(pvUserData); }
	
	// DataOid access to the ATR data
	const void *data() const { return rgbAtr; }
	size_t length() const { return cbAtr; }
	void setATR(const void *atr, size_t size);
	
	IFDUMP(void dump());
};


//
// A Session represents the entire process state for the PCSC protocol
//
class Session {
	friend class Card;
public:
	Session();
	virtual ~Session();

	void open();
	void close();
	bool isOpen() const { return mIsOpen; }
	
	void listReaders(vector<string> &readers, const char *groups = NULL);
	
	void statusChange(ReaderState *readers, unsigned int nReaders, long timeout = 0);
	void statusChange(ReaderState &reader, long timeout = 0)
	{ return statusChange(&reader, 1, timeout); }
	void statusChange(vector<ReaderState> &readers, long timeout = 0)
	{ return statusChange(&readers[0], readers.size(), timeout); }
	

private:
	bool check(long rc);
	
private:
	bool mIsOpen;
	SCARDCONTEXT mContext;
	std::vector<char> mReaderBuffer;
};


//
// A Card represents a PCSC-managed card slot
//
class Card {
public:
	static const unsigned long defaultProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
	
	Card();
	virtual ~Card();

	void connect(Session &session, const char *reader,
		unsigned long share = SCARD_SHARE_SHARED,
		unsigned long protocols = defaultProtocols);
	void reconnect(unsigned long share = SCARD_SHARE_SHARED,
		unsigned long protocols = defaultProtocols,
		unsigned long initialization = SCARD_LEAVE_CARD);
	void disconnect(unsigned long disposition = SCARD_LEAVE_CARD);
	virtual void didDisconnect();
	virtual void didEnd();

	void checkReset(unsigned int rv);
	bool isConnected() const { return mConnectedState == kConnected; }
	bool isInTransaction() const { return mTransactionNestLevel > 0; }

	void transmit(const unsigned char *pbSendBuffer, size_t cbSendLength,
		unsigned char *pbRecvBuffer, size_t &pcbRecvLength);

	// primitive transaction interface
	void begin();
	void end(unsigned long disposition = SCARD_LEAVE_CARD);
	void cancel();

protected:
	void setIOType(unsigned long activeProtocol);

	IFDUMP(void dump(const char *direction, const unsigned char *buffer, size_t length);)
	
private:
	enum
	{
		kInitial,
		kConnected,
		kDisconnected
	} mConnectedState;
		
	int32_t mHandle;
	int mTransactionNestLevel;
	SCARD_IO_REQUEST *mIOType;
};


//
// A PCSC-layer transaction (exclusive sequence of calls)
//
class Transaction : public ManagedTransaction<Card> {
public:
	Transaction(Card &card, Outcome outcome = conditional)
		: ManagedTransaction<Card>(card, outcome), mDisposition(SCARD_LEAVE_CARD) { }
	
	void disposition(unsigned long disp);	// change disposition on successful outcome

protected:
	void commitAction();

private:
	unsigned long mDisposition;				// disposition on success
};


}   // namespce PCSC
}   // namespace Security


#endif //_H_PCSC_PP
