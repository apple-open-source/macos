/*
 * Copyright (c) 2002-2006 Apple Computer, Inc. All Rights Reserved.
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
// Byte order ("endian-ness") handling
//
#include <security_cdsa_utilities/cssmendian.h>

namespace Security {


void n2hi(CssmKey::Header &header)
{
		header.HeaderVersion = n2h(header.HeaderVersion);
		header.BlobType = n2h(header.BlobType);
		header.Format = n2h(header.Format);
		header.AlgorithmId = n2h(header.AlgorithmId);
		header.KeyClass = n2h(header.KeyClass);
		header.LogicalKeySizeInBits = n2h(header.LogicalKeySizeInBits);
		header.KeyAttr = n2h(header.KeyAttr);
		header.KeyUsage = n2h(header.KeyUsage);
		header.WrapAlgorithmId = n2h(header.WrapAlgorithmId);
		header.WrapMode = n2h(header.WrapMode);
		header.Reserved = n2h(header.Reserved);
}

void h2ni(CssmKey::Header &key)
{
		n2hi(key);
}

}	// end namespace Security
