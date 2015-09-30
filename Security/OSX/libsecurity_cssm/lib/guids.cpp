/*
 * Copyright (c) 2000-2001,2004,2011,2014 Apple Inc. All Rights Reserved.
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


#include <Security/cssmapple.h>
#include <libkern/OSByteOrder.h>

// {87191ca0-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidCssm =
{
	OSSwapHostToBigConstInt32(0x87191ca0),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca1-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleFileDL =
{
	OSSwapHostToBigConstInt32(0x87191ca1),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca2-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleCSP =
{
	OSSwapHostToBigConstInt32(0x87191ca2),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca3-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleCSPDL =
{
	OSSwapHostToBigConstInt32(0x87191ca3), 
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca4-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleX509CL =
{
	OSSwapHostToBigConstInt32(0x87191ca4),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca5-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleX509TP =
{
	OSSwapHostToBigConstInt32(0x87191ca5),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca6-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleLDAPDL =
{
	OSSwapHostToBigConstInt32(0x87191ca6),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca7-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleDotMacTP =
{
	OSSwapHostToBigConstInt32(0x87191ca7),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// 87191ca8-0fc9-11d4-849a000502b52122
const CSSM_GUID gGuidAppleSdCSPDL =
{
	OSSwapHostToBigConstInt32(0x87191ca8),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};

// {87191ca9-0fc9-11d4-849a-000502b52122}
const CSSM_GUID gGuidAppleDotMacDL =
{
	OSSwapHostToBigConstInt32(0x87191ca9),
	OSSwapHostToBigConstInt16(0x0fc9),
	OSSwapHostToBigConstInt16(0x11d4),
	{ 0x84, 0x9a, 0x00, 0x05, 0x02, 0xb5, 0x21, 0x22 }
};
