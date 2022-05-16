/*
 * Copyright (c) 1998-2020 Apple Inc. All rights reserved.
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

#import <CoreFoundation/CoreFoundation.h>
#import "DADisk.h"
#import "DAFileSystem.h"
#import <Foundation/Foundation.h>
#import <UserFS/LiveFSUSBLocalStorageClient.h>
#import <LiveFS/LiveFSMountClient.h>
#import <LiveFS/LiveFS_LiveFileMounter.h>
#import "DALog.h"

int __DAMountUserFSVolume( void * parameter )
{
    int returnValue                            = 0;
    __DAFileSystemContext *context             = parameter;
    LiveFSUSBLocalStorageClient *userFSManager = nil;
    LiveFSMountClient           *mountClient   = nil;
    NSError *err                               = nil;
    NSArray *volumes                           = nil;
    
    NSString *fsType     = (__bridge NSString *)context->fileSystem;
    NSString *deviceName = (__bridge NSString *)context->deviceName;
    NSString *mountpoint = (__bridge NSString *)context->mountPoint;
    NSString *volumeName = (__bridge NSString *)context->volumeName;
    
    userFSManager = [LiveFSUSBLocalStorageClient newManager];
    mountClient = [LiveFSMountClient  newClientForProvider:@"com.apple.filesystems.UserFS.FileProvider"];
    
    volumes = [userFSManager loadVolumes:deviceName ofType:fsType withError:&err];
    
    if (err)
    {
        DALogError("%@ loadVolumes failed with %@", deviceName, err);
        goto exit;
    }

    if ( volumes )
    {
        if ( 1 == volumes.count )
        {
            NSString *UUID = volumes.firstObject;
            err = [ mountClient mountVolume:UUID
                                        displayName:volumeName
                                        provider:@"com.apple.filesystems.UserFS.FileProvider"
                                        on:mountpoint
                                        how:LIVEMOUNTER_MOUNT_DONTDOMAIN | LIVEMOUNTER_MOUNT_NOFOLLOW ];
            
            if ( err )
            {
                NSError *unloadError = nil;

                DALogError("%@ mount failed with %@", deviceName, err);
                unloadError = [userFSManager forgetVolume:UUID withFlags:0]; /* TODO: if this fails we need to do ungraceful as well */
                if (unloadError) {
                    DALogError(" unload for volume %@ failed with %@", volumeName, unloadError);
                }
            }
        }
        else
        {
            if (volumes.count > 1)
            {
                // Should not happen for now, we do not support it, so unload the volumes
                DALogError("%@ mount returned with more than one UUID", deviceName);
                NSString *UUID;
                for (UUID in volumes)
                {
                    NSError *unloadError = nil;
                    unloadError = [userFSManager forgetVolume:UUID withFlags:0];
                    if (unloadError) {
                        DALogError("unload for volume %@ failed with %@", UUID, unloadError);
                    }
                }
            } else {
                DALogError("%@ mount returned no usable volumes", deviceName);
            }
            err = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
        }
    }
        
exit:
    
    if (err)
    {
        returnValue = err.code;
    }
    [userFSManager release];
    [mountClient release];
    
    return returnValue;
}

