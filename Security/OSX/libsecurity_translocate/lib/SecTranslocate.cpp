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

#include <stdio.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <string>
#include <vector>

#include <security_utilities/cfutilities.h>
#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>

#include "SecTranslocate.h"
#include "SecTranslocateShared.hpp"
#include "SecTranslocateInterface.hpp"
#include "SecTranslocateUtilities.hpp"


/* Strategy:
 
 This library exists to create and destroy app translocation points. To ensure that a given
 process using this library is only making or destroying one mountpoint at a time, the two
 interface functions are sychronized on a dispatch queue.
 
 **** App Translocation Strategy w/o a destination path ****
 (This functionality is implemented in SecTranslocateShared.hpp/cpp)

 To create a translocation mountpoint, if no destination path is provided, first the app
 path to be translocated is realpathed to ensure it exists and there are no symlink's in
 the path we work with. Then the calling user's _CS_DARWIN_USER_TEMP_DIR is found. This 
 is used to calculate the user's AppTranslocation directory. The formula is:
   User's App Translocation Directory = realpath(confstr(_CS_DARWIN_USER_TEMP_DIR))+/AppTranslocation/

 Then the mount table is checked to see whether or not a translocation point already exists
 for this user for the app being requested. The rule for an already existing mount is that
 there must exist a mountpoint in the user's app translocation directory that is mounted 
 from realpath of the requested app.

 If a mount exists already for this user, then the path to the app in that mountpoint is 
 calculated and sanity checked.

 The rules to create the app path inside the mountpoint are:

 original app path = /some/path/<app name>
 new app path = realpath(confstr(_CS_DARWIN_USER_TEMP_DIR))+/AppTranslocation/<UUID>/d/<app name>

 The sanity check for the new app path is that:
 1. new app path exists
 2. new app path is in a nullfs mount
 3. new app path is already completely resolved.

 If the sanity checks pass for the new app path, then that path is returned to the caller.

 If no translocation mount point exists already per the mount table then an AppTranslocation
 directory is created within the temp dir if it doesn't already exist. After that a UUID is 
 generated, that UUID is used as the name of a new directory in the AppTranslocation directory.
 Once the new directory has been created and sanity checked, mount is called to create the 
 translocation between the original path and the new directory. Then the new path to the app
 within the mountpoint is calculated and sanity checked. 

 The sanity check rules for the mountpoint before the mount are:
 1. Something exists at the expected path
 2. That something is a directory
 3. That something is not already a mountpoint
 4. The expected path is fully resolved

 The sanity check for the new app path is listed above (for the mountpoint exists case).

 **** App Translocation strategy w/ a destination path ****
 (This functionality is implemented in SecTranslocateShared.hpp/cpp)

 If a destination path is provided, a sequence similar to that described above is followed
 with the following modifications. 

 The destination path is expected to be of the same form as new app path. This expectation 
 is verified.

 First we verify that the destination path ends with /d/<app name> and that the <app name> 
 component of the destination path matches the <app name> of the original app app path 
 requested. If not, an error occurs. Everything before the /d/ is treated becomes the 
 requested destination mount point.

 After the user's app translocation directory is calculated, we ensure that the requested 
 destination mount point is prefixed by the translocation directory, and contains only one
 path component after the user's app translocation path, otherwise an error occurs.

 When we check the mount table, we make sure that if the a translocation of the app already
 exists for the user, then the translocation path must exactly match the requested
 destination path, otherwise an error occurs.

 If no mountpoint exists for the app, then we attempt to create the requested directory within
 the user's app translocation directory. This becomes the mount point, and the mount point
 sanity checks listed above are applied. 

 If the requested destination mountpoint is successfully created, the flow continues as above
 to create the mount and verify the requested path within the mountpoint. The only extra step
 here is that at the end, the requested app path, must exactly equal the created app path.

 **** App Translocation error cleanup ****
 (This functionality is implemented in SecTranslocateShared.hpp/cpp)

 The error cleanup strategy for translocation creation is to try to destroy any directories
 or mount points in the user's translocation directory that were created before an error
 was detected. This means tracking whether we created a directory, or it already existed when
 a caller asked for it. Clean up is considered best effort.

 **** Deleting an App Translocation point ****
 (This functionality is implemented in SecTranslocateShared.hpp/cpp)

 To destroy an app translocation point, the first thing we do is calculate the user's app
 translocation directory to ensure that the requested path is actually within that directory.
 We also verify that it is in fact a nullfs mount point. If it is, then we attempt to unmount and
 remove the translocation point.

 Regardless of whether or not the requested path is a translocation point, we opportunistically
 attempt to cleanup the app translocation directory. Clean up means, looping through all the 
 directories currently in the user's app translocation directory and checking whether or not
 they are a mount point. If a directory inside the user's app translocation directory is not
 a mountpoint, then we attempt to delete it.

 **** Quarantine considerations ****
 (This functionality is implemented in SecTranslocateShared.hpp/cpp and SecTranslocateUtilities.hpp/cpp)

 If the original app path includes files with quarantine extended attributes, then those extended
 attributes will be readable through the created app translocation mountpoint. nullfs does not
 support removing or setting extended attributes on its vnodes. Changes to the quarantine
 attributes at the original path will be reflected in the app translocation mountpoint without
 creating a new mount point.

 If the original app path is inside a quarantined mountpoint (such as a quarantined dmg), then
 that the quarantine information for that mountpoint is read from the original app path's
 mountpoint and applied to the created app translocation mountpoint.

 **** Concurrency considerations ****
 This library treats the kernel as the source of truth for the status of the file system. 
 Unfortunately it has no way to lock the state of the file system and mount table while
 it is operating. Because of this, there are two potential areas that have race windows.

 First, if any other system entity (thread within the same process, or other process 
 within the system) is adding or removing entries from the mount table while
 SecTranslocateCreateSecureDirectoryForURL is executing, then there is the possibility that
 an incorrect decision will be made about the current existence of a mount point for a user
 for an app. This is because getfsstat gets a snapshot of the mount table state rather than a 
 locked window into the kernel and because we make two seperate calls to getfsstat, one to get
 the number of mountpoints, and a second to actually read the mountpoint data. If more than 
 one process is using this library for the same user, then both processes could attempt to
 create a translocation for the same app, and this could result in more than one translocation
 for that app for the user. This shouldn't effect the user other than using additional
 system resources. We attempt to mitigate this by allocating double the required memory from
 the first call and then trying the process again (once) if the initial memory was filled.

 Second, if more than one process is using this library simultaneously and one process calls
 SecTranslocateDeleteSecureDirectory for a user and the other calls 
 SecTranslocateCreateSecureDirectoryForURL for that same user, then the call to
 SecTranslocateDeleteSecureDirectory may cause SecTranslocateCreateSecureDirectoryForURL to
 fail. This will occur if the loop checking for unmounted directories in the user's app 
 translocation directory deletes a newly created directory before the mount call finishes. This
 race condition will probably result in a failed app launch. A second attempt to launch the app
 will probably succeed.

 Concurrency is now split between SecTranslocateClient.hpp/cpp, SecTranslocateServer.hpp/cpp,
 SecTranslocateDANotification.hpp/cpp, SecTranslocateLSNotification.hpp/cpp, and
 SecTranslocateXPCServer.hpp/cpp. Each of these represent different ways of entering translocation
 functionality.

 **** Logging Strategy ****
 Use warning logging for interesting conditions (e.g. translocation point created or destroyed).
 Use error logging for non-fatal failures. Use critical logging for fatal failures.
 */

