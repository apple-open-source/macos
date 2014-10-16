/*
 * Copyright (c) 2000-2006,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// Miscellaneous CSSM PODWrappers
//
#include <security_cdsa_utilities/cssmpods.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_utilities/endian.h>

//
// GUID <-> string conversions.
// Note that we DO check for {} on input and insist on rigid formatting.
// We don't require a terminating null byte on input, but generate it on output.
//
char *Guid::toString(char buffer[stringRepLength+1]) const
{
    sprintf(buffer, "{%8.8x-%4.4hx-%4.4hx-",
            int(n2h(Data1)), n2h(Data2), n2h(Data3));
    for (int n = 0; n < 2; n++)
        sprintf(buffer + 20 + 2*n, "%2.2hhx", Data4[n]);
	buffer[24] = '-';
    for (int n = 2; n < 8; n++)
        sprintf(buffer + 21 + 2*n, "%2.2hhx", Data4[n]);
    buffer[37] = '}';
    buffer[38] = '\0';
    return buffer;
}

string Guid::toString() const
{
	char buffer[stringRepLength+1];
	return toString(buffer);
}

Guid::Guid(const char *s)
{
	parseGuid(s);
}

Guid::Guid(const string &s)
{
	parseGuid(s.c_str());
}

void Guid::parseGuid(const char *string)
{
	// Arguably, we should be more flexible on input. But exactly what
	// padding rules should we follow, and how should we try to interprete
	// "doubtful" variations? Given that GUIDs are essentially magic
	// cookies, everybody's better off if we just cut-and-paste them
	// around the universe...
	
	// do sanity checking, don't assume that what's passed in makes sense
	if (string == NULL)
	{
        CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
	}
	
	// what follows had better be big enough
	if (strlen(string) < 37) // needed because the code hard codes the length
	{
        CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
	}
	
    int d1;
    uint16 d2, d3;
    if (sscanf(string, "{%8x-%4hx-%4hx-", &d1, &d2, &d3) != 3)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
	Data1 = h2n(uint32(d1));
	Data2 = h2n(d2);
	Data3 = h2n(d3);
	// once, we did not expect the - after byte 2 of Data4
	bool newForm = string[24] == '-';
    for (int n = 0; n < 8; n++) {
        unsigned char dn;
        if (sscanf(string + 20 + 2*n + (newForm && n >= 2), "%2hhx", &dn) != 1)
            CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
        Data4[n] = dn;
    }
	if (string[37 - !newForm] != '}')
		CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
}


CssmGuidData::CssmGuidData(const CSSM_GUID &guid) : CssmData(buffer, sizeof(buffer))
{
	Guid::overlay(guid).toString(buffer);
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
    // make sure we don't crash if we get bad data
    if (&otherUid == 0x0)
    {
        return false;
    }
    
	const CssmSubserviceUid &other = CssmSubserviceUid::overlay(otherUid);
	return subserviceId() == other.subserviceId()
		&& subserviceType() == other.subserviceType()
		&& guid() == other.guid();
}

bool CssmSubserviceUid::operator < (const CSSM_SUBSERVICE_UID &otherUid) const
{
    if (&otherUid == 0x0)
    {
        return false;
    }
    
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
// CryptoData & friends
//
CryptoDataClass::~CryptoDataClass()
{ }

CSSM_RETURN CryptoDataClass::callbackShim(CSSM_DATA *output, void *ctx)
{
	BEGIN_API
	*output = reinterpret_cast<CryptoDataClass *>(ctx)->yield();
	END_API(CSSM)
}
