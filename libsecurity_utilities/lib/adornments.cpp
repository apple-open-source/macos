/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// adornment - generic attached-storage facility
//
#include "adornments.h"
#include <security_utilities/utilities.h>
#include <security_utilities/debugging.h>


namespace Security {


//
// Adornment needs a virtual destructor for safe deletion.
//
Adornment::~Adornment()
{ }


//
// Adornable deletes all pointed-to objects when it dies.
//
Adornable::~Adornable()
{
	clearAdornments();
}


//
// Primitive (non-template) adornment operations
//
Adornment *Adornable::getAdornment(Key key) const
{
	if (mAdornments) {
		AdornmentMap::const_iterator it = mAdornments->find(key);
		return (it == mAdornments->end()) ? NULL : it->second;
	} else
		return NULL;	// nada
}

void Adornable::setAdornment(Key key, Adornment *ad)
{
	Adornment *&slot = adornmentSlot(key);
	delete slot;
	slot = ad;
}

Adornment *Adornable::swapAdornment(Key key, Adornment *ad)
{
	std::swap(ad, adornmentSlot(key));
	return ad;
}

Adornment *&Adornable::adornmentSlot(Key key)
{
	if (!mAdornments)
		mAdornments = new AdornmentMap;
	return (*mAdornments)[key];
}

void Adornable::clearAdornments()
{
	if (mAdornments) {
		for_each_map_delete(mAdornments->begin(), mAdornments->end());
		delete mAdornments;
		mAdornments = NULL;
	}
}

}	// end namespace Security
