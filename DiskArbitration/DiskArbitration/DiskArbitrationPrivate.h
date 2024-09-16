/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
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

#ifndef __DISKARBITRATION_DISKARBITRATIONPRIVATE__
#define __DISKARBITRATION_DISKARBITRATIONPRIVATE__

#include <dispatch/dispatch.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <os/availability.h>
#include <mach/mach.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if TARGET_OS_OSX || TARGET_OS_IOS
#define DA_FSKIT
#define TARGET_FSKIT    1
#else
#define TARGET_FSKIT    0
#endif

#ifndef __DISKARBITRATIOND__
#ifndef __LP64__

enum
{
    kDA_DISK_APPEARED                        = 1,
    kDA_DISK_UNMOUNT_PRE_NOTIFY              = 2,
    kDA_DISK_UNMOUNT_POST_NOTIFY             = 3,
    kDA_DISK_EJECT_PRE_NOTIFY                = 4,
    kDA_DISK_EJECT_POST_NOTIFY               = 5,
    kDA_CLIENT_DISCONNECTED                  = 6,
    kDA_DISK_CHANGED                         = 7,
    kDA_NOTIFICATIONS_COMPLETE               = 8,
    kDA_WILL_CLIENT_RELEASE_DEVICE           = 9,
    kDA_DEVICE_RESERVATION_STATUS            = 10,
    kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK = 11,
    kDA_DISK_APPEARED1                       = 12,
    kDA_DISK_APPEARED_WITH_MT                = 13,
    kDA_DISK_WILL_BE_CHECKED                 = 21,
    kDA_CALL_FAILED                          = 22,
    kDA_CALL_SUCCEEDED                       = 23,
    kDA_DISK_APPROVAL_NOTIFY                 = 24,
    kDA_DISK_APPEARED_COMPLETE               = 25
};

enum
{
    kDiskArbDeviceIsReserved          = 0,
    kDiskArbDeviceIsNotReserved       = 1,
    kDiskArbDeviceReservationObtained = 2,
    kDiskArbDeviceReservationRefused  = 3
};

enum
{
    kDiskArbDiskAppearedLockedMask                 = 1 << 0,
    kDiskArbDiskAppearedEjectableMask              = 1 << 1,
    kDiskArbDiskAppearedWholeDiskMask              = 1 << 2,
    kDiskArbDiskAppearedNetworkDiskMask            = 1 << 3,
    kDiskArbDiskAppearedBeingCheckedMask           = 1 << 4,
    kDiskArbDiskAppearedNonLeafDiskMask            = 1 << 5,
    kDiskArbDiskAppearedCDROMMask                  = 1 << 6,
    kDiskArbDiskAppearedDVDROMMask                 = 1 << 7,
    kDiskArbDiskAppearedUnrecognizableFormat       = 1 << 8,
    kDiskArbDiskAppearedUnrecognizableSection      = 1 << 9,
    kDiskArbDiskAppearedRecognizableSectionMounted = 1 << 10,
    kDiskArbDiskAppearedDialogDisplayed            = 1 << 11,
    kDiskArbDiskAppearedNoMountMask                = 1 << 12,
    kDiskArbDiskAppearedNoSizeMask                 = 1 << 13,
    kDiskArbDiskAppearedCheckFailed                = 1 << 14,
    kDiskArbDiskAppearedInternal                   = 1 << 15,
    kDiskArbDiskAppearedBDROMMask                  = 1 << 16
};

enum
{
    kDiskArbApproveMounting       = 0,
    kDiskArbDisallowMounting      = 1 << 0,
    kDiskArbEjectDevice           = 1 << 2,
    kDiskArbRequireAuthentication = 1 << 3,
    kDiskArbMountReadOnly         = 1 << 4
};

enum
{
    kDiskArbUnmountAllFlag      = 1 << 0,
    kDiskArbUnmountAndEjectFlag = 1 << 1,
    kDiskArbUnmountOneFlag      = 1 << 2,
    kDiskArbForceUnmountFlag    = 1 << 3,
    kDiskArbNetworkUnmountFlag  = 1 << 4
};

enum
{
    kDiskArbCompletedDiskAppeared = 1 << 0,
    kDiskArbCompletedPreUnmount   = 1 << 1,
    kDiskArbCompletedPostUnmount  = 1 << 2,
    kDiskArbCompletedPreEject     = 1 << 3,
    kDiskArbCompletedPostEject    = 1 << 4
};