/* Make a CFError from an POSIX error code */
static CFErrorRef SecTranslocateMakePosixError(CFIndex errorCode)
{
    return CFErrorCreate(NULL, kCFErrorDomainPOSIX, errorCode, NULL);
}

/* must be called before any other function in this SPI if the process is intended to be the server */
Boolean SecTranslocateStartListening(CFErrorRef* __nullable error)
{
    Boolean result = false;
    CFIndex errorCode  = 0;
    try
    {
        /* ask getTranslocator for the server */
        result = Security::SecTranslocate::getTranslocator(true) != NULL;
    }
    catch (Security::UnixError err)
    {
        errorCode = err.unixError();
    }
    catch(...)
    {
        Syslog::critical("SecTranslocate: uncaught exception during server initialization");
        errorCode = EINVAL;
    }

    if (error && errorCode)
    {
        *error = SecTranslocateMakePosixError(errorCode);
    }

    return result;
}

/* placeholder api for now to allow for future options at startup */
Boolean SecTranslocateStartListeningWithOptions(CFDictionaryRef __unused options, CFErrorRef * __nullable outError)
{
    return SecTranslocateStartListening(outError);
}

/* Register that a (translocated) pid has launched */
void SecTranslocateAppLaunchCheckin(pid_t pid)
{
    try
    {
        Security::SecTranslocate::getTranslocator()->appLaunchCheckin(pid);
    }
    catch (...)
    {
        Syslog::error("SecTranslocate: error in SecTranslocateAppLaunchCheckin");
    }
}

