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
// parameters - dynamic parameter retrieval interface
//
#ifndef _H_PARAMETERS
#define _H_PARAMETERS

#include <Security/typedvalue.h>
#include <Security/debugging.h>
#include <vector>
#include <cstdarg>


namespace Security {
namespace Network {


class ParameterSource {
public:
    // Keys are unsigned integers with integrated typing
    typedef uint32 Key;
    typedef GenericValue Value;
    
    enum {
        integerKey = 1,		// int
        stringKey = 2,		// string
        boolKey = 3,		// bool
        dataKey = 4			// ConstData
    };
#	define PARAMKEY(id,type)	((id) << 8 | (Security::Network::ParameterSource::type##Key))
    inline int keyType(Key key) const		{ return key & 0xFF; }

public:
    virtual ~ParameterSource() { }
    
public:
    // core form: this can be virtually overridden
    virtual bool getParams(Key key, Value &value) const = 0;
    
    // convenience form: unwrap to Value base type
    template <class T> bool get(Key key, T &result) const
    {
        TypedValue<T> value;
        if (getParams(key, value)) {
            result = value;
            secdebug("paramsource", "%p key=0x%lx retrieved", this, key);
            return true;
        } else {
            secdebug("paramsource", "%p key=0x%lx not found", this, key);
            return false;
        }
    }

    // convenience form: return value, use default if not found (no failure indication)
    template <class T> T getv(Key key, T value = T()) const
    {
        get(key, value);		// overwrite value if successful
        return value;			// then return it or remaining default
    }
};


//
// A ParameterPointer is a ParameterSource that has an indirection to another
// ParameterSource. The underlying ("base") reference can be changed at will.
// If it is NULL, all lookups fail.
//
class ParameterPointer : public ParameterSource {
public:
    ParameterPointer() : mBase(NULL) { }
    ParameterPointer(ParameterSource *src) : mBase(src) { }

    operator bool () const				{ return mBase; }
    ParameterSource *parameters() const { return mBase; }
    ParameterSource *parameters(ParameterSource *newBase)
    { ParameterSource *old = mBase; mBase = newBase; return old; }
    
    ParameterSource *parameters(ParameterSource &newBase)
    { return parameters(&newBase); }

    bool getParams(Key key, Value &value) const
    { return mBase && mBase->getParams(key, value); }

private:
    ParameterSource *mBase;				// where to get it from...
};


//
// Here's an entire (ordered) "stack" of ParameterSources. Just build a vector
// of pointers to ParameterSources, and have them searched in order.
//
class ParameterStack : public ParameterSource, public vector<ParameterSource *> {
public:
    bool getParams(Key key, Value &value) const;
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_PARAMETERS
