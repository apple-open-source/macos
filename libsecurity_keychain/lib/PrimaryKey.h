/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// PrimaryKey.h
//
#ifndef _SECURITY_PRIMARYKEY_H_
#define _SECURITY_PRIMARYKEY_H_

#include <security_cdsa_client/dlclient.h>
#include <security_keychain/Keychains.h>

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

protected:
	Mutex mMutex;
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
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_PRIMARYKEY_H_
