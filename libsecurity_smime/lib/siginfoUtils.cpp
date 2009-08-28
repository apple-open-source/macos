/*
 * Copyright (c) 2008 Apple Inc. All Rights Reserved.
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

/* 
 * siginfoUtils.cpp - private C++ routines for cmssiginfo
 */
 
#include <Security/SecCmsSignerInfo.h>
#include <security_utilities/simpleprefs.h>
#include "cmspriv.h"    /* prototype */

/*
 * RFC 3278 section section 2.1.1 states that the signatureAlgorithm 
 * field contains the full ecdsa-with-SHA1 OID, not plain old ecPublicKey 
 * as would appear in other forms of signed datas. However Microsoft doesn't 
 * do this, it puts ecPublicKey there, and if we put ecdsa-with-SHA1 there, 
 * MS can't verify - presumably because it takes the digest of the digest 
 * before feeding it to ECDSA.
 * We handle this with a preference; default if it's not there is 
 * "Microsoft compatibility mode". 
 */

bool SecCmsMsEcdsaCompatMode()
{
	bool msCompat = true;
	Dictionary *pd = Dictionary::CreateDictionary(kMSCompatibilityDomain, Dictionary::US_User, false);
	if(pd == NULL) {
	    pd = Dictionary::CreateDictionary(kMSCompatibilityDomain, Dictionary::US_System, false);
	}
	if(pd != NULL) {
	    /* 
	     * not present means true, the opposite of getBoolValue(), so we have to see if 
	     * it's there...
	     */
	    if(pd->getValue(kMSCompatibilityMode)) {
			msCompat = pd->getBoolValue(kMSCompatibilityMode);
	    }
	    delete pd;
	}
	return msCompat;
}