/* Create an app translocation point given the original path and an optional destination path. */
CFURLRef __nullable SecTranslocateCreateSecureDirectoryForURL (CFURLRef pathToTranslocate,
                                                               CFURLRef __nullable destinationPath,
                                                               CFErrorRef* __nullable error)
{
    CFURLRef result = NULL;
    CFIndex errorCode  = 0;

    try
    {
        string  sourcePath = cfString(pathToTranslocate); // returns an absolute path
        
        Security::SecTranslocate::TranslocationPath toTranslocatePath(sourcePath);

        if(!toTranslocatePath.shouldTranslocate())
        {
            /* We shouldn't translocate so, just retain so that the return value can be treated as a copy */
            CFRetain(pathToTranslocate);
            return pathToTranslocate;
        }

        /* We need to translocate so keep going */
        string destPath;
        
        if(destinationPath)
        {
            destPath = cfString(destinationPath); //returns an absolute path
        }
        
        string out_path = Security::SecTranslocate::getTranslocator()->translocatePathForUser(toTranslocatePath, destPath);
        
        if(!out_path.empty())
        {
            result = makeCFURL(out_path, true);
        }
        else
        {
            Syslog::error("SecTranslocateCreateSecureDirectoryForURL: No mountpoint and no prior exception. Shouldn't be here");
            UnixError::throwMe(EINVAL);
        }
        
    }
    catch (Security::UnixError err)
    {
        errorCode = err.unixError();
    }
    catch(...)
    {
        Syslog::critical("SecTranslocate: uncaught exception during mountpoint creation");
        errorCode = EACCES;
    }
    
    if (error && errorCode)
    {
        *error = SecTranslocateMakePosixError(errorCode);
    }
    return result;
}

/* Destroy the specified translocated path, and clean up the user's translocation directory. */
Boolean SecTranslocateDeleteSecureDirectory(CFURLRef translocatedPath, CFErrorRef* __nullable error)
{
    bool result = false;
    int errorCode = 0;
    
    if(translocatedPath == NULL)
    {
        errorCode = EINVAL;
        goto end;
    }
    
    try
    {
        string pathToDestroy = cfString(translocatedPath);
        result = Security::SecTranslocate::getTranslocator()->destroyTranslocatedPathForUser(pathToDestroy);
    }
    catch (Security::UnixError err)
    {
        errorCode = err.unixError();
    }
    catch(...)
    {
        Syslog::critical("SecTranslocate: uncaught exception during mountpoint deletion");
        errorCode = EACCES;
    }
end:
    if (error && errorCode)
    {
        *error = SecTranslocateMakePosixError(errorCode);
    }
    
    return result;
}

