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
 *  PKCS11Object.cpp
 *  TokendMuscle
 */

#include "PKCS11Object.h"

#include <security_utilities/debugging.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmerr.h>

#if defined(DEBUGDUMP)
#include "cryptoki.h"
#include "pkcs11.h"
#endif /* !defined(DEBUGDUMP) */

namespace Tokend
{

PKCS11Object::PKCS11Object(const void *inData, size_t inSize)
{
	const PKCS11ObjectHeader *object =
		reinterpret_cast<const PKCS11ObjectHeader *>(inData);
	if (inSize < sizeof(PKCS11ObjectHeader) || !object
		|| inSize < (object->size() + sizeof(PKCS11ObjectHeader)))
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

	size_t objectSize = object->size();
	const uint8_t *data = object->data();
	for (size_t bytesRead = 0; bytesRead < objectSize;)
	{
		const PKCS11Attribute *attribute =
			reinterpret_cast<const PKCS11Attribute *>(&data[bytesRead]);
		IFDUMPING("pkcs11", debugDump(*attribute));
		mAttributeMap.insert(pair<uint32_t,
			const PKCS11Attribute *>(attribute->attributeId(), attribute));
		bytesRead += sizeof(PKCS11Attribute) + attribute->size();
	}
}

const PKCS11Object::PKCS11Attribute *
PKCS11Object::attribute(uint32_t attributeId) const
{
	AttributeMap::const_iterator it = mAttributeMap.find(attributeId);
	if (it == mAttributeMap.end())
	{
		secdebug("pkcs11", "pkcs11 attribute: %08X not found", attributeId);
		return NULL;
	}

	secdebug("pkcs11-d", "accessing pkcs11 attribute: %08X size: %lu",
		attributeId, it->second->size());
	return it->second;
}

bool PKCS11Object::attributeValueAsBool(uint32_t attributeId) const
{
	const PKCS11Attribute *attr = attribute(attributeId);
	if (!attr)
		return false;

	if (attr->size() != 1)
	{
		secdebug("pkcs11",
			"attributeValueAsBool: pkcs11 attribute: %08X size: %lu",
			attributeId, attr->size());
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	}

	return *attr->data() != 0;
}

uint32_t PKCS11Object::attributeValueAsUint32(uint32_t attributeId) const
{
	const PKCS11Attribute *attr = attribute(attributeId);
	if (!attr)
		CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);

	if (attr->size() != 4)
	{
		secdebug("pkcs11",
			"attributeValueAsUint32: pkcs11 attribute: %08X size: %lu",
			attributeId, attr->size());
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	}

	const uint8_t *data = attr->data();
	return (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3]; 
}

void PKCS11Object::attributeValueAsData(uint32_t attributeId,
	const uint8_t *&data, size_t &size) const
{
	const PKCS11Attribute *attr = attribute(attributeId);
	if (!attr)
		CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);

	size = attr->size();
	data = attr->data();
}

#if defined(DEBUGDUMP)
void PKCS11Object::debugDump(const PKCS11Attribute &attribute)
{
	Debug::dump("found pkcs11 attribute: %s size: %lu ",
		attributeName(attribute.attributeId()), attribute.size());
	Debug::dumpData(attribute.data(), attribute.size());
	Debug::dump("\n");
}

const char *PKCS11Object::attributeName(uint32_t attributeId)
{
	static char buffer[20];

	switch (attributeId)
	{
	case CKA_CLASS: return "CLASS";
	case CKA_TOKEN: return "TOKEN";
	case CKA_PRIVATE: return "PRIVATE";
	case CKA_LABEL: return "LABEL";
	case CKA_APPLICATION: return "APPLICATION";
	case CKA_VALUE: return "VALUE";
	case CKA_OBJECT_ID: return "OBJECT_ID";
	case CKA_CERTIFICATE_TYPE: return "CERTIFICATE_TYPE";
	case CKA_ISSUER: return "ISSUER";
	case CKA_SERIAL_NUMBER: return "SERIAL_NUMBER";
	case CKA_AC_ISSUER: return "AC_ISSUER";
	case CKA_OWNER: return "OWNER";
	case CKA_ATTR_TYPES: return "ATTR_TYPES";
	case CKA_TRUSTED: return "TRUSTED";
	case CKA_KEY_TYPE: return "KEY_TYPE";
	case CKA_SUBJECT: return "SUBJECT";
	case CKA_ID: return "ID";
	case CKA_SENSITIVE: return "SENSITIVE";
	case CKA_ENCRYPT: return "ENCRYPT";
	case CKA_DECRYPT: return "DECRYPT";
	case CKA_WRAP: return "WRAP";
	case CKA_UNWRAP: return "UNWRAP";
	case CKA_SIGN: return "SIGN";
	case CKA_SIGN_RECOVER: return "SIGN_RECOVER";
	case CKA_VERIFY: return "VERIFY";
	case CKA_VERIFY_RECOVER: return "VERIFY_RECOVER";
	case CKA_DERIVE: return "DERIVE";
	case CKA_START_DATE: return "START_DATE";
	case CKA_END_DATE: return "END_DATE";
	case CKA_MODULUS: return "MODULUS";
	case CKA_MODULUS_BITS: return "MODULUS_BITS";
	case CKA_PUBLIC_EXPONENT: return "PUBLIC_EXPONENT";
	case CKA_PRIVATE_EXPONENT: return "PRIVATE_EXPONENT";
	case CKA_PRIME_1: return "PRIME_1";
	case CKA_PRIME_2: return "PRIME_2";
	case CKA_EXPONENT_1: return "EXPONENT_1";
	case CKA_EXPONENT_2: return "EXPONENT_2";
	case CKA_COEFFICIENT: return "COEFFICIENT";
	case CKA_PRIME: return "PRIME";
	case CKA_SUBPRIME: return "SUBPRIME";
	case CKA_BASE: return "BASE";
	case CKA_PRIME_BITS: return "PRIME_BITS";
	case CKA_SUB_PRIME_BITS: return "SUB_PRIME_BITS";
	case CKA_VALUE_BITS: return "VALUE_BITS";
	case CKA_VALUE_LEN: return "VALUE_LEN";
	case CKA_EXTRACTABLE: return "EXTRACTABLE";
	case CKA_LOCAL: return "LOCAL";
	case CKA_NEVER_EXTRACTABLE: return "NEVER_EXTRACTABLE";
	case CKA_ALWAYS_SENSITIVE: return "ALWAYS_SENSITIVE";
	case CKA_KEY_GEN_MECHANISM: return "KEY_GEN_MECHANISM";
	case CKA_MODIFIABLE: return "MODIFIABLE";
	case CKA_EC_PARAMS: return "EC_PARAMS";
	case CKA_EC_POINT: return "EC_POINT";
	case CKA_SECONDARY_AUTH: return "SECONDARY_AUTH";
	case CKA_AUTH_PIN_FLAGS: return "AUTH_PIN_FLAGS";
	case CKA_HW_FEATURE_TYPE: return "HW_FEATURE_TYPE";
	case CKA_RESET_ON_INIT: return "RESET_ON_INIT";
	case CKA_HAS_RESET: return "HAS_RESET";
	case CKA_VENDOR_DEFINED: return "VENDOR_DEFINED";
	default:
		snprintf(buffer, sizeof(buffer), "unknown(%0x08X)", attributeId);
		return buffer;
	}
}
#endif /* !defined(DEBUGDUMP) */


}	// end namespace Tokend

