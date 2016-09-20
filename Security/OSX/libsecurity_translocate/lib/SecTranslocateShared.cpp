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

#include <vector>
#include <string>
#include <exception>

#include <sys/stat.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ucred.h>
#include <dispatch/dispatch.h>
#include <string.h>
#include <dirent.h>

#define __APPLE_API_PRIVATE
#include <quarantine.h>
#undef __APPLE_API_PRIVATE

#include <security_utilities/cfutilities.h>
#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>
#include <Security/SecStaticCode.h>

#include "SecTranslocateShared.hpp"
#include "SecTranslocateUtilities.hpp"


namespace Security {
    
namespace SecTranslocate {
    
using namespace std;

/* String Constants for XPC  dictionary passing */
/* XPC Function keys */
const char* kSecTranslocateXPCFuncCreate = "create";
const char* kSecTranslocateXPCFuncCheckIn = "check-in";

/* XPC message argument keys */
const char* kSecTranslocateXPCMessageFunction = "function";
const char* kSecTranslocateXPCMessageOriginalPath = "original";
const char* kSecTranslocateXPCMessageDestinationPath = "dest";
const char* kSecTranslocateXPCMessagePid = "pid";

/*XPC message reply keys */
const char* kSecTranslocateXPCReplyError = "error";
const char* kSecTranslocateXPCReplySecurePath = "result";

//Functions used only within this file
static void setMountPointQuarantineIfNecessary(const string &mountPoint, const string &originalPath);
static string getMountpointFromAppPath(const string &appPath, const string &originalPath);

static vector<struct statfs> getMountTableSnapshot();
static string mountExistsForUser(const string &translationDirForUser, const string &originalPath, const string &destMount);
static void validateMountpoint(const string &mountpoint, bool owned=false);
static string makeNewMountpoint(const string &translationDir);
static string newAppPath (const string &mountPoint, const TranslocationPath &originalPath);
static void cleanupTranslocationDirForUser(const string &userDir);
static int removeMountPoint(const string &mountpoint, bool force = false);

/* calculate whether a translocation should occur and where from */
TranslocationPath::TranslocationPath(string originalPath)
{

    /* To support testing of translocation the policy is as follows:
     1. When the quarantine translocation sysctl is off, always translocate
     if we aren't already on a translocated mount point.
     2. When the quarantine translocation sysctl is on, use the quarantine
     bits to decide.
     when asking if a path should run translocated need to:
        check the current quarantine state of the path asked about
        if it is already on a nullfs mount
            do not translocate
        else if it is unquarantined
            do not translocate
        else
            if not QTN_FLAG_TRANSLOCATE or QTN_FLAG_DO_NOT_TRANSLOCATE
                do not translocate
            else
                find the outermost acceptable code bundle
                if not QTN_FLAG_TRANSLOCATE or QTN_FLAG_DO_NOT_TRANSLOCATE
                    don't translocate
                else
                    translocate

     See findOuterMostCodeBundleForFD for more info about what an acceptable outermost bundle is
     in particular it should be noted that the outermost acceptable bundle for a quarantined inner
     bundle can not be unquarantined. If the inner bundle is quarantined then any bundle containing it
     must also have been quarantined.
     */

    ExtendedAutoFileDesc fd(originalPath);

    should = false;
    realOriginalPath = fd.getRealPath();

    /* don't translocate if it already is */
    /* only consider translocation if the thing being asked about is marked for translocation */
    if(!fd.isFileSystemType(NULLFS_FSTYPE) && fd.isQuarantined() && fd.shouldTranslocate())
    {
        ExtendedAutoFileDesc &&outermost = findOuterMostCodeBundleForFD(fd);

        should = outermost.isQuarantined() && outermost.shouldTranslocate();
        pathToTranslocate = outermost.getRealPath();

        /* Calculate the path that will be needed to give the caller the path they asked for originally but in the translocated place */
        if (should)
        {
            vector<string> originalComponents = splitPath(realOriginalPath);
            vector<string> toTranslocateComponents = splitPath(pathToTranslocate);

            if (toTranslocateComponents.size() == 0 ||
                toTranslocateComponents.size() > originalComponents.size())
            {
                Syslog::error("SecTranslocate, TranslocationPath, path calculation failed:\n\toriginal: %s\n\tcalculated: %s",
                              realOriginalPath.c_str(),
                              pathToTranslocate.c_str());
                UnixError::throwMe(EINVAL);
            }

            for(size_t cnt = 0; cnt < originalComponents.size(); cnt++)
            {
                if (cnt < toTranslocateComponents.size())
                {
                    if (toTranslocateComponents[cnt] != originalComponents[cnt])
                    {
                        Syslog::error("SecTranslocate, TranslocationPath, translocation path calculation failed:\n\toriginal: %s\n\tcalculated: %s",
                                      realOriginalPath.c_str(),
                                      pathToTranslocate.c_str());
                        UnixError::throwMe(EINVAL);
                    }
                }
                else
                {
                    /*
                     want pathInsideTranslocationPoint to look like:
                        a/b/c
                     i.e. internal / but not at the front or back.
                     */
                    if(pathInsideTranslocationPoint.empty())
                    {
                        pathInsideTranslocationPoint = originalComponents[cnt];
                    }
                    else
                    {
                        pathInsideTranslocationPoint += "/" + originalComponents[cnt];
                    }
                }
            }
        }
    }
}

/* if we should translocate and a stored path inside the translocation point exists, then add it to the
   passed in string. If no path inside is stored, then return the passed in string if translocation
   should occur, and the original path for the TranslocationPath if translocation shouldn't occur */
string TranslocationPath::getTranslocatedPathToOriginalPath(const string &translocationPoint) const
{
    string seperator = translocationPoint.back() != '/' ? "/" : "";

    if (should)
    {
        if(!pathInsideTranslocationPoint.empty())
        {
            return translocationPoint + seperator + pathInsideTranslocationPoint;
        }
        else
        {
            return translocationPoint;
        }
    }
    else
    {
        //If we weren't supposed to translocate return the original path.
        return realOriginalPath;
    }
}

/* Given an fd for a path find the outermost acceptable code bundle and return an fd for that.
   an acceptable outermost bundle is quarantined, user approved, and a code bundle.
   If nothing is found outside the path to the fd provided, then passed in fd or a copy there of is returned.*/
ExtendedAutoFileDesc TranslocationPath::findOuterMostCodeBundleForFD(ExtendedAutoFileDesc &fd)
{
    if( fd.isMountPoint() || !fd.isQuarantined())
    {
        return fd;
    }
    vector<string> path = splitPath(fd.getRealPath());
    size_t currentIndex = path.size() - 1;
    size_t lastGoodIndex = currentIndex;

    string pathToCheck = joinPathUpTo(path, currentIndex);
    /*
     Proposed algorithm (pseudo-code):
     lastGood := path := canonicalized path to be launched

     while path is not a mount point
         if path is quarantined and not user-approved then exit loop	# Gatekeeper has not cleared this code
         if SecStaticCodeCreateWithPath(path) succeeds	# used as an “is a code bundle” oracle
             then lastGood := path
         path := parent directory of path
     return lastGood
     */
    while(currentIndex)
    {
        ExtendedAutoFileDesc currFd(pathToCheck);

        if (currFd.isMountPoint() || !currFd.isQuarantined() || !currFd.isUserApproved())
        {
            break;
        }

        SecStaticCodeRef staticCodeRef = NULL;

        if( SecStaticCodeCreateWithPath(CFTempURL(currFd.getRealPath()), kSecCSDefaultFlags, &staticCodeRef) == errSecSuccess)
        {
            lastGoodIndex = currentIndex;
            CFRelease(staticCodeRef);
        }

        currentIndex--;
        pathToCheck = joinPathUpTo(path, currentIndex);
    }

    return ExtendedAutoFileDesc(joinPathUpTo(path, lastGoodIndex));
}

/* Given an fd to a translocated file, build the path to the original file
 Throws if the fd isn't in a nullfs mount for the calling user. */
string getOriginalPath(const ExtendedAutoFileDesc& fd, bool* isDir)
{
    if (!fd.isFileSystemType(NULLFS_FSTYPE) ||
        isDir == NULL ||
        !fd.isInPrefixDir(fd.getMountPoint()))
    {
        Syslog::error("SecTranslocate::getOriginalPath called with invalid params: fs_type = %s, isDir = %p, realPath = %s, mountpoint = %s",
                      fd.getFsType().c_str(),
                      isDir,
                      fd.getRealPath().c_str(),
                      fd.getMountPoint().c_str());
        UnixError::throwMe(EINVAL);
    }
    
    string translocationBaseDir = translocationDirForUser();
    
    if(!fd.isInPrefixDir(translocationBaseDir))
    {
        Syslog::error("SecTranslocate::getOriginal path called with path (%s) that doesn't belong to user (%d)",
                      fd.getRealPath().c_str(),
                      getuid());
        UnixError::throwMe(EPERM);
    }
    
    *isDir = fd.isA(S_IFDIR);
    
    vector<string> mountFromPath = splitPath(fd.getMountFromPath());
    vector<string> mountPointPath = splitPath(fd.getMountPoint());
    vector<string> translocatedRealPath = splitPath(fd.getRealPath());
    
    if (mountPointPath.size() > translocatedRealPath.size())
    {
        Syslog::warning("SecTranslocate: invalid translocated path %s", fd.getRealPath().c_str());
        UnixError::throwMe(EINVAL);
    }
    
    string originalPath = fd.getMountFromPath();
    
    int i;
    
    for( i = 0; i<translocatedRealPath.size(); i++)
    {
        /* match the mount point directories to the real path directories */
        if( i < mountPointPath.size())
        {
            if(translocatedRealPath[i] != mountPointPath[i])
            {
                Syslog::error("SecTranslocate: invalid translocated path %s", fd.getRealPath().c_str());
                UnixError::throwMe(EINVAL);
            }
        }
        /* check for the d directory */
        else if( i == mountPointPath.size())
        {
            if( translocatedRealPath[i] != "d")
            {
                Syslog::error("SecTranslocate: invalid translocated path %s", fd.getRealPath().c_str());
                UnixError::throwMe(EINVAL);
            }
        }
        /* check for the app name */
        else if( i == mountPointPath.size() + 1)
        {
            if( translocatedRealPath[i] != mountFromPath.back())
            {
                Syslog::error("SecTranslocate: invalid translocated path %s", fd.getRealPath().c_str());
                UnixError::throwMe(EINVAL);
            }
        }
        /* we are past the app name so add what ever is left */
        else
        {
            originalPath +="/"+translocatedRealPath[i];
        }
    }
    
    if( i == mountPointPath.size() || i == mountPointPath.size() + 1)
    {
        //Asked for the original path of the mountpoint or /d/
        Syslog::warning("SecTranslocate: asked for the original path of a virtual directory: %s", fd.getRealPath().c_str());
        UnixError::throwMe(ENOENT);
    }
    
    /* Make sure what we built actually exists */
    ExtendedAutoFileDesc originalFD(originalPath);
    if(!originalFD.pathIsAbsolute())
    {
        Syslog::error("SecTranslocate: Calculated original path contains symlinks:\n\tExpected: %s\n\tRequested: %s",
                      originalFD.getRealPath().c_str(),
                      originalPath.c_str());
        UnixError::throwMe(EINVAL);
    }
    
    return originalPath;
}

/* Given a path that should be a translocation path, and the path to an app do the following:
 1. Validate that the translocation path (appPath) is a valid translocation path
 2. Validate that the translocation path (appPath) is valid for the app specified by originalPath
 3. Calculate what the mountpoint path would be given the app path
 */
static string getMountpointFromAppPath(const string &appPath, const string &originalPath)
{
    /* assume that appPath looks like:
     /my/user/temp/dir/AppTranslocation/MY-UUID/d/foo.app

     and assume original path looks like:
     /my/user/dir/foo.app

     In this function we find and return /my/user/temp/dir/AppTranslocation/MY-UUID/
     we also verify that the stuff after that in appPath was /d/foo.app if the last directory
     in originalPath was /foo.app
     */
    string result;

    vector<string> app = splitPath(appPath); // throws if empty or not absolute
    vector<string> original = splitPath(originalPath); //throws if empty or not absolute

    if (original.size() == 0) // had to have at least one directory, can't null mount /
    {
        Syslog::error("SecTranslocate: invalid original path: %s", originalPath.c_str());
        UnixError::throwMe(EINVAL);
    }

    if (app.size() >= 3 && //the app path must have at least 3 directories, can't null mount onto /
        app.back() == original.back()) //last directory of both match
    {
        app.pop_back();
        if(app.back() == "d") //last directory of app path is preceded by /d/
        {
            app.pop_back();
            result = joinPath(app);
            goto end;
        }
    }

    Syslog::error("SecTranslocate: invalid app path: %s", appPath.c_str());
    UnixError::throwMe(EINVAL);

end:
    return result;
}

/* Read the mount table and return it in a vector */
static vector<struct statfs> getMountTableSnapshot()
{
    vector<struct statfs> mntInfo;
    int fs_cnt_first = 0;
    int fs_cnt_second = 0;
    int retry = 2;

    /*Strategy here is:
     1. check the current mount table size
     2. allocate double the required space
     3. actually read the mount table
     4. if the read actually filled up that double size try again once otherwise we are done
     */

    while(retry)
    {
        fs_cnt_first = getfsstat(NULL, 0 , MNT_WAIT);
        if(fs_cnt_first <= 0)
        {
            Syslog::warning("SecTranslocate: error(%d) getting mount table info.", errno);
            UnixError::throwMe();
        }

        if( fs_cnt_first == fs_cnt_second)
        {
            /* this path only applies on a retry. If our second attempt to get the size is
             the same as what we already read then break. */
            break;
        }

        mntInfo.resize(fs_cnt_first*2);

        fs_cnt_second = getfsstat(mntInfo.data(), (int)(mntInfo.size() * sizeof(struct statfs)), MNT_WAIT);
        if (fs_cnt_second <= 0)
        {
            Syslog::warning("SecTranslocate: error(%d) getting mount table info.", errno);
            UnixError::throwMe();
        }

        if( fs_cnt_second == mntInfo.size())
        {
            retry--;
        }
        else
        {
            mntInfo.resize(fs_cnt_second); // trim the vector to what we actually need
            break;
        }
    }

    if( retry == 0)
    {
        Syslog::warning("SecTranslocate: mount table is growing very quickly");
    }

    return mntInfo;
}

/* Given the directory where app translocations go for this user, the path to the app to be translocated
 and an optional destination mountpoint path. Check the mount table to see if a mount point already
 user, for this app. If a destMountPoint is provided, make sure it is for this user, and that
 exists for this the mountpoint found in the mount table is the same as the one requested */
static string mountExistsForUser(const string &translationDirForUser, const string &originalPath, const string &destMountPoint)
{
    string result; // start empty

    if(!destMountPoint.empty())
    {
        /* Validate that destMountPoint path is well formed and for this user
         well formed means it is === translationDirForUser/<1 directory>
         */
        vector<string> splitDestMount = splitPath(destMountPoint);

        if(splitDestMount.size() < 2) //translationDirForUser is never /
        {
            Syslog::warning("SecTranslocate: invalid destination mount point: %s",
                            destMountPoint.c_str());
            UnixError::throwMe(EINVAL);
        }

        splitDestMount.pop_back(); // knock off one directory

        string destBaseDir = joinPath(splitDestMount)+"/"; //translationDirForUser has a / at the end

        if (translationDirForUser != destBaseDir)
        {
            Syslog::warning("SecTranslocate: invalid destination mount point for user\n\tExpected: %s\n\tRequested: %s",
                            translationDirForUser.c_str(),
                            destBaseDir.c_str());
            /* requested destination isn't valid for the user */
            UnixError::throwMe(EINVAL);
        }
    }

    vector <struct statfs> mntbuf = getMountTableSnapshot();

    for (auto &i : mntbuf)
    {
        string mountOnName = i.f_mntonname;
        size_t lastNonSlashPos = mountOnName.length() - 1; //start at the end of the string

        /* find the last position of the last non slash character */
        for(; lastNonSlashPos != 0 && mountOnName[lastNonSlashPos] == '/' ; lastNonSlashPos--);

        /* we want an exact match for originalPath and a prefix match for translationDirForUser
         also make sure that this is a nullfs mount and that the mount point name is longer than the
         translation directory with something other than / */

        if (i.f_mntfromname == originalPath && //mount is for the requested path
            strcmp(i.f_fstypename, NULLFS_FSTYPE) == 0 && // mount is a nullfs mount
            lastNonSlashPos > translationDirForUser.length()-1 && // no shenanigans, there must be more directory here than just the translation dir
            strncmp(i.f_mntonname, translationDirForUser.c_str(), translationDirForUser.length()) == 0) //mount is inside the translocation dir
        {
            if(!destMountPoint.empty())
            {
                if (mountOnName != destMountPoint)
                {
                    /* a mount exists for this path, but its not the one requested */
                    Syslog::warning("SecTranslocate: requested destination doesn't match existing\n\tExpected: %s\n\tRequested: %s",
                                    i.f_mntonname,
                                    destMountPoint.c_str());
                    UnixError::throwMe(EEXIST);
                }
            }
            result = mountOnName;
            break;
        }
    }

    return result;
}

/* Given what we think is a valid mountpoint, perform a sanity check, and clean up if we are wrong */
static void validateMountpoint(const string &mountpoint, bool owned)
{
    /* Requirements:
     1. can be opened
     2. is a directory
     3. is not already a mountpoint
     4. is an absolute path
     */
    bool isDir = false;
    bool isMount = false;
    bool isEmpty = true;

    try {
        /* first make sure this is a directory and that it is empty
         (it could be dangerous to mount over a directory that contains something,
         unfortunately this is still racy, and mount() is path based so we can't lock
         down the directory until the mount succeeds (lock down is because of the entitlement
         checks in nullfs))*/
        DIR* dir = opendir(mountpoint.c_str());
        int error = 0;

        if (dir == NULL)
        {
            error = errno;
            Syslog::warning("SecTranslocate: mountpoint is not a directory or doesn't exist: %s",
                            mountpoint.c_str());
            UnixError::throwMe(error);
        }

        isDir = true;

        struct dirent *d;
        struct dirent dirbuf;
        int cnt = 0;
        int err = 0;
        while(((err = readdir_r(dir, &dirbuf, &d)) == 0) &&
              d != NULL)
        {
            /* skip . and .. but break if there is more than that */
            if(++cnt > 2)
            {
                isEmpty = false;
                break;
            }
        }

        error = errno;
        (void)closedir(dir);

        if(err)
        {
            Syslog::warning("SecTranslocate: error while checking that mountpoint is empty");
            UnixError::throwMe(error);
        }

        if(!isEmpty)
        {
            Syslog::warning("Sectranslocate: mountpoint is not empty: %s",
                            mountpoint.c_str());
            UnixError::throwMe(EBUSY);
        }

        /* now check that the path is not a mountpoint */
        ExtendedAutoFileDesc fd(mountpoint);

        if(!fd.pathIsAbsolute())
        {
            Syslog::warning("SecTranslocate: mountpoint isn't fully resolved\n\tExpected: %s\n\tActual: %s",
                            fd.getRealPath().c_str(),
                            mountpoint.c_str());
            UnixError::throwMe(EINVAL);
        }

        isMount = fd.isMountPoint();

        if(isMount)
        {
            Syslog::warning("SecTranslocate:Translocation failed, new mountpoint is already a mountpoint (%s)",
                            mountpoint.c_str());
            UnixError::throwMe(EINVAL);
        }
    }
    catch(...)
    {
        if(owned)
        {
            if (!isMount)
            {
                if (isDir)
                {
                    if(isEmpty)
                    {
                        rmdir(mountpoint.c_str());
                    }
                    /* Already logged the else case above */
                }
                else
                {
                    Syslog::warning("SecTranslocate: unexpected file detected at mountpoint location (%s). Deleting.",
                                    mountpoint.c_str());
                    unlink(mountpoint.c_str());
                }
            }
        }
        rethrow_exception(current_exception());
    }
}

/* Create and validate the directory that we should mount at but don't create the mount yet */
static string makeNewMountpoint(const string &translationDir)
{
    AutoFileDesc fd(getFDForDirectory(translationDir));

    string uuid = makeUUID();

    UnixError::check(mkdirat(fd, uuid.c_str(), 0500));

    string mountpoint = translationDir+uuid;

    validateMountpoint(mountpoint);

    return mountpoint;
}

/* If the original path has mountpoint quarantine info, apply it to the new mountpoint*/
static void setMountPointQuarantineIfNecessary(const string &mountPoint, const string &originalPath)
{
    struct statfs sfsbuf;
    int error = 0;

    UnixError::check(statfs(originalPath.c_str(), &sfsbuf));
    qtn_file_t original_attr = qtn_file_alloc();

    if (original_attr != NULL)
    {
        if (qtn_file_init_with_mount_point(original_attr, sfsbuf.f_mntonname) == 0)
        {
            error = qtn_file_apply_to_mount_point(original_attr, mountPoint.c_str());
        }
        qtn_file_free(original_attr);
    }
    else
    {
        error = errno;
    }

    if (error)
    {
        Syslog::warning("SecTranslocate: Failed to apply quarantine information\n\tMountpoint: %s\n\tOriginal Path: %s",
                        mountPoint.c_str(),
                        originalPath.c_str());
        UnixError::throwMe(error);
    }
}

/* Given the path to a new mountpoint and the original path to translocate, calculate the path
 to the desired app in the new mountpoint, and sanity check that calculation */
static string newAppPath (const string &mountPoint, const TranslocationPath &originalPath)
{
    vector<string> original = splitPath(originalPath.getPathToTranslocate());

    if (original.size() == 0)
    {
        Syslog::error("SecTranslocate: Invalid originalPath: %s", originalPath.getPathToTranslocate().c_str());
        UnixError::throwMe(EINVAL);
    }

    string midPath = mountPoint+"/d";
    string outPath = originalPath.getTranslocatedPathToOriginalPath(midPath+"/"+original.back());

    /* ExtendedAutoFileDesc will throw if one of these doesn't exist or isn't accessible */
    ExtendedAutoFileDesc mountFd(mountPoint);
    ExtendedAutoFileDesc midFd(midPath);
    ExtendedAutoFileDesc outFd(outPath);

    if(!outFd.isFileSystemType(NULLFS_FSTYPE) ||
       !mountFd.isFileSystemType(NULLFS_FSTYPE) ||
       !midFd.isFileSystemType(NULLFS_FSTYPE))
    {
        Syslog::warning("SecTranslocate::App exists at expected translocation path (%s) but isn't a nullfs mount (%s)",
                        outPath.c_str(),
                        outFd.getFsType().c_str());
        UnixError::throwMe(EINVAL);
    }
    
    if(!outFd.pathIsAbsolute() ||
       !mountFd.pathIsAbsolute() ||
       !midFd.pathIsAbsolute() )
    {
        Syslog::warning("SecTranslocate::App path isn't resolved\n\tGot: %s\n\tExpected: %s",
                        outFd.getRealPath().c_str(),
                        outPath.c_str());
        UnixError::throwMe(EINVAL);
    }

    fsid_t outFsid = outFd.getFsid();
    fsid_t midFsid = midFd.getFsid();
    fsid_t mountFsid = mountFd.getFsid();

    /* different fsids mean that there is more than one volume between the expected mountpoint and the expected app path */
    if (memcmp(&outFsid, &midFsid, sizeof(fsid_t)) != 0 ||
        memcmp(&outFsid, &mountFsid, sizeof(fsid_t)) != 0)
    {
        Syslog::warning("SecTranslocate:: the fsid is not consistent between app, /d/ and mountpoint");
        UnixError::throwMe(EINVAL);
    }

    return outFd.getRealPath();
}

/* Create an app translocation point given the original path and an optional destination path.
 note the destination path can only be an outermost path (where the translocation would happen) and not a path to nested code
 synchronize the process on the dispatch queue. */
string translocatePathForUser(const TranslocationPath &originalPath, const string &destPath)
{
    string newPath;
    exception_ptr exception(0);

    string mountpoint;
    bool owned = false;
    try
    {
        const string &toTranslocate = originalPath.getPathToTranslocate();
        string baseDirForUser = translocationDirForUser(); //throws
        string destMountPoint;
        if(!destPath.empty())
        {
            destMountPoint = getMountpointFromAppPath(destPath, toTranslocate); //throws or returns a mountpoint
        }

        mountpoint = mountExistsForUser(baseDirForUser, toTranslocate, destMountPoint); //throws, detects invalid destMountPoint string

        if (!mountpoint.empty())
        {
            /* A mount point exists already so bail*/
            newPath = newAppPath(mountpoint, originalPath);
            return newPath; /* exit the block */
        }
        if (destMountPoint.empty())
        {
            mountpoint = makeNewMountpoint(baseDirForUser); //throws
            owned = true;
        }
        else
        {
            AutoFileDesc fd(getFDForDirectory(destMountPoint, &owned)); //throws, makes the directory if it doesn't exist

            validateMountpoint(destMountPoint, owned); //throws
            mountpoint = destMountPoint;
        }

        UnixError::check(mount(NULLFS_FSTYPE, mountpoint.c_str(), MNT_RDONLY, (void*)toTranslocate.c_str()));

        setMountPointQuarantineIfNecessary(mountpoint, toTranslocate); //throws

        newPath = newAppPath(mountpoint, originalPath); //throws

        if (!destPath.empty())
        {
            if (newPath != originalPath.getTranslocatedPathToOriginalPath(destPath))
            {
                Syslog::warning("SecTranslocate: created app translocation point did not equal requested app translocation point\n\texpected: %s\n\tcreated: %s",
                                newPath.c_str(),
                                destPath.c_str());
                /* the app at originalPath didn't match the one at destPath */
                UnixError::throwMe(EINVAL);
            }
        }
        // log that we created a new mountpoint (we don't log when we are re-using)
        Syslog::warning("SecTranslocateCreateSecureDirectoryForURL: created %s",
                        newPath.c_str());
    }
    catch (...)
    {
        exception = current_exception();

        if (!mountpoint.empty())
        {
            if (owned)
            {
                /* try to unmount/delete (best effort)*/
                unmount(mountpoint.c_str(), 0);
                rmdir(mountpoint.c_str());
            }
        }
    }

    /* rethrow outside the dispatch block */
    if (exception)
    {
        rethrow_exception(exception);
    }

    return newPath;
}

/* Loop through the directory in the specified user directory and delete any that aren't mountpoints */
static void cleanupTranslocationDirForUser(const string &userDir)
{
    DIR* translocationDir = opendir(userDir.c_str());

    if( translocationDir )
    {
        struct dirent de;
        struct statfs sfbuf;
        struct dirent * result = NULL;

        while (readdir_r(translocationDir, &de, &result) == 0 && result)
        {
            if(result->d_type == DT_DIR)
            {
                if (result->d_name[0] == '.')
                {
                    if(result->d_namlen == 1 ||
                       (result->d_namlen == 2 &&
                        result->d_name[1] == '.'))
                    {
                        /* skip . and .. */
                        continue;
                    }
                }
                string nextDir = userDir+string(result->d_name);
                if (0 == statfs(nextDir.c_str(), &sfbuf) &&
                    nextDir == sfbuf.f_mntonname)
                {
                    /* its a mount point so continue */
                    continue;
                }

                /* not a mountpoint so delete it */
                if(unlinkat(dirfd(translocationDir), result->d_name, AT_REMOVEDIR))
                {
                    Syslog::warning("SecTranslocate: failed to delete directory during cleanup (error %d)\n\tUser Dir: %s\n\tDir to delete: %s",
                                    errno,
                                    userDir.c_str(),
                                    result->d_name);
                }
            }
        }
        closedir(translocationDir);
    }
}

/* Unmount and delete a directory */
static int removeMountPoint(const string &mountpoint, bool force)
{
    int error = 0;

    if (0 == unmount(mountpoint.c_str(), force ? MNT_FORCE : 0) &&
        0 == rmdir(mountpoint.c_str()))
    {
        Syslog::warning("SecTranslocate: removed mountpoint: %s",
                        mountpoint.c_str());
    }
    else
    {
        error = errno;
        Syslog::warning("SecTranslocate: failed to unmount/remove mount point (errno: %d): %s",
                        error, mountpoint.c_str());
    }

    return error;
}

/* Destroy the specified translocated path, and clean up the user's translocation directory.
   It is the caller's responsibility to synchronize the operation on the dispatch queue. */
bool destroyTranslocatedPathForUser(const string &translocatedPath)
{
    bool result = false;
    int error = 0;
    /* steps
     1. verify the translocatedPath is for the user
     2. verify it is a nullfs mountpoint (with app path)
     3. unmount it
     4. delete it
     5. loop through all the other directories in the app translation directory looking for directories not mounted on and delete them.
     */

    string baseDirForUser = translocationDirForUser(); // throws
    bool shouldUnmount = false;
    string translocatedMountpoint;

    { //Use a block to get rid of the file descriptor before we try to unmount.
        ExtendedAutoFileDesc fd(translocatedPath);
        translocatedMountpoint = fd.getMountPoint();
        /*
         To support unmount when nested apps end, just make sure that the requested path is on a translocation
         point for this user, not that they asked for a translocation point to be removed.
         */
        shouldUnmount = fd.isInPrefixDir(baseDirForUser) && fd.isFileSystemType(NULLFS_FSTYPE);
    }

    if (shouldUnmount)
    {
        error = removeMountPoint(translocatedMountpoint);
        result = error == 0;
    }

    if (!result && !error)
    {
        Syslog::warning("SecTranslocate: mountpoint does not belong to user(%d): %s",
                        getuid(),
                        translocatedPath.c_str());
        error = EPERM;
    }

    cleanupTranslocationDirForUser(baseDirForUser);

    if (error)
    {
        UnixError::throwMe(error);
    }

    return result;
}

/* Cleanup any translocation directories for this user that are either mounted from the
 specified volume or from a volume that doesn't exist anymore. If an empty volumePath
 is provided this has the effect of only cleaning up translocation points that point
 to volumes that don't exist anymore.

 It is the caller's responsibility to synchronize the operation on the dispatch queue.
 */
bool destroyTranslocatedPathsForUserOnVolume(const string &volumePath)
{
    bool cleanupError = false;
    string baseDirForUser = translocationDirForUser();
    vector <struct statfs> mountTable = getMountTableSnapshot();
    fsid_t unmountingFsid;

    /* passing in an empty volume here will fail to open */
    ExtendedAutoFileDesc volume(volumePath, O_RDONLY, FileDesc::modeMissingOk);

    if(volume.isOpen())
    {
        unmountingFsid = volume.getFsid();
    }

    for (auto &mnt : mountTable)
    {
        /*
         we need to look at each translocation mount and check
         1. is it ours
         2. does its mntfromname still exist, if it doesn't unmount it
         3. if it does, is it the same as the volume we are cleaning up?, if so unmount it.
         */
        if (strcmp(mnt.f_fstypename, NULLFS_FSTYPE) == 0 &&
            strncmp(mnt.f_mntonname, baseDirForUser.c_str(), baseDirForUser.length()) == 0)
        {
            ExtendedAutoFileDesc volumeToCheck(mnt.f_mntfromname, O_RDONLY, FileDesc::modeMissingOk);

            if (!volumeToCheck.isOpen())
            {
                // In this case we are trying to unmount a translocation point that points to nothing. Force it.
                // Not forcing it currently hangs in UBC cleanup.
                (void)removeMountPoint(mnt.f_mntonname , true);
            }
            else if (volume.isOpen())
            {
                fsid_t toCheckFsid = volumeToCheck.getFsid();
                if( memcmp(&unmountingFsid, &toCheckFsid, sizeof(fsid_t)) == 0)
                {
                    if(removeMountPoint(mnt.f_mntonname) != 0)
                    {
                        cleanupError = true;
                    }
                }
            }
        }
    }

    return !cleanupError;
}
/* This is intended to be used periodically to clean up translocation points that aren't used anymore */
void tryToDestroyUnusedTranslocationMounts()
{
    vector <struct statfs> mountTable = getMountTableSnapshot();
    string baseDirForUser = translocationDirForUser();

    for (auto &mnt : mountTable)
    {
        if (strcmp(mnt.f_fstypename, NULLFS_FSTYPE) == 0 &&
            strncmp(mnt.f_mntonname, baseDirForUser.c_str(), baseDirForUser.length()) == 0)
        {
            ExtendedAutoFileDesc volumeToCheck(mnt.f_mntfromname, O_RDONLY, FileDesc::modeMissingOk);

            // Try to destroy the mount point. If the mirroed volume (volumeToCheck) isn't open then force it.
            // Not forcing it currently hangs in UBC cleanup.
            (void)removeMountPoint(mnt.f_mntonname , !volumeToCheck.isOpen());
        }
    }
}

} //namespace SecTranslocate
}// namespace Security
