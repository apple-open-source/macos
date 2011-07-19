/*
 * Copyright (c) 1998-2011 Apple Inc. All Rights Reserved.
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

#ifndef __DISKARBITRATIOND_DASUPPORT__
#define __DISKARBITRATIOND_DASUPPORT__

#include <CoreFoundation/CoreFoundation.h>

#include "DADisk.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum
{
    kDAAuthorizeOptionDefault  = 0x00000000,
    kDAAuthorizeOptionForce    = 0x00080000,
    kDAAuthorizeOptionInteract = 0x00000001
};

typedef UInt32 DAAuthorizeOptions;

typedef void ( *DAAuthorizeCallback )( DAReturn status, void * context );

extern DAReturn DAAuthorize( DASessionRef       session,
                             DAAuthorizeOptions options,
                             DADiskRef          disk,
                             uid_t              userUID,
                             gid_t              userGID,
                             const char *       right );

extern void DAAuthorizeWithCallback( DASessionRef        session,
                                     DAAuthorizeOptions  options,
                                     DADiskRef           disk,
                                     uid_t               userUID,
                                     gid_t               userGID,
                                     DAAuthorizeCallback callback,
                                     void *              callbackContext,
                                     const char *        right );

extern const CFStringRef kDAFileSystemKey; /* ( DAFileSystem ) */

extern void DAFileSystemListRefresh( void );

extern const CFStringRef kDAMountMapMountAutomaticKey; /* ( CFBoolean ) */
extern const CFStringRef kDAMountMapMountOptionsKey;   /* ( CFString  ) */
extern const CFStringRef kDAMountMapMountPathKey;      /* ( CFURL     ) */
extern const CFStringRef kDAMountMapProbeIDKey;        /* ( CFUUID    ) */
extern const CFStringRef kDAMountMapProbeKindKey;      /* ( CFString  ) */

extern void DAMountMapListRefresh1( void );
extern void DAMountMapListRefresh2( void );

extern const CFStringRef kDAPreferenceMountDeferExternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountDeferInternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountDeferRemovableKey; /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountTrustExternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountTrustInternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountTrustRemovableKey; /* ( CFBoolean ) */

extern void DAPreferenceListRefresh( void );

enum
{
    kDAUnitStateCommandActive    = 0x00000001,
    kDAUnitStateStagedUnreadable = 0x00010000
};

typedef UInt32 DAUnitState;

extern Boolean DAUnitGetState( DADiskRef disk, DAUnitState state );
extern void    DAUnitSetState( DADiskRef disk, DAUnitState state, Boolean value );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DASUPPORT__ */
