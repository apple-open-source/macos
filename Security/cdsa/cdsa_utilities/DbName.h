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


#ifndef _DBNAME_H_
#define _DBNAME_H_  1

#include <Security/utilities.h>
#include <Security/walkers.h>
#include <Security/cssmtype.h>
#include <string>

#ifdef _CPP_DBNAME
# pragma export on
#endif

// @@@ Should not use using in headers.
using namespace std;

namespace Security
{

//----------------------------------------------------------------
//typedef struct cssm_net_address {
//    CSSM_NET_ADDRESS_TYPE AddressType;
//    CSSM_DATA Address;
//} CSSM_NET_ADDRESS, *CSSM_NET_ADDRESS_PTR;
//----------------------------------------------------------------

// XXX TODO: Make CssmNetAddress use a factory to constuct netadrress objects based on CSSM_NET_ADDRESS_TYPE!
class CssmNetAddress : public PodWrapper<CssmNetAddress, CSSM_NET_ADDRESS>
{
public:
    // Create a CssmNetAddress wrapper.  Copies inAddress.Data
    CssmNetAddress(CSSM_DB_RECORDTYPE inAddressType, const CssmData &inAddress);
    CssmNetAddress(const CSSM_NET_ADDRESS &other);
    ~CssmNetAddress();
    CSSM_DB_RECORDTYPE addressType() const { return AddressType; }
    const CssmData &address() const { return CssmData::overlay(Address); }
    bool operator <(const CssmNetAddress &other) const
    {
        return AddressType != other.AddressType ? AddressType < other.AddressType : address() < other.address();
    }
};

class DbName
{
public:
    DbName (const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation);
    DbName(const DbName &other);
    DbName &operator =(const DbName &other);
    ~DbName ();
    const string &dbName() const { return mDbName; }
    const CssmNetAddress *dbLocation() const { return mDbLocation; }
    bool operator <(const DbName &other) const
    {
        // If mDbNames are not equal return whether our mDbName is less than others mDbName.
        if (mDbName != other.mDbName)
            return mDbName < other.mDbName;

        // DbNames are equal so check for pointer equality of DbLocations
        if (mDbLocation == other.mDbLocation)
            return false;

        // If either DbLocations is nil the one that is nil is less than the other.
        if (mDbLocation == nil || other.mDbLocation == nil)
            return mDbLocation < other.mDbLocation;

        // Return which mDbLocation is smaller.
        return *mDbLocation < *other.mDbLocation;
    }
	bool operator ==(const DbName &other) const
	{ return (!(*this < other)) && (!(other < *this)); }
	bool operator !=(const DbName &other) const
	{ return *this < other || other < *this; }

private:
    string mDbName;
    CssmNetAddress *mDbLocation;
};


namespace DataWalkers
{

template<class Action>
CssmNetAddress *walk(Action &operate, CssmNetAddress * &addr)
{
    operate(addr);
    walk(operate, addr->Address);
    return addr;
}

} // end namespace DataWalkers

} // end namespace Security

#ifdef _CPP_DBNAME
# pragma export off
#endif

#endif //_DBNAME_H_
