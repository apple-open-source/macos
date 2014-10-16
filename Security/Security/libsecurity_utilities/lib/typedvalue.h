/*
 * Copyright (c) 2000-2001,2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// typedvalue - type-safe reference transmission of arbitrary types.
//
// This slight-of-hand pair of classes allows arbitrary types to be passed through an API
// without losing C++ type safety. Specifically, the type is encapsulated in C++ polymorphism,
// and can be resolved through the C++ typeinfo/dynamic_cast system in a fairly
// convenient way. This is intended to replace passing (void *) arguments around.
// It is obviously not meant to replace proper class hierarchies in APIs.
//
#ifndef _H_TYPEDVALUE
#define _H_TYPEDVALUE

#include <security_utilities/utilities.h>
#include <security_utilities/debugging.h>


namespace Security {


//
// A GenericValue can represent any (one) type. Note the template assignment
// that works *only* for the type it actually represents. Note particularly that
// no automatic conversions are made (other than to subclass).
//
class GenericValue {
public:
    virtual ~GenericValue();
    
    template <class V>
    void operator = (const V &value);
};


//
// A TypedValue is a particular typed instance of a GenericValue
//
template <class Value>
class TypedValue : public GenericValue {
public:
    TypedValue() : mValue() { }
    TypedValue(const Value &v) : mValue(v) { }
    Value &value()					{ return mValue; }
    operator Value &()				{ return mValue; }
    void operator = (const Value &value) { mValue = value; }
    
private:
    Value mValue;
};


//
// The polymorphic assignment access method
//
template <class Value>
void GenericValue::operator = (const Value &value)
{ safer_cast<TypedValue<Value> &>(*this) = value; }


}	// end namespace Security


#endif //_H_TYPEDVALUE
