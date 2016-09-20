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

/* Purpose:
 This header and its corresponding implementation are intended to house functionality that's useful
 throughtout SecTranslocate but isn't directly tied to the SPI or things that must be serialized.
 */

#ifndef SecTranslocateUtilities_hpp
#define SecTranslocateUtilities_hpp

#include <stdio.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <security_utilities/unix++.h>

#include <string>
#include <vector>

#define NULLFS_FSTYPE "nullfs"

namespace Security {
    
using namespace Security::UnixPlusPlus;

namespace SecTranslocate {

using namespace std;
    
class ExtendedAutoFileDesc : public AutoFileDesc {
public:
    ExtendedAutoFileDesc() = delete; //Always want these initialized with a path
    
    ExtendedAutoFileDesc(const char *path, int flag = O_RDONLY, mode_t mode = 0666)
    : AutoFileDesc(path, flag, mode), originalPath(path) { init(); }
    ExtendedAutoFileDesc(const std::string &path, int flag = O_RDONLY, mode_t mode = 0666)
    : AutoFileDesc(path, flag, mode),originalPath(path) { init(); }
    
    bool isFileSystemType(const string &fsType) const;
    bool pathIsAbsolute() const;
    bool isMountPoint() const;
    bool isInPrefixDir(const string &prefixDir) const;
    string getFsType() const;
    string getMountPoint() const;
    string getMountFromPath() const;
    const string& getRealPath() const;
    fsid_t const getFsid() const;
    bool isQuarantined();
    bool isUserApproved();
    bool shouldTranslocate();
    
    // implicit destructor should call AutoFileDesc destructor. Nothing else to clean up.
private:
    void init();
    inline void notOpen() const { if(!isOpen()) UnixError::throwMe(EINVAL); };
    
    struct statfs fsInfo;
    string realPath;
    string originalPath;
    bool quarantineFetched;
    bool quarantined;
    uint32_t qtn_flags;
    void fetchQuarantine();
};

//General utilities
string makeUUID();
void* checkedDlopen(const char* path, int mode);
void* checkedDlsym(void* handle, const char* symbol);

//Path parsing functions
vector<string> splitPath(const string &path);
string joinPath(vector<string>& path);
    string joinPathUpTo(vector<string> &path, size_t index);

//File system utlities
string getRealPath(const string &path);
int getFDForDirectory(const string &directoryPath, bool *owned = NULL); //creates the directory if it can


//Translocation specific utilities
string translocationDirForUser();

} // namespace SecTranslocate
} // namespace Security


#endif /* SecTranslocateUtilities_hpp */
