/*
 * Copyright (c) 2008 Apple Inc. All Rights Reserved.
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
// vproc++ - interface to low-level transaction tracking facility
//
#ifndef _H_VPROCPP
#define _H_VPROCPP

#include <vproc.h>


namespace Security {
namespace VProc {


class Transaction {
public:
	Transaction(vproc_t vp = 0) : mVP(vp) { mTransaction = ::vproc_transaction_begin(vp); }
	Transaction(bool activate, vproc_t vp = 0) : mVP(vp)
		{ if (activate) mTransaction = ::vproc_transaction_begin(vp); else mTransaction = 0; }
	~Transaction() { deactivate(); }
	
	// explicit state management
	void activate();
	void deactivate()
		{ if (mTransaction) { ::vproc_transaction_end(mVP, mTransaction); mTransaction = 0; }}
	bool active() const { return mTransaction != 0; }
	
	static size_t debugCount();				// debug use only
	
private:
	vproc_t mVP;
	vproc_transaction_t mTransaction;
};


}	// end namespace VProc
}	// end namespace Security


#endif //_H_VPROCPP
