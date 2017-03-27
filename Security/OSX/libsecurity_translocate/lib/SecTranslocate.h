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

#ifndef H_LIBSECURITY_TRANSLOCATE
#define H_LIBSECURITY_TRANSLOCATE

#include <CoreFoundation/CoreFoundation.h>

CF_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

/*!
    @function SecTranslocateStartListening

    @abstract Initialize the SecTranslocate Library as the XPC Server, Disk Arbitration Listener, and Launch Services Notification listener

    @param error On error will be populated with an error object describing the failure (a posix domain error such as EINVAL)

    @result True on success False on failure
 */
Boolean SecTranslocateStartListening(CFErrorRef* __nullable error)
__OSX_AVAILABLE(10.12);

/*!
 @function SecTranslocateStartListeningWithOptions

 @abstract Initialize the SecTranslocate Library as the XPC Server, Disk Arbitration Listener, and Launch Services Notification listener

 @param options (currently unused) A dictionary of options that could impact server startup
 @param outError On error will be populated with an error object describing the failure (a posix domain error such as EINVAL)

 @result True on success False on failure
 */
Boolean SecTranslocateStartListeningWithOptions(CFDictionaryRef options, CFErrorRef * __nullable outError)
__OSX_AVAILABLE(10.12);

/*!
    @function SecTranslocateCreateSecureDirectoryForURL
 
    @abstract Create a CFURL pointing to a translocated location from which to access the directory specified by pathToTranslocate.

    @param pathToTranslocate URL of the directory to be accessed from a translocated location.
    @param destinationPath URL where the directory of interest should be translocated, or NULL for a random UUID location
    @param error On error will be populated with an error object describing the failure (a posix domain error such as EINVAL)

    @result A CFURL pointing to the translocated location of the directory.

    @discussion 
        Calls to this function and SecTranslocateDeleteSecureDirectory are serialized to ensure only one call to either
        is operating at a time.
        Translocations will be created in the calling users's DARWIN_USER_TEMPDIR/AppTranslocation/<UUID>
 
        pathToTranslocated is expected to be of the form /some/dir/myApp.app
        destinationPath is expected to be of the form /<DARWIN_USER_TEMPDIR>/AppTranslocation/<DIR>/d/myApp.app
        
        Resulting translocations are of the form /<DARWIN_USER_TEMPDIR>/AppTranslocation/<DIR>/d/myApp.app
            <DIR> will be a UUID if destinationPath isn't specified.
 
        If pathToTranslocate is in a quarantined mountpoint, the quarantine attributes will be propagated to the
            translocated location.
 
        pathToTranslocate will cause a failure if it doesn't resolve to a path that exists, or it exceeds MAXPATHLEN
 
        destinationPath will cause a failure if
            1. it doesn't match the app (last directory) specified by path to translocate
            2. it differs from an already existing mount location for pathToTranslocate
            3. It isn't in the user's current temp dir
            4. someone created a file with the same name as the provided path
            5. It doesn't match the form /<DARWIN_USER_TEMPDIR>/AppTranslocation/<DIR>/d/myApp.app

        pathToTranslocate is returned if it should not be translocated based on policy. It is retained if so it can be treated as a copy.

        This function can be run from any process. If the process is not the xpc server, then an xpc call is made.
 */
CFURLRef __nullable SecTranslocateCreateSecureDirectoryForURL (CFURLRef pathToTranslocate, CFURLRef __nullable destinationPath, CFErrorRef* __nullable error)
__OSX_AVAILABLE(10.12);

/*!
    @function SecTranslocateAppLaunchCheckin

    @abstract Register that a translocated pid is running

    @param pid the pid to register

    @discussion this function will log if there is a problem. The actual work is either sent to the server via xpc, or dispatched async.

        This function can be run from any process. If the process is not the xpc server, then an xpc call is made.
 */
void SecTranslocateAppLaunchCheckin(pid_t pid)
__OSX_AVAILABLE(10.12);

