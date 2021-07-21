/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

/* Purpose: This header exposes shared functions that actually implement mount creation, policy question
 answering and mount deletion.

 Important: None of these functions implement synchronization and they all throw exceptions. It is up
 to the caller to handle those concerns.
 */

#include <string>
#include "SecTranslocateUtilities.hpp"

#ifndef SecTranslocateShared_hpp
#define SecTranslocateShared_hpp

namespace Security {

namespace SecTranslocate {

using namespace std;

/* XPC Function keys */
extern const char* kSecTranslocateXPCFuncCreate;
extern const char* kSecTranslocateXPCFuncCheckIn;

/* XPC message argument keys */
extern const char* kSecTranslocateXPCMessageFunction;
extern const char* kSecTranslocateXPCMessageOriginalPath;
extern const char* kSecTranslocateXPCMessageDestinationPath;
extern const char* kSecTranslocateXPCMessageOptions;
extern const char* kSecTranslocateXPCMessagePid;

/*XPC message reply keys */
extern const char* kSecTranslocateXPCReplyError;
extern const char* kSecTranslocateXPCReplySecurePath;

enum class TranslocationOptions : int64_t {
    Default = 0,
    Generic = 1 << 0,
    Unveil  = 1 << 1
};

class GenericTranslocationPath
{
public:
    GenericTranslocationPath(const string& path, TranslocationOptions opts): mOptions(opts), mFd(path) { init(); };
    GenericTranslocationPath(int fd, TranslocationOptions opts): mOptions(opts), mFd(fd) { init(); };
    inline bool shouldTranslocate() const { return mShould; };
    inline const string & getOriginalRealPath() const { return mRealOriginalPath; };
    inline const string & getComponentNameToTranslocate() const { return mComponentNameToTranslocate; };
    inline TranslocationOptions getOptions() const { return mOptions; };
    int getFdForPathToTranslocate() const;
private:
    GenericTranslocationPath() = delete;
    void init();
    
    bool mShould;
    string mRealOriginalPath;
    string mComponentNameToTranslocate;
    TranslocationOptions mOptions;
    ExtendedAutoFileDesc mFd;
};

class TranslocationPath
{
public:
    TranslocationPath(string originalPath, TranslocationOptions opts): mOptions(opts), mFd(originalPath) { init(); };
    TranslocationPath(int fd, TranslocationOptions opts): mOptions(opts), mFd(fd) { init(); };
    inline bool shouldTranslocate() const { return mShould; };
    inline const string & getOriginalRealPath() const { return mRealOriginalPath; };
    inline const string & getPathToTranslocate() const { return mPathToTranslocate; };
    inline const string & getPathInsideTranslocation() const { return mPathInsideTranslocationPoint; };
    inline const string & getComponentNameToTranslocate() const { return mComponentNameToTranslocate; };
    string getTranslocatedPathToOriginalPath(const string &translocationPoint) const;
    inline TranslocationOptions getOptions() const { return mOptions; };
    int getFdForPathToTranslocate() const;
private:
    TranslocationPath() = delete;
    void init();

    bool mShould;
    string mRealOriginalPath;
    string mPathToTranslocate;
    string mComponentNameToTranslocate; //the final component of pathToTranslocate
    string mPathInsideTranslocationPoint;
    TranslocationOptions mOptions;
    ExtendedAutoFileDesc mFd;

    ExtendedAutoFileDesc findOuterMostCodeBundleForFD(ExtendedAutoFileDesc &fd);
};

string getOriginalPath(const ExtendedAutoFileDesc& fd, bool* isDir); //throws

// For methods below, the caller is responsible for ensuring that only one thread is
// accessing/modifying the mount table at a time
string translocatePathForUser(const TranslocationPath &originalPath, ExtendedAutoFileDesc &destFd); //throws
string translocatePathForUser(const GenericTranslocationPath &originalPath, ExtendedAutoFileDesc &destFd); //throws
bool destroyTranslocatedPathForUser(const string &translocatedPath); //throws
bool destroyTranslocatedPathsForUserOnVolume(const string &volumePath = ""); //throws
void tryToDestroyUnusedTranslocationMounts();

} //namespace SecTranslocate
}// namespace Security


#endif /* SecTranslocateShared_hpp */
