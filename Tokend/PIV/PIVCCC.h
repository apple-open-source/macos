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
 *  PIVCCC.h
 *  TokendPIV
 */

#ifndef _PIVCCC_H_
#define _PIVCCC_H_

#include "PIVDefines.h"
#include <security_cdsa_utilities/cssmdata.h>
#include <string>

class PIVCCC
{
public:
    PIVCCC();
    virtual ~PIVCCC();

	virtual void set(const CssmData &data);
	virtual void parse();

	const unsigned char *identifier() const { return mIdentifier; }
	std::string hexidentifier() const;
	
protected:

	static const unsigned int maxCCCLength = 300;		// SP800-73-1 says 266, but make a bit larger
	unsigned char *mData;
	
	// Reference: SP 800-73-1 Appendix A
	CssmData mIdentifier;				// 0xF0	Card Identifier
	unsigned char ccversion;			// Capability Container version number
	unsigned char cgversion;
	unsigned char mAppCardURL[128];		// 0xF3	Applications CardURL
	bool pkcs15;						// 0xF4	PKCS#15
	unsigned char datamodelnumber;		// 0xF5	Registered Data Model number
	unsigned char mACLRuleTable[17];	// 0xF6	Access Control Rule Table 

private:
};

#endif /* !_PIVCCC_H_ */
