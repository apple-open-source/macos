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


#ifdef __MWERKS__
#define _CPP_DBNAME
#endif
#include <Security/DbName.h>

#include <Security/utilities.h>

CssmNetAddress::CssmNetAddress(CSSM_DB_RECORDTYPE inAddressType, const CssmData &inAddress)
{
    AddressType = inAddressType;
    Address.Length = inAddress.Length;
    if (Address.Length > 0)
    {
        Address.Data = new uint8[Address.Length];
        memcpy (Address.Data, inAddress.Data, Address.Length);
    }
    else
        Address.Data = NULL;
}

CssmNetAddress::CssmNetAddress(const CSSM_NET_ADDRESS &other)
{
    AddressType = other.AddressType;
    Address.Length = other.Address.Length;
    if (Address.Length > 0)
    {
        Address.Data = new uint8[Address.Length];
        memcpy (Address.Data, other.Address.Data, Address.Length);
    }
    else
        Address.Data = NULL;
}

CssmNetAddress::~CssmNetAddress()
{
    if (Address.Length > 0)
        delete Address.Data;
}

DbName::DbName(const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation)
:mDbName(&Required(inDbName)),
mDbLocation(nil)
{
    if (inDbLocation)
    {
        mDbLocation = new CssmNetAddress(*inDbLocation);
    }
}

DbName::DbName(const DbName &other)
:mDbName(other.mDbName),
mDbLocation(nil)
{
    if (other.mDbLocation)
    {
        mDbLocation = new CssmNetAddress(*other.mDbLocation);
    }
}

DbName &
DbName::operator =(const DbName &other)
{
	mDbName = other.mDbName;
    if (other.mDbLocation)
    {
        mDbLocation = new CssmNetAddress(*other.mDbLocation);
    }

	return *this;
}

DbName::~DbName()
{
    if (mDbLocation)
    {
        delete mDbLocation;
    }
}
