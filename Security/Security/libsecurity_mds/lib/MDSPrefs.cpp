/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// MDSPrefs.cpp
//

#include "MDSPrefs.h"
#include <CoreFoundation/CFPreferences.h>
#include <stdlib.h>

// Construct the preferences object and read the current preference settings.

MDSPrefs::MDSPrefs()
    :	mPluginFolders(NULL)
{
    if (!readPathFromPrefs() && !readPathFromEnv())
        useDefaultPath();
}

// Destroy the preferences object.

MDSPrefs::~MDSPrefs()
{
    if (mPluginFolders)
        CFRelease(mPluginFolders);
}

// Obtain the plugin path from a preferences file. Returns true on success of false
// if no prefs could be found.

bool
MDSPrefs::readPathFromPrefs()
{
    static const CFStringRef kPrefsSuite = CFSTR("com.apple.mds");
    static const CFStringRef kPluginPathKey = CFSTR("securityPluginPath");

    bool result = true;

    CFPreferencesAddSuitePreferencesToApp(kCFPreferencesCurrentApplication, kPrefsSuite);
    
    CFPropertyListRef value;
    value = CFPreferencesCopyAppValue(kPluginPathKey, kCFPreferencesCurrentApplication);
    
    if (CFGetTypeID(value) != CFArrayGetTypeID())
		// the prefs object is not an array, so fail
		result = false;
		
	else {
        // make sure that all array elements are strings

		CFArrayRef array = (CFArrayRef) value;
		int numItems = CFArrayGetCount(array);
		for (int i = 0; i < numItems; i++)
			if (CFGetTypeID(CFArrayGetValueAtIndex(array, i)) != CFStringGetTypeID()) {
				result = false;
				break;
			}
	}
	
	if (result)
        mPluginFolders = (CFArrayRef) value;
	else
        CFRelease(value);

    return result;
}

bool
MDSPrefs::readPathFromEnv()
{
    static const char *kPluginPathEnv = "MDSPATH";
    static const CFStringRef kSeparator = CFSTR(":");

    char *envValue = getenv(kPluginPathEnv);
    if (envValue) {
        CFStringRef path = CFStringCreateWithCString(NULL, envValue, kCFStringEncodingUTF8);

        mPluginFolders = CFStringCreateArrayBySeparatingStrings(NULL, path, kSeparator);
        
        CFRelease(path);
        return true;
    }
    
    return false;
}

void
MDSPrefs::useDefaultPath()
{
    static const CFStringRef kDefaultPluginPath = CFSTR("/System/Library/Security");

    mPluginFolders = CFArrayCreate(NULL, (const void **) &kDefaultPluginPath, 1, &kCFTypeArrayCallBacks);
}

// Retrieve the elements of the plugin path.

int
MDSPrefs::getNumberOfPluginFolders() const
{
    if (mPluginFolders)
        return CFArrayGetCount(mPluginFolders);
        
    return 0;
}

const char *
MDSPrefs::getPluginFolder(int index)
{
    if (mPluginFolders) {
        int numValues = CFArrayGetCount(mPluginFolders);
        if (index >= 0 && index < numValues) {
            CFStringRef value = (CFStringRef) CFArrayGetValueAtIndex(mPluginFolders, index);
            if (value) {
                // we have to copy the string since it may be using a different native
                // encoding than the one we want. the copy is put in a temporary buffer,
                // so its lifetime is limited to the next call to getPluginFolder() or to
                // the destruction of the MDSPrefs object. Very long paths will silently fail.
                
                if (CFStringGetCString(value, mTempBuffer, kTempBufferSize, kCFStringEncodingUTF8))
                    return mTempBuffer;
            }
        }
    }
    
    return NULL;
}