enum
{
    kDiskArbRenameSuccessful      = 1 << 0,
    kDiskArbRenameRequiresRemount = 1 << 1
};

enum
{
    kDiskArbHandlesUnrecognizedFixedMedia           = 1 << 0,
    kDiskArbHandlesUnrecognizedCDMedia              = 1 << 1,
    kDiskArbHandlesUnrecognizedDVDMedia             = 1 << 2,
    kDiskArbHandlesUnrecognizedOtherRemovableMedia  = 1 << 3,
    kDiskArbHandlesUnrecognizedBDMedia              = 1 << 4,
    kDiskArbHandlesUninitializedFixedMedia          = 1 << 10,
    kDiskArbHandlesUninitializedCDMedia             = 1 << 11,
    kDiskArbHandlesUninitializedDVDMedia            = 1 << 12,
    kDiskArbHandlesUninitializedOtherRemovableMedia = 1 << 13,
    kDiskArbHandlesUninitializedBDMedia             = 1 << 14
};

enum
{
    kDiskArbHandlesAllUnrecognizedMedia                = 0x000000FF,
    kDiskArbHandlesAllUninitializedMedia               = 0x0000FF00,
    kDiskArbHandlesAllUnrecognizedOrUninitializedMedia = 0x0000FFFF,
};

enum
{
    kDiskArbUnmountRequestFailed         = 1 << 0,
    kDiskArbEjectRequestFailed           = 1 << 1,
    kDiskArbUnmountAndEjectRequestFailed = 1 << 2,
    kDiskArbDiskChangeRequestFailed      = 1 << 3
};

enum
{
    kDiskVolumeDevEncrypted         = 0x1, // device-level encryption
    kDiskVolumeFdeEncrypted         = 0x2, // full-disk encryption
    kDiskVolumeEncryptionConverting = 0x4, // encryption type is currently in flux
};

typedef char DiskArbDeviceTreePath[1024];
typedef char DiskArbDiskIdentifier[1024];
typedef char DiskArbGenericString[1024];
typedef char DiskArbIOContent[1024];
typedef char DiskArbMountpoint[1024];

typedef void ( *DiskArbCallback_CallFailedNotification_t )( char * disk, int type, int status );

typedef void ( *DiskArbCallback_CallSucceededNotification_t )( char * disk, int type );

typedef void ( *DiskArbCallback_ClientDisconnectedNotification_t )( void );

typedef void ( *DiskArbCallback_Device_Reservation_Status_t )( char * disk, int status, int pid );

typedef int  ( *DiskArbCallback_DiskAppeared_t )( char * disk, unsigned flags, char * mountpoint, char * content );

typedef void ( *DiskArbCallback_DiskAppeared2_t )( char *   disk,
                                                   unsigned flags,
                                                   char *   mountpoint,
                                                   char *   content,
                                                   char *   path,
                                                   unsigned sequence );

typedef void ( *DiskArbCallback_DiskAppearedComplete_t )( char *   disk,
                                                          unsigned flags,
                                                          char *   mountpoint,
                                                          char *   content,
                                                          char *   path,
                                                          unsigned sequence,
                                                          double   time,
                                                          char *   filesystem,
                                                          char *   name );

typedef int  ( *DiskArbCallback_DiskAppearedWithMountpoint_t )( char * disk, unsigned flags, char * mountpoint );

typedef void ( *DiskArbCallback_DiskApprovalNotification_t )( char *   disk,
                                                              char *   name,
                                                              char *   content,
                                                              char *   path,
                                                              unsigned flags,
                                                              int      writable,
                                                              int      removable,
                                                              int      whole,
                                                              char *   filesystem );

typedef void ( *DiskArbCallback_DiskChangedNotification_t )( char * disk, char * mountpoint, char * name, int flags, int success );

typedef void ( *DiskArbCallback_EjectPostNotification_t )( char * disk, int status, pid_t dissenter );

typedef void ( *DiskArbCallback_EjectPreNotification_t )( char * disk, unsigned reserved0032 );

typedef void ( *DiskArbCallback_DiskWillBeCheckedNotification_t )( char * disk, int flags, char * content );

typedef void ( *DiskArbCallback_NotificationComplete_t )( int type );

