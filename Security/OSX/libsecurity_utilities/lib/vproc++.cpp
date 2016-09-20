/*
 * Copyright (c) 2008,2011,2013 Apple Inc. All Rights Reserved.
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
// fdsel - select-style file descriptor set management
//
#include "vproc++.h"
#include <assert.h>
#include <security_utilities/debugging.h>
#include <vproc_priv.h>


namespace Security {
namespace VProc {


void Transaction::activate()
{
	assert(!active());
	mTransaction = ::vproc_transaction_begin(mVP);
}


//
// Get the in-process accumulated transaction count.
// Use for debugging only.
//
size_t Transaction::debugCount()
{
	return ::_vproc_transaction_count();
}


}	// end namespace VProc
}	// end namespace Security
