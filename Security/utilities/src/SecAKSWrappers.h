//
//  SecAKSWrappers.h
//  utilities
//
//  Created by Mitch Adler on 6/5/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//

#ifndef _SECAKSWRAPPERS_H_
#define _SECAKSWRAPPERS_H_

#include <utilities/SecCFError.h>
#include <AssertMacros.h>
#include <dispatch/dispatch.h>

#if TARGET_IPHONE_SIMULATOR

#include <IOKit/IOReturn.h>

// Make the compiler happy so this will compile.
#define device_keybag_handle 0
#define session_keybag_handle 0

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
    
    if (kIOReturnSuccess != status) {
        SecCFCreateError(status, CFSTR("com.apple.kern_return_t"), CFSTR("Kern return error"), NULL, error);
        return false;
    }

    return true;
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

#endif
