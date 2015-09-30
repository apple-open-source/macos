/*
 * Copyright (c) 2002-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
 * cssm utilities
 */
#ifndef _H_CSSMENDIAN
#define _H_CSSMENDIAN

#include <security_cdsa_utilities/cssmkey.h>
#include <security_utilities/endian.h>

namespace Security {


//
// Some structs we may want swapped in-place
//
void n2hi(CssmKey::Header &key);
void h2ni(CssmKey::Header &key);

inline void n2hi(CSSM_KEYHEADER &key) { n2hi(CssmKey::Header::overlay (key));}
inline void h2ni(CSSM_KEYHEADER &key) { h2ni(CssmKey::Header::overlay (key));}

}	// end namespace Security


#endif //_H_CSSMENDIAN

