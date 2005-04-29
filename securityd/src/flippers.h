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
#ifndef _H_FLIPPERS
#define _H_FLIPPERS

#include <security_utilities/endian.h>
#include <security_utilities/debugging.h>

// various types we make flippers for
#include <Security/Authorization.h>
#include <security_cdsa_utilities/cssmlist.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmdb.h>


namespace Flippers {


//
// The default flipper does nothing
//
template <class T>
inline void flip(T &obj)
{ }


//
// It's a bad idea to try to flip a const, so flag that
//
template <class T>
inline void flip(const T &)
{ tryingToFlipAConstWontWork(); }


//
// Basic flippers
//
void flip(uint32 &obj);
void flip(uint16 &obj);
void flip(sint32 &obj);
void flip(sint16 &obj);

template <class Base>
inline void flip(Base * &obj)			{ flip(&obj, sizeof(obj)); }


//
// The raw byte reversal flipper
//
void flip(void *addr, size_t size);


//
// Include automatically generated flipper declarations
//
#include "flip_gen.h"


}	// end namespace flippers


#endif //_H_FLIPPERS
