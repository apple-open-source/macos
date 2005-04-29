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
#include <security_cdsa_utilities/cssmdbname.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_utilities/utilities.h>

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
	: mDbName(inDbName ? inDbName : ""), mDbNameValid(inDbName), mDbLocation(NULL)
{
    if (inDbLocation)
    {
        mDbLocation = new CssmNetAddress(*inDbLocation);
    }
}

DbName::DbName(const DbName &other)
	: mDbName(other.mDbName), mDbNameValid(other.mDbNameValid), mDbLocation(NULL)
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
	mDbNameValid = other.mDbNameValid;
    if (other.mDbLocation)
    {
        mDbLocation = new CssmNetAddress(*other.mDbLocation);
    }

	return *this;
}

DbName::~DbName()
{
	delete mDbLocation;
}
