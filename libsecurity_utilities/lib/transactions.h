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
// transactions - generic transaction frame support
//
#ifndef _H_TRANSACTIONS
#define _H_TRANSACTIONS

#include <security_utilities/utilities.h>
#include <security_utilities/debugging.h>


namespace Security {


//
// Implementation base class. Do not use directly.
//
class TransactionBase {
public:
	// what happens if this object gets destroyed?
	enum Outcome {
		successful,						// succeeds as set
		cancelled,						// cancelled (rolled back)
		conditional						// succeeds normally, cancelled on exception
	};
	
public:
    virtual ~TransactionBase();
	
	void outcome(Outcome oc)	{ mOutcome = oc; }
	Outcome outcome() const		{ return mOutcome; }

protected:
	TransactionBase(Outcome outcome) : mOutcome(outcome) { }

	Outcome finalOutcome() const;

private:
	Outcome mOutcome;					// current outcome setting
};


//
// A ManagedTransaction will call methods begin() and end() on the Carrier object
// it belongs to, and manage the "outcome" state and semantics automatically.
// You would usually subclass this, though the class is complete in itself if you
// need nothing else out of your transaction objects.
//
template <class Carrier>
class ManagedTransaction : public TransactionBase {
public:
	ManagedTransaction(Carrier &carrier, Outcome outcome = conditional)
		: TransactionBase(outcome), mCarrier(carrier)
	{
		carrier.begin();
	}
	
	~ManagedTransaction()
	{
		switch (finalOutcome()) {
		case successful:
			this->commitAction();
			break;
		case cancelled:
			this->cancelAction();
			break;
		default:
			assert(false);
			break;
		}
	}

protected:
	virtual void commitAction()		{ mCarrier.end(); }
	virtual void cancelAction()		{ mCarrier.cancel(); }

	Carrier &mCarrier;
};


}	// end namespace Security


#endif //_H_TRANSACTIONS