typedef void ( *DiskArbCallback_UnknownFileSystemNotification_t )( char * disk,
                                                                   char * reserved0032,
                                                                   char * reserved0064,
                                                                   int    writable,
                                                                   int    removable,
                                                                   int    whole );

typedef void ( *DiskArbCallback_UnmountPostNotification_t )( char * disk, int status, pid_t dissenter );

typedef void ( *DiskArbCallback_UnmountPreNotification_t )( char * disk, unsigned reserved0032 );

typedef void ( *DiskArbCallback_Will_Client_Release_t )( char * disk, int pid );

typedef void ( *DiskArbCallback_Will_Client_Handle_Unrecognized_Disk_t )( char * disk,
                                                                          int    type,
                                                                          char * reserved0064,
                                                                          char * reserved0096,
                                                                          int    writable,
                                                                          int    removable,
                                                                          int    whole );

extern void          DiskArbAddCallbackHandler( int type, void * callback, int overwrite );
extern kern_return_t DiskArbClientHandlesUninitializedDisks_auto( int yes );
extern kern_return_t DiskArbClientHandlesUnrecognizedDisks( int types, int priority );
extern kern_return_t DiskArbClientRelinquishesReservation( char * disk, int pid, int status );
extern kern_return_t DiskArbClientWillHandleUnrecognizedDisk( char * disk, int yes );
extern kern_return_t DiskArbDiskAppearedWithMountpointPing_auto( char * disk, unsigned reserved0032, char * mountpoint );
extern kern_return_t DiskArbDiskApprovedAck_auto( char * disk, int status );
extern kern_return_t DiskArbDiskDisappearedPing_auto( char * disk, unsigned reserved0032 );
extern kern_return_t DiskArbEjectPreNotifyAck_async_auto( char * disk, int status );
extern kern_return_t DiskArbEjectRequest_async_auto( char * disk, unsigned flags );
extern int           DiskArbGetVolumeEncoding_auto( char * disk );
extern boolean_t     DiskArbHandleMsg( mach_msg_header_t * message, mach_msg_header_t * reply );
extern kern_return_t DiskArbInit( void );
extern int           DiskArbIsActive( void );
extern kern_return_t DiskArbIsDeviceReservedForClient( char * disk );
extern kern_return_t DiskArbMsgLoop( void );
extern kern_return_t DiskArbMsgLoopWithTimeout( mach_msg_timeout_t timeout );
extern void          DiskArbNoOp( void );
extern kern_return_t DiskArbRefresh_auto( void );
extern void          DiskArbRegisterCallback_CallFailedNotification( DiskArbCallback_CallFailedNotification_t callback );
extern void          DiskArbRegisterCallback_CallSucceededNotification( DiskArbCallback_CallSucceededNotification_t callback );
extern void          DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbCallback_ClientDisconnectedNotification_t callback );
extern void          DiskArbRegisterCallback_DiskAppeared( DiskArbCallback_DiskAppeared_t callback );
extern void          DiskArbRegisterCallback_DiskAppeared2( DiskArbCallback_DiskAppeared2_t callback );
extern void          DiskArbRegisterCallback_DiskAppearedWithMountpoint( DiskArbCallback_DiskAppearedWithMountpoint_t callback );
extern void          DiskArbRegisterCallback_DiskChangedNotification( DiskArbCallback_DiskChangedNotification_t callback );
extern void          DiskArbRegisterCallback_DiskWillBeCheckedNotification( DiskArbCallback_DiskWillBeCheckedNotification_t callback );
extern void          DiskArbRegisterCallback_EjectPostNotification( DiskArbCallback_EjectPostNotification_t callback );
extern void          DiskArbRegisterCallback_EjectPreNotification( DiskArbCallback_EjectPreNotification_t callback );
extern void          DiskArbRegisterCallback_NotificationComplete( DiskArbCallback_NotificationComplete_t callback );
extern void          DiskArbRegisterCallback_UnknownFileSystemNotification( DiskArbCallback_UnknownFileSystemNotification_t callback );
extern void          DiskArbRegisterCallback_UnmountPostNotification( DiskArbCallback_UnmountPostNotification_t callback );
extern void          DiskArbRegisterCallback_UnmountPreNotification( DiskArbCallback_UnmountPreNotification_t callback );
extern kern_return_t DiskArbReleaseClientReservationForDevice( char * disk );
extern void          DiskArbRemoveCallbackHandler( int type, void * callback );
extern kern_return_t DiskArbRequestDiskChange_auto( char * disk, char * name, int flags );
extern kern_return_t DiskArbRequestMount_auto( char * disk );
extern kern_return_t DiskArbRequestMountAndOwn_auto( char * disk );
extern kern_return_t DiskArbRetainClientReservationForDevice( char * disk );
extern kern_return_t DiskArbSetCurrentUser_auto( int user );
extern kern_return_t DiskArbSetVolumeEncoding_auto( char * disk, int encoding );
extern kern_return_t DiskArbStart( mach_port_t * port );
extern kern_return_t DiskArbUnmountAndEjectRequest_async_auto( char * disk, unsigned flags );
extern kern_return_t DiskArbUnmountPreNotifyAck_async_auto( char * disk, int status );
extern kern_return_t DiskArbUnmountRequest_async_auto( char * disk, unsigned flags );
extern void          DiskArbUpdateClientFlags( void );
extern kern_return_t DiskArbVSDBAdoptVolume_auto( char * disk );
extern kern_return_t DiskArbVSDBDisownVolume_auto( char * disk );
extern int           DiskArbVSDBGetVolumeStatus_auto( char * disk );

