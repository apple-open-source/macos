/*
 * Copyright (c) 2000-2001,2004 Apple Computer, Inc. All Rights Reserved.
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
// PrimaryKey.cpp
//

#include "PrimaryKey.h"

using namespace KeychainCore;
using namespace CssmClient;


PrimaryKeyImpl::PrimaryKeyImpl(const CSSM_DATA &data)
: CssmDataContainer(data.Data, data.Length), mMutex(Mutex::recursive)
{

//@@@ do bounds checking here, throw if invalid

}

PrimaryKeyImpl::PrimaryKeyImpl(const DbAttributes &primaryKeyAttrs) : mMutex(Mutex::recursive)
{
	Length = sizeof(uint32);
	for (uint32 ix = 0; ix < primaryKeyAttrs.size(); ++ix)
	{
		if (primaryKeyAttrs.at(ix).size() == 0)
			MacOSError::throwMe(errSecInvalidKeychain);

		Length += sizeof(uint32) + primaryKeyAttrs.at(ix).Value[0].Length;
	}

	// Careful with exceptions
	Data = mAllocator.alloc<uint8>(Length);
	uint8 *p = Data;

	putUInt32(p, primaryKeyAttrs.recordType());
	for (uint32 ix = 0; ix < primaryKeyAttrs.size(); ++ix)
	{
		uint32 len = primaryKeyAttrs.at(ix).Value[0].Length;
		putUInt32(p, len);
		memcpy(p, primaryKeyAttrs.at(ix).Value[0].Data, len);
		p += len;
	}
}

CssmClient::DbCursor
PrimaryKeyImpl::createCursor(const Keychain &keychain) 
{
	StLock<Mutex>_(mMutex);
	DbCursor cursor(keychain->database());

	// @@@ Set up cursor to find item with this.
	uint8 *p = Data;
	uint32 left = Length;
	if (left < sizeof(*p))
		MacOSError::throwMe(errSecNoSuchAttr); // XXX Not really but whatever.

	CSSM_DB_RECORDTYPE rt = getUInt32(p, left);
	const CssmAutoDbRecordAttributeInfo &infos = keychain->primaryKeyInfosFor(rt);

	cursor->recordType(rt);
	cursor->conjunctive(CSSM_DB_AND);
	for (uint32 ix = 0; ix < infos.size(); ++ix)
	{
		uint32 len = getUInt32(p, left);

		if (left < len)
			MacOSError::throwMe(errSecNoSuchAttr); // XXX Not really but whatever.

		CssmData value(p, len);
		left -= len;
		p += len;

		cursor->add(CSSM_DB_EQUAL, infos.at(ix), value);
	}

	return cursor;
}


void
PrimaryKeyImpl::putUInt32(uint8 *&p, uint32 value)
{
	*p++ = (value >> 24);
	*p++ = (value >> 16) & 0xff;
	*p++ = (value >> 8) & 0xff;
	*p++ = value & 0xff;
}

uint32
PrimaryKeyImpl::getUInt32(uint8 *&p, uint32 &left) const
{
	if (left < sizeof(uint32))
		MacOSError::throwMe(errSecNoSuchAttr); // XXX Not really but whatever.


	// @@@ Assumes data written in big endian.
	uint32 value = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
	p += sizeof(uint32);
	left -= sizeof(uint32);
	return value;
}



CSSM_DB_RECORDTYPE
PrimaryKeyImpl::recordType() const
{
	uint8 *data = Data;
	uint32 length = Length;
	return getUInt32(data, length);
}
