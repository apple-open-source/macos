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
extern const char* kSecTranslocateXPCMessagePid;

/*XPC message reply keys */
extern const char* kSecTranslocateXPCReplyError;
extern const char* kSecTranslocateXPCReplySecurePath;

class TranslocationPath
{
public:
    TranslocationPath(string originalPath);
    inline bool shouldTranslocate() const { return should; };
    inline const string & getOriginalRealPath() const { return realOriginalPath; };
    inline const string & getPathToTranslocate() const { return pathToTranslocate; };
    inline const string & getPathInsideTranslocation() const { return pathInsideTranslocationPoint; } ;
    string getTranslocatedPathToOriginalPath(const string &translocationPoint) const;
private:
    TranslocationPath() = delete;

    bool should;
    string realOriginalPath;
    string pathToTranslocate;
    string pathInsideTranslocationPoint;

    ExtendedAutoFileDesc findOuterMostCodeBundleForFD(ExtendedAutoFileDesc &fd);
};

string getOriginalPath(const ExtendedAutoFileDesc& fd, bool* isDir); //throws

// For methods below, the caller is responsible for ensuring that only one thread is
// accessing/modifying the mount table at a time
string translocatePathForUser(const TranslocationPath &originalPath, const string &destPath); //throws
bool destroyTranslocatedPathForUser(const string &translocatedPath); //throws
bool destroyTranslocatedPathsForUserOnVolume(const string &volumePath = ""); //throws
void tryToDestroyUnusedTranslocationMounts();

} //namespace SecTranslocate
}// namespace Security

#endif /* SecTranslocateShared_hpp */
