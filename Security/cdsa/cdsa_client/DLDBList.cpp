/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
    DLDbList.cpp
*/

#include "DLDBList.h"

using namespace CssmClient;

//----------------------------------------------------------------------
//			DLDbList implementation
//----------------------------------------------------------------------

void DLDbList::add(const DLDbIdentifier& dldbIdentifier)	// Adds at end if not in list
{
    for (DLDbList::const_iterator ix=begin();ix!=end();ix++)
        if (*ix==dldbIdentifier)		// already in list
            return;
    push_back(dldbIdentifier);
    changed(true);
}

void DLDbList::remove(const DLDbIdentifier& dldbIdentifier)	// Removes from list
{
    for (DLDbList::iterator ix=begin();ix!=end();ix++)
	if (*ix==dldbIdentifier)		// found in list
	{
		erase(ix);
		changed(true);
		break;
	}
}

void DLDbList::save()
{
}
