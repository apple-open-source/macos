/*
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
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

/** **This .m is NOT part of disklib. It must be added to each target's source list**  **/
/** **This behavior is because it depends on FSKit.framework**  **/
/** **This file must be compiled with ARC enabled.**  **/

#import "fskit_support.h"
#if __has_include(<FSKit/FSKit.h>)
#define HAS_FSKIT
#import <FSKit/FSKit.h>
#import <FSKit/FSKit_private.h>
#import <LiveFS/LiveFS.h>
#import <LiveFS/LiveFSMountManagerClient.h>

#import <err.h>
#else /* __has_include(<FSKit/FSKit.h>) */
#include <os/errno.h>
#endif /* __has_include(<FSKit/FSKit.h>) */


int
invoke_tool_from_fskit(fskit_command_t operation, int flags,
                       int argc, char * const *argv)
{
#ifdef HAS_FSKIT
            FSClient            *client;
            NSString            *shortName;
    __block FSModuleIdentity    *module;
    __block NSError             *error;

    if ([FSClient class] == nil) {
        // FSKit not present. Perhaps we are on an OS without it (darwin?)
        return ENOTSUP;
    }

#pragma mark - Find Extension
    client = [FSClient new];
    shortName = [NSString stringWithUTF8String:argv[0]];
    [client installedExtensionWithShortName:shortName
                                synchronous:TRUE
                               replyHandler:^(FSModuleIdentity * _Nullable mod,
                                              NSError * _Nullable err) {
        module = mod;
        error = err;
    }];

    if (error) {
        if (error.domain && [NSPOSIXErrorDomain isEqualToString:error.domain]
            && error.code == ENOENT) {
            //Not found
            return ENOENT;
        }
        // Something else went wrong.
        return EINVAL;
    }

#pragma mark - Parse command options
    NSDictionary        *attributes = module.attributes;
    NSDictionary        *commandOptions;
    FSTaskOptionsBundle *optionBundle;
    int                  expectedArgs;
    NSString            *operationName;

    switch (operation) {
        case check_fs_op:
            commandOptions = attributes[FSCheckOptionSyntaxKey];
            expectedArgs = 1;
            operationName = @"fsck";
            break;
        case format_fs_op:
            commandOptions = attributes[FSFormatOptionSyntaxKey];
            expectedArgs = 1;
            operationName = @"newfs";
            break;
        case mount_fs_op:
            commandOptions = attributes[FSActivateOptionSyntaxKey];
            expectedArgs = 2;
            operationName = @"mount";
            break;
        default:
            fprintf(stderr, "Internal error, operation type %d unrecognized\n", operation);
            return EINVAL;
            break;
    }

    if (!commandOptions) {
        warnx("Filesystem %s does not support operation %s",
              shortName.UTF8String, operationName.UTF8String);
        return ENOEXEC;
    }


    __block bool hitError = false;
    __block bool writable = false;   // How should we open a BlockDeviceResource?

    [FSTaskOptionsBundle resetOptionEnumeration];
    optionBundle = [FSTaskOptionsBundle bundleForArguments:argv
                                                     count:argc
                                          syntaxDictionary:commandOptions
                                              errorHandler:^(NSError * _Nonnull error,
                                                             NSString * _Nullable option) {
        hitError = true;
        warnx("Error %s parsing '%s'", error.description.UTF8String,
              option.UTF8String);
    }];

    if (hitError) {
        return EINVAL;
    }

    argv += optind;
    argc -= optind;

    if (argc != expectedArgs) {
        warnx("Argument count %d not equal to expected count %d", argc, expectedArgs);
        return EINVAL;
    }

    // Now figure out if it's read-only or not
    switch (operation) {
        case check_fs_op:
            // read-only if either -q or -n
            writable = true;
            [optionBundle enumerateOptionsWithBlock:^(int ch, NSString * _Nullable optarg,
                                                      NSUInteger idx, BOOL * _Nonnull stop) {
                if (ch == 'q' || ch == 'n') {
                    writable = false;
                    *stop = true;
                }
            }];
            break;

        case format_fs_op:
            writable = true;
            break;

        case mount_fs_op:
            // read-only if either -o ro or -o rdonly
            writable = true;
            [optionBundle enumerateOptionsWithBlock:^(int ch, NSString * _Nullable optarg,
                                                      NSUInteger idx, BOOL * _Nonnull stop) {
                if (ch != 'o' || optarg == nil) {
                    // Next!
                    return;
                }
                if ([optarg isEqualToString:@"ro"]
                        || [optarg isEqualToString:@"rdonly"]) {
                    writable = false;
                } else if ([optarg isEqualToString:@"rw"]) {
                    writable = true;
                }
            }];
            break;

        default:
            // ?? default is false, so go with that.
            break;
    }

#pragma mark - Set up resource and mount point string
    /*
     * Build our FSResource
     */
    NSString    *argv0String = [NSString stringWithUTF8String:argv[0]];
    FSResource  *theResource;
    NSString    *mountPathString;
    NSNumber    *num;
    bool acceptsBD =   ((num = attributes[@"FSSupportsBlockResources"])
                        && ([num isKindOfClass:[NSNumber class]])
                        && num.boolValue);
    bool acceptsPath = ((num = attributes[@"FSSupportsPathURLs"])
                        && ([num isKindOfClass:[NSNumber class]])
                        && num.boolValue);

    if (acceptsBD) {
        theResource = [FSBlockDeviceResource proxyResourceForBSDName:argv0String
                                                            writable:writable];
    } else if (acceptsPath) {
        bool securityScoped;
        NSURL   *url;

        securityScoped = ((num = attributes[@"FSRequiresSecurityScopedPathURLResources"])
                          && ([num isKindOfClass:[NSNumber class]])
                          && num.boolValue);
        url = [NSURL fileURLWithPath:argv0String];
        if (securityScoped) {
            theResource = [FSPathURLResource secureResourceWithURL:url
                                                          readonly:!writable];
        } else {
            theResource = [FSPathURLResource resourceWithURL:url];
        }
    } else {
        warnx("Filesystem %s supports neither Block Device nor PathURL resources.",
              shortName.UTF8String);
        return EINVAL;
    }

    if (operation == mount_fs_op) {
        mountPathString = [NSString stringWithUTF8String:argv[1]];
    }

#pragma mark - Perform the operation
    FSTaskOptionsBundle     *loadOptions = [FSTaskOptionsBundle new];
    FSContainerIdentifier   *containerID;
    __block NSArray <FSVolumeDescription *> *outVols;

    if (operation == format_fs_op || operation == check_fs_op) {
        containerID = [NSUUID UUID].fs_containerIdentifier;
    } else {
        __block FSProbeResult       *probeResult;
        [client probeResourceSync:theResource
                      usingBundle:module.bundleIdentifier
                     replyHandler:^(FSProbeResult * _Nullable res, NSError * _Nullable err) {
            error = err;
            probeResult = res;
        }];

        if (error) {
            warnx("Probing resource: %s", error.localizedDescription.UTF8String);
            return EIO;
        }
        containerID = probeResult.containerID;

        /* ***** prep/probe done. Now load ***** */

        // Really should load a resource into a given container. Can't do that yet.
        [client loadResource:theResource
                   shortName:shortName
                     options:loadOptions
     synchronousReplyHandler:^(NSArray <FSVolumeDescription *> * _Nullable volumes,
                               NSError * _Nullable err) {
            error = err;
            outVols = volumes;
        }];
        if (error) {
            warnx("Loading resource: %s", error.localizedDescription.UTF8String);
            return (int)error.code;
        }
    }

    FSMessageReceiver   *receiver;
    FSMessageConnection *conn;
    dispatch_group_t     theGroup;
    __block bool didCallLeave = false;

    theGroup = dispatch_group_create();
    receiver = [FSMessageReceiver receiverForStandardIO:^(NSError * _Nullable err) {
        // Called sometime AFTER the operation successfully starts. Not
        // called if operation never starts
        if (err) {
            warnx("Operation ended with error: %s", err.localizedDescription.UTF8String);
            error = err;
        }
        if (!didCallLeave) {
            dispatch_group_leave(theGroup);
            didCallLeave = true;
        }
    }];
    conn = [receiver getConnection];

    /* ***** Now do the op ***** */

    dispatch_group_enter(theGroup); // Once for the operation
    if (operation == format_fs_op) {
        [client formatResource:theResource
                   usingBundle:module.bundleIdentifier
                       options:optionBundle
                    connection:conn
                  replyHandler:^(NSUUID * _Nullable taskID, NSError * _Nullable err)
         {
            // dispatch_group_leave(theGroup); Hand off to the connection
            if (err) {
                warnx("Operation ended with error: %s", err.localizedDescription.UTF8String);
                error = err;
                dispatch_group_leave(theGroup); // connection didn't start, never completes
                didCallLeave = true;
            }
        }];
    } else if (operation == check_fs_op) {
        [client checkResource:theResource
                  usingBundle:module.bundleIdentifier
                      options:optionBundle
                   connection:conn
                 replyHandler:^(NSUUID * _Nullable taskID, NSError * _Nullable err)
         {
            // dispatch_group_leave(theGroup); Hand off to the connection
            if (err) {
                warnx("%s:%d: Operation ended with error: %s", __FUNCTION__, __LINE__, err.localizedDescription.UTF8String);
                error = err;
                dispatch_group_leave(theGroup); // connection didn't start, never completes
                didCallLeave = true;
            }
        }];
    } else if (operation == mount_fs_op) {
        if (!outVols || outVols.count == 0) {
            warnx("Operation did not add any volumes");
            return EINVAL;
        }
        [client activateVolume:outVols[0].volumeID
                     shortName:shortName
                       options:optionBundle
                  replyHandler:^(NSError * _Nullable err)
         {
            if (err) {
                warnx("Operation ended with error: %s", err.localizedDescription.UTF8String);
                error = err;
            }
            dispatch_group_leave(theGroup);
            didCallLeave = true;
        }];
    }
    dispatch_group_wait(theGroup, DISPATCH_TIME_FOREVER);
    if (error) {
        warnx("operation exiting with error %s", error.description.UTF8String);
        return (int)error.code;
    }

    if (operation == mount_fs_op) {
        // Successfully activated volume service. Tell fskitd to mount it
        LiveFSMountClient   *mountClient;
        NSString            *fpProviderName;
        NSString            *volumeID;

        fpProviderName = @"com.apple.filesystems.UserFS.FileProvider";;
        mountClient = [LiveFSMountClient newClientForProvider:fpProviderName];
        volumeID = outVols[0].volumeID.uuid.description;

#if TARGET_OS_IOS
        /*
         * On iOS, we mount volumes under Library/LiveFiles/..UserFS.FileProvider/UUID.
         * That's a lot to type on a CLI. Use an empty string passed in as shorthand for
         * Filling that in here. Use the fact that relative paths get ~/Library/LiveFiles
         * prepended.
         */
        if (mountPathString.length == 0) {
            // Empty string.
            mountPathString = [NSString stringWithFormat:@"%@/%@",
                               fpProviderName, volumeID];
        }
#endif /* TARGET_OS_IOS */

        error = [mountClient mountVolume:volumeID
                              fileSystem:shortName
                             displayName:outVols[0].volumeName
                                provider:fpProviderName
                             domainError:nil
                                      on:mountPathString
                                     how:0
                                 options:@""];
        if (error) {
            __block NSError *error1;

            warnx("Final mount step ended with error: %s",
                  error.localizedDescription.UTF8String);
            dispatch_group_enter(theGroup);
            [client deactivateVolume:outVols[0].volumeID
                           shortName:shortName
                             options:[FSTaskOptionsBundle new]
                        replyHandler:^(NSError * _Nullable err) {
                error1 = err;
                dispatch_group_leave(theGroup);
                didCallLeave = true;
            }];
            dispatch_group_wait(theGroup, DISPATCH_TIME_FOREVER);

            if (error1) {
                warnx("Final mount step cleanup ended with error: %s",
                      error1.localizedDescription.UTF8String);
            }

            return (int)error.code;
        }
    } else {
        // Successfully formatted or checked. Unload the resource
    }

    return 0;
#else
    return ENOTSUP;
#endif /* HAS_FSKIT */
}
