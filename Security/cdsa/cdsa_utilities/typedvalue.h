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

#include <Security/utilities.h>
#include <Security/debugging.h>


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