#endif /* !__LP64__ */
#endif /* !__DISKARBITRATIOND__ */

extern const CFStringRef kDAApprovalRunLoopMode API_AVAILABLE(macCatalyst(13.0)) API_UNAVAILABLE(ios, tvos, watchos);

extern const CFStringRef kDADiskDescriptionAppearanceTimeKey;

extern const CFStringRef kDADiskDescriptionMediaMatchKey;

extern const CFStringRef kDADiskDescriptionVolumeLifsURLKey;   /* ( CFString     ) */

#ifndef __DISKARBITRATIOND__

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
extern int _DAmkdir( const char * path, mode_t mode );

extern int _DArmdir( const char * path );
#endif

extern DADiskRef _DADiskCreateFromSerialization( CFAllocatorRef allocator, DASessionRef session, CFDataRef serialization );

extern DASessionRef _DADiskGetSession( DADiskRef disk );

extern DAReturn _DADiskSetAdoption( DADiskRef disk, Boolean adoption );

extern DAReturn _DADiskSetEncoding( DADiskRef disk, UInt32 encoding );

extern pid_t DADissenterGetProcessID( DADissenterRef dissenter );

typedef void ( *DAIdleCallback )( void * context );

extern void DARegisterIdleCallback( DASessionRef session, DAIdleCallback callback, void * context );

typedef void ( *DADiskListCompleteCallback )( void * context );

/*!
 * @function   DARegisterDiskListCompleteCallback
 * @abstract   Registers a callback function to be called when the initial stream of disk appeared events are complete
 * @abstract   for a session. Needs to be registered before registering DARegisterDiskAppearedCallback.
 * @param      session  The session object.
 * @param      callback The registered callback function.
 * @param      context  The user-defined context parameter to pass to the callback function.
 */

extern void DARegisterDiskListCompleteCallback( DASessionRef session, DADiskListCompleteCallback  callback, void * context );

extern DAReturn DASessionKeepAlive( DASessionRef session , dispatch_queue_t queue);


/*!
 * @typedef    DADiskAppearedCallbackBlock
 * @abstract   Type of the callback block used by DARegisterDiskAppearedCallbackBlock().
 * @param      disk    A disk object.
 */

typedef void ( ^DADiskAppearedCallbackBlock )( DADiskRef disk );

/*!
 * @function   DARegisterDiskAppearedCallbackBlock
 * @abstract   Registers a callback block to be called whenever a disk has appeared.
 * @param      session  The session object.
 * @param      match    The disk description keys to match.  Pass NULL for all disk objects.
 * @param      callback The callback block to call when a disk has appeared.
 */

extern void DARegisterDiskAppearedCallbackBlock( DASessionRef               session,
                                                    CFDictionaryRef __nullable match,
                                                    DADiskAppearedCallbackBlock     callback );
                                         
/*!
 * @typedef    DADiskDescriptionChangedCallbackBlock
 * @abstract   Type of the callback block used by DARegisterDiskDescriptionChangedCallbackBlock().
 * @param      disk    A disk object.
 * @param      keys    A list of changed keys.
 */

typedef void ( ^DADiskDescriptionChangedCallbackBlock )( DADiskRef disk, CFArrayRef keys );

