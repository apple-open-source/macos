/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
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
 *  PIVCCC.cpp
 *  TokendPIV
 */

#include "PIVCCC.h"
#include "PIVToken.h"
#include "PIVError.h"

#include "TLV.h"

PIVCCC::PIVCCC(const byte_string &data) throw(PIVError)
{
	/* Upon construction, parse the input data */
	parse(data);
}

PIVCCC::~PIVCCC()
{
}

void PIVCCC::parse(const byte_string &data) throw(PIVError)
{
	/*
		Sample CCC block
		
		53 44 F0 15 A0 00 00 03 08 01 02 20 50 50 00 11 07 00 00 83 58 00 00 
		83 58 F1 01 21 F2 01 21 F3 00 F4 01 00 F5 01 10 F6 11 00 00 00 00 00 
		00 00 00 00 00 00 00 00 00 00 00 00 F7 00 FA 00 FB 00 FC 00 FD 00 FE 00 90 00
	*/
	// Parse the CCC as a TLV
	TLV_ref tlv;
	try {
		tlv = TLV::parse(data);
	} catch (std::runtime_error &e) {
		PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	}
	// Check that the return-data tag is correct
	if(tlv->getTag().size() != 1 || tlv->getTag()[0] != PIV_GETDATA_RESPONSE_TAG)
		PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);

	// Iterate over the TLV's contained values to check for desired/invalid values
	TLVList list = tlv->getInnerValues();
	for(TLVList::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
		// No known CCC tags of > 1 byte
		if((*iter)->getTag().size() != 1)
			PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
		uint8_t tag = (*iter)->getTag()[0];
		switch (tag)
		{
		case PIV_CCC_TAG_CARD_IDENTIFIER:			// 0xF0
			// Store the card identifier value persistently
			mIdentifier_content = (*iter)->getValue();
			mIdentifier.Data = &mIdentifier_content[0];
			mIdentifier.Length = mIdentifier_content.size();
			break;
		case PIV_CCC_TAG_CARD_CONTAINER_VERS:		// 0xF1
		case PIV_CCC_TAG_CARD_GRAMMAR_VERS:			// 0xF2
		case PIV_CCC_TAG_APPS_URL:					// 0xF3
		case PIV_CCC_TAG_IS_PKCS15:					// 0xF4
		case PIV_CCC_TAG_DATA_MODEL_NUMBER:			// 0xF5
		case PIV_CCC_TAG_ACL_RULE_TABLE:			// 0xF6
		case PIV_CCC_TAG_CARD_APDUS:				// 0xF7
		case PIV_CCC_TAG_REDIRECTION:				// 0xFA
		case PIV_CCC_TAG_CAPABILITY_TUPLES:			// 0xFB
		case PIV_CCC_TAG_STATUS_TUPLES:				// 0xFC
		case PIV_CCC_TAG_NEXT_CCC:					// 0xFD
		case PIV_CCC_TAG_EXTENDED_APP_URL:			// 0xE3
		case PIV_CCC_TAG_SEC_OBJECT_BUFFER:			// 0xB4
		case PIV_CCC_TAG_ERROR_DETECTION:			// 0xFE
		case 0:
		case 0xFF:
			// Permit these values, but throw them away
			break;
		default:
			// Unknown data is an error condition
			PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
			break;
		}
	}
}

std::string PIVCCC::hexidentifier() const
{
	return mIdentifier.toHex();		// hex string of binary blob
}
