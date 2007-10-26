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
 *  PKCS11Object.h
 *  TokendMuscle
 */

#ifndef _TOKEND_PKCS11OBJECT_H_
#define _TOKEND_PKCS11OBJECT_H_

#include <stdint.h>
#include <map>
#include <security_utilities/debugging.h>

namespace Tokend
{

// This object doesn't copy it's data.  It's assumed that the data will live at
// least as long as this object does.
class PKCS11Object
{
public:
	PKCS11Object(const void *inData, size_t inSize);

	bool attributeValueAsBool(uint32_t attributeId) const;
	uint32_t attributeValueAsUint32(uint32_t attributeId) const;
	void PKCS11Object::attributeValueAsData(uint32_t attributeId,
		const uint8_t *&data, size_t &size) const;

private:
	struct PKCS11ObjectHeader
	{
		uint8_t oh_type;
		uint8_t oh_id[2];
		uint8_t oh_next_id[2];
		uint8_t oa_size[2];
		uint8_t oh_data[0];

		size_t size() const { return (oa_size[0] << 8) + oa_size[1]; }
		const uint8_t *data() const { return oh_data; }
	};

	struct PKCS11Attribute
	{
		uint8_t oa_id[4];  // big endian attribute type
		uint8_t oa_size[2]; // big endian attribute length
		uint8_t oa_data[0];

		uint32_t attributeId() const { return (oa_id[0] << 24)
			+ (oa_id[1] << 16) + (oa_id[2] << 8) + oa_id[3]; }
		size_t size() const { return (oa_size[0] << 8) + oa_size[1]; }
		const uint8_t *data() const { return oa_data; }
	};

	const PKCS11Attribute *attribute(uint32_t attributeId) const;

#if defined(DEBUGDUMP)
	void debugDump(const PKCS11Attribute &attribute);
	static const char *attributeName(uint32_t attributeId);
#endif /* !defined(DEBUGDUMP) */

	typedef std::map<uint32_t, const PKCS11Attribute *> AttributeMap;
	AttributeMap mAttributeMap;
};


} // end namespace Tokend

#endif /* !_TOKEND_PKCS11OBJECT_H_ */

