/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
// process - track a single client process and its belongings
//
#ifndef _H_FLIPPERS
#define _H_FLIPPERS

#include <Security/endian.h>
#include <Security/debugging.h>

// various types we make flippers for
#include <Security/authorization.h>
#include <Security/cssmlist.h>
#include <Security/cssmcred.h>
#include <Security/cssmacl.h>
#include <Security/cssmaclpod.h>
#include <Security/context.h>
#include <Security/cssmdb.h>


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
