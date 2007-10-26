/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// cssmdata.cpp -- Manager different CssmData types
//
#include <security_cdsa_utilities/cssmdata.h>
#include <security_utilities/utilities.h>
#include <cstring>


namespace Security {


//
// Comparing raw CSSM_DATA things
//
bool operator == (const CSSM_DATA &d1, const CSSM_DATA &d2)
{
    if (&d1 == &d2)
        return true;	// identical
    if (d1.Length != d2.Length)
        return false;	// can't be
    if (d1.Data == d2.Data)
        return true;	// points to same data
    return !memcmp(d1.Data, d2.Data, d1.Length);
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
// Conversion from/to hex digits.
// This could be separate functions, or Rep templates, but we just hang
// it onto generic CssmData.
//
string CssmData::toHex() const
{
	static const char digits[] = "0123456789abcdef";
	string result;
	unsigned char *p = Data;
	for (uint32 n = 0; n < length(); n++) {
		result.push_back(digits[p[n] >> 4]);
		result.push_back(digits[p[n] & 0xf]);
	}
	return result;
}

static unsigned char hexValue(char c)
{
	static const char digits[] = "0123456789abcdef";
	if (const char *p = strchr(digits, tolower(c)))
		return p - digits;
	else
		return 0;
}

void CssmData::fromHex(const char *hexDigits)
{
	size_t bytes = strlen(hexDigits) / 2;	// (discards malformed odd end)
	length(bytes);	// (will assert if we try to grow it)
	for (size_t n = 0; n < bytes; n++) {
		Data[n] = hexValue(hexDigits[2*n]) << 4 | hexValue(hexDigits[2*n+1]);
	}
}


//
// Managed data objects
//
CssmManagedData::~CssmManagedData()
{ }


//
// CssmOwnedData
//
void CssmOwnedData::set(CssmManagedData &source)
{
    if (source.length() == 0) {					// source is empty
        reset();								// so just clear old data
    } else if (allocator == source.allocator) {	// compatible allocators
        if (referent.data() == source.data()) {	// same data *and* we own it?!
            assert(this == &source);			//  this better *be* me!
        } else {								// different data
            reset();							// give up our old data
            referent = source.release();		// take over source's data
        }
    } else {									// different allocators
        copy(source);							// make a copy with our allocator
        source.reset();							// release source's data
    }
}


//
// CssmAutoData
//
CssmData CssmAutoData::release()
{
    CssmData result = mData;
    mData.clear();
    return result;
}

void CssmAutoData::reset()
{
    allocator.free(mData);
    mData.clear();
}


//
// CssmRemoteData
//
CssmData CssmRemoteData::release()
{
    iOwnTheData = false;
    return referent;
}

void CssmRemoteData::reset()
{
    if (iOwnTheData)
        allocator.free(referent);
    referent.clear();
}


//
// Date stuff
//
CssmDateData::CssmDateData(const CSSM_DATE &date)
: CssmData(buffer, sizeof(buffer))
{
	memcpy(buffer, date.Year, 4);
	memcpy(buffer + 4, date.Month, 2);
	memcpy(buffer + 6, date.Day, 2);
}


CssmData& CssmOwnedData::get() const throw()
{
	return referent;
}

}	// end namespace Security
