/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
#include "headermap.h"
#include <ctype.h>
#include <assert.h>

using namespace std;

namespace Security {


//
// Given a constant text string, extract the leading substring up to 'end' (or \0),
// canonicalize its case, and store the result for use.
//
HeaderMap::CanonicalKey::CanonicalKey(const char *key, char end)
{
    assert(key && key[0]);	// non-empty
    mValue[0] = toupper(key[0]);
    for (unsigned int n = 1; n < sizeof(mValue) - 1; n++) {
        if (key[n] == end) {
            mValue[n] = '\0';
            return;
        }
        mValue[n] = tolower(key[n]);
    }
    // overflow -- truncate? throw? dynamic allocation? seppuko? :-)
    assert(false);
}


//
// Add an entry
//
void HeaderMap::add(const char *key, const char *value)
{
    add(CanonicalKey(key), value);
}


//
// Given a standard form (Key: value), add its value to the headermap
//
void HeaderMap::add(const char *form)
{
    while (*form && isspace(*form))
        form++;
    if (const char *colon = strchr(form, ':')) {
        CanonicalKey key(form, ':');
        const char *value = colon + 1;
        while (*value && isspace(*value))
            value++;
        add(key, value);
    } else {
        // ignore this
        //@@@ signal an error? how? how bad?
    }
}


//
// Internal add method, given a canonicalized key
//
void HeaderMap::add(const CanonicalKey &key, const char *value)
{
    Map::iterator it = mMap.find(key);
    if (it == mMap.end())
        mMap[key] = value;
    else
        merge(key, mMap[key], value);
}


//
// Locate an entry in a headermap.
// Find returns NULL if not found; [] creates a new entry if needed and returns
// a reference to the value, in good STL tradition.
//
const char *HeaderMap::find(const char *key, const char *defaultValue) const
{
    Map::const_iterator it = mMap.find(CanonicalKey(key));
    return (it == mMap.end()) ? defaultValue : it->second.c_str();
}

string &HeaderMap::operator[] (const char *key)
{
    return mMap[CanonicalKey(key)];
}


//
// The default implementation of merge throws out the old contents and replaces
// them with the new.
//
void HeaderMap::merge(string key, string &old, string newValue)
{
    old = newValue;
}


//
// Collect the entire contents into a single string
// Note that this is NOT exactly what was passed in; certain canonicalizations have
// been done; fields are reordered; and duplicate-header fields have been coalesced.
//@@@ size could be pre-calculated (running counter).
//
string HeaderMap::collect(const char *lineEnding) const
{
    string value;
    for (Map::const_iterator it = mMap.begin(); it != mMap.end(); it++)
        value += it->first + ": " + it->second + lineEnding;
    return value;
}

size_t HeaderMap::collectLength(const char *lineEnding) const
{
    size_t size = 0;
    size_t sepLength = strlen(lineEnding);
    for (Map::const_iterator it = mMap.begin(); it != mMap.end(); it++)
        size += it->first.length() + 2 + it->second.length() + sepLength;
    return size;
}


}	// end namespace Security
