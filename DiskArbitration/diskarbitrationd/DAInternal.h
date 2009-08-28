/*
 * Copyright (c) 1998-2009 Apple Inc. All Rights Reserved.
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

#ifndef __DISKARBITRATIOND_DAINTERNAL__
#define __DISKARBITRATIOND_DAINTERNAL__

#include <mach/mach.h>
#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ___GID_ADMIN 80

#define ___GID_WHEEL 0
#define ___UID_ROOT  0

#define ___GID_UNKNOWN 99
#define ___UID_UNKNOWN 99

#define _kDAServiceName "com.apple.DiskArbitration.diskarbitrationd"

enum
{
    _kDADiskAppearedCallback,
    _kDADiskClaimCallback,
    _kDADiskClaimReleaseCallback,
    _kDADiskDescriptionChangedCallback,
    _kDADiskDisappearedCallback,
    _kDADiskEjectCallback,
    _kDADiskEjectApprovalCallback,
    _kDADiskMountCallback,
    _kDADiskMountApprovalCallback,
    _kDADiskPeekCallback,
    _kDADiskRefreshCallback,
    _kDADiskRenameCallback,
    _kDADiskUnmountCallback,
    _kDADiskUnmountApprovalCallback,
    _kDAIdleCallback
};

typedef UInt32 _DACallbackKind;

enum
{
    _kDADiskClaim   = _kDADiskClaimCallback,
    _kDADiskEject   = _kDADiskEjectCallback,
    _kDADiskMount   = _kDADiskMountCallback,
    _kDADiskRefresh = _kDADiskRefreshCallback,
    _kDADiskRename  = _kDADiskRenameCallback,
    _kDADiskUnmount = _kDADiskUnmountCallback
};

typedef UInt32 _DARequestKind;

__private_extern__ const char * _kDAAuthorizeRightAdopt;
__private_extern__ const char * _kDAAuthorizeRightEncode;
__private_extern__ const char * _kDAAuthorizeRightMount;
__private_extern__ const char * _kDAAuthorizeRightRename;
__private_extern__ const char * _kDAAuthorizeRightUnmount;

__private_extern__ const CFStringRef _kDACallbackAddressKey;       /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDACallbackArgument0Key;     /* ( CFType       ) */
__private_extern__ const CFStringRef _kDACallbackArgument1Key;     /* ( CFType       ) */
__private_extern__ const CFStringRef _kDACallbackContextKey;       /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDACallbackDiskKey;          /* ( DADisk       ) */
__private_extern__ const CFStringRef _kDACallbackKindKey;          /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDACallbackMatchKey;         /* ( CFDictionary ) */
__private_extern__ const CFStringRef _kDACallbackOrderKey;         /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDACallbackSessionKey;       /* ( DASession    ) */
__private_extern__ const CFStringRef _kDACallbackTimeKey;          /* ( CFDate       ) */
__private_extern__ const CFStringRef _kDACallbackWatchKey;         /* ( CFArray      ) */

__private_extern__ const CFStringRef _kDADiskIDKey;                /* ( CFData       ) */

__private_extern__ const CFStringRef _kDADissenterProcessIDKey;    /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDADissenterStatusKey;       /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDADissenterStatusStringKey; /* ( CFString     ) */

__private_extern__ const CFStringRef _kDARequestArgument1Key;      /* ( CFType       ) */
__private_extern__ const CFStringRef _kDARequestArgument2Key;      /* ( CFType       ) */
__private_extern__ const CFStringRef _kDARequestArgument3Key;      /* ( CFType       ) */
__private_extern__ const CFStringRef _kDARequestCallbackKey;       /* ( DACallback   ) */
__private_extern__ const CFStringRef _kDARequestDiskKey;           /* ( DADisk       ) */
__private_extern__ const CFStringRef _kDARequestDissenterKey;      /* ( DADissenter  ) */
__private_extern__ const CFStringRef _kDARequestKindKey;           /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDARequestLinkKey;           /* ( CFArray      ) */
__private_extern__ const CFStringRef _kDARequestStateKey;          /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDARequestUserGIDKey;        /* ( CFNumber     ) */
__private_extern__ const CFStringRef _kDARequestUserUIDKey;        /* ( CFNumber     ) */

__private_extern__ int          ___isadmin( uid_t uid );
__private_extern__ int          ___statfs( const char * path, struct statfs * buf, int flags );
__private_extern__ Boolean      ___CFArrayContainsValue( CFArrayRef array, const void * value );
__private_extern__ void         ___CFArrayRemoveValue( CFMutableArrayRef array, const void * value );
__private_extern__ vm_address_t ___CFDataCopyBytes( CFDataRef data, mach_msg_type_number_t * length );
__private_extern__ SInt64       ___CFDictionaryGetIntegerValue( CFDictionaryRef dictionary, const void * key );
__private_extern__ void         ___CFDictionarySetIntegerValue( CFMutableDictionaryRef dictionary, const void * key, SInt64 value );
__private_extern__ CFNumberRef  ___CFNumberCreateWithIntegerValue( CFAllocatorRef allocator, SInt64 value );
__private_extern__ SInt64       ___CFNumberGetIntegerValue( CFNumberRef number );
__private_extern__ char *       ___CFStringCopyCString( CFStringRef string );
__private_extern__ char *       ___CFURLCopyFileSystemRepresentation( CFURLRef url );

__private_extern__ const char * _DACallbackKindGetName( _DACallbackKind kind );
__private_extern__ const char * _DARequestKindGetName( _DARequestKind kind );

__private_extern__ CFDataRef              _DASerialize( CFAllocatorRef allocator, CFTypeRef object );
__private_extern__ CFDataRef              _DASerializeDiskDescription( CFAllocatorRef allocator, CFDictionaryRef description );
__private_extern__ CFTypeRef              _DAUnserialize( CFAllocatorRef allocator, CFDataRef data );
__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescription( CFAllocatorRef allocator, CFDataRef data );
__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescriptionWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length );
__private_extern__ CFTypeRef              _DAUnserializeWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length );

__private_extern__ char * _DAVolumeCopyID( const struct statfs * fs );
__private_extern__ char * _DAVolumeGetID( const struct statfs * fs );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAINTERNAL__ */
