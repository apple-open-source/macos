/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  MscWrappers.h
 *  TokendMuscle
 */

#ifndef _MSCWRAPPERS_H_
#define _MSCWRAPPERS_H_

#include <PCSC/musclecard.h>
#include <security_utilities/utilities.h>

#ifdef _DEBUG_OSTREAM
	#include <ostream>
#endif

#include <Security/cssmerr.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_cdsa_utilities/cssmerrors.h>

class MscKeyACL : public Security::PodWrapper<MscKeyACL, MSCKeyACL>
{
public:
    MscKeyACL() { }
    MscKeyACL(MSCUShort16 rd, MSCUShort16 wr, MSCUShort16 us) { readPermission = rd; writePermission = wr; usePermission = us; }
    MscKeyACL(MSCUShort16 perm) { readPermission = writePermission = usePermission = perm; }
    
    MSCUShort16 read() const	{ return readPermission; }
    MSCUShort16 write() const	{ return writePermission; }
    MSCUShort16 use() const		{ return usePermission; }
};

class MscObjectACL : public Security::PodWrapper<MscObjectACL, MSCObjectACL>
{
public:
    MscObjectACL() { }
    MscObjectACL(MSCUShort16 rd, MSCUShort16 wr, MSCUShort16 delx) { readPermission = rd; writePermission = wr; deletePermission = delx; }
    MscObjectACL(MSCUShort16 perm) { readPermission = writePermission = deletePermission = perm; }
	MscObjectACL(const MSCObjectACL &rObjectACL) { readPermission = rObjectACL.readPermission; writePermission = rObjectACL.writePermission; deletePermission = rObjectACL.deletePermission; }

    MSCUShort16 read() const	{ return readPermission; }
    MSCUShort16 write() const	{ return writePermission; }
    MSCUShort16 del() const		{ return deletePermission; }
//	operator uint32 () const	{ return effective(); }

#ifdef _DEBUG_OSTREAM
	friend std::ostream& operator << (std::ostream& strm, const MscObjectACL& oa);
#endif
};

class MscKeyPolicy : public Security::PodWrapper<MscKeyPolicy, MSCKeyPolicy>
{
public:
    MscKeyPolicy() { }
    MscKeyPolicy(MSCUShort16 modex, MSCUShort16 dir) { cipherMode = modex; cipherDirection = dir; }
    
    MSCUShort16 mode() const		{ return cipherMode; }
    MSCUShort16 direction() const	{ return cipherDirection; }
};

class MscKeyInfo : public Security::PodWrapper<MscKeyInfo, MSCKeyInfo>
{
public:
	// Note: these memcpy operations also copy keyPartner & keyMapping
	// See Guid in cssmpods.h for template template
    MscKeyInfo() { ::memset(this, 0, sizeof(*this)); }
    MscKeyInfo(const MSCKeyInfo &rKeyInfo) { ::memcpy(this, &rKeyInfo, sizeof(*this)); }

    MscKeyInfo &operator = (const MSCKeyInfo &rKeyInfo)
		{ ::memcpy(this, &rKeyInfo, sizeof(MSCKeyInfo)); return *this; }
    
    MSCUChar8 number() const	{ return keyNum; }
    MSCUChar8 type() const		{ return keyType; }
    MSCULong32 size() const		{ return keySize; }
	MscKeyACL &acl()			{ return MscKeyACL::overlay(keyACL); }
	const MscKeyACL &acl() const	{ return MscKeyACL::overlay(keyACL); }
	MscKeyPolicy &policy()			{ return MscKeyPolicy::overlay(keyPolicy); }
	const MscKeyPolicy &policy() const	{ return MscKeyPolicy::overlay(keyPolicy); }
};

class MscObjectInfo : public Security::PodWrapper<MscObjectInfo, MSCObjectInfo>
{
public:
    MscObjectInfo() { memset(this, 0, sizeof(*this)); }
    MscObjectInfo(const MSCObjectInfo &rObjectInfo) { ::memcpy(this, &rObjectInfo, sizeof(*this)); }

    MscObjectInfo &operator = (const MSCObjectInfo &rObjectInfo)
		{ ::memcpy(this, &rObjectInfo, sizeof(MSCObjectInfo)); return *this; }
    
    const char *objid() const	{ return reinterpret_cast<const char *>(objectID); }
    MSCULong32 size() const		{ return objectSize; }

#ifdef _DEBUG_OSTREAM
	friend std::ostream& operator << (std::ostream& strm, const MscObjectInfo& ee);
#endif
};

class MscTokenInfo : public Security::PodWrapper<MscTokenInfo, MSCTokenInfo>
{
public:
    MscTokenInfo() { memset(this, 0, sizeof(*this)); }
    MscTokenInfo(const MSCTokenInfo &rTokenInfo);
	MscTokenInfo(const SCARD_READERSTATE &readerState);	// An SCARD_READERSTATE is enough info to be able to open a connection

    MscTokenInfo &operator = (const MSCTokenInfo &rTokenInfo);

	const char *tname() const { return tokenName; }
	const char *sname() const { return slotName; }
	const char *provider() const { return svProvider; }
	const unsigned char *tid() const { return reinterpret_cast<const unsigned char *>(tokenId); }
	const char *app() const { return reinterpret_cast<const char *>(tokenApp); }

#ifdef _DEBUG_OSTREAM
	friend std::ostream& operator << (std::ostream& strm, const MscTokenInfo& ti);
#endif
};

class MscStatusInfo : public Security::PodWrapper<MscStatusInfo, MSCStatusInfo>
{
public:
    MscStatusInfo() { memset(this, 0, sizeof(*this)); }
    MscStatusInfo(const MscStatusInfo &rTokenInfo);

    MscStatusInfo &operator = (const MscStatusInfo &rTokenInfo);
	
#ifdef _DEBUG_OSTREAM
	friend std::ostream& operator << (std::ostream& strm, const MscStatusInfo& ti);
#endif
};

#ifdef _DEBUG_OSTREAM
std::ostream& operator << (std::ostream& strm, const MscObjectACL& oa);
std::ostream& operator << (std::ostream& strm, const MscObjectInfo& ee);
std::ostream& operator << (std::ostream& strm, const MscTokenInfo& ti);
std::ostream& operator << (std::ostream& strm, const MscStatusInfo& ti);
#endif

#endif /* !_MSCWRAPPERS_H_ */

