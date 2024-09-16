/*
 *  testmain.c
 *  Security
 *
 *  Copyright (c) 2010,2012-2014 Apple Inc. All Rights Reserved.
 *
 */
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <regressions/test/testenv.h>

#include "testlist.h"
#include <regressions/test/testlist_begin.h>
#include "testlist.h"
#include <regressions/test/testlist_end.h>

#include <dispatch/dispatch.h>
#include <CoreFoundation/CFRunLoop.h>
#include "featureflags/affordance_featureflags.h"
#include "keychain/ckks/CKKS.h"

int main(int argc, char *argv[])
{
    //printf("Build date : %s %s\n", __DATE__, __TIME__);
    //printf("WARNING: If running those tests on a device with a passcode, DONT FORGET TO UNLOCK!!!\n");

    SecCKKSDisable();
    KCSharingSetChangeTrackingEnabled(false);

#if 0 && NO_SERVER
    SOSCloudKeychainServerInit();
#endif

#if TARGET_OS_IPHONE
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        int result = tests_begin(argc, argv);

        fflush(stderr);
        fflush(stdout);

        sleep(1);
        
        exit(result);
    });

    CFRunLoopRun();

    return 0;
#else
    int result = tests_begin(argc, argv);

    fflush(stdout);
    fflush(stderr);

    sleep(1);

    return result;
#endif
}