/*!
 * @function   DARegisterDiskDescriptionChangedCallbackBlock
 * @abstract   Registers a callback block to be called whenever a disk description has changed.
 * @param      session  The session object.
 * @param      match    The disk description keys to match.  Pass NULL for all disk objects.
 * @param      watch    The disk description keys to watch.  Pass NULL for all keys.
 * @param      callback The callback block to call when a watched key changes.
 */

extern void DARegisterDiskDescriptionChangedCallbackBlock( DASessionRef                session,
                                                            CFDictionaryRef __nullable       match,
                                                            CFArrayRef __nullable            watch,
                                                            DADiskDescriptionChangedCallbackBlock callback );

/*!
 * @typedef    DADiskDisappearedCallbackBlock
 * @abstract   Type of the callback block used by DARegisterDiskDisappearedCallbackBlock().
 * @param      disk    A disk object.
 */

typedef void ( ^DADiskDisappearedCallbackBlock )( DADiskRef disk );

/*!
 * @function   DARegisterDiskDisappearedCallbackBlock
 * @abstract   Registers a callback block to be called whenever a disk has disappeared.
 * @param      session  The session object.
 * @param      match    The disk description keys to match.  Pass NULL for all disk objects.
 * @param      callback The callback block to call when a disk has disappeared.
 */

extern void DARegisterDiskDisappearedCallbackBlock( DASessionRef               session,
                                                    CFDictionaryRef __nullable match,
                                                    DADiskDisappearedCallbackBlock  callback );
                                                     

/*!
 * @typedef    DADiskMountCallbackBlock
 * @abstract   Type of the callback block used by DADiskMount().
 * @param      disk      The disk object.
 * @param      dissenter A dissenter object on failure or NULL on success.
 * @discussion
 * If the disk is already mounted, then status code in the dissenter object will be set to kDAReturnBusy
 */

typedef void ( ^DADiskMountCallbackBlock )( DADiskRef disk, DADissenterRef __nullable dissenter );

/*!
 * @function   DADiskMountWithBlock
 * @abstract   Mounts the volume at the specified disk object.
 * @param      disk     The disk object.
 * @param      path     The mount path.  Pass NULL for a "standard" mount path.
 * @param      options  The mount options.
 * @param      callback The callback block to call once the mount completes.
 */

extern void DADiskMountWithBlock( DADiskRef                      disk,
                                    CFURLRef __nullable            path,
                                    DADiskMountOptions             options,
                                    DADiskMountCallbackBlock __nullable callback );

/*!
 * @function   DADiskMountWithArgumentsAndBlock
 * @abstract   Mounts the volume at the specified disk object, with the specified mount options.
 * @param      disk      The disk object.
 * @param      path      The mount path.  Pass NULL for a "standard" mount path.
 * @param      options   The mount options.
 * @param      callback  The callback block to call once the mount completes.
 * @param      arguments The null-terminated list of mount options to pass to /sbin/mount -o.
 */

extern void DADiskMountWithArgumentsAndBlock( DADiskRef                      disk,
                                                CFURLRef __nullable            path,
                                                DADiskMountOptions             options,
                                                DADiskMountCallbackBlock __nullable callback,
                                                CFStringRef __nullable         arguments[_Nullable] );

/*!
 * @typedef    DADiskMountApprovalCallbackBlock
 * @abstract   Type of the callback block used by DARegisterDiskMountApprovalCallbackBlock().
 * @param      disk    A disk object.
 * @result     A dissenter reference.  Pass NULL to approve.
 * @discussion
 * The caller of this callback receives a reference to the returned object.  The
 * caller also implicitly retains the object and is responsible for releasing it
 * with CFRelease().
 */

typedef DADissenterRef __nullable ( ^DADiskMountApprovalCallbackBlock )( DADiskRef disk );
/*!
 * @function   DARegisterDiskMountApprovalCallback
 * @abstract   Registers a callback block to be called whenever a volume is to be mounted.
 * @param      session  The session object.
 * @param      match    The disk description keys to match.  Pass NULL for all disk objects.
 * @param      callback The callback block to call when a volume is to be mounted.
 */

extern void DARegisterDiskMountApprovalCallbackBlock( DASessionRef                session,
                                                        CFDictionaryRef __nullable  match,
                                                        DADiskMountApprovalCallbackBlock callback );
                                                        

