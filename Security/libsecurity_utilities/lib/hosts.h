/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// hosts - value-semantics host identifier class
//
// @@@ use vector instead of set to preserve resolver-generated order?
// @@@ preliminary implementation: at the very least, there'll be more subclasses (deferred, etc.)
//
#ifndef _H_HOSTS
#define _H_HOSTS

#include "ip++.h"
#include <security_utilities/refcount.h>
#include <set>


namespace Security {
namespace IPPlusPlus {


//
// Host identities
//
class Host {
public:
    Host(const char *form);
    Host() { }

    // equality is defined strongly: same host specification
    bool operator == (const Host &other) const;
    bool operator != (const Host &other) const	{ return !(*this == other); }
    bool operator < (const Host &other) const;	// for STL sorting
    
    // inclusion (<=) is defined semi-weakly: greater subsumes smaller, same access specs
    bool operator <= (const Host &other) const;
    bool operator >= (const Host &other) const	{ return other <= *this; }
    
    string name() const					{ return mSpec->name(); }
    set<IPAddress> addresses() const	{ return mSpec->addresses(); }

public:
    class Spec : public RefCount {
    public:
        virtual ~Spec() { }
        
        virtual set<IPAddress> addresses() const = 0;
        virtual string name() const = 0;
    
    private:
    };

private:
    RefPointer<Spec> mSpec;
};

}	// end namespace IPPlusPlus
}	// end namespace Security


#endif /* _H_HOSTS */
