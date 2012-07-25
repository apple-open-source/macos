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
 *
 */


//
// C++ gate to "Muscle" smartcard interface layer
//
// Note: This is written to go together with <pcsc++.h>, rather than stand on
// its own. It doesn't represent a "all Muscle" view of the card world.
//
#ifndef _H_MUSCLE_PP
#define _H_MUSCLE_PP

#include <security_utilities/refcount.h>
#include <security_utilities/pcsc++.h>
#include <PCSC/musclecard.h>
#include <set>


namespace Security {
namespace Muscle {


//
// Muscle-domain error exceptions
//
class Error : public CommonError {
public:
	Error(MSC_RV err);

    const MSC_RV error;
	OSStatus osStatus() const;
	int unixError() const;
	const char *what () const throw ();
	
	static void check(MSC_RV err) { if (err != MSC_SUCCESS) throwMe(err); }
	static void throwMe(MSC_RV err);
};


//
// Unified ACLs of the Muscle kind
//
class ACL {
public:
	typedef MSCUShort16 Value;

	ACL(Value write = MSC_AUT_ALL, Value read = MSC_AUT_ALL, Value erase = MSC_AUT_ALL);

	ACL() { mRead = mWrite = mErase = MSC_AUT_ALL; }
	
	operator MSCKeyACL () const;
	operator MSCObjectACL () const;

	Value read() const	{ return mRead; }
	bool read(Value mask) const { return mRead & mask; }
	Value &read()		{ return mRead; }
	Value write() const	{ return mWrite; }
	bool write(Value mask) const { return mWrite & mask; }
	Value &write()		{ return mWrite; }
	Value erase() const { return mErase; }
	bool erase(Value mask) const { return mErase & mask; }
	Value &erase()		{ return mErase; }
	// erase is "use" on keys; they're synonymous
	Value use() const { return mErase; }
	bool use(Value mask) const { return mErase & mask; }
	Value &use()		{ return mErase; }
	
	string form(char ue) const;

private:
	MSCUShort16 mRead;
	MSCUShort16 mWrite;
	MSCUShort16 mErase;
};


//
// Muscle item representations (keys and objects unified, the cheap way)
//
class CardItem : public RefCount {
protected:
	CardItem() { }
	
public:
	virtual ~CardItem();
	
	virtual unsigned size() const = 0;
	virtual const char *name() const = 0;

	virtual const ACL &acl() const = 0;
	virtual ACL &acl() = 0;
	
	virtual void debugDump() = 0;
	
	bool operator < (const CardItem &other) const { return this < &other; }
};

class Key : public CardItem, public MSCKeyInfo {
public:
	Key(const MSCKeyInfo &info);
	
	unsigned id() const				{ return this->keyNum; }
	const char *name() const;
	unsigned type() const			{ return this->keyType; }
	unsigned size() const;
	unsigned mode() const			{ return this->keyPolicy.cipherMode; }
	unsigned operations() const		{ return this->keyPolicy.cipherDirection; }

	const ACL &acl() const;
	ACL &acl();
	
	void debugDump();

private:
	char mKeyName[8];				// made-up name "Kn"
};

class Object : public CardItem, public MSCObjectInfo {
public:
	Object(const MSCObjectInfo &info) : MSCObjectInfo(info) { }

	const char *name() const;
	unsigned size() const;

	const ACL &acl() const;
	ACL &acl();
	
	void debugDump();
};


//
// A Muscle connection to a card.
// This is NOT a PodWrapper (for MSCTokenConnection or anything else).
//
class Transaction;

class Connection : public MSCTokenConnection, public MSCStatusInfo {
public:
	Connection();
	~Connection();
	
	void open(const PCSC::ReaderState &reader, unsigned share = MSC_SHARE_EXCLUSIVE);
	void close();
	
	operator bool () const { return mIsOpen; }
	
	void begin(Transaction *trans = NULL);
	void end(Transaction *trans = NULL);
	Transaction *currentTransaction() const;
	
	typedef set<RefPointer<CardItem> > ItemSet;
	void getItems(ItemSet &items, bool getKeys = true, bool getOthers = true);
	
	void updateStatus();
	
private:
	bool mIsOpen;
	Transaction *mCurrentTransaction;
};


class Transaction {
public:
	Transaction(Connection &con);
	~Transaction();
	
	Connection &connection;
};


}	// namespace Muscle
}	// namespace Security


#endif //_H_MUSCLE_PP
