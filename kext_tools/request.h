#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <CoreFoundation/CoreFoundation.h>
#include <libc.h>

#include <IOKit/kext/KXKextManager.h>
#include "queue.h"

typedef struct _request {
    unsigned int   type;
    char *         kmodname;
    char *         kmodvers;
    queue_chain_t  link;
} request_t;


Boolean kextd_launch_kernel_request_thread(void);
void * kextd_kernel_request_loop(void * arg);

void kextd_handle_kernel_request(void * info);

void kextd_load_kext(char * kmod_name,
    KXKextManagerError * result /* out */);

#ifndef NO_CFUserNotification
void kextd_handle_pended_kextload(void * info);
#endif /* NO_CFUserNotification */

#endif __REQUEST_H__
