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
// url - URL object with decomposition
//
// This is a wrapper around CoreFoundation CFURL objects,
// without any attempt at re-interpretation (other than cleaning
// up the obvious CFishness).
//
#ifndef _H_URL
#define _H_URL

#include <string>
#include "ip++.h"


struct __CFURL;
using IPPlusPlus::IPPort;


namespace Security {
namespace Network {


//
// A thin encapsulation of a URL
//
class URL {
public:
    URL();
    URL(const char *url);
    URL(const char *url, const URL &base);
    ~URL();
    
    operator string() const;
    
    string scheme() const;
    string host() const;
    IPPort port(IPPort defaultPort = 0) const;
    string path() const;
    string resourceSpec() const;
    string fullPath() const;
    
    string username() const;
    string password() const;
    string basename() const;
    string extension() const;
    
    void recreateURL(const char* url);
    
private:
    const __CFURL *ref;
};


}	// end namespace Network
}	// end namespace Security


#endif /* _H_URL */
