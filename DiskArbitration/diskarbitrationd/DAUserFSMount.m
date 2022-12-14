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
#import "DALog.h"
#if TARGET_OS_OSX || TARGET_OS_IOS
#import <UserFS/LiveFSUSBLocalStorageClient.h>
#import <LiveFS/LiveFSMountManagerClient.h>
#import <LiveFS/LiveFS_LiveFileMounter.h>
#import <os/log.h>


int __DAMountUserFSVolume( void * parameter )
{
    int returnValue                            = 0;
    __DAFileSystemContext *context             = parameter;
    LiveFSUSBLocalStorageClient *userFSManager = nil;
    LiveFSMountClient           *mountClient   = nil;
    NSError *err                               = nil;
    NSArray *volumes                           = nil;

    NSString *fsType       = (__bridge NSString *)context->fileSystem;
    NSString *deviceName   = (__bridge NSString *)context->deviceName;
    NSString *mountpoint   = (__bridge NSString *)context->mountPoint;
    NSString *volumeName   = (__bridge NSString *)context->volumeName;
    NSString *mountOptions = (__bridge NSString *)context->mountOptions;

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
#if TARGET_OS_OSX
        if ( 1 == volumes.count )
        {
            NSDictionary *volumeInfo = volumes.firstObject;
            NSString *UUID = [volumeInfo objectForKey:(@"UUID")];
            NSString *volName = [volumeInfo objectForKey:(@"name")];

            DALogInfo("%@ mounting with name %@ and options %@", UUID, volName, mountOptions);

            err = [mountClient mountVolume:UUID displayName:volName
                                  provider:@"com.apple.filesystems.UserFS.FileProvider"
                               domainError:nil
                                        on:mountpoint
                                       how:LIVEMOUNTER_MOUNT_DONTDOMAIN | LIVEMOUNTER_MOUNT_NOFOLLOW
                                   options:mountOptions ];
            
            if ( err )
            {
                NSError *unloadError = nil;

                DALogError("%@ mount failed with %@", deviceName, err);
                unloadError = [userFSManager forgetVolume:UUID withFlags:0]; /* TODO: if this fails we need to do ungraceful as well */
                if (unloadError) {
                    DALogError("unload for volume %@ failed with %@", volName, unloadError);
                }
            }
        }
        else
        {
            if (volumes.count > 1)
            {
                // Should not happen for now, we do not support it, so unload the volumes
                DALogError("%@ mount returned with more than one UUID", deviceName);
                for (NSDictionary *volume in volumes)
                {
                    NSError *unloadError = nil;
                    unloadError = [userFSManager forgetVolume:volume[@"UUID"] withFlags:0];
                    if (unloadError) {
                        DALogError("unload for volume %@ failed with %@", volume[@"name"], unloadError);
                    }
                }
            } else {
                DALogError("%@ mount returned no usable volumes", deviceName);
            }
            err = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
        }
#else
        NSDictionary *volumeInfo;
        for (volumeInfo in volumes)
        {
            NSError *errorForDomain = nil;
            int mountFlags = 0;
            NSString *UUID = [volumeInfo objectForKey:(@"UUID")];
            NSString *volName = [volumeInfo objectForKey:(@"name")];

            mountFlags |= [volumeInfo[@"how"] intValue];

            if ( volumeInfo[@"errorForDomain"] )
            {
                long code = [volumeInfo[@"errorForDomain"] integerValue];
                if (code == -1000)
                {
                    //TODO: We need to check if we can link again FileProvider
                    errorForDomain = [NSError errorWithDomain:NSPOSIXErrorDomain code:EAUTH userInfo:nil];
                } else
                {
                    DALogError("unsupported error code for domain: %d", code);
                    err = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
                    goto exit;
                }
            }
            DALogInfo("%@ mounting with name %@, error %@, and how 0x%x.", UUID, volName, errorForDomain, mountFlags);

            err = [mountClient mountVolume:UUID
                               displayName:volName
                                  provider:@"com.apple.filesystems.UserFS.FileProvider"
                               domainError:errorForDomain
                                        on:mountpoint
                                       how:mountFlags];

            if ( err )
            {
                DALogError("%@ mount failed with %@", deviceName, err);
                goto exit;
            }
            DALogInfo("Mounted %@ successfully.", UUID);
        }
#endif
    }
        
exit:
    
    if (err)
    {
        returnValue = (int)err.code;
    }
    [userFSManager release];
    [mountClient release];
    
    return returnValue;
}

int DAUserFSOpen( char *path, int flags )
{
    int fd = -1;


    xpc_connection_t server = xpc_connection_create_mach_service( "com.apple.filesystems.userfs_helper", NULL, 0 );
    assert( server != NULL );
    assert( xpc_get_type(server) == XPC_TYPE_CONNECTION );
    xpc_connection_set_event_handler( server, ^(xpc_object_t object) { /* do nothing */ } );
    xpc_connection_resume( server );

    xpc_object_t msg = xpc_dictionary_create( NULL, NULL, 0 );
    xpc_dictionary_set_string( msg, "path", path );
    xpc_dictionary_set_int64 (msg, "flags", flags );

    xpc_object_t reply = xpc_connection_send_message_with_reply_sync( server, msg );
    if ( reply != NULL && xpc_get_type(reply) == XPC_TYPE_DICTIONARY )
    {
        fd = xpc_dictionary_dup_fd( reply, "fd" );
        if ( fd < 0 )
        {
            int64_t error = xpc_dictionary_get_int64( reply, "error" );
            DALogInfo( "open:error:%d", (int)error );
            if ( error == 0 )
            {
                error = EIO;
            }
            errno = (int)error;
        }
    }
    else
    {
        DALogInfo( "open:invalidReply:%{public}s", reply ? xpc_copy_description(reply) : "NULL" );
        errno = EIO;
    }
    
    xpc_connection_cancel( server );
    xpc_release( msg );
    xpc_release( server );
    xpc_release( reply );

    return fd;
}
#endif
