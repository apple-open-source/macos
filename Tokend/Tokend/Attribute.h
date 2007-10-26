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
 *  Attribute.h
 *  TokendMuscle
 */

#ifndef _TOKEND_ATTRIBUTE_H_
#define _TOKEND_ATTRIBUTE_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <string>

namespace Tokend
{

class Attribute
{
public:
	Attribute();
	Attribute(const Attribute &attribute);
	Attribute(bool value);
	Attribute(sint32 value);
	Attribute(uint32 value);
	Attribute(const char *value);
	Attribute(const std::string &value);
	Attribute(const void *data, uint32 length);
	Attribute(const CSSM_DATA *datas, uint32 count);
	~Attribute();

	Attribute &operator = (const Attribute &attribute);

	uint32 size() const { return mCount; }
	const CSSM_DATA &operator [](uint32 ix) const { return mValues[ix]; }
	const CSSM_DATA *values() const { return mValues; }

	void getDateValue(CSSM_DATE &date) const;
	uint32 uint32Value() const;
	bool boolValue() const { return uint32Value() != 0; }

private:
	void set(const CSSM_DATA *datas, uint32 count);
	void set(const void *data, uint32 length);

    uint32 mCount;
    CSSM_DATA_PTR mValues;
};

} // end namespace Tokend

#endif /* !_TOKEND_ATTRIBUTE_H_ */

