/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#ifndef _SECAKSWRAPPERS_H_
#define _SECAKSWRAPPERS_H_

#include <TargetConditionals.h>
#include <utilities/SecCFError.h>
#include <AssertMacros.h>
#include <dispatch/dispatch.h>

#include <CoreFoundation/CFData.h>

#if RC_HORIZON
#define TARGET_HAS_KEYSTORE 0
#elif TARGET_OS_SIMULATOR
#define TARGET_HAS_KEYSTORE 0
#elif TARGET_OS_OSX
#if TARGET_CPU_X86
#define TARGET_HAS_KEYSTORE 0
#else
#define TARGET_HAS_KEYSTORE 1
#endif
#elif TARGET_OS_EMBEDDED
#define TARGET_HAS_KEYSTORE 1
#else
#error "unknown keystore status for this platform"
#endif

#if !TARGET_HAS_KEYSTORE

#include <IOKit/IOReturn.h>

// Make the compiler happy so this will compile.
#define device_keybag_handle 0
#define session_keybag_handle 0

#define bad_keybag_handle -1

enum keybag_state {
    keybag_state_unlocked = 0,
    keybag_state_locked = 1 << 0,
    keybag_state_no_pin = 1 << 1,
    keybag_state_been_unlocked = 1 << 2,
};
typedef uint32_t keybag_state_t;
typedef int32_t keybag_handle_t;

static kern_return_t aks_get_lock_state(keybag_handle_t handle, keybag_state_t *state) {
    if (state) *state = keybag_state_no_pin & keybag_state_been_unlocked;
    return kIOReturnSuccess;
}

#else

#include <libaks.h>

#endif

//
// MARK: User lock state
//

enum {
    user_keybag_handle = TARGET_OS_EMBEDDED ? device_keybag_handle : session_keybag_handle,
};

extern const char * const kUserKeybagStateChangeNotification;

static inline bool SecAKSGetLockedState(keybag_state_t *state, CFErrorRef* error)
{
    kern_return_t status = aks_get_lock_state(user_keybag_handle, state);

    return SecKernError(status, error, CFSTR("aks_get_lock_state failed: %d"), status);
}

// returns true if any of the bits in bits is set in the current state of the user bag
static inline bool SecAKSLockedAnyStateBitIsSet(bool* isSet, keybag_state_t bits, CFErrorRef* error)
{
    keybag_state_t state;
    bool success = SecAKSGetLockedState(&state, error);
    
    require_quiet(success, exit);
    
    if (isSet)
        *isSet = (state & bits);
    
exit:
    return success;

}

static inline bool SecAKSGetIsLocked(bool* isLocked, CFErrorRef* error)
{
    return SecAKSLockedAnyStateBitIsSet(isLocked, keybag_state_locked, error);
}

static inline bool SecAKSGetIsUnlocked(bool* isUnlocked, CFErrorRef* error)
{
    bool isLocked = false;
    bool success = SecAKSGetIsLocked(&isLocked, error);

    if (success && isUnlocked)
        *isUnlocked = !isLocked;

    return success;
}

static inline bool SecAKSGetHasBeenUnlocked(bool* hasBeenUnlocked, CFErrorRef* error)
{
    return SecAKSLockedAnyStateBitIsSet(hasBeenUnlocked, keybag_state_been_unlocked, error);
}

bool SecAKSDoWhileUserBagLocked(CFErrorRef *error, dispatch_block_t action);
//
// if you can't use the block version above, use these.
// !!!!!Remember to balance them!!!!!!
//
bool SecAKSUnLockUserKeybag(CFErrorRef *error);
bool SecAKSLockUserKeybag(uint64_t timeout, CFErrorRef *error);


CFDataRef SecAKSCopyBackupBagWithSecret(size_t size, uint8_t *secret, CFErrorRef *error);

#endif