/*!
    @function SecTranslocateURLShouldRunTranslocated

    @abstract Implements policy to decide whether the entity defined by path should be run translocated

    @param path URL to the entity in question

    @param shouldTranslocate true if the path should be translocated, false otherwise

    @param error On error will be populated with an error object describing the failure (a posix domain error such as EINVAL)

    @result true on success, false on failure (on failure error is set if provided). shouldTranslocate gives the answer

    @discussion The policy is as follows:
        1. If path is already on a nullfs mountpoint - no translocation
        2. No quarantine attributes - no translocation
        3. If QTN_FLAG_DO_NOT_TRANSLOCATE is set or QTN_FLAG_TRANSLOCATE is not set - no translocations
        4. Otherwise, if QTN_FLAG_TRANSLOCATE is set - translocation

        This function can be called from any process or thread.
 */
Boolean SecTranslocateURLShouldRunTranslocated(CFURLRef path, bool* shouldTranslocate, CFErrorRef* __nullable error)
__OSX_AVAILABLE(10.12);

/*!
    @function SecTranslocateIsTranslocatedURL

    @abstract indicates whether the provided path is an original path or a translocated path

    @param path path to check

    @param isTranslocated true if the path is translocated, false otherwise

    @param error On error will be populated with an error object describing the failure (a posix domain error such as EINVAL)

    @result true on success, false on failure (on failure error is set if provided). isTranslocated gives the answer

    @discussion will return
        1. false and EPERM if the caller doesn't have read access to the path
        2. false and ENOENT if the path doesn't exist
        3. false and ENINVAL if the parameters are broken
        4. true and isTranslocated = true if the path is on a nullfs mount
        5. true and isTranslocated = false if the path is not on a nullfs mount

        If path is a symlink, the results will reflect whatever the symlink actually points to.

        This function can be called from any process or thread.
*/
Boolean SecTranslocateIsTranslocatedURL(CFURLRef path, bool* isTranslocated, CFErrorRef* __nullable error)
__OSX_AVAILABLE(10.12);

/*!
    @function SecTranslocateCreateOriginalPathForURL

    @abstract finds the original path to a file given a translocated path

    @param translocatedPath the path to look up

    @param error On error will be populated with an error object describing the failure (a posix domain error such as EINVAL)

    @result A valid, existant path, or NULL on error

    @discussion will return
        1. NULL and EPERM if the caller doesn't have read access to the path
        2. NULL and ENOENT if the path doesn't exist
        3. NULL and ENINVAL if the parameters are broken
        4. A retained copy of translocatedPath if it isn't translocated
        5. The real path to original untranslocated file/directory.

        If translocatedPath is a symlink, the results will reflect whatever the symlink actually points to.

        This function can be called from any process or thread.
*/
CFURLRef __nullable SecTranslocateCreateOriginalPathForURL(CFURLRef translocatedPath, CFErrorRef* __nullable error)
__OSX_AVAILABLE(10.12);

/*!
 @function SecTranslocateDeleteSecureDirectory
 
 @abstract Unmount the translocated directory structure and delete the mount point directory.
 
 @param translocatedPath a CFURL pointing to a translocated location.
 
 @param error On error will be populated with an error object describing the failure (a posix domain error such as EINVAL).
 
 @result true on success, false on error.
 
 @discussion This function will make sure that the translocatedPath belongs to the calling user before unmounting.
    After an unmount, this function will iterate through all the directories in the user's AppTranslocation directory
    and delete any that aren't currently mounted on.
    This function can only be called from the XPC Server. An error will be returned if this is called from any other process.
 
 */
Boolean SecTranslocateDeleteSecureDirectory(CFURLRef translocatedPath, CFErrorRef* __nullable error)
__OSX_AVAILABLE(10.12);

    
#ifdef __cplusplus
}
#endif

CF_ASSUME_NONNULL_END

#endif /* H_LIBSECURITY_TRANSLOCATE */