/* Decide whether we need to translocate */
Boolean SecTranslocateURLShouldRunTranslocated(CFURLRef path, bool* shouldTranslocate, CFErrorRef* __nullable error)
{
    bool result = false;
    int errorCode = 0;

    if(path == NULL || shouldTranslocate == NULL)
    {
        errorCode = EINVAL;
        goto end;
    }

    try
    {
        string pathToCheck = cfString(path);
        Security::SecTranslocate::TranslocationPath tPath(pathToCheck);
        *shouldTranslocate = tPath.shouldTranslocate();
        result = true;
    }
    catch (Security::UnixError err)
    {
        errorCode = err.unixError();
    }
    catch(...)
    {
        Syslog::critical("SecTranslocate: uncaught exception during policy check");
        errorCode = EACCES;
    }

end:
    if (error && errorCode)
    {
        *error = SecTranslocateMakePosixError(errorCode);
    }

    return result;
}

/* Answer whether or not the passed in URL is a nullfs URL. This just checks nullfs rather than
   nullfs + in the user's translocation path to allow callers like LaunchServices to apply special
   handling to nullfs mounts regardless of the calling user (i.e. root lsd can identify all translocated
   mount points for all users).
 */
Boolean SecTranslocateIsTranslocatedURL(CFURLRef path, bool* isTranslocated, CFErrorRef* __nullable error)
{
    bool result = false;
    int errorCode  = 0;

    if(path == NULL || isTranslocated == NULL)
    {
        if(error)
        {
            *error = SecTranslocateMakePosixError(EINVAL);
        }
        return result;
    }

    *isTranslocated = false;

    try
    {
        string cpp_path = cfString(path);
        /* "/" i.e. the root volume, cannot be translocated (or mounted on by other file system after boot)
           so don't bother to make system calls if "/" is what is being asked about.
           This is an optimization to help LaunchServices which expects to use SecTranslocateIsTranslocatedURL
           on every App Launch.
         */
        if (cpp_path != "/")
        {
            /* to avoid AppSandbox violations, use a path based check here.
               We only look for nullfs file type anyway. */
            struct statfs sfb;
            if (statfs(cpp_path.c_str(), &sfb) == 0)
            {
                *isTranslocated = (strcmp(sfb.f_fstypename, NULLFS_FSTYPE) == 0);
                result = true;
            }
            else
            {
                errorCode = errno;
                Syslog::error("SecTranslocate: can not access %s, error: %s", cpp_path.c_str(), strerror(errorCode));
            }
        }
        else
        {
            result = true;
        }
    }
    catch (Security::UnixError err)
    {
        errorCode = err.unixError();
    }
    catch(...)
    {
        Syslog::critical("SecTranslocate: uncaught exception during policy check");
        errorCode = EACCES;
    }

    if (error && errorCode)
    {
        *error = SecTranslocateMakePosixError(errorCode);
    }

    return result;
}

/* Find the original path for translocation mounts belonging to the calling user.
   if the url isn't on a nullfs volume then returned a retained copy of the passed in url.
   if the url is on a nullfs volume but that volume doesn't belong to the user, or another 
   error occurs then null is returned */
CFURLRef __nullable SecTranslocateCreateOriginalPathForURL(CFURLRef translocatedPath, CFErrorRef* __nullable error)
{
    CFURLRef result = NULL;
    int errorCode  = 0;

    if(translocatedPath == NULL)
    {
        errorCode = EINVAL;
        goto end;
    }
    try
    {
        string path = cfString(translocatedPath);
        Security::SecTranslocate::ExtendedAutoFileDesc fd(path);

        if(fd.isFileSystemType(NULLFS_FSTYPE))
        {
            bool isDir = false;
            string out_path = Security::SecTranslocate::getOriginalPath(fd, &isDir);
            if(!out_path.empty())
            {
                result = makeCFURL(out_path, isDir);
            }
            else
            {
                Syslog::error("SecTranslocateCreateOriginalPath: No original and no prior exception. Shouldn't be here");
                UnixError::throwMe(EINVAL);
            }
        }
        else
        {
            result = translocatedPath;
            CFRetain(result);
        }
    }
    catch (Security::UnixError err)
    {
        errorCode = err.unixError();
    }
    catch(...)
    {
        Syslog::critical("SecTranslocate: uncaught exception during policy check");
        errorCode = EACCES;
    }
end:
    if (error && errorCode)
    {
        *error = SecTranslocateMakePosixError(errorCode);
    }
    return result;
}
