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

#include <string>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <sandbox/private.h>

#define __APPLE_API_PRIVATE
#include <quarantine.h>
#undef __APPLE_API_PRIVATE

#include <security_utilities/debugging.h>
#include <security_utilities/unix++.h>
#include <security_utilities/cfutilities.h>

#include "SecTranslocateUtilities.hpp"

#define APP_TRANSLOCATION_DIR "/AppTranslocation/"

namespace Security {

using namespace Security::UnixPlusPlus;

namespace SecTranslocate {

using namespace std;

ExtendedAutoFileDesc & ExtendedAutoFileDesc::operator=(ExtendedAutoFileDesc &&rhs)
{
	AutoFileDesc::operator=(std::move(rhs));
	mFsInfo = rhs.mFsInfo;
	mRealPath = rhs.mRealPath;
	mOriginalPath = rhs.mOriginalPath;
	mQuarantineFetched = rhs.mQuarantineFetched;
	mQuarantined = rhs.mQuarantined;
	mQtn_flags = rhs.mQtn_flags;
	return *this;
}

/* store the real path and fstatfs for the file descriptor. This throws if either fail */
void ExtendedAutoFileDesc::init()
{
    if(isOpen()) {
        UnixError::check(fstatfs(fd(), &mFsInfo));
        mRealPath = realPath();
        mQuarantined = false;
        mQtn_flags = 0;
        mQuarantineFetched = false; //only fetch quarantine info when we need it
    }
}

void ExtendedAutoFileDesc::open(const std::string &path, int flag, mode_t mode)
{
	FileDesc::open(path, flag, mode);
	if (isOpen()) {
		init();
	}
}

bool ExtendedAutoFileDesc::isFileSystemType(const string &fsType) const
{
    notOpen(); //Throws if not Open
    
    return fsType == mFsInfo.f_fstypename;
}

bool ExtendedAutoFileDesc::pathIsAbsolute() const
{
    notOpen(); //Throws if not Open
    
    return mOriginalPath == mRealPath;
}
    
bool ExtendedAutoFileDesc::isMountPoint() const
{
    notOpen(); //Throws if not Open
    return mRealPath == mFsInfo.f_mntonname;
}
bool ExtendedAutoFileDesc::isInPrefixDir(const string &prefixDir) const
{
    notOpen(); //Throws if not Open
    
    return strncmp(mRealPath.c_str(), prefixDir.c_str(), prefixDir.length()) == 0;
}

string ExtendedAutoFileDesc::getFsType() const
{
    notOpen(); //Throws if not Open
    
    return mFsInfo.f_fstypename;
}
    
string ExtendedAutoFileDesc::getMountPoint() const
{
    notOpen(); //Throws if not Open
    
    return mFsInfo.f_mntonname;
}
    
string ExtendedAutoFileDesc::getMountFromPath() const
{
    notOpen(); //Throws if not Open
    
    return mFsInfo.f_mntfromname;
}

const string& ExtendedAutoFileDesc::getRealPath() const
{
    notOpen(); //Throws if not Open
    
    return mRealPath;
}

fsid_t const ExtendedAutoFileDesc::getFsid() const
{
    notOpen(); //Throws if not Open
    
    return mFsInfo.f_fsid;
}

void ExtendedAutoFileDesc::fetchQuarantine()
{
    if (!mQuarantineFetched) {
        notOpen();

        qtn_file_t qf = qtn_file_alloc();

        if (qf) {
            if (0 == qtn_file_init_with_fd(qf, fd())) {
                mQuarantined = true;
                mQtn_flags = qtn_file_get_flags(qf);
            }
            qtn_file_free(qf);
            mQuarantineFetched = true;
        } else {
            secerror("SecTranslocate: failed to allocate memory for quarantine struct");
            UnixError::throwMe();
        }
    }
}

bool ExtendedAutoFileDesc::isQuarantined()
{
    notOpen();
    fetchQuarantine();

    return mQuarantined;
}

bool ExtendedAutoFileDesc::isUserApproved()
{
    notOpen();
    fetchQuarantine();

    return ((mQtn_flags & QTN_FLAG_USER_APPROVED) == QTN_FLAG_USER_APPROVED);
}

bool ExtendedAutoFileDesc::shouldTranslocate()
{
    notOpen();
    fetchQuarantine();

    return ((mQtn_flags & (QTN_FLAG_TRANSLOCATE | QTN_FLAG_DO_NOT_TRANSLOCATE)) == QTN_FLAG_TRANSLOCATE);
}

bool ExtendedAutoFileDesc::isSandcastleProtected()
{
	notOpen();
	bool result = false;
	char* approval = NULL;
	int rv = sandbox_query_approval_policy_for_path("file-read-data", mRealPath.c_str(), &approval);
	if (rv < 0) {
		secwarning("SecTranslocate: failed to get sandbox policy for: %s", mRealPath.c_str());
		// assuming no policy == not protected
	} else if (rv == 0) {
		// approved
		if (approval != NULL) {
			secerror("SecTranslocate: Do not allow translocation, access controled by (%s) for path: %s", approval, mRealPath.c_str());
			result = true;
		}
	} else {
		// denied
		result = true;
		if (approval != NULL) {
			secerror("SecTranslocate: Do not allow translocation, access controled by (%s) for path: %s", approval, mRealPath.c_str());
		} else {
			secerror("SecTranslocate: Do not allow translocation, access unconditionally denied for path: %s", mRealPath.c_str());
		}
	}
	if (approval != NULL) {
		free(approval);
	}
	return result;
}

/* Take an absolute path and split it into a vector of path components */
vector<string> splitPath(const string &path)
{
    vector<string> out;
    size_t start = 0;
    size_t end = 0;
    size_t len = 0;
    
    if (path.empty() || path.front() != '/') {
        secerror("SecTranslocate::splitPath: asked to split a non-absolute or empty path: %s",path.c_str());
        UnixError::throwMe(EINVAL);
    }
    
    while (end != string::npos) {
        end = path.find('/', start);
        len = (end == string::npos) ? end : (end - start);
        string temp = path.substr(start,len);
        
        if (!temp.empty()) {
            out.push_back(temp);
        }
        start = end + 1;
    }
    
    return out;
}

/* Take a vector of path components and turn it into an absolute path */
string joinPath(vector<string>& path)
{
    string out = "";
    for (auto &i : path) {
        out += "/"+i;
    }
    return out;
}

string joinPathUpTo(vector<string> &path, size_t index)
{
    if (path.size() == 0 || index > path.size() - 1) {
        secerror("SecTranslocate::joinPathUpTo invalid index %lu (size %lu)",index, path.size()-1);
        UnixError::throwMe(EINVAL);
    }

    string out = "";
    for (size_t i = 0; i <= index; i++)
    {
        out += "/" + path[i];
    }

    return out;
}
    
/* Fully resolve the path provided */
string getRealPath(const string &path)
{
    char absPath[MAXPATHLEN];
    AutoFileDesc fd(path);
    fd.fcntl(F_GETPATH, absPath);
    return absPath;
}
    
/* Create a UUID string */
string makeUUID()
{
    CFRef<CFUUIDRef> newUUID = CFUUIDCreate(NULL);
    if (!newUUID) {
        UnixError::throwMe(ENOMEM);
    }
    
    CFRef<CFStringRef> str = CFUUIDCreateString(NULL, newUUID.get());
    if (!str) {
        UnixError::throwMe(ENOMEM);
    }
    
    return cfString(str);
}

void* checkedDlopen(const char* path, int mode)
{
    void* handle = dlopen(path, mode);

    if (handle == NULL) {
		seccritical("SecTranslocate: failed to load library %s: %s", path, dlerror());
        UnixError::throwMe();
    }

    return handle;
}

void* checkedDlsym(void* handle, const char* symbol)
{
    void* result = dlsym(handle, symbol);

    if (result == NULL) {
		seccritical("SecTranslocate: failed to load symbol %s: %s", symbol, dlerror());
        UnixError::throwMe();
    }
    return result;
}
    
/* Calculate the app translocation directory for the user inside the user's temp directory */
string translocationDirForUser()
{
    char userTempPath[MAXPATHLEN];
    
    if (confstr(_CS_DARWIN_USER_TEMP_DIR, userTempPath, sizeof(userTempPath)) == 0)
    {
        secerror("SecTranslocate: Failed to get temp dir for user %d (error:%d)",
				 getuid(),
				 errno);
        UnixError::throwMe();
    }
    
    // confstr returns a path with a symlink, we want the resolved path */
    return getRealPath(userTempPath)+APP_TRANSLOCATION_DIR;
}

/* Get a file descriptor for the provided path. if the last component of the provided path doesn't
 exist, create it and then re-attempt to get the file descriptor.
 */
ExtendedAutoFileDesc getFDForDirectory(const string &directoryPath, bool *owned)
{
    ExtendedAutoFileDesc fd(directoryPath, O_RDONLY, FileDesc::modeMissingOk);
    if (!fd) {
        UnixError::check(mkdir(directoryPath.c_str(),0755));
        fd.open(directoryPath);
        /* owned means that the library created the directory rather than it being pre-existent.
         We just made a directory that didn't exist before, so set owned to true. */
        if(owned) {
            *owned = true;
        }
    } else if (owned) {
        *owned = false;
    }
    
    return fd;
}

} // namespace SecTranslocate
} // namespace Security
