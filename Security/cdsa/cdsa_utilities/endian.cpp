/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// Byte order ("endian-ness") handling
//
#include <Security/endian.h>

namespace Security {


void n2hi(CssmKey::Header &header)
{
		header.HeaderVersion = n2h(header.HeaderVersion);
		header.CspId.Data1 = n2h(header.CspId.Data1);
		header.CspId.Data2 = n2h(header.CspId.Data2);
		header.CspId.Data3 = n2h(header.CspId.Data3);
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

