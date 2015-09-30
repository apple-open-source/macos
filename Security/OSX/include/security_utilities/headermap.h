/*
 * Copyright (c) 2000-2002,2004,2011,2014 Apple Inc. All Rights Reserved.
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
// headermap - represent Internet-standard headers
//
//@@@ Handle duplicate entries.
//@@@ Be flexible: think HTTP (append with commas) vs. RFC822 (multiple Received: headers etc.)
//
#ifndef _H_HEADERMAP
#define _H_HEADERMAP

#include <string>
#include <map>


namespace Security {


//
// Header-maps
//
class HeaderMap {
    static const int maxKeyLength = 80;
    typedef std::map<std::string, std::string> Map;
public:
    HeaderMap() { }
    virtual ~HeaderMap() { }
    
    virtual void merge(std::string key, std::string &old, std::string newValue);
    
    void add(const char *key, const char *value);
    void add(const char *line);		// Key: value
    void remove(const char *key);

    const char *find(const char *key, const char *def = NULL) const;
    std::string &operator [] (const char *key);
    
    typedef Map::const_iterator ConstIterator;
    ConstIterator begin() const	{ return mMap.begin(); }
    ConstIterator end() const	{ return mMap.end(); }
    
    typedef Map::const_iterator Iterator;
    Iterator begin()			{ return mMap.begin(); }
    Iterator end()				{ return mMap.end(); }    
    
    std::string collect(const char *lineEnding = "\r\n") const;
    size_t collectLength(const char *lineEnding = "\r\n") const;
    
private:
    //
    // In-place case canonicalization of header keys
    //
    struct CanonicalKey {
        CanonicalKey(const char *key, char end = '\0');
        operator const char *() const { return mValue; }
        operator std::string () const { return mValue; }
    private:
        char mValue[maxKeyLength];
    };
    
    void add(const CanonicalKey &key, const char *value);

private:
    Map mMap;						// map of key: value pairs
};


}	// end namespace Security


#endif /* _H_HEADERMAP */
