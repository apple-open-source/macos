/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
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

#import <SystemConfiguration/SystemConfiguration.h>
#import <Foundation/Foundation.h>
#import <nw/private.h>
#import <os/log.h>
#import <netsmb/smb_dev.h>
#import <netsmb/smb_dev_2.h>

#include <ifaddrs.h>
#include <net/if_media.h>
#include <net/if.h>

@interface MCMonitor : NSObject
@property (atomic) int nsmbFd;
@end

@implementation MCMonitor

- (void)cancelMonitor
{
    if (_nsmbFd != -1) {
        close(_nsmbFd);
    }
}

- (int)getNsmbHandle
{
    int fd, i;
    char buf[20];

    if (_nsmbFd != -1) {
        close(_nsmbFd);
        _nsmbFd = -1;
    }

    /*
     * First try to open as clone
     */
    fd = open("/dev/"NSMB_NAME, O_RDWR);
    if (fd >= 0) {
        _nsmbFd = fd;
        return 0;
    }
    /*
     * well, no clone capabilities available - we have to scan
     * all devices in order to get free one
     */
    for (i = 0; i < 1024; i++) {
        snprintf(buf, sizeof(buf), "/dev/%s%x", NSMB_NAME, i);
        fd = open(buf, O_RDWR);
        if (fd >= 0) {
            _nsmbFd = fd;
            return 0;
        }
    }
    os_log_error(OS_LOG_DEFAULT,
                 "%s: %d failures to open nsmb device, error = %s",
                 __FUNCTION__,  i+1, strerror(errno));

    return ENOENT;
}

- (MCMonitor*)init
{
    self = [super init];
    if (self) {
        _nsmbFd = -1;
    }

    return self;
}

/*
 * Send kernel the notifier pid,
 * so each new mount would be able to know
 * if it needs to open one.
 */
- (int)updateNotifierPid:(pid_t) pid
{
    int error = 0;
    struct smbioc_notifier_pid ioc_pid;
    ioc_pid.pid = pid;
    if (ioctl(_nsmbFd, SMBIOC_UPDATE_NOTIFIER_PID, &ioc_pid) == -1) {
        /* Some internal error happened? */
        error = errno;
        os_log_error(OS_LOG_DEFAULT,
                     "%s: ioctl SMBIOC_UPDATE_NOTIFIER_PID failed with error [%d]",
                     __FUNCTION__, error);
    }

    return error;
}
@end

MCMonitor* monitor;

static void StoreMonitorCallback( SCDynamicStoreRef store, CFArrayRef changes, void *info )
{
    /*
     * Look for client's interface server and update the NIC
     * table.
     */
    struct smbioc_client_interface clientInterfaceUpdate;

    int error = get_client_interfaces(&clientInterfaceUpdate);

    if (!error) {
        if (ioctl(monitor.nsmbFd, SMBIOC_NOTIFIER_UPDATE_INTERFACES, &clientInterfaceUpdate) == -1) {
            /* Some internal error happened? */
            error = errno;
            os_log_error(OS_LOG_DEFAULT,
                         "%s: ioctl SMBIOC_NOTIFIER_UPDATE_INTERFACES failed with error [%d]",
                         __FUNCTION__, error);
        } else {
            /* The real error */
            error = clientInterfaceUpdate.ioc_errno;
            if (error) {
                os_log_error(OS_LOG_DEFAULT,
                             "%s: update interfaces failed with error [%d]",
                             __FUNCTION__, error);
            }
        }
        free(clientInterfaceUpdate.ioc_info_array);
    }

    if (error) {
        os_log_error(OS_LOG_DEFAULT,
                     "%s: monitor exiting with error %d",
                     __FUNCTION__, error);
        [monitor cancelMonitor];
        exit(error);
    }
}

int main(int argc, const char * argv[]) {
    CFStringRef patterns[2];
    CFArrayRef patternsArray;
    SCDynamicStoreRef storeIp;
    Boolean ok;
    monitor = [[MCMonitor alloc] init];

    /* Get the nsmb fd */
    int error = [monitor getNsmbHandle];
    if (error) {
        exit(error);
    }

    /* Update the kext with the process id */
    error = [monitor updateNotifierPid:getpid()];
    if (error) {
        [monitor cancelMonitor];
        exit(error);
    }

    /*
     * Watch dynamic store IPv4/6 key changes for all network interfaces
     * we should not use path_monitor as it will not notify in cases of interfaces
     * connecting/disconecting without causing path changes. Also, we should NOT
     * watch link key changes as it might notify too early when the link is up
     * with no IP address assigned. for more details please see rdar://69000233
     */
    storeIp = SCDynamicStoreCreate(NULL,
                                   CFSTR("watchIpChanges"),
                                   StoreMonitorCallback,
                                   NULL);

    patterns[0] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
                                                            kSCDynamicStoreDomainState,
                                                            kSCCompAnyRegex,
                                                            kSCEntNetIPv4 );

    patterns[1] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
                                                            kSCDynamicStoreDomainState,
                                                            kSCCompAnyRegex,
                                                            kSCEntNetIPv6 );

    patternsArray = CFArrayCreate(NULL, (void *)patterns, 2, &kCFTypeArrayCallBacks);
    ok = SCDynamicStoreSetNotificationKeys(storeIp, NULL, patternsArray);
    CFRelease(patternsArray);
    CFRelease(patterns[0]);
    CFRelease(patterns[1]);

    if (!ok)
    {
        os_log_error(OS_LOG_DEFAULT,
                     "%s: could not set notification keys (%s) - monitor exiting",
                     __FUNCTION__, SCErrorString(SCError()));
        [monitor cancelMonitor];
        exit(0);
    }

    ok = SCDynamicStoreSetDispatchQueue(storeIp, dispatch_get_main_queue());

    if (!ok)
    {
        os_log_error(OS_LOG_DEFAULT,
                     "%s: could not set SCDS dispatch queue (%s) - monitor exiting",
                     __FUNCTION__, SCErrorString(SCError()));
        [monitor cancelMonitor];
        exit(0);
    }

    /* set event handler to track when
       the kernel signal us to close the notifier */
    dispatch_source_t sig_src;
    sig_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(sig_src, ^{
        os_log_debug(OS_LOG_DEFAULT,
                     "Closing mc_notifier");
        [monitor cancelMonitor];
        exit(0);
    });

    signal(SIGTERM, SIG_IGN);
    dispatch_resume(sig_src);
    dispatch_main();
}