/*!
 * @typedef    DADiskRenameCallbackBlock
 * @abstract   Type of the callback block used by DADiskRenameWithBlock().
 * @param      disk      The disk object.
 * @param      dissenter A dissenter object on failure or NULL on success.
 */

typedef void ( ^DADiskRenameCallbackBlock )( DADiskRef disk, DADissenterRef __nullable dissenter );

/*!
 * @function   DADiskRenameWithBlock
 * @abstract   Renames the volume at the specified disk object.
 * @param      disk     The disk object.
 * @param      options  The rename options.
 * @param      callback The callback block to call once the rename completes.
 */

extern void DADiskRenameWithBlock( DADiskRef                       disk,
                                    CFStringRef                     name,
                                    DADiskRenameOptions             options,
                                  DADiskRenameCallbackBlock __nullable callback );

/*!
 * @typedef    DADiskUnmountCallbackBlock
 * @abstract   Type of the callback block used by DADiskUnmountWithBlock().
 * @param      disk      The disk object.
 * @param      dissenter A dissenter object on failure or NULL on success.
 */

typedef void ( ^DADiskUnmountCallbackBlock )( DADiskRef disk, DADissenterRef __nullable dissenter );

/*!
 * @function   DADiskUnmountWithBlock
 * @abstract   Unmounts the volume at the specified disk object.
 * @param      disk     The disk object.
 * @param      options  The unmount options.
 * @param      callback The callback block to call once the unmount completes.
 */

extern void DADiskUnmountWithBlock( DADiskRef                        disk,
                                    DADiskUnmountOptions             options,
                                    DADiskUnmountCallbackBlock __nullable callback );

/*!
 * @typedef    DADiskUnmountApprovalCallbackBlock
 * @abstract   Type of the callback block used by DARegisterDiskUnmountApprovalCallbackBlock().
 * @param      disk    A disk object.
 * @result     A dissenter reference.  Pass NULL to approve.
 * @discussion
 * The caller of this callback receives a reference to the returned object.  The
 * caller also implicitly retains the object and is responsible for releasing it
 * with CFRelease().
 */

typedef DADissenterRef __nullable ( ^DADiskUnmountApprovalCallbackBlock )( DADiskRef disk );

/*!
 * @function   DARegisterDiskUnmountApprovalCallbackBlock
 * @abstract   Registers a callback block to be called whenever a volume is to be unmounted.
 * @param      session  The session object.
 * @param      match    The disk description keys to match.  Pass NULL for all disk objects.
 * @param      callback The callback block to call when a volume is to be unmounted.
 */

extern void DARegisterDiskUnmountApprovalCallbackBlock( DASessionRef                  session,
                                                            CFDictionaryRef __nullable    match,
                                                            DADiskUnmountApprovalCallbackBlock callback );
                                                            
/*!
 * @typedef    DADiskEjectCallbackBlock
 * @abstract   Type of the callback block used by DADiskEjectWithBlock().
 * @param      disk      The disk object.
 * @param      dissenter A dissenter object on failure or NULL on success.
 */

typedef void ( ^DADiskEjectCallbackBlock )( DADiskRef disk, DADissenterRef __nullable dissenter );

/*!
 * @function   DADiskEjectWithBlock
 * @abstract   Ejects the specified disk object.
 * @param      disk     The disk object.
 * @param      options  The eject options.
 * @param      callback The callback block to call once the ejection completes.
 */

extern void DADiskEjectWithBlock( DADiskRef                      disk,
                                    DADiskEjectOptions             options,
                                    DADiskEjectCallbackBlock __nullable callback );

/*!
 * @typedef    DADiskEjectApprovalCallbackBlock
 * @abstract   Type of the callback block used by DARegisterDiskEjectApprovalCallbackBlock().
 * @param      disk    A disk object.
 * @result     A dissenter reference.  Pass NULL to approve.
 * @discussion
 * The caller of this callback receives a reference to the returned object.  The
 * caller also implicitly retains the object and is responsible for releasing it
 * with CFRelease().
 */

typedef DADissenterRef __nullable ( ^DADiskEjectApprovalCallbackBlock )( DADiskRef disk );

/*!
 * @function   DARegisterDiskEjectApprovalCallbackBlock
 * @abstract   Registers a callback block to be called whenever a volume is to be ejected.
 * @param      session  The session object.
 * @param      match    The disk description keys to match.  Pass NULL for all disk objects.
 * @param      callback The callback block to call when a volume is to be ejected.
 */

