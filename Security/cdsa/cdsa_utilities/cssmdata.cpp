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
// cssmdata.cpp -- Manager different CssmData types
//
#ifdef __MWERKS__
#define _CPP_CDSA_UTILITIES_CSSMDATA
#endif
#include <Security/cssmdata.h>

#include <Security/utilities.h>


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

CssmGuidData::CssmGuidData(const CSSM_GUID &guid) : CssmData(buffer, sizeof(buffer))
{
	Guid::overlay(guid).toString(buffer);
}

CssmDLPolyData::operator CSSM_DATE () const
{
	assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
	if (mData.Length != 8)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	CSSM_DATE date;
	memcpy(date.Year, mData.Data, 4);
	memcpy(date.Month, mData.Data + 4, 2);
	memcpy(date.Day, mData.Data + 6, 2);
	return date;
}

CssmDLPolyData::operator Guid () const
{
	assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
	if (mData.Length != Guid::stringRepLength + 1)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	return Guid(reinterpret_cast<const char *>(mData.Data));
}
