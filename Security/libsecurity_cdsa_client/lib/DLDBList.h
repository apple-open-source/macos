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
    DLDbList.h
    
    This implements a vector of DLDbIdentifiers. A DLDbIdentifier contains all of the
    information needed to find a particular DB within a particular DL. This file
    does not depend on CoreFoundation but does depend on CDSA headers.
*/

#ifndef _H_CDSA_CLIENT_DLDBLIST
#define _H_CDSA_CLIENT_DLDBLIST  1

#include <security_cdsa_utilities/cssmdb.h>
#include <security_utilities/refcount.h>
#include <vector>

namespace Security
{

namespace CssmClient
{

//-------------------------------------------------------------------------------------
//
//			Lists of DL/DBs
//
//-------------------------------------------------------------------------------------


//
// DLDbList
//
class DLDbList : public vector<DLDbIdentifier>
{
public:
    DLDbList() : mChanged(false) {}
    virtual ~DLDbList() {}
    
    // API
    virtual void add(const DLDbIdentifier& dldbIdentifier);		// Adds at end if not in list
    virtual void remove(const DLDbIdentifier& dldbIdentifier);	// Removes from list
    virtual void save();

    bool hasChanged() const { return mChanged; }

protected:
    void changed(bool hasChanged) { mChanged=hasChanged; }
    
private:
    bool mChanged;
};

}; // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_DLDBLIST