extern void DARegisterDiskEjectApprovalCallbackBlock( DASessionRef                session,
                                                        CFDictionaryRef __nullable  match,
                                                        DADiskEjectApprovalCallbackBlock callback );
                                                         

/*!
 * @typedef    DADiskClaimCallbackBlock
 * @abstract   Type of the callback block used by DADiskClaimWithBlock().
 * @param      disk      The disk object.
 * @param      dissenter A dissenter object on failure or NULL on success.
 */

typedef void ( ^DADiskClaimCallbackBlock )( DADiskRef disk, DADissenterRef __nullable dissenter );

/*!
 * @typedef    DADiskClaimReleaseCallbackBlock
 * @abstract   Type of the callback block used by DADiskClaimWithBlock().
 * @param      disk    The disk object.
 * @result     A dissenter reference.  Pass NULL to release claim.
 * @discussion
 * The caller of this callback receives a reference to the returned object.  The
 * caller also implicitly retains the object and is responsible for releasing it
 * with CFRelease().
 */

typedef DADissenterRef __nullable ( ^DADiskClaimReleaseCallbackBlock )( DADiskRef disk );

/*!
 * @function   DADiskClaimWithBlock
 * @abstract   Claims the specified disk object for exclusive use.
 * @param      disk            The disk object.
 * @param      options         The claim options.
 * @param      release         The callback block to call when the claim is to be released.
 * @param      callback        The callback block to call once the claim completes.
 */

extern void DADiskClaimWithBlock( DADiskRef                             disk,
                                    DADiskClaimOptions                    options,
                                    DADiskClaimReleaseCallbackBlock __nullable release,
                                    DADiskClaimCallbackBlock __nullable        callback );

/*!
 * @typedef    DADiskPeekCallbackBlock
 * @abstract   Type of the callback block used by DARegisterDiskPeekCallbackBlock().
 * @param      disk    A disk object.
 * @discussion
 * The peek callback functions are called in a specific order, from lowest order to highest
 * order.  DADiskClaim() could be used here to claim the disk object and DADiskSetOptions()
 * could be used here to set up options on the disk object.
 */

typedef void ( ^DADiskPeekCallbackBlock )( DADiskRef disk );

/*!
 * @function   DARegisterDiskPeekCallbackBlock
 * @abstract   Registers a callback block to be called whenever a disk has been probed.
 * @param      session  The session object.
 * @param      match    The disk description keys to match.  Pass NULL for all disk objects.
 * @param      order    The callback order, from lowest to highest.  Pass 0 for the default.
 * @param      callback The callback block to call when a disk has been probed.
 */

extern void DARegisterDiskPeekCallbackBlock( DASessionRef               session,
                                                CFDictionaryRef __nullable match,
                                                CFIndex                    order,
                                                DADiskPeekCallbackBlock    callback );


typedef void ( ^DAIdleCallbackBlock )( void );

extern void DARegisterIdleCallbackWithBlock( DASessionRef session, DAIdleCallbackBlock callback );


#ifdef DA_FSKIT

typedef void ( ^DADiskSetFSKitAdditionsCallbackBlock )( DAReturn );

/*!
 * @function    DADiskSetFSKitAdditions
 * @abstract    Updates the additional keys on a DADisk used to report FSKit information
 * @param       disk              The disk object.
 * @param       additions   Dictionary of keys to extend the associated disk object. Replaces the current additions
 * @discussion
 * Decorates the given DADisk with the keys and values in additions. Restricted to fskitd.
 */

extern void DADiskSetFSKitAdditions( DADiskRef                             disk,
                                      CFDictionaryRef __nullable             additions,
                                      DADiskSetFSKitAdditionsCallbackBlock   callback );

typedef void ( ^DADiskProbeCallbackBlock )( DAReturn );

/*!
 * @function    DADiskProbeWithBlock
 * @abstract    Requests that diskarbitrationd re-probe the indicated disk
 * @param       disk            The disk object indicating which disk to re-probe
 * @param       callback    The callback block to call after this probe has been processed
 */

extern void DADiskProbeWithBlock( DADiskRef                             disk,
                                  DADiskProbeCallbackBlock __nullable   callback );

#endif /* DA_FSKIT */

#endif /* !__DISKARBITRATIOND__ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATION_DISKARBITRATIONPRIVATE__ */
