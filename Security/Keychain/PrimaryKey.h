/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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


//
// PrimaryKey.h
//
#ifndef _SECURITY_PRIMARYKEY_H_
#define _SECURITY_PRIMARYKEY_H_

#include <Security/dlclient.h>
#include <Security/Keychains.h>

namespace Security
{

namespace KeychainCore
{

class PrimaryKeyImpl : public CssmDataContainer
{
public:
    PrimaryKeyImpl(const CSSM_DATA &data);
    PrimaryKeyImpl(const CssmClient::DbAttributes &primaryKeyAttrs);
    ~PrimaryKeyImpl() {}

	void putUInt32(uint8 *&p, uint32 value);
	uint32 getUInt32(uint8 *&p, uint32 &left) const;

	CssmClient::DbCursor createCursor(const Keychain &keychain);

	CSSM_DB_RECORDTYPE recordType() const;
private:
};


class PrimaryKey : public RefPointer<PrimaryKeyImpl>
{
public:
    PrimaryKey() {}
    PrimaryKey(PrimaryKeyImpl *impl) : RefPointer<PrimaryKeyImpl>(impl) {}
    PrimaryKey(const CSSM_DATA &data)
	: RefPointer<PrimaryKeyImpl>(new PrimaryKeyImpl(data)) {}
    PrimaryKey(const CssmClient::DbAttributes &primaryKeyAttrs)
	: RefPointer<PrimaryKeyImpl>(new PrimaryKeyImpl(primaryKeyAttrs)) {}

	bool operator <(const PrimaryKey &other) const { return **this < *other; }
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_PRIMARYKEY_H_
