/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// cssmerrno - number-to-string translation for CSSM error codes
//
#ifndef _H_CSSMERRNO
#define _H_CSSMERRNO

#include <Security/utilities.h>
#include <Security/cssmapple.h>		/* for cssmPerror() */
#include <string>

#ifdef _CPP_CSSMERRNO
#pragma export on
#endif

namespace Security
{

string cssmErrorString(CSSM_RETURN error);
string cssmErrorString(const CssmCommonError &error);

} // end namespace Security

#ifdef _CPP_CSSMERRNO
#pragma export off
#endif

#endif //_H_CSSMERRNO
