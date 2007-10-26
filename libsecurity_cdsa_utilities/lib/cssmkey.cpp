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
// PODWrapper for CssmKey and related types
//
#include <security_cdsa_utilities/cssmkey.h>


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
