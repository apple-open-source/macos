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


/*
    DLDBListCFPref.h
*/

#ifndef __DLDBLISTCFPREF_H_
#define __DLDBLISTCFPREF_H_

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFPreferences.h>
#include <Security/DLDBList.h>
#include <Security/cssmdb.h>
#include <stdexcept>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFDate.h>


namespace Security
{

class DLDbListCFPref : public CssmClient::DLDbList
{
public:
    DLDbListCFPref(CFStringRef theDLDbListKey=NULL,CFStringRef prefsDomain=NULL);
    ~DLDbListCFPref();
    
    void save();
    CssmClient::DLDbList& list() { return *this; }	// eventually, it should check mod dates of CFPrefs file, etc.
    
    static DLDbIdentifier cfDictionaryRefToDLDbIdentifier(CFDictionaryRef theDict);
    static CFDictionaryRef dlDbIdentifierToCFDictionaryRef(const DLDbIdentifier& dldbIdentifier);
	bool revert(bool force);
	void clearDefaultKeychain();
    
    static string ExpandTildesInPath(const string &inPath);
	static string StripPathStuff(const string &inPath);
    static string AbbreviatedPath(const string &inPath);
    static string HomeDir();

private:
    // Private member variables
    CFStringRef mPrefsDomain;
    CFStringRef mDLDbListKey;

    // Private member functions
    void loadOrCreate();

	CFAbsoluteTime mPrefsTimeStamp;
};

class CCFValue
{
public:
    template <class T>
    T cfref() const { return reinterpret_cast<T>(CFTypeRef(mRef)); }

	CCFValue() {}
	CCFValue(CFTypeRef ref) : mRef(ref) {}
	CCFValue &operator =(CFTypeRef ref) { mRef = ref; return *this; }

    CCFValue &operator = (bool value)
    {
        mRef = value?kCFBooleanTrue:kCFBooleanFalse;
        return *this;
    }

/*
    CCFValue &operator = (const string &value) { string(value); return *this; }

    void string(const string &value, CFStringEncoding encoding=kCFStringEncodingMacRoman)
    {
        mRef = CFStringCreate();
CFStringRef CFStringCreateWithBytes(CFAllocatorRef alloc, const UInt8 *bytes, CFIndex numBytes, CFStringEncoding encoding, Boolean isExternalRepresentation);
        if (!mRef) throw std::bad_alloc;
        CFRelease(mRef);
    }
*/

    bool hasValue() const { return mRef; }

    operator bool() const
    {
        if (!mRef) return false;
        if (::CFGetTypeID(mRef) != ::CFBooleanGetTypeID())
            throw std::logic_error("wrong type in property list");

        return ::CFBooleanGetValue(cfref<CFBooleanRef>());
    }

    operator sint32() const
    {
        if (!mRef) return 0;
        if (::CFGetTypeID(mRef) != ::CFNumberGetTypeID())
            throw std::logic_error("wrong type in property list");
        
        sint32 val;
        ::CFNumberGetValue(cfref<CFNumberRef>(),kCFNumberSInt32Type,&val);
        return val;
    }

    operator uint32() const { return uint32(sint32(*this)); }

    operator const string() const { return getString(); }

    const string getString(CFStringEncoding encoding=kCFStringEncodingMacRoman) const
    {
        if (!mRef)
            throw std::logic_error("missing string in property list");
        if (::CFGetTypeID(mRef) != ::CFStringGetTypeID())
            throw std::logic_error("wrong type in property list");

        const char *tmpStr=::CFStringGetCStringPtr(cfref<CFStringRef>(),encoding);
        if (tmpStr == NULL)
        {
            CFIndex maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfref<CFStringRef>()), encoding);
            auto_array<char> buffer(maxLen + 1);

            if (!::CFStringGetCString(cfref<CFStringRef>(),buffer.get(),maxLen + 1,encoding))
                throw std::logic_error("could not convert string from property list");

            tmpStr=buffer.get();
            return string(tmpStr?tmpStr:"");
        }
        return string(tmpStr?tmpStr:"");
    }
private:
	CFCopyRef<CFTypeRef>mRef;
};

} // end namespace Security

#endif /* __DLDBLISTCFPREF_H_ */
