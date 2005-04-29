/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// process - track a single client process and its belongings
//
#include "flippers.h"
#include <security_utilities/memutils.h>

using namespace LowLevelMemoryUtilities;


namespace Flippers {


//
// Automatically generated flippers
//
#include "flip_gen.cpp"


//
// The raw byte reversal flipper
//
void flip(void *addr, size_t size)
{
	assert(size > 1 && (size % 2 == 0));
	Byte *word = reinterpret_cast<uint8 *>(addr);
	for (size_t n = 0; n < size/2; n++) {
		Byte b = word[n];
		word[n] = word[size-1-n];
		word[size-1-n] = b;
	}
}


//
// Basic flippers
//
void flip(uint32 &obj)		{ flip(&obj, sizeof(obj)); }
void flip(uint16 &obj)		{ flip(&obj, sizeof(obj)); }
void flip(sint32 &obj)		{ flip(&obj, sizeof(obj)); }
void flip(sint16 &obj)		{ flip(&obj, sizeof(obj)); }


//
// Flip a context attribute. This is heavily polymorphic.
//
void flip(CSSM_CONTEXT_ATTRIBUTE &obj)
{
	flip(obj.AttributeType);
	flip(obj.AttributeLength);
	switch (obj.AttributeType & CSSM_ATTRIBUTE_TYPE_MASK) {
	case CSSM_ATTRIBUTE_DATA_UINT32:
		flip(obj.Attribute.Uint32);
		break;
	// all other alternatives are handled by CSSM_CONTEXT_ATTRIBUTE's walker
	default:
		break;
	}
}


}	// end namespace Flippers
