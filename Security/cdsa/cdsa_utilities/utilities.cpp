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


//
// Utilities
//
#include <Security/utilities.h>

#include <Security/cssmerrno.h>
#include <Security/debugging.h>
#include <typeinfo>
#include <stdio.h>


//
// The base of the exception hierarchy.
// Note that the debug output here depends on a particular
// implementation feature of gcc; to wit, that the exception object
// is created and then copied (at least once) via its copy constructor.
// If your compiler does not invoke the copy constructor, you won't get
// debug output, but nothing worse should happen.
//
CssmCommonError::CssmCommonError()
	IFDEBUG(: mCarrier(true))
{
}

CssmCommonError::CssmCommonError(const CssmCommonError &source)
{
#if !defined(NDEBUG)
	source.debugDiagnose(this);
	mCarrier = source.mCarrier;
	source.mCarrier = false;
#endif //NDEBUG
}

CssmCommonError::~CssmCommonError() throw ()
{
#if !defined(NDEBUG)
	if (mCarrier)
		secdebug("exception", "%p handled", this);
#endif //NDEBUG
}

OSStatus CssmCommonError::osStatus() const
{ return cssmError(); }

int CssmCommonError::unixError() const
{
	OSStatus err = osStatus();
	
	// embedded UNIX errno values are returned verbatim
	if (err >= errSecErrnoBase && err <= errSecErrnoLimit)
		return err - errSecErrnoBase;
	
	// re-map certain CSSM errors
    switch (err) {
	case CSSM_ERRCODE_MEMORY_ERROR:
		return ENOMEM;
	case CSSMERR_APPLEDL_DISK_FULL:
		return ENOSPC;
	case CSSMERR_APPLEDL_QUOTA_EXCEEDED:
		return EDQUOT;
	case CSSMERR_APPLEDL_FILE_TOO_BIG:
		return EFBIG;
	default:
		// cannot map this to errno space
		return -1;
    }
}

CSSM_RETURN CssmCommonError::cssmError(CSSM_RETURN base) const
{ return CssmError::merge(cssmError(), base); }

// default debugDiagnose gets what it can (virtually)
void CssmCommonError::debugDiagnose(const void *id) const
{
#if !defined(NDEBUG)
    secdebug("exception", "%p %s %s/0x%lx osstatus %ld",
		id,	Debug::typeName(*this).c_str(),
		cssmErrorString(cssmError()).c_str(), cssmError(),
		osStatus());
#endif //NDEBUG
}


//
// CssmError exceptions
//
CssmError::CssmError(CSSM_RETURN err) : error(err) { }

const char *CssmError::what() const throw ()
{ return "CSSM exception"; }

CSSM_RETURN CssmError::cssmError() const { return error; }

OSStatus CssmError::osStatus() const { return error; }

void CssmError::throwMe(CSSM_RETURN err) { throw CssmError(err); }


//
// UnixError exceptions
//
UnixError::UnixError() : error(errno) { }

UnixError::UnixError(int err) : error(err) { }

const char *UnixError::what() const throw ()
{ return "UNIX error exception"; }

CSSM_RETURN UnixError::cssmError() const
{
	// map some UNIX errors to well defined CSSM codes; embed the rest
    switch (error) {
#if defined(ENOMEM)
        case ENOMEM:
            return CSSM_ERRCODE_MEMORY_ERROR;
#endif
#if defined(ENOSPC)
		case ENOSPC:
			return CSSMERR_APPLEDL_DISK_FULL;
#endif
#if defined(EDQUOT)
		case EDQUOT:
			return CSSMERR_APPLEDL_QUOTA_EXCEEDED;
#endif
#if defined(EFBIG)
		case EFBIG:
			return CSSMERR_APPLEDL_FILE_TOO_BIG;
#endif
        default:
            return errSecErrnoBase + error;
    }
}

OSStatus UnixError::osStatus() const
{ return error + errSecErrnoBase; }

int UnixError::unixError() const
{ return error; }

void UnixError::throwMe(int err) { throw UnixError(err); }

// @@@ This is a hack for the Network protocol state machine
UnixError UnixError::make(int err) { return UnixError(err); }

#if !defined(NDEBUG)
void UnixError::debugDiagnose(const void *id) const
{
    secdebug("exception", "%p UnixError %s (%d) osStatus %ld",
		id, strerror(error), error, osStatus());
}
#endif //NDEBUG


//
// MacOSError exceptions
//
MacOSError::MacOSError(int err) : error(err) { }

const char *MacOSError::what() const throw ()
{ return "MacOS error"; }

