/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
// ssclient - SecurityServer client interface library
//
#include "ssblob.h"
#include <utilities/SecCFRelease.h>

namespace Security {
namespace SecurityServer {

uint32 CommonBlob::getCurrentVersion() {
    uint32 ret = version_MacOS_10_0;
    // If the integrity protections are turned on, use version_partition.
    // else, use version_MacOS_10_0.
    CFTypeRef integrity = (CFNumberRef)CFPreferencesCopyValue(CFSTR("KeychainIntegrity"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    if (integrity && CFGetTypeID(integrity) == CFBooleanGetTypeID()) {
        bool integrityProtections = CFBooleanGetValue((CFBooleanRef)integrity);

        if(integrityProtections) {
            secnotice("integrity", "creating a partition keychain; global is on");
            ret = version_partition;
        } else {
            secnotice("integrity", "creating a old-style keychain; global is off");
            ret = version_MacOS_10_0;
        }
    } else {
        secnotice("integrity", "global integrity not set, defaulting to on");
        ret = version_partition;
    }
    CFReleaseSafe(integrity);

    return ret;
}

uint32 CommonBlob::getCurrentVersionForDb(const char* dbName) {
    // Currently, the scheme is as follows:
    //   in ~/Library/Keychains:
    //     version_partition
    //   Elsewhere:
    //     version_MacOS_10_0`

    if(pathInHomeLibraryKeychains(dbName)) {
        return CommonBlob::getCurrentVersion();
    }

    secnotice("integrity", "outside ~/Library/Keychains/; creating a old-style keychain");
    return version_MacOS_10_0;
}

bool CommonBlob::pathInHomeLibraryKeychains(const string& path) {
    // We need to check if this path is in Some User's ~/Library/Keychains directory.
    // At this level, there's no great way of discovering what's actually a
    // user's home directory, so instead let's look for anything under
    // ./Library/Keychains/ that isn't /Library/Keychains or /System/Library/Keychains.

    string libraryKeychains = "/Library/Keychains";
    string systemLibraryKeychains = "/System/Library/Keychains";

    bool inALibraryKeychains = (string::npos != path.find(libraryKeychains));
    bool inRootLibraryKeychains = (0 == path.find(libraryKeychains));
    bool inSystemLibraryKeychains = (0 == path.find(systemLibraryKeychains));

    return (inALibraryKeychains && !inRootLibraryKeychains && !inSystemLibraryKeychains);
}

void CommonBlob::initialize()
{
    magic = magicNumber;

    this->blobVersion = getCurrentVersion();
}

//
// Initialize the blob header for a given version
//
void CommonBlob::initialize(uint32 version)
{
    magic = magicNumber;

    secinfo("integrity", "creating a keychain with version %d", version);
    this->blobVersion = version;
}


//
// Verify the blob header for basic sane-ness.
//
bool CommonBlob::isValid() const
{
	return magic == magicNumber;
}

void CommonBlob::validate(CSSM_RETURN failureCode) const
{
    if (!isValid())
        CssmError::throwMe(failureCode);
}

/*
 * This string is placed in KeyBlob.blobSignature to indicate a cleartext
 * public key.
 */
static const char clearPubKeySig[] = "Cleartext public key";

bool KeyBlob::isClearText()
{
	return (memcmp(blobSignature, clearPubKeySig, 
		sizeof(blobSignature)) == 0);
}

void KeyBlob::setClearTextSignature()
{
	memmove(blobSignature, clearPubKeySig, sizeof(blobSignature));
}

//
// Implementation of a "system keychain unlock key store"
//
SystemKeychainKey::SystemKeychainKey(const char *path)
: mPath(path), mValid(false)
{
    // explicitly set up a key header for a raw 3DES key
    CssmKey::Header &hdr = mKey.header();
    hdr.blobType(CSSM_KEYBLOB_RAW);
    hdr.blobFormat(CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING);
    hdr.keyClass(CSSM_KEYCLASS_SESSION_KEY);
    hdr.algorithm(CSSM_ALGID_3DES_3KEY_EDE);
    hdr.KeyAttr = 0;
    hdr.KeyUsage = CSSM_KEYUSE_ANY;
    mKey = CssmData::wrap(mBlob.masterKey);
}

SystemKeychainKey::~SystemKeychainKey()
{
}

bool SystemKeychainKey::matches(const DbBlob::Signature &signature)
{
    return update() && signature == mBlob.signature;
}

CssmKey& SystemKeychainKey::key()
{
    if(!mValid) {
        update();
    }
    return mKey;
}

bool SystemKeychainKey::update()
{
    // if we checked recently, just assume it's okay
    if (mValid && mUpdateThreshold > Time::now())
        return mValid;

    // check the file
    struct stat st;
    if (::stat(mPath.c_str(), &st)) {
        // something wrong with the file; can't use it
        mUpdateThreshold = Time::now() + Time::Interval(checkDelay);
        return mValid = false;
    }
    if (mValid && Time::Absolute(st.st_mtimespec) == mCachedDate)
        return true;
    mUpdateThreshold = Time::now() + Time::Interval(checkDelay);

    try {
        secnotice("syskc", "reading system unlock record from %s", mPath.c_str());
        UnixPlusPlus::AutoFileDesc fd(mPath, O_RDONLY);
        if (fd.read(mBlob) != sizeof(mBlob))
            return false;
        if (mBlob.isValid()) {
            mCachedDate = st.st_mtimespec;
            return mValid = true;
        } else
            return mValid = false;
    } catch (...) {
        secnotice("syskc", "system unlock record not available");
        return false;
    }
}

bool SystemKeychainKey::valid()
{
    update();
    return mValid;
}


} // end namespace SecurityServer

} // end namespace Security
