// This file must be compiled with -fno-builtin.

#include "Thread.h"

namespace Auto {

    //
    // clear_stack
    //
    // clears stack memory from the current sp to the depth that was scanned by the last collection
    //
    void Thread::clear_stack() {
        // We need to be careful about calling functions during stack clearing.
        // We can't use bzero or the like to do the zeroing because we don't know how much stack they use.
        // The amount to clear is typically small so just use a simple loop writing pointer sized NULL values.
        void **sp = (void **)auto_get_sp();
        void **zero_addr = (void **)_stack_scan_peak;
        _stack_scan_peak = sp;
        while (zero_addr < sp) {
            *zero_addr = NULL;
            zero_addr++;
        }
    }
    
};
