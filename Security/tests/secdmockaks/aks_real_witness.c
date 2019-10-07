
#include "aks_real_witness.h"
#include <TargetConditionals.h>

#if !TARGET_OS_SIMULATOR

#include <libaks.h>
#include <MobileKeyBag/MobileKeyBag.h>

#include <utilities/debugging.h>

bool hwaes_key_available(void)
{
    keybag_handle_t handle = bad_keybag_handle;
    keybag_handle_t special_handle = bad_keybag_handle;
#if TARGET_OS_OSX
    special_handle = session_keybag_handle;
#elif TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    special_handle = device_keybag_handle;
#else
#error "supported keybag target"
#endif

    kern_return_t kr = aks_get_system(special_handle, &handle);
    if (kr != kAKSReturnSuccess) {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
        /* TODO: Remove this once the kext runs the daemon on demand if
         there is no system keybag. */
        int kb_state = MKBGetDeviceLockState(NULL);
        secinfo("aks", "AppleKeyStore lock state: %d", kb_state);
#endif
    }
    return true;
}

#endif
