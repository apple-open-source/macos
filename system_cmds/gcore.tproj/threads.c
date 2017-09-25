/*
 * Copyright (c) 2015 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "utils.h"
#include "threads.h"
#include "corefile.h"

#include <sys/types.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>

typedef struct {
    int flavor;
    mach_msg_type_number_t count;
} threadflavor_t;

static threadflavor_t thread_flavor[] = {
#if defined(__i386__) || defined(__x86_64__)
    { x86_THREAD_STATE, x86_THREAD_STATE_COUNT },
    { x86_FLOAT_STATE, x86_FLOAT_STATE_COUNT },
    { x86_EXCEPTION_STATE, x86_EXCEPTION_STATE_COUNT },
#elif defined(__arm__)
    { ARM_THREAD_STATE, ARM_THREAD_STATE_COUNT },
    { ARM_VFP_STATE, ARM_VFP_STATE_COUNT },
    { ARM_EXCEPTION_STATE, ARM_EXCEPTION_STATE_COUNT },
#elif defined(__arm64__)
    { ARM_THREAD_STATE64, ARM_THREAD_STATE64_COUNT },
    /* ARM64_TODO: NEON? */
    { ARM_EXCEPTION_STATE64, ARM_EXCEPTION_STATE64_COUNT },
#else
#error architecture not supported
#endif
};

static const int nthread_flavors = sizeof (thread_flavor) / sizeof (thread_flavor[0]);

size_t
sizeof_LC_THREAD()
{
    size_t cmdsize = sizeof (struct thread_command);
    for (int i = 0; i < nthread_flavors; i++) {
        cmdsize += sizeof (thread_flavor[i]) +
        thread_flavor[i].count * sizeof (int);
    }
    return cmdsize;
}

void
dump_thread_state(native_mach_header_t *mh, struct thread_command *tc, mach_port_t thread)
{
    tc->cmd = LC_THREAD;
    tc->cmdsize = (uint32_t) sizeof_LC_THREAD();

    uint32_t *wbuf = (void *)(tc + 1);

    for (int f = 0; f < nthread_flavors; f++) {

        memcpy(wbuf, &thread_flavor[f], sizeof (thread_flavor[f]));
        wbuf += sizeof (thread_flavor[f]) / sizeof (*wbuf);

        const kern_return_t kr = thread_get_state(thread, thread_flavor[f].flavor, (thread_state_t)wbuf, &thread_flavor[f].count);
        if (KERN_SUCCESS != kr) {
            err_mach(kr, NULL, "getting flavor %d of thread",
                     thread_flavor[f].flavor);
            bzero(wbuf, thread_flavor[f].count * sizeof (int));
        }

        wbuf += thread_flavor[f].count;
    }
    assert((ptrdiff_t)tc->cmdsize == ((caddr_t)wbuf - (caddr_t)tc));

    mach_header_inc_ncmds(mh, 1);
    mach_header_inc_sizeofcmds(mh, tc->cmdsize);
}