CSSM_RETURN MacOSError::cssmError() const
{ return error; }	// @@@ eventually...

OSStatus MacOSError::osStatus() const
{ return error; }

void MacOSError::throwMe(int error)
{ throw MacOSError(error); }


//
// Manage CSSM errors
//
CSSM_RETURN CssmError::merge(CSSM_RETURN error, CSSM_RETURN base)
{
    if (0 < error && error < CSSM_ERRORCODE_COMMON_EXTENT) {
        return base + error;
    } else {
        return error;
    }
}


//
// CssmData out of line members
//
string CssmData::toString() const
{
	return data() ?
		string(reinterpret_cast<const char *>(data()), length())
		:
		string();
}


//
// GUID <-> string conversions.
// Note that we DO check for {} on input and insist on rigid formatting.
// We don't require a terminating null byte on input, but generate it on output.
//
char *Guid::toString(char buffer[stringRepLength+1]) const
{
    sprintf(buffer, "{%8.8lx-%4.4x-%4.4x-",
            (unsigned long)Data1, unsigned(Data2), unsigned(Data3));
    for (int n = 0; n < 2; n++)
        sprintf(buffer + 20 + 2*n, "%2.2x", Data4[n]);
	buffer[24] = '-';
    for (int n = 2; n < 8; n++)
        sprintf(buffer + 21 + 2*n, "%2.2x", Data4[n]);
    buffer[37] = '}';
    buffer[38] = '\0';
    return buffer;
}

Guid::Guid(const char *string)
{
	// Arguably, we should be more flexible on input. But exactly what
	// padding rules should we follow, and how should we try to interprete
	// "doubtful" variations? Given that GUIDs are essentially magic
	// cookies, everybody's better off if we just cut-and-paste them
	// around the universe...
    unsigned long d1;
    unsigned int d2, d3;
    if (sscanf(string, "{%lx-%x-%x-", &d1, &d2, &d3) != 3)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
	Data1 = d1;	Data2 = d2;	Data3 = d3;
	// once, we did not expect the - after byte 2 of Data4
	bool newForm = string[24] == '-';
    for (int n = 0; n < 8; n++) {
        unsigned int dn;
        if (sscanf(string + 20 + 2*n + (newForm && n >= 2), "%2x", &dn) != 1)
            CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
        Data4[n] = dn;
    }
	if (string[37 - !newForm] != '}')
		CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
}


//
// CssmSubserviceUids.
// Note that for comparison, we ignore the version field.
// This is not necessarily the Right Choice, but suits certain
// constraints in the Sec* layer. Perhaps we might reconsider
// this after a thorough code review to determine the intended
// (by the standard) semantics and proper use. Yeah, right.
//
CssmSubserviceUid::CssmSubserviceUid(const CSSM_GUID &guid,
	const CSSM_VERSION *version, uint32 subserviceId, CSSM_SERVICE_TYPE subserviceType)
{
	Guid = guid;
	SubserviceId = subserviceId;
	SubserviceType = subserviceType;
	if (version)
		Version = *version;
	else
		Version.Major = Version.Minor = 0;
}


bool CssmSubserviceUid::operator == (const CSSM_SUBSERVICE_UID &otherUid) const
{
	const CssmSubserviceUid &other = CssmSubserviceUid::overlay(otherUid);
	return subserviceId() == other.subserviceId()
		&& subserviceType() == other.subserviceType()
		&& guid() == other.guid();
}

bool CssmSubserviceUid::operator < (const CSSM_SUBSERVICE_UID &otherUid) const
{
	const CssmSubserviceUid &other = CssmSubserviceUid::overlay(otherUid);
	if (subserviceId() < other.subserviceId())
		return true;
	if (subserviceId() > other.subserviceId())
		return false;
	if (subserviceType() < other.subserviceType())
		return true;
	if (subserviceType() > other.subserviceType())
		return false;
	return guid() < other.guid();
}


//
// Methods for the CssmKey class
//
CssmKey::CssmKey(const CSSM_KEY &key)
{
	KeyHeader = key.KeyHeader;
    KeyData = key.KeyData;
}

CssmKey::CssmKey(const CSSM_DATA &keyData)
{
	clearPod();
    KeyData = keyData;
    KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
    KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
    KeyHeader.Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
}

CssmKey::CssmKey(uint32 length, void *data)
{
	clearPod();
	KeyData = CssmData(data, length);
    KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
    KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
    KeyHeader.Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
}

CryptoDataClass::~CryptoDataClass()
{
}

//
// Debug support
//
#if !defined(NDEBUG)

#endif //NDEBUG

